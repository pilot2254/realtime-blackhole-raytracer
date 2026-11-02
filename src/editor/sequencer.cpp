// Sequencer: track list on the left, ruler and tracks on the right. Click a key or a cue to edit it in
// Details, drag a key to retime it, click or drag anywhere else to scrub.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "app/app.h"
#include "core/util.h"
#include "editor/editor_internal.h"
#include "film/sequence.h"
#include "render/export.h"

namespace ed {

enum { TRACK_CAMERA, TRACK_TITLES, TRACK_CAPTIONS, TRACK_HUD, TRACK_COUNT };
static const char* TRACK_NAMES[TRACK_COUNT] = {"Camera", "Titles & chapters", "Captions", "HUD & effects"};
static const int   TRACK_ICONS[TRACK_COUNT] = {IC_CAMERA, IC_TEXT, IC_TEXT, IC_HUD};
static int cueTrack(int style) { return style == CUE_CAPTION ? TRACK_CAPTIONS : (style == CUE_HUD || style == CUE_EHT ? TRACK_HUD : TRACK_TITLES); }
static ImU32 cueColor(int style) {
    switch (style) {
    case CUE_TITLE: case CUE_SUBTITLE: return IM_COL32(176, 140, 70, 255);
    case CUE_CHAPTER:                  return IM_COL32(190, 110, 50, 255);
    case CUE_CAPTION:                  return IM_COL32(70, 105, 160, 255);
    case CUE_EHT:                      return IM_COL32(205, 120, 40, 255);
    default:                           return IM_COL32(60, 130, 85, 255);
    }
}

static void addCaption() {
    g_cues.push_back({g_playhead, g_playhead + 6.0f, CUE_CAPTION, "New caption"});
    selKind = SEL_CUE; selCue = (int)g_cues.size() - 1;
}
static void addKeyHere() {
    addKey(g_playhead, currentCam());
    for (int i = 0; i < (int)g_keys.size(); i++) if (std::fabs(g_keys[i].t - g_playhead) < 1e-3f) { selKind = SEL_KEY; selKey = i; }
}

void panelSequencer() {
    if (!ImGui::Begin("Sequencer")) { ImGui::End(); return; }
    bool busy = g_job.active;

    // ---- sequencer toolbar
    ImGui::BeginDisabled(busy);
    if (toolButton("##addkey", IC_KEY, "Add Key", "Key the current view at the playhead (K)", false, IM_COL32(255, 160, 60, 255))) addKeyHere();
    ImGui::SameLine(0, 2);
    if (toolButton("##addcap", IC_PLUS, "Add Caption", "Add a caption at the playhead")) addCaption();
    ImGui::EndDisabled();
    toolSeparator();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%d keys  |  %d cues", (int)g_keys.size(), (int)g_cues.size());
    ImGui::SameLine(ImGui::GetWindowWidth() - 150 * g_dpi);
    ImGui::AlignTextToFramePadding();
    ImGui::PushFont(fontBold, 0.0f);
    ImGui::Text("%s", mmss(g_playhead).c_str());
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled("/ %s", mmss(seqLength()).c_str());

    // ---- tracks
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float L = std::max(seqLength(), 1.0f);
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float headW = 170 * g_dpi, ruler = 22 * g_dpi, rowH = 26 * g_dpi;
    float x0 = p0.x + headW, w = std::max(60.0f, avail.x - headW), h = std::max(ruler + rowH * TRACK_COUNT, avail.y);
    auto X = [&](float t) { return x0 + std::max(0.0f, std::min(L, t)) / L * w; };
    auto T = [&](float x) { return std::max(0.0f, std::min(L, (x - x0) / w * L)); };
    auto rowY = [&](int r) { return p0.y + ruler + r * rowH; };

    // track headers
    dl->AddRectFilled(p0, ImVec2(x0, p0.y + h), IM_COL32(30, 30, 30, 255));
    for (int r = 0; r < TRACK_COUNT; r++) {
        float y = rowY(r);
        dl->AddRectFilled(ImVec2(p0.x, y), ImVec2(x0 - 1, y + rowH - 1), IM_COL32(42, 42, 42, 255));
        drawIcon(dl, TRACK_ICONS[r], ImVec2(p0.x + 14 * g_dpi, y + rowH * 0.5f), rowH * 0.24f, IM_COL32(160, 160, 160, 255));
        dl->AddText(ImVec2(p0.x + 28 * g_dpi, y + (rowH - ImGui::GetTextLineHeight()) * 0.5f), IM_COL32(200, 200, 200, 255), TRACK_NAMES[r]);
    }
    // ruler and track lanes
    dl->AddRectFilled(ImVec2(x0, p0.y), ImVec2(x0 + w, p0.y + ruler), IM_COL32(26, 26, 26, 255));
    dl->AddRectFilled(ImVec2(x0, p0.y + ruler), ImVec2(x0 + w, p0.y + h), IM_COL32(20, 20, 20, 255));
    for (int r = 0; r < TRACK_COUNT; r++)
        dl->AddLine(ImVec2(x0, rowY(r + 1) - 1), ImVec2(x0 + w, rowY(r + 1) - 1), IM_COL32(34, 34, 34, 255));
    if (g_blackAt > 0 && g_blackAt < L)
        dl->AddRectFilled(ImVec2(X(g_blackAt), p0.y + ruler), ImVec2(x0 + w, p0.y + h), IM_COL32(0, 0, 0, 120));
    float pps = w / L;
    float major = pps * 10 >= 64 ? 10.0f : (pps * 30 >= 64 ? 30.0f : 60.0f), minor = major / 5;
    for (int i = 0; i * minor <= L + 1e-3f; i++) {
        float t = i * minor, x = X(t);
        bool isMajor = i % 5 == 0;
        dl->AddLine(ImVec2(x, p0.y + (isMajor ? 4 : ruler * 0.62f)), ImVec2(x, p0.y + ruler), IM_COL32(95, 95, 95, 255));
        if (isMajor) {
            dl->AddLine(ImVec2(x, p0.y + ruler), ImVec2(x, p0.y + h), IM_COL32(32, 32, 32, 255));
            char b[16]; std::snprintf(b, sizeof b, "%d:%02d", (int)t / 60, (int)t % 60);
            dl->AddText(ImVec2(x + 4, p0.y + 3), IM_COL32(150, 150, 150, 255), b);
        }
    }

    // cues
    float pad = 4 * g_dpi;
    for (int i = 0; i < (int)g_cues.size(); i++) {
        const Cue& c = g_cues[i];
        float y = rowY(cueTrack(c.style));
        ImVec2 a(X(c.t0), y + pad), b(std::max(X(c.t1), X(c.t0) + 3), y + rowH - pad);
        if (c.style == CUE_SUBTITLE) a.y = b.y - 4 * g_dpi;            // thin strip under its title
        bool sel = selKind == SEL_CUE && selCue == i;
        dl->AddRectFilled(a, b, cueColor(c.style), 3.0f);
        if (sel) dl->AddRect(ImVec2(a.x - 1, a.y - 1), ImVec2(b.x + 1, b.y + 1), IM_COL32(255, 255, 255, 230), 3.0f, 0, 1.5f);
        if (c.style == CUE_EHT && b.x - a.x > 30)
            dl->AddText(ImVec2(a.x + 4, a.y + (b.y - a.y - ImGui::GetTextLineHeight()) * 0.5f), IM_COL32(255, 255, 255, 220), "EHT");
        if (c.style != CUE_HUD && c.style != CUE_EHT && c.style != CUE_SUBTITLE && b.x - a.x > 30) {
            dl->PushClipRect(a, ImVec2(b.x - 3, b.y), true);
            dl->AddText(ImVec2(a.x + 4, a.y + (b.y - a.y - ImGui::GetTextLineHeight()) * 0.5f), IM_COL32(255, 255, 255, 210), c.text.c_str());
            dl->PopClipRect();
        }
    }
    // camera keys
    float ky = rowY(TRACK_CAMERA) + rowH * 0.5f, ks = 6 * g_dpi;
    for (int i = 0; i + 1 < (int)g_keys.size(); i++)
        dl->AddLine(ImVec2(X(g_keys[i].t), ky), ImVec2(X(g_keys[i + 1].t), ky), IM_COL32(70, 70, 70, 255), 1.0f);
    for (int i = 0; i < (int)g_keys.size(); i++) {
        float x = X(g_keys[i].t);
        ImU32 col = g_keys[i].c.fall > 0.5f ? IM_COL32(110, 165, 255, 255) : IM_COL32(255, 160, 60, 255);
        bool sel = selKind == SEL_KEY && selKey == i;
        if (sel) dl->AddQuadFilled(ImVec2(x, ky - ks - 3), ImVec2(x + ks + 3, ky), ImVec2(x, ky + ks + 3), ImVec2(x - ks - 3, ky), IM_COL32(255, 255, 255, 255));
        dl->AddQuadFilled(ImVec2(x, ky - ks), ImVec2(x + ks, ky), ImVec2(x, ky + ks), ImVec2(x - ks, ky), col);
    }

    // ---- interaction (only on the ruler + tracks)
    ImGui::SetCursorScreenPos(ImVec2(x0, p0.y));
    ImGui::InvisibleButton("##tracks", ImVec2(w, h));
    static int dragKey = -1;
    static bool scrubbing = false;
    ImVec2 m = ImGui::GetIO().MousePos;
    if (!busy && ImGui::IsItemActivated()) {
        dragKey = -1; scrubbing = false;
        if (m.y < p0.y + ruler) scrubbing = true;
        else {
            int row = (int)((m.y - p0.y - ruler) / rowH), hit = -1;
            if (row == TRACK_CAMERA) {
                for (int i = 0; i < (int)g_keys.size(); i++) if (std::fabs(X(g_keys[i].t) - m.x) <= ks + 2) hit = i;
                if (hit >= 0) { selKind = SEL_KEY; selKey = hit; dragKey = hit; }
            } else {
                for (int i = 0; i < (int)g_cues.size(); i++) {
                    if (cueTrack(g_cues[i].style) != row || m.x < X(g_cues[i].t0) || m.x > std::max(X(g_cues[i].t1), X(g_cues[i].t0) + 3)) continue;
                    bool lowStrip = m.y > rowY(row) + rowH - pad - 4 * g_dpi;      // subtitles live in the bottom strip
                    if ((g_cues[i].style == CUE_SUBTITLE) == lowStrip || hit < 0) hit = i;
                }
                if (hit >= 0) { selKind = SEL_CUE; selCue = hit; }
            }
            if (hit < 0) scrubbing = true;
        }
    }
    if (!busy && ImGui::IsItemActive()) {
        if (scrubbing) { g_playhead = T(m.x); g_camMode = CAM_SEQUENCE; }
        else if (dragKey >= 0 && dragKey < (int)g_keys.size() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f))
            g_keys[dragKey].t = std::round(T(m.x) * 10.0f) / 10.0f;
    }
    if (ImGui::IsItemDeactivated() && dragKey >= 0 && dragKey < (int)g_keys.size()) {
        Key moved = g_keys[dragKey];
        sortKeys();
        for (int i = 0; i < (int)g_keys.size(); i++) if (g_keys[i].t == moved.t && g_keys[i].c.phi == moved.c.phi) selKey = i;
        dragKey = -1;
    }
    if (ImGui::IsItemHovered() && !ImGui::IsItemActive()) {
        dl->AddLine(ImVec2(m.x, p0.y + ruler), ImVec2(m.x, p0.y + h), IM_COL32(255, 255, 255, 40));
        if (m.y < p0.y + ruler) ImGui::SetTooltip("%s", mmss(T(m.x)).c_str());
    }

    // playhead on top
    float px = X(g_playhead);
    ImU32 red = ImGui::GetColorU32(RED);
    dl->AddLine(ImVec2(px, p0.y + ruler * 0.5f), ImVec2(px, p0.y + h), red, 1.5f);
    dl->AddRectFilled(ImVec2(px - 5 * g_dpi, p0.y + 2), ImVec2(px + 5 * g_dpi, p0.y + ruler * 0.55f), red, 2.0f);
    dl->AddTriangleFilled(ImVec2(px - 5 * g_dpi, p0.y + ruler * 0.55f), ImVec2(px + 5 * g_dpi, p0.y + ruler * 0.55f), ImVec2(px, p0.y + ruler * 0.85f), red);

    // shortcuts while the sequencer is focused
    if (!busy && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            if (selKind == SEL_KEY && selKey >= 0 && selKey < (int)g_keys.size()) { g_keys.erase(g_keys.begin() + selKey); selKind = SEL_OBJECT; }
            else if (selKind == SEL_CUE && selCue >= 0 && selCue < (int)g_cues.size()) { g_cues.erase(g_cues.begin() + selCue); selKind = SEL_OBJECT; }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_K)) addKeyHere();
    }
    ImGui::End();
}

}  // namespace ed
