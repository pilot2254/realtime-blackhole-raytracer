#pragma once
// The soundtrack, synthesised from the physics: a pipe organ that builds as the camera nears the
// horizon, a clock that ticks once per second of far-away time (so it speeds up with time dilation and
// stops inside the horizon), and a low roar near the disk. Deterministic: the same camera path always
// gives the same sound, so the video export renders it offline, sample-exact.
#include <cstdint>
#include <vector>
#include "sim/camera.h"

struct SoundControls {
    float level = 0.0f;         // master, follows the film's fades (0 = silence)
    float tension = 0.0f;       // 0 far away .. 1 at and inside the horizon
    float tickRate = 0.0f;      // ticks per second, 0 = no clock
    float tickLevel = 0.0f;
    float roar = 0.0f;          // disk proximity
    float shift = 1.0f;         // frequency shift straight ahead (brightens the roar)
    float hum = 0.0f;           // a ship close by
    float ocean = 0.0f;         // a planet's ocean close by
    float coda = 0.0f;          // 1 after the film's cut to black: resolve to A minor and ring out
};
// t: film time when film = true, else free-camera time (scene objects move with it)
SoundControls soundControls(const CamState& c, float fade, float t, bool film);
SoundControls filmSound(float t);               // the film at time t, including the ending after the cut

class Synth {
public:
    static constexpr int RATE = 48000;
    Synth();
    // interleaved stereo floats; t = musical time of the first frame, in seconds
    void render(float* lr, int frames, double t, const SoundControls& target);

private:
    struct Voice { float freq, pan; int table; float phA = 0, phB = 0, amp = 0; };
    struct Comb { std::vector<float> buf; int pos = 0; float store = 0; };
    struct Allpass { std::vector<float> buf; int pos = 0; };

    std::vector<Voice> voices;
    SoundControls cur;
    float subPhase[2] = {0, 0};
    double tickPhase = 0;
    float tickEnv = 0, tickAge = 0, tickLp = 0, brown[2] = {0, 0}, roarLp[2] = {0, 0}, hpPrev = 0, hpOut = 0;
    float humPhase[3] = {0, 0, 0}, humLp[2] = {0, 0}, oceanLp[2] = {0, 0}, oceanLp2[2] = {0, 0};
    float codaMix = 0.0f;
    uint32_t rng = 0x9E3779B9u;
    Comb comb[2][4];
    Allpass ap[2][2];

    float noise();
    float reverb(int ch, float x);
};
