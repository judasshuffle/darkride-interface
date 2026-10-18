# ROBOTDOLLY Parallax v5 — Native Camera

Project credit: Judas. Branch: `experimental/robotdolly-parallax-v5-native-camera`.
The v4 branch and commit `e42f13c2a4` are preserved. The v4 document describes the
old WebSocket implementation, not this native tracker.

## Use

Settings > ROBOTDOLLY Parallax v5 > Parallax ON opens the webcam inside the viewer.
Look straight ahead and hold still for six accepted frames to establish neutral.
Move your head sideways, vertically or towards/away from the screen. Recalibrate
resets the view immediately and collects a new neutral. OFF releases the camera.

Camera, sensitivity and smoothing settings include all detected webcams, a live
status message, sensitivity 0–5, smoothing 0–500 ms and horizontal camera FOV
30–110 degrees. Defaults: OFF, system default camera, sensitivity 1, smoothing
80 ms, FOV 60 degrees. Settings are session-only. Choose a camera and press OK;
if tracking is ON, it restarts and recalibrates. Reopen settings to refresh the
camera list after connecting a device. ON also retries after a capture failure.

Use a normal, unmirrored camera feed. Virtual cameras that flip the image reverse
the horizontal tracking convention. Use the webcam's horizontal FOV if known;
manufacturer specifications sometimes give diagonal FOV instead.

## Native implementation and limits

Qt 5.15.2 QCamera + QAbstractVideoSurface capture frames in the viewer process.
The viewfinder requests 640x480 at up to 30 fps. RGB and common Windows YUV camera
formats are converted with stride-aware copies. Bottom-up buffers are corrected.
A worker thread owns the CPU inference networks and a single replaceable pending
frame. There is no backlog of camera frames, no tracking process and no socket.

OpenCV 4.10 YuNet detects a face and eye locations. Its box defines a 1.5x square,
eye-aligned RGB crop for the bundled 468-point MediaPipe Face Mesh ONNX export.
Thirteen landmarks from the nose, eye corners, forehead and temples are matched
to the MediaPipe canonical 3D face. RANSAC perspective-n-point estimation and LM
refinement solve both rotation and translation. Only translation is passed to
the display camera; rotation is used to estimate translation and reject extreme
poses. This is not bounding-box movement or face-size-only depth estimation.

Quality gates require exactly one detected face, detector score >= 0.8,
mesh presence >= 0.95, at least 10/13 PnP inliers, low reprojection error,
positive distance 0.2–2 m and a face normal within 60 degrees of frontal.
A generic face model, approximate pinhole intrinsics and no lens-distortion
calibration mean metric scale is approximate. Individual face geometry,
expressions, occlusion, low light and wide-angle lenses can introduce jitter or
rotation/translation cross-talk. This is experimental monocular tracking, not
metrology or a guarantee of accurate six-axis tracking under all conditions.

The model origin is inside the canonical head; translation is not nose-tip
movement. Unmirrored camera coordinates map to viewer +X right, +Y up, +Z back.
Neutral needs six samples within 1.5 cm; jumps over 15 cm between accepted samples
are rejected. Offsets are relative to neutral, sensitivity-scaled, capped at
30 cm and exponentially smoothed with render delta time.

Capture timestamps, rather than inference completion times, drive the 500 ms
staleness limit. Old sessions and pre-recalibration frames are discarded. Loss
returns to the base view within 500 ms; reacquisition collects a fresh neutral.
Capture errors/missing cameras are shown in settings. Five seconds without frames
stops capture with guidance for Windows camera privacy/access or camera contention.
Failures require ON to retry. Shutdown stops capture and joins the worker before
Qt multimedia teardown; no detached inference thread remains.

The existing v4 display-only camera composition, culling and picking are retained.
Avatar transforms, animation, locomotion, mouse controls and outbound world-query
frusta are not modified. VR, editor and non-first-person modes bypass translation.
The camera continues capture while ON in an incompatible mode, but produces no
view offset there. Use OFF to release it. Visual offsets still have no additional
collision sweep and can clip through nearby geometry.

All inference is local. No frames or head poses are saved, uploaded or passed to
a separate application. No OpenTrack, Python bridge, WebSocket server or runtime
model download is required. The SDK download below is a build-time operation.

## Dependencies and build

OpenCV was chosen for native CPU DNN inference and its supported PnP solvers,
without adding a second inference runtime or the MediaPipe/Bazel application
framework. The Face Mesh export has a documented RGB/NCHW input contract. Dlib's
common 68-landmark pretrained model was not selected because its upstream example
flags a noncommercial training-data restriction.

Bundled model files, licenses, source URLs and exact SHA256 hashes are in
`interface/external/native-camera/models/NOTICE.txt`. Model licenses are MIT
(YuNet) and Apache-2.0 (Face Mesh export and canonical geometry). OpenCV's license
is included there too. Do not replace the model with an incompatible export.

On Windows, run `tools/setup-native-camera.ps1` (requires 7-Zip). It verifies and
extracts the official OpenCV 4.10.0 SDK under `build/native-camera-deps`. Alternatively,
supply `-DOpenCV_DIR=<SDK>/build` to CMake. The ordinary viewer CMake build now
requires OpenCV >=4.10. The Windows SDK links the OpenCV world DLL; only its CPU
core/imgproc/dnn/calib3d/objdetect APIs are used. CMake copies the DLL and models
beside the executable and includes them in the client installation. Existing Qt
multimedia DLLs and the Windows `mediaservice` camera plugin must accompany the
viewer as usual. Missing models stop tracking with an actionable status.

For this workspace, the full build uses the existing dependency cache:

```powershell
$env:TIVOLI_VCPKG_BASE = 'C:/dr-vcpkg'
cmake --build build --config Release --target interface --parallel 4
```

Standalone tests compile the actual production tracker, without the viewer:

```powershell
cmake -S tests-manual/parallax -B build/parallax-v5-tests -A x64 `
  -DCMAKE_PREFIX_PATH=<Qt5.15.2>/lib/cmake -DGLM_INCLUDE_DIR=<glm-include-directory>
cmake --build build/parallax-v5-tests --config Release
# Put Qt bin and its dependency DLL directories on PATH.
build/parallax-v5-tests/Release/parallax-smoke.exe
# Optional local face image: exercises detector + mesh + PnP.
build/parallax-v5-tests/Release/parallax-smoke.exe <image-path>
# Optional live webcam test, opens the default camera for 15 seconds, saves no frames.
build/parallax-v5-tests/Release/parallax-smoke.exe --camera
```

## Validation

The full Windows Release viewer build succeeded. All 23 native regression checks
passed with Qt 5.15.2, covering the offset/neutral lifecycle,
NaN and jump rejection, sensitivity, clamp, stale recovery, incompatible-mode
bypass, model loading, synthetic PnP recovery at three rotations, blank-frame
rejection and missing-camera/repeated-stop checks. OpenCV's Lena fixture passed
real detector + dense-mesh + pose inference (fixture is not bundled).

A live BRIO 4K Stream Edition test enumerated six camera devices, produced 441
valid poses over 15 seconds (~29.4/s) and released capture successfully. This
validates the native capture/inference path; it does not measure in-world latency
or establish pose accuracy against ground truth.

Manual viewer acceptance still required:

- ON/OFF, neutral, axis signs and recalibration; yaw/pitch in place should not
  rotate the view and should create little translation.
- WASD/arrows, mouse turning, jumping, wall collisions and avatar animation with
  tracking ON/OFF. Check picking and frustum-edge visibility with a nonzero offset.
- Hide face, introduce another face, return to frame, unplug camera, deny Windows
  camera permission and contend with another app. Check status, timeout and retry.
- Switch physical/virtual cameras, sensitivity 0/5 and smoothing 0/500 ms.
- Switch to VR and every incompatible camera/editor mode, then back.
- Exit while tracking, restart and confirm that the camera is available again.

References: [Qt camera](https://doc.qt.io/archives/qt-5.15/qcamera.html),
[OpenCV PnP](https://docs.opencv.org/4.x/d5/d1f/calib3d_solvePnP.html),
[Face Mesh export](https://github.com/yakhyo/mediapipe-face-mesh-onnx),
[dlib's model caveat](https://www.dlib.net/face_landmark_detection_ex.cpp.html).

