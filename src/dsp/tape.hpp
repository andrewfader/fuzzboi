// Tape, three ways: the mechanics of a Mellotron key (a strip of tape that runs out), the
// character of a tape path (wow, flutter, hiss, head loss, saturation), and the degradation of
// bouncing a mix between cassette decks generation after generation, the way Olivia Tremor
// Control built up tracks on a double-cassette boombox.
#pragma once

#include <cmath>

#include "core.hpp"
#include "envelopes.hpp"
#include "filters.hpp"

namespace fuzzboi::dsp {

// Wow (slow, around 0.5-2 Hz) and flutter (faster, 6-12 Hz, slightly irregular) as a speed
// ratio around 1.0. Depths are 0..1; at 1 the wow swings about +-1.2% and the flutter +-0.3%,
// which is already a badly worn machine.
class WowFlutter {
 public:
  void prepare(double sample_rate) { wow_.prepare(sample_rate); flutter_.prepare(sample_rate); drift_.prepare(sample_rate); }
  double next(double wow_depth, double flutter_depth) {
    const double wow = wow_.next(0.8) * 0.012 * wow_depth;
    const double flutter = flutter_.next(8.7 + 1.5 * drift_.next(0.3, LfoShape::Triangle)) * 0.003 * flutter_depth;
    return 1.0 + wow + flutter;
  }
 private:
  Lfo wow_, flutter_, drift_;
};

// A Mellotron-style key: the tape starts when the key goes down and stops dead after
// `length_seconds` (8 on the real instrument; 9 here means endless), rewinding when released.
// Playback position is counted here; the caller renders whatever is "on the tape" at that
// position (a sampled note or a synthesized one) and multiplies by `gain`.
class TapeKey {
 public:
  void prepare(double sample_rate) { rate_ = sample_rate; }
  void start() { position_ = 0.0; running_ = true; }
  void stop() { running_ = false; }
  bool running() const { return running_; }
  double position_seconds() const { return position_; }
  // Advances by one sample at the given speed ratio and returns the gain: 1 while tape remains,
  // a quick fade over the last 60 ms so the end does not click, 0 once it has run out.
  double advance(double speed, double length_seconds) {
    if (!running_) return 0.0;
    position_ += speed / rate_;
    if (length_seconds >= 9.0) return 1.0;
    const double left = length_seconds - position_;
    if (left <= 0.0) { running_ = false; return 0.0; }
    return std::min(1.0, left / 0.06);
  }
 private:
  double rate_ = 48000.0, position_ = 0.0;
  bool running_ = false;
};

// The sound of the path: head-bump-free gentle low-pass for worn tape ("age"), a touch of
// soft saturation ("drive"), and a hiss floor shaped like real tape noise (pinkish).
class TapePath {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    corner_ = 0.0;
    set(0.0, 18000.0);
    hiss_tilt_.set(6000.0, sample_rate);
    dc_.set(8.0, sample_rate);
  }
  // age 0..1 lowers the head loss corner from 18 kHz down to about 3 kHz; bandwidth_hz caps it.
  // The loss is two poles (12 dB per octave), as a worn head and the tape's own loss stack up.
  void set(double age, double bandwidth_hz) {
    const double corner = std::min(bandwidth_hz, 18000.0 * std::pow(3000.0 / 18000.0, std::clamp(age, 0.0, 1.0)));
    if (std::abs(corner - corner_) < corner_ * 0.01) return;
    corner_ = corner;
    loss_.set(Biquad::Type::LowPass, corner, 0.6, 0.0, rate_);
  }
  float process(float in, double drive, double hiss) {
    const float hot = in * static_cast<float>(1.0 + 4.0 * drive);
    float out = drive > 0.0 ? warm_clip(hot) / static_cast<float>(1.0 + 1.5 * drive) : in;
    out = loss_.process(out);
    // Full hiss is -40 dBFS: a worn cassette rather than a studio machine, by design.
    if (hiss > 0.0) out += hiss_tilt_.low(rng_.bipolar()) * static_cast<float>(hiss * 0.012);
    return dc_.process(out);
  }
 private:
  double rate_ = 48000.0, corner_ = 0.0;
  Biquad loss_;
  OnePole hiss_tilt_;
  DcBlocker dc_;
  Rng rng_{0x7a9e5eedu};
};

// Cassette-to-cassette generations: each bounce loses more top end, adds its own hiss and a
// little flutter. Applied as N stacked passes of a cheap per-generation model.
class Generations {
 public:
  static constexpr int kMax = 6;
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    for (auto& g : loss_) g.set(12000.0, sample_rate);
    flutter_.prepare(sample_rate);
    line_.prepare(static_cast<size_t>(sample_rate * 0.01));
  }
  // Returns the speed ratio the caller should apply to any pitched read it does; the
  // generations themselves modulate a short delay to put flutter on the whole mix.
  float process(float in, int generations) {
    const int n = std::clamp(generations, 0, kMax);
    if (n == 0) return in;
    float x = in;
    for (int g = 0; g < n; ++g) {
      // Cassette duplication: ~10 kHz at first copy, falling faster with each pass.
      loss_[g].set(12000.0 * std::pow(0.72, g), rate_);
      x = loss_[g].low(warm_clip(x * 1.15f) * 0.9f);
      // Each copy adds its own hiss: about -62 dBFS per generation, a decent home deck. The
      // master Hiss knob is there for a worse one.
      x += rng_.bipolar() * 0.0008f;
    }
    // One flutter path shared by the stack, deeper with more generations.
    line_.push(x);
    const double speed = flutter_.next(0.0, 0.5 + 0.1 * n) - 1.0;
    return line_.read(2.0 + 200.0 * speed * n);
  }
 private:
  double rate_ = 48000.0;
  std::array<OnePole, kMax> loss_{};
  WowFlutter flutter_;
  DelayLine line_;
  Rng rng_{0xca55e77eu};
};

}  // namespace fuzzboi::dsp
