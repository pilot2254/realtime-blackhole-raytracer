#pragma once
// Offline rendering of the film (or one screenshot) in tiles, with motion blur and antialiasing,
// piped to ffmpeg. Everything lands in the renders/ folder.
#include <cstdint>
#include <string>
#include <vector>
#include "core/gl.h"
#include "sim/camera.h"

struct ExportJob {
    bool active = false, screenshot = false, useFfmpeg = false, haveFrame = false;
    int W = 1920, H = 1080, fps = 60, samples = 8, frame = 0, total = 0, sub = 0, tile = 0;
    float shutter = 0.5f, step = 0.012f;
    HANDLE pipe = nullptr, proc = nullptr;
    std::string out, folder;
    CamState shotCam; float shotTime = 0, shotFade = 1; bool shotSeq = false;
    LARGE_INTEGER t0;
    Target scene, accum, out8;
    std::vector<Target> mips;
    std::vector<uint8_t> px;
};
extern ExportJob g_job;

inline constexpr const char* RENDER_DIR = "renders";
inline constexpr int RES_W[] = {1280, 1920, 2560, 3840}, RES_H[] = {720, 1080, 1440, 2160};
inline constexpr int FPS_V[] = {24, 30, 60};
inline constexpr int TILE = 256;

extern int   g_exRes, g_exFps, g_exSamples;
extern float g_exShutter;
extern bool  g_exUltra;
extern char  g_exName[260];                     // video file name inside renders/
extern char  g_ffmpeg[260];
extern std::string g_stillName, g_stillPrefix;  // fixed screenshot file name (used by --stills)

std::string renderPath(const std::string& name); // renders\name, creating the folder
void  openRendersFolder();
float exportProgress();                         // 0..1 of the running job
void  beginExport(bool screenshot);
void  endExport(bool cancelled);
void  exportStep();                             // renders tiles for ~45 ms, call once per frame
