// ROBOTDOLLY Parallax v4 — Judas
// Distributed under the Apache License, Version 2.0.
#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QMutex>
#include <QTimer>
#include <QtWebSockets/QWebSocket>
#include <glm/glm.hpp>

// Socket and controls live on the application thread. Render reads a locked snapshot.
class DesktopParallax : public QObject {
public:
    DesktopParallax();
    void setEnabled(bool enabled);
    void recalibrate();
    void showSettings();
    glm::vec3 displayOffset(float deltaTime, bool compatible);
private:
    void receive(const QString& message);
    void resetPose();
    QWebSocket _socket;
    QTimer _retry;
    QElapsedTimer _clock;
    QMutex _mutex;
    bool _enabled { false };
    bool _calibrated { false };
    qint64 _lastPose { -1 };
    glm::vec3 _neutral { 0.0f };
    glm::vec3 _position { 0.0f };
    glm::vec3 _filtered { 0.0f };
    float _sensitivity { 1.0f };
    float _smoothingSeconds { 0.08f };
};
