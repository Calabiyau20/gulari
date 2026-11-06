#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace pg
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;
constexpr int kControlBlock = 16;   // samples between modulation updates

inline float clamp01 (float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float clampf (float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float semisToRatio (float s) { return std::exp2 (s * (1.0f / 12.0f)); }
inline float midiToHz (float n) { return 440.0f * std::exp2 ((n - 69.0f) * (1.0f / 12.0f)); }
inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

inline float fastTanh (float x)
{
    if (x > 4.97f) return 1.0f;
    if (x < -4.97f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// 4-point, 3rd-order Hermite interpolation
inline float hermite (float xm1, float x0, float x1, float x2, float t)
{
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b = w + a;
    return ((((a * t) - b) * t + c) * t + x0);
}

inline void panGains (float pan, float& l, float& r)
{
    const float p = (clampf (pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
    l = std::cos (p);
    r = std::sin (p);
}

// Small, fast, deterministic RNG (xorshift32)
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    void seed (uint32_t v) { s = v ? v : 0x9E3779B9u; }
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // [0,1)
    float bi() { return uni() * 2.0f - 1.0f; }                              // [-1,1)
};

inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

// One-pole smoother
struct Smooth
{
    float y = 0.0f, a = 0.0f;
    void setTime (float seconds, float rate) { a = seconds <= 0.0f ? 0.0f : std::exp (-1.0f / (seconds * rate)); }
    float process (float x) { y = x + (y - x) * a; return y; }
};
} // namespace pg
