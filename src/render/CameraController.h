// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "render/PresentationState.h"

#include <array>

namespace threebs {

struct CameraBasis {
    Vec3 position{};
    Vec3 target{};
    Vec3 right{1.0, 0.0, 0.0};
    Vec3 up{0.0, 1.0, 0.0};
    Vec3 forward{0.0, 0.0, -1.0};
};

class CameraController {
public:
    static constexpr double fieldOfViewRadians = 0.7853981633974483;
    static constexpr double focusTransitionSeconds = 0.7;
    static constexpr double autoOrbitDelaySeconds = 3.0;
    static constexpr double autoOrbitFadeSeconds = 0.5;
    static constexpr double autoFramePadding = 1.15;
    static constexpr double autoFrameResponse = 2.5;

    CameraController() = default;

    void setState(const CameraState& state, const std::array<BodyState, bodyCount>& bodies,
                  double now) noexcept;
    CameraState state() const noexcept { return state_; }
    CameraBasis basis() const noexcept;

    void beginInteraction(double now) noexcept;
    void orbit(double deltaX, double deltaY, double viewportWidth, double now) noexcept;
    void zoom(double delta, const std::array<BodyState, bodyCount>& bodies, double now) noexcept;
    void resetView(const std::array<BodyState, bodyCount>& bodies, double aspectRatio,
                   double now) noexcept;
    void selectFocus(int body, const std::array<BodyState, bodyCount>& bodies, double now) noexcept;
    void update(double now, const std::array<BodyState, bodyCount>& bodies,
                double aspectRatio = 16.0 / 9.0) noexcept;

    int hitTest(double x, double y, double width, double height,
                const std::array<BodyState, bodyCount>& bodies) const noexcept;

    static Vec3 barycenter(const std::array<BodyState, bodyCount>& bodies) noexcept;
    static double framingDistance(const std::array<BodyState, bodyCount>& bodies,
                                  double aspectRatio) noexcept;
    static bool isClick(double deltaX, double deltaY) noexcept;

private:
    Vec3 desiredTarget(const std::array<BodyState, bodyCount>& bodies) const noexcept;

    CameraState state_{};
    Vec3 focus_{};
    Vec3 transitionStart_{};
    double transitionStartTime_{};
    double previousUpdateTime_{};
    double lastInteractionTime_{-1.0e9};
    bool transitioning_{};
};

} // namespace threebs
