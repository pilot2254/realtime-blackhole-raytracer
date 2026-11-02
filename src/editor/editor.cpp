// Editor frame: menu bar, main toolbar, status bar, dock layout and the viewport.
#include "editor/editor.h"
#include <algorithm>
#include <cmath>
#include <commdlg.h>
#include <cstdio>
#include <cstring>
#include "app/app.h"
#include "core/util.h"
#include "editor/editor_internal.h"
#include "film/sequence.h"
#include "render/export.h"
#include "sim/params.h"

const char* QUALITY_NAMES[4] = {"Low", "Medium", "High", "Ultra"};
int g_quality = Q_LOW;
int g_viewW = 0, g_viewH = 0;

void applyQuality(int q) {
    static const float scale[] = {0.35f, 0.6f, 1.0f, 1.0f};
    g_quality = q; P.renderScale = scale[q]; P.ultra = q == Q_ULTRA;
}

static double g_cinemaSince = -100.0;
static bool   g_resetLayout = false, g_showHelp = false, g_showAbout = false;
static bool   g_winOutliner = true, g_winDetails = true, g_winSequencer = true, g_winQueue = true;

void enterCinema() { g_showUI = false; g_cinemaSince = nowSeconds(); }

void editorInit() {
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = "gargantua_ui.ini";
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    char dir[MAX_PATH]; GetWindowsDirectoryA(dir, MAX_PATH);
    std::string fonts = std::string(dir) + "\\Fonts\\";
    if (GetFileAttributesA((fonts + "segoeui.ttf").c_str()) != INVALID_FILE_ATTRIBUTES)
        io.Fonts->AddFontFromFileTTF((fonts + "segoeui.ttf").c_str(), 15.0f);
    if (GetFileAttributesA((fonts + "seguisb.ttf").c_str()) != INVALID_FILE_ATTRIBUTES)
        ed::fontBold = io.Fonts->AddFontFromFileTTF((fonts + "seguisb.ttf").c_str(), 15.0f);
    if (!ed::fontBold) ed::fontBold = io.Fonts->Fonts.empty() ? io.Fonts->AddFontDefault() : io.Fonts->Fonts[0];
    ed::applyTheme();
}

// ---------------------------------------------------------------- files
static bool fileDialog(bool save, char* path, int size, const char* filter, const char* ext) {
    char buf[MAX_PATH]; std::snprintf(buf, sizeof buf, "%s", path);
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof ofn; ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = filter; ofn.lpstrFile = buf; ofn.nMaxFile = sizeof buf; ofn.lpstrDefExt = ext;
    ofn.Flags = OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (!(save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn))) return false;
    std::snprintf(path, size, "%s", buf);
    return true;
}
static const char* SEQ_FILTER = "Sequence (*.txt)\0*.txt\0All files\0*.*\0";
static void openSequence() {
    if (fileDialog(false, g_seqPath, sizeof g_seqPath, SEQ_FILTER, "txt"))
        g_status = loadSequence(g_seqPath) ? "Opened " + std::string(g_seqPath) : "Could not open " + std::string(g_seqPath);
}
static void saveSequenceAs(bool ask) {
    if (ask && !fileDialog(true, g_seqPath, sizeof g_seqPath, SEQ_FILTER, "txt")) return;
    g_status = saveSequence(g_seqPath) ? "Saved " + std::string(g_seqPath) : "Could not save " + std::string(g_seqPath);
}

static void togglePlay() { g_camMode = CAM_SEQUENCE; g_playing = !g_playing; }
static void stopFilm()   { g_camMode = CAM_SEQUENCE; g_playing = false; g_playhead = 0; }

// ---------------------------------------------------------------- menu bar
static void drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    bool busy = g_job.active;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open Sequence...", "Ctrl+O", false, !busy)) openSequence();
        if (ImGui::MenuItem("Save Sequence", "Ctrl+S", false, !busy)) saveSequenceAs(false);
        if (ImGui::MenuItem("Save Sequence As...", nullptr, false, !busy)) saveSequenceAs(true);
        ImGui::Separator();
        if (ImGui::MenuItem("Load Default Film", nullptr, false, !busy)) defaultSequence();
        ImGui::Separator();
        if (ImGui::MenuItem("Open Renders Folder")) openRendersFolder();
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) g_running = false;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")) {
        ImGui::MenuItem("Outliner", nullptr, &g_winOutliner);
        ImGui::MenuItem("Details", nullptr, &g_winDetails);
        ImGui::MenuItem("Sequencer", nullptr, &g_winSequencer);
        ImGui::MenuItem("Movie Render Queue", nullptr, &g_winQueue);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) { g_resetLayout = true; g_winOutliner = g_winDetails = g_winSequencer = g_winQueue = true; }
        ImGui::Separator();
        if (ImGui::MenuItem("Cinema Mode", "Tab")) enterCinema();
        if (ImGui::MenuItem("Fullscreen", "F11", g_fullscreen)) toggleFullscreen();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Controls")) g_showHelp = true;
        if (ImGui::MenuItem("About")) g_showAbout = true;
        ImGui::EndMenu();
    }
    const char* title = "Gargantua Studio";
    ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(title).x - 16 * g_dpi);
    ImGui::TextDisabled("%s", title);
    ImGui::EndMainMenuBar();
}

// ---------------------------------------------------------------- main toolbar
static void drawToolbar() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6 * g_dpi, 6 * g_dpi));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8 * g_dpi, 5 * g_dpi));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4 * g_dpi, 4 * g_dpi));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ed::rgb(28, 28, 28));
    float h = ImGui::GetFrameHeight() + 10 * g_dpi;
    ImGuiWindowFlags fl = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::BeginViewportSideBar("##toolbar", vp, ImGuiDir_Up, h, fl)) {
        bool busy = g_job.active;
        ImGui::BeginDisabled(busy);
        if (ed::toolButton("##save", ed::IC_SAVE, nullptr, "Save sequence (Ctrl+S)")) saveSequenceAs(false);
        ImGui::SameLine();
        if (ed::toolButton("##open", ed::IC_OPEN, nullptr, "Open sequence (Ctrl+O)")) openSequence();
        ed::toolSeparator();

        // camera source, like a segmented control
        bool film = g_camMode == CAM_SEQUENCE;
        if (ed::toolButton("##film", ed::IC_FILM, "Film", "Watch through the film's camera", film)) g_camMode = CAM_SEQUENCE;
        ImGui::SameLine(0, 1);
        if (ed::toolButton("##free", ed::IC_CAMERA, "Free Camera", "Fly the camera yourself", !film)) grabCamera();
        ed::toolSeparator();

        // transport
        bool playing = film && g_playing;
        if (ed::toolButton("##start", ed::IC_START, nullptr, "Go to start")) { g_camMode = CAM_SEQUENCE; g_playhead = 0; }
        ImGui::SameLine(0, 1);
        if (ed::toolButton("##play", playing ? ed::IC_PAUSE : ed::IC_PLAY, nullptr, playing ? "Pause (Space)" : "Play (Space)", false,
                           playing ? 0 : ImGui::GetColorU32(ed::GREEN))) togglePlay();
        ImGui::SameLine(0, 1);
        if (ed::toolButton("##stop", ed::IC_STOP, nullptr, "Stop and rewind")) stopFilm();
        ImGui::SameLine(0, 1);
        if (ed::toolButton("##end", ed::IC_END, nullptr, "Go to end")) { g_camMode = CAM_SEQUENCE; g_playhead = seqLength(); g_playing = false; }
        ImGui::SameLine(0, 1);
        if (ed::toolButton("##loop", ed::IC_LOOP, nullptr, "Loop playback", g_loop)) g_loop = !g_loop;
        ImGui::SameLine(0, 8 * g_dpi);
        ImGui::SetNextItemWidth(70 * g_dpi);
        static const float speeds[] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f};
        char cur[16]; std::snprintf(cur, sizeof cur, "%gx", g_playSpeed);
        if (ImGui::BeginCombo("##speed", cur)) {
            for (float s : speeds) { char b[16]; std::snprintf(b, sizeof b, "%gx", s); if (ImGui::Selectable(b, s == g_playSpeed)) g_playSpeed = s; }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Playback speed");
        ImGui::EndDisabled();

        // right side: preview quality, screenshot, render, cinema
        float right = ImGui::CalcTextSize("Scalability: Ultra").x + ImGui::GetFrameHeight() * 1.6f + ed::toolButtonWidth("Screenshot") +
                      ed::toolButtonWidth(busy ? "Cancel Render" : "Render") + ed::toolButtonWidth("Cinema") + 40 * g_dpi;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 10, ImGui::GetWindowWidth() - right));
        ImGui::BeginDisabled(busy);
        char q[48]; std::snprintf(q, sizeof q, "Scalability: %s", QUALITY_NAMES[g_quality]);
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("Scalability: Ultra").x + ImGui::GetFrameHeight() * 1.6f);
        if (ImGui::BeginCombo("##quality", q)) {
            for (int i = 0; i < 4; i++) if (ImGui::Selectable(QUALITY_NAMES[i], i == g_quality)) applyQuality(i);
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Real-time preview quality. Renders and screenshots always use full quality.");
        ImGui::SameLine();
        if (ed::toolButton("##shot", ed::IC_SHOT, "Screenshot", "Screenshot at render resolution (F12)")) beginExport(true);
        ImGui::EndDisabled();
        ImGui::SameLine(0, 1);
        if (!busy) { if (ed::toolButton("##render", ed::IC_RECORD, "Render", "Render the film to video", false, ImGui::GetColorU32(ed::RED))) beginExport(false); }
        else if (ed::toolButton("##cancel", ed::IC_STOP, "Cancel Render", "Stop rendering", true, ImGui::GetColorU32(ed::RED))) endExport(true);
        ImGui::SameLine(0, 1);
        if (ed::toolButton("##cinema", ed::IC_CINEMA, "Cinema", "Full-window picture (Tab)")) enterCinema();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

// ---------------------------------------------------------------- status bar
static void drawStatusBar(float fps, int renderW, int renderH) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10 * g_dpi, 3 * g_dpi));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ed::rgb(21, 21, 21));
    float h = ImGui::GetFrameHeight() + 2 * g_dpi;
    if (ImGui::BeginViewportSideBar("##status", vp, ImGuiDir_Down, h, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::AlignTextToFramePadding();
        if (g_job.active) {
            float prog = exportProgress();
            ImGui::TextColored(ed::rgb(240, 120, 100), g_job.screenshot ? "Rendering screenshot" : "Rendering video");
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ed::BLUE);
            char ov[32]; std::snprintf(ov, sizeof ov, "%.0f%%", prog * 100);
            ImGui::ProgressBar(prog, ImVec2(220 * g_dpi, ImGui::GetTextLineHeight()), ov);
            ImGui::PopStyleColor();
        } else {
            ImGui::TextDisabled("%s", g_status.empty() ? "Ready" : g_status.c_str());
        }
        char right[160];
        if (g_job.active) std::snprintf(right, sizeof right, "%d x %d   |   %.0f fps", g_job.W, g_job.H, fps);
        else std::snprintf(right, sizeof right, "%s   |   Preview %d x %d (%s)   |   %.0f fps",
                           P.spin > 0 ? fmt("Kerr  a = %.2f", P.spin).c_str() : "Schwarzschild", renderW, renderH, QUALITY_NAMES[g_quality], fps);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(right).x - 12 * g_dpi);
        if (fps < 20 && !g_job.active) ImGui::TextColored(ed::rgb(240, 120, 100), "%s", right);
        else ImGui::TextDisabled("%s", right);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ---------------------------------------------------------------- dock layout
static void drawDockspace() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGuiID id = ImGui::GetID("GargantuaDock_v5");
    if (g_resetLayout || !ImGui::DockBuilderGetNode(id)) {
        g_resetLayout = false;
        ImGui::DockBuilderRemoveNode(id);
        ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(id, vp->WorkSize);
        ImGuiID center = id;
        ImGuiID right  = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.23f, nullptr, &center);
        ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.27f, nullptr, &center);
        ImGuiID details = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.70f, nullptr, &right);
        ImGui::DockBuilderDockWindow("Viewport", center);
        ImGui::DockBuilderDockWindow("Movie Render Queue", bottom);
        ImGui::DockBuilderDockWindow("Sequencer", bottom);
        ImGui::DockBuilderDockWindow("Outliner", right);
        ImGui::DockBuilderDockWindow("Details", details);
        ImGui::DockBuilderFinish(id);
    }
    ImGui::DockSpaceOverViewport(id, vp);
}

// ---------------------------------------------------------------- viewport
namespace ed {
void panelViewport(GLuint tex, int renderW, int renderH) {
    (void)renderW; (void)renderH;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    g_viewW = g_viewH = 0;
    if (!open) { ImGui::End(); return; }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    g_viewW = (int)avail.x; g_viewH = (int)avail.y;
    if (avail.x < 4 || avail.y < 4) { ImGui::End(); return; }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, ImVec2(p0.x + avail.x, p0.y + avail.y), IM_COL32(0, 0, 0, 255));

    if (g_job.active) {
        if (g_job.haveFrame) {                          // latest finished frame, fitted
            float a = (float)g_job.W / g_job.H, vw = avail.x, vh = vw / a;
            if (vh > avail.y) { vh = avail.y; vw = vh * a; }
            float vx = p0.x + (avail.x - vw) * 0.5f, vy = p0.y + (avail.y - vh) * 0.5f;
            dl->AddImage((ImTextureID)(intptr_t)g_job.out8.tex, ImVec2(vx, vy), ImVec2(vx + vw, vy + vh), ImVec2(0, 1), ImVec2(1, 0));
        }
    } else if (tex) {
        dl->AddImage((ImTextureID)(intptr_t)tex, p0, ImVec2(p0.x + avail.x, p0.y + avail.y), ImVec2(0, 1), ImVec2(1, 0));
    }

    // camera controls on the whole image; the overlay buttons below take priority
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##view", avail, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    ImGuiIO& io = ImGui::GetIO();
    g_dragging = false;
    if (!g_job.active) {
        if (ImGui::IsItemActivated()) grabCamera();
        if (ImGui::IsItemActive()) {
            g_dragging = true;
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                g_orbit.phi += io.MouseDelta.x * 0.005f;
                g_orbit.el   = std::max(-1.53f, std::min(1.53f, g_orbit.el + io.MouseDelta.y * 0.004f));
            } else if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                g_orbit.yaw  += io.MouseDelta.x * 0.003f;
                g_orbit.pitch = std::max(-1.5f, std::min(1.5f, g_orbit.pitch - io.MouseDelta.y * 0.003f));
            }
        }
        if (ImGui::IsItemHovered() && io.MouseWheel != 0.0f) {
            grabCamera();
            g_orbit.dist = std::max(MIN_DIST, std::min(300.0f, g_orbit.dist * std::pow(0.9f, io.MouseWheel)));
        }
    }

    // viewport toolbar, top left, like Unreal's "Perspective / Lit / Show"
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 8 * g_dpi, p0.y + 8 * g_dpi));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8 * g_dpi, 3 * g_dpi));
    ImGui::PushStyleColor(ImGuiCol_Button, rgb(20, 20, 20, 0.72f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(60, 60, 60, 0.9f));
    ImGui::BeginDisabled(g_job.active);
    if (ImGui::Button(g_camMode == CAM_SEQUENCE ? "Film Camera" : "Free Camera")) ImGui::OpenPopup("vp_cam");
    ImGui::SameLine(0, 4);
    if (ImGui::Button(P.spin > 0 ? "Kerr" : "Schwarzschild")) ImGui::OpenPopup("vp_bh");
    ImGui::SameLine(0, 4);
    char q[32]; std::snprintf(q, sizeof q, "%s", QUALITY_NAMES[g_quality]);
    if (ImGui::Button(q)) ImGui::OpenPopup("vp_q");
    ImGui::SameLine(0, 4);
    if (ImGui::Button(g_letterboxOn ? "2.39 : 1" : "16 : 9")) g_letterboxOn = !g_letterboxOn;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Letterbox on / off");
    ImGui::EndDisabled();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    if (ImGui::BeginPopup("vp_cam")) {
        if (ImGui::MenuItem("Film Camera", nullptr, g_camMode == CAM_SEQUENCE)) g_camMode = CAM_SEQUENCE;
        if (ImGui::MenuItem("Free Camera", nullptr, g_camMode == CAM_ORBIT)) grabCamera();
        ImGui::Separator();
        ImGui::TextDisabled("Left-drag orbit, right-drag look, wheel zoom");
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("vp_bh")) {
        if (ImGui::MenuItem("Spinning (Kerr, a = 0.6)", nullptr, P.spin > 0)) P.spin = 0.6f;
        if (ImGui::MenuItem("Still (Schwarzschild)", nullptr, P.spin == 0)) P.spin = 0.0f;
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("vp_q")) {
        for (int i = 0; i < 4; i++) if (ImGui::MenuItem(QUALITY_NAMES[i], nullptr, i == g_quality)) applyQuality(i);
        ImGui::EndPopup();
    }
    ImGui::End();
}
}  // namespace ed

// ---------------------------------------------------------------- help
static void helpWindows() {
    if (g_showHelp) { ImGui::OpenPopup("Controls"); g_showHelp = false; }
    if (g_showAbout) { ImGui::OpenPopup("About Gargantua Studio"); g_showAbout = false; }
    ImGui::SetNextWindowSize(ImVec2(460 * g_dpi, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Controls", nullptr, ImGuiWindowFlags_NoResize)) {
        static const char* rows[][2] = {
            {"Left-drag viewport", "orbit the black hole"}, {"Right-drag viewport", "look around"},
            {"Mouse wheel", "move closer / farther"},       {"Space", "play / pause"},
            {"K", "add a camera key (Sequencer)"},          {"Delete", "delete the selected key or cue"},
            {"Tab", "cinema mode on / off"},                {"Esc", "leave cinema mode"},
            {"F11 / Alt+Enter", "fullscreen"},               {"F12", "screenshot"},
            {"Ctrl+O / Ctrl+S", "open / save sequence"},
        };
        if (ed::beginProps("ctl")) {
            for (auto& r : rows) { ed::propLabel(r[0]); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(r[1]); }
            ed::endProps();
        }
        ImGui::TextDisabled("Drag panel tabs to rearrange. Window > Reset Layout restores them.");
        if (ImGui::Button("Close", ImVec2(-1, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::SetNextWindowSize(ImVec2(460 * g_dpi, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("About Gargantua Studio", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::PushFont(ed::fontBold, 0.0f); ImGui::TextUnformatted("Gargantua Studio"); ImGui::PopFont();
        ImGui::TextWrapped("A real-time black hole ray tracer. Every pixel follows a ray of light through curved spacetime: "
                           "Schwarzschild geodesics for a still black hole, Kerr geodesics for a spinning one.");
        ImGui::TextDisabled("C++, OpenGL 3.3, Dear ImGui %s", IMGUI_VERSION);
        if (ImGui::Button("Close", ImVec2(-1, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------- frame
void drawEditor(float fps, GLuint viewTex, int renderW, int renderH) {
    if (!g_showUI) {
        double age = nowSeconds() - g_cinemaSince;
        if (age < 4.0) {
            const char* hint = "Press Tab or Esc to return to the editor";
            ImVec2 sz = ImGui::CalcTextSize(hint);
            ImU32 col = IM_COL32(220, 220, 220, (int)(200 * std::min(1.0, 4.0 - age)));
            ImGui::GetForegroundDrawList()->AddText(ImVec2((g_w - sz.x) * 0.5f, g_h * 0.08f), col, hint);
        }
        return;
    }
    if (!ImGui::GetIO().WantTextInput && !g_job.active) {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O)) openSequence();
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) saveSequenceAs(false);
    }
    drawMenuBar();
    drawToolbar();
    drawStatusBar(fps, renderW, renderH);
    drawDockspace();

    // a docked window shown for the first time becomes its tab group's front tab, so the
    // Movie Render Queue goes before the Sequencer
    ed::panelViewport(viewTex, renderW, renderH);
    if (g_winQueue) ed::panelRenderQueue();
    ImGui::BeginDisabled(g_job.active);
    if (g_winOutliner) ed::panelOutliner();
    if (g_winDetails) ed::panelDetails();
    if (g_winSequencer) ed::panelSequencer();
    ImGui::EndDisabled();
    helpWindows();
}
