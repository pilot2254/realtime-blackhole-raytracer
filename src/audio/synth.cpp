#include "audio/synth.h"
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
    // scene objects nearby: a ship's engines, a planet's ocean
    CamFrame cf = camBasis(c);
    for (const SceneObject& o : g_objects) {
        if (!objectActive(o, t, film) || (o.type != SO_SHIP && o.type != SO_PLANET)) continue;
        float d = len(objectPosition(o, t, film) - cf.pos) - o.size;
        if (o.type == SO_SHIP)   s.hum   = std::max(s.hum, smoothstepf(4.0f, 0.6f, d));
        if (o.type == SO_PLANET) s.ocean = std::max(s.ocean, smoothstepf(3.5f, 0.8f, d));
    }
    return s;
}

// After the cut to black the picture is gone but the music is not: the camera's state is held at the
// cut, the effects stop, and the organ resolves home and dies away in the hall over the end card.
SoundControls filmSound(float t) {
    float L = seqLength(), cut = g_blackAt > 0.0f ? std::min(g_blackAt, L) : L;
    if (t < cut) return soundControls(evalSequence(t), seqFade(t), t, true);
    SoundControls s = soundControls(evalSequence(cut - 0.01f), 1.0f, cut, true);
    s.level = std::exp(-(t - cut) / 3.2f);
    s.coda = 1.0f;
    s.tension = 0.45f;
    s.tickLevel = 0.0f; s.tickRate = 0.0f; s.roar = 0.0f; s.hum = 0.0f; s.ocean = 0.0f;
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
static float midiHz(int n) { return 440.0f * std::pow(2.0f, (n - 69) / 12.0f); }

// A minor: Am - Fmaj7 - C - G, one chord every 9 s with long crossfades
static const int CHORDS[4][5] = {{45, 52, 57, 60, 64}, {41, 48, 57, 60, 64}, {48, 55, 60, 64, 67}, {43, 50, 55, 59, 62}};
static const float CHORD_LEN = 9.0f, CROSSFADE = 3.0f;

// voice layout per chord: 5 base notes, 1 pedal, 5 octave-up, 1 shimmer
enum { BASE = 0, PEDAL = 5, OCTAVE = 6, SHIMMER = 11, PER_CHORD = 12 };

Synth::Synth() {
    buildTables();
    for (int c = 0; c < 4; c++) {
        for (int n = 0; n < 5; n++) voices.push_back({midiHz(CHORDS[c][n]), (n - 2) * 0.25f, 0});
        voices.push_back({midiHz(CHORDS[c][0] - 12), 0.0f, 0});
        for (int n = 0; n < 5; n++) voices.push_back({midiHz(CHORDS[c][n] + 12), (2 - n) * 0.3f, 1});
        voices.push_back({midiHz(CHORDS[c][0] + 26), 0.15f, 1});
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
        cur.hum       += (target.hum - cur.hum) * k;
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

        float targetAmp[4 * PER_CHORD];
        for (int c = 0; c < 4; c++)
            for (int v = 0; v < PER_CHORD; v++) {
                float a = v < PEDAL ? 0.10f * organ : v == PEDAL ? 0.16f * pedal : v < SHIMMER ? 0.055f * octave : 0.05f * shimmer;
                targetAmp[c * PER_CHORD + v] = a * w[c] * breath;
            }
        float sub = 0.14f * T * T;

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
            subPhase[0] += 27.5f / RATE; subPhase[0] -= (int)subPhase[0];
            subPhase[1] += 55.0f / RATE; subPhase[1] -= (int)subPhase[1];
            float d = sub * (std::sin(2 * PI * subPhase[0]) * 0.7f + std::sin(2 * PI * subPhase[1]) * 0.5f);
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
            float knock = std::sin(2 * PI * 620.0f * tickAge / RATE) + 0.15f * hpOut;
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

            // a ship's engines: low detuned saws, slowly throbbing
            if (cur.hum > 1e-3f) {
                const float hz[3] = {41.2f, 41.5f, 82.4f};
                float saw = 0;
                for (int k3 = 0; k3 < 3; k3++) {
                    humPhase[k3] += hz[k3] / RATE; humPhase[k3] -= (int)humPhase[k3];
                    saw += (2.0f * humPhase[k3] - 1.0f) * (k3 == 2 ? 0.5f : 1.0f);
                }
                float throb = 0.75f + 0.25f * std::sin(2 * PI * 0.5f * (float)(ts + i / (double)RATE));
                for (int ch = 0; ch < 2; ch++) humLp[ch] += (saw - humLp[ch]) * (ch ? 0.030f : 0.034f);
                L += humLp[0] * 0.16f * cur.hum * throb; R += humLp[1] * 0.16f * cur.hum * throb;
            }
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

            // hall, then the master level (after the reverb, so a cut to black is true silence), soft clip
            float wetL = reverb(0, (L + R) * 0.5f + L * 0.2f), wetR = reverb(1, (L + R) * 0.5f + R * 0.2f);
            float lv = levelFrom + (cur.level - levelFrom) * fi;
            float oL = (L * 0.7f + wetL * 0.55f) * lv, oR = (R * 0.7f + wetR * 0.55f) * lv;
            lr[2 * (start + i)]     = std::tanh(oL * 1.2f) * 0.85f;
            lr[2 * (start + i) + 1] = std::tanh(oR * 1.2f) * 0.85f;
        }
        for (size_t v = 0; v < voices.size(); v++) voices[v].amp = targetAmp[v];
    }
}
