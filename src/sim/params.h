#pragma once

// Everything about the black hole, disk, sky and lens. Units: Schwarzschild radius r_s = 1.
struct Params {
    float renderScale = 0.35f; bool ultra = false;       // real-time preview, = Scalability "Low"
    float gravity = 1.0f, doppler = 1.0f, redshift = 1.0f;
    bool  disk = true;
    float diskIn = 3.0f, diskOut = 16.0f, tempPeak = 8000.0f, diskBright = 1.25f, diskOpacity = 1.8f;
    float swirl = 1.0f, glow = 0.005f, obsShift = 1.0f, autoExposure = 1.0f;
    float spin = 0.6f; bool iscoEdge = true;            // a/M; Interstellar's renders used 0.6
    float stars = 1.0f, milky = 1.0f, nebula = 1.0f, galaxy = 1.0f;
    float exposure = 1.0f, bloom = 0.06f, vignette = 0.45f, ca = 0.010f, grain = 2.5f;
    float telescope = 0.0f;                             // 0..1 Event Horizon Telescope view
};
extern Params P;
