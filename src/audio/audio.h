#pragma once
// Live playback through waveOut, and the offline soundtrack for video export.
#include <string>
#include "audio/synth.h"

extern bool  g_soundOn;         // live sound in the editor
extern float g_volume;
extern bool  g_exAudio;         // put the soundtrack into rendered videos

bool audioOpen();               // false if there is no output device (the app just stays silent)
void audioClose();
// keeps ~130 ms queued; musicTime = film time (or free-camera time), running = time is moving
void audioPump(double musicTime, bool running, const SoundControls& ctl);

// renders the film's soundtrack (camera path from the sequence) to a 48 kHz 16-bit stereo WAV
bool renderSoundtrack(const std::string& path, float seconds);
