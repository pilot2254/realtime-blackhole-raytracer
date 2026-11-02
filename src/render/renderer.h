#pragma once
// Scene ray tracing pass, bloom, and the final composite.
#include <vector>
#include "core/gl.h"
#include "sim/camera.h"
#include "render/overlay.h"

extern GLuint g_progScene, g_progDown, g_progUp, g_progComp, g_progCopy;

void createPrograms();
// film = true: t is film time (objects use their film windows)
void setSceneUniforms(int w, int h, const CamFrame& cf, float t, float jx, float jy, float step, bool film);
void runBloom(const Target& src, std::vector<Target>& mips);
void composite(GLuint fbo, int w, int h, const Target& scene, const std::vector<Target>& mips, float t, float fade,
               const Overlay& ov, float exposure, float telescope, float shake = 0.0f);
float telescopeAmount(float t, bool film);       // EHT view: the manual setting or the film's cue
void copyTex(GLuint tex, float ox, float oy, float w, float h, float scale);
