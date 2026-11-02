#include "render/refine.h"
#include <cstring>
#include "core/math.h"
#include "core/util.h"
#include "render/renderer.h"
#include "sim/params.h"
#include "sim/scene.h"

bool g_refineOn = true;

static Target s_sample, s_accum, s_avg;
static std::vector<Target> s_mips;
static int s_n = 0, s_tile = 0;
static uint64_t s_key = 0;
static double s_since = 0.0;
static const int TILE_PX = 128;
static const double STILL_DELAY = 0.3, BUDGET = 0.010;     // seconds

static uint64_t fnv(const void* p, size_t n, uint64_t h) {
    const uint8_t* b = (const uint8_t*)p;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}

int refineSamples() { return s_n; }
std::vector<Target>& refineMips() { return s_mips; }

const Target* refineUpdate(int w, int h, const CamFrame& cf, float t, bool film, bool allowed) {
    // anything that changes the traced image restarts the refinement
    uint64_t key = 1469598103934665603ull;
    key = fnv(&cf, sizeof cf, key);
    key = fnv(&t, sizeof t, key);
    key = fnv(&P, sizeof P, key);
    key = fnv(&w, sizeof w, key);
    key = fnv(&h, sizeof h, key);
    uint64_t oh = objectsHash();
    key = fnv(&oh, sizeof oh, key);
    key = fnv(&film, sizeof film, key);
    double now = nowSeconds();
    if (!allowed || key != s_key) { s_key = key; s_n = 0; s_tile = 0; s_since = now; return nullptr; }
    if (now - s_since < STILL_DELAY) return nullptr;

    if (s_sample.w != w || s_sample.h != h) {
        freeTarget(s_sample); freeTarget(s_accum); freeTarget(s_avg);
        s_sample = makeTarget(w, h); s_accum = makeTarget(w, h); s_avg = makeTarget(w, h);
        buildMips(s_mips, w, h);
        s_n = 0; s_tile = 0;
    }

    if (s_n < REFINE_MAX) {
        int tx = (w + TILE_PX - 1) / TILE_PX, ty = (h + TILE_PX - 1) / TILE_PX;
        LARGE_INTEGER t0; QueryPerformanceCounter(&t0);
        bool uniforms = false;
        while (s_n < REFINE_MAX && secondsSince(t0) < BUDGET) {
            if (!uniforms) {
                float jx = halton(s_n + 1, 2) - 0.5f, jy = halton(s_n + 1, 3) - 0.5f;
                setSceneUniforms(w, h, cf, t, jx, jy, 0.012f, film);
                uniforms = true;
            }
            p_glBindFramebuffer(K_FRAMEBUFFER, s_sample.fbo);
            glViewport(0, 0, w, h);
            glEnable(GL_SCISSOR_TEST);
            glScissor((s_tile % tx) * TILE_PX, (s_tile / tx) * TILE_PX, TILE_PX, TILE_PX);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glDisable(GL_SCISSOR_TEST);
            glFinish();                                     // short submissions keep the UI responsive
            if (++s_tile < tx * ty) continue;

            // one full sample done: add it up and refresh the average
            s_tile = 0;
            p_glBindFramebuffer(K_FRAMEBUFFER, s_accum.fbo);
            glViewport(0, 0, w, h);
            if (s_n == 0) { glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT); }
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            copyTex(s_sample.tex, 0, 0, (float)w, (float)h, 1.0f);
            glDisable(GL_BLEND);
            s_n++;
            p_glBindFramebuffer(K_FRAMEBUFFER, s_avg.fbo);
            copyTex(s_accum.tex, 0, 0, (float)w, (float)h, 1.0f / s_n);
            uniforms = false;                               // next sample, new jitter
        }
    }
    if (s_n == 0) return nullptr;
    runBloom(s_avg, s_mips);
    return &s_avg;
}
