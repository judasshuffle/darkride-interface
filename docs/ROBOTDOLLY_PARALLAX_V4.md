# ROBOTDOLLY Parallax v4

## Quick start — webcam and octopus edition 🐙

**This is an experimental source-code branch, not a downloadable installer.** Build the `interface` viewer from this branch (or use a locally compiled build). Ordinary DarkRide does not include the native Parallax menu yet.

You need:
- A webcam.
- [OpenTrack](https://github.com/opentrack/opentrack/releases/latest) with its **NeuralNet tracker** input.
- Python 3 with the `websockets` package.
- The [ROBOTDOLLY local bridge](https://github.com/judasshuffle/hifi-content/blob/master/robotdolly-parallax/opentrack_bridge.py) saved as `opentrack_bridge.py`.
- DarkRide built from this Parallax v4 branch.

**On Windows (tested setup):**

1. Open OpenTrack, select **NeuralNet tracker** as Input, choose your webcam, and select **UDP over network** as Output.
2. Open the Output settings and set **IP/host `127.0.0.1`**, **port `4242`**. Start tracking; the octopus preview should respond to your movements.
3. Install the Python dependency: `py -m pip install "websockets>=11"`.
4. Open PowerShell in the folder containing the bridge, then run `py opentrack_bridge.py`. **Leave it running** while using Parallax. When the viewer connects, the bridge should report `viewers=1`.
5. Start the modified DarkRide viewer. **Stop older ROBOTDOLLY Parallax JavaScript scripts** so they do not fight the native camera.
6. Switch to **first-person desktop mode**. Go to **Settings → ROBOTDOLLY Parallax v4 → Parallax ON**.
7. Look straight ahead and choose **Recalibrate parallax**. Move your head side to side to peek around scenery.
8. If sideways movement feels reversed, in OpenTrack go to **Options → Output** and enable **Post-invert for X**. This corrected left/right motion on the original Windows test setup; other webcam setups may differ.
9. Adjust **Parallax sensitivity and smoothing** if needed. Settings are not persisted across sessions. To stop the effect, choose **Parallax OFF**; stopping the bridge also returns the view to normal.

**Troubleshooting:** If the bridge shows `viewers=0`, the viewer has not connected. If it shows `viewers=1` but there is no movement, check **Parallax ON**, first-person view, calibration, and whether the X/Y/Z values in the bridge output actually change. Third-person and VR deliberately bypass the effect. OpenTrack's **Mirror** preview checkbox is not the same as output axis inversion.

**Privacy:** The webcam video stays within your local tracker. The helper sends *numerical head-position values only* over localhost (`127.0.0.1`), not webcam footage. No external transmission is part of this feature.

---


Project credit: Judas.

Experimental desktop first-person camera translation. Settings > ROBOTDOLLY Parallax v4 offers ON, OFF, recalibration, sensitivity (0–5) and smoothing (0–500 ms). Defaults: OFF, sensitivity 1, smoothing 80 ms. Settings are session-only.

The fixed WebSocket endpoint is ws://127.0.0.1:8765 with proxies disabled. Only JSON objects with kind:"pose" and finite numeric x/y/z positions in centimetres are accepted. No webcam access, images, pose uploads or external endpoint is implemented. Coordinate convention is +X right, +Y up, +Z backward; confirm the bridge signs during the first live test.

The first fresh sample establishes neutral. Recalibration takes the next valid sample. Relative translation is converted to metres, scaled, capped at 30 cm and exponentially smoothed with a frame-time-based filter. OFF, disconnect and a 500 ms pose timeout restore the base view; recovery establishes a fresh neutral. Rotation fields are ignored.

The normal camera is calculated first. A separate display camera receives the local translation; its transform and frustum drive rendering, render culling and desktop pick rays. Avatar transforms, motors, drive keys, head rotation and animation state are not modified. VR, editor, third person, selfie, mirror, independent and entity modes bypass the effect.

Avatar collisions are unchanged. The visual eye offset is not a physics movement and does not have an additional camera collision sweep; near walls it can clip despite the 30 cm bound.

Manual validation after building:
- Start with the bridge running: view remains normal until ON.
- Recalibrate, translate each axis, and confirm signs and centimetre scale. Head rotation alone must not rotate the view.
- Walk/strafe with WASD and arrows while translating, turn with mouse, jump and collide with walls. Compare avatar motion/animation against OFF.
- Click visible objects at screen centre and edges with a nonzero offset; check visibility at frustum edges.
- Test OFF, bridge disconnect, connected socket with no poses, malformed messages and reconnect. Normal rendering must return within 500 ms of the last valid pose, and resume at neutral.
- Switch to VR and each incompatible mode, then back. Check sensitivity 0 and smoothing 0/500 ms.

Automated validation: the standalone tests-manual/parallax CMake project compiles the actual tracker and exercises 16 checks with a synthetic loopback WebSocket server. All 16 passed on Windows/MSVC 14.39 with Qt 5.15.2. Stop the real bridge before running: the test requires exclusive use of 127.0.0.1:8765 and refuses to replace an existing listener. Supply CMAKE_PREFIX_PATH for Qt and GLM_INCLUDE_DIR for GLM; run the Release parallax-smoke executable with the Qt and dependency DLL directories on PATH. No GUI is opened.

The live bridge refused connections during implementation, so live axis signs and in-world interaction remain manual validation items. Render culling and mouse picking use the display frustum; existing world-query frusta remain based on the normal camera so tracking offsets are not added to outbound world queries.
