#include "audio/synth.h"
#include "audio/notes.h"
#include <algorithm>
#include <cmath>
#include "core/math.h"
#include "sim/params.h"
#include "sim/physics.h"
#include "sim/scene.h"
#include "film/sequence.h"

// ---------------------------------------------------------------- what the physics says
SoundControls soundControls(const CamState& c, float fade, float t, bool film) {
    SoundControls s;
    s.level = fade;
    float r = camRadius(c), rh = std::max(horizonRadius(), 1e-3f);
    s.tension = std::min(1.0f, std::sqrt(rh / std::max(r, 1e-3f)));   // 0.06 far out, 0.33 at 9 r_s, 1 at the horizon
    float rate = farClockRate(c);
    s.tickRate  = rate > 0.0f ? std::min(rate, 60.0f) : 0.0f;
    s.tickLevel = smoothstepf(0.22f, 0.42f, s.tension);             // the clock fades in below ~10 r_s
    s.roar  = diskProximity(c);
    s.shift = std::max(0.3f, std::min(4.0f, viewShift(c)));
    // scene objects nearby: a planet's ocean
    CamFrame cf = camBasis(c);
    for (const SceneObject& o : g_objects) {
        if (!objectActive(o, t, film) || o.type != SO_PLANET) continue;
        float d = len(objectPosition(o, t, film) - cf.pos) - o.size, pr = objectPresence(o, t, film);
        s.ocean = std::max(s.ocean, pr * smoothstepf(3.5f, 0.8f, d));
    }
    return s;
}

// After the cut to black the picture is gone but the music is not: the camera's state is held at the
// cut, the effects stop, and the organ resolves home and dies away in the hall over the end credits.
// The music only follows the picture's fade-in; the picture's fade-out to the cut must not dip it, or the
// organ would stop at the cut and swell back for the coda.
SoundControls filmSound(float t) {
    float L = seqLength(), cut = g_blackAt > 0.0f ? std::min(g_blackAt, L) : L;
    float fadeIn = g_fadeIn > 0.0f ? smoothstepf(0.0f, g_fadeIn, t) : 1.0f;
    if (t < cut) return soundControls(evalSequence(t), cut < L ? fadeIn : seqFade(t), t, true);   // no credits: fade with it
    SoundControls s = soundControls(evalSequence(cut - 0.01f), 1.0f, cut, true);
    float tail = std::max(1.0f, L - cut);                    // the credits' length: ring out across all of it
    s.level = std::exp(-(t - cut) / (0.45f * tail)) * smoothstepf(L, L - std::min(6.0f, 0.5f * tail), t);
    s.coda = 1.0f;
    s.tension = 0.45f;
    s.tickLevel = 0.0f; s.tickRate = 0.0f; s.roar = 0.0f; s.ocean = 0.0f;
    return s;
}

// ---------------------------------------------------------------- organ wavetables
static const int TAB = 4096;
static float g_organ[TAB], g_bright[TAB];

static void buildTables() {
    static bool done = false;
    if (done) return;
    done = true;
    // flue pipes 8' 4' 2 2/3' 2' 1 3/5' 1', and a mixture stop for the bright layer
    const float organ[][2]  = {{1, 1.0f}, {2, 0.55f}, {3, 0.30f}, {4, 0.28f}, {5, 0.08f}, {8, 0.12f}};
    const float bright[][2] = {{1, 0.5f}, {6, 0.40f}, {8, 0.35f}, {10, 0.22f}, {12, 0.30f}, {16, 0.18f}};
    float peakO = 0, peakB = 0;
    for (int i = 0; i < TAB; i++) {
        float x = 2 * PI * i / TAB, o = 0, b = 0;
        for (auto& h : organ)  o += h[1] * std::sin(h[0] * x);
        for (auto& h : bright) b += h[1] * std::sin(h[0] * x);
        g_organ[i] = o; g_bright[i] = b;
        peakO = std::max(peakO, std::fabs(o)); peakB = std::max(peakB, std::fabs(b));
    }
    for (int i = 0; i < TAB; i++) { g_organ[i] /= peakO; g_bright[i] /= peakB; }
}
static inline float lookup(const float* tab, float ph) {
    float x = ph * TAB;
    int i = (int)x;
    float f = x - i;
    return tab[i & (TAB - 1)] * (1 - f) + tab[(i + 1) & (TAB - 1)] * f;
}
// sawtooth with a polyBLEP correction at the wrap, so high notes don't alias into a buzz
static inline float sawBlep(float ph, float dt) {
    float v = 2.0f * ph - 1.0f;
    if (ph < dt)             { float t = ph / dt;          v -= t + t - t * t - 1.0f; }
    else if (ph > 1.0f - dt) { float t = (ph - 1.0f) / dt; v -= t * t + t + t + 1.0f; }
    return v;
}
static inline void advance(float& ph, float f) { ph += f / Synth::RATE; ph -= (int)ph; }

// A minor: Am - Fmaj7 - C - G, one chord every 9 s with long crossfades
static const int CHORDS[4][5] = {
    {NOTE_A2, NOTE_E3, NOTE_A3, NOTE_C4, NOTE_E4},              // Am
    {NOTE_F2, NOTE_C3, NOTE_A3, NOTE_C4, NOTE_E4},              // Fmaj7
    {NOTE_C3, NOTE_G3, NOTE_C4, NOTE_E4, NOTE_G4},              // C
    {NOTE_G2, NOTE_D3, NOTE_G3, NOTE_B3, NOTE_D4},              // G
};
static const float CHORD_LEN = 9.0f, CROSSFADE = 3.0f;

// every note belongs to the chord under it, also through the 3 s crossfades into the next chord
static const float THEME_T[11] = {0, 3, 6, 9, 13.5f, 15, 18, 22.5f, 27, 30, 33};
static const int   THEME_N[11] = {NOTE_E4, NOTE_A4, NOTE_C5,             // Am (C5 also fits F, coming next)
                                  NOTE_A4, NOTE_C5, NOTE_E5,             // F  (E5 is its maj7 and C's third)
                                  NOTE_G4, NOTE_E4,                      // C
                                  NOTE_D5, NOTE_B4, NOTE_G4};            // G  (G4 sits on Am as its 7th)

// voice layout per chord: 5 base notes, 1 pedal, 5 octave-up, 1 shimmer
enum { BASE = 0, PEDAL = 5, OCTAVE = 6, SHIMMER = 11, PER_CHORD = 12 };

Synth::Synth() {
    buildTables();
    for (int c = 0; c < 4; c++) {
        for (int n = 0; n < 5; n++) voices.push_back({noteHz(CHORDS[c][n]), (n - 2) * 0.25f, 0});
        voices.push_back({noteHz(CHORDS[c][0] - 12), 0.0f, 0});
        for (int n = 0; n < 5; n++) voices.push_back({noteHz(CHORDS[c][n] + 12), (2 - n) * 0.3f, 1});
        voices.push_back({noteHz(CHORDS[c][0] + 24 + 2), 0.15f, 1});   // the 9th, two octaves up
    }
    // Freeverb-style reverb: lengths at 44.1 kHz scaled to 48 kHz, right channel slightly longer for width
    const int combLen[4] = {1557, 1617, 1491, 1422}, apLen[2] = {556, 441};
    for (int ch = 0; ch < 2; ch++) {
        for (int i = 0; i < 4; i++) comb[ch][i].buf.assign((size_t)((combLen[i] + ch * 23) * RATE / 44100), 0.0f);
        for (int i = 0; i < 2; i++) ap[ch][i].buf.assign((size_t)((apLen[i] + ch * 23) * RATE / 44100), 0.0f);
    }
}

float Synth::noise() {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return (rng & 0xFFFFFF) / float(0x800000) - 1.0f;
}

float Synth::reverb(int ch, float x) {
    float out = 0;
    for (auto& c : comb[ch]) {
        float y = c.buf[c.pos];
        c.store = y * 0.7f + c.store * 0.3f;                    // damping
        c.buf[c.pos] = x + c.store * 0.86f;                     // feedback: a big hall
        if (++c.pos >= (int)c.buf.size()) c.pos = 0;
        out += y;
    }
    for (auto& a : ap[ch]) {
        float b = a.buf[a.pos];
        a.buf[a.pos] = out + b * 0.5f;
        out = b - out;
        if (++a.pos >= (int)a.buf.size()) a.pos = 0;
    }
    return out * 0.25f;
}

void Synth::render(float* lr, int frames, double t, const SoundControls& target) {
    const int SUB = 64;
    for (int start = 0; start < frames; start += SUB) {
        int n = std::min(SUB, frames - start);
        double ts = t + start / (double)RATE;

        // controls glide (about 150 ms; the master level is fast so cuts to black stay sharp)
        // in the coda everything relaxes slowly (seconds), so nothing snaps off at the cut
        float glide = target.coda > 0.5f ? 2.5f : 0.15f;
        float k = 1.0f - std::exp(-n / (glide * RATE)), kl = 1.0f - std::exp(-n / ((target.coda > 0.5f ? 0.4f : 0.012f) * RATE));
        codaMix += (target.coda - codaMix) * (1.0f - std::exp(-n / (1.2f * RATE)));
        cur.tension   += (target.tension - cur.tension) * k;
        cur.tickLevel += (target.tickLevel - cur.tickLevel) * k;
        cur.roar      += (target.roar - cur.roar) * k;
        cur.shift     += (target.shift - cur.shift) * k;
        cur.ocean     += (target.ocean - cur.ocean) * k;
        cur.tickRate   = target.tickRate;
        float levelFrom = cur.level;
        cur.level     += (target.level - cur.level) * kl;

        // chord weights: each chord holds for CHORD_LEN, crossfading into the next
        float w[4] = {0, 0, 0, 0};
        double cyc = std::fmod(std::max(0.0, ts), 4.0 * CHORD_LEN);
        int ci = (int)(cyc / CHORD_LEN);
        float in = (float)(cyc - ci * CHORD_LEN);
        float x = smoothstepf(CHORD_LEN - CROSSFADE, CHORD_LEN, in);
        w[ci] = 1.0f - x; w[(ci + 1) % 4] = x;
        for (int c = 0; c < 4; c++) w[c] = w[c] * (1.0f - codaMix) + (c == 0 ? codaMix : 0.0f);   // home: A minor

        float T = cur.tension;
        float organ   = 0.14f + 0.86f * T;                       // near silence far away, swelling as we close in
        float pedal   = smoothstepf(0.10f, 0.55f, T);
        float octave  = smoothstepf(0.35f, 0.85f, T);
        float shimmer = smoothstepf(0.60f, 1.00f, T);
        float breath  = 1.0f + 0.08f * std::sin(2.0f * PI * 0.07f * (float)ts);
        // the orchestra: strings from the very start, the theme from ~45 r_s, the choir from ~15 r_s, brass
        // impacts from ~6 r_s and every half bar near the end. Each chord swells and eases like a phrase.
        float epic   = smoothstepf(0.38f, 0.92f, T) * (1.0f - codaMix);
        float swell  = 0.72f + 0.28f * std::sin(PI * in / CHORD_LEN);
        float strLvl = (0.45f + 0.55f * smoothstepf(0.05f, 0.6f, T)) * swell * (1.0f - 0.5f * epic);
        float choLvl = smoothstepf(0.25f, 0.7f, T) * swell * (1.0f - 0.4f * epic);
        float melLvl = smoothstepf(0.14f, 0.5f, T);
        float melOct = smoothstepf(0.55f, 0.9f, T);
        breath *= 1.0f - 0.45f * epic;                           // the organ steps back for the brass (never mutes)
        float strA = 1.0f - std::exp(-2 * PI * (500.0f + 2400.0f * T * swell + 900.0f * epic) / RATE);
        float melA = 1.0f - std::exp(-2 * PI * (700.0f + 2200.0f * T) / RATE);

        // the theme: which note now (the coda holds A)
        {
            int note = NOTE_A4;
            if (codaMix < 0.5f) {
                int k = 0;
                while (k + 1 < 11 && THEME_T[k + 1] <= cyc) k++;
                note = THEME_N[k];
            }
            if (note != melNote) {
                melTarget = noteHz(note);
                if (melNote < 0) melFreq = melTarget;
                melEnv *= 0.55f;                                  // a soft new bow stroke, not a restart
                melNote = note;
            }
        }
        float melAtk = 1.0f - std::exp(-n / (0.35f * RATE)), melGlide = 1.0f - std::exp(-n / (0.08f * RATE));
        float strK = 1.0f - std::exp(-n / (0.05f * RATE));

        // braams: on every chord change, and on every half bar once we're deep in
        if (epic > 0.03f) {
            double beat = epic > 0.75f ? CHORD_LEN * 0.5 : CHORD_LEN;
            long b = (long)std::floor(ts / beat);
            if (b != braamBeat) {
                if (braamBeat >= 0) {
                    Braam& br = braam[braamNext]; braamNext = (braamNext + 1) % 3;
                    int chord = (int)(std::fmod(b * beat, 4.0 * CHORD_LEN) / CHORD_LEN) % 4;
                    int root = CHORDS[chord][0];
                    root = inOctave(root, NOTE_C2);               // brass in the C2..B2 octave
                    br = Braam{};
                    br.age = 0.0f; br.amp = std::sqrt(epic); br.freq = noteHz(root);   // full-blooded early on
                }
                braamBeat = b;
            }
        }

        float targetAmp[4 * PER_CHORD];
        for (int c = 0; c < 4; c++)
            for (int v = 0; v < PER_CHORD; v++) {
                float a = v < PEDAL ? 0.10f * organ : v == PEDAL ? 0.16f * pedal * (1.0f - 0.7f * epic) : v < SHIMMER ? 0.055f * octave : 0.05f * shimmer;
                targetAmp[c * PER_CHORD + v] = a * w[c] * breath;
            }
        float sub = 0.14f * T * T * (1.0f - 0.85f * epic);      // the braams own the low end once they come in

        for (int i = 0; i < n; i++) {
            float fi = (i + 1) / (float)n, L = 0, R = 0;
            for (size_t v = 0; v < voices.size(); v++) {
                Voice& vo = voices[v];
                float a = vo.amp + (targetAmp[v] - vo.amp) * fi;
                if (a < 1e-5f) continue;
                const float* tab = vo.table ? g_bright : g_organ;
                vo.phA += vo.freq * 1.0012f / RATE; vo.phA -= (int)vo.phA;    // two slightly detuned pipes per note
                vo.phB += vo.freq * 0.9988f / RATE; vo.phB -= (int)vo.phB;
                float sA = lookup(tab, vo.phA), sB = lookup(tab, vo.phB);
                L += a * (sA * (0.5f - 0.5f * vo.pan) + sB * 0.5f);
                R += a * (sB * (0.5f + 0.5f * vo.pan) + sA * 0.5f);
            }
            // deep drone, felt more than heard
            // on each chord's root, A0..G#1 and an octave up, crossfading with the chords
            float d = 0.0f;
            for (int c = 0; c < 4; c++) {
                float f = noteHz(inOctave(CHORDS[c][0], NOTE_A0));
                advance(subPhase[c][0], f); advance(subPhase[c][1], 2.0f * f);
                d += w[c] * (std::sin(2 * PI * subPhase[c][0]) * 0.7f + std::sin(2 * PI * subPhase[c][1]) * 0.5f);
            }
            d *= sub;
            L += d; R += d;

            // the clock: one tick per second of far-away time
            if (cur.tickRate > 0.0f) {
                tickPhase += cur.tickRate / RATE;
                if (tickPhase >= 1.0) { tickPhase -= std::floor(tickPhase); tickEnv = 1.0f; }
            }
            float nz = noise();
            hpOut = 0.95f * (hpOut + nz - hpPrev); hpPrev = nz;
            if (tickEnv == 1.0f) tickAge = 0.0f;
            // a soft wooden knock: low sine with a touch of filtered noise, ~9 ms decay, mostly heard in the hall
            float knock = std::sin(2 * PI * noteHz(NOTE_E5) * tickAge / RATE) + 0.15f * hpOut;
            tickLp += (knock - tickLp) * 0.35f;
            float click = tickEnv * tickLp * 0.09f * cur.tickLevel;
            tickEnv *= 0.99769f;
            tickAge += 1.0f;
            L += click; R += click * 0.9f;

            // the disk's roar: brown noise, brighter when blueshifted
            float fc = std::min(500.0f, 70.0f * cur.shift), alpha = 1.0f - std::exp(-2 * PI * fc / RATE);
            for (int ch = 0; ch < 2; ch++) {
                brown[ch] = brown[ch] * 0.996f + noise() * 0.06f;
                roarLp[ch] += (brown[ch] - roarLp[ch]) * alpha;
            }
            float roar = 0.22f * cur.roar;                            // a bed under the organ, not over it
            L += roarLp[0] * roar; R += roarLp[1] * roar;

            // a planet's ocean: noise in slow swells
            if (cur.ocean > 1e-3f) {
                float swell = 0.5f + 0.5f * std::sin(2 * PI * 0.09f * (float)(ts + i / (double)RATE));
                swell *= swell;
                for (int ch = 0; ch < 2; ch++) {
                    oceanLp[ch]  += (noise() - oceanLp[ch]) * 0.08f;
                    oceanLp2[ch] += (oceanLp[ch] - oceanLp2[ch]) * 0.08f;
                }
                float o = 0.5f * cur.ocean * (0.25f + 0.75f * swell);
                L += oceanLp2[0] * o; R += oceanLp2[1] * o;
            }


            // strings: per chord 6 notes x 5 detuned saws, spread across the stereo field, one filter
            float sL = 0.0f, sR = 0.0f, cMono[2] = {0.0f, 0.0f};
            static const float DET[5] = {-0.0065f, -0.003f, 0.0f, 0.003f, 0.0065f};
            for (int c = 0; c < 4; c++) {
                Strings& st = strings[c];
                st.amp += (w[c] * strLvl - st.amp) * strK * (1.0f / n);
                Choir& co = choir[c];
                co.amp += (w[c] * choLvl - co.amp) * strK * (1.0f / n);
                if (st.amp > 1e-5f)
                    for (int k = 0; k < 6; k++) {
                        float f = noteHz(k < 5 ? CHORDS[c][k] : CHORDS[c][0] - 12);
                        for (int q = 0; q < 5; q++) {
                            float fq = f * (1.0f + DET[q]);
                            advance(st.ph[k][q], fq);
                            float v = sawBlep(st.ph[k][q], fq / RATE) * st.amp * (k == 5 ? 1.3f : 1.0f);
                            float pan = (q - 2) * 0.4f + (k - 2.5f) * 0.06f;
                            sL += v * (0.5f - 0.5f * pan); sR += v * (0.5f + 0.5f * pan);
                        }
                    }
                if (co.amp > 1e-5f)
                    for (int k = 0; k < 3; k++) {
                        float f = noteHz(CHORDS[c][k + 2]);
                        for (int q = 0; q < 3; q++) {
                            float fq = f * (1.0f + DET[q + 1] * 1.4f);
                            advance(co.ph[k][q], fq);
                            cMono[q & 1] += sawBlep(co.ph[k][q], fq / RATE) * co.amp;
                        }
                    }
            }
            for (int ch = 0; ch < 2; ch++) {
                float x = ch ? sR : sL;
                strLp[ch][0] += (x - strLp[ch][0]) * strA;
                strLp[ch][1] += (strLp[ch][0] - strLp[ch][1]) * strA;
            }
            L += strLp[0][1] * 0.055f; R += strLp[1][1] * 0.055f;

            // choir: "aah" from three vowel formants (800, 1150, 2900 Hz) on the saws
            if (cMono[0] != 0.0f || cMono[1] != 0.0f) {
                static const float FF[3] = {800.0f, 1150.0f, 2900.0f}, FG[3] = {1.0f, 0.55f, 0.22f};
                for (int ch = 0; ch < 2; ch++) {
                    float x = cMono[ch], y = 0.0f;
                    for (int f = 0; f < 3; f++) {
                        float fr = 2.0f * std::sin(PI * FF[f] * (ch ? 1.02f : 1.0f) / RATE);
                        float& lo = formant[ch][f][0];
                        float& bp = formant[ch][f][1];
                        lo += fr * bp;
                        float hi = x - lo - 0.12f * bp;
                        bp += fr * hi;
                        y += bp * FG[f];
                    }
                    (ch ? R : L) += y * 0.018f;
                }
            }

            // the theme: a bowed line (two saws, an octave above joining as we fall), soft attacks, vibrato
            if (melLvl > 1e-3f) {
                melEnv  += (1.0f - melEnv) * melAtk * (1.0f / n);
                melFreq += (melTarget - melFreq) * melGlide * (1.0f / n);
                float vib = 1.0f + 0.0035f * melEnv * std::sin(2 * PI * 5.2f * (float)(ts + i / (double)RATE));
                float f = melFreq * vib;
                advance(melPh[0], f); advance(melPh[1], f * 1.004f); advance(melPh[2], f * 2.0f);
                float m = sawBlep(melPh[0], f / RATE) + sawBlep(melPh[1], f * 1.004f / RATE)
                        + melOct * 0.7f * sawBlep(melPh[2], 2.0f * f / RATE);
                for (int ch = 0; ch < 2; ch++) {
                    melLp[ch][0] += (m - melLp[ch][0]) * melA;
                    melLp[ch][1] += (melLp[ch][0] - melLp[ch][1]) * melA;
                }
                float g = 0.05f * melLvl * melEnv;
                L += melLp[0][1] * g * 1.05f; R += melLp[1][1] * g * 0.95f;
            }

            // braams: low brass and a horn section an octave and a fifth up swell in over ~0.3 s, blare open and
            // hold for seconds, overblown into a slight growl. Under them a clean sine sub an octave below (and
            // two below where that is still audible) that swells with the brass, no attack of its own, so it
            // never reads as a kick drum. The sub stays dry: low end in the hall turns to mud.
            float sub2 = 0.0f;
            for (Braam& br : braam) {
                if (br.age < 0.0f) continue;
                float tb = br.age / RATE;
                float env = (1.0f - std::exp(-tb / 0.3f)) * std::exp(-tb / 4.5f);
                static const float MUL[10] = {1.0f, 1.004f, 0.997f, 1.4983f, 1.503f, 2.0f, 2.006f, 1.997f, 2.997f, 3.004f};
                static const float GAIN[10] = {1.0f, 1.0f, 0.8f, 0.55f, 0.55f, 0.7f, 0.7f, 0.5f, 0.35f, 0.35f};
                float saw[2] = {0.0f, 0.0f};
                for (int q = 0; q < 10; q++) {
                    float f = br.freq * MUL[q];
                    advance(br.ph[q], f);
                    saw[q & 1] += sawBlep(br.ph[q], f / RATE) * GAIN[q];
                }
                float bloom = (1.0f - std::exp(-tb / 0.35f)) * (0.55f + 0.45f * std::exp(-tb / 2.5f));
                float fc = 250.0f + 3600.0f * (0.4f + 0.6f * br.amp) * bloom;
                float a = 1.0f - std::exp(-2 * PI * fc / RATE);
                for (int ch = 0; ch < 2; ch++) {
                    br.lp[ch][0] += (saw[ch] - br.lp[ch][0]) * a;
                    br.lp[ch][1] += (br.lp[ch][0] - br.lp[ch][1]) * a;
                }
                float brass = 0.75f * br.amp * env;
                L += std::tanh(1.8f * br.lp[0][1]) * brass; R += std::tanh(1.8f * br.lp[1][1]) * brass;

                float f1 = br.freq * 0.5f, f2 = br.freq * 0.25f, s2 = 0.0f;
                advance(br.sub[0], f1);
                if (f2 >= 26.0f) { advance(br.sub[1], f2); s2 = 0.55f * std::sin(2 * PI * br.sub[1]); }
                float senv = (1.0f - std::exp(-tb / 0.4f)) * std::exp(-tb / 4.5f);
                sub2 += br.amp * senv * (std::sin(2 * PI * br.sub[0]) + s2) * 0.6f;

                br.age += 1.0f;
                if (tb > 30.0f) br.age = -1.0f;
            }

            // hall, then the master level (after the reverb, so a cut to black is true silence), then a
            // limiter that keeps the low end loud without breaking up, and a soft clip
            float wetL = reverb(0, (L + R) * 0.5f + L * 0.2f), wetR = reverb(1, (L + R) * 0.5f + R * 0.2f);
            float lv = levelFrom + (cur.level - levelFrom) * fi;
            float oL = (L * 0.7f + wetL * 0.55f + sub2) * lv, oR = (R * 0.7f + wetR * 0.55f + sub2) * lv;
            float peak = std::max(std::fabs(oL), std::fabs(oR));
            limEnv = peak > limEnv ? limEnv + (peak - limEnv) * 0.05f : limEnv * 0.99995f;   // fast attack, ~0.4 s release
            float g = limEnv > 0.9f ? 0.9f / limEnv : 1.0f;
            lr[2 * (start + i)]     = std::tanh(oL * g * 1.2f) * 0.85f;
            lr[2 * (start + i) + 1] = std::tanh(oR * g * 1.2f) * 0.85f;
        }
        for (size_t v = 0; v < voices.size(); v++) voices[v].amp = targetAmp[v];
    }
}
