#include "sim/physics.h"
#include "sim/params.h"

// camera velocity relative to the local free-falling observer, outward along r (units of c).
// A hovering camera moves outward at beta = sqrt(r_s / r) relative to the falling one; |v| < 0.95 keeps
// it a real observer, so near and inside the horizon the camera is forced into (almost) free fall.
float camRadialVel(float dist, float fall) {
    float beta = std::sqrt(std::max(P.gravity, 0.0f) / std::max(MIN_DIST, dist));
    return std::min(0.95f, (1.0f - fall) * beta);
}

// ---- Kerr-Schild helpers, same conventions as the shader (M = r_s / 2, spin along world -y)
V3 toStd(V3 v) { return {v.x, v.z, -v.y}; }
float kerrM() { return 0.5f * std::max(P.gravity, 0.0f); }
float ksField(V3 x, float& f, V3& l) {
    float M = kerrM(), a = P.spin * M, a2 = a * a, b = dot(x, x) - a2;
    float r2 = std::max(0.5f * (b + std::sqrt(b * b + 4.0f * a2 * x.z * x.z)), 1e-10f), r = std::sqrt(r2);
    float w = r2 + a2;
    l = {(r * x.x + a * x.y) / w, (r * x.y - a * x.x) / w, x.z / r};
    f = 2.0f * M * r2 * r / (r2 * r2 + a2 * x.z * x.z);
    return r;
}
// camera speed along l relative to the Kerr-Schild normal observer (see the shader's kerrMain)
float kerrCamSpeed(float f, float fall) {
    float sf = std::sqrt(f);
    return std::max(-0.95f, std::min(0.95f, (1.0f - fall) * f - fall * sf / (1.0f + sf + f)));
}
// innermost stable circular orbit (prograde), outer and inner horizon, in r_s
float iscoRs(float chi) {
    float z1 = 1.0f + std::cbrt(1.0f - chi * chi) * (std::cbrt(1.0f + chi) + std::cbrt(1.0f - chi));
    float z2 = std::sqrt(3.0f * chi * chi + z1 * z1);
    return 0.5f * (3.0f + z2 - std::sqrt((3.0f - z1) * (3.0f + z1 + 2.0f * z2)));
}
float horizonRs(float chi, float sign) { return 0.5f * (1.0f + sign * std::sqrt(std::max(0.0f, 1.0f - chi * chi))); }

// frequency shift the camera sees straight ahead (same maths as the shader), drives auto exposure
float viewShift(const CamState& c) {
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
float autoExposure(const CamState& c) {
    float q = std::max(0.1f, std::min(10.0f, viewShift(c)));
    return std::pow(q, -2.5f * P.autoExposure * P.obsShift);
}

float camRadius(const CamState& c) {
    if (P.spin <= 0.0f) return std::max(MIN_DIST, c.dist);
    float f; V3 l;
    return ksField(toStd(camBasis(c).pos), f, l);
}

float horizonRadius() { return horizonRs(P.spin, 1.0f) * std::max(P.gravity, 0.0f); }

// what the HUD's CLOCKS line shows: a hovering observer here ticks slow by sqrt(1 - r_s/r) (Kerr: sqrt(1 - f)),
// and the camera's speed relative to it adds the special-relativistic factor. Inside the horizon (and in the
// ergosphere) nothing can hover, so there is no outside clock to compare with.
float farClockRate(const CamState& c) {
    if (P.spin > 0.0f) {
        float f; V3 l;
        ksField(toStd(camBasis(c).pos), f, l);
        if (f >= 0.999f) return -1.0f;
        double s = kerrCamSpeed(f, c.fall), vrel = (s - f) / (1.0 - s * f);
        return (float)(1.0 / std::sqrt(1.0 - vrel * vrel) / std::sqrt(1.0 - f));
    }
    double r = std::max(MIN_DIST, c.dist);
    if (r <= 1.0005) return -1.0f;
    double beta = std::sqrt(1.0 / r), v = camRadialVel((float)r, c.fall);
    double vrel = (v - beta) / (1.0 - v * beta);
    return (float)(1.0 / std::sqrt(1.0 - vrel * vrel) / std::sqrt(1.0 - 1.0 / r));
}

// Drives the sub-bass and the camera shake. Nothing physical shakes (for a hole this big the tides at the
// horizon are gentle); this is the score marking the moment of no return: a growl on the way down, the
// full weight the instant we cross, fading as we fall on inside.
float horizonQuake(const CamState& c) {
    float x = camRadius(c) / std::max(horizonRadius(), 1e-3f);
    if (x >= 1.0f) { float b = smoothstepf(1.8f, 1.0f, x); return 0.35f * b * b; }
    return std::exp(-(1.0f - x) * 3.0f);
}

float diskProximity(const CamState& c) {
    if (!P.disk) return 0.0f;
    V3 p = camBasis(c).pos;
    float rho = std::sqrt(p.x * p.x + p.z * p.z);
    float dr = rho < P.diskIn ? P.diskIn - rho : (rho > P.diskOut ? rho - P.diskOut : 0.0f);
    float d2 = dr * dr + p.y * p.y;
    return 1.0f / (1.0f + d2 / 6.0f);
}
