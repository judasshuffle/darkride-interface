// Native camera geometry, inference and lifecycle checks. No socket/server required.
#include "DesktopParallax.h"
#include "NativeHeadPose.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QCameraInfo>
#include <QEventLoop>
#include <QTimer>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <limits>

class ParallaxTestAccess {
public:
    static void enable(DesktopParallax& t) { t._enabled = true; t.resetPose(); }
    static void sample(DesktopParallax& t, glm::vec3 p) {
        QThread::msleep(2);
        QElapsedTimer clock; clock.start();
        t.receive(p.x, p.y, p.z, clock.msecsSinceReference());
    }
    static void neutral(DesktopParallax& t, glm::vec3 p) { for (int i = 0; i < 6; ++i) sample(t, p); }
    static void sensitivity(DesktopParallax& t, float s) { t._sensitivity = s; }
};
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        std::cout << (ok ? "PASS " : "FAIL ") << name << std::endl;
        if (!ok) ++failures;
    };
    auto zero = [](glm::vec3 p) { return glm::length(p) < .00001f; };
    DesktopParallax tracker;
    auto offset = [&] { return tracker.displayOffset(.1f, true); };
    check(zero(offset()), "disabled by default");
    ParallaxTestAccess::enable(tracker);
    ParallaxTestAccess::sample(tracker, {0, 0, .6f});
    check(zero(offset()), "one frame cannot establish neutral");
    ParallaxTestAccess::neutral(tracker, {0, 0, .6f});
    ParallaxTestAccess::sample(tracker, {.05f, -.02f, .65f});
    auto p = offset();
    check(p.x > 0 && p.x < .05f && p.y < 0 && p.z > 0, "axis signs and smoothing");
    for (int i = 0; i < 20; ++i) offset();
    check(glm::length(offset() - glm::vec3(.05f, -.02f, .05f)) < .0001f, "relative metres");
    check(zero(tracker.displayOffset(.1f, false)), "VR/incompatible mode bypass");
    ParallaxTestAccess::sample(tracker, {1, 0, .6f});
    for (int i = 0; i < 20; ++i) offset();
    check(offset().x < .06f, "implausible jump rejected");
    ParallaxTestAccess::sample(tracker, {std::numeric_limits<float>::quiet_NaN(), 0, .6f});
    check(std::isfinite(offset().x), "NaN rejected");
    ParallaxTestAccess::sensitivity(tracker, 5);
    ParallaxTestAccess::sample(tracker, {.1f, 0, .6f});
    for (int i = 0; i < 20; ++i) offset();
    check(std::abs(glm::length(offset()) - .3f) < .0001f, "30 cm clamp");
    ParallaxTestAccess::sensitivity(tracker, 0);
    for (int i = 0; i < 20; ++i) offset();
    check(zero(offset()), "zero sensitivity");
    ParallaxTestAccess::sensitivity(tracker, 1);
    QThread::msleep(510);
    check(zero(offset()), "lost tracking returns to base within 500 ms");
    ParallaxTestAccess::neutral(tracker, {.2f, 0, .7f});
    check(zero(offset()), "reacquisition establishes new neutral");
    tracker.recalibrate();
    check(zero(offset()), "recalibration resets view");
    tracker.setEnabled(false);
    ParallaxTestAccess::sample(tracker, {.25f, 0, .7f});
    check(zero(offset()), "OFF discards samples");
    try {
        NativeHeadPose pose((QCoreApplication::applicationDirPath() + "/parallax-models").toStdString());
        check(pose.modelPoints().size() == 13, "bundled models load");
        cv::Vec3d result;
        for (double yaw : { -.5, 0.0, .5 }) {
            std::vector<cv::Point2d> pixels;
            cv::projectPoints(pose.modelPoints(), cv::Vec3d(.1, yaw, .05), cv::Vec3d(.04, -.02, .65),
                              NativeHeadPose::intrinsics({640,480},60), cv::noArray(), pixels);
            bool valid = NativeHeadPose::solve(pose.modelPoints(), pixels, {640,480}, 60, result);
            check(valid && cv::norm(result - cv::Vec3d(-.04,.02,.65)) < .001, "PnP separates rotation from translation");
        }
        cv::Mat blank(480,640,CV_8UC3,cv::Scalar(0,0,0));
        check(!pose.estimate(blank,60,result), "blank frame is not a head pose");
        std::vector<cv::Point2d> badPixels(pose.modelPoints().size(), {100,100});
        check(!NativeHeadPose::solve(pose.modelPoints(),badPixels,{640,480},60,result), "collapsed landmarks rejected");
        badPixels[0].x = std::numeric_limits<double>::quiet_NaN();
        check(!NativeHeadPose::solve(pose.modelPoints(),badPixels,{640,480},60,result), "nonfinite landmarks rejected");
        if (argc > 1 && QString::fromLocal8Bit(argv[1]) != "--camera") {
            const cv::Mat face = cv::imread(argv[1]);
            check(!face.empty() && pose.estimate(face,60,result), "real face executes dense mesh and PnP");
            std::cout << "Estimated position: " << result << std::endl;
            if (!face.empty()) {
                cv::Mat pair;
                cv::hconcat(face, face, pair);
                check(!pose.estimate(pair,60,result), "multiple faces cannot drive the view");
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        check(false, "inference exception");
    }
    { NativeCameraTracker camera;
      camera.start("nonexistent-test-camera",60);
      camera.stop(); camera.stop(); }
    check(true, "missing camera and repeated shutdown are safe");
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) == "--camera") {
        const auto cameras = QCameraInfo::availableCameras();
        std::cout << "Cameras: " << cameras.size() << std::endl;
        for (const auto& info : cameras) { std::cout << info.description().toStdString() << std::endl; }
        NativeCameraTracker camera;
        QEventLoop loop;
        int poses = 0;
        QString previousStatus;
        QObject::connect(&camera, &NativeCameraTracker::poseReady, &loop,
                         [&](float, float, float, qint64) { ++poses; });
        QObject::connect(&camera, &NativeCameraTracker::statusChanged, &loop, [&](const QString& status) {
            if (status != previousStatus) { std::cout << status.toStdString() << std::endl; previousStatus = status; }
        });
        camera.start({},60);
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        loop.exec();
        camera.stop();
        std::cout << "Valid live poses in 15 seconds: " << poses << std::endl;
    }
    return failures ? 1 : 0;
}
