// The three classic Mellotron tape banks, synthesized: there are no sampled Mellotron tapes in
// this project (none that are free to redistribute), so each bank is a small model of what was
// on the tape, played through the tape mechanics in tape.hpp. A user WAV can stand in for any
// bank; the engine handles that case and only calls these generators when no tape is loaded.
//
//   Flute   a near-sine with a breathy noise component and a slow natural vibrato, the
//           "Strawberry Fields" sound
//   Strings a small section: several detuned saws through a soft low-pass, with slow drift
//   Choir   a vowel ("ah" to "oo") formant stack on a pulse source
#pragma once

#include <array>
#include <cmath>

#include "core.hpp"
#include "envelopes.hpp"
#include "filters.hpp"
#include "oscillators.hpp"

namespace fuzzboi::dsp {

enum class TapeBank : int { Flute = 0, Strings = 1, Choir = 2, User = 3 };

class TapeToneGenerator {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    for (size_t i = 0; i < section_.size(); ++i) { section_[i].reset(0.17 * static_cast<double>(i)); drift_[i].prepare(sample_rate); }
    breath_.set(1200.0, sample_rate);
    section_tone_.set(2600.0, sample_rate);
    for (auto& f : formants_) f.prepare(sample_rate);
    vibrato_.prepare(sample_rate);
    choir_.reset();
    flute_.reset();
  }
  void note_on() { breath_env_.prepare(rate_); breath_env_.trigger(90.0); }

  // One sample of the chosen bank at `hz`. Level is roughly -6 dBFS peak for every bank.
  float next(TapeBank bank, double hz) {
    switch (bank) {
      case TapeBank::Flute: {
        const double vib = 1.0 + 0.004 * vibrato_.next(4.6);
        const float tone = flute_.next(Wave::Sine, hz * vib, rate_);
        // A second harmonic gives the woody edge; breath noise is band-limited and louder at
        // the attack ("chiff").
        const float second = static_cast<float>(std::sin(kTau * 2.0 * flute_.phase())) * 0.18f;
        const float breath = breath_.low(rng_.bipolar()) * static_cast<float>(0.05 + 0.25 * breath_env_.next());
        return (tone + second) * 0.45f + breath * 0.5f;
      }
      case TapeBank::Strings: {
        float sum = 0.0f;
        for (size_t i = 0; i < section_.size(); ++i) {
          const double detune = kSectionCents[i] + 3.0 * drift_[i].next(0.11 + 0.03 * static_cast<double>(i), LfoShape::Triangle);
          sum += section_[i].next(Wave::Saw, hz * cents_ratio(detune), rate_);
        }
        return section_tone_.low(sum * 0.11f);
      }
      case TapeBank::Choir: default: {
        // Pulse source into three formants that slide from "ah" toward "oo" with a slow LFO.
        const float source = choir_.next(Wave::Pulse, hz, rate_, 0.3);
        const double morph = 0.5 + 0.5 * vibrato_.next(0.23);
        float out = 0.0f;
        for (size_t i = 0; i < formants_.size(); ++i) {
          const double f = lerp(kAh[i], kOo[i], morph);
          formants_[i].set(f, 8.0);
          out += formants_[i].band(source) * kFormantGain[i];
        }
        return out * 0.5f;
      }
    }
  }

 private:
  static constexpr std::array<double, 5> kSectionCents{-9.0, -4.0, 0.0, 5.0, 11.0};
  static constexpr std::array<double, 3> kAh{700.0, 1200.0, 2600.0}, kOo{350.0, 800.0, 2400.0};
  static constexpr std::array<float, 3> kFormantGain{1.0f, 0.6f, 0.25f};
  double rate_ = 48000.0;
  Oscillator flute_, choir_;
  std::array<Oscillator, 5> section_{};
  std::array<Lfo, 5> drift_{};
  std::array<StateVariable, 3> formants_{};
  OnePole breath_, section_tone_;
  Lfo vibrato_;
  Decay breath_env_;
  Rng rng_{0x7a9e5ee1u};
};

}  // namespace fuzzboi::dsp
