// The master section: lo-fi reduction, backwards playback, the amp-feedback voice, the tempo
// pump, and the master tape machine (drive, wow and flutter, hiss, bandwidth, cassette generations).
#pragma once

#include <cmath>

#include "../core.hpp"
#include "../filters.hpp"
#include "../tape.hpp"

namespace fuzzboi::dsp {

// Bit depth reduction with the step quantiser a digital toy or an early sampler would have,
// and sample-rate reduction by sample-and-hold (no anti-aliasing: the aliasing is the point).
class LoFi {
 public:
  void prepare(double sample_rate) { rate_ = sample_rate; hold_ = 0.0f; phase_ = 0.0; }
  float process(float in, int bits, double rate_hz) {
    if (bits >= 16 && rate_hz >= rate_) return in;
    phase_ += std::clamp(rate_hz, 100.0, rate_) / rate_;
    if (phase_ >= 1.0) {
      phase_ -= 1.0;
      const double steps = std::exp2(std::clamp(bits, 2, 16) - 1);
      hold_ = static_cast<float>(std::round(static_cast<double>(in) * steps) / steps);
    }
    return hold_;
  }
 private:
  double rate_ = 48000.0, phase_ = 0.0;
  float hold_ = 0.0f;
};

// Backwards: the audio is recorded in chunks and each chunk is played out reversed while the
// next is recorded, so the output is a continuous stream of reversed slices. The classic
// backwards-tape effect of 1966 made polite enough to run live.
class Backwards {
 public:
  static constexpr double kMaxSeconds = 2.0;
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    buffer_.assign(static_cast<size_t>(sample_rate * kMaxSeconds) * 2, 0.0f);
    write_ = 0; read_ = 0; chunk_ = static_cast<size_t>(sample_rate * 0.5); half_ = 0;
    fade_.set(300.0, sample_rate);
  }
  float process(float in, double mix, double chunk_ms) {
    if (mix <= 0.0 && !ever_on_) return in;
    ever_on_ = true;
    const size_t wanted = std::clamp(static_cast<size_t>(chunk_ms * 0.001 * rate_), static_cast<size_t>(64), buffer_.size() / 2);
    // Two halves: write into one while reading the other backwards.
    const size_t base_write = half_ * (buffer_.size() / 2), base_read = (1 - half_) * (buffer_.size() / 2);
    buffer_[base_write + write_] = in;
    const float reversed = buffer_[base_read + (chunk_ - 1 - std::min(read_, chunk_ - 1))];
    ++write_; ++read_;
    if (write_ >= chunk_) { write_ = 0; read_ = 0; half_ = 1 - half_; chunk_ = wanted; }
    // A short fade at each chunk edge hides the splice.
    const float edge = static_cast<float>(std::min<size_t>(read_, chunk_ - read_)) / 96.0f;
    const float wet = reversed * std::min(1.0f, edge);
    return in * static_cast<float>(1.0 - mix) + wet * static_cast<float>(mix);
  }
 private:
  double rate_ = 48000.0;
  std::vector<float> buffer_;
  size_t write_ = 0, read_ = 0, chunk_ = 24000, half_ = 0;
  OnePole fade_;
  bool ever_on_ = false;
};

// The guitar-held-to-the-amp feedback: the loudest harmonic of the played note builds up over
// a second or two until it dominates. Model: a comb (a delay tuned to the chosen harmonic of
// the note) with loop gain just over unity and a soft limiter, blended in with a swell
// envelope while a note is held and released when it ends.
class FeedbackVoice {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    line_.prepare(static_cast<size_t>(sample_rate / 20.0) + 4);
    damp_.set(5000.0, sample_rate);
    // The loop must not pass DC: a comb with gain above unity would otherwise settle on a
    // silent DC fixed point instead of howling. Pickups and speakers block it anyway.
    loop_hp_.set(120.0, sample_rate);
    swell_.set_time(800.0, sample_rate);
    swell_.snap(0.0);
    dc_.set(10.0, sample_rate);
  }
  void clear() { line_.clear(); swell_.snap(0.0); }
  // note_hz is the (lowest) held note, 0 when nothing is held.
  float process(float in, double amount, int harmonic, double swell_ms, double note_hz) {
    if (amount <= 0.0) { swell_.snap(0.0); line_.push(in * 0.1f); return in; }
    swell_.set_time(std::max(10.0, swell_ms) * 0.4, rate_);
    swell_.target(note_hz > 0.0 ? 1.0 : 0.0);
    const double s = swell_.next();
    if (note_hz > 0.0) {
      const double wanted = note_hz * (1 + std::clamp(harmonic, 0, 3));
      if (wanted != hz_) {
        hz_ = wanted;
        // The loop's low-pass sits just above the chosen harmonic so the comb's first peak wins
        // over its multiples: the howl lands on the harmonic asked for, not two octaves up.
        const double corner = std::clamp(hz_ * 1.6, 100.0, rate_ * 0.45);
        damp_.set(corner, rate_);
        // The low-pass delays the loop a little; shorten the comb by its group delay so the
        // resonance still lands on the harmonic itself.
        const double ratio = hz_ / corner, hp_ratio = hz_ / 120.0;
        const double lp_lag = rate_ / (kTau * corner) / (1.0 + ratio * ratio);
        const double hp_lead = rate_ / (kTau * 120.0) / (1.0 + hp_ratio * hp_ratio);
        lag_samples_ = lp_lag - hp_lead;
      }
    }
    const double period = rate_ / std::max(20.0, hz_);
    const float ring = line_.read(std::max(1.0, period - 1.0 - lag_samples_));
    // Loop gain rises with the swell: below unity it is a resonance, above it the howl grows
    // until the limiter holds it.
    const float gain = static_cast<float>(0.6 + 0.75 * s * amount);
    // An amplifier is never silent: the loop's own noise is what the howl grows from, exactly as
    // on stage, so a note whose harmonics miss the comb still feeds back.
    const float hiss = rng_.bipolar() * 2e-4f;
    const float fed = warm_clip((in * 0.6f + hiss + loop_hp_.high(damp_.low(ring)) * gain) * 1.5f) * 0.7f;
    line_.push(fed);
    const float howl = dc_.process(ring) * static_cast<float>(s * amount);
    return in + howl * 0.9f;
  }
 private:
  double rate_ = 48000.0, hz_ = 220.0, lag_samples_ = 0.0;
  DelayLine line_;
  OnePole damp_, loop_hp_;
  Smoother swell_;
  DcBlocker dc_;
  Rng rng_{0xfeedbacu};
};

// Pump: the sidechain-compressor duck that Tame Impala records run everything through, with no
// kick drum needed. On every beat of the chosen division the gain drops by `depth` and then
// recovers over the beat, so pads and echoes breathe in time with the host. The envelope is
// smoothed so the duck is a swell, not a click, and the clock free-runs from the engine's
// reset at the host tempo.
class Pump {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate; phase_ = 0.0; envelope_ = 0.0; gain_ = 1.0;
    // The duck's edge is smoothed over about 1 ms so it never clicks.
    smoothing_ = 1.0 - std::exp(-kTau * 250.0 / sample_rate);
  }
  void reset() { phase_ = 0.0; envelope_ = 0.0; gain_ = 1.0; }
  double phase() const { return phase_; }
  // depth 0..1, period_s the beat length. Advances one sample and returns the gain to apply
  // (one clock serves both channels). At zero depth the gain is exactly one.
  float gain(double depth, double period_s) {
    const double period = std::max(0.02, period_s);
    phase_ += 1.0 / (period * rate_);
    if (phase_ >= 1.0) { phase_ -= std::floor(phase_); envelope_ = 1.0; }
    // Recover over about 60% of the beat (time to fall 60 dB), the classic slow-release setting.
    envelope_ *= std::exp(-6.9 / (0.6 * period * rate_));
    if (depth <= 0.0) { gain_ = 1.0; return 1.0f; }
    gain_ += (1.0 - std::clamp(depth, 0.0, 1.0) * envelope_ - gain_) * smoothing_;
    return static_cast<float>(gain_);
  }
  float process(float in, double depth, double period_s) { return in * gain(depth, period_s); }
 private:
  double rate_ = 48000.0, phase_ = 0.0, envelope_ = 0.0, gain_ = 1.0, smoothing_ = 0.03;
};

// The master tape machine: a TapePath for drive, loss and hiss, wow and flutter on a short
// modulated delay, and the cassette generations stack.
class TapeMachine {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    path_.prepare(sample_rate);
    speed_.prepare(sample_rate);
    line_.prepare(static_cast<size_t>(sample_rate * 0.02) + 4);
    generations_.prepare(sample_rate);
  }
  float process(float in, double drive, double wow, double flutter, double hiss, double bandwidth_hz, int generations) {
    path_.set(0.0, bandwidth_hz);
    float x = path_.process(in, drive, hiss);
    if (wow > 0.0 || flutter > 0.0) {
      line_.push(x);
      const double speed = speed_.next(wow, flutter);
      // Speed variation becomes a varying delay around a 5 ms centre.
      x = line_.read(0.005 * rate_ * (1.0 + 20.0 * (speed - 1.0)));
    }
    return generations_.process(x, generations);
  }
 private:
  double rate_ = 48000.0;
  TapePath path_;
  WowFlutter speed_;
  DelayLine line_;
  Generations generations_;
};

// The output limiter: the last thing before the host. Nothing leaves above the ceiling, so a
// strum whose pick transient sits 20 dB over its body can be set as loud as a fuzz drone
// without clipping. The gain drops the instant a sample would cross the ceiling (one detector
// for both channels keeps the stereo image) and recovers over about 120 ms, so it is a peak
// catcher, not a compressor: a chord held under the ceiling passes through untouched. The
// maximum reduction since the last reading is kept for the meter, and the clip light comes on
// once it passes kClipDb.
class Limiter {
 public:
  static constexpr float kCeiling = 0.794f;  // -2 dBFS
  // Reduction beyond which the squash is plainly audible: the clip light's threshold.
  static constexpr double kClipDb = 6.0;
  void prepare(double sample_rate) { release_ = std::exp(-1.0 / (0.120 * sample_rate)); reset(); }
  void reset() { gain_ = 1.0; held_db_ = 0.0; }
  // Returns the gain to apply to both channels for this sample.
  float gain(float l, float r) {
    const double peak = std::max(std::abs(l), std::abs(r));
    const double wanted = peak > kCeiling ? kCeiling / peak : 1.0;
    if (wanted < gain_) gain_ = wanted;
    else gain_ += (wanted - gain_) * (1.0 - release_);
    if (gain_ < 1.0) held_db_ = std::max(held_db_, -20.0 * std::log10(gain_));
    return static_cast<float>(gain_);
  }
  // The deepest reduction in dB since the last reset_meter(); 0 when the ceiling was never hit.
  double held_db() const { return held_db_; }
  void reset_meter() { held_db_ = 0.0; }
 private:
  double release_ = 0.9998, gain_ = 1.0, held_db_ = 0.0;
};

}  // namespace fuzzboi::dsp
