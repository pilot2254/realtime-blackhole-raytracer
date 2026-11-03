// =====================================================================================
//   G A R G A N T U A   S T U D I O
//   Real-time black hole ray tracer (Schwarzschild and Kerr) with an Unreal-style editor,
//   a camera sequencer and offline cinematic video export (motion blur, antialiasing, 4K).
//   Win32 + OpenGL 3.3 + Dear ImGui. Windows only.
//
//   src/core     reusable: maths, OpenGL loader and helpers, utilities
//   src/sim      camera, scene parameters, CPU-side relativity
//   src/render   shaders, renderer, text overlay, video export
//   src/film     sequencer engine and the GARGANTUA film
//   src/editor   the editor UI
//   src/app      shared application state
//   main.cpp     window, input and the frame loop
// =====================================================================================
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#include "app/app.h"
#include "audio/audio.h"
#include "core/gl.h"
#include "core/util.h"
#include "editor/editor.h"
#include "film/sequence.h"
#include "render/export.h"
#include "render/overlay.h"
#include "render/refine.h"
#include "render/renderer.h"
#include "sim/params.h"
#include "sim/physics.h"
#include "sim/scene.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

#ifdef _MSC_VER
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
#endif

static int g_lastMX = 0, g_lastMY = 0;
static WINDOWPLACEMENT g_wp = {sizeof(WINDOWPLACEMENT)};

void toggleFullscreen() {
    DWORD style = (DWORD)GetWindowLongPtrA(g_hwnd, GWL_STYLE);
    if (!g_fullscreen) {
        MONITORINFO mi = {sizeof mi};
        GetWindowPlacement(g_hwnd, &g_wp);
        GetMonitorInfoA(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
        SetWindowLongPtrA(g_hwnd, GWL_STYLE, (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
        SetWindowPos(g_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        g_fullscreen = true;
    } else {
        SetWindowLongPtrA(g_hwnd, GWL_STYLE, (style & ~WS_POPUP) | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(g_hwnd, &g_wp);
        SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        g_fullscreen = false;
    }
}

// keyboard everywhere; raw mouse only in cinema mode (in the editor the viewport panel owns the mouse)
static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(h, m, wp, lp)) return 1;
    bool imguiReady = ImGui::GetCurrentContext() != nullptr;
    bool uiMouse = imguiReady && ImGui::GetIO().WantCaptureMouse;
    bool uiKeys  = imguiReady && ImGui::GetIO().WantCaptureKeyboard;

    switch (m) {
    case WM_CLOSE: case WM_DESTROY: g_running = false; return 0;
    case WM_SIZE: g_w = std::max(1, (int)LOWORD(lp)); g_h = std::max(1, (int)HIWORD(lp)); return 0;
    case WM_SYSKEYDOWN:
        if (wp == VK_RETURN) { toggleFullscreen(); return 0; }
        break;
    case WM_KEYDOWN:
        if (uiKeys) break;
        switch (wp) {
        case VK_ESCAPE: if (!g_showUI) g_showUI = true; break;
        case VK_F11:    toggleFullscreen(); break;
        case VK_F12:    if (!g_job.active) beginExport(true); break;
        case VK_TAB:    if (g_showUI) enterCinema(); else g_showUI = true; break;
        case 'P':       g_paused = !g_paused; break;                // freeze all motion
        case VK_SPACE:
            if (g_camMode == CAM_SEQUENCE) g_playing = !g_playing; else g_paused = !g_paused;
            break;
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (uiMouse || g_job.active || g_showUI) break;
        grabCamera();
        g_dragging = true; g_lastMX = GET_X_LPARAM(lp); g_lastMY = GET_Y_LPARAM(lp);
        SetCapture(h); return 0;
    case WM_LBUTTONUP:
        if (g_dragging && !g_showUI) { g_dragging = false; ReleaseCapture(); }
        break;
    case WM_MOUSEMOVE:
        if (g_dragging && !g_showUI) {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            g_orbit.phi += (x - g_lastMX) * 0.005f;
            g_orbit.el   = std::max(-1.53f, std::min(1.53f, g_orbit.el + (y - g_lastMY) * 0.004f));
            g_lastMX = x; g_lastMY = y;
        }
        break;
    case WM_MOUSEWHEEL:
        if (uiMouse || g_job.active || g_showUI) break;
        grabCamera();
        g_orbit.dist = std::max(MIN_DIST, std::min(300.0f, g_orbit.dist * std::pow(0.9f, GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA)));
        return 0;
    }
    return DefWindowProcA(h, m, wp, lp);
}

int main() {
    ImGui_ImplWin32_EnableDpiAwareness();
    g_filmCameraAt = evalSequence;
    defaultSequence();

    // --stills t1,t2,...  [--res 0..3] [--samples n] [--spin a] [--sequence file] [--prefix name_]
    //   saves screenshots of the film at those times into renders/, then quits
    // --render          renders the film to video with the current settings, then quits
    // --at t            opens the film paused at time t
    // --soundtrack file writes the film's soundtrack as a WAV, then quits
    std::vector<float> stills;
    bool renderMode = false, renderStarted = false;
    for (int i = 1; i < __argc; i++) {
        std::string a = __argv[i];
        if (a == "--stills" && i + 1 < __argc) {
            for (char* tok = std::strtok(__argv[++i], ","); tok; tok = std::strtok(nullptr, ",")) stills.push_back((float)std::atof(tok));
        } else if (a == "--res" && i + 1 < __argc) g_exRes = std::max(0, std::min(3, std::atoi(__argv[++i])));
        else if (a == "--samples" && i + 1 < __argc) g_exSamples = std::max(1, std::atoi(__argv[++i]));
        else if (a == "--sequence" && i + 1 < __argc) loadSequence(__argv[++i]);
        else if (a == "--spin" && i + 1 < __argc) P.spin = std::max(0.0f, std::min(0.998f, (float)std::atof(__argv[++i])));
        else if (a == "--prefix" && i + 1 < __argc) g_stillPrefix = __argv[++i];
        else if (a == "--render") renderMode = true;
        else if (a == "--fps" && i + 1 < __argc) g_exFps = std::max(0, std::min(2, std::atoi(__argv[++i])));
        else if (a == "--at" && i + 1 < __argc) {                   // open the film paused at this time
            g_playhead = (float)std::atof(__argv[++i]); g_playing = false;
        }
        else if (a == "--soundtrack" && i + 1 < __argc) {           // write the film's soundtrack and quit
            return renderSoundtrack(__argv[++i], seqLength()) ? 0 : 1;
        }
    }
    size_t stillNext = 0;
    if (!stills.empty() || renderMode) { CreateDirectoryA("renders", nullptr); g_errorLog = "renders/error_log.txt"; }

    HINSTANCE inst = GetModuleHandleA(nullptr);
    WNDCLASSA wc = {};
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "GargantuaStudio";
    RegisterClassA(&wc);

    g_hwnd = CreateWindowA(wc.lpszClassName, "Gargantua Studio", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 1600, 900, nullptr, nullptr, inst, nullptr);
    HDC dc = GetDC(g_hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof pfd; pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 24;
    SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd);
    HGLRC glrc = wglCreateContext(dc);
    wglMakeCurrent(dc, glrc);
    if (!loadGL()) fatal("OpenGL", "Your GPU driver does not expose OpenGL 3.3. Update your graphics driver.");
    if (p_wglSwapIntervalEXT) p_wglSwapIntervalEXT(1);

    createPrograms();
    GLuint vao; p_glGenVertexArrays(1, &vao); p_glBindVertexArray(vao);

    ShowWindow(g_hwnd, SW_SHOWMAXIMIZED);
    UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    g_dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(g_hwnd);
    editorInit();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplOpenGL3_Init("#version 330");
    if (stills.empty() && !renderMode) audioOpen();

    Target scene, view8;                 // HDR scene at render scale, graded LDR image for the viewport panel
    std::vector<Target> mips;

    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);
    float fps = 60.0f;

    while (g_running) {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        if (!g_running) break;
        if (IsIconic(g_hwnd) && !g_job.active) { Sleep(30); QueryPerformanceCounter(&prev); continue; }

        QueryPerformanceCounter(&now);
        float dt = std::min(0.1f, (float)(now.QuadPart - prev.QuadPart) / (float)freq.QuadPart);
        prev = now;
        fps = fps * 0.95f + (1.0f / std::max(dt, 1e-4f)) * 0.05f;

        if (!stills.empty() && !g_job.active) {
            if (stillNext == stills.size()) break;
            g_camMode = CAM_SEQUENCE; g_playing = false;
            g_playhead = stills[stillNext++];
            g_stillName = g_stillPrefix + fmt("%06.2f", g_playhead);
            g_exUltra = true;
            beginExport(true);
        }

        if (renderMode && !g_job.active) {
            if (renderStarted) break;
            beginExport(false);
            renderStarted = true;
        }

        if (P.iscoEdge) P.diskIn = iscoRs(P.spin) * std::max(P.gravity, 0.01f);
        P.diskOut = std::max(P.diskOut, P.diskIn + 0.5f);

        // ---- advance time
        if (!g_job.active) {
            if (!g_paused) g_simTime += dt;
            if (g_camMode == CAM_ORBIT && g_autoRotate && !g_paused && !g_dragging) g_orbit.phi += g_rotSpeed * dt;
            if (g_camMode == CAM_SEQUENCE && g_playing && !g_paused) {
                g_playhead += dt * g_playSpeed;
                float L = seqLength();
                if (g_playhead > L) { if (g_loop && L > 0) g_playhead = std::fmod(g_playhead, L); else { g_playhead = L; g_playing = false; } }
            }
        }

        // ---- render the scene: into the viewport panel's texture (size from the last UI frame), or the
        //      whole window in cinema mode
        int outW = g_showUI ? g_viewW : g_w, outH = g_showUI ? g_viewH : g_h;
        bool drewToWindow = false;
        if (g_job.active) {
            exportStep();
        } else if (outW >= 8 && outH >= 8) {
            CamState cam = currentCam();
            CamFrame cf = camBasis(cam);
            float t = currentTime();
            if (g_showUI && (view8.w != outW || view8.h != outH)) { freeTarget(view8); view8 = makeTarget(outW, outH, false); }
            // a still picture is refined at full resolution; otherwise the fast preview at render scale
            bool film = g_camMode == CAM_SEQUENCE;
            const Target* img = refineUpdate(outW, outH, cf, t, film, g_refineOn);
            std::vector<Target>* imgMips = &refineMips();
            if (!img) {
                int sw = std::max(1, (int)(outW * P.renderScale)), sh = std::max(1, (int)(outH * P.renderScale));
                if (scene.w != sw || scene.h != sh) { freeTarget(scene); scene = makeTarget(sw, sh); buildMips(mips, sw, sh); }
                p_glBindFramebuffer(K_FRAMEBUFFER, scene.fbo);
                glViewport(0, 0, scene.w, scene.h);
                setSceneUniforms(scene.w, scene.h, cf, t, 0, 0, P.ultra ? 0.012f : 0.03f, film);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                runBloom(scene, mips);
                img = &scene; imgMips = &mips;
            }
            overlayUpdate(g_ovView, outW, outH, g_playhead, g_camMode == CAM_SEQUENCE);
            composite(g_showUI ? view8.fbo : 0, outW, outH, *img, *imgMips, t, currentFade(), g_ovView,
                      P.exposure * autoExposure(cam), telescopeAmount(t, film));
            drewToWindow = !g_showUI;
        }
        p_glBindFramebuffer(K_FRAMEBUFFER, 0);
        glViewport(0, 0, g_w, g_h);
        if (!drewToWindow) { glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT); }
        if (g_job.active && g_job.haveFrame && !g_showUI) {      // cinema mode while rendering: show the frame
            float a = (float)g_job.W / g_job.H, vw = (float)g_w, vh = vw / a;
            if (vh > g_h) { vh = (float)g_h; vw = vh * a; }
            float vx = (g_w - vw) * 0.5f, vy = (g_h - vh) * 0.5f;
            glViewport((int)vx, (int)vy, (int)vw, (int)vh);
            copyTex(g_job.out8.tex, vx, vy, vw, vh, 1.0f);
            glViewport(0, 0, g_w, g_h);
        }

        // ---- sound: the organ follows the camera live; silent while paused or rendering
        {
            bool running = !g_job.active && !g_paused && (g_camMode == CAM_ORBIT || g_playing);
            SoundControls ctl = g_camMode == CAM_SEQUENCE ? filmSound(g_playhead)
                                                          : soundControls(currentCam(), 1.0f, currentTime(), false);
            ctl.level *= (g_soundOn && running) ? g_volume : 0.0f;
            audioPump(currentTime(), running, ctl);
        }

        // ---- editor
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        drawEditor(fps, view8.tex, scene.w, scene.h);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SwapBuffers(dc);
    }

    if (g_job.active) endExport(true);
    audioClose();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(glrc);
    ReleaseDC(g_hwnd, dc);
    DestroyWindow(g_hwnd);
    return 0;
}
