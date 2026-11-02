#pragma once
// Shared between the editor's source files: theme, widgets, selection, panels.
#include "imgui.h"
#include "imgui_internal.h"
#include "core/gl.h"

namespace ed {

// ---------------------------------------------------------------- theme
extern ImFont* fontBold;
inline ImVec4 rgb(int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); }
inline const ImVec4 BLUE      = rgb(0, 112, 224);       // Unreal's selection blue
inline const ImVec4 GREEN     = rgb(76, 200, 84);       // play
inline const ImVec4 RED       = rgb(230, 72, 60);       // record, playhead
inline const ImVec4 TEXT_DIM  = rgb(128, 128, 128);
void applyTheme();

// ---------------------------------------------------------------- selection (Outliner / Sequencer -> Details)
enum { OBJ_BLACKHOLE, OBJ_DISK, OBJ_SKY, OBJ_CAMERA, OBJ_POST, OBJ_FILM, OBJ_COUNT };
enum { SEL_OBJECT, SEL_KEY, SEL_CUE };
extern int selKind, selObject, selKey, selCue;

// ---------------------------------------------------------------- icons and widgets
enum Icon {
    IC_PLAY, IC_PAUSE, IC_STOP, IC_START, IC_END, IC_LOOP, IC_SAVE, IC_OPEN, IC_SHOT, IC_RECORD, IC_CINEMA,
    IC_SETTINGS, IC_FOLDER, IC_PLUS, IC_EYE, IC_EYE_OFF, IC_KEY, IC_SOUND, IC_MUTE,
    IC_BLACKHOLE, IC_DISK, IC_SKY, IC_CAMERA, IC_POST, IC_FILM, IC_TEXT, IC_HUD
};
void drawIcon(ImDrawList* dl, int icon, ImVec2 c, float s, ImU32 col);
float toolButtonWidth(const char* label);
bool  toolButton(const char* id, int icon, const char* label, const char* tip, bool active = false, ImU32 iconCol = 0);
void  toolSeparator();

bool category(const char* name, bool defaultOpen = true);   // collapsible section, like Details categories
bool beginProps(const char* id);                             // two-column name | value grid
void endProps();
void propLabel(const char* label, const char* tip = nullptr);
bool propFloat(const char* label, float* v, float lo, float hi, const char* format = "%.2f", ImGuiSliderFlags fl = 0, const char* tip = nullptr);
bool propDrag(const char* label, float* v, float speed, float lo, float hi, const char* format = "%.2f", const char* tip = nullptr);
bool propAngle(const char* label, float* rad, float loDeg, float hiDeg, const char* tip = nullptr);
bool propFov(const char* label, float* tanHalf);
bool propCheck(const char* label, bool* v, const char* tip = nullptr);
bool propCombo(const char* label, int* v, const char* items, const char* tip = nullptr);
void propValue(const char* label, const char* text, const char* tip = nullptr);
bool primaryButton(const char* label, ImVec2 size);

// ---------------------------------------------------------------- panels
void panelOutliner();
void panelDetails();
void panelSequencer();
void panelRenderQueue();
void panelViewport(GLuint tex, int renderW, int renderH);

}  // namespace ed
