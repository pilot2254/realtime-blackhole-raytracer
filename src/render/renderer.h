#pragma once
// Scene ray tracing pass, bloom, and the final composite.
#include <vector>
#include "core/gl.h"
#include "sim/camera.h"
#include "render/overlay.h"

extern GLuint g_progScene, g_progDown, g_progUp, g_progComp, g_progCopy;

void createPrograms();
void setSceneUniforms(int w, int h, const CamFrame& cf, float t, float jx, float jy, float step);
void runBloom(const Target& src, std::vector<Target>& mips);
void composite(GLuint fbo, int w, int h, const Target& scene, const std::vector<Target>& mips, float t, float fade,
               const Overlay& ov, float exposure);
void copyTex(GLuint tex, float ox, float oy, float w, float h, float scale);
