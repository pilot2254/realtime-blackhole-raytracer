// Outliner (scene objects), Details (properties of the selection) and Movie Render Queue.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "app/app.h"
#include "core/util.h"
#include "editor/editor.h"
#include "editor/editor_internal.h"
#include "film/sequence.h"
#include "render/export.h"
#include "sim/params.h"
#include "sim/physics.h"

namespace ed {

int selKind = SEL_OBJECT, selObject = OBJ_BLACKHOLE, selKey = -1, selCue = -1;

struct ObjectInfo { const char* name; const char* type; int icon; ImU32 color; };
static const ObjectInfo OBJECTS[OBJ_COUNT] = {
    {"Gargantua",      "Black Hole",     IC_BLACKHOLE, IM_COL32(255, 170, 90, 255)},
    {"Accretion Disk", "Disk",           IC_DISK,      IM_COL32(255, 200, 120, 255)},
    {"Sky",            "Environment",    IC_SKY,       IM_COL32(170, 190, 255, 255)},
    {"Camera",         "Camera",         IC_CAMERA,    IM_COL32(200, 200, 200, 255)},
    {"Post Process",   "Volume",         IC_POST,      IM_COL32(150, 220, 200, 255)},
    {"Film",           "Level Sequence", IC_FILM,      IM_COL32(220, 150, 220, 255)},
};
static const char* CUE_NAMES[CUE_STYLES] = {"Title", "Subtitle", "Caption", "Chapter", "HUD"};

// ---------------------------------------------------------------- Outliner
void panelOutliner() {
    if (!ImGui::Begin("Outliner")) { ImGui::End(); return; }
    ImGuiTableFlags tf = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("outliner", 3, tf)) {
        float h = ImGui::GetFrameHeight();
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("##vis", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, h);
        ImGui::TableSetupColumn("Item Label", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.4f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < OBJ_COUNT; i++) {
            const ObjectInfo& o = OBJECTS[i];
            ImGui::PushID(i);
            ImGui::TableNextRow(0, h);
            // visibility toggle where it means something
            ImGui::TableSetColumnIndex(0);
            bool* vis = i == OBJ_DISK ? &P.disk : (i == OBJ_FILM ? &g_cuesOn : nullptr);
            ImVec2 p = ImGui::GetCursorScreenPos();
            if (vis) {
                if (ImGui::InvisibleButton("##eye", ImVec2(h, h))) *vis = !*vis;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(i == OBJ_DISK ? "Show the accretion disk" : "Show titles, captions and HUD");
                drawIcon(ImGui::GetWindowDrawList(), *vis ? IC_EYE : IC_EYE_OFF, ImVec2(p.x + h * 0.5f, p.y + h * 0.5f), h * 0.3f,
                         *vis ? IM_COL32(190, 190, 190, 255) : IM_COL32(90, 90, 90, 255));
            }
            ImGui::TableSetColumnIndex(1);
            bool sel = selKind == SEL_OBJECT && selObject == i;
            p = ImGui::GetCursorScreenPos();
            ImGui::PushStyleColor(ImGuiCol_Header, rgb(0, 112, 224, 0.55f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, sel ? rgb(0, 112, 224, 0.65f) : rgb(255, 255, 255, 0.06f));
            if (ImGui::Selectable("##row", sel, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0, h))) {
                selKind = SEL_OBJECT; selObject = i;
            }
            ImGui::PopStyleColor(2);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            drawIcon(dl, o.icon, ImVec2(p.x + h * 0.45f, p.y + h * 0.5f), h * 0.3f, o.color);
            dl->AddText(ImVec2(p.x + h * 1.0f, p.y + (h - ImGui::GetTextLineHeight()) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), o.name);
            ImGui::TableSetColumnIndex(2);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", o.type);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

// ---------------------------------------------------------------- Details: one function per object
static void detailsBlackHole() {
    if (category("Presets")) {
        float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Physically based", ImVec2(w, 0))) { P.doppler = 1; P.redshift = 1; P.tempPeak = 8000; P.diskBright = 1.25f; P.glow = 0.005f; }
        ImGui::SameLine();
        if (ImGui::Button("Interstellar look", ImVec2(w, 0))) { P.spin = 0.6f; P.doppler = 0; P.redshift = 0.3f; P.tempPeak = 5200; P.diskBright = 1.6f; P.glow = 0.008f; }
        ImGui::Spacing();
    }
    if (category("Spacetime") && beginProps("bh_st")) {
        propFloat("Gravity", &P.gravity, 0.0f, 2.0f, "%.2f x", 0, "Lensing strength. 0 = light travels in straight lines.");
        propFloat("Spin (a/M)", &P.spin, 0.0f, 0.998f, "%.3f", 0,
                  "0 = Schwarzschild (fast tracer). Above 0 the Kerr tracer runs:\nframe dragging, a lopsided shadow, the disk reaching closer in.");
        char b[64];
        if (P.spin > 0.0f) std::snprintf(b, sizeof b, "%.2f r_s  (inner %.2f r_s)", horizonRs(P.spin, 1.0f), horizonRs(P.spin, -1.0f));
        else std::snprintf(b, sizeof b, "1.00 r_s");
        propValue("Event horizon", b);
        std::snprintf(b, sizeof b, "%.2f r_s", iscoRs(P.spin));
        propValue("Innermost stable orbit", b, "ISCO: no circular orbit is stable closer than this.");
        endProps();
    }
    if (category("Light") && beginProps("bh_light")) {
        propFloat("Doppler beaming", &P.doppler, 0.0f, 1.0f, "%.2f", 0, "Gas moving toward us looks brighter and bluer.");
        propFloat("Gravitational redshift", &P.redshift, 0.0f, 1.0f);
        propFloat("Observer shift", &P.obsShift, 0.0f, 1.0f, "%.2f", 0,
                  "Shift from the camera's own motion and depth.\nFalling in, light ahead turns blue and bright, light behind turns red.");
        endProps();
    }
}

static void detailsDisk() {
    if (category("Disk") && beginProps("disk")) {
        propCheck("Visible", &P.disk);
        propCheck("Inner edge at ISCO", &P.iscoEdge, "Keep the inner edge where physics puts it.");
        ImGui::BeginDisabled(P.iscoEdge);
        propDrag("Inner radius", &P.diskIn, 0.02f, 0.5f, 40.0f, "%.2f r_s");
        ImGui::EndDisabled();
        propDrag("Outer radius", &P.diskOut, 0.05f, 1.0f, 60.0f, "%.1f r_s");
        P.diskOut = std::max(P.diskOut, P.diskIn + 0.5f);
        endProps();
    }
    if (category("Appearance") && beginProps("disk_look")) {
        propFloat("Temperature", &P.tempPeak, 1500.0f, 30000.0f, "%.0f K", ImGuiSliderFlags_Logarithmic);
        propFloat("Brightness", &P.diskBright, 0.05f, 8.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        propFloat("Opacity", &P.diskOpacity, 0.0f, 5.0f);
        propFloat("Swirl speed", &P.swirl, 0.0f, 4.0f);
        propFloat("Photon glow", &P.glow, 0.0f, 0.04f, "%.3f", 0, "Faint glow of light orbiting close to the hole.");
        endProps();
    }
}

static void detailsSky() {
    if (category("Layers") && beginProps("sky")) {
        propFloat("Stars", &P.stars, 0.0f, 3.0f);
        propFloat("Milky Way", &P.milky, 0.0f, 3.0f);
        propFloat("Nebulae", &P.nebula, 0.0f, 3.0f);
        propFloat("Spiral galaxy", &P.galaxy, 0.0f, 3.0f, "%.2f", 0, "The galaxy that becomes an Einstein ring.");
        endProps();
    }
}

static void detailsCamera() {
    if (category("Source") && beginProps("cam_src")) {
        int mode = g_camMode;
        if (propCombo("Camera", &mode, "Film camera\0Free camera\0")) {
            if (mode == CAM_ORBIT) grabCamera(); else g_camMode = CAM_SEQUENCE;
        }
        propCheck("Pause time", &g_paused, "Freeze the disk's motion.");
        endProps();
    }
    if (g_camMode == CAM_SEQUENCE) {
        if (category("Current shot") && beginProps("cam_shot")) {
            CamState c = currentCam();
            char b[64];
            std::snprintf(b, sizeof b, "%.2f r_s", c.dist);                          propValue("Distance", b);
            std::snprintf(b, sizeof b, "%.1f deg", c.el * 180 / PI);                 propValue("Elevation", b);
            std::snprintf(b, sizeof b, "%.0f deg", 2 * std::atan(c.fov) * 180 / PI); propValue("Field of view", b);
            propValue("Motion", c.fall > 0.5f ? "free fall" : "hovering");
            endProps();
        }
        ImGui::TextDisabled("Drag the viewport to take over the camera.");
        return;
    }
    if (category("Transform") && beginProps("cam_tr")) {
        float wrapped = std::remainder(g_orbit.phi, 2 * PI);
        if (propAngle("Azimuth", &wrapped, -180, 180)) g_orbit.phi = wrapped;
        propAngle("Elevation", &g_orbit.el, -88, 88);
        propFloat("Distance", &g_orbit.dist, MIN_DIST, 300.0f, "%.2f r_s", ImGuiSliderFlags_Logarithmic);
        endProps();
    }
    if (category("Lens") && beginProps("cam_lens")) {
        propFov("Field of view", &g_orbit.fov);
        propAngle("Roll", &g_orbit.roll, -60, 60);
        propAngle("Look yaw", &g_orbit.yaw, -180, 180, "Turn away from the hole (right-drag in the viewport).");
        propAngle("Look pitch", &g_orbit.pitch, -89, 89);
        endProps();
    }
    if (category("Motion") && beginProps("cam_motion")) {
        propFloat("Free fall", &g_orbit.fall, 0.0f, 1.0f, "%.2f", 0,
                  "0 = hovering on rockets, 1 = falling freely from far away.\nNear and inside the horizon nothing can hover, so the camera falls anyway.");
        propCheck("Auto-rotate", &g_autoRotate);
        propFloat("Rotate speed", &g_rotSpeed, -0.6f, 0.6f, "%.2f rad/s");
        propCheck("Cinematic drift", &g_drift, "Slow breathing of elevation, distance and roll.");
        endProps();
    }
}

static void detailsPost() {
    if (category("Exposure") && beginProps("post_exp")) {
        propFloat("Exposure", &P.exposure, 0.1f, 8.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        propFloat("Auto exposure", &P.autoExposure, 0.0f, 1.0f, "%.2f", 0, "Stops the lens down when everything ahead is blueshifted and bright.");
        endProps();
    }
    if (category("Lens") && beginProps("post_lens")) {
        propFloat("Bloom", &P.bloom, 0.0f, 0.4f);
        propFloat("Vignette", &P.vignette, 0.0f, 1.0f);
        propFloat("Chromatic aberration", &P.ca, 0.0f, 0.05f, "%.3f");
        propFloat("Film grain", &P.grain, 0.0f, 10.0f);
        endProps();
    }
}

static void detailsFilm() {
    if (category("Frame") && beginProps("film_frame")) {
        propCheck("Letterbox", &g_letterboxOn);
        ImGui::BeginDisabled(!g_letterboxOn);
        propFloat("Aspect ratio", &g_letterbox, 1.78f, 2.76f, "%.2f : 1");
        ImGui::EndDisabled();
        propCheck("Titles, captions & HUD", &g_cuesOn);
        endProps();
    }
    if (category("Timing") && beginProps("film_time")) {
        propDrag("Fade in", &g_fadeIn, 0.05f, 0.0f, 10.0f, "%.1f s");
        propDrag("Fade out", &g_fadeOut, 0.05f, 0.0f, 10.0f, "%.2f s");
        propDrag("Cut to black at", &g_blackAt, 0.1f, 0.0f, 600.0f, g_blackAt > 0 ? "%.1f s" : "end of film");
        char b[32]; std::snprintf(b, sizeof b, "%s", mmss(seqLength()).c_str());
        propValue("Length", b);
        endProps();
    }
    if (category("HUD") && beginProps("film_hud")) {
        propFloat("Black hole mass", &g_massSun, 5.0f, 1.0e10f, "%.3g suns", ImGuiSliderFlags_Logarithmic,
                  "Only changes the real-world units in the HUD.\nSmall black holes have deadly tides at the horizon, big ones gentle.");
        endProps();
    }
}

static void detailsKey() {
    if (selKey < 0 || selKey >= (int)g_keys.size()) { selKind = SEL_OBJECT; return; }
    Key& k = g_keys[selKey];
    if (category("Key") && beginProps("key")) {
        propDrag("Time", &k.t, 0.05f, 0.0f, 600.0f, "%.2f s");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            float t = k.t; sortKeys();
            for (int i = 0; i < (int)g_keys.size(); i++) if (g_keys[i].t == t) selKey = i;
        }
        endProps();
        float w = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3.0f;
        if (ImGui::Button("Jump to key", ImVec2(w, 0))) { g_playhead = g_keys[selKey].t; g_camMode = CAM_SEQUENCE; g_playing = false; }
        ImGui::SameLine();
        if (ImGui::Button("Set from view", ImVec2(w, 0))) g_keys[selKey].c = currentCam();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy the current viewport camera into this key.");
        ImGui::SameLine();
        if (ImGui::Button("Delete", ImVec2(w, 0))) { g_keys.erase(g_keys.begin() + selKey); selKind = SEL_OBJECT; selObject = OBJ_FILM; return; }
        ImGui::Spacing();
    }
    Key& kk = g_keys[selKey];
    if (category("Transform") && beginProps("key_tr")) {
        propDrag("Azimuth", &kk.c.phi, 0.005f, -100.0f, 100.0f, "%.3f rad");
        propAngle("Elevation", &kk.c.el, -88, 88);
        propFloat("Distance", &kk.c.dist, MIN_DIST, 300.0f, "%.2f r_s", ImGuiSliderFlags_Logarithmic);
        endProps();
    }
    if (category("Lens") && beginProps("key_lens")) {
        propFov("Field of view", &kk.c.fov);
        propAngle("Roll", &kk.c.roll, -60, 60);
        propAngle("Look yaw", &kk.c.yaw, -180, 180);
        propAngle("Look pitch", &kk.c.pitch, -89, 89);
        propFloat("Free fall", &kk.c.fall, 0.0f, 1.0f);
        endProps();
    }
}

static void detailsCue() {
    if (selCue < 0 || selCue >= (int)g_cues.size()) { selKind = SEL_OBJECT; return; }
    Cue& c = g_cues[selCue];
    if (category("Timing") && beginProps("cue_t")) {
        propDrag("Start", &c.t0, 0.05f, 0.0f, 600.0f, "%.2f s");
        propDrag("End", &c.t1, 0.05f, 0.0f, 600.0f, "%.2f s");
        c.t1 = std::max(c.t1, c.t0 + 0.1f);
        propCombo("Style", &c.style, "Title\0Subtitle\0Caption\0Chapter\0HUD\0");
        endProps();
    }
    if (c.style != CUE_HUD && category("Text")) {
        char buf[1024]; std::snprintf(buf, sizeof buf, "%s", c.text.c_str());
        if (ImGui::InputTextMultiline("##text", buf, sizeof buf, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4.5f), ImGuiInputTextFlags_WordWrap))
            c.text = buf;
        ImGui::Spacing();
    }
    float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("Jump to start", ImVec2(w, 0))) { g_playhead = c.t0 + 0.01f; g_camMode = CAM_SEQUENCE; g_playing = false; }
    ImGui::SameLine();
    if (ImGui::Button("Delete", ImVec2(w, 0))) { g_cues.erase(g_cues.begin() + selCue); selKind = SEL_OBJECT; selObject = OBJ_FILM; }
}

void panelDetails() {
    if (!ImGui::Begin("Details")) { ImGui::End(); return; }
    // header: what is selected
    int icon; ImU32 color; char name[96]; const char* type;
    if (selKind == SEL_KEY && selKey >= 0 && selKey < (int)g_keys.size()) {
        icon = IC_KEY; color = g_keys[selKey].c.fall > 0.5f ? IM_COL32(110, 165, 255, 255) : IM_COL32(255, 160, 60, 255);
        std::snprintf(name, sizeof name, "Camera key %d", selKey + 1); type = "Sequencer key";
    } else if (selKind == SEL_CUE && selCue >= 0 && selCue < (int)g_cues.size()) {
        icon = g_cues[selCue].style == CUE_HUD ? IC_HUD : IC_TEXT; color = IM_COL32(200, 200, 200, 255);
        std::snprintf(name, sizeof name, "%s at %s", CUE_NAMES[g_cues[selCue].style], mmss(g_cues[selCue].t0).c_str()); type = "Sequencer cue";
    } else {
        selKind = SEL_OBJECT;
        const ObjectInfo& o = OBJECTS[selObject];
        icon = o.icon; color = o.color; std::snprintf(name, sizeof name, "%s", o.name); type = o.type;
    }
    float h = ImGui::GetFrameHeight();
    ImVec2 p = ImGui::GetCursorScreenPos();
    drawIcon(ImGui::GetWindowDrawList(), icon, ImVec2(p.x + h * 0.5f, p.y + h * 0.5f), h * 0.36f, color);
    ImGui::SetCursorScreenPos(ImVec2(p.x + h * 1.15f, p.y));
    ImGui::AlignTextToFramePadding();
    ImGui::PushFont(fontBold, 0.0f); ImGui::TextUnformatted(name); ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", type);
    ImGui::Separator();

    ImGui::BeginChild("props");
    if (selKind == SEL_KEY) detailsKey();
    else if (selKind == SEL_CUE) detailsCue();
    else switch (selObject) {
        case OBJ_BLACKHOLE: detailsBlackHole(); break;
        case OBJ_DISK:      detailsDisk(); break;
        case OBJ_SKY:       detailsSky(); break;
        case OBJ_CAMERA:    detailsCamera(); break;
        case OBJ_POST:      detailsPost(); break;
        case OBJ_FILM:      detailsFilm(); break;
    }
    ImGui::EndChild();
    ImGui::End();
}

// ---------------------------------------------------------------- Movie Render Queue
void panelRenderQueue() {
    if (!ImGui::Begin("Movie Render Queue")) { ImGui::End(); return; }
    bool busy = g_job.active;
    float colW = std::min(560.0f * g_dpi, ImGui::GetContentRegionAvail().x);

    ImGui::BeginChild("mrq_settings", ImVec2(colW, 0));
    ImGui::BeginDisabled(busy);
    if (beginProps("mrq")) {
        propLabel("Output folder");
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s\\", RENDER_DIR);
        ImGui::SameLine();
        if (ImGui::SmallButton("Open")) openRendersFolder();
        propLabel("File name", "Video file name inside the renders folder.");
        ImGui::InputText("##name", g_exName, sizeof g_exName);
        propCombo("Resolution", &g_exRes, "1280 x 720\0" "1920 x 1080\0" "2560 x 1440\0" "3840 x 2160 (4K)\0");
        propCombo("Frame rate", &g_exFps, "24 fps (film)\0" "30 fps\0" "60 fps\0");
        propLabel("Samples per frame", "Sub-frames averaged per frame: antialiasing + motion blur.\nRender time scales linearly.");
        ImGui::SliderInt("##samples", &g_exSamples, 1, 64);
        propFloat("Shutter", &g_exShutter, 0.0f, 1.0f, "%.2f", 0, "Fraction of the frame the shutter is open. 0.5 = 180 degrees.");
        propCheck("Ultra geodesics", &g_exUltra, "Smaller integration steps: sharper lensing, slower.");
        propLabel("ffmpeg", "Path to ffmpeg.exe. Without it, BMP frames are written instead.");
        ImGui::InputText("##ffmpeg", g_ffmpeg, sizeof g_ffmpeg);
        endProps();
    }
    ImGui::EndDisabled();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("mrq_job");
    int frames = (int)std::ceil(seqLength() * FPS_V[g_exFps]);
    ImGui::PushFont(fontBold, 0.0f); ImGui::TextUnformatted("GARGANTUA"); ImGui::PopFont();
    ImGui::TextDisabled("%s  |  %d frames  |  %d x %d", mmss(seqLength()).c_str(), frames, RES_W[g_exRes], RES_H[g_exRes]);
    ImGui::Spacing();
    float bw = std::min(260.0f * g_dpi, ImGui::GetContentRegionAvail().x);
    if (busy) {
        const ExportJob& j = g_job;
        float prog = exportProgress();
        double el = secondsSince(j.t0), eta = prog > 0.001f ? el / prog * (1.0 - prog) : 0.0;
        char ov[64]; std::snprintf(ov, sizeof ov, "%d / %d", std::min(j.frame + 1, j.total), j.total);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, BLUE);
        ImGui::ProgressBar(prog, ImVec2(bw, 0), ov);
        ImGui::PopStyleColor();
        ImGui::TextDisabled("elapsed %s   remaining ~%s", fmtDuration(el).c_str(), fmtDuration(eta).c_str());
        ImGui::TextDisabled("-> %s", j.useFfmpeg ? j.out.c_str() : "BMP frames (ffmpeg not found)");
        if (ImGui::Button("Cancel", ImVec2(bw, 0))) endExport(true);
    } else {
        if (primaryButton("Render Film", ImVec2(bw, ImGui::GetFrameHeight() * 1.3f))) beginExport(false);
        if (ImGui::Button("Screenshot of current view", ImVec2(bw, 0))) beginExport(true);
        ImGui::TextDisabled("Tip: try 720p with 4 samples first to check the timing.");
    }
    if (!g_status.empty()) { ImGui::Spacing(); ImGui::TextWrapped("%s", g_status.c_str()); }
    ImGui::EndChild();
    ImGui::End();
}

}  // namespace ed
