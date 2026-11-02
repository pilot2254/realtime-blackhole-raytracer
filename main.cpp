// =====================================================================================
//   G A R G A N T U A   S T U D I O
//   Real-time Schwarzschild black hole ray tracer + live controls + camera sequencer
//   + offline cinematic video export (motion blur, antialiasing, up to 4K).
//
//   Win32 + OpenGL 3.3 + Dear ImGui. Windows only.
//
//   Physics (units: Schwarzschild radius r_s = 1  ->  G = c = 1, M = 1/2)
//   - Null geodesics via the Binet equation u'' + u = (3/2) r_s u^2, reproduced exactly by
//     the Cartesian pseudo-force a = -(3/2) h^2 p / |p|^5, integrated with leapfrog.
//   - Thin Keplerian accretion disk from the ISCO (3 r_s), Novikov-Thorne-like temperatures.
//   - Doppler shift + gravitational redshift change the observed blackbody temperature,
//     brightness scales with (D g)^4.
//   - Sky (stars, Milky Way, nebulae, a spiral galaxy) is sampled along the bent ray.
//
//   Video export: each frame is the average of N sub-frames spread over the shutter
//   interval (true motion blur) with Halton sub-pixel jitter (antialiasing). Rendering is
//   split into tiles so long frames never trigger the Windows GPU watchdog (TDR).
//   Frames are piped straight into ffmpeg (H.264 MP4). Without ffmpeg, a BMP image
//   sequence is written instead.
//
//   Controls:  drag = orbit   wheel = zoom   TAB = hide UI   SPACE = play/pause
//              F11 / ALT+ENTER = fullscreen   ESC = quit
// =====================================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <GL/gl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

#ifdef _MSC_VER
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
#endif

// ============================================================================= GL loader
typedef char GLch;
#define GLFN(ret, name, args) typedef ret (WINAPI* PFN_##name) args; static PFN_##name p_##name = nullptr;
GLFN(GLuint, glCreateShader, (GLenum))
GLFN(void,   glShaderSource, (GLuint, GLsizei, const GLch* const*, const GLint*))
GLFN(void,   glCompileShader, (GLuint))
GLFN(void,   glGetShaderiv, (GLuint, GLenum, GLint*))
GLFN(void,   glGetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLch*))
GLFN(GLuint, glCreateProgram, (void))
GLFN(void,   glAttachShader, (GLuint, GLuint))
GLFN(void,   glLinkProgram, (GLuint))
GLFN(void,   glGetProgramiv, (GLuint, GLenum, GLint*))
GLFN(void,   glGetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLch*))
GLFN(void,   glDeleteShader, (GLuint))
GLFN(void,   glUseProgram, (GLuint))
GLFN(GLint,  glGetUniformLocation, (GLuint, const GLch*))
GLFN(void,   glUniform1f, (GLint, GLfloat))
GLFN(void,   glUniform2f, (GLint, GLfloat, GLfloat))
GLFN(void,   glUniform3f, (GLint, GLfloat, GLfloat, GLfloat))
GLFN(void,   glUniform1i, (GLint, GLint))
GLFN(void,   glGenVertexArrays, (GLsizei, GLuint*))
GLFN(void,   glBindVertexArray, (GLuint))
GLFN(void,   glGenFramebuffers, (GLsizei, GLuint*))
GLFN(void,   glDeleteFramebuffers, (GLsizei, const GLuint*))
GLFN(void,   glBindFramebuffer, (GLenum, GLuint))
GLFN(void,   glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint))
GLFN(GLenum, glCheckFramebufferStatus, (GLenum))
GLFN(void,   glActiveTexture, (GLenum))
GLFN(BOOL,   wglSwapIntervalEXT, (int))

static bool loadGL() {
    bool ok = true;
#define LOAD(name) p_##name = (PFN_##name)wglGetProcAddress(#name); if (!p_##name) ok = false;
    LOAD(glCreateShader) LOAD(glShaderSource) LOAD(glCompileShader) LOAD(glGetShaderiv)
    LOAD(glGetShaderInfoLog) LOAD(glCreateProgram) LOAD(glAttachShader) LOAD(glLinkProgram)
    LOAD(glGetProgramiv) LOAD(glGetProgramInfoLog) LOAD(glDeleteShader) LOAD(glUseProgram)
    LOAD(glGetUniformLocation) LOAD(glUniform1f) LOAD(glUniform2f) LOAD(glUniform3f)
    LOAD(glUniform1i) LOAD(glGenVertexArrays) LOAD(glBindVertexArray) LOAD(glGenFramebuffers)
    LOAD(glDeleteFramebuffers) LOAD(glBindFramebuffer) LOAD(glFramebufferTexture2D)
    LOAD(glCheckFramebufferStatus) LOAD(glActiveTexture)
#undef LOAD
    p_wglSwapIntervalEXT = (PFN_wglSwapIntervalEXT)wglGetProcAddress("wglSwapIntervalEXT");
    return ok;
}

constexpr GLenum K_FRAGMENT_SHADER = 0x8B30, K_VERTEX_SHADER = 0x8B31, K_COMPILE_STATUS = 0x8B81,
                 K_LINK_STATUS = 0x8B82, K_FRAMEBUFFER = 0x8D40, K_COLOR_ATTACHMENT0 = 0x8CE0,
                 K_FRAMEBUFFER_COMPLETE = 0x8CD5, K_TEXTURE0 = 0x84C0, K_RGBA16F = 0x881A,
                 K_RGBA8 = 0x8058, K_CLAMP_TO_EDGE = 0x812F, K_BGR = 0x80E0;

// ============================================================================= SHADERS
static const char* VS_FULLSCREEN = R"GLSL(
#version 330 core
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

static const char* FS_SCENE_A = R"GLSL(
#version 330 core
out vec4 fragColor;

uniform vec2  uRes;
uniform vec2  uJitter;
uniform float uTime;
uniform vec3  uCamPos;
uniform vec3  uCamFwd;
uniform vec3  uCamRight;
uniform vec3  uCamUp;
uniform float uFov;
uniform float uStep;
uniform vec3  uCamVel;      // camera velocity relative to a free-falling ("rain") observer, units of c
uniform float uObsShift;    // how much of the camera's own Doppler/gravitational shift is shown
uniform float uSpin;        // a / M, 0 = Schwarzschild tracer
uniform float uFall;        // camera: 0 hovering .. 1 free fall (Kerr path; the Schwarzschild path uses uCamVel)

uniform float uGravity;
uniform float uDiskOn;
uniform float uDiskIn;
uniform float uDiskOut;
uniform float uTempPeak;
uniform float uDiskBright;
uniform float uDiskOpacity;
uniform float uSwirl;
uniform float uDoppler;
uniform float uRedshift;
uniform float uGlow;
uniform float uStars;
uniform float uMilky;
uniform float uNebula;
uniform float uGalaxy;

const float PI       = 3.14159265;
const float RS       = 1.0;
const float ESCAPE_R = 60.0;

float hash13(vec3 p3) {
    p3 = fract(p3 * 0.1031);
    p3 += dot(p3, p3.zyx + 31.32);
    return fract((p3.x + p3.y) * p3.z);
}
vec3 hash33(vec3 p3) {
    p3 = fract(p3 * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yxz + 33.33);
    return fract((p3.xxy + p3.yxx) * p3.zyx);
}
float vnoise(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash13(i),                  hash13(i + vec3(1, 0, 0)), f.x),
                   mix(hash13(i + vec3(0, 1, 0)),  hash13(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hash13(i + vec3(0, 0, 1)),  hash13(i + vec3(1, 0, 1)), f.x),
                   mix(hash13(i + vec3(0, 1, 1)),  hash13(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
float fbm(vec3 p, int oct) {
    float a = 0.5, s = 0.0;
    for (int i = 0; i < oct; i++) {
        s += a * vnoise(p);
        p = p * 2.03 + vec3(1.7, 9.2, 3.1);
        a *= 0.5;
    }
    return s;
}

vec3 blackbody(float T) {
    T = clamp(T, 1000.0, 40000.0) / 100.0;
    vec3 c;
    c.r = T <= 66.0 ? 1.0 : clamp(1.29293618606 * pow(T - 60.0, -0.1332047592), 0.0, 1.0);
    c.g = T <= 66.0 ? clamp(0.39008157876 * log(T) - 0.63184144378, 0.0, 1.0)
                    : clamp(1.12989086089 * pow(T - 60.0, -0.0755148492), 0.0, 1.0);
    c.b = T >= 66.0 ? 1.0 : (T <= 19.0 ? 0.0 : clamp(0.54320678911 * log(T - 10.0) - 1.19625408914, 0.0, 1.0));
    return pow(c, vec3(2.2));
}
)GLSL";

static const char* FS_SCENE_B = R"GLSL(
vec3 nebula(vec3 d) {
    vec3 bandN = normalize(vec3(0.62, 0.70, 0.35));
    float bd   = dot(d, bandN);
    float band = exp(-bd * bd * 9.0);
    vec3 c = vec3(0.0);
    if (uMilky > 0.0) {
        float n    = fbm(d * 3.0 + 2.0, 5);
        float n2   = fbm(d * 7.0 + vec3(4.1, 1.7, 9.3), 4);
        float dust = smoothstep(0.45, 0.75, n2);
        c += uMilky * vec3(0.42, 0.36, 0.75) * band * (0.03 + 0.17 * n * n) * (1.0 - 0.85 * dust * band);
        c += uMilky * vec3(0.95, 0.60, 0.38) * pow(band, 5.0) * 0.14 * n * (1.0 - 0.6 * dust);
    }
    if (uNebula > 0.0) {
        float e  = fbm(d * 2.2 + vec3(9.1, 3.3, 1.1), 4);
        c += uNebula * vec3(0.80, 0.14, 0.35) * pow(smoothstep(0.55, 0.85, e), 2.0) * 0.20 * (0.3 + band);
        float e2 = fbm(d * 2.6 + vec3(1.3, 7.7, 5.5), 4);
        c += uNebula * vec3(0.10, 0.42, 0.75) * pow(smoothstep(0.58, 0.88, e2), 2.0) * 0.14;
    }
    return c;
}

vec3 stars(vec3 d, float fp) {
    vec3 a = abs(d);
    vec2 uv; float face;
    if (a.x >= a.y && a.x >= a.z) { uv = d.yz / a.x; face = d.x > 0.0 ? 0.0 : 1.0; }
    else if (a.y >= a.z)          { uv = d.xz / a.y; face = d.y > 0.0 ? 2.0 : 3.0; }
    else                          { uv = d.xy / a.z; face = d.z > 0.0 ? 4.0 : 5.0; }

    vec3 col = vec3(0.0);
    for (int L = 0; L < 3; L++) {
        float K    = L == 0 ? 170.0 : (L == 1 ? 60.0 : 20.0);
        float dens = L == 0 ? 0.30  : (L == 1 ? 0.22 : 0.20);
        float bri  = L == 0 ? 0.5   : (L == 1 ? 2.0  : 9.0);
        vec2 g  = uv * K;
        vec2 gi = floor(g);
        for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            vec2 cell = gi + vec2(i, j);
            vec3 h = hash33(vec3(cell, face * 17.0 + float(L) * 131.0));
            if (h.x > dens) continue;
            vec2 sp    = cell + 0.5 + (h.yz - 0.5) * 0.7;
            float dist = length(g - sp) / K;
            float s0   = 0.0007;
            float s    = max(s0, fp * 0.75);
            float rb   = hash13(vec3(cell, face + float(L) * 7.0 + 3.3));
            float b    = bri * (0.04 + pow(rb, 7.0)) * (s0 * s0) / (s * s);
            float tk   = hash13(vec3(cell.yx, face + 51.0 + float(L)));
            col += blackbody(2800.0 + 11000.0 * tk * tk) * b * exp(-dist * dist / (2.0 * s * s));
        }
    }
    return col;
}

vec3 galaxy(vec3 d) {
    vec3 G = normalize(vec3(0.30, -0.20, -0.93));
    if (dot(d, G) < 0.9) return vec3(0.0);
    vec3 e1 = normalize(cross(G, vec3(0.0, 1.0, 0.0)));
    vec3 e2 = cross(e1, G);
    vec2 p  = vec2(dot(d, e1), dot(d, e2)) / 0.07;
    float ca = cos(0.6), sa = sin(0.6);
    p = mat2(ca, -sa, sa, ca) * p;
    p.y /= 0.45;
    float r  = length(p);
    float th = atan(p.y, p.x);
    float arms = pow(0.5 + 0.5 * cos(2.0 * th - 6.0 * log(r + 0.05)), 3.0);
    float disk = exp(-r * 3.5);
    float core = exp(-r * r * 40.0);
    float n    = fbm(vec3(p * 5.0, 1.7), 4);
    vec3 col = vec3(0.50, 0.65, 1.00) * disk * arms * (0.4 + n) * 1.4
             + vec3(1.00, 0.82, 0.60) * (core * 3.0 + disk * 0.25);
    return col * 0.7;
}

vec3 sky(vec3 d, float fp) {
    vec3 c = nebula(d);
    if (uStars  > 0.0) c += uStars  * stars(d, fp);
    if (uGalaxy > 0.0) c += uGalaxy * galaxy(d);
    return c;
}
)GLSL";

static const char* FS_SCENE_C = R"GLSL(
float diskTex(float r, float ang, float t) {
    float omega = 2.6 * uSwirl * pow(r, -1.5);
    const float P = 10.0;
    float lr  = log(r);
    float acc = 0.0;
    for (int k = 0; k < 2; k++) {
        float tk = mod(t + float(k) * P * 0.5, P);
        float w  = sin(PI * tk / P); w *= w;
        float ph = ang - omega * tk;
        vec3 q = vec3(cos(ph) * 1.6, sin(ph) * 1.6, lr * 14.0) + float(k) * vec3(5.2, 1.3, 7.7);
        float n = fbm(q * 1.3, 5);
        float streak = 0.5 + 0.5 * sin(lr * 60.0 + n * 6.0);
        acc += w * (smoothstep(0.32, 0.78, n) * (0.65 + 0.35 * streak));
    }
    return 0.12 + 1.35 * acc;
}

// frequency seen at infinity / emitted, for gas on a circular orbit (Schwarzschild, ray direction known)
float schwShift(vec3 hit, float r, vec3 rayDir) {
    vec3  tang  = vec3(-hit.z, 0.0, hit.x) / r;
    float beta  = sqrt(0.5 / r);
    float gamma = inversesqrt(1.0 - beta * beta);
    float cosT  = -dot(tang, rayDir);
    float D     = mix(1.0, 1.0 / (gamma * (1.0 - beta * cosT)), uDoppler);
    float g     = mix(1.0, sqrt(max(1.0 - RS / r, 0.0)), uRedshift);
    return D * g;
}

// s = frequency seen by the camera / emitted
void shadeDisk(vec3 hit, float r, float s, inout vec3 col, inout float T) {
    float ang  = atan(hit.z, hit.x);
    float tex  = diskTex(r, ang, uTime);
    float x    = uDiskIn / r;
    float edge = smoothstep(uDiskIn, uDiskIn + 0.45, r) *
                 smoothstep(uDiskOut, uDiskOut - 0.45 * (uDiskOut - uDiskIn), r);

    float temp = uTempPeak * 1.6 * pow(x, 0.75) * pow(max(1.0 - 0.97 * sqrt(x), 0.0), 0.25);

    float I = x * x * edge * tex * min(pow(s, 4.0), 25.0);
    col += T * blackbody(temp * s) * I * uDiskBright;
    T   *= 1.0 - clamp(pow(x, 1.1) * edge * tex * uDiskOpacity, 0.0, 0.95);
}

void trace(vec3 ro, vec3 rd, float qObs, out vec3 col, out float T, out vec3 dirOut, out float escaped) {
    vec3 p = ro, v = rd;
    vec3 L = cross(p, v);
    float k = 1.5 * dot(L, L) * uGravity;
    col = vec3(0.0); T = 1.0; escaped = 0.0; dirOut = rd;

    for (int i = 0; i < 1200; i++) {
        float r2 = dot(p, p);
        float r  = sqrt(r2);
        // traced backwards in time, a ray only dies falling into the horizon; from a camera inside the
        // horizon every ray starts out moving outward
        if (r < RS && dot(p, v) < 0.0) { dirOut = normalize(v); return; }
        if (r > ESCAPE_R && dot(p, v) > 0.0) { escaped = 1.0; dirOut = normalize(v); return; }

        float dt = clamp(uStep * r, 0.0003, 2.5) / length(v);
        vec3 pp = p;
        v += (-k * p / (r2 * r2 * r)) * (0.5 * dt);
        p += v * dt;
        r2 = dot(p, p); r = sqrt(r2);
        v += (-k * p / (r2 * r2 * r)) * (0.5 * dt);

        col += T * vec3(1.0, 0.55, 0.25) * (uGlow * dt * length(v) / max(r2 * r2, 1.0));

        if (uDiskOn > 0.5 && (pp.y > 0.0) != (p.y > 0.0)) {
            float f  = pp.y / (pp.y - p.y);
            vec3 hit = mix(pp, p, f);
            float hr = length(hit);
            if (hr > uDiskIn && hr < uDiskOut) {
                shadeDisk(hit, hr, schwShift(hit, hr, normalize(v)) * qObs, col, T);
                if (T < 0.01) { dirOut = normalize(v); return; }
            }
        }
    }
    dirOut = normalize(v);
}

// ------------------------------------------------------------------ Kerr (spinning) black hole
// Null geodesics in Cartesian Kerr-Schild coordinates: g = eta + f l l, with l null and |l| = 1.
// These coordinates go smoothly through the horizon. Mass M = r_s / 2; "std" axes put the spin on +z,
// world y is up, so std = (x, z, -y) and the disk (world y = 0) orbits counter-clockwise around +z.
// The ray is the real (future directed) photon, integrated backwards in its affine parameter with RK4,
// with photon energy at infinity fixed to 1 (p_t = -1). H = (-1 + p.p - f (1 + l.p)^2) / 2.
float KM, KA;     // mass and spin a = chi M

vec3 toStd(vec3 v)   { return vec3(v.x, v.z, -v.y); }
vec3 fromStd(vec3 v) { return vec3(v.x, -v.z, v.y); }

// Boyer-Lindquist radius, f and l at x
float ksField(vec3 x, out float f, out vec3 l) {
    float a2 = KA * KA, b = dot(x, x) - a2;
    float r2 = max(0.5 * (b + sqrt(b * b + 4.0 * a2 * x.z * x.z)), 1e-10), r = sqrt(r2);
    float w = r2 + a2;
    l = vec3((r * x.x + KA * x.y) / w, (r * x.y - KA * x.x) / w, x.z / r);
    f = 2.0 * KM * r2 * r / (r2 * r2 + a2 * x.z * x.z);
    return r;
}

// Hamilton's equations: dx = dH/dp, dp = -dH/dx. Also returns r and grad r.
void ksDeriv(vec3 x, vec3 p, out vec3 dx, out vec3 dp, out float r, out vec3 gr) {
    float a = KA, a2 = a * a, b = dot(x, x) - a2;
    float sq = sqrt(b * b + 4.0 * a2 * x.z * x.z);
    float r2 = max(0.5 * (b + sq), 1e-10);
    r  = sqrt(r2);
    gr = (r2 * x + vec3(0.0, 0.0, a2 * x.z)) / (r * max(sq, 1e-10));     // from r^4 - b r^2 - a^2 z^2 = 0
    float w  = r2 + a2;
    float xp = x.x * p.x + x.y * p.y;
    float Q  = r * xp + a * (x.y * p.x - x.x * p.y);
    float S  = Q / w + x.z * p.z / r;                                       // l.p
    vec3  l  = vec3((r * x.x + a * x.y) / w, (r * x.y - a * x.x) / w, x.z / r);
    float D  = r2 * r2 + a2 * x.z * x.z;
    float f  = 2.0 * KM * r2 * r / D;
    vec3  gf = 2.0 * KM * (3.0 * r2 * gr * D - r2 * r * (4.0 * r2 * r * gr + vec3(0.0, 0.0, 2.0 * a2 * x.z))) / (D * D);
    vec3  gQ = gr * xp + r * vec3(p.x, p.y, 0.0) + a * vec3(-p.y, p.x, 0.0);
    vec3  gS = (gQ * w - Q * 2.0 * r * gr) / (w * w) + vec3(0.0, 0.0, p.z) / r - x.z * p.z * gr / r2;
    float c  = 1.0 + S;
    dx = p - f * c * l;
    dp = 0.5 * gf * c * c + f * c * gS;
}

// frequency seen at infinity / emitted for prograde circular orbits in the equatorial plane
float kerrDiskShift(float r, float Lz) {
    float sM    = sqrt(KM);
    float Omega = sM / (r * sqrt(r) + KA * sM);
    float gtt = -(1.0 - 2.0 * KM / r), gtp = -2.0 * KM * KA / r, gpp = r * r + KA * KA + 2.0 * KM * KA * KA / r;
    float ut  = inversesqrt(max(-(gtt + 2.0 * Omega * gtp + Omega * Omega * gpp), 1e-8));
    float gTot  = 1.0 / (ut * (1.0 - Omega * Lz));
    float gGrav = sqrt(max(1.0 - 2.0 * KM / r, 1e-4));                    // split only for the UI sliders
    return mix(1.0, gTot / gGrav, uDoppler) * mix(1.0, gGrav, uRedshift);
}

void traceKerr(vec3 x, vec3 p, float qObs, out vec3 col, out float T, out vec3 dirOut, out float escaped) {
    float rH = KM + sqrt(max(KM * KM - KA * KA, 0.0));                     // outer horizon
    float Lz = x.x * p.y - x.y * p.x;                                       // conserved
    col = vec3(0.0); T = 1.0; escaped = 0.0; dirOut = fromStd(normalize(p));
    int stuck = 0;

    for (int i = 0; i < 1500; i++) {
        vec3 dx1, dp1, gr; float r;
        ksDeriv(x, p, dx1, dp1, r, gr);
        float R  = length(x);
        vec3 back = -dx1;                                                    // where the light came from
        if (R > ESCAPE_R && dot(x, back) > 0.0) { escaped = 1.0; dirOut = fromStd(normalize(back)); return; }
        // falling (back in time) onto the horizon from outside, it only ever creeps towards it: captured.
        // From inside, light that hugs the horizon came out of the past horizon: nothing to see either.
        if (r > rH && r < rH * 1.02 && dot(gr, back) < 0.0) return;
        stuck = (r < rH && r > rH * 0.95) ? stuck + 1 : 0;
        if (stuck > 60 || r < 0.01 * KM) return;

        float h = clamp(uStep * 2.0 * R, 0.0003, 3.0) / length(dx1);
        vec3 dx2, dp2, dx3, dp3, dx4, dp4; vec3 g2; float r2_;
        ksDeriv(x - 0.5 * h * dx1, p - 0.5 * h * dp1, dx2, dp2, r2_, g2);
        ksDeriv(x - 0.5 * h * dx2, p - 0.5 * h * dp2, dx3, dp3, r2_, g2);
        ksDeriv(x - h * dx3,       p - h * dp3,       dx4, dp4, r2_, g2);
        vec3 xn = x - h / 6.0 * (dx1 + 2.0 * dx2 + 2.0 * dx3 + dx4);
        vec3 pn = p - h / 6.0 * (dp1 + 2.0 * dp2 + 2.0 * dp3 + dp4);

        float stepLen = length(xn - x);
        if (r > rH) col += T * vec3(1.0, 0.55, 0.25) * (uGlow * stepLen / max(r * r * r * r * 16.0 * KM * KM * KM * KM, 1.0));

        if (uDiskOn > 0.5 && (x.z > 0.0) != (xn.z > 0.0)) {
            vec3 hit = mix(x, xn, x.z / (x.z - xn.z));
            float fh; vec3 lh;
            float hr = ksField(hit, fh, lh);
            if (hr > uDiskIn && hr < uDiskOut) {
                shadeDisk(fromStd(hit), hr, kerrDiskShift(hr, Lz) * qObs, col, T);
                if (T < 0.01) { dirOut = fromStd(normalize(-dx1)); return; }
            }
        }
        x = xn; p = pn;
    }
    dirOut = fromStd(normalize(-p));
}

// colour change of a white source seen with frequency ratio q
vec3 shiftTint(float q) { return blackbody(6500.0 * q) / blackbody(6500.0); }

// Kerr camera. The local reference observer is the Kerr-Schild "normal" observer (at rest in the t = const
// slice), which exists at every radius. In its frame, space is stretched by sqrt(1 + f) along l.
// Relative to it, a hovering observer moves at speed f along l, and one falling from rest at infinity at
// -sqrt(f) / (1 + sqrt(f) + f) (exact for no spin). uFall blends the two.
void kerrMain(vec3 rd, out vec3 col, out vec3 dir, out float q) {
    KM = 0.5 * max(uGravity, 0.0);
    KA = uSpin * KM;
    vec3 X = toStd(uCamPos);
    float f; vec3 l;
    ksField(X, f, l);
    float sf = sqrt(f);
    float s  = clamp((1.0 - uFall) * f - uFall * sf / (1.0 + sf + f), -0.95, 0.95);
    vec3  V  = s * l;

    vec3  n   = toStd(-rd);
    float vv  = s * s;
    float gam = inversesqrt(1.0 - vv);
    float vn  = dot(V, n);
    vec3  nf  = normalize((n + V * ((gam - 1.0) * vn / max(vv, 1e-12) + gam)) / (gam * (1.0 + vn)));

    float nl  = dot(nf, l);
    float den = 1.0 - f * nl;                       // > 0 for light that can have come from our universe
    float E   = sqrt(1.0 + f) / max(den, 1e-4);     // energy seen by the normal observer / at infinity
    vec3  p   = E * (nf + nl * l * (sqrt(1.0 + f) - 1.0));
    q = clamp(pow(E / (gam * (1.0 + vn)), uObsShift), 0.02, 50.0);

    col = vec3(0.0); dir = rd;
    float T = 0.0, esc = 0.0;
    if (den > 1e-4) traceKerr(X, p, q, col, T, dir, esc);
    float fp = clamp(length(fwidth(dir)), 2.0 * uFov / uRes.y, 0.05);
    if (esc > 0.5) col += T * sky(dir, fp) * shiftTint(q) * min(q * q * q * q, 40.0);
}

void main() {
    vec2 uv = (2.0 * (gl_FragCoord.xy + uJitter) - uRes) / uRes.y;
    vec3 rd = normalize(uCamFwd + uCamRight * (uv.x * uFov) + uCamUp * (uv.y * uFov));
    if (uSpin > 0.0) {
        vec3 col, dir; float q;
        kerrMain(rd, col, dir, q);
        fragColor = vec4(col, 1.0);
        return;
    }

    // 1) camera frame -> local free-fall frame: relativistic aberration by the camera velocity
    vec3  n   = -rd;                                    // direction the photon travels
    float vv  = dot(uCamVel, uCamVel);
    float gam = inversesqrt(1.0 - vv);
    float vn  = dot(uCamVel, n);
    vec3  nf  = (n + uCamVel * ((gam - 1.0) * vn / max(vv, 1e-12) + gam)) / (gam * (1.0 + vn));
    vec3  rdf = -normalize(nf);

    // 2) free-fall frame -> spatial path. Free-falling observers see flat space (Painleve-Gullstrand
    //    coordinates), the photon's coordinate velocity is its local direction minus the fall velocity
    //    beta = sqrt(r_s / r). This is exact at every radius, inside the horizon too.
    float rc   = length(uCamPos);
    vec3  rhat = uCamPos / rc;
    float beta = sqrt(max(uGravity, 0.0) * RS / rc);
    float eInf = 1.0 + beta * dot(rdf, rhat);           // photon energy at infinity / energy seen here
    float q    = 1.0 / (gam * (1.0 + vn) * max(eInf, 1e-4));
    q = clamp(pow(q, uObsShift), 0.02, 50.0);           // frequency seen by the camera / at infinity

    vec3 col = vec3(0.0), dir = rd; float T = 0.0, esc = 0.0;
    // eInf <= 0 only happens inside the horizon: such light never came from our universe
    if (eInf > 1e-4) trace(uCamPos, normalize(rdf + beta * rhat), q, col, T, dir, esc);
    float fp = clamp(length(fwidth(dir)), 2.0 * uFov / uRes.y, 0.05);
    if (esc > 0.5) col += T * sky(dir, fp) * shiftTint(q) * min(q * q * q * q, 40.0);
    fragColor = vec4(col, 1.0);
}
)GLSL";

static const char* FS_DOWN = R"GLSL(
#version 330 core
out vec4 o;
uniform sampler2D uSrc;
uniform vec2 uSrcTexel;
uniform vec2 uDstRes;
vec3 S(vec2 uv) { return min(texture(uSrc, uv).rgb, vec3(60.0)); }
void main() {
    vec2 uv = gl_FragCoord.xy / uDstRes;
    vec2 t  = uSrcTexel;
    vec3 a = S(uv + t * vec2(-2,  2)), b = S(uv + t * vec2(0,  2)), c = S(uv + t * vec2(2,  2));
    vec3 d = S(uv + t * vec2(-2,  0)), e = S(uv),                    f = S(uv + t * vec2(2,  0));
    vec3 g = S(uv + t * vec2(-2, -2)), h = S(uv + t * vec2(0, -2)), i = S(uv + t * vec2(2, -2));
    vec3 j = S(uv + t * vec2(-1,  1)), k = S(uv + t * vec2(1,  1));
    vec3 l = S(uv + t * vec2(-1, -1)), m = S(uv + t * vec2(1, -1));
    vec3 r = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
    o = vec4(max(r, vec3(0.0)), 1.0);
}
)GLSL";

static const char* FS_UP = R"GLSL(
#version 330 core
out vec4 o;
uniform sampler2D uSrc;
uniform vec2 uSrcTexel;
uniform vec2 uDstRes;
void main() {
    vec2 uv = gl_FragCoord.xy / uDstRes;
    vec2 t  = uSrcTexel;
    vec3 r = texture(uSrc, uv).rgb * 4.0;
    r += (texture(uSrc, uv + t * vec2(-1, 0)).rgb + texture(uSrc, uv + t * vec2(1, 0)).rgb +
          texture(uSrc, uv + t * vec2(0, -1)).rgb + texture(uSrc, uv + t * vec2(0, 1)).rgb) * 2.0;
    r += (texture(uSrc, uv + t * vec2(-1, -1)).rgb + texture(uSrc, uv + t * vec2(1, -1)).rgb +
          texture(uSrc, uv + t * vec2(-1, 1)).rgb  + texture(uSrc, uv + t * vec2(1, 1)).rgb);
    o = vec4(r / 16.0, 1.0);
}
)GLSL";

static const char* FS_COMPOSITE = R"GLSL(
#version 330 core
out vec4 o;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform vec2  uRes;
uniform float uTime;
uniform float uBloomStr;
uniform float uExposure;
uniform float uVignette;
uniform float uCA;
uniform float uGrain;
uniform float uFade;
uniform sampler2D uOverlay;   // titles / captions / HUD, brightness = coverage, rows top-down
uniform float uOverlayOn;
uniform float uBar;           // letterbox bar height in pixels

vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
float hash(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }

void main() {
    vec2 uv  = gl_FragCoord.xy / uRes;
    vec2 cc  = uv - 0.5;
    vec2 off = cc * dot(cc, cc) * uCA;
    vec3 hdr = vec3(texture(uScene, uv + off).r, texture(uScene, uv).g, texture(uScene, uv - off).b);
    vec3 blm = texture(uBloom, uv).rgb;
    vec3 c   = mix(hdr, blm, uBloomStr) + blm * uBloomStr * 0.25;
    c = aces(c * uExposure);
    c *= 1.0 - uVignette * pow(length(cc * vec2(1.0, 1.15)) * 1.25, 2.6);
    c *= uFade;
    c = pow(c, vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + fract(uTime) * 917.0) - 0.5) * (uGrain / 255.0);
    if (gl_FragCoord.y < uBar || gl_FragCoord.y > uRes.y - uBar) c = vec3(0.0);
    if (uOverlayOn > 0.5) {
        vec2 ov = vec2(uv.x, 1.0 - uv.y);
        vec2 px = vec2(uRes.y / 1080.0) / uRes;
        float sh = 0.0;
        for (int i = 1; i <= 4; i++) sh += texture(uOverlay, ov + px * vec2(-1.2, 1.2) * float(i)).g;
        c *= 1.0 - 0.6 * clamp(sh * 0.35, 0.0, 1.0);
        c = mix(c, vec3(0.97, 0.94, 0.88), texture(uOverlay, ov).g);
    }
    o = vec4(c, 1.0);
}
)GLSL";

// copy / scale / letterbox blit
static const char* FS_COPY = R"GLSL(
#version 330 core
out vec4 o;
uniform sampler2D uSrc;
uniform vec2  uOffset;
uniform vec2  uRes;
uniform float uScale;
void main() {
    vec2 uv = (gl_FragCoord.xy - uOffset) / uRes;
    o = vec4(texture(uSrc, uv).rgb * uScale, 1.0);
}
)GLSL";

// ============================================================================= GL helpers
static void fatal(const char* title, const std::string& msg) {
    MessageBoxA(nullptr, msg.c_str(), title, MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

static GLuint compileShader(GLenum type, std::vector<const char*> src, const char* name) {
    GLuint s = p_glCreateShader(type);
    p_glShaderSource(s, (GLsizei)src.size(), src.data(), nullptr);
    p_glCompileShader(s);
    GLint ok = 0; p_glGetShaderiv(s, K_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[8192]; p_glGetShaderInfoLog(s, sizeof log, nullptr, log);
        fatal("Shader compile error", std::string(name) + ":\n" + log);
    }
    return s;
}

static GLuint makeProgram(std::vector<const char*> fs, const char* name) {
    GLuint v = compileShader(K_VERTEX_SHADER, {VS_FULLSCREEN}, "fullscreen.vert");
    GLuint f = compileShader(K_FRAGMENT_SHADER, fs, name);
    GLuint p = p_glCreateProgram();
    p_glAttachShader(p, v); p_glAttachShader(p, f);
    p_glLinkProgram(p);
    GLint ok = 0; p_glGetProgramiv(p, K_LINK_STATUS, &ok);
    if (!ok) {
        char log[8192]; p_glGetProgramInfoLog(p, sizeof log, nullptr, log);
        fatal("Shader link error", std::string(name) + ":\n" + log);
    }
    p_glDeleteShader(v); p_glDeleteShader(f);
    return p;
}

struct Target { GLuint tex = 0, fbo = 0; int w = 0, h = 0; };

static Target makeTarget(int w, int h, bool hdr = true) {
    Target t; t.w = w; t.h = h;
    glGenTextures(1, &t.tex);
    glBindTexture(GL_TEXTURE_2D, t.tex);
    if (hdr) glTexImage2D(GL_TEXTURE_2D, 0, K_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    else     glTexImage2D(GL_TEXTURE_2D, 0, K_RGBA8,   w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, K_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, K_CLAMP_TO_EDGE);
    p_glGenFramebuffers(1, &t.fbo);
    p_glBindFramebuffer(K_FRAMEBUFFER, t.fbo);
    p_glFramebufferTexture2D(K_FRAMEBUFFER, K_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);
    if (p_glCheckFramebufferStatus(K_FRAMEBUFFER) != K_FRAMEBUFFER_COMPLETE)
        fatal("OpenGL", "Framebuffer incomplete (out of GPU memory or RGBA16F unsupported).");
    p_glBindFramebuffer(K_FRAMEBUFFER, 0);
    return t;
}
static void freeTarget(Target& t) {
    if (t.fbo) p_glDeleteFramebuffers(1, &t.fbo);
    if (t.tex) glDeleteTextures(1, &t.tex);
    t = Target{};
}
static void buildMips(std::vector<Target>& mips, int w, int h) {
    for (auto& m : mips) freeTarget(m);
    mips.clear();
    w /= 2; h /= 2;
    for (int i = 0; i < 7 && w >= 4 && h >= 4; i++) { mips.push_back(makeTarget(w, h)); w /= 2; h /= 2; }
}

static void bindTex(int unit, GLuint tex) { p_glActiveTexture(K_TEXTURE0 + unit); glBindTexture(GL_TEXTURE_2D, tex); }
static void u1f(GLuint p, const char* n, float x)                   { p_glUniform1f(p_glGetUniformLocation(p, n), x); }
static void u2f(GLuint p, const char* n, float x, float y)          { p_glUniform2f(p_glGetUniformLocation(p, n), x, y); }
static void u3f(GLuint p, const char* n, float x, float y, float z) { p_glUniform3f(p_glGetUniformLocation(p, n), x, y, z); }
static void u1i(GLuint p, const char* n, int x)                     { p_glUniform1i(p_glGetUniformLocation(p, n), x); }

// ============================================================================= math
static const float PI = 3.14159265f;
struct V3 { float x, y, z; };
static V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static float len(V3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
static V3 norm(V3 a) { return a * (1.0f / len(a)); }
static float smoothstepf(float e0, float e1, float x) {
    float t = std::min(1.0f, std::max(0.0f, (x - e0) / (e1 - e0)));
    return t * t * (3.0f - 2.0f * t);
}
static float halton(int i, int b) {
    float f = 1.0f, r = 0.0f;
    while (i > 0) { f /= b; r += f * (i % b); i /= b; }
    return r;
}

// camera: spherical position around the hole, looking at it, turned by yaw/pitch.
// fall: 0 = hovering in place (rockets on), 1 = free fall from rest at infinity.
struct CamState {
    float phi = 0.0f, el = 0.19f, dist = 24.0f, roll = 0.0f, fov = 0.40f;   // fov = tan(half vertical FOV)
    float yaw = 0.0f, pitch = 0.0f, fall = 0.0f;
    static const int N = 8;
    float& operator[](int i) { return (&phi)[i]; }
    float  operator[](int i) const { return (&phi)[i]; }
};
struct CamFrame { V3 pos, fwd, right, up; float fov, fall; };

static const float MIN_DIST = 0.03f;

static CamFrame camBasis(const CamState& c) {
    CamFrame f;
    float el = std::max(-1.55f, std::min(1.55f, c.el));
    float d  = std::max(MIN_DIST, c.dist);
    f.pos = {d * std::cos(el) * std::cos(c.phi), d * std::sin(el), d * std::cos(el) * std::sin(c.phi)};
    V3 fwd = norm(f.pos * -1.0f);
    V3 r = norm(cross(fwd, V3{0, 1, 0}));
    V3 u = cross(r, fwd);
    V3 f1 = fwd * std::cos(c.yaw) + r * std::sin(c.yaw);           // yaw turns right
    V3 r1 = r * std::cos(c.yaw) - fwd * std::sin(c.yaw);
    f.fwd = f1 * std::cos(c.pitch) + u * std::sin(c.pitch);        // pitch tilts up
    V3 u2 = u * std::cos(c.pitch) - f1 * std::sin(c.pitch);
    f.right = r1 * std::cos(c.roll) + u2 * std::sin(c.roll);
    f.up    = u2 * std::cos(c.roll) + r1 * -std::sin(c.roll);
    f.fov   = c.fov;
    f.fall  = std::max(0.0f, std::min(1.0f, c.fall));
    return f;
}

// ============================================================================= app state
struct Params {
    float renderScale = 1.0f; bool ultra = false;
    float gravity = 1.0f, doppler = 1.0f, redshift = 1.0f;
    bool  disk = true;
    float diskIn = 3.0f, diskOut = 16.0f, tempPeak = 8000.0f, diskBright = 1.25f, diskOpacity = 1.8f;
    float swirl = 1.0f, glow = 0.005f, obsShift = 1.0f, autoExposure = 1.0f;
    float spin = 0.6f; bool iscoEdge = true;            // a/M; Interstellar's renders used 0.6
    float stars = 1.0f, milky = 1.0f, nebula = 1.0f, galaxy = 1.0f;
    float exposure = 1.0f, bloom = 0.06f, vignette = 0.45f, ca = 0.010f, grain = 2.5f;
} P;

struct Key { float t; CamState c; };
static std::vector<Key> g_keys;
static float g_playhead = 0.0f, g_playSpeed = 1.0f, g_fadeIn = 2.0f, g_fadeOut = 3.0f;
static float g_blackAt = 0.0f;      // picture fully black from here on (0 = end of sequence)
static bool  g_playing = true, g_loop = true;

// on-screen text, timed with the sequence
enum { CUE_TITLE = 0, CUE_SUBTITLE, CUE_CAPTION, CUE_CHAPTER, CUE_HUD, CUE_STYLES };
struct Cue { float t0, t1; int style; std::string text; };
static std::vector<Cue> g_cues;
static bool  g_cuesOn = true, g_letterboxOn = true;
static float g_letterbox = 2.39f;
static float g_massSun = 1.0e8f;    // Gargantua, Kip Thorne's figure for the film

enum { CAM_ORBIT = 0, CAM_SEQUENCE = 1 };
static int      g_camMode = CAM_SEQUENCE;
static CamState g_orbit;
static bool     g_autoRotate = true, g_drift = true;
static float    g_rotSpeed = 0.11f;
static float    g_simTime = 0.0f;
static bool     g_paused = false;

static HWND  g_hwnd;
static int   g_w = 1600, g_h = 900;
static bool  g_running = true, g_fullscreen = false, g_dragging = false, g_showUI = true;
static int   g_lastMX = 0, g_lastMY = 0;
static float g_dpi = 1.0f;
static WINDOWPLACEMENT g_wp = {sizeof(WINDOWPLACEMENT)};

// ---------------------------------------------------------------- sequence
static void defaultSequence() {
    // the galaxy sits in direction G; parking the camera at -G puts it right behind the hole
    const float galPhi = 1.8828f, galEl = 0.2019f;
    //        t      phi     el      dist    roll    fov    yaw    pitch  fall
    g_keys = {
        {  0.0f, {-0.95f,  0.04f, 260.0f,  0.00f, 0.30f,  0.0f,  0.00f, 0.0f}},  // I. a dot bending the stars
        { 12.0f, {-0.65f,  0.07f, 110.0f,  0.03f, 0.33f,  0.0f,  0.00f, 0.0f}},
        { 24.0f, {-0.20f,  0.09f,  40.0f, -0.05f, 0.40f,  0.0f,  0.00f, 0.0f}},
        { 34.0f, { 0.40f,  0.05f,  27.0f, -0.12f, 0.42f,  0.0f,  0.00f, 0.0f}},  // edge-on, the classic shot
        { 44.0f, { 0.95f,  0.42f,  25.0f, -0.05f, 0.44f,  0.0f,  0.00f, 0.0f}},  // II. rising
        { 54.0f, { 1.40f,  1.28f,  23.0f,  0.00f, 0.48f,  0.0f,  0.00f, 0.0f}},  // top-down whirlpool
        { 62.0f, { 1.70f,  0.62f,  30.0f,  0.04f, 0.42f,  0.0f,  0.00f, 0.0f}},
        { 70.0f, {galPhi, galEl,   42.0f,  0.00f, 0.30f,  0.0f,  0.00f, 0.0f}},  // III. Einstein ring
        { 78.0f, {galPhi + 0.04f, galEl + 0.01f, 40.0f, 0.00f, 0.30f, 0.0f, 0.00f, 0.0f}},
        { 88.0f, { 2.55f,  0.20f,  15.0f, -0.12f, 0.52f, -0.30f,  0.00f, 0.0f}},  // IV. down to the disk
        { 98.0f, { 3.40f,  0.11f,   9.0f, -0.22f, 0.62f, -0.80f, -0.04f, 0.0f}},  // skimming, looking ahead
        {108.0f, { 4.35f,  0.10f,   7.5f, -0.18f, 0.62f, -0.65f, -0.03f, 0.0f}},
        {118.0f, { 5.10f,  0.16f,   6.5f, -0.06f, 0.55f, -0.20f,  0.00f, 0.0f}},  // turning to face it
        {126.0f, { 5.40f,  0.24f,   5.2f,  0.00f, 0.50f,  0.00f,  0.00f, 1.0f}},  // V. let go: free fall
        {136.0f, { 5.65f,  0.30f,   3.0f,  0.04f, 0.52f,  0.00f,  0.00f, 1.0f}},  // past the inner edge
        {146.0f, { 5.85f,  0.34f,   1.6f,  0.08f, 0.58f,  0.30f,  0.05f, 1.0f}},  // photon sphere
        {154.0f, { 5.95f,  0.36f,   1.0f,  0.10f, 0.62f,  0.65f,  0.08f, 1.0f}},  // event horizon
        {160.0f, { 6.00f,  0.37f,   0.75f, 0.08f, 0.65f,  1.05f,  0.10f, 1.0f}},
        {168.0f, { 6.03f,  0.38f,   0.50f, 0.02f, 0.70f,  1.65f,  0.10f, 1.0f}},  // turn to the band of sky
        {175.0f, { 6.05f,  0.38f,   0.32f, 0.00f, 0.75f,  1.75f,  0.05f, 1.0f}},  // nearing the inner horizon
        {186.0f, { 6.05f,  0.38f,   0.32f, 0.00f, 0.75f,  1.75f,  0.05f, 1.0f}},  // end card on black
    };
    g_cues = {
        {  2.5f,  11.0f, CUE_TITLE,    "GARGANTUA"},
        {  4.5f,  11.0f, CUE_SUBTITLE, "a journey past the point of no return"},
        { 13.0f,  20.5f, CUE_CAPTION,  "A black hole of 100 million suns. Its event horizon is wider than the orbit of the Earth."},
        { 24.0f,  31.0f, CUE_CHAPTER,  "I   ·   THE SHADOW"},
        { 27.0f,  34.5f, CUE_CAPTION,  "The glow above and below the shadow is the far side of the disk, its light bent around the hole by gravity."},
        { 37.0f,  44.5f, CUE_CAPTION,  "One side of the disk rushes toward us at nearly half the speed of light — brighter and bluer. The other side recedes."},
        { 46.0f,  53.0f, CUE_CHAPTER,  "II   ·   THE WHIRLPOOL"},
        { 48.5f,  55.5f, CUE_CAPTION,  "From above: gas spiralling inward, heated by friction until it glows."},
        { 56.0f,  62.5f, CUE_CAPTION,  "Gargantua spins. It drags space around with it, so its shadow is lopsided and the gas can orbit closer in."},
        { 63.0f,  70.0f, CUE_CHAPTER,  "III   ·   THE RING"},
        { 66.0f,  76.0f, CUE_CAPTION,  "A galaxy sits exactly behind the black hole. Gravity bends its light into a perfect circle — an Einstein ring."},
        { 81.0f,  88.0f, CUE_CHAPTER,  "IV   ·   THE DISK"},
        { 84.0f, 174.5f, CUE_HUD,      ""},
        { 90.0f,  97.0f, CUE_CAPTION,  "Skimming the disk. The gas below circles the hole once every few days, faster the closer it gets."},
        {104.0f, 112.0f, CUE_CAPTION,  "The inner edge of the disk. The spin lets gas orbit closer than it could around a still black hole, but no closer than this."},
        {119.0f, 126.0f, CUE_CHAPTER,  "V   ·   THE FALL"},
        {121.0f, 128.5f, CUE_CAPTION,  "We stop fighting gravity and let go. From here on, we are in free fall."},
        {131.0f, 138.0f, CUE_CAPTION,  "Our own speed squeezes the view ahead and makes it brighter. Light from behind us turns red."},
        {140.5f, 147.5f, CUE_CAPTION,  "Here light itself can orbit the hole: closer on the side turning with it, farther on the side turning against it."},
        {150.5f, 154.5f, CUE_CAPTION,  "The ergosphere. Space is dragged around so fast that nothing here can stand still."},
        {155.0f, 160.5f, CUE_CAPTION,  "The event horizon. We cross it and feel nothing. For a black hole this large, the tides are gentle."},
        {161.0f, 166.5f, CUE_CAPTION,  "Inside a spinning black hole the way down leads to a second, inner horizon."},
        {167.5f, 174.0f, CUE_CAPTION,  "What lies beyond it, no theory can yet tell us."},
        {177.0f, 185.0f, CUE_TITLE,    "GARGANTUA"},
        {177.5f, 185.0f, CUE_SUBTITLE, "every pixel is a ray of light traced through curved spacetime"},
    };
    g_fadeIn = 3.0f; g_fadeOut = 0.25f; g_blackAt = 175.0f;
}
static float seqLength() { return g_keys.empty() ? 0.0f : g_keys.back().t; }

static CamState evalSequence(float t) {
    if (g_keys.empty()) return g_orbit;
    if (g_keys.size() == 1 || t <= g_keys.front().t) return g_keys.front().c;
    if (t >= g_keys.back().t) return g_keys.back().c;
    size_t k = 0;
    while (k + 1 < g_keys.size() && g_keys[k + 1].t <= t) k++;
    const Key& a = g_keys[k];
    const Key& b = g_keys[k + 1];
    float h = std::max(1e-4f, b.t - a.t), u = (t - a.t) / h;
    auto tangent = [&](size_t i, int c) -> float {
        if (i == 0 || i + 1 >= g_keys.size()) return 0.0f;      // ease in/out at the ends
        float dtk = g_keys[i + 1].t - g_keys[i - 1].t;
        return dtk > 1e-4f ? (g_keys[i + 1].c[c] - g_keys[i - 1].c[c]) / dtk : 0.0f;
    };
    float h00 = 2 * u * u * u - 3 * u * u + 1, h10 = u * u * u - 2 * u * u + u;
    float h01 = -2 * u * u * u + 3 * u * u,    h11 = u * u * u - u * u;
    CamState out;
    for (int c = 0; c < CamState::N; c++)
        out[c] = h00 * a.c[c] + h10 * h * tangent(k, c) + h01 * b.c[c] + h11 * h * tangent(k + 1, c);
    out.dist = std::max(MIN_DIST, out.dist);
    out.fov  = std::max(0.05f, out.fov);
    out.fall = std::max(0.0f, std::min(1.0f, out.fall));
    return out;
}
static float seqFade(float t) {
    float L = g_blackAt > 0 ? std::min(g_blackAt, seqLength()) : seqLength();
    float a = g_fadeIn  > 0 ? smoothstepf(0.0f, g_fadeIn, t) : 1.0f;
    float b = g_fadeOut > 0 ? smoothstepf(L, L - g_fadeOut, t) : (t <= L ? 1.0f : 0.0f);
    return a * b;
}
static void sortKeys() { std::sort(g_keys.begin(), g_keys.end(), [](const Key& a, const Key& b) { return a.t < b.t; }); }

static void addKey(float t, CamState c) {
    // unwrap azimuth so the camera takes the short way from the previous key
    for (auto& k : g_keys) if (k.t <= t) {
        while (c.phi - k.c.phi >  PI) c.phi -= 2 * PI;
        while (c.phi - k.c.phi < -PI) c.phi += 2 * PI;
    }
    for (auto& k : g_keys) if (std::fabs(k.t - t) < 1e-3f) { k.c = c; return; }
    g_keys.push_back({t, c});
    sortKeys();
}
static bool saveSequence(const char* path) {
    FILE* f = std::fopen(path, "w");
    if (!f) return false;
    std::fprintf(f, "# time phi elevation distance roll fov yaw pitch fall\n");
    for (auto& k : g_keys)
        std::fprintf(f, "%f %f %f %f %f %f %f %f %f\n", k.t, k.c.phi, k.c.el, k.c.dist, k.c.roll, k.c.fov, k.c.yaw, k.c.pitch, k.c.fall);
    std::fprintf(f, "fade %f %f %f\n", g_fadeIn, g_fadeOut, g_blackAt);
    std::fprintf(f, "# cue start end style(0 title, 1 subtitle, 2 caption, 3 chapter, 4 HUD) text\n");
    for (auto& c : g_cues) std::fprintf(f, "cue %f %f %d %s\n", c.t0, c.t1, c.style, c.text.c_str());
    std::fclose(f);
    return true;
}
static bool loadSequence(const char* path) {
    FILE* f = std::fopen(path, "r");
    if (!f) return false;
    std::vector<Key> keys;
    std::vector<Cue> cues;
    char line[1024];
    while (std::fgets(line, sizeof line, f)) {
        line[std::strcspn(line, "\r\n")] = 0;
        Key k; Cue c; int n = 0;
        if (std::sscanf(line, "cue %f %f %d %n", &c.t0, &c.t1, &c.style, &n) == 3 && n > 0) {
            c.style = std::max(0, std::min(CUE_STYLES - 1, c.style));
            c.text = line + n;
            cues.push_back(c);
        } else if (std::sscanf(line, "fade %f %f %f", &g_fadeIn, &g_fadeOut, &g_blackAt) == 3) {
        } else if (std::sscanf(line, "%f %f %f %f %f %f %f %f %f", &k.t, &k.c.phi, &k.c.el, &k.c.dist, &k.c.roll, &k.c.fov,
                               &k.c.yaw, &k.c.pitch, &k.c.fall) >= 6) {
            keys.push_back(k);                     // old 6-column files keep yaw = pitch = fall = 0
        }
    }
    std::fclose(f);
    if (keys.empty()) return false;
    g_keys = keys; sortKeys();
    g_cues = cues;
    return true;
}

static CamState orbitEffective() {
    CamState c = g_orbit;
    if (g_drift) {
        c.el   += 0.10f * std::sin(g_simTime * 0.19f);
        c.dist *= 1.0f + 0.12f * std::sin(g_simTime * 0.13f);
        c.roll += 0.15f * std::sin(g_simTime * 0.17f);
    }
    return c;
}
static CamState currentCam()   { return g_camMode == CAM_SEQUENCE ? evalSequence(g_playhead) : orbitEffective(); }
static float    currentTime()  { return g_camMode == CAM_SEQUENCE ? g_playhead : g_simTime; }
static float    currentFade()  { return g_camMode == CAM_SEQUENCE ? seqFade(g_playhead) : 1.0f; }

// switch to orbit mode keeping the exact current view (used when the user grabs the camera)
static void grabCamera() {
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

// camera velocity relative to the local free-falling observer, outward along r (units of c).
// A hovering camera moves outward at beta = sqrt(r_s / r) relative to the falling one; |v| < 0.95 keeps
// it a real observer, so near and inside the horizon the camera is forced into (almost) free fall.
static float camRadialVel(float dist, float fall) {
    float beta = std::sqrt(std::max(P.gravity, 0.0f) / std::max(MIN_DIST, dist));
    return std::min(0.95f, (1.0f - fall) * beta);
}

// ---- Kerr-Schild helpers, same conventions as the shader (M = r_s / 2, spin along world -y)
static V3 toStd(V3 v) { return {v.x, v.z, -v.y}; }
static float kerrM() { return 0.5f * std::max(P.gravity, 0.0f); }
static float ksField(V3 x, float& f, V3& l) {
    float M = kerrM(), a = P.spin * M, a2 = a * a, b = dot(x, x) - a2;
    float r2 = std::max(0.5f * (b + std::sqrt(b * b + 4.0f * a2 * x.z * x.z)), 1e-10f), r = std::sqrt(r2);
    float w = r2 + a2;
    l = {(r * x.x + a * x.y) / w, (r * x.y - a * x.x) / w, x.z / r};
    f = 2.0f * M * r2 * r / (r2 * r2 + a2 * x.z * x.z);
    return r;
}
// camera speed along l relative to the Kerr-Schild normal observer (see the shader's kerrMain)
static float kerrCamSpeed(float f, float fall) {
    float sf = std::sqrt(f);
    return std::max(-0.95f, std::min(0.95f, (1.0f - fall) * f - fall * sf / (1.0f + sf + f)));
}
// innermost stable circular orbit (prograde), outer and inner horizon, in r_s
static float iscoRs(float chi) {
    float z1 = 1.0f + std::cbrt(1.0f - chi * chi) * (std::cbrt(1.0f + chi) + std::cbrt(1.0f - chi));
    float z2 = std::sqrt(3.0f * chi * chi + z1 * z1);
    return 0.5f * (3.0f + z2 - std::sqrt((3.0f - z1) * (3.0f + z1 + 2.0f * z2)));
}
static float horizonRs(float chi, float sign) { return 0.5f * (1.0f + sign * std::sqrt(std::max(0.0f, 1.0f - chi * chi))); }

// frequency shift the camera sees straight ahead (same maths as the shader), drives auto exposure
static float viewShift(const CamState& c) {
    if (P.spin > 0.0f) {
        CamFrame cf = camBasis(c);
        V3 X = toStd(cf.pos), l; float f;
        ksField(X, f, l);
        float s = kerrCamSpeed(f, cf.fall), gam = 1.0f / std::sqrt(1.0f - s * s);
        V3 V = l * s, n = toStd(cf.fwd * -1.0f);
        float vn = dot(V, n);
        V3 nf = norm((n + V * ((gam - 1.0f) * vn / std::max(s * s, 1e-12f) + gam)) * (1.0f / (gam * (1.0f + vn))));
        float E = std::sqrt(1.0f + f) / std::max(1.0f - f * dot(nf, l), 0.05f);
        return E / (gam * (1.0f + vn));
    }
    CamFrame f = camBasis(c);
    float r = len(f.pos);
    V3 rh = f.pos * (1.0f / r);
    float v = camRadialVel(r, f.fall), vv = v * v;
    V3 V = rh * v, n = f.fwd * -1.0f;
    float gam = 1.0f / std::sqrt(1.0f - vv), vn = dot(V, n);
    V3 nf = (n + V * ((gam - 1.0f) * vn / std::max(vv, 1e-12f) + gam)) * (1.0f / (gam * (1.0f + vn)));
    float beta = std::sqrt(std::max(P.gravity, 0.0f) / r);
    float eInf = 1.0f + beta * dot(norm(nf) * -1.0f, rh);
    return 1.0f / (gam * (1.0f + vn) * std::max(eInf, 0.05f));
}
// a real camera stops down when everything ahead is blueshifted and bright
static float autoExposure(const CamState& c) {
    float q = std::max(0.1f, std::min(10.0f, viewShift(c)));
    return std::pow(q, -2.5f * P.autoExposure * P.obsShift);
}

// ============================================================================= film overlay
// Titles, captions and the instrument readout are drawn with GDI into a DIB as white on black (the
// brightness is the coverage), uploaded to a texture and laid over the graded picture by the composite.
struct Overlay {
    HDC dc = nullptr; HBITMAP bmp = nullptr; uint8_t* bits = nullptr;
    int w = 0, h = 0;
    HFONT font[CUE_STYLES] = {};
    GLuint tex = 0;
    std::string sig;
    bool visible = false;
};
static Overlay g_ovView, g_ovExport;

static std::wstring widen(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
static float barHeight(int w, int h) { return g_letterboxOn ? std::max(0.0f, (h - w / g_letterbox) * 0.5f) : 0.0f; }

static void overlayFree(Overlay& o) {
    for (auto& f : o.font) if (f) { DeleteObject(f); f = nullptr; }
    if (o.dc) { DeleteDC(o.dc); o.dc = nullptr; }
    if (o.bmp) { DeleteObject(o.bmp); o.bmp = nullptr; }
    if (o.tex) { glDeleteTextures(1, &o.tex); o.tex = 0; }
    o.w = o.h = 0; o.sig.clear(); o.visible = false;
}

static void overlayResize(Overlay& o, int w, int h) {
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

static std::string fmt(const char* f, double a, double b = 0, double c = 0) {
    char buf[128]; std::snprintf(buf, sizeof buf, f, a, b, c); return buf;
}
static std::string fmtDuration(double s) {
    if (s < 1.0)    return fmt("%.2f s", s);
    if (s < 60.0)   return fmt("%.1f s", s);
    if (s < 3600.0) return fmt("%.0f min %02.0f s", std::floor(s / 60), std::fmod(s, 60.0));
    if (s < 86400.0 * 2) return fmt("%.0f h %02.0f min", std::floor(s / 3600), std::floor(std::fmod(s, 3600.0) / 60));
    return fmt("%.1f days", s / 86400.0);
}

// instrument readout for a camera, in real units for a black hole of g_massSun suns
static std::vector<std::pair<std::string, std::string>> hudLines(const CamState& c) {
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
static void overlayUpdate(Overlay& o, int w, int h, float t, bool film) {
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

// ============================================================================= renderer
static GLuint g_progScene, g_progDown, g_progUp, g_progComp, g_progCopy;

static void setSceneUniforms(int w, int h, const CamFrame& cf, float t, float jx, float jy, float step) {
    GLuint p = g_progScene;
    p_glUseProgram(p);
    u2f(p, "uRes", (float)w, (float)h);
    u2f(p, "uJitter", jx, jy);
    u1f(p, "uTime", t);
    u3f(p, "uCamPos", cf.pos.x, cf.pos.y, cf.pos.z);
    u3f(p, "uCamFwd", cf.fwd.x, cf.fwd.y, cf.fwd.z);
    u3f(p, "uCamRight", cf.right.x, cf.right.y, cf.right.z);
    u3f(p, "uCamUp", cf.up.x, cf.up.y, cf.up.z);
    u1f(p, "uFov", cf.fov);
    u1f(p, "uStep", step);
    V3 vel = norm(cf.pos) * camRadialVel(len(cf.pos), cf.fall);
    u3f(p, "uCamVel", vel.x, vel.y, vel.z);
    u1f(p, "uObsShift", P.obsShift);
    u1f(p, "uSpin", P.spin);
    u1f(p, "uFall", cf.fall);
    u1f(p, "uGravity", P.gravity);
    u1f(p, "uDiskOn", P.disk ? 1.0f : 0.0f);
    u1f(p, "uDiskIn", P.diskIn);
    u1f(p, "uDiskOut", P.diskOut);
    u1f(p, "uTempPeak", P.tempPeak);
    u1f(p, "uDiskBright", P.diskBright);
    u1f(p, "uDiskOpacity", P.diskOpacity);
    u1f(p, "uSwirl", P.swirl);
    u1f(p, "uDoppler", P.doppler);
    u1f(p, "uRedshift", P.redshift);
    u1f(p, "uGlow", P.glow);
    u1f(p, "uStars", P.stars);
    u1f(p, "uMilky", P.milky);
    u1f(p, "uNebula", P.nebula);
    u1f(p, "uGalaxy", P.galaxy);
}

static void runBloom(const Target& src, std::vector<Target>& mips) {
    p_glUseProgram(g_progDown);
    u1i(g_progDown, "uSrc", 0);
    const Target* s = &src;
    for (auto& m : mips) {
        p_glBindFramebuffer(K_FRAMEBUFFER, m.fbo);
        glViewport(0, 0, m.w, m.h);
        bindTex(0, s->tex);
        u2f(g_progDown, "uSrcTexel", 1.0f / s->w, 1.0f / s->h);
        u2f(g_progDown, "uDstRes", (float)m.w, (float)m.h);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        s = &m;
    }
    p_glUseProgram(g_progUp);
    u1i(g_progUp, "uSrc", 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (int i = (int)mips.size() - 1; i > 0; i--) {
        Target& d = mips[i - 1];
        p_glBindFramebuffer(K_FRAMEBUFFER, d.fbo);
        glViewport(0, 0, d.w, d.h);
        bindTex(0, mips[i].tex);
        u2f(g_progUp, "uSrcTexel", 1.0f / mips[i].w, 1.0f / mips[i].h);
        u2f(g_progUp, "uDstRes", (float)d.w, (float)d.h);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glDisable(GL_BLEND);
}

static void composite(GLuint fbo, int w, int h, const Target& scene, const std::vector<Target>& mips, float t, float fade,
                      const Overlay& ov, float exposure) {
    p_glBindFramebuffer(K_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
    GLuint p = g_progComp;
    p_glUseProgram(p);
    bindTex(0, scene.tex);
    bindTex(1, mips.empty() ? scene.tex : mips[0].tex);
    bindTex(2, ov.tex ? ov.tex : scene.tex);
    u1i(p, "uScene", 0);
    u1i(p, "uBloom", 1);
    u1i(p, "uOverlay", 2);
    u1f(p, "uOverlayOn", ov.visible && ov.tex ? 1.0f : 0.0f);
    u1f(p, "uBar", barHeight(w, h));
    u2f(p, "uRes", (float)w, (float)h);
    u1f(p, "uTime", t);
    u1f(p, "uBloomStr", P.bloom);
    u1f(p, "uExposure", exposure);
    u1f(p, "uVignette", P.vignette);
    u1f(p, "uCA", P.ca);
    u1f(p, "uGrain", P.grain);
    u1f(p, "uFade", fade);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    p_glActiveTexture(K_TEXTURE0);
}

static void copyTex(GLuint tex, float ox, float oy, float w, float h, float scale) {
    p_glUseProgram(g_progCopy);
    bindTex(0, tex);
    u1i(g_progCopy, "uSrc", 0);
    u2f(g_progCopy, "uOffset", ox, oy);
    u2f(g_progCopy, "uRes", w, h);
    u1f(g_progCopy, "uScale", scale);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

// ============================================================================= export
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
} g_job;

static int   g_exRes = 1, g_exFps = 2, g_exSamples = 8;
static float g_exShutter = 0.5f;
static bool  g_exUltra = true;
static char  g_exPath[260] = "gargantua.mp4";
static char  g_ffmpeg[260] = "ffmpeg";
static char  g_seqPath[260] = "sequence.txt";
static std::string g_status;
static std::string g_stillName, g_stillPrefix = "still_";          // fixed screenshot file name (used by --stills)
static const int RES_W[] = {1280, 1920, 2560, 3840}, RES_H[] = {720, 1080, 1440, 2160};
static const int FPS_V[] = {24, 30, 60};
static const int TILE = 256;

static double secondsSince(LARGE_INTEGER t0) {
    LARGE_INTEGER f, n; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&n);
    return double(n.QuadPart - t0.QuadPart) / double(f.QuadPart);
}

static bool startFfmpeg(const std::string& args) {
    SECURITY_ATTRIBUTES sa = {sizeof sa, nullptr, TRUE};
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 1 << 20)) return false;
    SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0);
    HANDLE log = CreateFileA("ffmpeg_log.txt", GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
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

static std::string timestamp() {
    char b[64]; time_t t = time(nullptr); tm lt; localtime_s(&lt, &t);
    std::strftime(b, sizeof b, "%Y%m%d_%H%M%S", &lt);
    return b;
}

static void beginExport(bool screenshot) {
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
        j.out = g_stillName.empty() ? "gargantua_" + timestamp() + ".png" : g_stillName + ".png";
        args = std::string("-y -loglevel error -f rawvideo -pix_fmt rgb24 -s ") + sz + " -i - -frames:v 1 \"" + j.out + "\"";
    } else {
        j.out = g_exPath;
        args = std::string("-y -loglevel error -f rawvideo -pix_fmt rgb24 -s ") + sz + " -r " + std::to_string(j.fps) +
               " -i - -c:v libx264 -preset slow -crf 14 -pix_fmt yuv420p -movflags +faststart \"" + j.out + "\"";
    }
    j.useFfmpeg = startFfmpeg(args);
    if (!j.useFfmpeg) {
        if (screenshot) j.out = g_stillName.empty() ? "gargantua_" + timestamp() + ".bmp" : g_stillName + ".bmp";
        else { j.folder = "frames_" + timestamp(); CreateDirectoryA(j.folder.c_str(), nullptr); }
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

static void endExport(bool cancelled) {
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
        std::snprintf(b, sizeof b, "Done: %s  (%.1f s)", j.useFfmpeg || !j.folder.empty() ? j.out.c_str() : j.out.c_str(), secondsSince(j.t0));
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
    composite(j.out8.fbo, j.W, j.H, j.accum, j.mips, ft, fade, g_ovExport, ex);
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
            g_status = "ffmpeg stopped accepting frames, see ffmpeg_log.txt";
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
static void exportStep() {
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
        setSceneUniforms(j.W, j.H, camBasis(cs), sim, jx, jy, j.step);
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

// ============================================================================= UI
static void applyStyle() {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 8; s.FrameRounding = 4; s.GrabRounding = 4; s.PopupRounding = 4;
    s.WindowBorderSize = 0; s.FramePadding = ImVec2(8, 4); s.ItemSpacing = ImVec2(8, 6);
    ImVec4* c = s.Colors;
    ImVec4 accent(1.00f, 0.55f, 0.18f, 1.0f), accentDim(0.75f, 0.38f, 0.10f, 1.0f);
    c[ImGuiCol_WindowBg]         = ImVec4(0.05f, 0.05f, 0.07f, 0.88f);
    c[ImGuiCol_Header]           = ImVec4(0.30f, 0.16f, 0.06f, 0.80f);
    c[ImGuiCol_HeaderHovered]    = accentDim;
    c[ImGuiCol_HeaderActive]     = accent;
    c[ImGuiCol_Button]           = ImVec4(0.28f, 0.15f, 0.06f, 1.0f);
    c[ImGuiCol_ButtonHovered]    = accentDim;
    c[ImGuiCol_ButtonActive]     = accent;
    c[ImGuiCol_FrameBg]          = ImVec4(0.14f, 0.11f, 0.10f, 1.0f);
    c[ImGuiCol_FrameBgHovered]   = ImVec4(0.24f, 0.16f, 0.10f, 1.0f);
    c[ImGuiCol_FrameBgActive]    = ImVec4(0.32f, 0.20f, 0.10f, 1.0f);
    c[ImGuiCol_SliderGrab]       = accentDim;
    c[ImGuiCol_SliderGrabActive] = accent;
    c[ImGuiCol_CheckMark]        = accent;
    c[ImGuiCol_TitleBgActive]    = ImVec4(0.30f, 0.16f, 0.06f, 1.0f);
    c[ImGuiCol_PlotHistogram]    = accent;
    s.ScaleAllSizes(g_dpi);
#if IMGUI_VERSION_NUM >= 19200
    s.FontScaleDpi = g_dpi;
#else
    ImGui::GetIO().FontGlobalScale = g_dpi;
#endif
}

static bool fovSlider(const char* label, float* tanHalf) {
    float deg = 2.0f * std::atan(*tanHalf) * 180.0f / PI;
    if (ImGui::SliderFloat(label, &deg, 10.0f, 120.0f, "%.0f deg")) { *tanHalf = std::tan(deg * PI / 360.0f); return true; }
    return false;
}

static void drawUI(float fps, int rw, int rh) {
    if (!g_showUI) return;
    ImGui::SetNextWindowPos(ImVec2(12 * g_dpi, 12 * g_dpi), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(430 * g_dpi, 760 * g_dpi), ImGuiCond_FirstUseEver);
    ImGui::Begin("GARGANTUA STUDIO");

    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.25f, 1.0f), "%.0f fps", fps);
    ImGui::SameLine(); ImGui::TextDisabled("| %dx%d | TAB hides UI", rw, rh);

    // ---------------------------------------------------------------- export (locks the rest while running)
    if (g_job.active) {
        ExportJob& j = g_job;
        ImGui::Separator();
        ImGui::Text(j.screenshot ? "Rendering screenshot %dx%d" : "Rendering video %dx%d @ %d fps", j.W, j.H, j.fps);
        int tiles = ((j.W + TILE - 1) / TILE) * ((j.H + TILE - 1) / TILE);
        float sub  = (j.sub + j.tile / (float)tiles) / j.samples;
        float prog = (j.frame + sub) / (float)j.total;
        char ov[64]; std::snprintf(ov, sizeof ov, "frame %d / %d", std::min(j.frame + 1, j.total), j.total);
        ImGui::ProgressBar(prog, ImVec2(-1, 0), ov);
        double el = secondsSince(j.t0);
        double eta = prog > 0.001f ? el / prog * (1.0 - prog) : 0.0;
        ImGui::Text("elapsed %02d:%02d   remaining ~%02d:%02d", (int)el / 60, (int)el % 60, (int)eta / 60, (int)eta % 60);
        ImGui::TextDisabled(j.useFfmpeg ? "-> %s (ffmpeg)" : "-> BMP frames (ffmpeg not found)", j.out.c_str());
        if (ImGui::Button("Cancel", ImVec2(-1, 0))) endExport(true);
        ImGui::End();
        return;
    }

    // ---------------------------------------------------------------- camera
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::RadioButton("Free orbit", &g_camMode, CAM_ORBIT); ImGui::SameLine();
        ImGui::RadioButton("Play sequence", &g_camMode, CAM_SEQUENCE);
        if (g_camMode == CAM_ORBIT) {
            ImGui::Checkbox("Auto-rotate", &g_autoRotate); ImGui::SameLine();
            ImGui::Checkbox("Cinematic drift", &g_drift);
            ImGui::SliderFloat("Rotate speed", &g_rotSpeed, -0.6f, 0.6f, "%.2f rad/s");
            float wrapped = std::remainder(g_orbit.phi, 2 * PI);
            if (ImGui::SliderAngle("Azimuth", &wrapped, -180, 180)) g_orbit.phi = wrapped;
            ImGui::SliderAngle("Elevation", &g_orbit.el, -88, 88);
            ImGui::SliderFloat("Distance", &g_orbit.dist, MIN_DIST, 300.0f, "%.2f r_s", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderAngle("Roll", &g_orbit.roll, -60, 60);
            fovSlider("Field of view", &g_orbit.fov);
            ImGui::SliderAngle("Look yaw", &g_orbit.yaw, -180, 180);
            ImGui::SliderAngle("Look pitch", &g_orbit.pitch, -89, 89);
            ImGui::SliderFloat("Free fall", &g_orbit.fall, 0.0f, 1.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 = hovering on rockets, 1 = falling freely from far away.\n"
                                                          "Near and inside the horizon nothing can hover, so the camera falls anyway.");
            ImGui::TextDisabled("Drag the viewport to orbit, wheel to zoom.");
        } else {
            ImGui::TextDisabled("Grabbing the view switches to free orbit.");
        }
        ImGui::Checkbox("Pause time", &g_paused);
    }

    // ---------------------------------------------------------------- physics
    if (ImGui::CollapsingHeader("Black hole & disk", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Gravity (lensing)", &P.gravity, 0.0f, 2.0f, "%.2f x");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 = light travels in straight lines. See what gravity is doing.");
        ImGui::SliderFloat("Doppler beaming", &P.doppler, 0.0f, 1.0f);
        ImGui::SliderFloat("Grav. redshift", &P.redshift, 0.0f, 1.0f);
        ImGui::Checkbox("Accretion disk", &P.disk);
        ImGui::SliderFloat("Spin (a/M)", &P.spin, 0.0f, 0.998f, "%.3f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 = Schwarzschild (fast tracer). Above 0 the Kerr tracer runs:\n"
                                                      "frame dragging, a lopsided shadow, the disk reaching closer in.");
        ImGui::Checkbox("Disk starts at the ISCO", &P.iscoEdge);
        ImGui::BeginDisabled(P.iscoEdge);
        ImGui::DragFloatRange2("Disk radii", &P.diskIn, &P.diskOut, 0.05f, 0.5f, 40.0f, "in %.2f r_s", "out %.1f r_s");
        ImGui::EndDisabled();
        P.diskOut = std::max(P.diskOut, P.diskIn + 0.5f);
        ImGui::SliderFloat("Temperature", &P.tempPeak, 1500.0f, 30000.0f, "%.0f K", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Brightness", &P.diskBright, 0.05f, 8.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Opacity", &P.diskOpacity, 0.0f, 5.0f);
        ImGui::SliderFloat("Swirl speed", &P.swirl, 0.0f, 4.0f);
        ImGui::SliderFloat("Photon glow", &P.glow, 0.0f, 0.04f, "%.3f");
        ImGui::SliderFloat("Observer shift", &P.obsShift, 0.0f, 1.0f);
        ImGui::SliderFloat("Auto exposure", &P.autoExposure, 0.0f, 1.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Doppler and gravitational shift caused by the camera's own motion and depth.\n"
                                                      "Falling in, light ahead turns blue and bright, light behind turns red.");
        if (ImGui::Button("Physically based")) { P.doppler = 1; P.redshift = 1; P.tempPeak = 8000; P.diskBright = 1.25f; P.glow = 0.005f; }
        ImGui::SameLine();
        if (ImGui::Button("Interstellar look")) { P.spin = 0.6f; P.doppler = 0; P.redshift = 0.3f; P.tempPeak = 5200; P.diskBright = 1.6f; P.glow = 0.008f; }
        if (P.spin > 0.0f)
            ImGui::TextDisabled("horizon %.2f r_s | ISCO %.2f r_s", horizonRs(P.spin, 1.0f), iscoRs(P.spin));
        else
            ImGui::TextDisabled("horizon 1 r_s | photon sphere 1.5 r_s | ISCO 3 r_s");
    }

    // ---------------------------------------------------------------- sky
    if (ImGui::CollapsingHeader("Sky")) {
        ImGui::SliderFloat("Stars", &P.stars, 0.0f, 3.0f);
        ImGui::SliderFloat("Milky Way", &P.milky, 0.0f, 3.0f);
        ImGui::SliderFloat("Nebulae", &P.nebula, 0.0f, 3.0f);
        ImGui::SliderFloat("Spiral galaxy", &P.galaxy, 0.0f, 3.0f);
    }

    // ---------------------------------------------------------------- post
    if (ImGui::CollapsingHeader("Lens & post")) {
        ImGui::SliderFloat("Exposure", &P.exposure, 0.1f, 8.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Bloom", &P.bloom, 0.0f, 0.4f);
        ImGui::SliderFloat("Vignette", &P.vignette, 0.0f, 1.0f);
        ImGui::SliderFloat("Chromatic aberr.", &P.ca, 0.0f, 0.05f, "%.3f");
        ImGui::SliderFloat("Film grain", &P.grain, 0.0f, 10.0f);
    }

    // ---------------------------------------------------------------- quality
    if (ImGui::CollapsingHeader("Quality")) {
        ImGui::SliderFloat("Render scale", &P.renderScale, 0.25f, 2.0f, "%.2f x");
        ImGui::Checkbox("Ultra geodesic integration", &P.ultra);
        ImGui::TextDisabled("Lower the render scale if the frame rate drops.");
    }

    // ---------------------------------------------------------------- sequence
    if (ImGui::CollapsingHeader("Sequence", ImGuiTreeNodeFlags_DefaultOpen)) {
        float L = seqLength();
        if (ImGui::Button(g_playing ? "Pause" : "Play", ImVec2(70 * g_dpi, 0))) { g_playing = !g_playing; g_camMode = CAM_SEQUENCE; }
        ImGui::SameLine();
        if (ImGui::Button("|<")) { g_playhead = 0; g_camMode = CAM_SEQUENCE; }
        ImGui::SameLine();
        ImGui::Checkbox("Loop", &g_loop);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90 * g_dpi);
        ImGui::SliderFloat("##speed", &g_playSpeed, 0.1f, 4.0f, "%.1fx speed");
        if (ImGui::SliderFloat("Playhead", &g_playhead, 0.0f, std::max(L, 0.01f), "%.2f s")) g_camMode = CAM_SEQUENCE;
        ImGui::SetNextItemWidth(70 * g_dpi);
        ImGui::DragFloat("Fade in", &g_fadeIn, 0.05f, 0.0f, 10.0f, "%.1f s"); ImGui::SameLine();
        ImGui::SetNextItemWidth(70 * g_dpi);
        ImGui::DragFloat("Fade out", &g_fadeOut, 0.05f, 0.0f, 10.0f, "%.2f s"); ImGui::SameLine();
        ImGui::SetNextItemWidth(70 * g_dpi);
        ImGui::DragFloat("Black at", &g_blackAt, 0.1f, 0.0f, 600.0f, g_blackAt > 0 ? "%.1f s" : "end");

        if (ImGui::Button("Add key at playhead (current view)")) { addKey(g_playhead, currentCam()); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Frame a shot in free orbit, move the playhead, click.");

        if (ImGui::BeginTable("keys", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY,
                              ImVec2(0, 190 * g_dpi))) {
            ImGui::TableSetupColumn("time", ImGuiTableColumnFlags_WidthFixed, 80 * g_dpi);
            ImGui::TableSetupColumn("view", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 90 * g_dpi);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 22 * g_dpi);
            ImGui::TableHeadersRow();
            int del = -1;
            bool resort = false;
            for (int i = 0; i < (int)g_keys.size(); i++) {
                Key& k = g_keys[i];
                ImGui::PushID(i);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##t", &k.t, 0.05f, 0.0f, 600.0f, "%.2f");
                if (ImGui::IsItemDeactivatedAfterEdit()) resort = true;
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("r %.2f  el %.0f  fov %.0f%s", k.c.dist, k.c.el * 180 / PI, 2 * std::atan(k.c.fov) * 180 / PI,
                            k.c.fall > 0.5f ? "  falling" : "");
                ImGui::TableSetColumnIndex(2);
                if (ImGui::SmallButton("Go")) { g_playhead = k.t; g_camMode = CAM_SEQUENCE; g_playing = false; }
                ImGui::SameLine();
                if (ImGui::SmallButton("Set")) { CamState c = currentCam(); k.c = c; }
                ImGui::TableSetColumnIndex(3);
                if (ImGui::SmallButton("x")) del = i;
                ImGui::PopID();
            }
            if (del >= 0) g_keys.erase(g_keys.begin() + del);
            if (resort) sortKeys();
            ImGui::EndTable();
        }
        ImGui::SetNextItemWidth(160 * g_dpi);
        ImGui::InputText("##seqpath", g_seqPath, sizeof g_seqPath); ImGui::SameLine();
        if (ImGui::Button("Save")) g_status = saveSequence(g_seqPath) ? "Sequence saved." : "Could not save sequence.";
        ImGui::SameLine();
        if (ImGui::Button("Load")) g_status = loadSequence(g_seqPath) ? "Sequence loaded." : "Could not load sequence.";
        ImGui::SameLine();
        if (ImGui::Button("Default film")) defaultSequence();
    }

    // ---------------------------------------------------------------- film
    if (ImGui::CollapsingHeader("Film")) {
        ImGui::Checkbox("Letterbox", &g_letterboxOn); ImGui::SameLine();
        ImGui::SetNextItemWidth(120 * g_dpi);
        ImGui::SliderFloat("##aspect", &g_letterbox, 1.78f, 2.76f, "%.2f : 1");
        ImGui::Checkbox("Titles, captions & HUD", &g_cuesOn);
        ImGui::SliderFloat("Mass (suns)", &g_massSun, 5.0f, 1.0e10f, "%.3g", ImGuiSliderFlags_Logarithmic);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Only changes the real-world units in the HUD.\n"
                                                      "Small black holes have deadly tides at the horizon, big ones gentle.");
        if (ImGui::TreeNode("Cues", "Cues (%d)", (int)g_cues.size())) {
            static const char* styles = "Title\0Subtitle\0Caption\0Chapter\0HUD\0";
            int del = -1;
            for (int i = 0; i < (int)g_cues.size(); i++) {
                Cue& c = g_cues[i];
                ImGui::PushID(i);
                ImGui::SetNextItemWidth(110 * g_dpi);
                ImGui::DragFloatRange2("##t", &c.t0, &c.t1, 0.05f, 0.0f, 600.0f, "%.1f", "%.1f"); ImGui::SameLine();
                ImGui::SetNextItemWidth(80 * g_dpi);
                ImGui::Combo("##s", &c.style, styles); ImGui::SameLine();
                if (ImGui::SmallButton("x")) del = i;
                char buf[512]; std::snprintf(buf, sizeof buf, "%s", c.text.c_str());
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##text", buf, sizeof buf)) c.text = buf;
                ImGui::PopID();
            }
            if (del >= 0) g_cues.erase(g_cues.begin() + del);
            if (ImGui::Button("Add caption at playhead")) g_cues.push_back({g_playhead, g_playhead + 6.0f, CUE_CAPTION, "New caption"});
            ImGui::TreePop();
        }
    }

    // ---------------------------------------------------------------- export
    if (ImGui::CollapsingHeader("Render video", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Combo("Resolution", &g_exRes, "1280 x 720\0" "1920 x 1080\0" "2560 x 1440\0" "3840 x 2160 (4K)\0");
        ImGui::Combo("Frame rate", &g_exFps, "24 fps (film)\0" "30 fps\0" "60 fps\0");
        ImGui::SliderInt("Samples / frame", &g_exSamples, 1, 64);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sub-frames averaged per frame: antialiasing + motion blur.\nRender time scales linearly.");
        ImGui::SliderFloat("Shutter", &g_exShutter, 0.0f, 1.0f, "%.2f (180 deg = 0.5)");
        ImGui::Checkbox("Ultra geodesics", &g_exUltra);
        ImGui::InputText("Output", g_exPath, sizeof g_exPath);
        ImGui::InputText("ffmpeg", g_ffmpeg, sizeof g_ffmpeg);
        int frames = (int)std::ceil(seqLength() * FPS_V[g_exFps]);
        ImGui::TextDisabled("%d frames, %.1f s of sequence", frames, seqLength());
        if (ImGui::Button("Render sequence to video", ImVec2(-1, 0))) beginExport(false);
        if (ImGui::Button("Screenshot of current view", ImVec2(-1, 0))) beginExport(true);
    }

    if (!g_status.empty()) { ImGui::Separator(); ImGui::TextWrapped("%s", g_status.c_str()); }
    ImGui::End();
}

// ============================================================================= window
static void toggleFullscreen() {
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
        case VK_ESCAPE: if (!g_job.active) g_running = false; break;
        case VK_F11:    toggleFullscreen(); break;
        case VK_TAB:    g_showUI = !g_showUI; break;
        case VK_SPACE:
            if (g_camMode == CAM_SEQUENCE) g_playing = !g_playing; else g_paused = !g_paused;
            break;
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (uiMouse || g_job.active) break;
        grabCamera();
        g_dragging = true; g_lastMX = GET_X_LPARAM(lp); g_lastMY = GET_Y_LPARAM(lp);
        SetCapture(h); return 0;
    case WM_LBUTTONUP:
        if (g_dragging) { g_dragging = false; ReleaseCapture(); }
        break;
    case WM_MOUSEMOVE:
        if (g_dragging) {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            g_orbit.phi += (x - g_lastMX) * 0.005f;
            g_orbit.el  += (y - g_lastMY) * 0.004f;
            g_orbit.el   = std::max(-1.53f, std::min(1.53f, g_orbit.el));
            g_lastMX = x; g_lastMY = y;
        }
        break;
    case WM_MOUSEWHEEL:
        if (uiMouse || g_job.active) break;
        grabCamera();
        g_orbit.dist = std::max(MIN_DIST, std::min(300.0f, g_orbit.dist * std::pow(0.9f, GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA)));
        return 0;
    }
    return DefWindowProcA(h, m, wp, lp);
}

// ============================================================================= main
int main() {
    ImGui_ImplWin32_EnableDpiAwareness();
    defaultSequence();

    // --stills t1,t2,...  [--res 0..3] [--samples n]: save screenshots of the film at those times, then quit
    std::vector<float> stills;
    for (int i = 1; i < __argc; i++) {
        std::string a = __argv[i];
        if (a == "--stills" && i + 1 < __argc) {
            for (char* tok = std::strtok(__argv[++i], ","); tok; tok = std::strtok(nullptr, ",")) stills.push_back((float)std::atof(tok));
        } else if (a == "--res" && i + 1 < __argc) g_exRes = std::max(0, std::min(3, std::atoi(__argv[++i])));
        else if (a == "--samples" && i + 1 < __argc) g_exSamples = std::max(1, std::atoi(__argv[++i]));
        else if (a == "--sequence" && i + 1 < __argc) loadSequence(__argv[++i]);
        else if (a == "--spin" && i + 1 < __argc) P.spin = std::max(0.0f, std::min(0.998f, (float)std::atof(__argv[++i])));
        else if (a == "--prefix" && i + 1 < __argc) g_stillPrefix = __argv[++i];
    }
    size_t stillNext = 0;

    HINSTANCE inst = GetModuleHandleA(nullptr);
    WNDCLASSA wc = {};
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "GargantuaStudio";
    RegisterClassA(&wc);

    g_hwnd = CreateWindowA(wc.lpszClassName, "GARGANTUA STUDIO", WS_OVERLAPPEDWINDOW,
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

    g_progScene = makeProgram({FS_SCENE_A, FS_SCENE_B, FS_SCENE_C}, "scene.frag");
    g_progDown  = makeProgram({FS_DOWN}, "bloom_down.frag");
    g_progUp    = makeProgram({FS_UP}, "bloom_up.frag");
    g_progComp  = makeProgram({FS_COMPOSITE}, "composite.frag");
    g_progCopy  = makeProgram({FS_COPY}, "copy.frag");
    GLuint vao; p_glGenVertexArrays(1, &vao); p_glBindVertexArray(vao);

    ShowWindow(g_hwnd, SW_SHOWMAXIMIZED);
    UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = "gargantua_ui.ini";
    g_dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(g_hwnd);
    applyStyle();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplOpenGL3_Init("#version 330");

    Target scene;
    std::vector<Target> mips;
    int builtW = 0, builtH = 0; float builtScale = 0;

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

        if (P.iscoEdge) P.diskIn = iscoRs(P.spin) * std::max(P.gravity, 0.01f);
        P.diskOut = std::max(P.diskOut, P.diskIn + 0.5f);

        // ---- advance time
        if (!g_job.active) {
            if (!g_paused) g_simTime += dt;
            if (g_camMode == CAM_ORBIT && g_autoRotate && !g_paused && !g_dragging) g_orbit.phi += g_rotSpeed * dt;
            if (g_camMode == CAM_SEQUENCE && g_playing) {
                g_playhead += dt * g_playSpeed;
                float L = seqLength();
                if (g_playhead > L) { if (g_loop && L > 0) g_playhead = std::fmod(g_playhead, L); else { g_playhead = L; g_playing = false; } }
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        drawUI(fps, g_job.active ? g_job.W : scene.w, g_job.active ? g_job.H : scene.h);

        if (g_job.active) {
            // ---- offline render, then show the latest finished frame letterboxed
            exportStep();
            p_glBindFramebuffer(K_FRAMEBUFFER, 0);
            glViewport(0, 0, g_w, g_h);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            if (g_job.active && g_job.haveFrame) {
                float a = (float)g_job.W / g_job.H;
                float vw = (float)g_w, vh = vw / a;
                if (vh > g_h) { vh = (float)g_h; vw = vh * a; }
                float vx = (g_w - vw) * 0.5f, vy = (g_h - vh) * 0.5f;
                glViewport((int)vx, (int)vy, (int)vw, (int)vh);
                copyTex(g_job.out8.tex, vx, vy, vw, vh, 1.0f);
                glViewport(0, 0, g_w, g_h);
            }
        } else {
            // ---- real-time render
            if (g_w != builtW || g_h != builtH || P.renderScale != builtScale) {
                freeTarget(scene);
                scene = makeTarget(std::max(1, (int)(g_w * P.renderScale)), std::max(1, (int)(g_h * P.renderScale)));
                buildMips(mips, scene.w, scene.h);
                builtW = g_w; builtH = g_h; builtScale = P.renderScale;
            }
            p_glBindFramebuffer(K_FRAMEBUFFER, scene.fbo);
            glViewport(0, 0, scene.w, scene.h);
            setSceneUniforms(scene.w, scene.h, camBasis(currentCam()), currentTime(), 0, 0, P.ultra ? 0.012f : 0.03f);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            runBloom(scene, mips);
            overlayUpdate(g_ovView, g_w, g_h, g_playhead, g_camMode == CAM_SEQUENCE);
            composite(0, g_w, g_h, scene, mips, currentTime(), currentFade(), g_ovView,
                      P.exposure * autoExposure(currentCam()));
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SwapBuffers(dc);
    }

    if (g_job.active) endExport(true);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(glrc);
    ReleaseDC(g_hwnd, dc);
    DestroyWindow(g_hwnd);
    return 0;
}
