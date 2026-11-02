#include "audio/audio.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include "film/sequence.h"

#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#endif

bool  g_soundOn = true;
float g_volume  = 0.7f;
bool  g_exAudio = true;

static int16_t toPcm(float x) { return (int16_t)std::lround(std::max(-1.0f, std::min(1.0f, x)) * 32767.0f); }

// ---------------------------------------------------------------- live
static const int NBUF = 6, BLOCK = 1024;
static HWAVEOUT s_wo = nullptr;
static WAVEHDR  s_hdr[NBUF];
static int16_t  s_pcm[NBUF][BLOCK * 2];
static Synth*   s_synth = nullptr;
static double   s_t = 0.0;

bool audioOpen() {
    WAVEFORMATEX wf = {};
    wf.wFormatTag = WAVE_FORMAT_PCM; wf.nChannels = 2; wf.nSamplesPerSec = Synth::RATE;
    wf.wBitsPerSample = 16; wf.nBlockAlign = 4; wf.nAvgBytesPerSec = Synth::RATE * 4;
    if (waveOutOpen(&s_wo, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) { s_wo = nullptr; return false; }
    for (int i = 0; i < NBUF; i++) {
        s_hdr[i] = WAVEHDR{};
        s_hdr[i].lpData = (LPSTR)s_pcm[i];
        s_hdr[i].dwBufferLength = BLOCK * 4;
        waveOutPrepareHeader(s_wo, &s_hdr[i], sizeof(WAVEHDR));
    }
    s_synth = new Synth();
    return true;
}

void audioClose() {
    if (!s_wo) return;
    waveOutReset(s_wo);
    for (int i = 0; i < NBUF; i++) waveOutUnprepareHeader(s_wo, &s_hdr[i], sizeof(WAVEHDR));
    waveOutClose(s_wo);
    s_wo = nullptr;
    delete s_synth; s_synth = nullptr;
}

void audioPump(double musicTime, bool running, const SoundControls& ctl) {
    if (!s_wo) return;
    // the synth runs up to the queue length ahead; resync when the playhead jumps or stops
    double ahead = NBUF * BLOCK / (double)Synth::RATE;
    if (!running || s_t < musicTime - 0.1 || s_t > musicTime + ahead + 0.25) s_t = musicTime;
    std::vector<float> buf(BLOCK * 2);
    for (int i = 0; i < NBUF; i++) {
        if (s_hdr[i].dwFlags & WHDR_INQUEUE) continue;
        s_synth->render(buf.data(), BLOCK, s_t, ctl);
        if (running) s_t += BLOCK / (double)Synth::RATE;
        for (int k = 0; k < BLOCK * 2; k++) s_pcm[i][k] = toPcm(buf[k]);
        waveOutWrite(s_wo, &s_hdr[i], sizeof(WAVEHDR));
    }
}

// ---------------------------------------------------------------- offline soundtrack
static void put32(FILE* f, uint32_t v) { std::fwrite(&v, 4, 1, f); }
static void put16(FILE* f, uint16_t v) { std::fwrite(&v, 2, 1, f); }

bool renderSoundtrack(const std::string& path, float seconds) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const int frames = (int)std::ceil(seconds * Synth::RATE);
    const uint32_t bytes = (uint32_t)frames * 4;
    std::fwrite("RIFF", 1, 4, f); put32(f, 36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, Synth::RATE); put32(f, Synth::RATE * 4); put16(f, 4); put16(f, 16);
    std::fwrite("data", 1, 4, f); put32(f, bytes);

    Synth synth;
    const int B = 512;
    std::vector<float> buf(B * 2);
    std::vector<int16_t> pcm(B * 2);
    for (int i = 0; i < frames; i += B) {
        int n = std::min(B, frames - i);
        float t = i / (float)Synth::RATE;
        synth.render(buf.data(), n, t, filmSound(t));
        for (int k = 0; k < n * 2; k++) pcm[k] = toPcm(buf[k]);
        std::fwrite(pcm.data(), 2, (size_t)n * 2, f);
    }
    std::fclose(f);
    return true;
}
