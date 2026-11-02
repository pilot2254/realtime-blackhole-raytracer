#include "render/renderer.h"
#include "render/shaders.h"
#include "sim/params.h"
#include "sim/physics.h"
#include "sim/scene.h"
#include "film/sequence.h"

GLuint g_progScene, g_progDown, g_progUp, g_progComp, g_progCopy;

void createPrograms() {
    g_progScene = makeProgram(VS_FULLSCREEN, {FS_SCENE_A, FS_SCENE_B, FS_SCENE_C}, "scene.frag");
    g_progDown  = makeProgram(VS_FULLSCREEN, {FS_DOWN}, "bloom_down.frag");
    g_progUp    = makeProgram(VS_FULLSCREEN, {FS_UP}, "bloom_up.frag");
    g_progComp  = makeProgram(VS_FULLSCREEN, {FS_COMPOSITE}, "composite.frag");
    g_progCopy  = makeProgram(VS_FULLSCREEN, {FS_COPY}, "copy.frag");
}

void setSceneUniforms(int w, int h, const CamFrame& cf, float t, float jx, float jy, float step, bool film) {
    GLuint p = g_progScene;
    p_glUseProgram(p);
    u2f(p, "uRes", (float)w, (float)h);
    u2f(p, "uJitter", jx, jy);
    u1f(p, "uTime", t);
    u3f(p, "uCamPos", cf.pos.x, cf.pos.y, cf.pos.z);
    u3f(p, "uCamFwd", cf.fwd.x, cf.fwd.y, cf.fwd.z);
    u3f(p, "uCamRight", cf.right.x, cf.right.y, cf.right.z);
    u3f(p, "uCamUp", cf.up.x, cf.up.y, cf.up.z);
    u1f(p, "uFov", cf.fov);
    u1f(p, "uStep", step);
    V3 vel = norm(cf.pos) * camRadialVel(len(cf.pos), cf.fall);
    u3f(p, "uCamVel", vel.x, vel.y, vel.z);
    u1f(p, "uObsShift", P.obsShift);
    u1f(p, "uSpin", P.spin);
    u1f(p, "uFall", cf.fall);
    u1f(p, "uGravity", P.gravity);
    u1f(p, "uDiskOn", P.disk ? 1.0f : 0.0f);
    u1f(p, "uDiskIn", P.diskIn);
    u1f(p, "uDiskOut", P.diskOut);
    u1f(p, "uTempPeak", P.tempPeak);
    u1f(p, "uDiskBright", P.diskBright);
    u1f(p, "uDiskOpacity", P.diskOpacity);
    u1f(p, "uSwirl", P.swirl);
    u1f(p, "uDoppler", P.doppler);
    u1f(p, "uRedshift", P.redshift);
    u1f(p, "uGlow", P.glow);
    u1f(p, "uStars", P.stars);
    u1f(p, "uMilky", P.milky);
    u1f(p, "uNebula", P.nebula);
    u1f(p, "uGalaxy", P.galaxy);

    ObjectGPU obj[MAX_OBJECTS];
    int n = gatherObjects(t, film, obj);
    float A[4 * MAX_OBJECTS] = {}, B[4 * MAX_OBJECTS] = {}, C[4 * MAX_OBJECTS] = {}, V[4 * MAX_OBJECTS] = {};
    for (int i = 0; i < n; i++)
        for (int k = 0; k < 4; k++) { A[4 * i + k] = obj[i].a[k]; B[4 * i + k] = obj[i].b[k]; C[4 * i + k] = obj[i].c[k]; V[4 * i + k] = obj[i].v[k]; }
    u1i(p, "uObjCount", n);
    u4fv(p, "uObjA", MAX_OBJECTS, A);
    u4fv(p, "uObjB", MAX_OBJECTS, B);
    u4fv(p, "uObjC", MAX_OBJECTS, C);
    u4fv(p, "uObjV", MAX_OBJECTS, V);
}

float telescopeAmount(float t, bool film) {
    float cue = film && g_cuesOn ? cueLevel(CUE_EHT, t) : 0.0f;
    return std::max(P.telescope, cue);
}

void runBloom(const Target& src, std::vector<Target>& mips) {
    p_glUseProgram(g_progDown);
    u1i(g_progDown, "uSrc", 0);
    const Target* s = &src;
    for (auto& m : mips) {
        p_glBindFramebuffer(K_FRAMEBUFFER, m.fbo);
        glViewport(0, 0, m.w, m.h);
        bindTex(0, s->tex);
        u2f(g_progDown, "uSrcTexel", 1.0f / s->w, 1.0f / s->h);
        u2f(g_progDown, "uDstRes", (float)m.w, (float)m.h);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        s = &m;
    }
    p_glUseProgram(g_progUp);
    u1i(g_progUp, "uSrc", 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (int i = (int)mips.size() - 1; i > 0; i--) {
        Target& d = mips[i - 1];
        p_glBindFramebuffer(K_FRAMEBUFFER, d.fbo);
        glViewport(0, 0, d.w, d.h);
        bindTex(0, mips[i].tex);
        u2f(g_progUp, "uSrcTexel", 1.0f / mips[i].w, 1.0f / mips[i].h);
        u2f(g_progUp, "uDstRes", (float)d.w, (float)d.h);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glDisable(GL_BLEND);
}

void composite(GLuint fbo, int w, int h, const Target& scene, const std::vector<Target>& mips, float t, float fade,
                      const Overlay& ov, float exposure, float telescope, float shake) {
    p_glBindFramebuffer(K_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
    GLuint p = g_progComp;
    p_glUseProgram(p);
    bindTex(0, scene.tex);
    bindTex(1, mips.empty() ? scene.tex : mips[0].tex);
    bindTex(2, ov.tex ? ov.tex : scene.tex);
    u1i(p, "uScene", 0);
    u1i(p, "uBloom", 1);
    u1i(p, "uOverlay", 2);
    u1f(p, "uOverlayOn", ov.visible && ov.tex ? 1.0f : 0.0f);
    u1f(p, "uBar", barHeight(w, h));
    u2f(p, "uRes", (float)w, (float)h);
    u1f(p, "uTime", t);
    u1f(p, "uBloomStr", P.bloom);
    u1f(p, "uExposure", exposure);
    u1f(p, "uVignette", P.vignette);
    u1f(p, "uCA", P.ca);
    u1f(p, "uGrain", P.grain);
    u1f(p, "uFade", fade);
    const Target& blur = mips.empty() ? scene : mips[std::min<size_t>(3, mips.size() - 1)];
    bindTex(3, blur.tex);
    u1i(p, "uBlur", 3);
    u2f(p, "uBlurTexel", 1.0f / blur.w, 1.0f / blur.h);
    u1f(p, "uEHT", telescope);
    u1f(p, "uShake", shake * P.shake);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    p_glActiveTexture(K_TEXTURE0);
}

void copyTex(GLuint tex, float ox, float oy, float w, float h, float scale) {
    p_glUseProgram(g_progCopy);
    bindTex(0, tex);
    u1i(g_progCopy, "uSrc", 0);
    u2f(g_progCopy, "uOffset", ox, oy);
    u2f(g_progCopy, "uRes", w, h);
    u1f(g_progCopy, "uScale", scale);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
