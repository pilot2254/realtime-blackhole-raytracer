#include "render/overlay.h"
#include <cmath>
#include <cstring>
#include "core/util.h"
#include "sim/params.h"
#include "sim/physics.h"

// ============================================================================= film overlay
// Titles, captions and the instrument readout are drawn with GDI into a DIB as white on black (the
// brightness is the coverage), uploaded to a texture and laid over the graded picture by the composite.
Overlay g_ovView, g_ovExport;

float barHeight(int w, int h) { return g_letterboxOn ? std::max(0.0f, (h - w / g_letterbox) * 0.5f) : 0.0f; }

void overlayFree(Overlay& o) {
    for (auto& f : o.font) if (f) { DeleteObject(f); f = nullptr; }
    if (o.dc) { DeleteDC(o.dc); o.dc = nullptr; }
    if (o.bmp) { DeleteObject(o.bmp); o.bmp = nullptr; }
    if (o.tex) { glDeleteTextures(1, &o.tex); o.tex = 0; }
    o.w = o.h = 0; o.sig.clear(); o.visible = false;
}

void overlayResize(Overlay& o, int w, int h) {
    if (o.dc && o.w == w && o.h == h) return;
    overlayFree(o);
    o.w = w; o.h = h;
    o.dc = CreateCompatibleDC(nullptr);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;          // top-down rows
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    o.bmp = CreateDIBSection(o.dc, &bi, DIB_RGB_COLORS, (void**)&o.bits, nullptr, 0);
    SelectObject(o.dc, o.bmp);
    SetBkMode(o.dc, TRANSPARENT);

    float s = std::min(h / 1080.0f, w / 1920.0f);
    auto mk = [&](float px, int weight, const wchar_t* face) {
        return CreateFontW(-(int)std::max(6.0f, px * s), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
                           CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face);
    };
    o.font[CUE_TITLE]    = mk(112, FW_LIGHT,  L"Segoe UI Light");
    o.font[CUE_SUBTITLE] = mk(32,  FW_LIGHT,  L"Segoe UI Light");
    o.font[CUE_CAPTION]  = mk(30,  FW_NORMAL, L"Segoe UI Semilight");
    o.font[CUE_CHAPTER]  = mk(24,  FW_SEMIBOLD, L"Segoe UI Semibold");
    o.font[CUE_HUD]      = mk(20,  FW_NORMAL, L"Consolas");

    glGenTextures(1, &o.tex);
    glBindTexture(GL_TEXTURE_2D, o.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, K_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, K_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, K_CLAMP_TO_EDGE);
}


// instrument readout for a camera, in real units for a black hole of g_massSun suns
std::vector<std::pair<std::string, std::string>> hudLines(const CamState& c) {
    std::vector<std::pair<std::string, std::string>> L;
    double r    = std::max(MIN_DIST, c.dist);
    double rsM  = 2953.25 * g_massSun;                  // Schwarzschild radius, metres
    double rsC  = rsM / 299792458.0;                    // light crossing time, seconds
    double beta = std::sqrt(1.0 / r);
    float  kf = 0; V3 kl;
    if (P.spin > 0.0f) r = ksField(toStd(camBasis(c).pos), kf, kl);       // Boyer-Lindquist radius
    double au   = r * rsM / 1.495978707e11;
    L.push_back({"DISTANCE", au > 0.05 ? fmt("%.2f r_s   %.2f AU", r, au) : fmt("%.3f r_s   %.0f km", r, r * rsM / 1000.0)});
    if (P.spin > 0.0f) {
        // static observers move at speed f along l relative to the Kerr-Schild normal observer
        if (kf < 0.999f) {
            double s = kerrCamSpeed(kf, c.fall), vrel = (s - kf) / (1.0 - s * kf);
            double gam = 1.0 / std::sqrt(1.0 - vrel * vrel);
            L.push_back({"VELOCITY", std::fabs(vrel) < 1e-3 ? std::string("hovering") : fmt("%.3f c inward", -vrel)});
            L.push_back({"CLOCKS", fmt("1 s here = %.3f s far away", gam / std::sqrt(1.0 - kf))});
        } else if (r > horizonRs(P.spin, 1.0f)) {
            L.push_back({"VELOCITY", "ergosphere: space drags everything around"});
            L.push_back({"CLOCKS", "no clock here can stand still"});
        } else {
            L.push_back({"VELOCITY", "no observer can stand still here"});
            L.push_back({"CLOCKS", "outside time no longer reaches us"});
        }
    } else if (r > 1.0005) {
        double v    = camRadialVel((float)r, c.fall);
        double vrel = (v - beta) / (1.0 - v * beta);    // relative to an observer hovering here
        double gam  = 1.0 / std::sqrt(1.0 - vrel * vrel);
        L.push_back({"VELOCITY", std::fabs(vrel) < 1e-3 ? std::string("hovering") : fmt("%.3f c inward", -vrel)});
        L.push_back({"CLOCKS", fmt("1 s here = %.3f s far away", gam / std::sqrt(1.0 - 1.0 / r))});
    } else {
        L.push_back({"VELOCITY", "no observer can stand still here"});
        L.push_back({"CLOCKS", "outside time no longer reaches us"});
    }
    double tide = 299792458.0 * 299792458.0 * 2.0 / (rsM * rsM * r * r * r) / 9.81;   // head-to-toe, 2 m
    L.push_back({"TIDES", tide < 0.01 ? fmt("%.1e g head to toe", tide) : fmt("%.2f g head to toe", tide)});
    if (P.spin > 0.0f)
        L.push_back({"SPIN", fmt("a = %.2f M   horizons at %.2f and %.2f r_s", P.spin, horizonRs(P.spin, 1.0f), horizonRs(P.spin, -1.0f))});
    else if (c.fall > 0.5)
        L.push_back({"SINGULARITY", "in " + fmtDuration(2.0 / 3.0 * std::pow(r, 1.5) * rsC) + " of our time"});
    return L;
}

// draws the cues visible at sequence time t; only redraws and uploads when something changed
void overlayUpdate(Overlay& o, int w, int h, float t, bool film) {
    overlayResize(o, w, h);
    struct Item { int style; float a; std::string text; };
    std::vector<Item> items;
    std::vector<std::pair<std::string, std::string>> hud;
    std::string sig;
    if (film && g_cuesOn) {
        for (auto& c : g_cues) {
            float fd = (c.style == CUE_TITLE || c.style == CUE_SUBTITLE) ? 1.5f : 0.7f;
            float a  = smoothstepf(c.t0, c.t0 + fd, t) * smoothstepf(c.t1, c.t1 - fd, t);
            if (a < 0.004f) continue;
            a = std::floor(a * 128.0f) / 128.0f;
            items.push_back({c.style, a, c.text});
            sig += std::to_string(c.style) + fmt(" %.3f|", a) + c.text;
            if (c.style == CUE_HUD) {
                hud = hudLines(evalSequence(t));
                for (auto& l : hud) sig += l.second;
            }
        }
    }
    sig += fmt("%.3f %.0f", g_letterboxOn ? g_letterbox : 0.0f, g_massSun);
    o.visible = !items.empty();
    if (sig == o.sig) return;
    o.sig = sig;
    if (!o.visible) return;

    std::memset(o.bits, 0, (size_t)w * h * 4);
    float bar = barHeight(w, h), ph = h - 2 * bar;
    auto draw = [&](int style, float a, RECT rc, UINT flags, const std::wstring& s, int spacing) {
        int v = (int)(255.0f * std::min(1.0f, a) + 0.5f);
        SelectObject(o.dc, o.font[style]);
        SetTextCharacterExtra(o.dc, spacing);
        SetTextColor(o.dc, RGB(v, v, v));
        DrawTextW(o.dc, s.c_str(), -1, &rc, flags | DT_NOPREFIX);
    };
    auto drawCentered = [&](int style, float a, int cy, const std::wstring& s, int spacing) {
        SelectObject(o.dc, o.font[style]);
        SetTextCharacterExtra(o.dc, spacing);
        SIZE sz; GetTextExtentPoint32W(o.dc, s.c_str(), (int)s.size(), &sz);
        int x = (w - (sz.cx - spacing)) / 2;                    // the last letter's spacing is not ink
        draw(style, a, {x, cy - sz.cy / 2, w, cy + sz.cy}, DT_LEFT | DT_SINGLELINE, s, spacing);
    };
    auto fontPx = [&](int style) { TEXTMETRICW tm; SelectObject(o.dc, o.font[style]); GetTextMetricsW(o.dc, &tm); return tm; };

    for (auto& it : items) {
        std::wstring s = widen(it.text);
        TEXTMETRICW tm = fontPx(it.style);
        int lh = tm.tmHeight;
        switch (it.style) {
        case CUE_TITLE: {
            int sp = (int)(lh * 0.42f), cy = (int)(bar + ph * 0.44f);
            drawCentered(it.style, it.a, cy, s, sp);
        } break;
        case CUE_SUBTITLE: {
            int sp = (int)(lh * 0.18f), cy = (int)(bar + ph * 0.44f + fontPx(CUE_TITLE).tmHeight * 0.85f);
            drawCentered(it.style, it.a * 0.85f, cy, s, sp);
        } break;
        case CUE_CAPTION: {
            RECT rc = {(LONG)(w * 0.14f), 0, (LONG)(w * 0.86f), 0};
            SelectObject(o.dc, o.font[it.style]); SetTextCharacterExtra(o.dc, 0);
            DrawTextW(o.dc, s.c_str(), -1, &rc, DT_CENTER | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
            int th = rc.bottom - rc.top;
            int top = bar >= th + lh * 0.6f ? (int)(h - bar * 0.5f - th * 0.5f) : (int)(h - bar - ph * 0.07f - th);
            rc.left = (LONG)(w * 0.14f); rc.right = (LONG)(w * 0.86f); rc.top = top; rc.bottom = top + th + 2;
            draw(it.style, it.a, rc, DT_CENTER | DT_WORDBREAK, s, 0);
        } break;
        case CUE_CHAPTER: {
            int x = (int)(w * 0.045f), y = (int)(bar + ph * 0.07f);
            draw(it.style, it.a * 0.9f, {x, y, w, y + lh * 2}, DT_LEFT | DT_SINGLELINE, s, (int)(lh * 0.3f));
        } break;
        case CUE_HUD: {
            int cw = tm.tmAveCharWidth, x = (int)(w * 0.045f);
            int y = (int)(h - bar - ph * 0.07f - lh * 1.25f * hud.size());
            for (auto& l : hud) {
                draw(it.style, it.a * 0.5f, {x, y, w, y + lh * 2}, DT_LEFT | DT_SINGLELINE, widen(l.first), 0);
                draw(it.style, it.a * 0.95f, {x + cw * 13, y, w, y + lh * 2}, DT_LEFT | DT_SINGLELINE, widen(l.second), 0);
                y += (int)(lh * 1.25f);
            }
        } break;
        }
    }
    GdiFlush();
    glBindTexture(GL_TEXTURE_2D, o.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, 0x80E1 /* GL_BGRA */, GL_UNSIGNED_BYTE, o.bits);
}
