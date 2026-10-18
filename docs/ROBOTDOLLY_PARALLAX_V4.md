# ROBOTDOLLY Parallax v4

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
