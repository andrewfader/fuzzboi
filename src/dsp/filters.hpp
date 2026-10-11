// Filters: a Moog-style four-pole ladder with a tanh in the loop, a state-variable filter for
// resonant band-passes and formants, an RBJ biquad for equalisation, and first-order allpasses
// for the phaser.
#pragma once

#include <array>
#include <cmath>

#include "core.hpp"

namespace fuzzboi::dsp {

// Four cascaded one-poles with feedback from the output, saturated once per sample. Oversampled
// twice so the resonance stays usable at high cutoffs. Based on the classic Stilson/Smith
// structure with the Huovilainen-style nonlinearity.
class LadderFilter {
 public:
  void prepare(double sample_rate) { rate_ = sample_rate * 2.0; reset(); }
  void reset() { stage_.fill(0.0f); delay_.fill(0.0f); }
  // cutoff in Hz, resonance 0..1 (self-oscillation near 1), drive >= 1 pushes the input harder.
  void set(double cutoff_hz, double resonance, double drive) {
    const double f = std::clamp(cutoff_hz, 20.0, rate_ * 0.45) / rate_;
    // Tuning polynomial from the Stilson/Smith paper keeps the cutoff accurate below Nyquist/4.
    const double fc = 2.0 * f;
    g_ = static_cast<float>(1.0 - std::exp(-kTau * f));
    // Four is the textbook edge of oscillation; a little more lets the tanh bound a real ring.
    k_ = static_cast<float>(4.3 * std::clamp(resonance, 0.0, 1.0) * (1.0 - 0.15 * fc * fc));
    drive_ = static_cast<float>(std::max(0.1, drive));
  }
  float process(float in) {
    float out = 0.0f;
    for (int pass = 0; pass < 2; ++pass) {
      const float x = std::tanh((in * drive_ - k_ * delay_[3]) * 0.5f) * 2.0f;
      stage_[0] += g_ * (x - stage_[0]);
      stage_[1] += g_ * (stage_[0] - stage_[1]);
      stage_[2] += g_ * (stage_[1] - stage_[2]);
      stage_[3] += g_ * (stage_[2] - stage_[3]);
      // Half-sample delay in the feedback path smooths the loop.
      out = 0.5f * (stage_[3] + delay_[3]);
      delay_ = stage_;
    }
    // Resonance thins the bass; add a little back the way the Minimoog's circuit does.
    return out * (1.0f + 0.5f * k_ / 4.0f);
  }
 private:
  double rate_ = 96000.0;
  float g_ = 0.1f, k_ = 0.0f, drive_ = 1.0f;
  std::array<float, 4> stage_{}, delay_{};
};

// Topology-preserving (trapezoidal) state-variable filter: all three outputs at once. The band
// output is normalised so its peak is unity whatever the Q, which keeps formant and resonator
// levels predictable.
class StateVariable {
 public:
  struct Outputs { float low, band, high; };
  void prepare(double sample_rate) { rate_ = sample_rate; reset(); }
  void reset() { ic1_ = ic2_ = 0.0f; }
  // q from 0.5 (broad) to about 40 (ringing).
  void set(double cutoff_hz, double q) {
    g_ = static_cast<float>(std::tan(kPi * std::clamp(cutoff_hz, 5.0, rate_ * 0.49) / rate_));
    k_ = static_cast<float>(1.0 / std::max(0.5, q));
    a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
    a2_ = g_ * a1_;
    a3_ = g_ * a2_;
  }
  Outputs process(float in) {
    const float v3 = in - ic2_;
    const float v1 = a1_ * ic1_ + a2_ * v3;
    const float v2 = ic2_ + a2_ * ic1_ + a3_ * v3;
    ic1_ = 2.0f * v1 - ic1_;
    ic2_ = 2.0f * v2 - ic2_;
    return {v2, k_ * v1, in - k_ * v1 - v2};
  }
  float low(float in) { return process(in).low; }
  float band(float in) { return process(in).band; }
  // The raw resonator output (peak gain Q): an impulse rings at about 2g regardless of Q, which
  // is what a struck mode wants.
  float ring(float in) { return process(in).band / k_; }
  float g() const { return g_; }
 private:
  double rate_ = 48000.0;
  float g_ = 0.1f, k_ = 1.0f, a1_ = 1.0f, a2_ = 0.0f, a3_ = 0.0f, ic1_ = 0.0f, ic2_ = 0.0f;
};

// RBJ biquad (direct form I), for peaking, shelving and simple low/high-passes.
class Biquad {
 public:
  enum class Type { LowPass, HighPass, Peak, LowShelf, HighShelf, BandPass };
  void reset() { x1_ = x2_ = y1_ = y2_ = 0.0f; }
  void set(Type type, double hz, double q, double gain_db, double sample_rate) {
    const double w = kTau * std::clamp(hz, 5.0, sample_rate * 0.49) / sample_rate;
    const double cw = std::cos(w), sw = std::sin(w);
    const double alpha = sw / (2.0 * std::max(0.05, q));
    const double A = std::pow(10.0, gain_db / 40.0);
    double b0, b1, b2, a0, a1, a2;
    switch (type) {
      case Type::LowPass: b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
      case Type::HighPass: b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
      case Type::BandPass: b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
      case Type::Peak: b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
      case Type::LowShelf: {
        const double s = 2 * std::sqrt(A) * alpha;
        b0 = A * ((A + 1) - (A - 1) * cw + s); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - s);
        a0 = (A + 1) + (A - 1) * cw + s; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - s;
        break;
      }
      default: {
        const double s = 2 * std::sqrt(A) * alpha;
        b0 = A * ((A + 1) + (A - 1) * cw + s); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - s);
        a0 = (A + 1) - (A - 1) * cw + s; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - s;
        break;
      }
    }
    b0_ = static_cast<float>(b0 / a0); b1_ = static_cast<float>(b1 / a0); b2_ = static_cast<float>(b2 / a0);
    a1_ = static_cast<float>(a1 / a0); a2_ = static_cast<float>(a2 / a0);
  }
  float process(float in) {
    const float out = b0_ * in + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;
    x2_ = x1_; x1_ = in; y2_ = y1_; y1_ = out;
    return out;
  }
 private:
  float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f, a1_ = 0.0f, a2_ = 0.0f;
  float x1_ = 0.0f, x2_ = 0.0f, y1_ = 0.0f, y2_ = 0.0f;
};

// First-order allpass: unity magnitude, phase swinging through 90 degrees at the corner. Chains
// of these are what a phaser pedal sweeps.
class Allpass1 {
 public:
  void set(double corner_hz, double sample_rate) {
    const double t = std::tan(kPi * std::clamp(corner_hz, 10.0, sample_rate * 0.49) / sample_rate);
    a_ = static_cast<float>((t - 1.0) / (t + 1.0));
  }
  float process(float in) {
    const float out = a_ * in + z_;
    z_ = in - a_ * out;
    return out;
  }
  void reset() { z_ = 0.0f; }
 private:
  float a_ = 0.0f, z_ = 0.0f;
};

}  // namespace fuzzboi::dsp
