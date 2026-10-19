// ROBOTDOLLY Benchmark Logger v0.1 — DarkRide / High Fidelity Interface script
// Requires the companion local Node recorder (see README).
(function () {
    "use strict";
    var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
    var button = tablet.addButton({ text: "BENCH", icon: "icons/tablet-icons/avatar-i.svg", activeIcon: "icons/tablet-icons/avatar-a.svg" });
    var recording = false, samples = [], timer = null, start = 0, build = "", label = "";
    var ENDPOINT = "http://127.0.0.1:18473/report";
    function num(v) { var n = Number(v); return isFinite(n) ? n : null; }
    function metric(k) { try { return num(Stats[k]); } catch (e) { return null; } }
    function snapshot() {
        var s = { elapsed_ms: Date.now() - start, fps: metric("renderRate"),
            present_fps: metric("presentRate"), gpu_time_ms: metric("gpuTime"),
            game_rate: metric("gameRate"), triangles: metric("triangleCount"),
            draw_calls: metric("drawCalls"), texture_memory: metric("textureMemory") };
        samples.push(s);
        if (samples.length % 10 === 0) print("ROBOTDOLLY BENCH: " + samples.length + " seconds captured");
    }
    function send(report) {
        try {
            var xhr = new XMLHttpRequest();
            xhr.open("POST", ENDPOINT, true);
            xhr.setRequestHeader("Content-Type", "application/json");
            xhr.onreadystatechange = function () {
                if (xhr.readyState === 4) {
                    Window.alert(xhr.status === 200 ? "Benchmark saved to text file by local recorder." :
                        "Recorder unavailable (HTTP " + xhr.status + "). Start node recorder.js first.");
                }
            };
            xhr.onerror = function () { Window.alert("Recorder not reachable. Run node recorder.js on this PC."); };
            xhr.send(JSON.stringify(report));
        } catch (e) { Window.alert("Could not export: " + e); }
    }
    function stop() {
        if (!recording) return;
        recording = false;
        if (timer !== null) { Script.clearInterval(timer); timer = null; }
        button.editProperties({ isActive: false });
        var href = "";
        try { href = location.href; } catch (e) {}
        send({ tool: "ROBOTDOLLY Benchmark", schema_version: 1, timestamp: new Date().toISOString(),
            build_id: build, test_label: label, location: href,
            intended_resolution: "1920x1080", graphics_settings_verified: false,
            note: "Verify actual screen/window resolution, render scale, VSync and graphics settings manually. Measurements are 1Hz snapshots, not accurate frame-time percentile data.",
            samples: samples });
    }
    function toggle() {
        if (recording) { stop(); return; }
        build = Window.prompt("Build/version or Git commit (enter exact hash if known):", build || "UNKNOWN");
        if (build === null) return;
        label = Window.prompt("Test location / saved landmark label:", label || "Benchmark location");
        if (label === null) return;
        samples = []; start = Date.now(); recording = true;
        button.editProperties({ isActive: true });
        snapshot(); timer = Script.setInterval(snapshot, 1000);
        Window.alert("Recording started. Keep view fixed; tap BENCH again to save.");
    }
    button.clicked.connect(toggle);
    Script.scriptEnding.connect(function () {
        if (recording) stop();
        button.clicked.disconnect(toggle);
        tablet.removeButton(button);
    });
    print("ROBOTDOLLY Benchmark ready. Start the local recorder, then tap BENCH.");
}());
