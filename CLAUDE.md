# CLAUDE.md

Gargantua Studio: a real-time black hole ray tracer (Schwarzschild and Kerr) with an Unreal-style ImGui editor, a camera sequencer, a physics-driven synthesised soundtrack, and offline video export. C++17, OpenGL 3.3 (own loader), Dear ImGui docking branch (vendored in `imgui/`). **Windows only.** The readme explains the physics for humans; this file is for working on the code.

## Build and run

- `build.bat` builds `gargantua_gl.exe` in the repo root (MSVC if `cl` is on PATH, else MinGW `g++`; about a minute). From PowerShell: `& .\build.bat`. From Git Bash `cmd //c build.bat` may fail to find it; use PowerShell or call g++ directly.
- New `.cpp` files must be added to `build.bat`'s `APP` list (both compilers use it).
- `build.bat` must keep CRLF line endings (`.gitattributes` pins `*.bat`); `cmd` breaks on `^` continuations with LF.
- The exe, `renders/`, `build/`, `gargantua_ui.ini` and `sequence.txt` are git-ignored. `showcase/` (readme images) is tracked.

## Testing without the GUI

Prefer headless checks; the user is often at the machine.
- `gargantua_gl.exe --stills 30,134 --res 0 --samples 1 --prefix t_` renders film frames to `renders/t_*.png` and quits. Use `--res 1 --samples 8` for showcase quality. Combine into contact sheets with ffmpeg (`tile`, `xstack`), then view them.
- `--sequence file.txt` loads a test scene (same format as File > Save, including `obj ...` lines), `--spin 0.0001` forces the Kerr tracer.
- `--soundtrack file.wav` writes the film's audio (about 3 s). Analyse it with plain Python (`wave`, no numpy installed). For synth unit tests, link a small harness against `src/audio/synth.cpp src/sim/*.cpp src/film/*.cpp src/app/app.cpp`.
- `--render` renders the film to video; `--at 134` opens the editor paused at that film time.
- In batch modes (`--stills`, `--render`) `fatal()` writes `renders/error_log.txt` instead of a MessageBox. GLSL compile errors show up there.
- To check the UI, capture **only the app's own window** with `PrintWindow` (PowerShell + user32). Never full-screen captures (they can catch the user's private windows), and never synthesise mouse clicks (they can land in other apps).
- Do not delete or overwrite files in `renders/` that you did not create this session; it holds the user's renders.

## Layout

| Path | Role |
| --- | --- |
| `src/main.cpp` | window, WndProc (keys: Space, P freeze, Tab cinema, F11, F12), CLI, frame loop. The scene is rendered before the UI each frame, at the viewport size from the previous frame. |
| `src/core/` | `math.h` (V3), `gl.h/.cpp` (X-macro GL loader, targets, uniforms, `fatal`), `util` (fmt, timing, `widen`). |
| `src/sim/` | `params.h` (global `Params P`), `camera` (CamState → CamFrame), `physics` (CPU mirror of the shader's relativity), `scene` (user objects: hot spot, planet, ship, jet). |
| `src/render/` | `shaders_scene.cpp` (scene shader, three parts A/B/C compiled together), `shaders_post.cpp` (bloom, composite incl. EHT view and letterbox), `renderer`, `refine` (progressive refinement), `overlay` (GDI text: titles, captions, HUD), `export` (tiles, sub-frame motion blur, ffmpeg pipe). |
| `src/film/` | `sequence` (keys, cues, fades, save/load, `cueLevel`), `gargantua_film.cpp` (the default film and default scene objects, as data). |
| `src/audio/` | `synth` (organ wavetables, clock, roar, hum, ocean, coda, reverb; `soundControls`, `filmSound`), `audio` (waveOut live playback, offline WAV). |
| `src/editor/` | `theme.cpp` (UE5 colours, drawn icons, Details property grid), `panels.cpp` (Outliner, Details, Movie Render Queue), `sequencer.cpp`, `editor.cpp` (menu, toolbar, status bar, dock layout, viewport). |
| `src/app/` | shared app state (camera mode, window, status) and `currentCam/grabCamera`. |

## Physics conventions (keep the shader and `sim/physics.cpp` in sync)

- Units: Schwarzschild radius r_s = 1, so M = 0.5, G = c = 1. World y is up; the disk lies in y = 0 and orbits toward +phi.
- Schwarzschild tracer: Binet equation as a Cartesian pseudo-force, leapfrog. Camera = Painlevé–Gullstrand free-faller boosted by `uCamVel`; exact inside the horizon.
- Kerr tracer: Hamilton's equations in Cartesian Kerr–Schild coordinates, RK4, integrated backwards with p_t = -1. "std" coordinates put the spin on +z: `toStd(v) = (x, z, -y)`. Spin a = chi * M; `P.spin` is chi (film uses 0.6). At spin 0 the Schwarzschild tracer runs; both must give the same image (validate with `--spin 0` vs `--spin 0.0001`).
- `CamState` = phi, el, dist, roll, fov (tan of half vertical FOV), yaw, pitch, fall (0 hovering .. 1 free fall). Film keys use exactly this order.
- Scene objects reach the shader as up to 8 × 4 vec4 uniforms (`uObjA/B/C/V`) and are tested per ray segment in `objectsAlong`. Follow-mode ships fly the film camera path via `g_filmCameraAt`.
- Photon-ring handling is deliberate: at most two disk images, a per-ray glow budget, winding-based smoothing (`gRing`), and the sky faded where `fwidth(dir)` is huge. Don't remove these without re-checking 1080p crops of the shadow edge (dots and dashes come back).
- GLSL gotcha: some ordinary words are reserved (`coherent`, `sample`, `filter`, ...); a reserved name fails only at runtime.

## Film and sound

- The film (`gargantua_film.cpp`) is about 238 s: hot spot, EHT cue, Endurance in act IV, Miller's planet in act V, the fall in act VI, cut to black at `g_blackAt` (201 s), then end credits (subtitle cues; `|` breaks lines, the first line is the heading). The music must never follow the picture's fade-out to the cut (only the fade-in); after the cut `filmSound` rings out over the credits. Objects with a time window fade in/out (`objectPresence`, partly see-through in the shader) and should still leave the frame on their own before `t1` (the follow ship climbs away). Captions quote numbers from the physics (clock rates, time dilation, horizons); if you move keys, re-check them with stills plus the HUD.
- The soundtrack is a deterministic function of the camera path (`filmSound(t)`): the same in live playback and in the export. After the cut the coda resolves to A minor and decays; keep it from cutting hard.
- The epic layers are derived from `tension` inside `Synth::render`: an organ ostinato (`ost`, rate steps at tension 0.45 and 0.8) and brass braams with a dry sine sub (`epic`, on chord changes, half bars once epic > 0.75). As `epic` rises the constant low bed (pedal, drone) and the pad step back so the impacts own the low end. A limiter sits before the soft clip; check levels with a band analysis of `--soundtrack` output. The user does not want camera shake, ducking/muting before a hit, or a single horizon boom.
- Captions and the HUD are text cues; `CUE_HUD` and `CUE_EHT` are effect cues without text.

## Editor

- The dock layout is built in `drawDockspace` under the ID `GargantuaDock_vN`. Bump N when renaming or adding docked windows, or old `gargantua_ui.ini` layouts leave them floating. A docked window drawn last on its first frame becomes the front tab.
- Details rows use `beginProps`/`propFloat`/... (label left, value right). Match that style for new settings.

## Working with this user

- Windows; Git Bash and PowerShell. The user is not a physicist: explain physics plainly, and keep the captions honest (say when something is cinematic licence).
- Commit finished, verified work without asking (the user allowed auto-commits); never push unless asked. The user works directly on `main`. Keep commit messages descriptive, with the Co-Authored-By trailer.
- Shell heredocs in this environment mangle backslash escapes (`\n`, `\\`) inside generated code. For edits containing escapes, use the Edit tool or write a Python script with the Write tool.
