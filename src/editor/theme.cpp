// Unreal Engine 5 dark theme, icons drawn with the draw list, and the Details-style property grid.
#include <algorithm>
#include <cmath>
#include "app/app.h"
#include "core/math.h"
#include "editor/editor_internal.h"

namespace ed {

ImFont* fontBold = nullptr;

void applyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    ImGui::StyleColorsDark(&s);
    s.WindowPadding = ImVec2(8, 8);   s.FramePadding = ImVec2(6, 4);     s.CellPadding = ImVec2(6, 3);
    s.ItemSpacing = ImVec2(6, 5);     s.ItemInnerSpacing = ImVec2(5, 4); s.IndentSpacing = 14;
    s.ScrollbarSize = 11;             s.GrabMinSize = 6;
    s.WindowRounding = 0; s.ChildRounding = 2; s.FrameRounding = 3; s.PopupRounding = 4;
    s.ScrollbarRounding = 6; s.GrabRounding = 2; s.TabRounding = 4;
    s.WindowBorderSize = 0; s.ChildBorderSize = 0; s.PopupBorderSize = 1; s.FrameBorderSize = 0;
    s.TabBorderSize = 0; s.TabBarBorderSize = 0; s.TabBarOverlineSize = 2;
    s.DockingSeparatorSize = 3;
    s.SeparatorTextBorderSize = 1;
    s.WindowMenuButtonPosition = ImGuiDir_None;
    s.WindowTitleAlign = ImVec2(0.0f, 0.5f);

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                      = rgb(200, 200, 200);
    c[ImGuiCol_TextDisabled]              = TEXT_DIM;
    c[ImGuiCol_WindowBg]                  = rgb(36, 36, 36);
    c[ImGuiCol_ChildBg]                   = rgb(0, 0, 0, 0.0f);
    c[ImGuiCol_PopupBg]                   = rgb(26, 26, 26);
    c[ImGuiCol_Border]                    = rgb(10, 10, 10);
    c[ImGuiCol_BorderShadow]              = rgb(0, 0, 0, 0.0f);
    c[ImGuiCol_FrameBg]                   = rgb(15, 15, 15);
    c[ImGuiCol_FrameBgHovered]            = rgb(24, 24, 24);
    c[ImGuiCol_FrameBgActive]             = rgb(24, 24, 24);
    c[ImGuiCol_TitleBg]                   = rgb(21, 21, 21);
    c[ImGuiCol_TitleBgActive]             = rgb(21, 21, 21);
    c[ImGuiCol_TitleBgCollapsed]          = rgb(21, 21, 21);
    c[ImGuiCol_MenuBarBg]                 = rgb(21, 21, 21);
    c[ImGuiCol_ScrollbarBg]               = rgb(0, 0, 0, 0.0f);
    c[ImGuiCol_ScrollbarGrab]             = rgb(70, 70, 70);
    c[ImGuiCol_ScrollbarGrabHovered]      = rgb(90, 90, 90);
    c[ImGuiCol_ScrollbarGrabActive]       = rgb(110, 110, 110);
    c[ImGuiCol_CheckMark]                 = rgb(230, 230, 230);
    c[ImGuiCol_SliderGrab]                = rgb(0, 112, 224, 0.85f);
    c[ImGuiCol_SliderGrabActive]          = rgb(40, 140, 250);
    c[ImGuiCol_Button]                    = rgb(56, 56, 56);
    c[ImGuiCol_ButtonHovered]             = rgb(87, 87, 87);
    c[ImGuiCol_ButtonActive]              = BLUE;
    c[ImGuiCol_Header]                    = rgb(47, 47, 47);
    c[ImGuiCol_HeaderHovered]             = rgb(58, 58, 58);
    c[ImGuiCol_HeaderActive]              = rgb(66, 66, 66);
    c[ImGuiCol_Separator]                 = rgb(12, 12, 12);
    c[ImGuiCol_SeparatorHovered]          = BLUE;
    c[ImGuiCol_SeparatorActive]           = BLUE;
    c[ImGuiCol_ResizeGrip]                = rgb(0, 0, 0, 0.0f);
    c[ImGuiCol_ResizeGripHovered]         = rgb(0, 112, 224, 0.6f);
    c[ImGuiCol_ResizeGripActive]          = BLUE;
    c[ImGuiCol_InputTextCursor]           = rgb(230, 230, 230);
    c[ImGuiCol_TabHovered]                = rgb(56, 56, 56);
    c[ImGuiCol_Tab]                       = rgb(21, 21, 21);
    c[ImGuiCol_TabSelected]               = rgb(36, 36, 36);
    c[ImGuiCol_TabSelectedOverline]       = BLUE;
    c[ImGuiCol_TabDimmed]                 = rgb(21, 21, 21);
    c[ImGuiCol_TabDimmedSelected]         = rgb(36, 36, 36);
    c[ImGuiCol_TabDimmedSelectedOverline] = rgb(0, 0, 0, 0.0f);
    c[ImGuiCol_DockingPreview]            = rgb(0, 112, 224, 0.5f);
    c[ImGuiCol_DockingEmptyBg]            = rgb(10, 10, 10);
    c[ImGuiCol_PlotHistogram]             = BLUE;
    c[ImGuiCol_TableHeaderBg]             = rgb(47, 47, 47);
    c[ImGuiCol_TableBorderStrong]         = rgb(15, 15, 15);
    c[ImGuiCol_TableBorderLight]          = rgb(28, 28, 28);
    c[ImGuiCol_TableRowBg]                = rgb(0, 0, 0, 0.0f);
    c[ImGuiCol_TableRowBgAlt]             = rgb(255, 255, 255, 0.02f);
    c[ImGuiCol_TextSelectedBg]            = rgb(0, 112, 224, 0.4f);
    c[ImGuiCol_NavCursor]                 = BLUE;
    c[ImGuiCol_ModalWindowDimBg]          = rgb(0, 0, 0, 0.55f);

    s.ScaleAllSizes(g_dpi);
    s.FontScaleDpi = g_dpi;
}

// ---------------------------------------------------------------- icons (all drawn, no font glyphs needed)
void drawIcon(ImDrawList* dl, int icon, ImVec2 c, float s, ImU32 col) {
    float t = std::max(1.0f, s * 0.14f);                   // stroke
    auto P = [&](float x, float y) { return ImVec2(c.x + x * s, c.y + y * s); };
    switch (icon) {
    case IC_PLAY:   dl->AddTriangleFilled(P(-0.6f, -0.8f), P(-0.6f, 0.8f), P(0.8f, 0.0f), col); break;
    case IC_PAUSE:  dl->AddRectFilled(P(-0.6f, -0.75f), P(-0.15f, 0.75f), col); dl->AddRectFilled(P(0.15f, -0.75f), P(0.6f, 0.75f), col); break;
    case IC_STOP:   dl->AddRectFilled(P(-0.6f, -0.6f), P(0.6f, 0.6f), col, 1.0f); break;
    case IC_START:  dl->AddRectFilled(P(-0.75f, -0.7f), P(-0.45f, 0.7f), col);
                    dl->AddTriangleFilled(P(0.75f, -0.7f), P(0.75f, 0.7f), P(-0.4f, 0.0f), col); break;
    case IC_END:    dl->AddRectFilled(P(0.45f, -0.7f), P(0.75f, 0.7f), col);
                    dl->AddTriangleFilled(P(-0.75f, -0.7f), P(-0.75f, 0.7f), P(0.4f, 0.0f), col); break;
    case IC_LOOP:   dl->PathArcTo(c, s * 0.62f, -2.6f, 2.2f, 18); dl->PathStroke(col, 0, t);
                    dl->AddTriangleFilled(P(-0.85f, -0.75f), P(-0.25f, -0.75f), P(-0.55f, -0.2f), col); break;
    case IC_SAVE:   dl->AddRect(P(-0.7f, -0.7f), P(0.7f, 0.7f), col, 1.5f, 0, t);
                    dl->AddRectFilled(P(-0.4f, -0.7f), P(0.35f, -0.25f), col);
                    dl->AddRectFilled(P(-0.4f, 0.15f), P(0.4f, 0.7f), col); break;
    case IC_OPEN:
    case IC_FOLDER: dl->AddRectFilled(P(-0.8f, -0.55f), P(-0.15f, -0.3f), col, 1.0f);
                    dl->AddRect(P(-0.8f, -0.35f), P(0.8f, 0.65f), col, 1.5f, 0, t); break;
    case IC_SHOT:   dl->AddRect(P(-0.85f, -0.45f), P(0.85f, 0.7f), col, 2.0f, 0, t);
                    dl->AddRectFilled(P(-0.35f, -0.7f), P(0.35f, -0.45f), col);
                    dl->AddCircle(P(0.0f, 0.12f), s * 0.33f, col, 16, t); break;
    case IC_RECORD: dl->AddCircleFilled(c, s * 0.62f, col, 20); break;
    case IC_CINEMA: dl->AddRect(P(-0.85f, -0.6f), P(0.85f, 0.6f), col, 1.0f, 0, t);
                    dl->AddRectFilled(P(-0.85f, -0.6f), P(0.85f, -0.3f), col);
                    dl->AddRectFilled(P(-0.85f, 0.3f), P(0.85f, 0.6f), col); break;
    case IC_SETTINGS:
        for (int i = 0; i < 8; i++) {
            float a = i * PI / 4;
            dl->AddLine(P(std::cos(a) * 0.45f, std::sin(a) * 0.45f), P(std::cos(a) * 0.8f, std::sin(a) * 0.8f), col, t * 1.6f);
        }
        dl->AddCircle(c, s * 0.48f, col, 16, t); dl->AddCircleFilled(c, s * 0.18f, col, 10); break;
    case IC_PLUS:   dl->AddLine(P(-0.6f, 0.0f), P(0.6f, 0.0f), col, t * 1.3f); dl->AddLine(P(0.0f, -0.6f), P(0.0f, 0.6f), col, t * 1.3f); break;
    case IC_EYE:
    case IC_EYE_OFF:
        dl->PathArcTo(P(0.0f, 0.55f), s * 0.95f, -2.45f, -0.69f, 12); dl->PathStroke(col, 0, t);
        dl->PathArcTo(P(0.0f, -0.55f), s * 0.95f, 0.69f, 2.45f, 12); dl->PathStroke(col, 0, t);
        if (icon == IC_EYE) dl->AddCircleFilled(c, s * 0.26f, col, 10);
        else dl->AddLine(P(-0.7f, 0.7f), P(0.7f, -0.7f), col, t);
        break;
    case IC_KEY:    dl->AddQuadFilled(P(0.0f, -0.7f), P(0.7f, 0.0f), P(0.0f, 0.7f), P(-0.7f, 0.0f), col); break;
    case IC_HOTSPOT: dl->AddCircleFilled(c, s * 0.42f, col, 16);
                     dl->AddCircle(c, s * 0.75f, (col & 0x00FFFFFF) | 0x70000000, 18, t); break;
    case IC_PLANET:  dl->AddCircleFilled(c, s * 0.62f, col, 18);
                     dl->AddEllipse(c, ImVec2(s * 0.95f, s * 0.28f), IM_COL32(230, 230, 230, 200), -0.3f, 18, t); break;
    case IC_SHIP:    dl->AddCircle(c, s * 0.7f, col, 18, t * 1.2f);
                     for (int i = 0; i < 6; i++) {
                         float a = i * PI / 3;
                         dl->AddRectFilled(P(std::cos(a) * 0.7f - 0.13f, std::sin(a) * 0.7f - 0.13f), P(std::cos(a) * 0.7f + 0.13f, std::sin(a) * 0.7f + 0.13f), col);
                     }
                     dl->AddCircleFilled(c, s * 0.16f, col, 8); break;
    case IC_JET:     dl->AddTriangleFilled(c, P(-0.3f, -0.9f), P(0.3f, -0.9f), col);
                     dl->AddTriangleFilled(c, P(0.3f, 0.9f), P(-0.3f, 0.9f), (col & 0x00FFFFFF) | 0x90000000);
                     dl->AddCircleFilled(c, s * 0.2f, IM_COL32(0, 0, 0, 255), 8); break;
    case IC_EHT:     dl->AddCircle(c, s * 0.65f, IM_COL32(255, 150, 40, 255), 18, t * 2.2f);
                     dl->AddCircle(P(0.25f, 0.2f), s * 0.25f, IM_COL32(255, 220, 120, 255), 10, t * 1.6f); break;
    case IC_SOUND:
    case IC_MUTE:   dl->AddRectFilled(P(-0.85f, -0.3f), P(-0.5f, 0.3f), col);
                    dl->AddTriangleFilled(P(-0.55f, -0.3f), P(0.0f, -0.75f), P(0.0f, 0.75f), col);
                    dl->AddTriangleFilled(P(-0.55f, -0.3f), P(0.0f, 0.75f), P(-0.55f, 0.3f), col);
                    if (icon == IC_SOUND) {
                        dl->PathArcTo(P(0.0f, 0.0f), s * 0.45f, -0.8f, 0.8f, 8); dl->PathStroke(col, 0, t);
                        dl->PathArcTo(P(0.0f, 0.0f), s * 0.8f, -0.8f, 0.8f, 10); dl->PathStroke(col, 0, t);
                    } else {
                        dl->AddLine(P(0.25f, -0.35f), P(0.85f, 0.35f), col, t * 1.3f);
                        dl->AddLine(P(0.25f, 0.35f), P(0.85f, -0.35f), col, t * 1.3f);
                    }
                    break;
    case IC_BLACKHOLE:
        dl->AddEllipse(c, ImVec2(s * 0.95f, s * 0.38f), col, 0.0f, 20, t);
        dl->AddCircleFilled(c, s * 0.45f, IM_COL32(0, 0, 0, 255), 16);
        dl->AddCircle(c, s * 0.48f, col, 16, t); break;
    case IC_DISK:   dl->AddEllipse(c, ImVec2(s * 0.9f, s * 0.45f), col, 0.0f, 20, t * 1.4f);
                    dl->AddEllipse(c, ImVec2(s * 0.45f, s * 0.2f), col, 0.0f, 16, t); break;
    case IC_SKY:
        for (int i = 0; i < 4; i++) {
            float a = i * PI / 2 - PI / 2, b = a + PI / 4;
            dl->AddTriangleFilled(c, P(std::cos(a) * 0.85f, std::sin(a) * 0.85f), P(std::cos(b) * 0.3f, std::sin(b) * 0.3f), col);
            dl->AddTriangleFilled(c, P(std::cos(a) * 0.85f, std::sin(a) * 0.85f), P(std::cos(a - PI / 4) * 0.3f, std::sin(a - PI / 4) * 0.3f), col);
        }
        break;
    case IC_CAMERA: dl->AddRectFilled(P(-0.85f, -0.45f), P(0.35f, 0.5f), col, 1.5f);
                    dl->AddTriangleFilled(P(0.35f, 0.0f), P(0.85f, -0.4f), P(0.85f, 0.4f), col); break;
    case IC_POST:   dl->AddCircle(c, s * 0.75f, col, 20, t);
                    for (int i = 0; i < 6; i++) {
                        float a = i * PI / 3;
                        dl->AddLine(P(std::cos(a) * 0.75f, std::sin(a) * 0.75f), P(std::cos(a + 2.1f) * 0.3f, std::sin(a + 2.1f) * 0.3f), col, t);
                    }
                    break;
    case IC_FILM:   dl->AddRect(P(-0.7f, -0.8f), P(0.7f, 0.8f), col, 1.0f, 0, t);
                    for (int i = 0; i < 4; i++) {
                        float y = -0.6f + i * 0.4f;
                        dl->AddRectFilled(P(-0.6f, y - 0.08f), P(-0.4f, y + 0.08f), col);
                        dl->AddRectFilled(P(0.4f, y - 0.08f), P(0.6f, y + 0.08f), col);
                    }
                    break;
    case IC_TEXT:   dl->AddLine(P(-0.7f, -0.65f), P(0.7f, -0.65f), col, t * 1.5f); dl->AddLine(P(0.0f, -0.65f), P(0.0f, 0.75f), col, t * 1.5f); break;
    case IC_HUD:    dl->AddRect(P(-0.8f, -0.65f), P(0.8f, 0.65f), col, 1.0f, 0, t);
                    dl->AddLine(P(-0.5f, -0.25f), P(0.4f, -0.25f), col, t); dl->AddLine(P(-0.5f, 0.05f), P(0.15f, 0.05f), col, t);
                    dl->AddLine(P(-0.5f, 0.35f), P(0.3f, 0.35f), col, t); break;
    }
}

// flat toolbar button: icon, optional label; background only on hover or when active
float toolButtonWidth(const char* label) {
    float h = ImGui::GetFrameHeight();
    return label ? h * 0.9f + ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 1.5f : h * 1.15f;
}
bool toolButton(const char* id, int icon, const char* label, const char* tip, bool active, ImU32 iconCol) {
    float h = ImGui::GetFrameHeight(), w = toolButtonWidth(label);
    ImGui::PushStyleColor(ImGuiCol_Button, active ? rgb(0, 112, 224, 0.35f) : rgb(0, 0, 0, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? rgb(0, 112, 224, 0.5f) : rgb(255, 255, 255, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgb(255, 255, 255, 0.14f));
    bool pressed = ImGui::Button(id, ImVec2(w, h));
    ImGui::PopStyleColor(3);
    if (tip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", tip);
    ImVec2 a = ImGui::GetItemRectMin();
    bool disabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
    ImU32 col = iconCol ? iconCol : ImGui::GetColorU32(ImGuiCol_Text);
    if (disabled) col = (col & 0x00FFFFFF) | 0x60000000;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float cx = label ? a.x + h * 0.55f : a.x + w * 0.5f;
    drawIcon(dl, icon, ImVec2(cx, a.y + h * 0.5f), h * 0.3f, col);
    if (label) dl->AddText(ImVec2(a.x + h * 0.95f, a.y + (h - ImGui::GetTextLineHeight()) * 0.5f),
                           ImGui::GetColorU32(disabled ? ImGuiCol_TextDisabled : ImGuiCol_Text), label);
    return pressed;
}
void toolSeparator() {
    ImGui::SameLine();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetFrameHeight();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x + 3, p.y + h * 0.2f), ImVec2(p.x + 3, p.y + h * 0.8f), IM_COL32(70, 70, 70, 255), 1.0f);
    ImGui::Dummy(ImVec2(7, h));
    ImGui::SameLine();
}

bool primaryButton(const char* label, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button, BLUE);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(30, 135, 245));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgb(0, 90, 190));
    ImGui::PushStyleColor(ImGuiCol_Text, rgb(255, 255, 255));
    bool r = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return r;
}

// ---------------------------------------------------------------- Details-style categories and property grid
bool category(const char* name, bool defaultOpen) {
    ImGui::PushFont(fontBold, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6 * g_dpi, 5 * g_dpi));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    bool open = ImGui::CollapsingHeader(name, defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    ImGui::PopStyleVar(2);
    ImGui::PopFont();
    return open;
}

bool beginProps(const char* id) {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8 * g_dpi, 3 * g_dpi));
    bool ok = ImGui::BeginTable(id, 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp);
    if (!ok) { ImGui::PopStyleVar(); return false; }
    ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch, 0.42f);
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.58f);
    return true;
}
void endProps() {
    ImGui::EndTable();
    ImGui::PopStyleVar();
    ImGui::Spacing();
}

void propLabel(const char* label, const char* tip) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::Indent(6 * g_dpi);
    ImGui::TextUnformatted(label);
    ImGui::Unindent(6 * g_dpi);
    if (tip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", tip);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

bool propFloat(const char* label, float* v, float lo, float hi, const char* format, ImGuiSliderFlags fl, const char* tip) {
    propLabel(label, tip);
    ImGui::PushID(label);
    bool r = ImGui::SliderFloat("##v", v, lo, hi, format, fl);
    ImGui::PopID();
    return r;
}
bool propDrag(const char* label, float* v, float speed, float lo, float hi, const char* format, const char* tip) {
    propLabel(label, tip);
    ImGui::PushID(label);
    bool r = ImGui::DragFloat("##v", v, speed, lo, hi, format);
    ImGui::PopID();
    return r;
}
bool propAngle(const char* label, float* rad, float loDeg, float hiDeg, const char* tip) {
    propLabel(label, tip);
    ImGui::PushID(label);
    bool r = ImGui::SliderAngle("##v", rad, loDeg, hiDeg, "%.1f deg");
    ImGui::PopID();
    return r;
}
bool propFov(const char* label, float* tanHalf) {
    propLabel(label, "Vertical field of view");
    ImGui::PushID(label);
    float deg = 2.0f * std::atan(*tanHalf) * 180.0f / PI;
    bool r = ImGui::SliderFloat("##v", &deg, 10.0f, 120.0f, "%.0f deg");
    if (r) *tanHalf = std::tan(deg * PI / 360.0f);
    ImGui::PopID();
    return r;
}
bool propCheck(const char* label, bool* v, const char* tip) {
    propLabel(label, tip);
    ImGui::PushID(label);
    bool r = ImGui::Checkbox("##v", v);
    ImGui::PopID();
    return r;
}
bool propCombo(const char* label, int* v, const char* items, const char* tip) {
    propLabel(label, tip);
    ImGui::PushID(label);
    bool r = ImGui::Combo("##v", v, items);
    ImGui::PopID();
    return r;
}
void propValue(const char* label, const char* text, const char* tip) {
    propLabel(label, tip);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", text);
}

}  // namespace ed
