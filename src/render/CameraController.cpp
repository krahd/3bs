// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "render/CameraController.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace threebs {
namespace {

constexpr double pi = 3.14159265358979323846;

double smoothstep(double value) noexcept {
    value = std::clamp(value, 0.0, 1.0);
    return value * value * (3.0 - 2.0 * value);
}

bool raySphere(const Vec3& origin, const Vec3& direction, const Vec3& center,
               double radius, double& distance) noexcept {
    const auto offset = origin - center;
    const auto b = dot(offset, direction);
    const auto c = dot(offset, offset) - radius * radius;
    const auto discriminant = b * b - c;
    if (discriminant < 0.0)
        return false;
    const auto root = std::sqrt(discriminant);
    const auto nearDistance = -b - root;
    const auto farDistance = -b + root;
    distance = nearDistance > 0.0 ? nearDistance : farDistance;
    return distance > 0.0;
}

} // namespace

void CameraController::setState(const CameraState& state,
                                const std::array<BodyState, bodyCount>& bodies,
                                double now) noexcept {
    state_ = sanitizedCameraState(state);
    focus_ = desiredTarget(bodies);
    transitionStart_ = focus_;
    transitionStartTime_ = now;
    previousUpdateTime_ = now;
    transitioning_ = false;
}

CameraBasis CameraController::basis() const noexcept {
    const auto cosPitch = std::cos(static_cast<double>(state_.pitch));
    const Vec3 offset{
        static_cast<double>(state_.distance) * cosPitch * std::sin(static_cast<double>(state_.yaw)),
        static_cast<double>(state_.distance) * std::sin(static_cast<double>(state_.pitch)),
        static_cast<double>(state_.distance) * cosPitch * std::cos(static_cast<double>(state_.yaw))};
    CameraBasis result;
    result.target = focus_;
    result.position = focus_ + offset;
    result.forward = normalized(focus_ - result.position, {0.0, 0.0, -1.0});
    result.right = normalized(cross(result.forward, {0.0, 1.0, 0.0}), {1.0, 0.0, 0.0});
    result.up = normalized(cross(result.right, result.forward), {0.0, 1.0, 0.0});
    return result;
}

void CameraController::beginInteraction(double now) noexcept {
    lastInteractionTime_ = now;
}

void CameraController::orbit(double deltaX, double deltaY, double viewportWidth, double now) noexcept {
    const auto scale = 2.8 / std::max(1.0, viewportWidth);
    state_.yaw = static_cast<float>(std::remainder(static_cast<double>(state_.yaw) - deltaX * scale,
                                                   2.0 * pi));
    state_.pitch = std::clamp(static_cast<float>(state_.pitch + deltaY * scale),
                             static_cast<float>(-pi * 0.4722), static_cast<float>(pi * 0.4722));
    lastInteractionTime_ = now;
}

void CameraController::zoom(double delta, const std::array<BodyState, bodyCount>& bodies,
                            double now) noexcept {
    if (state_.focusBody >= 0) {
        state_.focusBody = -1;
        transitionStart_ = focus_;
        transitionStartTime_ = now;
        transitioning_ = true;
    }
    const auto factor = std::exp(-delta * 0.08);
    state_.distance = std::clamp(static_cast<float>(state_.distance * factor),
                                 state_.minimumDistance, state_.maximumDistance);
    state_.autoFrame = false;
    lastInteractionTime_ = now;
    (void)bodies;
}

void CameraController::resetView(const std::array<BodyState, bodyCount>& bodies,
                                 double aspectRatio, double now) noexcept {
    state_.focusBody = -1;
    state_.autoFrame = true;
    state_.distance = static_cast<float>(std::clamp(
        framingDistance(bodies, aspectRatio), static_cast<double>(state_.minimumDistance),
        static_cast<double>(state_.maximumDistance)));
    focus_ = barycenter(bodies);
    transitionStart_ = focus_;
    transitionStartTime_ = now;
    previousUpdateTime_ = now;
    lastInteractionTime_ = now;
    transitioning_ = false;
}

void CameraController::selectFocus(int body, const std::array<BodyState, bodyCount>& bodies,
                                   double now) noexcept {
    state_.focusBody = std::clamp(body, -1, static_cast<int>(bodyCount) - 1);
    transitionStart_ = focus_;
    transitionStartTime_ = now;
    transitioning_ = true;
    lastInteractionTime_ = now;
    (void)bodies;
}

void CameraController::update(double now, const std::array<BodyState, bodyCount>& bodies,
                              double aspectRatio) noexcept {
    const auto desired = desiredTarget(bodies);
    if (transitioning_) {
        const auto amount = smoothstep((now - transitionStartTime_) / focusTransitionSeconds);
        focus_ = transitionStart_ * (1.0 - amount) + desired * amount;
        transitioning_ = amount < 1.0;
    } else {
        focus_ = desired;
    }

    const auto delta = std::clamp(now - previousUpdateTime_, 0.0, 0.1);
    if (state_.autoFrame && state_.focusBody < 0) {
        const auto desiredDistance = std::clamp(
            framingDistance(bodies, aspectRatio),
            static_cast<double>(state_.minimumDistance),
            static_cast<double>(state_.maximumDistance));
        const auto amount = 1.0 - std::exp(-delta * autoFrameResponse);
        state_.distance = static_cast<float>(
            static_cast<double>(state_.distance) + (desiredDistance - state_.distance) * amount);
    }
    const auto idle = now - lastInteractionTime_ - autoOrbitDelaySeconds;
    const auto resume = smoothstep(idle / autoOrbitFadeSeconds);
    state_.yaw = static_cast<float>(std::remainder(
        static_cast<double>(state_.yaw) + static_cast<double>(state_.autoOrbit) * delta * resume,
        2.0 * pi));
    previousUpdateTime_ = now;
}

int CameraController::hitTest(double x, double y, double width, double height,
                              const std::array<BodyState, bodyCount>& bodies) const noexcept {
    if (width <= 0.0 || height <= 0.0)
        return -1;
    const auto camera = basis();
    const auto aspect = width / height;
    const auto tanHalfFov = std::tan(fieldOfViewRadians * 0.5);
    const auto ndcX = x / width * 2.0 - 1.0;
    const auto ndcY = 1.0 - y / height * 2.0;
    const auto ray = normalized(camera.forward
                                + camera.right * (ndcX * aspect * tanHalfFov)
                                + camera.up * (ndcY * tanHalfFov));
    auto nearest = std::numeric_limits<double>::max();
    int selected = -1;
    for (std::size_t body = 0; body < bodyCount; ++body) {
        double distance{};
        if (raySphere(camera.position, ray, bodies[body].position,
                      planetRadius(bodies[body].mass), distance)
            && distance < nearest) {
            nearest = distance;
            selected = static_cast<int>(body);
        }
    }
    return selected;
}

Vec3 CameraController::barycenter(const std::array<BodyState, bodyCount>& bodies) noexcept {
    Vec3 weighted{};
    double totalMass{};
    for (const auto& body : bodies) {
        const auto mass = std::max(0.0, body.mass);
        weighted += body.position * mass;
        totalMass += mass;
    }
    return totalMass > 1.0e-12 ? weighted / totalMass : Vec3{};
}

double CameraController::framingDistance(const std::array<BodyState, bodyCount>& bodies,
                                         double aspectRatio) noexcept {
    const auto center = barycenter(bodies);
    double radius{};
    for (const auto& body : bodies)
        radius = std::max(radius, length(body.position - center) + planetRadius(body.mass));

    const auto safeAspect = std::clamp(aspectRatio, 0.25, 8.0);
    const auto halfVertical = fieldOfViewRadians * 0.5;
    const auto halfHorizontal = std::atan(std::tan(halfVertical) * safeAspect);
    const auto limitingHalfAngle = std::min(halfVertical, halfHorizontal);
    return radius * autoFramePadding / std::max(0.01, std::sin(limitingHalfAngle));
}

bool CameraController::isClick(double deltaX, double deltaY) noexcept {
    return deltaX * deltaX + deltaY * deltaY < 16.0;
}

Vec3 CameraController::desiredTarget(const std::array<BodyState, bodyCount>& bodies) const noexcept {
    if (state_.focusBody >= 0 && state_.focusBody < static_cast<int>(bodyCount))
        return bodies[static_cast<std::size_t>(state_.focusBody)].position;
    return barycenter(bodies);
}

} // namespace threebs
