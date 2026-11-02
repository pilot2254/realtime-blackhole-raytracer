**For video export, install ffmpeg** (`winget install ffmpeg`). Frames get piped straight into an H.264 MP4. Without ffmpeg, it writes BMP frames instead.

**The film: GARGANTUA (3 min 6 s)**

The app starts by playing it in a loop, letterboxed to 2.39:1 with titles, chapter cards and captions:
1. **The shadow.** Title card over deep space, where a tiny black hole bends the stars. Then the approach into the classic edge-on view.
2. **The whirlpool.** Rise to a top-down shot of the whole swirling disk.
3. **The ring.** A distant spiral galaxy lines up exactly behind the hole, and gravity bends it into a complete **Einstein ring**.
4. **The disk.** Descend to skim just above the disk, looking ahead as the gas streams past. An instrument HUD fades in.
5. **The fall.** Turn to face the hole and let go. Free fall past the photon orbits, through the ergosphere, across the event horizon, and down toward the inner horizon. Cut to black, then the end card.

**Gargantua spins**

Like the film's black hole, Gargantua rotates, with spin a = 0.6 M (the value *Interstellar*'s renders used). Set **Spin** to anything above 0 and a full Kerr ray tracer takes over:
- Light rays follow exact null geodesics in Cartesian Kerr–Schild coordinates (Hamilton's equations, RK4), which stay smooth through the horizon.
- The spin drags space around, so the shadow is lopsided, and the disk's inner edge moves in to the spinning ISCO (1.91 r_s at a = 0.6, instead of 3).
- The disk's colour shift is exact for gas on circular Kerr orbits.
- Inside a spinning hole there is a second, inner horizon (0.10 r_s at a = 0.6). Physics as we know it fails there, so the film stops short of it.

At spin 0 the faster Schwarzschild tracer is used. Both tracers produce the same frames at zero spin, inside the horizon too, which cross-checks two independent sets of equations.

**The physics of the fall (no spin)**

The camera is a free-falling observer. Inside the horizon, a hovering ("static") camera can't exist, so the ray tracer works in the free-falling frame instead (Painlevé–Gullstrand coordinates). In that frame space is flat, and a light ray's path is its local direction plus the fall velocity √(r_s/r). The maths is exact at every radius, including inside the horizon. What you see along the way:
- **Aberration:** the view ahead is squeezed and brightened.
- **Your own Doppler and gravitational shift:** light ahead turns blue, light behind turns red.
- **A black cone:** inside the horizon, light from straight "below" can't have come from our universe.

The HUD shows the real numbers for a 100 million solar-mass black hole: distance, speed relative to a hovering observer, clock rate compared to far away, and head-to-toe tides. Without spin it also shows the proper time left before the singularity; with spin it shows the spin and both horizons, and it says when you are in the ergosphere, where nothing can stay still.

**What the UI controls**

- **Camera:** Free orbit or sequence playback. Free orbit has look yaw/pitch (to look away from the hole) and a **free fall** slider (0 = hovering on rockets, 1 = falling from far away). If you drag the viewport during playback, it switches to free orbit and keeps the exact current view.
- **Physics:** A **gravity slider** (at 0, light travels straight; at 2, lensing is exaggerated), plus Doppler and redshift strength, disk radii, temperature, brightness, opacity, and swirl speed.
  - **Spin (a/M)** from 0 to 0.998, and **Disk starts at the ISCO**, which keeps the inner edge where physics puts it.
  - **Observer shift** controls the camera's own blue/redshift.
  - **Auto exposure** stops the lens down when everything ahead is blueshifted.
  - Presets: "Physically based" and "Interstellar look" (the film turned the Doppler effect off).
- **Sky / Post:** Separate strength sliders for each sky layer. Exposure, bloom, vignette, chromatic aberration, and grain.
- **Sequencer:** To make your own shot, frame it in free orbit, move the playhead, and click "Add key". Keys can be retimed, jumped to, overwritten, or deleted. The camera moves along smooth spline curves between keys. "Black at" sets when the picture cuts to black. The sequence, with its cues, saves to and loads from `sequence.txt`.
- **Film:** Letterbox on/off and its aspect ratio. Titles/captions/HUD on/off. The black hole mass used for the HUD's real-world units. A cue editor for every title, chapter card, caption and HUD timing.
- **Render video:** 720p up to 4K, at 24, 30, or 60 fps. "Samples / frame" averages sub-frames over the shutter interval, which gives real motion blur plus antialiasing. Titles, captions and the HUD are rendered into the video.
  - Rendering is split into tiles, so heavy 4K frames won't crash your GPU driver, and the UI stays responsive with a progress bar and an estimated time remaining.
  - There's also a one-click **Screenshot** of the current view at export resolution.

**Recommended settings for a final render:** 1080p or 4K, 60 fps, 16 samples, shutter 0.5, Ultra geodesics on. That's 11,160 frames × 16 samples, so try a 720p render with 4 samples first to check the timing.

**Stills from the command line:** `gargantua_gl.exe --stills 30,72,163 --res 1 --samples 8` saves `still_<time>.png` for each film time and quits (`--res` 0–3 = 720p…4K, `--sequence file.txt` to use your own sequence, `--spin 0.9` to change the spin, `--prefix name_` for the file names).
