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
