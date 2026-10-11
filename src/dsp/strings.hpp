// The jangle: a Rickenbacker-style 12-string as plucked-string pairs. Each course is a
// Karplus-Strong loop (a delay the length of the period with a damping filter in the loop) in
// the Jaffe/Smith extended form: the pick position is a feed-forward comb on the excitation
// (picking at a fraction p of the string notches every harmonic that has a node there), the
// loop length follows the pitch it is given so bends, vibrato and glides re-tune the ringing
// string instead of being ignored, higher notes decay faster as real strings do, and a harder
// pick (velocity) is brighter. The second string of the course is tuned an octave up on the four
// lower courses, as on the instrument, and slightly detuned from its partner so the pair
// shimmers. The output goes through either an electric pickup's resonance or a wooden body's
// resonances, by the Body control.
#pragma once

#include <array>
#include <cmath>

#include "core.hpp"
#include "envelopes.hpp"
#include "filters.hpp"

namespace fuzzboi::dsp {

struct JangleSettings {
  double octave = 0.7;   // level of the octave string
  double bright = 0.7;   // pick brightness
  double decay_s = 3.0;  // time for an A3 string to fall 60 dB
  double pick = 0.3;     // pick position, 0 at the bridge (thin, all harmonics) to 1 at the middle (hollow, even harmonics notched)
  double body = 0.3;     // 0 an electric pickup's resonance, 1 a wooden acoustic body
};

class PluckedString {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    line_.prepare(static_cast<size_t>(sample_rate / 20.0) + 4);
    comb_.prepare(static_cast<size_t>(sample_rate / 20.0) + 4);
    damping_.set(6000.0, sample_rate);
  }
  // `pick` is the fraction of the string length the pick strikes at (0.02 .. 0.5).
  void pluck(double hz, double bright, double decay_s, Rng& rng, double strength = 1.0, double pick = 0.15) {
    hz_ = std::max(20.0, hz);
    period_ = rate_ / hz_;
    // Loop gain per period for the asked-for decay: 60 dB over decay_s seconds at A3, shorter
    // for higher strings and longer for lower ones, as on a real neck.
    const double seconds = decay_s * std::clamp(std::pow(220.0 / hz_, 0.35), 0.4, 1.6);
    const double periods = seconds * hz_;
    gain_ = static_cast<float>(std::pow(1e-3, 1.0 / std::max(1.0, periods)));
    const double corner = 1500.0 + 9000.0 * bright;
    damping_.set(corner, rate_);
    // The loop is one push plus the read delay plus the damping filter's delay; take the
    // filter's share out of the read so the period, and so the pitch, is what was asked for. For
    // the one-pole y = a x + (1 - a) y1 the low-frequency delay is (1 - a) / a samples.
    const double a = 1.0 - std::exp(-kTau * corner / rate_);
    lag_ = (1.0 - a) / a;
    line_.clear();
    comb_.clear();
    // Excite with a burst shaped by the pick: brighter picks get whiter noise. The pick
    // position comb (a delay of pick * period subtracted) puts the nodes where the pick was.
    // Two poles on the burst stand in for the pick's own low-pass: a pick is not a hammer, and
    // a whiter burst only adds a first-period spike that the loop loses within milliseconds.
    OnePole shape, shape2; shape.set(400.0 + 3600.0 * bright, rate_); shape2.set(600.0 + 5400.0 * bright, rate_);
    const int n = static_cast<int>(period_) + 1;
    const double pick_delay = std::clamp(pick, 0.02, 0.5) * period_;
    // The burst is soft-limited: a pick displaces a string by a bounded amount, and without
    // the limit the first period's noise peak sat 15 to 20 dB above the ringing that follows.
    for (int i = 0; i < n; ++i) {
      const float raw = shape2.low(shape.low(rng.bipolar())) * static_cast<float>(1.8 * strength);
      comb_.push(raw);
      line_.push(warm_clip(2.2f * (raw - 0.95f * comb_.read(pick_delay))) * 0.45f);
    }
    ringing_ = true;
  }
  // Reads one period of the given pitch back (fractional), filters, feeds back. A pitch other
  // than the plucked one re-tunes the ringing string: that is a bend.
  float next(double hz) {
    if (!ringing_) return 0.0f;
    const double period = rate_ / std::clamp(hz, 20.0, rate_ * 0.45);
    const float delayed = line_.read(std::max(1.0, period - 1.0 - lag_));
    const float out = damping_.low(delayed) * gain_;
    line_.push(out);
    return out;
  }
 private:
  double rate_ = 48000.0, hz_ = 440.0, period_ = 100.0, lag_ = 0.0;
  float gain_ = 0.99f;
  DelayLine line_, comb_;
  OnePole damping_;
  bool ringing_ = false;
};

class JangleVoice {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    for (auto& s : strings_) s.prepare(sample_rate);
    // Electric: a single-coil pickup's resonance, a peak near 3.5 kHz falling away above it.
    pickup_.prepare(sample_rate);
    pickup_.set(3500.0, 1.6);
    // Acoustic: the air resonance near 100 Hz, a wood resonance near 220 Hz, and a top that
    // stops at the soundboard's reach.
    air_.set(Biquad::Type::Peak, 100.0, 3.0, 6.0, sample_rate);
    wood_.set(Biquad::Type::Peak, 220.0, 2.0, 3.0, sample_rate);
    top_.set(Biquad::Type::LowPass, 5500.0, 0.7, 0.0, sample_rate);
    click_.prepare(sample_rate);
    click_tone_.set(2500.0, sample_rate);
  }
  // `velocity` 0..1: a harder pick is brighter and louder at the attack.
  void note_on(const JangleSettings& s, double hz, double note, double velocity = 1.0) {
    // Below the B course (MIDI 71) the pair is an octave apart; above it is unison.
    const bool octave_pair = note < 71.0;
    octave_level_ = static_cast<float>(octave_pair ? s.octave : 0.85);
    const double bright = std::clamp(s.bright * (0.55 + 0.45 * velocity), 0.0, 1.0);
    const double pick = 0.02 + 0.48 * std::clamp(s.pick, 0.0, 1.0);
    strings_[0].pluck(hz, bright, s.decay_s, rng_, 1.0, pick);
    // The thin octave string is plucked harder: on the instrument it is what makes the jangle.
    octave_ratio_ = octave_pair ? 2.0 : 1.0;
    strings_[1].pluck(hz * octave_ratio_ * cents_ratio(6.0), bright, s.decay_s * 0.8, rng_, 2.0, pick * 0.9);
    click_.trigger(2.5);
    click_level_ = static_cast<float>(0.12 * velocity * (0.5 + 0.5 * s.bright));
    hz_ = hz;
  }
  // `hz` is the pitch the voice wants now (bend, vibrato and glide included).
  float next(const JangleSettings& s, double hz) {
    const float a = strings_[0].next(hz);
    const float b = strings_[1].next(hz * octave_ratio_ * cents_ratio(6.0)) * octave_level_;
    // The pick meeting the string: a 2.5 ms bright click.
    const float click = click_tone_.high(rng_.bipolar()) * static_cast<float>(click_.next()) * click_level_;
    const float dry = (a + b) * 1.5f + click;
    const float electric = pickup_.low(dry) * 1.15f;
    const float acoustic = top_.process(wood_.process(air_.process(dry))) * 0.9f;
    const float body = static_cast<float>(std::clamp(s.body, 0.0, 1.0));
    return electric * (1.0f - body) + acoustic * body;
  }

 private:
  double rate_ = 48000.0, hz_ = 220.0, octave_ratio_ = 2.0;
  std::array<PluckedString, 2> strings_{};
  StateVariable pickup_;
  Biquad air_, wood_, top_;
  Decay click_;
  OnePole click_tone_;
  Rng rng_{0x12571257u};
  float octave_level_ = 0.7f, click_level_ = 0.0f;
};

}  // namespace fuzzboi::dsp
