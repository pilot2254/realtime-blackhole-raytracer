#include "film/sequence.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "core/math.h"
#include "sim/scene.h"

std::vector<Key> g_keys;
std::vector<Cue> g_cues;
float g_playhead = 0.0f, g_playSpeed = 1.0f, g_fadeIn = 2.0f, g_fadeOut = 3.0f;
float g_blackAt = 0.0f;
bool  g_playing = true, g_loop = true;
bool  g_cuesOn = true, g_letterboxOn = true;
float g_letterbox = 2.39f;
float g_massSun = 1.0e8f;    // Gargantua, Kip Thorne's figure for the film

float seqLength() { return g_keys.empty() ? 0.0f : g_keys.back().t; }

CamState evalSequence(float t) {
    if (g_keys.empty()) return CamState{};
    if (g_keys.size() == 1 || t <= g_keys.front().t) return g_keys.front().c;
    if (t >= g_keys.back().t) return g_keys.back().c;
    size_t k = 0;
    while (k + 1 < g_keys.size() && g_keys[k + 1].t <= t) k++;
    const Key& a = g_keys[k];
    const Key& b = g_keys[k + 1];
    float h = std::max(1e-4f, b.t - a.t), u = (t - a.t) / h;
    auto tangent = [&](size_t i, int c) -> float {
        if (i == 0 || i + 1 >= g_keys.size()) return 0.0f;      // ease in/out at the ends
        float dtk = g_keys[i + 1].t - g_keys[i - 1].t;
        return dtk > 1e-4f ? (g_keys[i + 1].c[c] - g_keys[i - 1].c[c]) / dtk : 0.0f;
    };
    float h00 = 2 * u * u * u - 3 * u * u + 1, h10 = u * u * u - 2 * u * u + u;
    float h01 = -2 * u * u * u + 3 * u * u,    h11 = u * u * u - u * u;
    CamState out;
    for (int c = 0; c < CamState::N; c++)
        out[c] = h00 * a.c[c] + h10 * h * tangent(k, c) + h01 * b.c[c] + h11 * h * tangent(k + 1, c);
    out.dist = std::max(MIN_DIST, out.dist);
    out.fov  = std::max(0.05f, out.fov);
    out.fall = std::max(0.0f, std::min(1.0f, out.fall));
    return out;
}
float seqFade(float t) {
    float L = g_blackAt > 0 ? std::min(g_blackAt, seqLength()) : seqLength();
    float a = g_fadeIn  > 0 ? smoothstepf(0.0f, g_fadeIn, t) : 1.0f;
    float b = g_fadeOut > 0 ? smoothstepf(L, L - g_fadeOut, t) : (t <= L ? 1.0f : 0.0f);
    return a * b;
}
void sortKeys() { std::sort(g_keys.begin(), g_keys.end(), [](const Key& a, const Key& b) { return a.t < b.t; }); }

void addKey(float t, CamState c) {
    // unwrap azimuth so the camera takes the short way from the previous key
    for (auto& k : g_keys) if (k.t <= t) {
        while (c.phi - k.c.phi >  PI) c.phi -= 2 * PI;
        while (c.phi - k.c.phi < -PI) c.phi += 2 * PI;
    }
    for (auto& k : g_keys) if (std::fabs(k.t - t) < 1e-3f) { k.c = c; return; }
    g_keys.push_back({t, c});
    sortKeys();
}
bool saveSequence(const char* path) {
    FILE* f = std::fopen(path, "w");
    if (!f) return false;
    std::fprintf(f, "# time phi elevation distance roll fov yaw pitch fall\n");
    for (auto& k : g_keys)
        std::fprintf(f, "%f %f %f %f %f %f %f %f %f\n", k.t, k.c.phi, k.c.el, k.c.dist, k.c.roll, k.c.fov, k.c.yaw, k.c.pitch, k.c.fall);
    std::fprintf(f, "fade %f %f %f\n", g_fadeIn, g_fadeOut, g_blackAt);
    std::fprintf(f, "# cue start end style(0 title, 1 subtitle, 2 caption, 3 chapter, 4 HUD) text\n");
    for (auto& c : g_cues) std::fprintf(f, "cue %f %f %d %s\n", c.t0, c.t1, c.style, c.text.c_str());
    saveObjects(f);
    std::fclose(f);
    return true;
}
bool loadSequence(const char* path) {
    FILE* f = std::fopen(path, "r");
    if (!f) return false;
    std::vector<Key> keys;
    std::vector<Cue> cues;
    std::vector<SceneObject> objects;
    bool haveObjects = false;
    char line[1024];
    while (std::fgets(line, sizeof line, f)) {
        line[std::strcspn(line, "\r\n")] = 0;
        Key k; Cue c; SceneObject o; int n = 0;
        if (std::strncmp(line, "obj ", 4) == 0) {
            haveObjects = true;
            if (parseObjectLine(line, o)) objects.push_back(o);
        } else if (std::sscanf(line, "cue %f %f %d %n", &c.t0, &c.t1, &c.style, &n) == 3 && n > 0) {
            c.style = std::max(0, std::min(CUE_STYLES - 1, c.style));
            c.text = line + n;
            cues.push_back(c);
        } else if (std::sscanf(line, "fade %f %f %f", &g_fadeIn, &g_fadeOut, &g_blackAt) == 3) {
        } else if (std::sscanf(line, "%f %f %f %f %f %f %f %f %f", &k.t, &k.c.phi, &k.c.el, &k.c.dist, &k.c.roll, &k.c.fov,
                               &k.c.yaw, &k.c.pitch, &k.c.fall) >= 6) {
            keys.push_back(k);                     // old 6-column files keep yaw = pitch = fall = 0
        }
    }
    std::fclose(f);
    if (keys.empty()) return false;
    g_keys = keys; sortKeys();
    g_cues = cues;
    if (haveObjects) g_objects = objects;                   // older files keep the current objects
    return true;
}

float cueLevel(int style, float t) {
    float level = 0.0f;
    for (const Cue& c : g_cues)
        if (c.style == style) level = std::max(level, smoothstepf(c.t0, c.t0 + 1.2f, t) * smoothstepf(c.t1, c.t1 - 1.2f, t));
    return level;
}
