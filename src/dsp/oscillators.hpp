// Band-limited analog-style oscillators. The saw and pulse use PolyBLEP corrections at their
// discontinuities; the triangle integrates a pulse, which is how analog triangle cores work and
// gives it the same band-limiting for free.
#pragma once

#include <cmath>

#include "core.hpp"

namespace fuzzboi::dsp {

enum class Wave : int { Saw = 0, Pulse = 1, Triangle = 2, Sine = 3 };

// Two-sample polynomial correction applied around a step at phase 0 (phase in [0, 1), increment
// per sample).
inline float poly_blep(double phase, double increment) {
  if (increment <= 0.0) return 0.0f;
  if (phase < increment) {
    const double x = phase / increment;
    return static_cast<float>(x + x - x * x - 1.0);
  }
  if (phase > 1.0 - increment) {
    const double x = (phase - 1.0) / increment;
    return static_cast<float>(x * x + x + x + 1.0);
  }
  return 0.0f;
}

inline float saw(double phase, double increment) {
  return static_cast<float>(2.0 * phase - 1.0) - poly_blep(phase, increment);
}

inline float pulse(double phase, double increment, double duty) {
  float value = phase < duty ? 1.0f : -1.0f;
  value += poly_blep(phase, increment);
  double falling = phase - duty;
  if (falling < 0.0) falling += 1.0;
  value -= poly_blep(falling, increment);
  return value;
}

inline float naive_triangle(double phase) {
  return static_cast<float>(phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase);
}

// One oscillator with its own phase, and a leaky integrator for the triangle.
class Oscillator {
 public:
  void reset(double phase = 0.0) { phase_ = phase; integrator_ = 0.0f; }
  void set_phase(double phase) { phase_ = phase - std::floor(phase); }
  double phase() const { return phase_; }

  // Advances one sample at `hz` and returns the waveform in roughly [-1, 1].
  float next(Wave wave, double hz, double sample_rate, double duty = 0.5) {
    const double increment = std::clamp(hz / sample_rate, 0.0, 0.5);
    float out;
    switch (wave) {
      case Wave::Saw: out = saw(phase_, increment); break;
      case Wave::Pulse: out = pulse(phase_, increment, std::clamp(duty, 0.02, 0.98)); break;
      case Wave::Triangle: {
        // Integrating a 50% pulse gives a triangle; the leak keeps it centred.
        const float square = pulse(phase_, increment, 0.5);
        integrator_ = 0.999f * integrator_ + static_cast<float>(4.0 * increment) * square;
        out = integrator_ * 1.0f;
        break;
      }
      default: out = static_cast<float>(std::sin(kTau * phase_)); break;
    }
    phase_ += increment;
    if (phase_ >= 1.0) phase_ -= 1.0;
    return out;
  }

 private:
  double phase_ = 0.0;
  float integrator_ = 0.0f;
};

}  // namespace fuzzboi::dsp
