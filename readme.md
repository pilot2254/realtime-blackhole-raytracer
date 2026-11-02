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

**The editor**

The app opens as an Unreal-style editor:
- **Menu bar:** File (open/save sequence, screenshot, render video), View (show/hide panels, reset layout, cinema mode, fullscreen), Help (controls).
- **Toolbar:** Film / Free camera switch, transport (start, play/pause, stop, end, loop), time, playback speed, the **Scalability** dropdown, and Screenshot / Render / Cinema buttons. While a video renders, the progress bar and Cancel button sit here.
- **Viewport:** the live image. Left-drag orbits, right-drag looks around, the wheel moves closer or farther. Grabbing the view during the film switches to the free camera at the exact current shot.
- **Panels:** Camera, Black hole, Sky & lens, Film, Sequencer, Render. Drag their tabs to dock them anywhere; View > Reset layout restores the default.
- **Cinema mode (Tab):** the image fills the window with no UI. Tab or Esc brings the editor back.

The preview always starts on **Low** quality, so it runs on any GPU. Raise it with Scalability (Low, Medium, High, Ultra). Video renders and screenshots always use full quality, whatever the preview is set to.

**What the panels control**

- **Camera:** Free camera position, lens, look yaw/pitch (to look away from the hole) and a **free fall** slider (0 = hovering on rockets, 1 = falling from far away).
- **Black hole:** A **gravity slider** (at 0, light travels straight; at 2, lensing is exaggerated), **spin (a/M)** from 0 to 0.998, Doppler, redshift and observer shift, plus the disk: inner edge at the ISCO, radii, temperature, brightness, opacity, swirl speed. Presets: "Physically based" and "Interstellar look" (the film turned the Doppler effect off).
- **Sky & lens:** Separate strength sliders for each sky layer. Exposure, auto exposure, bloom, vignette, chromatic aberration, and grain.
- **Film:** Letterbox on/off and its aspect ratio, titles/captions/HUD on/off, fades and the cut to black, and the black hole mass used for the HUD's real-world units.
- **Sequencer:** A timeline with the camera keys (orange = hovering, blue = free fall), lanes for titles, captions and the HUD, and the playhead; click or drag to scrub. Under it, the key list (add a key at the playhead from the current view, retime, jump, overwrite, delete) and the caption editor. The camera moves along smooth spline curves between keys. File > Save writes the sequence with its captions to a text file.
- **Render:** Preview quality, then video: 720p up to 4K, at 24, 30, or 60 fps. "Samples / frame" averages sub-frames over the shutter interval, which gives real motion blur plus antialiasing. Titles, captions and the HUD are rendered into the video.
  - Rendering is split into tiles, so heavy 4K frames won't crash your GPU driver, and the UI stays responsive.
  - There's also a one-click **Screenshot** (F12) of the current view at export resolution.

**Recommended settings for a final render:** 1080p or 4K, 60 fps, 16 samples, shutter 0.5, Ultra geodesics on. That's 11,160 frames × 16 samples, so try a 720p render with 4 samples first to check the timing.

**Stills from the command line:** `gargantua_gl.exe --stills 30,72,163 --res 1 --samples 8` saves `still_<time>.png` for each film time and quits (`--res` 0–3 = 720p…4K, `--sequence file.txt` to use your own sequence, `--spin 0.9` to change the spin, `--prefix name_` for the file names).
