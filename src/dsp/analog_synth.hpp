// A two-oscillator subtractive synth voice in the Minimoog / Polymoog / Prophet line: two
// band-limited oscillators with interval and detune, a sub-octave square, white noise, a ladder
// filter with its own ADSR, key tracking and drive.
#pragma once

#include <cmath>

#include "core.hpp"
#include "envelopes.hpp"
#include "filters.hpp"
#include "oscillators.hpp"

namespace fuzzboi::dsp {

struct SynthSettings {
  Wave osc1 = Wave::Saw, osc2 = Wave::Pulse;
  int osc2_semitones = 0;
  double osc2_cents = 7.0;
  double mix = 0.5;             // 0 all osc 1, 1 all osc 2
  double pulse_width = 0.5;
  double pwm_depth = 0.0;       // LFO modulation of the pulse width, 0..1
  double sub = 0.0, noise = 0.0;
  double cutoff_hz = 2500.0, resonance = 0.2;
  double filter_env = 0.4;      // -1..1, octaves of cutoff sweep at full envelope (times 5)
  double filter_attack_ms = 5, filter_decay_ms = 300, filter_sustain = 0.3, filter_release_ms = 200;
  double key_track = 0.5;       // 0 none, 1 cutoff follows the note fully
  double drive = 0.2;
};

class SynthVoice {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    filter_.prepare(sample_rate);
    filter_env_.prepare(sample_rate);
    pwm_.prepare(sample_rate);
    osc1_.reset(); osc2_.reset(0.37); sub_.reset();
  }
  void note_on(double velocity) {
    velocity_ = velocity;
    filter_env_.note_on();
  }
  void note_off() { filter_env_.note_off(); }
  bool filter_envelope_active() const { return filter_env_.active(); }

  float next(const SynthSettings& s, double hz, double note) {
    filter_env_.set(s.filter_attack_ms, s.filter_decay_ms, s.filter_sustain, s.filter_release_ms);
    const double env = filter_env_.next();
    const double hz2 = hz * std::exp2(s.osc2_semitones / 12.0) * cents_ratio(s.osc2_cents);
    const double width = std::clamp(s.pulse_width + 0.45 * s.pwm_depth * pwm_.next(0.6), 0.05, 0.95);
    const float a = osc1_.next(s.osc1, hz, rate_, width);
    const float b = osc2_.next(s.osc2, hz2, rate_, width);
    const float sub = sub_.next(Wave::Pulse, hz * 0.5, rate_, 0.5);
    float mixed = a * static_cast<float>(1.0 - s.mix) + b * static_cast<float>(s.mix) +
                  sub * static_cast<float>(s.sub * 0.8) + rng_.bipolar() * static_cast<float>(s.noise * 0.5);
    // Cutoff: base, plus key tracking (relative to middle C), plus the envelope in octaves.
    const double track = (note - 60.0) / 12.0 * s.key_track;
    const double sweep = s.filter_env * env * 5.0 * (0.5 + 0.5 * velocity_);
    const double cutoff = s.cutoff_hz * std::exp2(track + sweep);
    filter_.set(std::clamp(cutoff, 20.0, rate_ * 0.45), s.resonance, 1.0 + 3.0 * s.drive);
    return filter_.process(mixed * 0.5f) * 1.2f;
  }

 private:
  double rate_ = 48000.0, velocity_ = 1.0;
  Oscillator osc1_, osc2_, sub_;
  LadderFilter filter_;
  Adsr filter_env_;
  Lfo pwm_;
  Rng rng_{0x5eed1234u};
};

}  // namespace fuzzboi::dsp
