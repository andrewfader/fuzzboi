// Envelopes and low-frequency oscillators.
#pragma once

#include <cmath>

#include "core.hpp"

namespace fuzzboi::dsp {

// ADSR with analog-style curves: the attack is a fast exponential approach that overshoots its
// target so it reaches 1.0 in the stated time, decay and release approach exponentially. Times
// are in milliseconds and may change while the envelope runs.
class Adsr {
 public:
  enum class Stage { Idle, Attack, Decay, Sustain, Release };

  void prepare(double sample_rate) { rate_ = sample_rate; }
  void set(double attack_ms, double decay_ms, double sustain, double release_ms) {
    attack_ = attack_coefficient(attack_ms); decay_ = coefficient(decay_ms); release_ = coefficient(release_ms);
    sustain_ = std::clamp(sustain, 0.0, 1.0);
    attack_instant_ = attack_ms <= 0.0;
  }
  void note_on() { stage_ = Stage::Attack; if (attack_instant_) { level_ = 1.0; stage_ = Stage::Decay; } }
  void note_off() { if (stage_ != Stage::Idle) stage_ = Stage::Release; }
  void kill() { stage_ = Stage::Idle; level_ = 0.0; }
  bool active() const { return stage_ != Stage::Idle; }
  bool releasing() const { return stage_ == Stage::Release; }
  Stage stage() const { return stage_; }
  double level() const { return level_; }

  double next() {
    switch (stage_) {
      case Stage::Attack:
        // Aim 30% above full scale so the curve hits 1.0 rather than crawling toward it.
        level_ += (1.3 - level_) * attack_;
        if (level_ >= 1.0) { level_ = 1.0; stage_ = Stage::Decay; }
        break;
      case Stage::Decay:
        level_ += (sustain_ - level_) * decay_;
        if (std::abs(level_ - sustain_) < 1e-4) { level_ = sustain_; stage_ = Stage::Sustain; }
        break;
      case Stage::Sustain:
        level_ = sustain_;
        break;
      case Stage::Release:
        level_ += (0.0 - level_) * release_;
        if (level_ < 1e-4) { level_ = 0.0; stage_ = Stage::Idle; }
        break;
      default: level_ = 0.0; break;
    }
    return level_;
  }

 private:
  // Decay and release are stated as the time to fall 60 dB (6.9 time constants), the way
  // reverb and release times are conventionally quoted.
  double coefficient(double ms) const {
    const double samples = std::max(1.0, ms * 0.001 * rate_);
    return 1.0 - std::exp(-6.9 / samples);
  }
  // The attack aims 30% past full scale and stops at 1.0, so it arrives in exactly `ms`:
  // 1.3 (1 - e^(-t/tau)) = 1 at t = tau ln(1.3 / 0.3).
  double attack_coefficient(double ms) const {
    const double samples = std::max(1.0, ms * 0.001 * rate_);
    return 1.0 - std::exp(-std::log(1.3 / 0.3) / samples);
  }
  double rate_ = 48000.0, attack_ = 1.0, decay_ = 1.0, release_ = 1.0, sustain_ = 1.0, level_ = 0.0;
  bool attack_instant_ = false;
  Stage stage_ = Stage::Idle;
};

// Simple exponential decay from 1 toward 0, for percussion and key clicks.
class Decay {
 public:
  void prepare(double sample_rate) { rate_ = sample_rate; }
  void trigger(double ms) { level_ = 1.0; coefficient_ = 1.0 - std::exp(-4.0 / std::max(1.0, ms * 0.001 * rate_)); }
  double next() { level_ -= level_ * coefficient_; return level_; }
  double level() const { return level_; }
  bool active() const { return level_ > 1e-4; }
 private:
  double rate_ = 48000.0, level_ = 0.0, coefficient_ = 1.0;
};

enum class LfoShape : int { Sine, Triangle, Square, Random };

class Lfo {
 public:
  void prepare(double sample_rate) { rate_ = sample_rate; }
  void reset(double phase = 0.0) { phase_ = phase; }
  double phase() const { return phase_; }
  // Returns a value in [-1, 1].
  double next(double hz, LfoShape shape = LfoShape::Sine) {
    double out;
    switch (shape) {
      case LfoShape::Triangle: out = phase_ < 0.5 ? 4.0 * phase_ - 1.0 : 3.0 - 4.0 * phase_; break;
      case LfoShape::Square: out = phase_ < 0.5 ? 1.0 : -1.0; break;
      case LfoShape::Random: out = held_; break;
      default: out = std::sin(kTau * phase_); break;
    }
    phase_ += std::max(0.0, hz) / rate_;
    if (phase_ >= 1.0) { phase_ -= 1.0; held_ = rng_.bipolar(); }
    return out;
  }
 private:
  double rate_ = 48000.0, phase_ = 0.0, held_ = 0.0;
  Rng rng_{0x1234567u};
};

}  // namespace fuzzboi::dsp
