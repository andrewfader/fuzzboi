// The pedalboard: fuzz, speaker cabinet, phaser, chorus, rotary speaker and tremolo. Each is a
// small model of the circuit's behaviour rather than a schematic simulation, tuned to the
// records the genre grew from.
#pragma once

#include <array>
#include <cmath>

#include "../core.hpp"
#include "../envelopes.hpp"
#include "../filters.hpp"

namespace fuzzboi::dsp {

// --- fuzz -----------------------------------------------------------------------------------------
//
// Five flavours:
//   Muff   the Big Muff Pi: an input gain stage, two identical diode-clipping stages with a
//          high-pass before each (so the clipping is frequency dependent), and the passive
//          tone stack that crossfades a low-pass against a high-pass with its mid scoop
//   Face   the Fuzz Face: one asymmetric soft stage with the bass thinned ahead of it, which
//          cleans up as the signal falls
//   Bender the Tone Bender MkII: harder, gated, with a treble lift
//   Plexi  not a pedal but a cranked, variac'd Marshall Plexi head (the "brown sound"): three
//          biased triode stages, each a soft asymmetric clip with a coupling high-pass, a
//          mid push ahead of the last, so it adds even harmonics and stays more touch-sensitive
//   Octave the Octavia: a full-wave rectifier after the gain, which folds the negative half up
//          and puts the octave above the note on top of the fuzz (ring-mod sparkle on chords)
enum class FuzzMode : int { Muff = 0, Face = 1, Bender = 2, Plexi = 3, Octave = 4 };

class Fuzz {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    input_hp_.set(70.0, sample_rate);
    for (auto& hp : stage_hp_) hp.set(120.0, sample_rate);
    for (auto& lp : stage_lp_) lp.set(7000.0, sample_rate);
    tone_lp_.set(500.0, sample_rate);
    tone_hp_.set(1200.0, sample_rate);
    for (auto& hp : triode_hp_) hp.set(160.0, sample_rate);
    octave_dc_.set(20.0, sample_rate);
    mid_push_.set(Biquad::Type::Peak, 800.0, 0.8, 6.0, sample_rate);
    mid_push_.reset();
    dc_.set(10.0, sample_rate);
  }
  // amount 0..1 (0 is bypass), tone 0 dark .. 1 bright, level in dB, mode.
  float process(float in, double amount, double tone, double level_db, FuzzMode mode) {
    if (amount <= 0.0) return in;
    const float gain = static_cast<float>(1.0 + 60.0 * amount * amount);
    float x = input_hp_.high(in) * gain;
    switch (mode) {
      case FuzzMode::Muff:
        for (int stage = 0; stage < 2; ++stage) {
          x = stage_hp_[static_cast<size_t>(stage)].high(x);
          x = diode_pair(x);
          x = stage_lp_[static_cast<size_t>(stage)].low(x) * 3.0f;
        }
        x = diode_pair(x * 0.5f);
        break;
      case FuzzMode::Face:
        x = asymmetric(x * 0.6f);
        break;
      case FuzzMode::Plexi:
        // Less raw gain than a pedal: the stages share it, and the amp keeps some dynamics.
        x *= 0.12f;
        for (size_t stage = 0; stage < triode_hp_.size(); ++stage) {
          x = triode_hp_[stage].high(x);
          if (stage == 2) x = mid_push_.process(x);
          x = triode(x * 1.6f);
        }
        break;
      case FuzzMode::Octave: {
        // Rectify before the clipping, as the Octavia's transformer and diodes do: |sin| has no
        // fundamental, so the octave dominates. Clipping a square first would rectify to DC.
        const float clean = x / gain;
        const float folded = octave_dc_.process(std::abs(clean));
        x = diode_pair(folded * gain * 1.6f) + 0.15f * std::tanh(clean * gain * 0.3f);
        break;
      }
      default:
        x = static_cast<float>(std::clamp(static_cast<double>(x) * 1.5, -1.0, 1.0));
        x = asymmetric(x * 2.0f);
        // Gate: the Bender chokes small signals.
        x = std::abs(x) < 0.02f ? x * 0.2f : x;
        break;
    }
    // The Big Muff tone stack: a low-pass and a high-pass summed, with a scooped middle.
    const float dark = tone_lp_.low(x), bright = tone_hp_.high(x);
    const float t = static_cast<float>(std::clamp(tone, 0.0, 1.0));
    float out = dark * (1.0f - t) + bright * t;
    if (mode == FuzzMode::Bender) out = out * 0.7f + x * 0.3f;
    out = dc_.process(out) * static_cast<float>(db_to_gain(level_db)) * 0.9f;
    return in * static_cast<float>(1.0 - amount) + out * static_cast<float>(amount);
  }
 private:
  static float diode_pair(float x) {
    // Silicon diode clamp: linear until ~0.6, then a knee that limits at about 0.9.
    const float a = std::abs(x);
    const float clipped = a < 0.55f ? a : 0.55f + 0.35f * std::tanh((a - 0.55f) * 3.0f);
    return std::copysign(clipped, x);
  }
  static float triode(float x) {
    // A biased triode: grid conduction squares off the top early, cut-off rounds the bottom
    // late, so each stage adds even harmonics. The offset is removed again so it cannot pile up.
    constexpr float kBias = 0.25f;
    return std::tanh(x + kBias) - std::tanh(kBias);
  }
  static float asymmetric(float x) {
    // Germanium transistor: the positive half clips sooner than the negative.
    return x > 0.0f ? 0.8f * std::tanh(x * 1.6f) : std::tanh(x * 0.9f);
  }
  double rate_ = 48000.0;
  OnePole input_hp_, tone_lp_, tone_hp_;
  std::array<OnePole, 2> stage_hp_{}, stage_lp_{};
  std::array<OnePole, 3> triode_hp_{};
  Biquad mid_push_;
  DcBlocker dc_, octave_dc_;
};

// --- speaker cabinet --------------------------------------------------------------------------------
//
// What a guitar speaker in a box does to anything: a bump at the cabinet's low resonance, a
// mid shape, a presence peak, and a top end that stops a few kilohertz up because a paper cone
// cannot follow further. Three voicings: a tweed combo (warm, early roll-off), a British 4x12
// (scooped mids, mid-bass thump, presence), and a top-boost combo (the brightest, the chime).
// Values are chosen by ear against the published descriptions, not measured cabinets.
enum class CabType : int { Tweed = 0, British = 1, TopBoost = 2 };

class Cabinet {
 public:
  void prepare(double sample_rate) { rate_ = sample_rate; type_ = -1; set(CabType::British); hp_.set(70.0, sample_rate); }
  float process(float in, double mix, CabType type) {
    if (mix <= 0.0) return in;
    if (static_cast<int>(type) != type_) set(type);
    float x = hp_.high(in);
    x = presence_.process(mid_.process(low_.process(x)));
    x = lp2_.process(lp1_.process(x));
    return in * static_cast<float>(1.0 - mix) + x * static_cast<float>(mix);
  }
 private:
  void set(CabType type) {
    type_ = static_cast<int>(type);
    switch (type) {
      case CabType::Tweed:
        low_.set(Biquad::Type::Peak, 95.0, 1.2, 4.0, rate_); mid_.set(Biquad::Type::Peak, 500.0, 1.0, -2.0, rate_);
        presence_.set(Biquad::Type::Peak, 2200.0, 1.5, 3.0, rate_); lp1_.set(Biquad::Type::LowPass, 4200.0, 0.8, 0.0, rate_); lp2_.set(Biquad::Type::LowPass, 5200.0, 0.6, 0.0, rate_);
        break;
      case CabType::British:
        low_.set(Biquad::Type::Peak, 110.0, 1.5, 5.0, rate_); mid_.set(Biquad::Type::Peak, 400.0, 1.2, -4.0, rate_);
        presence_.set(Biquad::Type::Peak, 3000.0, 2.0, 4.0, rate_); lp1_.set(Biquad::Type::LowPass, 5000.0, 0.8, 0.0, rate_); lp2_.set(Biquad::Type::LowPass, 6000.0, 0.6, 0.0, rate_);
        break;
      default:
        low_.set(Biquad::Type::Peak, 120.0, 1.2, 2.0, rate_); mid_.set(Biquad::Type::Peak, 800.0, 1.0, 1.0, rate_);
        presence_.set(Biquad::Type::Peak, 3200.0, 1.6, 6.0, rate_); lp1_.set(Biquad::Type::LowPass, 6500.0, 0.8, 0.0, rate_); lp2_.set(Biquad::Type::LowPass, 7500.0, 0.6, 0.0, rate_);
        break;
    }
    low_.reset(); mid_.reset(); presence_.reset(); lp1_.reset(); lp2_.reset();
  }
  double rate_ = 48000.0;
  int type_ = -1;
  OnePole hp_;
  Biquad low_, mid_, presence_, lp1_, lp2_;
};

// --- phaser ----------------------------------------------------------------------------------------
//
// A chain of 2 to 8 first-order allpasses swept together by a slow LFO, with feedback from the
// chain's output into its input (the Small Stone's "color" switch), mixed with the dry
// signal for the notches.
class Phaser {
 public:
  void prepare(double sample_rate) { rate_ = sample_rate; lfo_.prepare(sample_rate); for (auto& a : stages_) a.reset(); feedback_ = 0.0f; }
  // stages 0..3 selects 2, 4, 6 or 8 stages. depth 0..1, feedback 0..0.9.
  float process(float in, double mix, double rate_hz, double depth, double feedback, int stages) {
    if (mix <= 0.0) { lfo_.next(rate_hz, LfoShape::Triangle); return in; }
    const int count = std::clamp(stages, 0, 3) * 2 + 2;
    // Sweep from 200 Hz to a few kHz on a log scale.
    const double sweep = 0.5 + 0.5 * lfo_.next(rate_hz, LfoShape::Triangle);
    const double corner = 200.0 * std::pow(20.0, sweep * depth);
    float x = in + feedback_ * static_cast<float>(feedback);
    for (int i = 0; i < count; ++i) {
      stages_[static_cast<size_t>(i)].set(corner * (1.0 + 0.1 * i), rate_);
      x = stages_[static_cast<size_t>(i)].process(x);
    }
    feedback_ = x;
    const float wet = (in + x) * 0.5f;
    return in * static_cast<float>(1.0 - mix) + wet * static_cast<float>(mix);
  }
 private:
  double rate_ = 48000.0;
  Lfo lfo_;
  std::array<Allpass1, 8> stages_{};
  float feedback_ = 0.0f;
};

// --- chorus ----------------------------------------------------------------------------------------
//
// The Juno-60's bucket-brigade chorus, the sound under every Tame Impala pad: one short BBD
// delay per side, swept by a triangle LFO, the two sides swept in opposite directions so the
// image widens, the wet signal band-limited the way a bucket brigade is (its clock filter
// rolls off above a few kHz) and summed with the dry at equal level. Juno "I" is slow and
// shallow, "II" faster and deeper, "I+II" both LFOs at once (the famous wobble), and
// "Ensemble" is the three-phase string-machine chorus (Solina, Dimension D) with three taps
// 120 degrees apart plus a faster shallow sweep. "Flanger" is the same bucket brigade run
// much shorter (0.3 to 5 ms) with its output fed back and summed with the dry at equal level,
// the MXR / Electric Mistress jet: a comb whose notches sweep the whole spectrum.
enum class ChorusMode : int { JunoI = 0, JunoII = 1, JunoBoth = 2, Ensemble = 3, Flanger = 4 };

class Chorus {
 public:
  struct Stereo { float left, right; };
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    for (auto& l : line_) l.prepare(static_cast<size_t>(sample_rate * 0.03) + 8);
    for (size_t i = 0; i < lfo_.size(); ++i) { lfo_[i].prepare(sample_rate); lfo_[i].reset(static_cast<double>(i) / 3.0); }
    fast_.prepare(sample_rate);
    for (auto& b : bbd_) b.set(7500.0, sample_rate);
    for (auto& b : bbd2_) b.set(9000.0, sample_rate);
    for (auto& h : hp_) h.set(60.0, sample_rate);
  }
  // mix 0..1 (0 is bypass), rate_hz is the base LFO speed, depth 0..1 (sweep width).
  Stereo process(float in_l, float in_r, double mix, double rate_hz, double depth, ChorusMode mode) {
    // LFOs run whether or not the chorus is heard, so turning it up never jumps.
    double slow = lfo_[0].next(rate_hz, LfoShape::Triangle);
    const double faster = lfo_[1].next(rate_hz * 1.7, LfoShape::Triangle);
    const double third = lfo_[2].next(rate_hz, LfoShape::Triangle);
    const double shimmer = fast_.next(6.0, LfoShape::Sine);
    if (mix <= 0.0) { line_[0].push(in_l); line_[1].push(in_r); flange_ = {0.0f, 0.0f}; return {in_l, in_r}; }
    if (mode == ChorusMode::Flanger) {
      line_[0].push(in_l + 0.5f * std::tanh(flange_[0]));
      line_[1].push(in_r + 0.5f * std::tanh(flange_[1]));
    } else {
      line_[0].push(hp_[0].high(in_l));
      line_[1].push(hp_[1].high(in_r));
    }
    const double centre = 0.0055 * rate_, swing = 0.0028 * rate_ * std::clamp(depth, 0.0, 1.0);
    float wet_l = 0.0f, wet_r = 0.0f;
    switch (mode) {
      case ChorusMode::JunoI:
        wet_l = line_[0].read(centre + swing * slow);
        wet_r = line_[1].read(centre - swing * slow);
        break;
      case ChorusMode::JunoII:
        wet_l = line_[0].read(centre + swing * 1.4 * faster);
        wet_r = line_[1].read(centre - swing * 1.4 * faster);
        break;
      case ChorusMode::JunoBoth: {
        const double both = 0.5 * (slow + 1.4 * faster);
        wet_l = line_[0].read(centre + swing * 1.3 * both);
        wet_r = line_[1].read(centre - swing * 1.3 * both);
        break;
      }
      case ChorusMode::Flanger: {
        // The sweep is exponential in delay, so the notches move evenly in pitch. The right side
        // runs a third of a cycle behind for width. The regeneration (feedback) sharpens the
        // comb's peaks, so the peak-to-notch contrast is deeper than the plain sum gives.
        const double depth01 = std::clamp(depth, 0.0, 1.0);
        const auto delay_at = [&](double lfo) { return 0.0003 * rate_ * std::pow(16.0, depth01 * (0.5 + 0.5 * lfo)); };
        const float fl = line_[0].read(delay_at(slow)), fr = line_[1].read(delay_at(third));
        flange_[0] = fl; flange_[1] = fr;
        const float ml = static_cast<float>(mix);
        const float out_l = in_l * (1.0f - 0.5f * ml) + fl * 0.5f * ml, out_r = in_r * (1.0f - 0.5f * ml) + fr * 0.5f * ml;
        return {out_l, out_r};
      }
      default: {
        // Three taps at 0, 120 and 240 degrees; the odd one goes to the opposite side.
        const double a = slow + 0.12 * shimmer, b = faster + 0.12 * shimmer, c = third - 0.12 * shimmer;
        wet_l = (line_[0].read(centre + swing * a) + line_[0].read(centre + swing * c)) * 0.6f;
        wet_r = (line_[1].read(centre + swing * b) + line_[1].read(centre - swing * c)) * 0.6f;
        break;
      }
    }
    // The bucket brigade's clock filter: two poles, dull enough to sit behind the dry signal.
    wet_l = bbd2_[0].low(bbd_[0].low(wet_l));
    wet_r = bbd2_[1].low(bbd_[1].low(wet_r));
    const float m = static_cast<float>(mix);
    return {in_l * (1.0f - 0.3f * m) + wet_l * m * 0.9f, in_r * (1.0f - 0.3f * m) + wet_r * m * 0.9f};
  }
 private:
  double rate_ = 48000.0;
  std::array<DelayLine, 2> line_{};
  std::array<Lfo, 3> lfo_{};
  Lfo fast_;
  std::array<OnePole, 2> bbd_{}, bbd2_{}, hp_{};
  std::array<float, 2> flange_{};
};

// --- rotary speaker -----------------------------------------------------------------------------
//
// A Leslie reduced to what the ear keys on: the treble horn and bass rotor spin at different
// speeds (the horn a little faster), each producing amplitude modulation and a Doppler pitch
// wobble, and the two rotors are heard from opposite sides so the stereo image spins.
class Rotary {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    crossover_.prepare(sample_rate);
    crossover_.set(800.0, 0.7);
    horn_line_.prepare(static_cast<size_t>(sample_rate * 0.005));
    drum_line_.prepare(static_cast<size_t>(sample_rate * 0.005));
    horn_phase_ = 0.0; drum_phase_ = 0.25;
    speed_.set_time(600.0, sample_rate);
    speed_.snap(6.0);
  }
  struct Stereo { float left, right; };
  // speed_hz is the horn speed; the drum follows at 85% and ramps slowly like a real motor.
  Stereo process(float in, double mix, double speed_hz) {
    if (mix <= 0.0) return {in, in};
    speed_.target(speed_hz);
    const double horn_hz = speed_.next(), drum_hz = horn_hz * 0.85;
    horn_phase_ += horn_hz / rate_; if (horn_phase_ >= 1.0) horn_phase_ -= 1.0;
    drum_phase_ += drum_hz / rate_; if (drum_phase_ >= 1.0) drum_phase_ -= 1.0;
    const auto bands = crossover_.process(in);
    horn_line_.push(bands.high);
    drum_line_.push(bands.low);
    // Doppler: the delay swings with the cosine of the angle; AM with the sine, deeper on the horn.
    const double horn_angle = kTau * horn_phase_, drum_angle = kTau * drum_phase_;
    const float horn = horn_line_.read(1.0 + 0.0009 * rate_ * (1.0 + std::cos(horn_angle)));
    const float drum = drum_line_.read(1.0 + 0.0005 * rate_ * (1.0 + std::cos(drum_angle)));
    const float horn_l = horn * static_cast<float>(0.55 + 0.45 * std::sin(horn_angle));
    const float horn_r = horn * static_cast<float>(0.55 - 0.45 * std::sin(horn_angle));
    const float drum_l = drum * static_cast<float>(0.75 - 0.25 * std::sin(drum_angle));
    const float drum_r = drum * static_cast<float>(0.75 + 0.25 * std::sin(drum_angle));
    const float m = static_cast<float>(mix), d = 1.0f - m;
    return {in * d + (horn_l + drum_l) * m, in * d + (horn_r + drum_r) * m};
  }
 private:
  double rate_ = 48000.0, horn_phase_ = 0.0, drum_phase_ = 0.25;
  StateVariable crossover_;
  DelayLine horn_line_, drum_line_;
  Smoother speed_;
};

// --- tremolo --------------------------------------------------------------------------------------
//
// Sine for the smooth amp tremolo, square for the choppy pedal kind, and "bias" for the
// Vox/Fender bias-wiggle shape that dips harder than it swells.
enum class TremoloShape : int { Sine = 0, Square = 1, Bias = 2 };

class Tremolo {
 public:
  void prepare(double sample_rate) { lfo_.prepare(sample_rate); edge_.set(400.0, sample_rate); }
  float process(float in, double depth, double rate_hz, TremoloShape shape) {
    const double raw = lfo_.next(rate_hz, shape == TremoloShape::Square ? LfoShape::Square : LfoShape::Sine);
    if (depth <= 0.0) return in;
    double gain;
    switch (shape) {
      case TremoloShape::Square: gain = 1.0 - depth * (raw < 0.0 ? 1.0 : 0.0); break;
      case TremoloShape::Bias: gain = 1.0 - depth * std::pow(0.5 - 0.5 * raw, 1.8); break;
      default: gain = 1.0 - depth * (0.5 - 0.5 * raw); break;
    }
    // Soften the square's edges so it does not click.
    const float g = edge_.low(static_cast<float>(gain));
    return in * g;
  }
 private:
  Lfo lfo_;
  OnePole edge_;
};

}  // namespace fuzzboi::dsp
