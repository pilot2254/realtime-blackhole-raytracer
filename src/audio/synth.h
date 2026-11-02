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
};
SoundControls soundControls(const CamState& c, float fade);

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
    float tickEnv = 0, brown[2] = {0, 0}, roarLp[2] = {0, 0}, hpPrev = 0, hpOut = 0;
    uint32_t rng = 0x9E3779B9u;
    Comb comb[2][4];
    Allpass ap[2][2];

    float noise();
    float reverb(int ch, float x);
};
