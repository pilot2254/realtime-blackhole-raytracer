#include "render/export.h"
#include <shellapi.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "app/app.h"
#include "audio/audio.h"
#include "core/util.h"
#include "film/sequence.h"
#include "render/overlay.h"
#include "render/renderer.h"
#include "sim/params.h"
#include "sim/physics.h"

ExportJob g_job;

int   g_exRes = 1, g_exFps = 2, g_exSamples = 8;
float g_exShutter = 0.5f;
bool  g_exUltra = true;
char  g_exName[260] = "gargantua.mp4";
char  g_ffmpeg[260] = "ffmpeg";
std::string g_stillName, g_stillPrefix = "still_";

std::string renderPath(const std::string& name) {
    CreateDirectoryA(RENDER_DIR, nullptr);
    return std::string(RENDER_DIR) + "\\" + name;
}

void openRendersFolder() {
    CreateDirectoryA(RENDER_DIR, nullptr);
    ShellExecuteA(nullptr, "open", RENDER_DIR, nullptr, nullptr, SW_SHOWNORMAL);
}

float exportProgress() {
    const ExportJob& j = g_job;
    if (!j.active) return 0.0f;
    int tiles = ((j.W + TILE - 1) / TILE) * ((j.H + TILE - 1) / TILE);
    return (j.frame + (j.sub + j.tile / (float)tiles) / j.samples) / (float)j.total;
}

static bool startFfmpeg(const std::string& args) {
    SECURITY_ATTRIBUTES sa = {sizeof sa, nullptr, TRUE};
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 1 << 20)) return false;
    SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0);
    HANDLE log = CreateFileA(renderPath("ffmpeg_log.txt").c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOA si = {sizeof si};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = rd; si.hStdOutput = log; si.hStdError = log;
    PROCESS_INFORMATION pi = {};
    std::string cmd = "\"" + std::string(g_ffmpeg) + "\" " + args;
    std::vector<char> cmdBuf(cmd.begin(), cmd.end()); cmdBuf.push_back(0);
    BOOL ok = CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(rd);
    if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
    if (!ok) { CloseHandle(wr); return false; }
    CloseHandle(pi.hThread);
    g_job.pipe = wr; g_job.proc = pi.hProcess;
    return true;
}

static bool writeBMP(const std::string& path, int w, int h, const uint8_t* bgrBottomUp) {
    int stride = (w * 3 + 3) & ~3;
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    uint32_t size = 54 + stride * h;
    uint8_t hdr[54] = {'B', 'M'};
    auto put32 = [&](int off, uint32_t v) { std::memcpy(hdr + off, &v, 4); };
    put32(2, size); put32(10, 54); put32(14, 40); put32(18, w); put32(22, h);
    hdr[26] = 1; hdr[28] = 24; put32(34, stride * h);
    std::fwrite(hdr, 1, 54, f);
    std::vector<uint8_t> row(stride, 0);
    for (int y = 0; y < h; y++) {
        std::memcpy(row.data(), bgrBottomUp + (size_t)y * w * 3, (size_t)w * 3);
        std::fwrite(row.data(), 1, stride, f);
    }
    std::fclose(f);
    return true;
}


void beginExport(bool screenshot) {
    ExportJob& j = g_job;
    j = ExportJob{};
    j.screenshot = screenshot;
    j.W = RES_W[g_exRes]; j.H = RES_H[g_exRes];
    j.fps = FPS_V[g_exFps];
    j.samples = std::max(1, g_exSamples);
    j.shutter = screenshot ? 0.0f : g_exShutter;
    j.step = g_exUltra ? 0.012f : 0.03f;
    j.total = screenshot ? 1 : std::max(1, (int)std::ceil(seqLength() * j.fps));
    if (screenshot) {
        j.shotCam = currentCam(); j.shotTime = currentTime();
        j.shotSeq = g_camMode == CAM_SEQUENCE; j.shotFade = currentFade();
    }

    char sz[64]; std::snprintf(sz, sizeof sz, "%dx%d", j.W, j.H);
    std::string args;
    if (screenshot) {
        j.out = renderPath(g_stillName.empty() ? "gargantua_" + timestamp() + ".png" : g_stillName + ".png");
        args = std::string("-y -loglevel error -f rawvideo -pix_fmt rgb24 -s ") + sz + " -i - -frames:v 1 \"" + j.out + "\"";
    } else {
        j.out = renderPath(g_exName);
        // the soundtrack is rendered offline first (a few seconds), then muxed in by ffmpeg
        std::string audio, stem = g_exName;
        if (stem.size() > 4 && stem.compare(stem.size() - 4, 4, ".mp4") == 0) stem.resize(stem.size() - 4);
        std::string wav = renderPath(stem + "_soundtrack.wav");
        if (g_exAudio && renderSoundtrack(wav, seqLength()))
            audio = " -i \"" + wav + "\" -map 0:v -map 1:a -c:a aac -b:a 256k -shortest";
        args = std::string("-y -loglevel error -f rawvideo -pix_fmt rgb24 -s ") + sz + " -r " + std::to_string(j.fps) +
               " -i -" + audio + " -c:v libx264 -preset slow -crf 14 -pix_fmt yuv420p -movflags +faststart \"" + j.out + "\"";
    }
    j.useFfmpeg = startFfmpeg(args);
    if (!j.useFfmpeg) {
        if (screenshot) j.out = renderPath(g_stillName.empty() ? "gargantua_" + timestamp() + ".bmp" : g_stillName + ".bmp");
        else { j.folder = renderPath("frames_" + timestamp()); CreateDirectoryA(j.folder.c_str(), nullptr); }
    }

    j.scene = makeTarget(j.W, j.H);
    j.accum = makeTarget(j.W, j.H);
    j.out8  = makeTarget(j.W, j.H, false);
    buildMips(j.mips, j.W, j.H);
    j.px.resize((size_t)j.W * j.H * 3);
    QueryPerformanceCounter(&j.t0);
    j.active = true;
    g_status.clear();
}

void endExport(bool cancelled) {
    ExportJob& j = g_job;
    if (j.pipe) { CloseHandle(j.pipe); j.pipe = nullptr; }        // EOF -> ffmpeg finalizes the file
    if (j.proc) { WaitForSingleObject(j.proc, 30000); CloseHandle(j.proc); j.proc = nullptr; }
    freeTarget(j.scene); freeTarget(j.accum); freeTarget(j.out8);
    for (auto& m : j.mips) freeTarget(m);
    j.mips.clear();
    overlayFree(g_ovExport);
    j.active = false;
    char b[512];
    if (cancelled) std::snprintf(b, sizeof b, "Cancelled after %d frames.", j.frame);
    else if (j.useFfmpeg || j.screenshot)
        std::snprintf(b, sizeof b, "Done: %s  (%.1f s)", j.out.c_str(), secondsSince(j.t0));
    else
        std::snprintf(b, sizeof b, "Done: %d BMP frames in %s\\  (ffmpeg not found)", j.total, j.folder.c_str());
    g_status = b;
}

static void finishFrame() {
    ExportJob& j = g_job;
    float ft   = j.screenshot ? j.shotTime : j.frame / (float)j.fps;
    float fade = j.screenshot ? j.shotFade : seqFade(ft);
    runBloom(j.accum, j.mips);
    overlayUpdate(g_ovExport, j.W, j.H, ft, !j.screenshot || j.shotSeq);
    float ex = P.exposure * autoExposure(j.screenshot ? j.shotCam : evalSequence(ft));
    composite(j.out8.fbo, j.W, j.H, j.accum, j.mips, ft, fade, g_ovExport, ex, telescopeAmount(ft, !j.screenshot || j.shotSeq));
    j.haveFrame = true;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    if (j.useFfmpeg) {
        glReadPixels(0, 0, j.W, j.H, GL_RGB, GL_UNSIGNED_BYTE, j.px.data());
        size_t row = (size_t)j.W * 3;
        std::vector<uint8_t> tmp(row);
        for (int y = 0; y < j.H / 2; y++) {                      // GL is bottom-up, video is top-down
            uint8_t* a = &j.px[(size_t)y * row];
            uint8_t* b = &j.px[(size_t)(j.H - 1 - y) * row];
            std::memcpy(tmp.data(), a, row); std::memcpy(a, b, row); std::memcpy(b, tmp.data(), row);
        }
        DWORD written = 0;
        if (!WriteFile(j.pipe, j.px.data(), (DWORD)j.px.size(), &written, nullptr)) {
            endExport(true);
            g_status = "ffmpeg stopped accepting frames, see renders\\ffmpeg_log.txt";
            return;
        }
    } else {
        glReadPixels(0, 0, j.W, j.H, K_BGR, GL_UNSIGNED_BYTE, j.px.data());
        std::string path = j.screenshot ? j.out : (j.folder + "\\frame_" + std::to_string(100000 + j.frame).substr(1) + ".bmp");
        writeBMP(path, j.W, j.H, j.px.data());
    }
    j.frame++;
    if (j.frame >= j.total) endExport(false);
}

// renders tiles of sub-frames until the time budget is used, keeping the UI responsive
void exportStep() {
    ExportJob& j = g_job;
    LARGE_INTEGER start; QueryPerformanceCounter(&start);
    int tx = (j.W + TILE - 1) / TILE, ty = (j.H + TILE - 1) / TILE;

    while (j.active && secondsSince(start) < 0.045) {
        float ft = j.screenshot ? j.shotTime : j.frame / (float)j.fps;
        float st = ft + (j.shutter / j.fps) * ((j.sub + 0.5f) / j.samples);
        CamState cs = j.screenshot ? j.shotCam : evalSequence(st);
        float sim   = j.screenshot ? j.shotTime : st;
        int hi = j.frame * j.samples + j.sub + 1;
        float jx = j.samples > 1 ? halton(hi, 2) - 0.5f : 0.0f;
        float jy = j.samples > 1 ? halton(hi, 3) - 0.5f : 0.0f;

        p_glBindFramebuffer(K_FRAMEBUFFER, j.scene.fbo);
        glViewport(0, 0, j.W, j.H);
        setSceneUniforms(j.W, j.H, camBasis(cs), sim, jx, jy, j.step, !j.screenshot || j.shotSeq);
        glEnable(GL_SCISSOR_TEST);
        glScissor((j.tile % tx) * TILE, (j.tile / tx) * TILE, TILE, TILE);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisable(GL_SCISSOR_TEST);
        glFinish();                                       // keep each GPU submission short (TDR-safe)

        if (++j.tile < tx * ty) continue;
        j.tile = 0;

        // accumulate this sub-frame
        p_glBindFramebuffer(K_FRAMEBUFFER, j.accum.fbo);
        glViewport(0, 0, j.W, j.H);
        if (j.sub == 0) { glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT); }
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        copyTex(j.scene.tex, 0, 0, (float)j.W, (float)j.H, 1.0f / j.samples);
        glDisable(GL_BLEND);

        if (++j.sub < j.samples) continue;
        j.sub = 0;
        finishFrame();
    }
}
