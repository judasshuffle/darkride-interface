// ROBOTDOLLY Parallax v5 — Judas. Apache-2.0.
#pragma once
#include <QObject>
#include <QString>
#include <memory>

class NativeCameraTracker : public QObject {
    Q_OBJECT
public:
    explicit NativeCameraTracker(QObject* parent = nullptr);
    ~NativeCameraTracker() override;
    void start(const QByteArray& device, double horizontalFov);
    void stop();
signals:
    void poseReady(float x, float y, float z, qint64 capturedAt);
    void statusChanged(const QString& status);
private:
    struct State;
    std::unique_ptr<State> _state;
};
