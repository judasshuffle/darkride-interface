# ROBOTDOLLY DarkRide Benchmark (experimental v0.1)

A first-pass **script-driven** performance sampler for DarkRide/High Fidelity Interface. Does not change the viewer or the benchmark scene. Saves plain-text reports, including PC hardware, on the Windows machine.

## Usage (Windows)

1. Install Node.js 18 or later (if not already installed).
2. Download `recorder.js` and `robotdolly-benchmark.js` into the same folder.
3. In PowerShell in that folder, run `node recorder.js` and leave it running.
4. In DarkRide, open **Running Scripts** and load `robotdolly-benchmark.js` from disk.
5. Click **BENCH** on the tablet, enter the viewer build/commit ID and the saved landmark's name. Click **BENCH** again to finish.
6. Find `darkride_<build>_<timestamp>.txt` under `reports/`.

## Controlled comparisons

Use the **same machine**, **same saved landmark / camera angle**, **same Woody avatar**, **1920×1080 render resolution**, **100% render scale**, **same LOD/shadows** and **VSync off** with no FPS cap. Let the world load fully before pressing Start. Repeat three times. The script records intended resolution only: it **cannot enforce or verify** it. Explicitly record any differences in the test label.

## What's implemented

- A tablet button to start/stop 1Hz samples from the viewer's `Stats` object
- Location URL captured when stopping and human-labelled benchmark location
- TXT export through a small localhost Node.js recorder
- Windows WMI/CIM CPU, GPU, OS, RAM inventory; sampled FPS summary
- Each file includes the manually entered viewer build/commit ID

## Current limitations

- **UNTESTED on the specific DarkRide Windows build.** Tablet/Stats property availability and localhost XMLHttpRequest permissions must be checked on your installation. Some unsupported values will appear as null.
- Scripts may not have access to the underlying local networking or save-file APIs; if localhost requests are blocked, the exporter will need a viewer API or helper change.
- Build ID must currently be entered manually, rather than assumed to be a reliable Git commit from the executable.
- No automatic reading of saved-landmark inventory, teleporting or camera control yet. Manually choose your existing landmark before each run.
- 1Hz samples cannot produce a statistically correct 1% low; requires per-frame records.
- No automatic resolution/graphics setting enforcement, no server stress simulation.
- Recorder binds only to 127.0.0.1; reports stay on the PC. Keep the recorder running only while testing.

This is an incremental prototype, not a validated professional benchmark.
