#pragma once
// The film: camera keys (Hermite-spline interpolated) and timed text cues, with fades.
#include <string>
#include <vector>
#include "sim/camera.h"

struct Key { float t; CamState c; };

enum { CUE_TITLE = 0, CUE_SUBTITLE, CUE_CAPTION, CUE_CHAPTER, CUE_HUD, CUE_EHT, CUE_STYLES };   // HUD, EHT: effects, no text
struct Cue { float t0, t1; int style; std::string text; };

extern std::vector<Key> g_keys;
extern std::vector<Cue> g_cues;
extern float g_playhead, g_playSpeed, g_fadeIn, g_fadeOut;
extern float g_blackAt;                         // picture fully black from here on (0 = end of sequence)
extern bool  g_playing, g_loop;
extern bool  g_cuesOn, g_letterboxOn;
extern float g_letterbox;                       // aspect ratio of the letterbox
extern float g_massSun;                         // black hole mass for the HUD's real units

void     defaultSequence();                     // the GARGANTUA film (film/gargantua_film.cpp)
float    seqLength();
CamState evalSequence(float t);
float    seqFade(float t);
void     sortKeys();
void     addKey(float t, CamState c);
bool     saveSequence(const char* path);
bool     loadSequence(const char* path);
float    cueLevel(int style, float t);          // 0..1, with the same fades the overlay uses
