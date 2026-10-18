// Synthetic loopback checks for ROBOTDOLLY Parallax v4. Judas.
#include "DesktopParallax.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QWebSocketServer>
#include <QHostAddress>
#include <iostream>
#include <cmath>

void pump(int ms = 30) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QWebSocketServer server("Parallax synthetic test", QWebSocketServer::NonSecureMode);
    if (!server.listen(QHostAddress::LocalHost, 8765)) {
        std::cerr << "SKIP: loopback port already occupied\n";
        return 2;
    }
    int failures = 0;
    auto check = [&](bool condition, const char* name) {
        std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
        if (!condition) ++failures;
    };
    DesktopParallax tracker;
    auto offset = [&] { return tracker.displayOffset(0.1f, true); };
    auto zero = [](glm::vec3 v) { return glm::length(v) < 0.00001f; };
    check(zero(offset()), "starts disabled");
    tracker.setEnabled(true);
    pump(100);
    if (!server.hasPendingConnections()) { std::cerr << "No connection\n"; return 3; }
    auto peer = server.nextPendingConnection();
    auto send = [&](const char* text) { peer->sendTextMessage(QString::fromUtf8(text)); pump(); };
    send(R"({"kind":"pose","x":100,"y":100,"z":100})");
    check(zero(offset()), "first pose calibrates neutral");
    send(R"({"kind":"pose","x":110,"y":80,"z":105,"yaw":90})");
    auto first = offset();
    check(first.x > 0 && first.y < 0 && first.z > 0 && first.x < 0.1f, "axis signs and smoothing");
    for (int i = 0; i < 30; ++i) offset();
    check(glm::length(offset() - glm::vec3(0.1f,-0.2f,0.05f)) < 0.0001f, "centimetres converted to relative metres");
    send(R"({"kind":"pose","x":"bad","y":0,"z":0})");
    check(glm::length(offset() - glm::vec3(0.1f,-0.2f,0.05f)) < 0.0001f, "invalid fields rejected");
    send(R"({"kind":"other","x":0,"y":0,"z":0})");
    check(glm::length(offset() - glm::vec3(0.1f,-0.2f,0.05f)) < 0.0001f, "non-pose messages rejected");
    send(R"({"kind":"pose","x":1000,"y":1000,"z":1000})");
    for (int i = 0; i < 30; ++i) offset();
    check(std::abs(glm::length(offset()) - 0.3f) < 0.0001f, "translation bounded to 30 cm");
    check(zero(tracker.displayOffset(0.1f,false)), "incompatible mode bypass");
    tracker.recalibrate();
    check(zero(offset()), "recalibration resets view immediately");
    send(R"({"kind":"pose","x":25,"y":30,"z":40})");
    check(zero(offset()), "recalibration uses next pose");
    send(R"({"kind":"pose","x":35,"y":30,"z":40})");
    check(!zero(offset()), "tracking resumes after calibration");
    pump(550);
    check(zero(offset()), "stale data restores normal view");
    send(R"({"kind":"pose","x":200,"y":200,"z":200})");
    check(zero(offset()), "stale recovery establishes new neutral");
    send(R"({"kind":"pose","x":210,"y":200,"z":200})");
    check(!zero(offset()), "fresh recovery movement");
    peer->close(); pump();
    check(zero(offset()), "disconnect restores normal view");
    tracker.setEnabled(false);
    check(zero(offset()), "OFF restores normal view");
    delete peer;
    return failures ? 1 : 0;
}
