**For video export, install ffmpeg** (`winget install ffmpeg`). Frames get piped straight into an H.264 MP4. Without ffmpeg, it writes BMP frames instead.

**The default animation (70 seconds)**

The app starts by playing it in a loop:
1. Fade in from deep space, with stars already warping around a tiny black hole.
2. Approach into the classic edge-on view.
3. Rise to a top-down shot of the whole swirling disk.
4. Swing around until the distant spiral galaxy is exactly behind the hole. Gravity bends the galaxy into a complete **Einstein ring** (bottom right of the preview).
5. Dive in and skim over the disk at 8 r_s.
6. Pull back out into space and fade out.

**What the UI controls**

- **Camera:** Free orbit or sequence playback. If you drag the viewport during playback, it switches to free orbit and keeps the exact current view.
- **Physics:** A **gravity slider** (at 0, light travels straight and you see the flat disk, as in the last preview frame; at 2, lensing is exaggerated), Doppler and redshift strength, disk radii, temperature, brightness, opacity, and swirl speed. There are also presets: "Physically based" and "Interstellar look" (the film turned the Doppler effect off).
- **Sky / Post:** Separate strength sliders for each sky layer. Exposure, bloom, vignette, chromatic aberration, and grain.
- **Sequencer:** To make your own shot, frame it in free orbit, move the playhead, and click "Add key". Keys can be retimed, jumped to, overwritten, or deleted, and the sequence saves to and loads from `sequence.txt`. The camera moves along smooth spline curves between keys.
- **Render video:** 720p up to 4K, at 24, 30, or 60 fps. "Samples / frame" averages sub-frames over the shutter interval, which gives real motion blur plus antialiasing.
  - Rendering is split into tiles, so heavy 4K frames won't crash your GPU driver, and the UI stays responsive with a progress bar and an estimated time remaining.
  - There's also a one-click **Screenshot** of the current view at export resolution.

**Recommended settings for a final render:** 1080p or 4K, 60 fps, 16 samples, shutter 0.5, Ultra geodesics on. That takes a while (4,200 frames × 16 samples), so try a 720p render with 4 samples first to check the timing.
