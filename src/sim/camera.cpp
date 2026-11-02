#include "sim/camera.h"

CamFrame camBasis(const CamState& c) {
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
