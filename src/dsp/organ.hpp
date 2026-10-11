// A transistor combo organ in the Vox Continental / Farfisa family, with a Lowrey-style
// "marimba repeat" borrowed from Who's Next.
//
// Those organs derive every footage from divide-down square-ish tone generators and shape them
// with "flute" (filtered toward a sine) and "reed" (buzzier, saw-like) tabs; drawbars or tabs set
// the footage mix. The model: one additive bank per note at the six classic footages, each
// footage a blend of a sine and a band-limited pulse ("reed"), a key click burst on note-on, an
// optional percussion decay on the 4' footage, and a shared vibrato. The repeat gates the whole
// voice on and off at a set rate, the way Lowrey's repeat percussion did.
#pragma once

#include <array>
#include <cmath>

#include "core.hpp"
#include "envelopes.hpp"
#include "oscillators.hpp"

namespace fuzzboi::dsp {

struct OrganSettings {
  // 16', 8', 4', 2 2/3', 2', 1' levels 0..1
  std::array<double, 6> drawbars{0.6, 1.0, 0.8, 0.3, 0.5, 0.2};
  double reed = 0.5;          // 0 flute (sine), 1 reed (pulse)
  double click = 0.4;         // key click level
  double percussion = 0.0;    // level of the decaying 4' percussion
  double percussion_ms = 300; // its decay
  double vibrato = 0.3;       // 0..1 depth of the ~6 Hz vibrato
  bool repeat = false;        // marimba repeat
  double repeat_hz = 8.0;
};

// Footage ratios relative to the played note: 16' is an octave down, 8' unison, 4' up an
// octave, 2 2/3' a twelfth, 2' two octaves, 1' three octaves.
inline constexpr std::array<double, 6> kFootageRatios{0.5, 1.0, 2.0, 3.0, 4.0, 8.0};

class OrganVoice {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    click_.prepare(sample_rate);
    percussion_.prepare(sample_rate);
    for (auto& osc : oscillators_) osc.reset();
    vibrato_.prepare(sample_rate);
    repeat_phase_ = 0.0;
  }
  void note_on(const OrganSettings& s) {
    click_.trigger(12.0);
    percussion_.trigger(std::max(20.0, s.percussion_ms));
    repeat_phase_ = 0.0;
    // Divide-down organs do not reset generator phase per key; starting each footage at a
    // different fixed phase keeps the sum from stacking into one spike.
    for (size_t i = 0; i < oscillators_.size(); ++i) oscillators_[i].set_phase(0.13 * static_cast<double>(i));
  }
  float next(const OrganSettings& s, double hz) {
    const double vib = 1.0 + 0.006 * s.vibrato * vibrato_.next(6.2);
    float sum = 0.0f;
    const double perc = percussion_.next();
    for (size_t i = 0; i < oscillators_.size(); ++i) {
      double level = s.drawbars[i];
      if (i == 2) level += s.percussion * perc;  // percussion rides the 4' footage
      if (level <= 1e-4) { oscillators_[i].next(Wave::Sine, hz * kFootageRatios[i] * vib, rate_); continue; }
      const double f = hz * kFootageRatios[i] * vib;
      // Each footage: a sine for the flute tab, a 50% pulse for the reed tab. Running the
      // oscillator once and deriving both from the phase keeps them locked.
      const double phase = oscillators_[i].phase();
      const float sine = static_cast<float>(std::sin(kTau * phase));
      const float reed = pulse(phase, f / rate_, 0.5);
      oscillators_[i].next(Wave::Sine, f, rate_);
      sum += static_cast<float>(level) * (sine * static_cast<float>(1.0 - s.reed) + reed * static_cast<float>(s.reed * 0.7));
    }
    // Key click: a short broadband burst, as the key contacts close.
    const float click = static_cast<float>(s.click * click_.next()) * rng_.bipolar() * 0.6f;
    float gate = 1.0f;
    if (s.repeat) {
      // Repeat: a soft-edged gate; the Lowrey's percussion repeat re-struck the note.
      repeat_phase_ += s.repeat_hz / rate_;
      if (repeat_phase_ >= 1.0) repeat_phase_ -= 1.0;
      gate = repeat_phase_ < 0.55 ? 1.0f : 0.0f;
      const double edge = std::min(repeat_phase_, std::abs(repeat_phase_ - 0.55)) * s.repeat_hz / rate_ * 400.0;
      gate *= static_cast<float>(std::clamp(edge * 50.0, 0.0, 1.0));
    }
    return (sum * 0.28f + click) * gate;
  }

 private:
  double rate_ = 48000.0;
  std::array<Oscillator, 6> oscillators_{};
  Decay click_, percussion_;
  Lfo vibrato_;
  Rng rng_{0x0c0ffee1u};
  double repeat_phase_ = 0.0;
};

}  // namespace fuzzboi::dsp
