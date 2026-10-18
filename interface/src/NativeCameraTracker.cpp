// ROBOTDOLLY Parallax v5 — Judas. Apache-2.0.
#include "NativeCameraTracker.h"
#include "NativeHeadPose.h"
#include <QAbstractVideoSurface>
#include <QCamera>
#include <QCameraInfo>
#include <QCameraViewfinderSettings>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QTimer>
#include <QVideoSurfaceFormat>
#include <opencv2/imgproc.hpp>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace {
qint64 monotonicNow() {
    QElapsedTimer timer;
    timer.start();
    return timer.msecsSinceReference();
}

class CameraSurface : public QAbstractVideoSurface {
public:
    std::function<void(QImage, qint64)> frame;
    explicit CameraSurface(QObject* parent) : QAbstractVideoSurface(parent) {}
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType type) const override {
        if (type != QAbstractVideoBuffer::NoHandle) { return {}; }
        return { QVideoFrame::Format_RGB32, QVideoFrame::Format_ARGB32, QVideoFrame::Format_RGB24,
                 QVideoFrame::Format_BGR24, QVideoFrame::Format_YUYV, QVideoFrame::Format_UYVY,
                 QVideoFrame::Format_NV12, QVideoFrame::Format_YUV420P };
    }
    bool present(const QVideoFrame& incoming) override {
        const qint64 timestamp = monotonicNow();
        QVideoFrame video(incoming);
        if (!video.isValid() || !video.map(QAbstractVideoBuffer::ReadOnly)) { return false; }
        QImage image;
        const auto imageFormat = QVideoFrame::imageFormatFromPixelFormat(video.pixelFormat());
        if (imageFormat != QImage::Format_Invalid) {
            image = QImage(video.bits(), video.width(), video.height(), video.bytesPerLine(), imageFormat)
                        .convertToFormat(QImage::Format_RGB888).copy();
        } else {
            try {
                cv::Mat rgb;
                if (video.pixelFormat() == QVideoFrame::Format_YUYV || video.pixelFormat() == QVideoFrame::Format_UYVY) {
                    cv::Mat packed(video.height(), video.width(), CV_8UC2, video.bits(), video.bytesPerLine());
                    cv::cvtColor(packed, rgb, video.pixelFormat() == QVideoFrame::Format_YUYV ?
                                 cv::COLOR_YUV2RGB_YUY2 : cv::COLOR_YUV2RGB_UYVY);
                } else if (video.pixelFormat() == QVideoFrame::Format_BGR24) {
                    cv::Mat packed(video.height(), video.width(), CV_8UC3, video.bits(), video.bytesPerLine());
                    cv::cvtColor(packed, rgb, cv::COLOR_BGR2RGB);
                } else if (video.planeCount() >= 2 && video.width() % 2 == 0 && video.height() % 2 == 0) {
                    // Pack planes row by row: camera buffers can have padded, different strides.
                    cv::Mat packed(video.height() * 3 / 2, video.width(), CV_8UC1);
                    for (int y = 0; y < video.height(); ++y) {
                        memcpy(packed.ptr(y), video.bits(0) + y * video.bytesPerLine(0), video.width());
                    }
                    auto* destination = packed.ptr(video.height());
                    const bool nv12 = video.pixelFormat() == QVideoFrame::Format_NV12;
                    if (!nv12 && video.planeCount() < 3) { video.unmap(); return false; }
                    for (int plane = 1; plane < (nv12 ? 2 : 3); ++plane) {
                        const int width = nv12 ? video.width() : video.width() / 2;
                        for (int y = 0; y < video.height() / 2; ++y) {
                            memcpy(destination, video.bits(plane) + y * video.bytesPerLine(plane), width);
                            destination += width;
                        }
                    }
                    cv::cvtColor(packed, rgb, nv12 ? cv::COLOR_YUV2RGB_NV12 : cv::COLOR_YUV2RGB_I420);
                }
                if (!rgb.empty()) { image = QImage(rgb.data, rgb.cols, rgb.rows, int(rgb.step), QImage::Format_RGB888).copy(); }
            } catch (const cv::Exception&) { /* watchdog reports unsupported/failed capture */ }
        }
        video.unmap();
        if (image.isNull()) { return false; }
        if (surfaceFormat().scanLineDirection() == QVideoSurfaceFormat::BottomToTop) { image = image.mirrored(false, true); }
        if (frame) { frame(std::move(image), timestamp); }
        return true;
    }
};
}

struct NativeCameraTracker::State {
    QCamera* camera { nullptr };
    CameraSurface* surface { nullptr };
    QTimer* watchdog { nullptr };
    std::thread worker;
    std::mutex mutex;
    std::condition_variable ready;
    bool stopping { true };
    QImage pending;
    qint64 capturedAt { 0 };
    qint64 lastFrame { 0 };
    quint64 generation { 0 };
};

NativeCameraTracker::NativeCameraTracker(QObject* parent) : QObject(parent), _state(new State) {
    _state->watchdog = new QTimer(this);
    _state->watchdog->setInterval(1000);
    connect(_state->watchdog, &QTimer::timeout, this, [this] {
        qint64 lastFrame;
        { std::lock_guard<std::mutex> guard(_state->mutex); lastFrame = _state->lastFrame; }
        if (monotonicNow() - lastFrame > 5000) {
            stop();
            emit statusChanged("No camera frames. Check Windows camera privacy/access and camera use by other apps; choose ON to retry.");
        }
    });
}

NativeCameraTracker::~NativeCameraTracker() { stop(); }

void NativeCameraTracker::stop() {
    ++_state->generation; // Discard queued results from old sessions/cameras.
    _state->watchdog->stop();
    if (_state->camera) {
        _state->camera->stop();
        _state->camera->setViewfinder(static_cast<QAbstractVideoSurface*>(nullptr));
        delete _state->camera;
        _state->camera = nullptr;
    }
    delete _state->surface;
    _state->surface = nullptr;
    {
        std::lock_guard<std::mutex> guard(_state->mutex);
        _state->stopping = true;
        _state->pending = QImage();
    }
    _state->ready.notify_one();
    if (_state->worker.joinable()) { _state->worker.join(); }
}

void NativeCameraTracker::start(const QByteArray& device, double horizontalFov) {
    stop();
    const auto cameras = QCameraInfo::availableCameras();
    QCameraInfo selected = device.isEmpty() ? QCameraInfo::defaultCamera() : QCameraInfo();
    for (const auto& camera : cameras) {
        if (!device.isEmpty() && camera.deviceName().toUtf8() == device) { selected = camera; break; }
    }
    if (selected.isNull()) {
        emit statusChanged("Selected camera is unavailable. Connect a webcam, select it in settings and choose ON.");
        return;
    }
    const auto generation = _state->generation;
    {
        std::lock_guard<std::mutex> guard(_state->mutex);
        _state->stopping = false;
        _state->lastFrame = monotonicNow();
    }
    const auto directory = (QCoreApplication::applicationDirPath() + "/parallax-models").toStdString();
    _state->worker = std::thread([this, generation, directory, horizontalFov] {
        try {
            NativeHeadPose estimator(directory);
            while (true) {
                QImage image;
                qint64 capturedAt;
                {
                    std::unique_lock<std::mutex> guard(_state->mutex);
                    _state->ready.wait(guard, [this] { return _state->stopping || !_state->pending.isNull(); });
                    if (_state->stopping) { break; }
                    image = std::move(_state->pending);
                    capturedAt = _state->capturedAt;
                }
                cv::Mat rgb(image.height(), image.width(), CV_8UC3, image.bits(), image.bytesPerLine()), bgr;
                cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
                cv::Vec3d position;
                const bool valid = estimator.estimate(bgr, horizontalFov, position);
                QMetaObject::invokeMethod(this, [this, generation, valid, position, capturedAt] {
                    if (generation != _state->generation || monotonicNow() - capturedAt > 500) { return; }
                    if (valid) {
                        emit poseReady(float(position[0]), float(position[1]), float(position[2]), capturedAt);
                        emit statusChanged("Tracking locally. Recalibrate while looking straight ahead.");
                    } else {
                        emit statusChanged("Tracking lost: keep one well-lit face in view, facing the screen.");
                    }
                }, Qt::QueuedConnection);
            }
        } catch (const std::exception&) {
            QMetaObject::invokeMethod(this, [this, generation] {
                if (generation != _state->generation) { return; }
                stop();
                emit statusChanged("Head tracker could not load/run its local models. Repair the parallax-models installation, then choose ON.");
            }, Qt::QueuedConnection);
        }
    });
    _state->surface = new CameraSurface(this);
    _state->surface->frame = [this](QImage image, qint64 timestamp) {
        // One pending frame only: inference never builds an old-frame queue.
        if (image.width() > 640) { image = image.scaledToWidth(640, Qt::SmoothTransformation); }
        {
            std::lock_guard<std::mutex> guard(_state->mutex);
            if (_state->stopping) { return; }
            _state->pending = std::move(image);
            _state->capturedAt = timestamp;
            _state->lastFrame = timestamp;
        }
        _state->ready.notify_one();
    };
    _state->camera = new QCamera(selected, this);
    connect(_state->camera, QOverload<QCamera::Error>::of(&QCamera::error), this,
            [this, generation](QCamera::Error error) {
        if (error == QCamera::NoError || generation != _state->generation || !_state->camera) { return; }
        const auto message = _state->camera->errorString();
        stop();
        emit statusChanged("Camera unavailable or access denied: " + message + ". Check Windows camera privacy; choose ON to retry.");
    }, Qt::QueuedConnection);
    _state->camera->setViewfinder(_state->surface);
    QCameraViewfinderSettings settings;
    settings.setResolution(640, 480);
    settings.setMaximumFrameRate(30);
    _state->camera->setViewfinderSettings(settings);
    emit statusChanged("Starting local webcam tracking...");
    _state->watchdog->start();
    _state->camera->start();
}
