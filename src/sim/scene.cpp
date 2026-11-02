#include "sim/scene.h"
#include <cmath>
#include <cstring>
#include "sim/params.h"

std::vector<SceneObject> g_objects;

const char* objectTypeName(int type) {
    static const char* names[SO_TYPES] = {"Hot Spot", "Planet", "Ship", "Relativistic Jet"};
    return type >= 0 && type < SO_TYPES ? names[type] : "Object";
}

SceneObject makeObject(int type) {
    SceneObject o;
    o.type = type;
    switch (type) {
    case SO_HOTSPOT: o.orbitR = 5.0f; o.size = 0.35f; o.temp = 3500.0f; o.bright = 3.0f; o.speed = 0.25f;
                     std::snprintf(o.name, sizeof o.name, "Hot Spot"); break;
    case SO_PLANET:  o.orbitR = 2.4f; o.size = 0.14f; o.speed = 0.02f; o.incline = -0.2f; o.color = {0.20f, 0.33f, 0.40f};
                     std::snprintf(o.name, sizeof o.name, "Planet"); break;
    case SO_SHIP:    o.orbitR = 8.0f; o.size = 0.12f; o.speed = 0.03f; o.color = {0.80f, 0.80f, 0.78f}; o.spin = 0.4f;
                     std::snprintf(o.name, sizeof o.name, "Ship"); break;
    case SO_JET:     o.size = 0.7f; o.orbitR = 40.0f; o.temp = 9000.0f; o.bright = 2.5f; o.spin = 0.9f;
                     std::snprintf(o.name, sizeof o.name, "Relativistic Jet"); break;
    }
    return o;
}

bool objectActive(const SceneObject& o, float t, bool film) {
    if (!o.visible) return false;
    return !film || o.t1 <= o.t0 || (t >= o.t0 && t <= o.t1);
}

// circular orbit, tilted by 'incline' about the x axis; velocity is the physical (Keplerian) one, used for
// the Doppler shift, while the cinematic angular speed is free
static V3 orbitPoint(float R, float phi, float incl) {
    V3 p = {R * std::cos(phi), 0.0f, R * std::sin(phi)};
    return {p.x, -p.z * std::sin(incl), p.z * std::cos(incl)};
}

V3 objectPosition(const SceneObject& o, float t, const CamFrame& cam, V3* velocity, V3* axis) {
    if (o.type == SO_JET) {
        if (velocity) *velocity = {0, 0, 0};
        if (axis) *axis = {0, 1, 0};
        return {0, 0, 0};
    }
    if (o.type == SO_SHIP && o.follow) {
        // overtakes the camera from behind when it appears, pulls away ahead before it disappears
        float ahead = o.offset.x;
        if (o.t1 > o.t0) {
            float in  = smoothstepf(o.t0, o.t0 + 7.0f, t);
            float out = smoothstepf(o.t1 - 6.0f, o.t1, t);
            ahead = -0.8f + (o.offset.x + 0.8f) * in + 9.0f * out * out;
        }
        if (velocity) *velocity = {0, 0, 0};
        if (axis) *axis = cam.fwd;
        return cam.pos + cam.fwd * ahead + cam.right * o.offset.y + cam.up * o.offset.z;
    }
    float phi = o.phase + o.speed * t;
    V3 p = orbitPoint(o.orbitR, phi, o.incline);
    if (velocity) {
        V3 tang = norm(orbitPoint(o.orbitR, phi + 0.001f, o.incline) - p);
        float beta = std::sqrt(0.5f * std::max(P.gravity, 0.0f) / std::max(o.orbitR, 1.05f));
        *velocity = tang * std::min(beta, 0.9f);
    }
    if (axis) *axis = norm(cross(p, orbitPoint(o.orbitR, phi + 0.001f, o.incline)));
    return p;
}

float orbitClockRate(float r) {
    float M = 0.5f * std::max(P.gravity, 1e-3f), chi = P.spin;
    float x = r / M;                                     // radius in units of M
    float den = std::pow(x, 1.5f) - 3.0f * std::sqrt(x) + 2.0f * chi;
    if (den <= 0.0f) return -1.0f;
    return (std::pow(x, 1.5f) + chi) / (std::pow(x, 0.75f) * std::sqrt(den));
}

uint64_t objectsHash() {
    uint64_t h = 1469598103934665603ull;
    for (const SceneObject& o : g_objects) {
        const uint8_t* b = (const uint8_t*)&o;
        for (size_t i = 0; i < sizeof o; i++) { h ^= b[i]; h *= 1099511628211ull; }
    }
    return h;
}

int gatherObjects(float t, const CamFrame& cam, bool film, ObjectGPU out[MAX_OBJECTS]) {
    int n = 0;
    for (const SceneObject& o : g_objects) {
        if (n >= MAX_OBJECTS || !objectActive(o, t, film)) continue;
        V3 vel, axis;
        V3 p = objectPosition(o, t, cam, &vel, &axis);
        ObjectGPU& g = out[n++];
        g = ObjectGPU{};
        g.a[0] = p.x; g.a[1] = p.y; g.a[2] = p.z; g.a[3] = o.size;
        g.b[0] = (float)o.type; g.b[1] = o.temp; g.b[2] = o.bright; g.b[3] = o.type == SO_JET ? o.orbitR : o.spin * t;
        g.c[0] = o.color.x; g.c[1] = o.color.y; g.c[2] = o.color.z; g.c[3] = o.type == SO_JET ? o.spin : 0.0f;
        if (o.type == SO_HOTSPOT) { g.v[0] = vel.x; g.v[1] = vel.y; g.v[2] = vel.z; }
        else                      { g.v[0] = axis.x; g.v[1] = axis.y; g.v[2] = axis.z; }
    }
    return n;
}

// ---------------------------------------------------------------- save / load (lines in the sequence file)
void saveObjects(FILE* f) {
    std::fprintf(f, "# obj type visible t0 t1 orbitR phase speed incline size temp bright r g b follow ahead right up spin name\n");
    for (const SceneObject& o : g_objects)
        std::fprintf(f, "obj %d %d %f %f %f %f %f %f %f %f %f %f %f %f %d %f %f %f %f %s\n", o.type, o.visible ? 1 : 0, o.t0, o.t1,
                     o.orbitR, o.phase, o.speed, o.incline, o.size, o.temp, o.bright, o.color.x, o.color.y, o.color.z,
                     o.follow ? 1 : 0, o.offset.x, o.offset.y, o.offset.z, o.spin, o.name);
}

bool parseObjectLine(const char* line, SceneObject& o) {
    int vis = 1, fol = 0, n = 0;
    o = SceneObject{};
    if (std::sscanf(line, "obj %d %d %f %f %f %f %f %f %f %f %f %f %f %f %d %f %f %f %f %n", &o.type, &vis, &o.t0, &o.t1,
                    &o.orbitR, &o.phase, &o.speed, &o.incline, &o.size, &o.temp, &o.bright, &o.color.x, &o.color.y, &o.color.z,
                    &fol, &o.offset.x, &o.offset.y, &o.offset.z, &o.spin, &n) < 19 || n == 0) return false;
    if (o.type < 0 || o.type >= SO_TYPES) return false;
    o.visible = vis != 0; o.follow = fol != 0;
    std::snprintf(o.name, sizeof o.name, "%s", line + n);
    return true;
}
