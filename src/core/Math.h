// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include <algorithm>
#include <cmath>

namespace threebs {

struct Vec3 {
    double x{};
    double y{};
    double z{};

    constexpr Vec3 operator+(const Vec3& other) const noexcept {
        return {x + other.x, y + other.y, z + other.z};
    }
    constexpr Vec3 operator-(const Vec3& other) const noexcept {
        return {x - other.x, y - other.y, z - other.z};
    }
    constexpr Vec3 operator*(double scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar};
    }
    constexpr Vec3 operator/(double scalar) const noexcept {
        return {x / scalar, y / scalar, z / scalar};
    }
    constexpr Vec3& operator+=(const Vec3& other) noexcept {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
    constexpr Vec3& operator-=(const Vec3& other) noexcept {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }
    constexpr Vec3& operator*=(double scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }
};

constexpr Vec3 operator*(double scalar, const Vec3& vector) noexcept {
    return vector * scalar;
}

constexpr double dot(const Vec3& a, const Vec3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

constexpr Vec3 cross(const Vec3& a, const Vec3& b) noexcept {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline double lengthSquared(const Vec3& value) noexcept {
    return dot(value, value);
}

inline double length(const Vec3& value) noexcept {
    return std::sqrt(lengthSquared(value));
}

inline Vec3 normalized(const Vec3& value, const Vec3& fallback = {1.0, 0.0, 0.0}) noexcept {
    const auto magnitude = length(value);
    return magnitude > 1.0e-12 ? value / magnitude : fallback;
}

inline bool isFinite(const Vec3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

inline double clamp01(double value) noexcept {
    return std::clamp(value, 0.0, 1.0);
}

} // namespace threebs
