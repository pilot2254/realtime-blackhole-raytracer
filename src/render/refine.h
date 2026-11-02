#pragma once
// Progressive refinement: while the picture does not change, re-trace it at full resolution with
// sub-pixel jitter and Ultra geodesics, a few tiles per frame, and average the samples. Converges in
// REFINE_MAX samples, then stops tracing altogether.
#include <vector>
#include "core/gl.h"
#include "sim/camera.h"

inline constexpr int REFINE_MAX = 64;
extern bool g_refineOn;

// returns the refined (averaged, bloomed) image, or nullptr while there is none yet
const Target* refineUpdate(int w, int h, const CamFrame& cf, float t, bool allowed);
std::vector<Target>& refineMips();
int  refineSamples();
