// ROBOTDOLLY Parallax v5 — Judas. Apache-2.0.
#include "NativeHeadPose.h"
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
// Nose bridge/tip, eye corners, forehead and temples; avoid lips and jaw motion.
const std::vector<int> LANDMARKS { 1, 4, 6, 33, 133, 362, 263, 168, 127, 356, 234, 454, 10 };
}

NativeHeadPose::NativeHeadPose(const std::string& directory) {
    _detector = cv::FaceDetectorYN::create(directory + "/face_detection_yunet_2023mar.onnx", "",
                                          { 320, 240 }, 0.80f, 0.3f, 5000,
                                          cv::dnn::DNN_BACKEND_OPENCV, cv::dnn::DNN_TARGET_CPU);
    _mesh = cv::dnn::readNetFromONNX(directory + "/face_mesh_Nx3x192x192.onnx");
    _mesh.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    _mesh.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    std::ifstream file(directory + "/canonical_face_model.obj");
    std::vector<cv::Point3d> vertices;
    std::string line;
    while (std::getline(file, line)) {
        if (line.compare(0, 2, "v ") != 0) { continue; }
        std::istringstream stream(line.substr(2));
        double x, y, z;
        if (!(stream >> x >> y >> z)) { throw std::runtime_error("Invalid canonical face model"); }
        // MediaPipe canonical centimetres (+Y up, +Z towards camera) -> CV metres.
        vertices.emplace_back(x * .01, -y * .01, -z * .01);
    }
    if (vertices.size() != 468) { throw std::runtime_error("Missing or invalid canonical face model"); }
    for (int index : LANDMARKS) { _model.push_back(vertices[index]); }
}

cv::Matx33d NativeHeadPose::intrinsics(cv::Size size, double fov) {
    const double focal = size.width / (2.0 * std::tan(fov * CV_PI / 360.0));
    return { focal, 0, size.width * .5, 0, focal, size.height * .5, 0, 0, 1 };
}

bool NativeHeadPose::solve(const std::vector<cv::Point3d>& model, const std::vector<cv::Point2d>& pixels,
                           cv::Size size, double fov, cv::Vec3d& position) {
    if (model.size() < 6 || model.size() != pixels.size() || size.width <= 0 || size.height <= 0 ||
        !std::isfinite(fov) || fov < 30 || fov > 110) { return false; }
    for (const auto& p : pixels) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || p.x < 0 || p.y < 0 ||
            p.x >= size.width || p.y >= size.height) { return false; }
    }
    const auto camera = intrinsics(size, fov);
    cv::Vec3d rotation, translation;
    cv::Mat inliers;
    const float tolerance = std::max(2.0f, size.width * .006f);
    if (!cv::solvePnPRansac(model, pixels, camera, cv::noArray(), rotation, translation, false,
                           100, tolerance, .99, inliers, cv::SOLVEPNP_EPNP) ||
        inliers.total() < (model.size() * 7 + 9) / 10) { return false; }
    std::vector<cv::Point3d> goodModel;
    std::vector<cv::Point2d> goodPixels;
    for (int i = 0; i < inliers.rows; ++i) {
        const int index = inliers.at<int>(i);
        goodModel.push_back(model[index]);
        goodPixels.push_back(pixels[index]);
    }
    cv::solvePnPRefineLM(goodModel, goodPixels, camera, cv::noArray(), rotation, translation);
    cv::Matx33d orientation;
    cv::Rodrigues(rotation, orientation);
    if (!cv::checkRange(cv::Mat(translation)) || !cv::checkRange(cv::Mat(orientation)) ||
        translation[2] < .20 || translation[2] > 2.0 || orientation(2, 2) < .5) { return false; }
    std::vector<cv::Point2d> projected;
    cv::projectPoints(goodModel, rotation, translation, camera, cv::noArray(), projected);
    double error = 0;
    for (size_t i = 0; i < projected.size(); ++i) {
        const auto delta = projected[i] - goodPixels[i];
        error += delta.dot(delta);
    }
    if (std::sqrt(error / projected.size()) > tolerance) { return false; }
    // Unmirrored webcam: physical viewer-right is image-left. Viewer +Y up, +Z back.
    position = { -translation[0], -translation[1], translation[2] };
    return true;
}

bool NativeHeadPose::estimate(const cv::Mat& bgr, double fov, cv::Vec3d& position) {
    cv::Mat small;
    cv::resize(bgr, small, { 320, std::max(1, cvRound(bgr.rows * 320.0 / bgr.cols)) });
    _detector->setInputSize(small.size());
    cv::Mat faces;
    _detector->detect(small, faces);
    // Do not jump between people. Resume with a new neutral once just one is visible.
    if (faces.rows != 1) { return false; }
    const float* face = faces.ptr<float>();
    const double sx = bgr.cols / 320.0, sy = double(bgr.rows) / small.rows;
    const double side = 1.5 * std::max(face[2] * sx, face[3] * sy);
    if (side < 90) { return false; }
    const cv::Point2f center(float((face[0] + face[2] * .5) * sx), float((face[1] + face[3] * .5) * sy));
    const double angle = std::atan2((face[7] - face[5]) * sy, (face[6] - face[4]) * sx) * 180 / CV_PI;
    cv::Mat warp = cv::getRotationMatrix2D(center, angle, 192.0 / side);
    warp.at<double>(0, 2) += 96 - center.x;
    warp.at<double>(1, 2) += 96 - center.y;
    cv::Mat crop, inverse;
    cv::warpAffine(bgr, crop, warp, { 192, 192 });
    cv::invertAffineTransform(warp, inverse);
    _mesh.setInput(cv::dnn::blobFromImage(crop, 1.0 / 255, { 192, 192 }, {}, true, false));
    std::vector<cv::Mat> output;
    _mesh.forward(output, std::vector<cv::String> { "landmarks", "score" });
    if (output.size() != 2 || output[0].total() != 468 * 3 || output[1].total() != 1 ||
        !cv::checkRange(output[0]) || !cv::checkRange(output[1]) || output[1].ptr<float>()[0] < 2.94f) {
        return false; // sigmoid(logit) >= .95
    }
    std::vector<cv::Point2d> pixels;
    const float* points = output[0].ptr<float>();
    for (int index : LANDMARKS) {
        const double x = points[index * 3], y = points[index * 3 + 1];
        pixels.emplace_back(inverse.at<double>(0, 0) * x + inverse.at<double>(0, 1) * y + inverse.at<double>(0, 2),
                            inverse.at<double>(1, 0) * x + inverse.at<double>(1, 1) * y + inverse.at<double>(1, 2));
    }
    return solve(_model, pixels, bgr.size(), fov, position);
}





