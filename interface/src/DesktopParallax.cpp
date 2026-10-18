// ROBOTDOLLY Parallax v5 — Judas. Apache-2.0.
#include "DesktopParallax.h"
#include <QCameraInfo>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QLabel>
#include <QMutexLocker>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace {
constexpr qint64 STALE_MS = 500;
constexpr float MAX_TRANSLATION_METRES = 0.30f;
qint64 now() { QElapsedTimer timer; timer.start(); return timer.msecsSinceReference(); }
}

DesktopParallax::DesktopParallax() {
    connect(&_tracker, &NativeCameraTracker::poseReady, this, &DesktopParallax::receive);
    connect(&_tracker, &NativeCameraTracker::statusChanged, this, [this](const QString& status) { _status = status; });
}
DesktopParallax::~DesktopParallax() { _tracker.stop(); }

void DesktopParallax::resetPose() {
    QMutexLocker lock(&_mutex);
    _calibrated = false;
    _neutralSamples = 0;
    _lastPose = -1;
    _acceptAfter = now();
    _filtered = glm::vec3(0.0f);
}

void DesktopParallax::setEnabled(bool enabled) {
    { QMutexLocker lock(&_mutex); _enabled = enabled; }
    resetPose();
    if (enabled) { _tracker.start(_device, _horizontalFov); }
    else { _tracker.stop(); _status = "OFF — camera released."; }
}

void DesktopParallax::recalibrate() { resetPose(); }

void DesktopParallax::receive(float x, float y, float z, qint64 capturedAt) {
    const glm::vec3 position(x, y, z);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || glm::length(position) > 3.0f) { return; }
    QMutexLocker lock(&_mutex);
    const auto time = now();
    if (!_enabled || capturedAt <= _acceptAfter || time - capturedAt > STALE_MS || capturedAt > time ||
        (_lastPose >= 0 && capturedAt <= _lastPose)) { return; }
    if (_lastPose < 0 || capturedAt - _lastPose > STALE_MS) {
        _calibrated = false;
        _neutralSamples = 0;
        _filtered = glm::vec3(0.0f);
    }
    // Reject implausible jumps rather than driving the view to the safety clamp.
    if (_calibrated && glm::length(position - _position) > .15f) { return; }
    if (!_calibrated) {
        if (_neutralSamples == 0 || glm::length(position - _neutral) > .015f) {
            _neutral = position;
            _neutralSamples = 1;
        } else {
            _neutral += (position - _neutral) / float(++_neutralSamples);
        }
        _calibrated = _neutralSamples >= 6;
    }
    _position = position;
    _lastPose = capturedAt;
}

glm::vec3 DesktopParallax::displayOffset(float deltaTime, bool compatible) {
    QMutexLocker lock(&_mutex);
    if (!_enabled || !compatible || !_calibrated || _lastPose < 0 || now() - _lastPose > STALE_MS) {
        _filtered = glm::vec3(0.0f);
        return _filtered;
    }
    glm::vec3 target = (_position - _neutral) * _sensitivity;
    const float distance = glm::length(target);
    if (distance > MAX_TRANSLATION_METRES) { target *= MAX_TRANSLATION_METRES / distance; }
    const float dt = std::isfinite(deltaTime) ? std::max(0.0f, std::min(deltaTime, 0.1f)) : 0.0f;
    const float alpha = _smoothingSeconds > 0.0f ? 1.0f - std::exp(-dt / _smoothingSeconds) : 1.0f;
    _filtered += alpha * (target - _filtered);
    return _filtered;
}

void DesktopParallax::showSettings() {
    QDialog dialog;
    dialog.setWindowTitle("ROBOTDOLLY Parallax v5 — Native Camera — Judas");
    QFormLayout layout(&dialog);
    QLabel description("Local webcam head tracking. Translation only; desktop first-person modes.\n"
                       "Maximum offset: 30 cm. Hold still briefly to establish neutral.\n"
                       "Distance is approximate; set your webcam's horizontal field of view if known.");
    layout.addRow(&description);
    QLabel status(_status);
    status.setWordWrap(true);
    status.setMaximumWidth(600);
    layout.addRow("Status", &status);
    QTimer refresh;
    connect(&refresh, &QTimer::timeout, &dialog, [&] {
        QString text = _status;
        { QMutexLocker lock(&_mutex);
          if (_enabled && !_calibrated && _lastPose >= 0 && now() - _lastPose <= STALE_MS) {
              text = "Hold still: establishing neutral...";
          } }
        status.setText(text);
    });
    refresh.start(250);
    QComboBox camera;
    camera.addItem("System default", QByteArray());
    for (const auto& info : QCameraInfo::availableCameras()) { camera.addItem(info.description(), info.deviceName().toUtf8()); }
    int selected = camera.findData(_device);
    if (selected < 0) { camera.addItem("Selected camera (disconnected)", _device); selected = camera.count() - 1; }
    camera.setCurrentIndex(selected);
    layout.addRow("Webcam", &camera);
    QDoubleSpinBox fov;
    fov.setRange(30, 110);
    fov.setSuffix(" degrees");
    fov.setValue(_horizontalFov);
    layout.addRow("Horizontal field of view", &fov);
    QDoubleSpinBox sensitivity;
    sensitivity.setRange(0.0, 5.0);
    sensitivity.setSingleStep(0.1);
    QDoubleSpinBox smoothing;
    smoothing.setRange(0.0, 500.0);
    smoothing.setSuffix(" ms");
    { QMutexLocker lock(&_mutex);
      sensitivity.setValue(_sensitivity);
      smoothing.setValue(_smoothingSeconds * 1000.0f); }
    layout.addRow("Sensitivity", &sensitivity);
    layout.addRow("Smoothing", &smoothing);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        const bool restart = _device != camera.currentData().toByteArray() || _horizontalFov != fov.value();
        _device = camera.currentData().toByteArray();
        _horizontalFov = fov.value();
        bool enabled;
        { QMutexLocker lock(&_mutex);
          _sensitivity = static_cast<float>(sensitivity.value());
          _smoothingSeconds = static_cast<float>(smoothing.value() / 1000.0);
          enabled = _enabled; }
        if (restart && enabled) { setEnabled(true); }
    }
}
