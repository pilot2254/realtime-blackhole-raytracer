#pragma once
// Scene objects the user can add and delete: hot spots, planets, ships and jets. They are traced along
// the same bent light rays as the disk, so they are lensed, Doppler shifted and seen in secondary images.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "core/math.h"
#include "sim/camera.h"

enum { SO_HOTSPOT = 0, SO_PLANET, SO_SHIP, SO_JET, SO_TYPES };
inline constexpr int MAX_OBJECTS = 8;           // traced per frame (the visible ones)

struct SceneObject {
    int   type = SO_HOTSPOT;
    bool  visible = true;
    float t0 = 0.0f, t1 = 0.0f;                 // shown only between t0 and t1 (film time); t1 <= t0 = always
    // orbit around the hole (hot spot, planet, orbiting ship)
    float orbitR = 5.0f, phase = 0.0f, speed = 0.2f, incline = 0.0f;   // r_s, rad, rad per second, rad
    float size = 0.3f;                          // radius, r_s
    float temp = 3500.0f, bright = 3.0f;        // hot spot / jet emission
    V3    color = {0.20f, 0.33f, 0.40f};        // planet ocean / ship hull
    // ship: chase the camera instead of orbiting
    bool  follow = false;
    V3    offset = {1.4f, -0.35f, -0.12f};      // ahead, right, up, in camera space (r_s)
    float spin = 0.4f;                          // ring rotation, rad per second / jet speed (v/c)
    char  name[48] = "Object";
};
extern std::vector<SceneObject> g_objects;

const char* objectTypeName(int type);
SceneObject makeObject(int type);               // a sensible default of each kind
bool  objectActive(const SceneObject& o, float t, bool film);
V3    objectPosition(const SceneObject& o, float t, const CamFrame& cam, V3* velocity = nullptr, V3* axis = nullptr);
float orbitClockRate(float r);                  // circular-orbit time dilation (dt/dtau), < 0 if no such orbit
uint64_t objectsHash();

// packed for the shader: per object 4 vec4s
struct ObjectGPU { float a[4], b[4], c[4], v[4]; };
int  gatherObjects(float t, const CamFrame& cam, bool film, ObjectGPU out[MAX_OBJECTS]);

void saveObjects(FILE* f);
bool parseObjectLine(const char* line, SceneObject& o);
