#pragma once
// Small vector maths and helpers. Header only.
#include <algorithm>
#include <cmath>

constexpr float PI = 3.14159265f;

struct V3 { float x, y, z; };
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float len(V3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
inline V3 norm(V3 a) { return a * (1.0f / len(a)); }

inline float smoothstepf(float e0, float e1, float x) {
    float t = std::min(1.0f, std::max(0.0f, (x - e0) / (e1 - e0)));
    return t * t * (3.0f - 2.0f * t);
}

// low-discrepancy sequence for sub-pixel jitter
inline float halton(int i, int b) {
    float f = 1.0f, r = 0.0f;
    while (i > 0) { f /= b; r += f * (i % b); i /= b; }
    return r;
}
