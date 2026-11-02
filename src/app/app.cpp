#include "app/app.h"
#include "film/sequence.h"
#include "sim/params.h"

Params P;

int      g_camMode = CAM_SEQUENCE;
CamState g_orbit;
bool     g_autoRotate = true, g_drift = true;
float    g_rotSpeed = 0.11f, g_simTime = 0.0f;
bool     g_paused = false;

HWND  g_hwnd;
int   g_w = 1600, g_h = 900;
bool  g_running = true, g_fullscreen = false, g_dragging = false, g_showUI = true;
float g_dpi = 1.0f;

char        g_seqPath[260] = "sequence.txt";
std::string g_status;

CamState orbitEffective() {
    CamState c = g_orbit;
    if (g_drift) {
        c.el   += 0.10f * std::sin(g_simTime * 0.19f);
        c.dist *= 1.0f + 0.12f * std::sin(g_simTime * 0.13f);
        c.roll += 0.15f * std::sin(g_simTime * 0.17f);
    }
    return c;
}
CamState currentCam()   { return g_camMode == CAM_SEQUENCE ? evalSequence(g_playhead) : orbitEffective(); }
float    currentTime()  { return g_camMode == CAM_SEQUENCE ? g_playhead : g_simTime; }
float    currentFade()  { return g_camMode == CAM_SEQUENCE ? seqFade(g_playhead) : 1.0f; }

// switch to orbit mode keeping the exact current view (used when the user grabs the camera)
void grabCamera() {
    if (g_camMode == CAM_SEQUENCE) {
        g_orbit = evalSequence(g_playhead);
        g_simTime = g_playhead;
        g_camMode = CAM_ORBIT;
        g_drift = false;
        g_autoRotate = false;
    } else if (g_drift) {
        g_orbit = orbitEffective();
        g_drift = false;
    }
}
