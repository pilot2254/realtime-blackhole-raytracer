#pragma once
// The Unreal-style editor around the viewport.
#include "core/gl.h"

enum { Q_LOW = 0, Q_MEDIUM, Q_HIGH, Q_ULTRA };
extern const char* QUALITY_NAMES[4];
extern int g_quality;
void applyQuality(int q);

extern int g_viewW, g_viewH;    // size the viewport panel wants the scene image to be, in pixels

void editorInit();              // fonts and theme; call after ImGui::CreateContext
void enterCinema();             // full-window picture, no UI
void drawEditor(float fps, GLuint viewTex, int renderW, int renderH);
