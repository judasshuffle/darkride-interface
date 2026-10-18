// ROBOTDOLLY Parallax v5 — Judas. Apache-2.0.
#pragma once
#include <QObject>
#include <QMutex>
#include <glm/glm.hpp>
#include "NativeCameraTracker.h"

// Controls/capture live on the application thread; render reads a locked snapshot.
class DesktopParallax : public QObject {
public:
    DesktopParallax();
    ~DesktopParallax() override;
    void setEnabled(bool enabled);
    void recalibrate();
    void showSettings();
    glm::vec3 displayOffset(float deltaTime, bool compatible);
private:
    friend class ParallaxTestAccess;
    void receive(float x, float y, float z, qint64 capturedAt);
    void resetPose();
    NativeCameraTracker _tracker;
    QMutex _mutex;
    bool _enabled { false };
    bool _calibrated { false };
    int _neutralSamples { 0 };
    qint64 _lastPose { -1 };
    qint64 _acceptAfter { 0 };
    glm::vec3 _neutral { 0.0f };
    glm::vec3 _position { 0.0f };
    glm::vec3 _filtered { 0.0f };
    float _sensitivity { 1.0f };
    float _smoothingSeconds { 0.08f };
    double _horizontalFov { 60.0 };
    QByteArray _device;
    QString _status { "OFF — camera released." };
};
