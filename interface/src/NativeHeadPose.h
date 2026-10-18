// ROBOTDOLLY Parallax v5 — Judas. Apache-2.0.
#pragma once
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/objdetect.hpp>
#include <string>
#include <vector>

// CPU-only inference. Own and call this object on one worker thread.
class NativeHeadPose {
public:
    explicit NativeHeadPose(const std::string& modelDirectory);
    bool estimate(const cv::Mat& bgr, double horizontalFov, cv::Vec3d& position);
    // Exposed separately so geometric recovery can be tested without a webcam.
    static bool solve(const std::vector<cv::Point3d>& model, const std::vector<cv::Point2d>& pixels,
                      cv::Size size, double horizontalFov, cv::Vec3d& position);
    static cv::Matx33d intrinsics(cv::Size size, double horizontalFov);
    const std::vector<cv::Point3d>& modelPoints() const { return _model; }
private:
    cv::Ptr<cv::FaceDetectorYN> _detector;
    cv::dnn::Net _mesh;
    std::vector<cv::Point3d> _model;
};
