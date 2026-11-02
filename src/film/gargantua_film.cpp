// GARGANTUA: the default film. Camera keys, scene objects, titles, chapters, captions and effect cues.
// Tuned for spin a = 0.6 M: the fall ends between the outer (0.90 r_s) and inner (0.10 r_s) horizon.
#include <cstdio>
#include "film/sequence.h"
#include "sim/scene.h"

static void defaultObjects() {
    g_objects.clear();

    SceneObject hot = makeObject(SO_HOTSPOT);              // the red spot riding the disk
    hot.orbitR = 5.0f; hot.phase = 0.5f; hot.speed = 0.22f; hot.size = 0.35f; hot.temp = 3300.0f; hot.bright = 2.4f;
    std::snprintf(hot.name, sizeof hot.name, "Hot Spot");
    g_objects.push_back(hot);

    // just outside the inner edge (ISCO 1.91 r_s), tilted a little above the disk; in view during act V
    SceneObject miller = makeObject(SO_PLANET);
    miller.orbitR = 2.4f; miller.size = 0.14f; miller.phase = 4.17f; miller.speed = 0.012f; miller.incline = 0.2f;
    miller.t0 = 100.0f; miller.t1 = 212.0f;
    std::snprintf(miller.name, sizeof miller.name, "Miller's Planet");
    g_objects.push_back(miller);

    // overtakes the camera over the disk in act IV, then pulls away ahead
    SceneObject ship = makeObject(SO_SHIP);
    ship.follow = true; ship.offset = {1.3f, 0.45f, -0.15f}; ship.size = 0.14f; ship.spin = 0.4f;
    ship.t0 = 89.0f; ship.t1 = 117.0f;
    std::snprintf(ship.name, sizeof ship.name, "Endurance");
    g_objects.push_back(ship);

    SceneObject jet = makeObject(SO_JET);                   // not part of the film; switch it on in the Outliner
    jet.visible = false;
    g_objects.push_back(jet);
}

void defaultSequence() {
    defaultObjects();
    // the galaxy sits in direction G; parking the camera at -G puts it right behind the hole
    const float galPhi = 1.8828f, galEl = 0.2019f;
    //        t      phi     el      dist    roll    fov    yaw    pitch  fall
    g_keys = {
        {  0.0f, {-0.95f,  0.04f, 260.0f,  0.00f, 0.30f,  0.0f,  0.00f, 0.0f}},  // I. a dot bending the stars
        { 12.0f, {-0.65f,  0.07f, 110.0f,  0.03f, 0.33f,  0.0f,  0.00f, 0.0f}},
        { 24.0f, {-0.20f,  0.09f,  40.0f, -0.05f, 0.40f,  0.0f,  0.00f, 0.0f}},
        { 34.0f, { 0.40f,  0.05f,  27.0f, -0.12f, 0.42f,  0.0f,  0.00f, 0.0f}},  // edge-on, the classic shot
        { 44.0f, { 0.95f,  0.42f,  25.0f, -0.05f, 0.44f,  0.0f,  0.00f, 0.0f}},  // II. rising
        { 54.0f, { 1.40f,  1.28f,  23.0f,  0.00f, 0.48f,  0.0f,  0.00f, 0.0f}},  // top-down whirlpool, the hot spot
        { 62.0f, { 1.70f,  0.62f,  30.0f,  0.04f, 0.42f,  0.0f,  0.00f, 0.0f}},
        { 70.0f, {galPhi, galEl,   42.0f,  0.00f, 0.30f,  0.0f,  0.00f, 0.0f}},  // III. Einstein ring, then EHT
        { 72.0f, {galPhi + 0.01f, galEl + 0.003f, 41.5f, 0.00f, 0.30f, 0.0f, 0.00f, 0.0f}},
        { 75.5f, {galPhi + 0.02f, galEl + 0.006f, 41.0f, 0.00f, 0.20f, 0.0f, 0.00f, 0.0f}},  // zoom in for the telescope view
        { 80.0f, {galPhi + 0.04f, galEl + 0.010f, 40.0f, 0.00f, 0.20f, 0.0f, 0.00f, 0.0f}},
        { 88.0f, { 2.55f,  0.20f,  15.0f, -0.12f, 0.52f, -0.30f,  0.00f, 0.0f}},  // IV. down to the disk
        { 98.0f, { 3.40f,  0.11f,   9.0f, -0.22f, 0.62f, -0.80f, -0.04f, 0.0f}},  // skimming, the Endurance overtakes
        {108.0f, { 4.35f,  0.10f,   7.5f, -0.18f, 0.62f, -0.65f, -0.03f, 0.0f}},
        {118.0f, { 5.00f,  0.14f,   5.8f, -0.08f, 0.55f, -0.35f, -0.02f, 0.0f}},  // V. Miller's planet
        {126.0f, { 5.30f,  0.10f,   4.2f, -0.05f, 0.55f, -0.25f,  0.00f, 0.0f}},
        {134.0f, { 5.48f,  0.09f,   3.5f,  0.00f, 0.55f, -0.15f,  0.00f, 0.0f}},
        {142.0f, { 5.58f,  0.12f,   3.6f,  0.02f, 0.55f,  0.00f,  0.00f, 0.0f}},
        {148.0f, { 5.62f,  0.20f,   3.4f,  0.02f, 0.52f,  0.00f,  0.00f, 1.0f}},  // VI. let go: free fall
        {160.0f, { 5.72f,  0.30f,   2.5f,  0.04f, 0.53f,  0.00f,  0.00f, 1.0f}},  // past the inner edge
        {172.0f, { 5.85f,  0.34f,   1.6f,  0.08f, 0.58f,  0.30f,  0.05f, 1.0f}},  // photon orbits
        {180.0f, { 5.95f,  0.36f,   1.0f,  0.10f, 0.62f,  0.65f,  0.08f, 1.0f}},  // event horizon
        {186.0f, { 6.00f,  0.37f,   0.75f, 0.08f, 0.65f,  1.05f,  0.10f, 1.0f}},
        {194.0f, { 6.03f,  0.38f,   0.50f, 0.02f, 0.70f,  1.65f,  0.10f, 1.0f}},  // turn to the band of sky
        {201.0f, { 6.05f,  0.38f,   0.32f, 0.00f, 0.75f,  1.75f,  0.05f, 1.0f}},  // nearing the inner horizon
        {238.0f, { 6.05f,  0.38f,   0.32f, 0.00f, 0.75f,  1.75f,  0.05f, 1.0f}},  // end credits on black
    };
    g_cues = {
        {  2.5f,  11.0f, CUE_TITLE,    "GARGANTUA"},
        {  4.5f,  11.0f, CUE_SUBTITLE, "a journey past the point of no return"},
        { 13.0f,  20.5f, CUE_CAPTION,  "A black hole of 100 million suns. Its event horizon is wider than the orbit of the Earth."},
        { 24.0f,  31.0f, CUE_CHAPTER,  "I   ·   THE SHADOW"},
        { 27.0f,  34.5f, CUE_CAPTION,  "The glow above and below the shadow is the far side of the disk, its light bent around the hole by gravity."},
        { 37.0f,  44.5f, CUE_CAPTION,  "One side of the disk rushes toward us at nearly half the speed of light — brighter and bluer. The other side recedes."},
        { 46.0f,  53.0f, CUE_CHAPTER,  "II   ·   THE WHIRLPOOL"},
        { 48.5f,  55.5f, CUE_CAPTION,  "The red spot is a hot spot: a bubble of superheated gas riding the disk. Telescopes have watched them circle Sagittarius A*."},
        { 56.0f,  62.5f, CUE_CAPTION,  "Gargantua spins. It drags space around with it, so its shadow is lopsided and the gas can orbit closer in."},
        { 63.0f,  70.0f, CUE_CHAPTER,  "III   ·   THE RING"},
        { 65.0f,  71.5f, CUE_CAPTION,  "A galaxy sits exactly behind the black hole. Gravity bends its light into a perfect circle — an Einstein ring."},
        { 72.0f,  81.0f, CUE_EHT,      ""},
        { 73.0f,  80.0f, CUE_CAPTION,  "Through the Event Horizon Telescope, an Earth-sized radio dish, it blurs into a glowing ring, brighter where the gas comes toward us."},
        { 82.0f,  89.0f, CUE_CHAPTER,  "IV   ·   THE DISK"},
        { 84.0f, 200.5f, CUE_HUD,      ""},
        { 90.0f,  97.0f, CUE_CAPTION,  "Skimming the disk. The gas below circles the hole once every few days, faster the closer it gets."},
        { 99.0f, 106.0f, CUE_CAPTION,  "A ship rides above the disk: twelve modules on a turning ring, the way the Endurance made its own gravity."},
        {109.0f, 116.5f, CUE_CAPTION,  "The inner edge of the disk. The spin lets gas orbit closer than it could around a still black hole, but no closer than this."},
        {119.0f, 126.0f, CUE_CHAPTER,  "V   ·   MILLER'S PLANET"},
        {122.0f, 129.5f, CUE_CAPTION,  "An ocean world on the closest stable orbit, lit by the disk."},
        {131.0f, 140.0f, CUE_CAPTION,  "Down there, one hour lasts an hour and a half far away. In the film the hole spins within a hundred-trillionth of the limit, and that hour is seven years."},
        {145.0f, 152.0f, CUE_CHAPTER,  "VI   ·   THE FALL"},
        {147.0f, 154.5f, CUE_CAPTION,  "We stop fighting gravity and let go. From here on, we are in free fall."},
        {158.0f, 165.0f, CUE_CAPTION,  "Our own speed squeezes the view ahead and makes it brighter. Light from behind us turns red."},
        {166.5f, 173.5f, CUE_CAPTION,  "Here light itself can orbit the hole: closer on the side turning with it, farther on the side turning against it."},
        {176.5f, 180.5f, CUE_CAPTION,  "The ergosphere. Space is dragged around so fast that nothing here can stand still."},
        {181.0f, 186.5f, CUE_CAPTION,  "The event horizon. We cross it and feel nothing. For a black hole this large, the tides are gentle."},
        {187.0f, 192.5f, CUE_CAPTION,  "Inside a spinning black hole the way down leads to a second, inner horizon."},
        {193.5f, 200.0f, CUE_CAPTION,  "What lies beyond it, no theory can yet tell us."},
        {203.0f, 211.0f, CUE_TITLE,    "GARGANTUA"},
        {203.5f, 211.0f, CUE_SUBTITLE, "every pixel is a ray of light traced through curved spacetime"},
        // end credits: subtitle cards, '|' breaks lines and the first line is the heading
        {212.5f, 220.0f, CUE_SUBTITLE, "MADE OF NOTHING BUT MATHEMATICS | written in C++ and OpenGL, computed live on the GPU"
                                       " | no game engine · no textures · no 3D models · no recorded sound"},
        {221.0f, 228.5f, CUE_SUBTITLE, "THE PHYSICS | light traced backwards through the spacetime of a spinning black hole"
                                       " | Kerr geometry · Doppler beaming · gravitational redshift · frame dragging"},
        {229.5f, 237.0f, CUE_SUBTITLE, "CREATED BY | Michal Flaška, with Claude by Anthropic"
                                       " | inspired by Interstellar and the science of Kip Thorne"},
    };
    g_fadeIn = 3.0f; g_fadeOut = 0.25f; g_blackAt = 201.0f;
}
