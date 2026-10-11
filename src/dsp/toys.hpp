// The toy cupboard: a toy piano and a music box (struck and plucked metal tines), the
// 13th Floor Elevators' electric jug, a theremin, a singing saw, a plastic piccolo and a
// Stylophone. Each is a small physically-motivated model rather than a sample.
#pragma once

#include <array>
#include <cmath>

#include "core.hpp"
#include "envelopes.hpp"
#include "filters.hpp"
#include "oscillators.hpp"

namespace fuzzboi::dsp {

// --- struck and plucked tines --------------------------------------------------------------------
//
// A toy piano's note comes from a steel rod clamped at one end; the heard pitch is not the
// rod's fundamental (which is subsonic for the rods in question) but its second mode. Relative
// to that heard pitch the next modes of a clamped-free bar fall at about 2.80x, 5.49x and 9.07x
// (from the bar mode ratios 1 : 6.267 : 17.55 : 34.39 : 56.84). Those inharmonic partials are
// why a toy piano sounds nothing like a piano. A music box comb tooth is the same physics with
// a brighter pluck and a shorter decay.
struct ModalSettings {
  double hammer = 0.5;       // 0 soft felt, 1 hard: more energy in the upper modes and the clank
  double damping = 0.3;      // 0 long ring, 1 short
  double inharmonic = 0.5;   // 0 pulls the modes toward harmonic ratios, 1 the bar ratios
  bool music_box = false;    // brighter pluck, faster decay, higher body
};

class ModalVoice {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    for (auto& m : modes_) m.prepare(sample_rate);
    body_.prepare(sample_rate);
    clank_.prepare(sample_rate);
    clank_tone_.set(4000.0, sample_rate);
    body_.set(900.0, 2.0);
  }
  void note_on(const ModalSettings& s, double hz, double velocity) {
    hz_ = hz;
    const double strike = 0.6 + 0.4 * velocity;
    const double partial_tilt = s.music_box ? 0.9 : 0.4 + 0.5 * s.hammer;
    for (size_t i = 0; i < modes_.size(); ++i) {
      const double ratio = lerp(kHarmonic[i], kBar[i], s.inharmonic);
      const double f = hz * ratio;
      // Higher modes decay faster; damping shortens everything. Q is "cycles to ring".
      const double seconds = (s.music_box ? 1.2 : 2.5) * std::pow(0.5, static_cast<double>(i)) * (1.0 - 0.97 * s.damping);
      const double q = std::clamp(kPi * f * seconds, 2.0, 4000.0);
      modes_[i].set(std::min(f, rate_ * 0.45), q);
      // A unit impulse rings a mode at about 2g; scale so the heard partial lands near 0.5.
      const double ring_gain = 2.0 * modes_[i].g();
      excite_[i] = static_cast<float>(strike * 0.5 / ring_gain * std::pow(partial_tilt, static_cast<double>(i)) * (f < rate_ * 0.45 ? 1.0 : 0.0));
    }
    kick_ = 1;
    clank_.trigger(s.music_box ? 4.0 : 9.0);
  }
  float next(const ModalSettings& s) {
    // The strike is one impulse into each resonator plus a short noisy "clank" of the hammer
    // meeting the rod (or the pin releasing the tooth).
    const float impulse = kick_ ? 1.0f : 0.0f;
    kick_ = 0;
    const float clank = clank_tone_.low(rng_.bipolar()) * static_cast<float>(clank_.next() * (0.15 + 0.35 * s.hammer));
    float out = 0.0f;
    for (size_t i = 0; i < modes_.size(); ++i) out += modes_[i].ring(impulse * excite_[i] + clank * (i == 0 ? 0.02f : 0.004f));
    // The plastic/wood body colours everything slightly.
    out += body_.band(out) * 0.3f;
    return out * 0.8f;
  }
  bool quiet() const {
    // Not tracked exactly; the amplitude envelope in the engine ends the voice.
    return false;
  }

 private:
  static constexpr std::array<double, 4> kBar{1.0, 17.55 / 6.267, 34.39 / 6.267, 56.84 / 6.267};
  static constexpr std::array<double, 4> kHarmonic{1.0, 3.0, 5.0, 9.0};
  double rate_ = 48000.0, hz_ = 440.0;
  std::array<StateVariable, 4> modes_{};
  std::array<float, 4> excite_{};
  StateVariable body_;
  Decay clank_;
  OnePole clank_tone_;
  Rng rng_{0x70797069u};
  int kick_ = 0;
};

// --- the electric jug -------------------------------------------------------------------------------
//
// Tommy Hall sang rapid runs into the mouth of a jug with a microphone pressed to the opening.
// The jug is a Helmholtz resonator: a strong resonance around 100-300 Hz plus a weaker body
// mode, and it colours a voice into the burbling pulse on "You're Gonna Miss Me". Model: a
// glottal-ish pulse at the sung pitch, amplitude-fluttered at the rate of the runs, through the
// resonator and a slightly overdriven microphone.
struct JugSettings {
  double resonance_hz = 160.0;
  double flutter_hz = 9.0;   // rate of the tongued pulses
  double growl = 0.4;        // microphone overdrive and extra sub-harmonic buzz
};

class JugVoice {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    helmholtz_.prepare(sample_rate);
    body_.prepare(sample_rate);
    mouth_.set(2500.0, sample_rate);
    flutter_.prepare(sample_rate);
    flutter_.reset(0.25);
    source_.reset();
  }
  void note_on() { flutter_.reset(0.25); }
  float next(const JugSettings& s, double hz) {
    // Voice-like source: a narrow pulse (glottal) plus its octave-down "growl".
    const float glottal = source_.next(Wave::Pulse, hz, rate_, 0.12);
    const double sub_phase = source_.phase() * 0.5;
    const float growl = static_cast<float>(std::sin(kTau * sub_phase) * s.growl * 0.4);
    // The tongued flutter: a raised sine gate, never fully closed.
    const double gate = 0.35 + 0.65 * (0.5 + 0.5 * flutter_.next(s.flutter_hz));
    const float voice = (glottal * 0.5f + growl) * static_cast<float>(gate);
    helmholtz_.set(s.resonance_hz, 6.0);
    body_.set(s.resonance_hz * 2.7, 3.0);
    const float resonated = helmholtz_.band(voice) * 1.6f + body_.band(voice) * 0.5f + mouth_.low(voice) * 0.25f;
    // The microphone, overdriven in proportion to the growl.
    return warm_clip(resonated * static_cast<float>(1.0 + 3.0 * s.growl)) / static_cast<float>(1.0 + 1.2 * s.growl);
  }
 private:
  double rate_ = 48000.0;
  Oscillator source_;
  StateVariable helmholtz_, body_;
  OnePole mouth_;
  Lfo flutter_;
};

// --- theremin ------------------------------------------------------------------------------------------
//
// A heterodyne oscillator: nearly a sine, with the gentle asymmetry of the RCA/Etherwave
// output stage, and a pitch that glides between notes because a hand never jumps. Unison
// stacks several detuned theremins for the "ton of theremins" on Black Foliage.
struct ThereminSettings {
  double glide_ms = 250.0;
  double shape = 0.3;   // 0 pure sine, 1 noticeably asymmetric
  int unison = 1;       // 1..8
};

class ThereminVoice {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    glide_.set_time(250.0, sample_rate);
    for (size_t i = 0; i < oscillators_.size(); ++i) { oscillators_[i].reset(0.11 * static_cast<double>(i)); drift_[i].prepare(sample_rate); }
    vibrato_.prepare(sample_rate);
  }
  void note_on(double hz, bool legato) {
    if (!legato) glide_.snap(hz);
    glide_.target(hz);
  }
  float next(const ThereminSettings& s, double target_hz) {
    glide_.set_time(std::max(1.0, s.glide_ms) * 0.5, rate_);
    glide_.target(target_hz);
    const double hz = glide_.next() * (1.0 + 0.003 * vibrato_.next(5.5));
    const int n = std::clamp(s.unison, 1, 8);
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
      const double detune = n > 1 ? (static_cast<double>(i) / (n - 1) - 0.5) * 24.0 : 0.0;
      const double wobble = 4.0 * drift_[static_cast<size_t>(i)].next(0.2 + 0.07 * i, LfoShape::Triangle);
      const float sine = oscillators_[static_cast<size_t>(i)].next(Wave::Sine, hz * cents_ratio(detune + wobble), rate_);
      // Asymmetry: a touch of second harmonic from a squared term.
      sum += sine + static_cast<float>(s.shape) * 0.35f * (sine * sine - 0.5f);
    }
    return sum * 0.7f / std::sqrt(static_cast<float>(n));
  }
 private:
  double rate_ = 48000.0;
  Smoother glide_;
  std::array<Oscillator, 8> oscillators_{};
  std::array<Lfo, 8> drift_{};
  Lfo vibrato_;
};

// --- singing saw --------------------------------------------------------------------------------------
//
// A bowed, bent hand saw: a pure sine with a very slow onset as the bow bites, a wide slow
// vibrato from the player's hand, and bow noise that fades as the note settles.
struct SawSettings { double bow = 0.4; };

class SingingSawVoice {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    bow_noise_.set(1800.0, sample_rate);
    bow_env_.prepare(sample_rate);
    vibrato_.prepare(sample_rate);
    glide_.set_time(60.0, sample_rate);
    osc_.reset();
  }
  void note_on(double hz, bool legato) { if (!legato) glide_.snap(hz); glide_.target(hz); bow_env_.trigger(400.0); }
  float next(const SawSettings& s, double target_hz) {
    glide_.target(target_hz);
    const double hz = glide_.next() * (1.0 + 0.012 * vibrato_.next(4.2));
    const float tone = osc_.next(Wave::Sine, hz, rate_);
    const float scrape = bow_noise_.low(rng_.bipolar()) * static_cast<float>(s.bow * (0.03 + 0.25 * bow_env_.next()));
    return tone * 0.6f + scrape;
  }
 private:
  double rate_ = 48000.0;
  Oscillator osc_;
  OnePole bow_noise_;
  Decay bow_env_;
  Lfo vibrato_;
  Smoother glide_;
  Rng rng_{0x5a775a77u};
};

// --- plastic piccolo ----------------------------------------------------------------------------------
//
// A toy recorder: odd-harmonic-heavy tone, plenty of breath noise, overblown chiff at the
// attack, and tuning that wanders because the thing is plastic and the player is a child.
struct PiccoloSettings { double breath = 0.4, chaos = 0.3; };

class PiccoloVoice {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    breath_.set(3500.0, sample_rate);
    chiff_.prepare(sample_rate);
    wander_.prepare(sample_rate);
    osc_.reset();
  }
  void note_on(const PiccoloSettings& s) {
    chiff_.trigger(70.0);
    // Each note lands a little off, by up to a quarter tone at full chaos.
    offset_cents_ = 50.0 * s.chaos * rng_.bipolar();
  }
  float next(const PiccoloSettings& s, double hz) {
    const double wander = 12.0 * s.chaos * wander_.next(1.7, LfoShape::Triangle);
    const double f = hz * cents_ratio(offset_cents_ + wander);
    const float tone = osc_.next(Wave::Sine, f, rate_);
    const double phase = osc_.phase();
    const float third = static_cast<float>(std::sin(kTau * 3.0 * phase)) * 0.25f;
    const float fifth = static_cast<float>(std::sin(kTau * 5.0 * phase)) * 0.08f;
    const double chiff = chiff_.next();
    const float air = breath_.low(rng_.bipolar()) * static_cast<float>(s.breath * (0.08 + 0.5 * chiff));
    return (tone + third + fifth) * 0.5f * static_cast<float>(1.0 + 0.6 * chiff) + air;
  }
 private:
  double rate_ = 48000.0, offset_cents_ = 0.0;
  Oscillator osc_;
  OnePole breath_;
  Decay chiff_;
  Lfo wander_;
  Rng rng_{0x9100010u};
};

// --- Stylophone ---------------------------------------------------------------------------------------
//
// The 1968 pocket organ: a single transistor relaxation oscillator (a raw, buzzy, slightly
// rounded pulse) switched by a stylus, with the famous on/off vibrato.
class StylophoneVoice {
 public:
  void prepare(double sample_rate) { rate_ = sample_rate; rounding_.set(9000.0, sample_rate); vibrato_.prepare(sample_rate); osc_.reset(); }
  float next(double hz, double vibrato_depth_cents) {
    const double vib = cents_ratio(vibrato_depth_cents * vibrato_.next(6.0));
    const float raw = osc_.next(Wave::Pulse, hz * vib, rate_, 0.35);
    return rounding_.low(raw) * 0.55f;
  }
 private:
  double rate_ = 48000.0;
  Oscillator osc_;
  OnePole rounding_;
  Lfo vibrato_;
};

}  // namespace fuzzboi::dsp
