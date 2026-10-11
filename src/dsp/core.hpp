// Small building blocks every other DSP header leans on: constants, conversions, a one-pole
// filter, a DC blocker, a parameter smoother, a fractional delay line and a deterministic random
// generator. Everything is header-only, allocation-free once constructed, and written per sample.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fuzzboi::dsp {

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kTau = 2.0 * kPi;

inline double midi_to_hz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
inline double hz_to_midi(double hz) { return 69.0 + 12.0 * std::log2(hz / 440.0); }
inline double db_to_gain(double db) { return std::pow(10.0, db / 20.0); }
inline double gain_to_db(double gain) { return 20.0 * std::log10(std::max(gain, 1e-9)); }
inline double cents_ratio(double cents) { return std::exp2(cents / 1200.0); }
inline double lerp(double a, double b, double t) { return a + (b - a) * t; }
inline float soft_clip(float x) { return std::tanh(x); }
// Gentler than tanh around zero and still bounded: the "warm" curve for tape and preamps.
inline float warm_clip(float x) { return x / std::sqrt(1.0f + x * x); }

// xorshift32: fast, deterministic, the same stream on every platform. Never used for anything
// that needs real randomness; the tests rely on repeatability.
class Rng {
 public:
  explicit Rng(uint32_t seed = 0x9e3779b9u) : state_(seed ? seed : 1u) {}
  uint32_t next() {
    state_ ^= state_ << 13; state_ ^= state_ >> 17; state_ ^= state_ << 5;
    return state_;
  }
  // Uniform in [-1, 1).
  float bipolar() { return static_cast<float>(next() >> 8) * (2.0f / 16777216.0f) - 1.0f; }
  // Uniform in [0, 1).
  float unipolar() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
  void seed(uint32_t s) { state_ = s ? s : 1u; }
 private:
  uint32_t state_;
};

// One-pole low-pass (and its high-pass complement), set by cutoff in Hz.
class OnePole {
 public:
  void set(double cutoff_hz, double sample_rate) {
    const double x = std::exp(-kTau * std::clamp(cutoff_hz, 1.0, sample_rate * 0.49) / sample_rate);
    a_ = static_cast<float>(1.0 - x); b_ = static_cast<float>(x);
  }
  float low(float in) { state_ = a_ * in + b_ * state_; return state_; }
  float high(float in) { return in - low(in); }
  void reset() { state_ = 0.0f; }
  float state() const { return state_; }
 private:
  float a_ = 1.0f, b_ = 0.0f, state_ = 0.0f;
};

// Removes DC with a very low high-pass corner; the standard y = x - x1 + R y1.
class DcBlocker {
 public:
  void set(double corner_hz, double sample_rate) {
    r_ = static_cast<float>(1.0 - kTau * corner_hz / sample_rate);
  }
  float process(float in) {
    const float out = in - x1_ + r_ * y1_;
    x1_ = in; y1_ = out;
    return out;
  }
  void reset() { x1_ = y1_ = 0.0f; }
 private:
  float r_ = 0.995f, x1_ = 0.0f, y1_ = 0.0f;
};

// Exponential approach to a target, for de-zippering parameters. `time_ms` is the time to
// cover about 63% of a step.
class Smoother {
 public:
  void set_time(double time_ms, double sample_rate) {
    coefficient_ = time_ms <= 0.0 ? 1.0 : 1.0 - std::exp(-1000.0 / (time_ms * sample_rate));
  }
  void snap(double value) { value_ = target_ = value; }
  void target(double value) { target_ = value; }
  double next() { value_ += (target_ - value_) * coefficient_; return value_; }
  double value() const { return value_; }
  bool settled(double tolerance = 1e-6) const { return std::abs(value_ - target_) <= tolerance; }
 private:
  double coefficient_ = 1.0, value_ = 0.0, target_ = 0.0;
};

// A circular buffer read at a fractional delay with linear interpolation. The size is fixed at
// prepare time so the audio thread never allocates.
class DelayLine {
 public:
  void prepare(size_t max_samples) {
    buffer_.assign(std::max<size_t>(max_samples + 2, 4), 0.0f);
    write_ = 0;
  }
  void clear() { std::fill(buffer_.begin(), buffer_.end(), 0.0f); }
  size_t capacity() const { return buffer_.empty() ? 0 : buffer_.size() - 2; }
  void push(float sample) {
    if (buffer_.empty()) return;
    buffer_[write_] = sample;
    write_ = (write_ + 1) % buffer_.size();
  }
  // Sample written `delay` samples ago (0 = the most recent push), interpolated.
  float read(double delay) const {
    if (buffer_.empty()) return 0.0f;
    const double clamped = std::clamp(delay, 0.0, static_cast<double>(capacity()));
    const size_t whole = static_cast<size_t>(clamped);
    const float frac = static_cast<float>(clamped - static_cast<double>(whole));
    const size_t n = buffer_.size();
    const size_t i0 = (write_ + n - 1 - whole) % n;
    const size_t i1 = (i0 + n - 1) % n;
    return buffer_[i0] + (buffer_[i1] - buffer_[i0]) * frac;
  }
  float read_int(size_t delay) const {
    if (buffer_.empty()) return 0.0f;
    const size_t n = buffer_.size();
    return buffer_[(write_ + n - 1 - std::min(delay, capacity())) % n];
  }
 private:
  std::vector<float> buffer_;
  size_t write_ = 0;
};

}  // namespace fuzzboi::dsp
