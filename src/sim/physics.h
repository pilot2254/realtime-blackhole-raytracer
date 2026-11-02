#pragma once
// CPU-side relativity that mirrors the scene shader: observer velocities, Kerr-Schild fields,
// horizons and the ISCO, and the frequency shift straight ahead (for auto exposure).
#include "core/math.h"
#include "sim/camera.h"

// Schwarzschild: camera speed relative to the free-falling (rain) observer, outward
float camRadialVel(float dist, float fall);

// Kerr-Schild, same conventions as the shader: M = r_s / 2, spin along world -y, std = (x, z, -y)
V3    toStd(V3 v);
float kerrM();
float ksField(V3 x, float& f, V3& l);          // returns the Boyer-Lindquist radius
float kerrCamSpeed(float f, float fall);        // along l, relative to the Kerr-Schild normal observer

float iscoRs(float chi);                        // prograde innermost stable circular orbit, in r_s
float horizonRs(float chi, float sign);         // +1 outer, -1 inner horizon, in r_s

float viewShift(const CamState& c);             // frequency seen straight ahead / at infinity
float autoExposure(const CamState& c);          // exposure multiplier from viewShift
