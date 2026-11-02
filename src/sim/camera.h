#pragma once
#include "core/math.h"

// Camera: spherical position around the hole (phi, el, dist), looking at it, turned by yaw/pitch.
// fall: 0 = hovering in place (rockets on), 1 = free fall from rest at infinity.
struct CamState {
    float phi = 0.0f, el = 0.19f, dist = 24.0f, roll = 0.0f, fov = 0.40f;   // fov = tan(half vertical FOV)
    float yaw = 0.0f, pitch = 0.0f, fall = 0.0f;
    static const int N = 8;
    float& operator[](int i) { return (&phi)[i]; }
    float  operator[](int i) const { return (&phi)[i]; }
};

struct CamFrame { V3 pos, fwd, right, up; float fov, fall; };

inline constexpr float MIN_DIST = 0.03f;

CamFrame camBasis(const CamState& c);
