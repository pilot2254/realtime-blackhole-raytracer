// Scene shader: a full ray tracer per pixel.
//   Units: Schwarzschild radius r_s = 1 (G = c = 1, M = 1/2).
//   - Schwarzschild: null geodesics via the Binet equation u'' + u = (3/2) r_s u^2, reproduced exactly
//     by the Cartesian pseudo-force a = -(3/2) h^2 p / |p|^5, integrated with leapfrog. The camera is
//     a free-falling (Painleve-Gullstrand) observer, exact inside the horizon too.
//   - Kerr: Hamilton's equations in Cartesian Kerr-Schild coordinates, RK4.
//   - Thin Keplerian disk from the ISCO, Novikov-Thorne-like temperatures; Doppler + gravitational
//     shift change the observed blackbody temperature, brightness scales with shift^4.
//   - Sky (stars, Milky Way, nebulae, a spiral galaxy) is sampled along the bent ray.
#include "render/shaders.h"

const char* FS_SCENE_A = R"GLSL(
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

const char* FS_SCENE_B = R"GLSL(
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

const char* FS_SCENE_C = R"GLSL(
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
