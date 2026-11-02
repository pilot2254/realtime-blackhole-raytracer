#pragma once
// Application state shared by the main loop, the editor and the exporter.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include "sim/camera.h"

enum { CAM_ORBIT = 0, CAM_SEQUENCE = 1 };
extern int      g_camMode;
extern CamState g_orbit;                        // the free camera
extern bool     g_autoRotate, g_drift;
extern float    g_rotSpeed, g_simTime;
extern bool     g_paused;

extern HWND  g_hwnd;
extern int   g_w, g_h;                          // window client size
extern bool  g_running, g_fullscreen, g_dragging, g_showUI;
extern float g_dpi;

extern char        g_seqPath[260];
extern std::string g_status;                    // last message for the status bar

CamState orbitEffective();                      // free camera with the cinematic drift applied
CamState currentCam();
float    currentTime();
float    currentFade();
void     grabCamera();                          // switch to the free camera, keeping the exact view
void     toggleFullscreen();                    // main.cpp
