// ROBOTDOLLY Parallax v4 — Judas
// Distributed under the Apache License, Version 2.0.
#include "DesktopParallax.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMutexLocker>
#include <QNetworkProxy>
#include <algorithm>
#include <cmath>

namespace {
constexpr qint64 STALE_MS = 500;
constexpr float MAX_TRANSLATION_METRES = 0.30f;
}

DesktopParallax::DesktopParallax() {
    _clock.start();
    // Fixed loopback endpoint; never use a system proxy or transmit pose/image data.
    _socket.setProxy(QNetworkProxy::NoProxy);
    _socket.setMaxAllowedIncomingMessageSize(4096);
    _socket.setMaxAllowedIncomingFrameSize(4096);
    connect(&_socket, &QWebSocket::textMessageReceived, this, &DesktopParallax::receive);
    connect(&_socket, &QWebSocket::disconnected, this, [this] { resetPose(); });
    connect(&_retry, &QTimer::timeout, this, [this] {
        bool enabled;
        { QMutexLocker lock(&_mutex); enabled = _enabled; }
        if (enabled && _socket.state() == QAbstractSocket::UnconnectedState) {
            _socket.open(QUrl(QStringLiteral("ws://127.0.0.1:8765")));
        }
    });
    _retry.setInterval(1000);
}

void DesktopParallax::resetPose() {
    QMutexLocker lock(&_mutex);
    _calibrated = false;
    _lastPose = -1;
    _filtered = glm::vec3(0.0f);
}

void DesktopParallax::setEnabled(bool enabled) {
    { QMutexLocker lock(&_mutex); _enabled = enabled; }
    resetPose();
    if (enabled) {
        _retry.start();
        if (_socket.state() == QAbstractSocket::UnconnectedState) {
            _socket.open(QUrl(QStringLiteral("ws://127.0.0.1:8765")));
        }
    } else {
        _retry.stop();
        _socket.abort();
    }
}

void DesktopParallax::recalibrate() {
    resetPose(); // The next valid sample becomes neutral.
}

void DesktopParallax::receive(const QString& message) {
    if (message.size() > 4096) { return; }
    const auto document = QJsonDocument::fromJson(message.toUtf8());
    if (!document.isObject()) { return; }
    const auto object = document.object();
    if (object.value("kind").toString() != "pose") { return; }
    glm::vec3 position;
    const char* axes[] = { "x", "y", "z" };
    for (int i = 0; i < 3; ++i) {
        const auto value = object.value(axes[i]);
        if (!value.isDouble()) { return; }
        const double cm = value.toDouble();
        if (!std::isfinite(cm) || std::abs(cm) > 10000.0) { return; }
        position[i] = static_cast<float>(cm * 0.01);
    }
    QMutexLocker lock(&_mutex);
    if (!_enabled) { return; }
    const auto now = _clock.elapsed();
    if (!_calibrated || _lastPose < 0 || now - _lastPose > STALE_MS) {
        _neutral = position;
        _filtered = glm::vec3(0.0f);
        _calibrated = true;
    }
    _position = position;
    _lastPose = now;
}

glm::vec3 DesktopParallax::displayOffset(float deltaTime, bool compatible) {
    QMutexLocker lock(&_mutex);
    if (!_enabled || !compatible || !_calibrated || _lastPose < 0 || _clock.elapsed() - _lastPose > STALE_MS) {
        _filtered = glm::vec3(0.0f);
        return _filtered;
    }
    // Bridge coordinates: +X right, +Y up, +Z back, in centimetres.
    glm::vec3 target = (_position - _neutral) * _sensitivity;
    const float distance = glm::length(target);
    if (distance > MAX_TRANSLATION_METRES) {
        target *= MAX_TRANSLATION_METRES / distance;
    }
    const float dt = std::isfinite(deltaTime) ? std::max(0.0f, std::min(deltaTime, 0.1f)) : 0.0f;
    const float alpha = _smoothingSeconds > 0.0f ? 1.0f - std::exp(-dt / _smoothingSeconds) : 1.0f;
    _filtered += alpha * (target - _filtered);
    return _filtered;
}

void DesktopParallax::showSettings() {
    QDialog dialog;
    dialog.setWindowTitle("ROBOTDOLLY Parallax v4 — Judas");
    QFormLayout layout(&dialog);
    QLabel description("Translation only. Local bridge: 127.0.0.1:8765\nFirst-person desktop modes. Maximum offset: 30 cm.");
    layout.addRow(&description);
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
        QMutexLocker lock(&_mutex);
        _sensitivity = static_cast<float>(sensitivity.value());
        _smoothingSeconds = static_cast<float>(smoothing.value() / 1000.0);
    }
}
