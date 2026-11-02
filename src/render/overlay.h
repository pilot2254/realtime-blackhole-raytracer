#pragma once
// Titles, captions and the instrument HUD, drawn with GDI into a texture for the composite pass.
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "core/gl.h"
#include "film/sequence.h"

struct Overlay {
    HDC dc = nullptr; HBITMAP bmp = nullptr; uint8_t* bits = nullptr;
    int w = 0, h = 0;
    HFONT font[CUE_STYLES] = {};
    GLuint tex = 0;
    std::string sig;
    bool visible = false;
};
extern Overlay g_ovView, g_ovExport;

float barHeight(int w, int h);                  // letterbox bar height for an image size
void  overlayFree(Overlay& o);
void  overlayUpdate(Overlay& o, int w, int h, float t, bool film);
std::vector<std::pair<std::string, std::string>> hudLines(const CamState& c);
