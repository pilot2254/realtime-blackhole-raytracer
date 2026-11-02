// GARGANTUA: the default film. Camera keys, titles, chapters, captions and the HUD timing.
// Tuned for spin a = 0.6 M: the fall ends between the outer (0.90 r_s) and inner (0.10 r_s) horizon.
#include "film/sequence.h"

void defaultSequence() {
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
