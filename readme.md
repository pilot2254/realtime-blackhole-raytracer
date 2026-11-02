# Gargantua Studio

A real-time black hole ray tracer for Windows, written in C++ with OpenGL and Dear ImGui. Every pixel follows a ray of light through curved spacetime, so the lensing, the lopsided shadow, the Einstein ring and the view from inside the horizon all come out of the maths. There are no textures or tricks. It comes with an Unreal-style editor, a camera sequencer, and a short film, **GARGANTUA**, that orbits the black hole, skims its disk and falls in.

![Gargantua, edge-on](showcase/gargantua_20261102_134653.png)

## How this was made

This project is mostly AI-generated. This morning I decided to test what Claude can do and see how good it is with math, and to compare it with how good it was a few years ago. I was blown away by the results, so I decided to publish it. My earlier Claude benchmark projects are in [cpp_projects/claude-test](https://github.com/pilot2254/cpp_projects/blob/main/claude-test/).

The images below were rendered with the app.

## Gallery

| | |
| --- | --- |
| ![Edge-on view](showcase/gargantua_20261102_134653.png) | ![Inside the fall](showcase/gargantua_20261102_143630.png) |
| The classic edge-on shot: the far side of the disk is bent over and under the shadow. | Falling toward the horizon: the disk's light wraps around the shadow, blueshifted by the fall. |
| ![The ergosphere](showcase/chk_152.00.png) | |
| The fall through the ergosphere, with the instrument HUD and a caption. | |

**Stills from the film, spinning black hole (a = 0.6):**

![Film stills](showcase/film_preview_kerr_early.png)

**The fall, from the photon orbits to the inner horizon:**

![The fall](showcase/film_preview_kerr_fall.png)

## Features

- **Two exact ray tracers:**
  - **Schwarzschild** (still black hole): photon paths from the Binet equation.
  - **Kerr** (spinning black hole, like the one in *Interstellar*): Hamilton's equations in Kerr–Schild coordinates.
  - At zero spin both produce the same frames, which cross-checks two independent sets of equations.
- **A camera that can fall in:** a free-falling observer whose maths stays exact inside the horizon, with relativistic aberration and its own Doppler and gravitational shift.
- **An accretion disk** with an inner edge at the innermost stable orbit, blackbody colours, Doppler beaming and gravitational redshift.
- **A procedural sky:** stars, the Milky Way, nebulae, and a galaxy that becomes an Einstein ring.
- **Scene objects you can add and delete:** hot spots, planets (Miller's planet), ships (the Endurance) and relativistic jets. They're traced along the same bent rays, so they get lensed and Doppler shifted too.
- **Event Horizon Telescope view:** the image as an Earth-sized radio telescope would see it, in the colours of the real M87* and Sagittarius A* photos.
- **A soundtrack generated from the physics:** an organ that builds as you near the horizon, a clock that ticks with time dilation and stops at the horizon, a ship's engines and a planet's ocean. It plays live and renders into the video.
- **Progressive refinement:** a still view sharpens itself to full resolution with 64-sample antialiasing.
- **Film tools:** a sequencer with spline camera keys, titles and captions, a physics HUD in real units, letterbox and fades.
- **Video export** up to 4K with real motion blur and antialiasing, piped into ffmpeg. Renders are split into tiles so heavy frames never crash the GPU driver.

## Requirements

- Windows and a GPU with OpenGL 3.3.
- **For video export, install ffmpeg** (`winget install ffmpeg`). Frames get piped straight into an H.264 MP4. Without ffmpeg, it writes BMP frames instead.

## The film: GARGANTUA (3 min 32 s)

The app starts by playing it in a loop, letterboxed to 2.39:1 with titles, chapter cards and captions:
1. **The shadow.** Title card over deep space, where a tiny black hole bends the stars. Then the approach into the classic edge-on view.
2. **The whirlpool.** Rise to a top-down shot of the whole swirling disk, with the red **hot spot** riding it.
3. **The ring.** A distant spiral galaxy lines up exactly behind the hole, and gravity bends it into a complete **Einstein ring**. Then the view fades into what the **Event Horizon Telescope** would see.
4. **The disk.** Descend to skim just above the disk as the **Endurance** overtakes the camera. An instrument HUD fades in.
5. **Miller's planet.** An ocean world on the closest stable orbit, lit by the disk.
6. **The fall.** Turn to face the hole and let go. Free fall past the photon orbits, through the ergosphere, across the event horizon, and down toward the inner horizon. Cut to black, then the end card.

## Scene objects

Add them with **+ Add** in the Outliner (or right-click its empty space). Right-click an object to rename, duplicate or delete it, and use the eye icon to hide it. Up to 8 visible objects are traced at once. Each has its own settings in Details, and a film window: while the film plays it only shows between two times. Objects are saved with the sequence.
- **Hot Spot:** a bubble of superheated gas on an orbit. Flares like this have been observed circling Sagittarius A*, the black hole at the centre of our galaxy. It is Doppler boosted by its real orbital speed, so it brightens on the side coming toward you, and you can see its lensed second image chase it around the ring.
- **Planet:** an ocean world lit by the disk, with long swells, drifting cloud bands, a sun glint and a thin glowing atmosphere. Details shows its real time dilation for the current spin. On Miller's orbit at a = 0.6, one hour on the surface is about 1 h 31 min far away. *Interstellar*'s seven years per hour needs a black hole spinning within a hundred-trillionth of the maximum.
- **Ship:** the Endurance, twelve modules on a spinning ring around a hub, with lit windows. It can orbit the hole or fly the film camera's own path through space, a little ahead of it: it overtakes from behind when its film window opens and pulls away ahead before it closes. In the free camera it waits at its stretch of the path, so you can fly over to it.
- **Relativistic Jet:** plasma shot out along the spin axis at a chosen speed. Relativistic beaming makes the jet coming toward you outshine the one going away. It's in the scene but switched off, because Gargantua in the film has none.

Planet and ship sizes are cinematic: a real planet would be a speck next to a black hole of 100 million suns.

## The Event Horizon Telescope view

In 2019 and 2022 the Event Horizon Telescope, a network of radio dishes as wide as the Earth, photographed the black holes M87* and Sagittarius A*. They appear as a blurred orange ring around a dark centre, brighter on one side, because the gas there moves toward us (Doppler beaming). The telescope view (Post Process in Details, or the **EHT** button in the viewport) blurs the image to roughly that resolution, darkens the radio sky, and uses the same colour map. The film fades into it during act III with a **Telescope view** cue, which you can add to your own sequences too.

## Gargantua spins

Like the film's black hole, Gargantua rotates, with spin a = 0.6 M (the value *Interstellar*'s renders used). Set **Spin** to anything above 0 and a full Kerr ray tracer takes over:
- Light rays follow exact null geodesics in Cartesian Kerr–Schild coordinates (Hamilton's equations, RK4), which stay smooth through the horizon.
- The spin drags space around, so the shadow is lopsided, and the disk's inner edge moves in to the spinning ISCO (1.91 r_s at a = 0.6, instead of 3).
- The disk's colour shift is exact for gas on circular Kerr orbits.
- Inside a spinning hole there is a second, inner horizon (0.10 r_s at a = 0.6). Physics as we know it fails there, so the film stops short of it.

At spin 0 the faster Schwarzschild tracer is used.

The spin also pushes the shadow sideways, toward the side of the disk that comes toward you (by roughly half a Schwarzschild radius at a = 0.6). That's why a relativistic jet, which comes out of the true spin axis, doesn't line up with the middle of the shadow. At spin 0 the two line up exactly.

The thin bright line hugging the shadow is the photon ring: light that circled the hole before reaching you. It's far thinner than a pixel, so the tracer draws it with the disk's average brightness instead of its streaks, and leaves out the even thinner rings inside it; otherwise it would only sparkle.

## The physics of the fall (no spin)

The camera is a free-falling observer. Inside the horizon, a hovering ("static") camera can't exist, so the ray tracer works in the free-falling frame instead (Painlevé–Gullstrand coordinates). In that frame space is flat, and a light ray's path is its local direction plus the fall velocity √(r_s/r). The maths is exact at every radius, including inside the horizon. What you see along the way:
- **Aberration:** the view ahead is squeezed and brightened.
- **Your own Doppler and gravitational shift:** light ahead turns blue, light behind turns red.
- **A black cone:** inside the horizon, light from straight "below" can't have come from our universe.

The HUD shows the real numbers for a 100 million solar-mass black hole: distance, speed relative to a hovering observer, clock rate compared to far away, and head-to-toe tides. Without spin it also shows the proper time left before the singularity; with spin it shows the spin and both horizons, and it says when you are in the ergosphere, where nothing can stay still.

## The soundtrack

There's no recorded music. The soundtrack is synthesised from the camera's state, so it follows exactly what the physics is doing:
- **Distance from the horizon builds the organ.** Far away it's one quiet chord, a slow progression in A minor in a big cathedral reverb. Closer in, it adds a deep pedal note, then an octave doubling, then a high shimmer, and swells from about −26 dB to −9 dB.
- **Time dilation drives a ticking clock:** a soft knock, once per second of far-away time, the same number the HUD shows. Far out it ticks once a second. During the fall it speeds up to 7 and then 30 ticks a second, because your clock runs that much slower. It stops dead when no outside clock exists any more: in the ergosphere and inside the horizon.
- **The disk** adds a low roar when you're close to it, and the roar brightens with the Doppler shift you see straight ahead.
- **Scene objects:** a ship nearby adds a low, slowly throbbing engine hum; a planet nearby adds the swell of its ocean.
- **The ending:** at the cut to black the clock, roar, hum and ocean stop with the picture, but the music doesn't. The organ resolves home to A minor and rings out in the hall over the end card.

It plays live in the editor: fly the free camera toward the hole and listen. It also renders into videos sample-exact, and is saved as a WAV next to the video. The speaker button on the toolbar mutes it; the volume is under Film in Details.

## The editor

The app opens as an editor laid out like Unreal Engine 5:
- **Toolbar:** save/open, a Film / Free Camera switch, transport (start, play/pause, stop, end, loop), **freeze time** (P) and playback speed. On the right: **Scalability**, Screenshot, Render and Cinema.
- **Viewport:** the live image. Left-drag orbits, right-drag looks around, and the wheel moves closer or farther. Grabbing the view during the film switches to the free camera at the exact current shot. The buttons in its top-left corner switch the camera, the black hole (Kerr / Schwarzschild), the quality and the letterbox.
- **Outliner:** the scene's actors: Gargantua, Accretion Disk, Sky, Camera, Post Process and Film, plus the objects you add (hot spots, planets, ships, jets). **+ Add** creates one; right-click to rename, duplicate or delete; the eye icons show or hide.
- **Details:** the properties of whatever is selected, in collapsible categories. Select something in the Outliner or the Sequencer to edit it here.
- **Sequencer:** the film as tracks: camera keys (orange = hovering, blue = free fall), titles & chapters, captions, and the HUD & effects (HUD, telescope view). Click a key or a clip to edit it in Details, drag a key to retime it, and click or drag anywhere else to scrub. **K** adds a key from the current view and **Delete** removes the selection. The camera moves along smooth spline curves between keys.
- **Movie Render Queue:** video settings and the **Render Film** button. You can choose 720p up to 4K, at 24, 30 or 60 fps. "Samples per frame" averages sub-frames over the shutter interval, which gives real motion blur plus antialiasing. Titles, captions and the HUD are rendered into the video.
- **Status bar:** messages on the left. On the right: the tracer, the preview resolution and quality (or the refinement progress), and the fps.
- **Cinema mode (Tab):** the image fills the window with no UI. Tab or Esc brings the editor back.

Drag any panel's tab to dock it somewhere else. Window > Reset Layout restores the default.

The preview always starts on **Low** quality, so it runs on any GPU. Raise it with Scalability (Low, Medium, High, Ultra). Video renders and screenshots always use full quality, whatever the preview is set to.

**Progressive refinement:** when the picture stops changing (the film paused, or time frozen with the hourglass button or P), the viewport re-traces it at full resolution with Ultra geodesics and sub-pixel jitter. It traces a few tiles per frame so the editor stays responsive, and averages up to 64 samples. Then it stops tracing, so the GPU goes idle. The status bar shows "Refining 12 / 64". It's on by default; turn it off in the viewport's quality menu.

## Renders

Everything the app renders goes into the `renders\` folder next to the exe, which is created when needed: videos, screenshots (F12), command-line stills, BMP frames when ffmpeg is missing, and `ffmpeg_log.txt`. File > Open Renders Folder opens it.

**Recommended settings for a final render:** 1080p or 4K, 60 fps, 16 samples, shutter 0.5, Ultra geodesics on. That's 11,160 frames × 16 samples, so try a 720p render with 4 samples first to check the timing.

**Command line:**
- `--stills 30,72,163`: save `renders\still_<time>.png` for each film time, then quit.
- `--render`: render the film to video with the current settings, then quit (for batch renders).
- `--soundtrack file.wav`: write the film's soundtrack, then quit.
- `--at 90`: open the editor with the film paused at that time.

Options for the above: `--res 0`–`3` (720p up to 4K), `--fps 0`–`2` (24/30/60), `--samples 8`, `--sequence file.txt` (your own sequence), `--spin 0.9`, and `--prefix name_` for still file names.

## Building

Run `build.bat`, from an "x64 Native Tools Command Prompt for VS" for MSVC, or from any shell with MinGW `g++` on PATH. The source is split into modules:

| Folder | What's in it |
| --- | --- |
| `src/core` | Reusable in any project: vector maths, an OpenGL 3.3 loader with shader/target/uniform helpers, formatting and timing utilities. |
| `src/sim` | The camera, the scene parameters, scene objects (orbits, the chase ship, save/load), and CPU-side relativity (observer velocities, Kerr–Schild fields, horizons, ISCO, orbital time dilation, auto exposure). |
| `src/render` | The GLSL shaders, the render passes (scene, bloom, composite), progressive refinement, the GDI text overlay, and the video/screenshot exporter. |
| `src/film` | The sequencer engine (keys, cues, fades, save/load) and `gargantua_film.cpp`, the film itself as data. |
| `src/editor` | The editor: theme and widgets, Outliner/Details/Movie Render Queue, Sequencer, and the frame (menu, toolbar, status bar, dock layout, viewport). |
| `src/audio` | The physics-driven synthesiser, live playback (waveOut) and the offline soundtrack. |
| `src/app` | Application state shared by the modules. |
| `src/main.cpp` | Window, input and the frame loop. |
