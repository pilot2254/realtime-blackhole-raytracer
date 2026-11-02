#pragma once
// GLSL sources. The scene shader is split in three parts that are compiled together.
extern const char* VS_FULLSCREEN;
extern const char* FS_SCENE_A;      // uniforms, noise, blackbody
extern const char* FS_SCENE_B;      // sky: stars, Milky Way, nebulae, galaxy
extern const char* FS_SCENE_C;      // disk, Schwarzschild + Kerr ray tracers, camera
extern const char* FS_DOWN;         // bloom downsample
extern const char* FS_UP;           // bloom upsample
extern const char* FS_COMPOSITE;    // grading, letterbox, text overlay
extern const char* FS_COPY;
