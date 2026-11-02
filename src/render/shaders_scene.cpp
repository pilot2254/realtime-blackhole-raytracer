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

// photon-ring light (rays that wound far round the hole) is thinner than a pixel: it gets the disk's average
// brightness instead of its streaks, and is softened, so it reads as a thin line instead of sparkles
float gRing = 0.0;
float ringFade(float wind) { return clamp((wind - 4.4) * 0.8, 0.0, 1.0); }      // past ~1.4 half-turns

// s = frequency seen by the camera / emitted
void shadeDisk(vec3 hit, float r, float s, inout vec3 col, inout float T) {
    float ang  = atan(hit.z, hit.x);
    float tex  = mix(diskTex(r, ang, uTime), 0.75, gRing) * (1.0 - 0.45 * gRing);
    float x    = uDiskIn / r;
    float edge = smoothstep(uDiskIn, uDiskIn + 0.45, r) *
                 smoothstep(uDiskOut, uDiskOut - 0.45 * (uDiskOut - uDiskIn), r);

    float temp = uTempPeak * 1.6 * pow(x, 0.75) * pow(max(1.0 - 0.97 * sqrt(x), 0.0), 0.25);

    float I = x * x * edge * tex * min(pow(s, 4.0), 25.0);
    col += T * blackbody(temp * s) * I * uDiskBright;
    T   *= 1.0 - clamp(pow(x, 1.1) * edge * tex * uDiskOpacity, 0.0, 0.95);
}

// ------------------------------------------------------------------ scene objects
// Every object is tested against the straight piece of the bent ray between two integration steps, so
// it is lensed like everything else (and shows up in secondary images). World coordinates.
//   a: centre.xyz, radius      b: type, temperature, brightness, ship spin angle / jet length
//   c: colour.rgb, jet speed   v: hot-spot velocity, or the ship / planet axis
const int MAX_OBJ = 8;
uniform int  uObjCount;
uniform vec4 uObjA[MAX_OBJ];
uniform vec4 uObjB[MAX_OBJ];
uniform vec4 uObjC[MAX_OBJ];
uniform vec4 uObjV[MAX_OBJ];

// first point of segment a + s d (s in [0, 1]) inside the sphere, or -1
float segSphere(vec3 a, vec3 d, vec3 c, float R) {
    vec3 m = a - c;
    float A = dot(d, d), B = dot(m, d), C = dot(m, m) - R * R;
    float disc = B * B - A * C;
    if (disc < 0.0) return -1.0;
    float s = (-B - sqrt(disc)) / A;
    if (s > 1.0) return -1.0;
    if (s < 0.0) return (C < 0.0) ? 0.0 : -1.0;
    return s;
}

// the Endurance: twelve modules on a ring around a hub, four spokes. Units of its radius, axis = z.
float sdBox(vec3 p, vec3 b) { vec3 q = abs(p) - b; return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0); }
float sdCylZ(vec3 p, float r, float h) { vec2 d = abs(vec2(length(p.xy), p.z)) - vec2(r, h); return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)); }
float sdCylX(vec3 p, float r, float h) { vec2 d = abs(vec2(length(p.yz), p.x)) - vec2(r, h); return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)); }
// the Endurance: twelve rounded modules on a torus ring, a cylindrical hub, four spokes. Units of its radius.
float shipSDF(vec3 p) {
    float ang = atan(p.y, p.x);
    float a = (floor(ang / (PI / 6.0)) + 0.5) * (PI / 6.0);
    vec3 q = vec3(cos(a) * p.x + sin(a) * p.y, -sin(a) * p.x + cos(a) * p.y, p.z);
    float module = sdBox(q - vec3(1.0, 0.0, 0.0), vec3(0.085, 0.15, 0.085)) - 0.035;
    float ring = length(vec2(length(p.xy) - 1.0, p.z)) - 0.028;
    float hub = min(sdCylZ(p, 0.13, 0.20) - 0.015, sdCylZ(p - vec3(0.0, 0.0, 0.25), 0.07, 0.07));
    float b4 = floor(ang / (PI * 0.5) + 0.5) * (PI * 0.5);
    vec3 sp = vec3(cos(b4) * p.x + sin(b4) * p.y, -sin(b4) * p.x + cos(b4) * p.y, p.z);
    float spoke = sdCylX(sp - vec3(0.56, 0.0, 0.0), 0.018, 0.43);
    return min(min(module, ring), min(hub, spoke));
}
void shipFrame(int i, out vec3 X, out vec3 Y, out vec3 Z) {
    Z = normalize(uObjV[i].xyz);
    vec3 helper = abs(Z.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 x0 = normalize(cross(helper, Z)), y0 = cross(Z, x0);
    float sa = sin(uObjB[i].w), ca = cos(uObjB[i].w);
    X = x0 * ca + y0 * sa; Y = -x0 * sa + y0 * ca;
}
float shipLocalSDF(int i, vec3 p, vec3 X, vec3 Y, vec3 Z) {
    vec3 m = p - uObjA[i].xyz;
    return shipSDF(vec3(dot(m, X), dot(m, Y), dot(m, Z)) / uObjA[i].w) * uObjA[i].w;
}
// sphere-trace the ship along the segment; s = hit parameter
bool shipHit(int i, vec3 a, vec3 d, out float s) {
    float s0 = segSphere(a, d, uObjA[i].xyz, uObjA[i].w * 1.35);
    if (s0 < 0.0) return false;
    vec3 X, Y, Z; shipFrame(i, X, Y, Z);
    float L = length(d), t = s0 * L;
    vec3 dir = d / L;
    for (int k = 0; k < 48; k++) {
        float dist = shipLocalSDF(i, a + dir * t, X, Y, Z);
        if (dist < 0.002 * uObjA[i].w) { s = t / L; return true; }
        t += dist * 0.9;
        if (t > L) return false;
    }
    return false;
}

// lit by the hot inner disk around the hole
// lit by the hot inner disk round the hole (key), the disk glowing underneath or above (fill), and the sky
vec3 surfaceLight(vec3 p, vec3 n, vec3 V, vec3 albedo, float gloss, float specAmt) {
    vec3 L = normalize(-p + vec3(0.0, -0.25 * p.y, 0.0));
    vec3 Lf = vec3(0.0, p.y > 0.0 ? -1.0 : 1.0, 0.0);
    vec3 H = normalize(L + V);
    float key  = max(dot(n, L), 0.0);
    float fill = max(dot(n, Lf), 0.0) * 0.22;
    float sky  = 0.10 + 0.06 * max(dot(n, -L), 0.0);
    float spec = pow(max(dot(n, H), 0.0), gloss) * specAmt * key;
    float rim  = pow(1.0 - max(dot(n, V), 0.0), 3.0) * (0.25 + 0.75 * key) * 0.25;
    vec3 warm = vec3(1.0, 0.85, 0.68);
    return (albedo * (warm * (key * 1.7 + fill) + vec3(0.6, 0.7, 0.9) * sky) + warm * spec * 1.6 + vec3(0.55, 0.65, 0.8) * rim);
}

// Miller's planet: a shallow ocean with long, mountain-sized swells, streaky cloud bands and a thin
// atmosphere, lit by the disk
vec3 shadePlanet(int i, vec3 hp, vec3 V) {
    vec3 c = uObjA[i].xyz;
    vec3 n = normalize(hp - c);
    float rot = uTime * 0.03;
    vec3 m = vec3(cos(rot) * n.x - sin(rot) * n.z, n.y, sin(rot) * n.x + cos(rot) * n.z);
    vec3 L  = normalize(-c + vec3(0.0, -0.25 * c.y, 0.0));
    vec3 Lf = vec3(0.0, c.y > 0.0 ? -1.0 : 1.0, 0.0);
    float NdL = dot(n, L);

    float swell = fbm(m * vec3(2.0, 7.0, 2.0) + 2.0, 4);                      // long bands of waves
    vec3 nw = normalize(n + 0.10 * (vec3(fbm(m * 5.0 + 3.1, 3), fbm(m * 5.0 + 7.3, 3), fbm(m * 5.0 + 1.7, 3)) - 0.5)
                          + 0.06 * (swell - 0.5) * vec3(0.0, 1.0, 0.0));
    vec3 ocean = uObjC[i].rgb * (0.55 + 0.7 * swell);
    float clouds = smoothstep(0.50, 0.78, fbm(m * vec3(2.2, 6.5, 2.2) + vec3(11.0, uTime * 0.01, 4.0), 5)) * 0.85;

    vec3 warm = vec3(1.0, 0.85, 0.68);
    float lit  = max((NdL + 0.18) / 1.18, 0.0);                               // soft terminator
    vec3 H = normalize(L + V);
    float glint = pow(max(dot(nw, H), 0.0), 160.0) * 4.0 * (1.0 - clouds) * smoothstep(0.0, 0.2, NdL);
    float fres  = pow(1.0 - max(dot(nw, V), 0.0), 5.0);
    float fill  = max(dot(n, Lf), 0.0) * 0.18;

    vec3 col = ocean * warm * (lit * 1.5 + fill) + warm * glint + vec3(0.30, 0.38, 0.45) * fres * lit * 0.6;
    col = mix(col, vec3(0.93, 0.94, 0.96) * warm * (lit * 1.7 + fill + 0.02), clouds);
    float rim = pow(1.0 - max(dot(n, V), 0.0), 2.5);
    col += vec3(0.42, 0.60, 0.95) * rim * smoothstep(-0.25, 0.5, NdL) * 0.8;
    return col;
}

vec3 shadeShip(int i, vec3 hp, vec3 V) {
    vec3 X, Y, Z; shipFrame(i, X, Y, Z);
    const float e = 0.0015;
    float R = uObjA[i].w;
    vec3 n = normalize(vec3(
        shipLocalSDF(i, hp + vec3(e, 0, 0) * R, X, Y, Z) - shipLocalSDF(i, hp - vec3(e, 0, 0) * R, X, Y, Z),
        shipLocalSDF(i, hp + vec3(0, e, 0) * R, X, Y, Z) - shipLocalSDF(i, hp - vec3(0, e, 0) * R, X, Y, Z),
        shipLocalSDF(i, hp + vec3(0, 0, e) * R, X, Y, Z) - shipLocalSDF(i, hp - vec3(0, 0, e) * R, X, Y, Z)));
    vec3 m = (hp - uObjA[i].xyz) / R;
    vec3 lp = vec3(dot(m, X), dot(m, Y), dot(m, Z));
    float rr = length(lp.xy);
    bool onModule = rr > 0.86 && rr < 1.14 && abs(lp.z) < 0.13;
    bool onHub = rr < 0.16;
    vec3 albedo = uObjC[i].rgb * (onHub ? 0.55 : 1.0);
    // panel seams and a dark band round each module
    float seam = onModule ? smoothstep(0.02, 0.0, abs(fract(atan(lp.y, lp.x) / (PI / 6.0)) - 0.5) - 0.40) : 0.0;
    albedo *= 1.0 - 0.35 * seam - 0.25 * step(0.05, abs(lp.z)) * float(onModule) * step(abs(lp.z), 0.07);
    vec3 col = surfaceLight(hp, n, V, albedo, 50.0, 0.8);
    // small lit windows on the outer faces of the modules
    if (onModule && rr > 1.06) {
        float w = step(0.75, fract(atan(lp.y, lp.x) * 60.0 / PI)) * step(abs(lp.z), 0.035);
        col += vec3(1.0, 0.8, 0.5) * w * 0.9;
    }
    return col;
}

// adds glowing objects along the segment; returns true when an opaque object was hit (T becomes 0)
// (a solid object fading in or out only dims T, and the ray goes on)
bool objectsAlong(vec3 a, vec3 b, float qObs, inout vec3 col, inout float T) {
    vec3 d = b - a;
    float L2 = dot(d, d), best = 2.0;
    int hit = -1;
    vec3 n = -d * inversesqrt(L2);                                        // direction the photon travels
    for (int i = 0; i < MAX_OBJ; i++) {
        if (i >= uObjCount) break;
        int type = int(uObjB[i].x + 0.5);
        vec3 c = uObjA[i].xyz;
        float R = uObjA[i].w;
        if (type == 0) {
            // hot spot: a blob of superheated gas, Doppler boosted by its orbit
            vec3 m = a - c;
            float s = clamp(-dot(m, d) / L2, 0.0, 1.0);
            vec3 pc = m + d * s;
            float dist2 = dot(pc, pc);
            if (dist2 < 9.0 * R * R) {
                vec3 beta = uObjV[i].xyz;
                float gam = inversesqrt(1.0 - dot(beta, beta));
                float D = mix(1.0, 1.0 / (gam * (1.0 - dot(beta, n))), uDoppler);
                float g = mix(1.0, sqrt(max(1.0 - RS / max(length(c), 1.01), 0.0)), uRedshift);
                float sh = D * g * qObs;
                float em = exp(-dist2 / (R * R)) * sqrt(L2) / R * 0.35 * uObjB[i].z * uObjV[i].w;
                col += T * blackbody(uObjB[i].y * sh) * em * min(sh * sh * sh * sh, 30.0);
            }
        } else if (type == 3) {
            // relativistic jet along the spin axis, both ways; beaming makes the approaching one brighter
            vec3 mid = a + 0.5 * d;
            float h = abs(mid.y), len = uObjB[i].w, w = R * (0.3 + h * 0.12);
            if (h > 1.0 && h < len) {
                float rho2 = mid.x * mid.x + mid.z * mid.z;
                float beta = uObjC[i].w, gam = inversesqrt(1.0 - beta * beta);
                float D = 1.0 / (gam * (1.0 - beta * sign(mid.y) * n.y));
                float em = exp(-rho2 / (w * w)) * smoothstep(1.0, 2.0, h) * exp(-h / (0.4 * len)) * sqrt(L2) / w;
                col += T * blackbody(uObjB[i].y * D * qObs) * em * uObjB[i].z * uObjV[i].w * 0.15 * min(D * D * D, 20.0);
            }
        } else if (type == 1) {
            float s = segSphere(a, d, c, R);
            if (s >= 0.0 && s < best) { best = s; hit = i; }
            // atmosphere: a thin glowing shell, brightest on the lit limb
            vec3 m = a - c;
            float sc = clamp(-dot(m, d) / L2, 0.0, 1.0);
            vec3 pc = m + d * sc;
            float dist = length(pc);
            if (dist > R && dist < R * 1.12) {
                float h = (dist - R) / (0.035 * R);
                float lit = smoothstep(-0.3, 0.6, dot(pc / dist, normalize(-c)));
                float chord = min(sqrt(L2), 2.0 * sqrt(max(2.0 * R * (dist - R), 1e-6)) + 0.05 * R);
                col += T * vec3(0.40, 0.58, 1.0) * exp(-h) * lit * chord / R * 0.9 * uObjV[i].w;
            }
        } else if (type == 2) {
            float s;
            if (shipHit(i, a, d, s) && s < best) { best = s; hit = i; }
        }
    }
    if (hit < 0) return false;
    vec3 hp = a + d * best;
    int type = int(uObjB[hit].x + 0.5);
    // presence < 1 while an object fades in or out at the ends of its time window: partly see-through
    float pr = uObjV[hit].w;
    col += T * pr * (type == 1 ? shadePlanet(hit, hp, n) : shadeShip(hit, hp, n)) * min(qObs * qObs, 4.0);
    T *= 1.0 - pr;
    return T < 1e-3;
}

void trace(vec3 ro, vec3 rd, float qObs, out vec3 col, out float T, out vec3 dirOut, out float escaped) {
    vec3 p = ro, v = rd;
    vec3 L = cross(p, v);
    float k = 1.5 * dot(L, L) * uGravity;
    col = vec3(0.0); T = 1.0; escaped = 0.0; dirOut = rd;
    float wind = 0.0;       // angle the ray has swept round the hole
    float glowUsed = 0.0;   // photon glow budget: rays skimming the photon orbit would otherwise pile it up
    int images = 0;     // disk crossings so far: only the direct image and the first photon ring are drawn,
                        // the higher-order rings are far thinner than a pixel and would only sparkle

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

        wind += length(cross(pp, p)) / max(length(pp) * r, 1e-6);          // true angle swept round the hole
        float gw = dt * length(v) / max(r2 * r2, 1.0);
        col += T * vec3(1.0, 0.55, 0.25) * (uGlow * gw * clamp(1.0 - glowUsed / 1.2, 0.0, 1.0));
        glowUsed += gw;
        if (uObjCount > 0 && objectsAlong(pp, p, qObs, col, T)) { dirOut = normalize(v); return; }

        if (uDiskOn > 0.5 && (pp.y > 0.0) != (p.y > 0.0)) {
            float f  = pp.y / (pp.y - p.y);
            vec3 hit = mix(pp, p, f);
            float hr = length(hit);
            if (hr > uDiskIn && hr < uDiskOut && ++images <= 2) {
                gRing = ringFade(wind);
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
    int stuck = 0, images = 0;
    float glowUsed = 0.0, wind = 0.0;

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
        wind += length(cross(x, xn)) / max(R * length(xn), 1e-6);
        if (r > rH) {
            float gw = stepLen / max(r * r * r * r * 16.0 * KM * KM * KM * KM, 1.0);
            col += T * vec3(1.0, 0.55, 0.25) * (uGlow * gw * clamp(1.0 - glowUsed / 1.2, 0.0, 1.0));
            glowUsed += gw;
        }
        if (uObjCount > 0 && objectsAlong(fromStd(x), fromStd(xn), qObs, col, T)) return;

        if (uDiskOn > 0.5 && (x.z > 0.0) != (xn.z > 0.0)) {
            vec3 hit = mix(x, xn, x.z / (x.z - xn.z));
            float fh; vec3 lh;
            float hr = ksField(hit, fh, lh);
            if (hr > uDiskIn && hr < uDiskOut && ++images <= 2) {
                gRing = ringFade(wind);
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
    // right at the shadow's edge the sky seen through neighbouring pixels jumps around wildly; there the
    // pixel would see the average of a huge patch of sky (almost black), so fade it instead of sparkling
    float fpRaw = length(fwidth(dir));
    float fp = clamp(fpRaw, 2.0 * uFov / uRes.y, 0.05);
    float skySeen = 1.0 - smoothstep(0.012, 0.05, fpRaw);
    if (esc > 0.5) col += T * sky(dir, fp) * shiftTint(q) * min(q * q * q * q, 40.0) * skySeen;
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
    float fpRaw = length(fwidth(dir));
    float fp = clamp(fpRaw, 2.0 * uFov / uRes.y, 0.05);
    float skySeen = 1.0 - smoothstep(0.012, 0.05, fpRaw);          // see kerrMain
    if (esc > 0.5) col += T * sky(dir, fp) * shiftTint(q) * min(q * q * q * q, 40.0) * skySeen;
    fragColor = vec4(col, 1.0);
}
)GLSL";
