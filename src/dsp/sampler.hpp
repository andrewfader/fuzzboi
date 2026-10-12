// The sample side of the instrument: a sample (mono frames plus root pitch and loop), the
// voice that plays one with varispeed and loops, the collage behaviour that chops it into
// re-ordered, sometimes reversed slices, the Optigan character, and a found-sound generator
// that gives an empty slot something to play.
#pragma once

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core.hpp"
#include "filters.hpp"
#include "tape.hpp"

namespace fuzzboi::dsp {

struct Sample {
  std::vector<float> frames;  // mono
  double rate = 48000.0;
  double root_hz = 261.63;    // the pitch the recording plays at ratio 1
  size_t loop_start = 0, loop_end = 0;  // loop_end 0 means the whole sample
  std::string name, source, license;
  size_t end() const { return loop_end > 0 && loop_end <= frames.size() ? loop_end : frames.size(); }
};

struct SamplerSettings {
  double varispeed_semitones = 0.0;
  bool loop = true;
  double slice_ms = 600.0;     // collage: how long each slice plays before the dice roll
  double slice_chance = 0.0;   // probability that a slice jumps somewhere else
  double reverse_chance = 0.0; // probability that a slice plays backwards
  double scatter = 0.0;        // random start point at note-on, 0..1 of the sample
  double optigan = 0.0;        // 0 clean, 1 full optical-disc grain
};

class SamplerVoice {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    optigan_tone_.set(4500.0, sample_rate);
    optigan_noise_.set(2000.0, sample_rate);
    flutter_.prepare(sample_rate);
  }
  void note_on(std::shared_ptr<const Sample> sample, const SamplerSettings& s, uint32_t seed) {
    sample_ = std::move(sample);
    rng_.seed(seed);
    reverse_ = false;
    done_ = false;
    if (!sample_ || sample_->frames.empty()) { position_ = 0.0; return; }
    const double span = static_cast<double>(sample_->end());
    position_ = s.scatter > 0.0 ? rng_.unipolar() * s.scatter * span : 0.0;
    slice_left_ = s.slice_ms * 0.001 * rate_;
    roll(s);
  }
  bool finished() const { return !sample_ || sample_->frames.empty() || done_; }

  float next(const SamplerSettings& s, double hz, bool collage) {
    if (!sample_ || sample_->frames.empty() || done_) return 0.0f;
    const Sample& smp = *sample_;
    const size_t end = smp.end();
    double ratio = (hz / smp.root_hz) * (smp.rate / rate_) * std::exp2(s.varispeed_semitones / 12.0);
    if (s.optigan > 0.0) ratio *= flutter_.next(0.6 * s.optigan, 0.8 * s.optigan);
    if (collage) {
      slice_left_ -= 1.0;
      if (slice_left_ <= 0.0) { slice_left_ = std::max(1.0, s.slice_ms * 0.001 * rate_); roll(s); }
    }
    // Linear interpolation between the two neighbouring frames.
    const size_t i0 = static_cast<size_t>(std::floor(position_));
    const size_t i1 = std::min(i0 + 1, end - 1);
    const float frac = static_cast<float>(position_ - static_cast<double>(i0));
    float out = smp.frames[std::min(i0, end - 1)] * (1.0f - frac) + smp.frames[i1] * frac;
    position_ += reverse_ ? -ratio : ratio;
    const double loop_start = static_cast<double>(std::min(smp.loop_start, end - 1));
    if (position_ >= static_cast<double>(end)) {
      if (s.loop) position_ = loop_start + (position_ - static_cast<double>(end));
      else done_ = true;
    } else if (position_ < loop_start) {
      if (reverse_ && s.loop) position_ = static_cast<double>(end) - 1.0 - (loop_start - position_);
      else if (reverse_) done_ = true;
      else position_ = loop_start;
    }
    if (s.optigan > 0.0) {
      // The optical pickup: bandwidth limited, grainy, with a little light-bulb hum.
      const float grain = optigan_noise_.low(rng_.bipolar()) * 0.02f;
      const float narrow = optigan_tone_.low(out);
      hum_phase_ += 120.0 / rate_;
      if (hum_phase_ >= 1.0) hum_phase_ -= 1.0;
      const float hum = static_cast<float>(std::sin(kTau * hum_phase_)) * 0.01f;
      out = out * static_cast<float>(1.0 - s.optigan) + (narrow + grain + hum) * static_cast<float>(s.optigan);
    }
    return out;
  }

 private:
  void roll(const SamplerSettings& s) {
    if (!sample_) return;
    const double span = static_cast<double>(sample_->end());
    if (rng_.unipolar() < s.slice_chance) position_ = rng_.unipolar() * span;
    reverse_ = rng_.unipolar() < s.reverse_chance;
    position_ = std::clamp(position_, 0.0, std::max(0.0, span - 2.0));
  }
  double rate_ = 48000.0, position_ = 0.0, slice_left_ = 0.0, hum_phase_ = 0.0;
  bool reverse_ = false, done_ = false;
  std::shared_ptr<const Sample> sample_;
  OnePole optigan_tone_, optigan_noise_;
  WowFlutter flutter_;
  Rng rng_{0xc0a1a9e5u};
};

// A few seconds of synthetic "found sound" for the collage sampler to chop when no sample is
// loaded: shortwave radio sweeps, record crackle, a muffled voice-like murmur, a tape stop, a
// clock tick. Deterministic for a given seed, so a preset always sounds the same.
inline std::shared_ptr<Sample> generate_found_sound(uint32_t seed, double rate, double seconds = 4.0) {
  auto sample = std::make_shared<Sample>();
  sample->rate = rate;
  sample->root_hz = 261.63;
  sample->name = "Found sound (built in)";
  sample->source = "synthesized";
  sample->license = "none needed";
  const size_t n = static_cast<size_t>(rate * seconds);
  sample->frames.assign(n, 0.0f);
  Rng rng(seed);
  StateVariable formant; formant.prepare(rate);
  OnePole crackle_tone; crackle_tone.set(5000.0, rate);
  double sweep_phase = 0.0, voice_phase = 0.0, tick = 0.0;
  const double sweep_from = 300.0 + 1500.0 * rng.unipolar(), sweep_to = 2000.0 + 4000.0 * rng.unipolar();
  for (size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / rate;
    float x = 0.0f;
    // Shortwave: a slow whistle sweeping up with a wobble, fading in and out.
    const double sweep_hz = lerp(sweep_from, sweep_to, 0.5 + 0.5 * std::sin(t * 0.7)) * (1.0 + 0.02 * std::sin(t * 31.0));
    sweep_phase += sweep_hz / rate;
    x += static_cast<float>(std::sin(kTau * sweep_phase) * 0.18 * (0.5 + 0.5 * std::sin(t * 1.3 + 1.0)));
    // Crackle: sparse impulses through a bright one-pole.
    const float pop = rng.unipolar() < 0.0015f ? rng.bipolar() * 0.8f : 0.0f;
    x += crackle_tone.low(pop) * 0.9f;
    // Murmur: a pulse at a speaking pitch through a wandering formant, gated into syllables.
    const double syllable = std::fmod(t * 3.1, 1.0) < 0.45 ? 1.0 : 0.0;
    voice_phase += (110.0 + 20.0 * std::sin(t * 2.0)) / rate;
    if (voice_phase >= 1.0) voice_phase -= 1.0;
    const float buzz = voice_phase < 0.15 ? 1.0f : -0.18f;
    formant.set(400.0 + 500.0 * (0.5 + 0.5 * std::sin(t * 5.3)), 6.0);
    x += formant.band(buzz) * static_cast<float>(0.22 * syllable);
    // A clock tick every second, and a tape stop in the last half second.
    tick += 1.0 / rate;
    if (tick >= 1.0) tick -= 1.0;
    if (tick < 0.004) x += rng.bipolar() * 0.5f;
    const double stop = std::max(0.0, (t - (seconds - 0.5)) / 0.5);
    // Scaled to sit where the harvested bank sits: its WAVs peak at -3 dBFS but hold around
    // -23 dBFS RMS, and the audit phrase through the collage presets lands at the same loudness
    // with either (tests/engine_tests.cpp every_preset_meets_the_quality_bar).
    sample->frames[i] = static_cast<float>(x * (1.0 - stop) * 0.75);
  }
  return sample;
}

}  // namespace fuzzboi::dsp
