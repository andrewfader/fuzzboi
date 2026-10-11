// Echo and reverberation: a three-head tape echo in the Space Echo / Echoplex family, a spring
// reverb built from dispersive allpass chains after Parker, a Schroeder/Moorer chamber with
// Freeverb's tunings, and the Wall of Sound macro that stacks doubling, chamber and squash.
#pragma once

#include <array>
#include <cmath>

#include "../core.hpp"
#include "../envelopes.hpp"
#include "../filters.hpp"
#include "../tape.hpp"

namespace fuzzboi::dsp {

// --- tape echo ------------------------------------------------------------------------------------
//
// One loop of tape with a record head and three playback heads at increasing distances; the
// head selector picks which heads feed the output and the feedback. Wow and flutter modulate
// the tape speed, the feedback path saturates and loses top end with every pass ("age"), and
// feedback above unity runs away into self-oscillation the way the real box does, held in
// check by the saturation.
class TapeEcho {
 public:
  static constexpr double kMaxSeconds = 2.0;
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    line_.prepare(static_cast<size_t>(sample_rate * kMaxSeconds) + 8);
    speed_.prepare(sample_rate);
    loss_.set(6000.0, sample_rate);
    hp_.set(90.0, sample_rate);
    time_.set_time(80.0, sample_rate);
    time_.snap(0.38);
    dc_.set(5.0, sample_rate);
  }
  void clear() { line_.clear(); feedback_ = 0.0f; }
  // heads: 0..6 as kEchoHeadNames (1, 2, 3, 1+2, 2+3, 1+3, 1+2+3). time_s is head 1's delay;
  // heads 2 and 3 sit at 1.5x and 2x along the tape, as on the RE-201's spacing.
  float process(float in, double mix, double time_s, double feedback, int heads, double wow, double saturate, double age) {
    const double speed = speed_.next(wow, wow * 0.7);
    time_.target(std::clamp(time_s, 0.03, kMaxSeconds / 2.0));
    const double base = time_.next() * rate_ * speed;
    const double distances[3] = {base, base * 1.5, base * 2.0};
    static constexpr int kMasks[7] = {1, 2, 4, 3, 6, 5, 7};
    const int mask = kMasks[std::clamp(heads, 0, 6)];
    float heard = 0.0f;
    int count = 0;
    for (int h = 0; h < 3; ++h)
      if (mask & (1 << h)) { heard += line_.read(std::min(distances[h], static_cast<double>(line_.capacity()))); ++count; }
    if (count > 1) heard /= static_cast<float>(std::sqrt(static_cast<double>(count)));
    // The feedback path: preamp saturation, head loss that worsens with age, bass trimmed. The
    // saturation is normalised so a quiet repeat still loses exactly `feedback` per pass: the
    // knob is the loop gain, and the drive only compresses loud repeats. (Without that, 75%
    // feedback with the preamp hot looped above unity and the echo never ended.)
    loss_.set(9000.0 * std::pow(0.25, age), rate_);
    const float drive = static_cast<float>(1.0 + 2.0 * saturate);
    const float hot = heard * static_cast<float>(feedback) * drive;
    float back = warm_clip(hot) / drive;
    back = hp_.high(loss_.low(back));
    line_.push(dc_.process(in + back));
    if (mix <= 0.0) return in;
    return in * static_cast<float>(1.0 - mix) + heard * static_cast<float>(mix);
  }
 private:
  double rate_ = 48000.0;
  DelayLine line_;
  WowFlutter speed_;
  OnePole loss_, hp_;
  Smoother time_;
  DcBlocker dc_;
  float feedback_ = 0.0f;
};

// --- spring reverb --------------------------------------------------------------------------------
//
// A spring's echoes are dispersive: high frequencies arrive later than low ones, so each
// reflection is a downward chirp ("boing"). Parker's method builds that from a long chain of
// identical first-order allpasses (a spectral delay) inside a feedback loop. Two springs of
// different lengths are summed; "tension" sets the loop delay, "drip" the number of allpasses
// and so how pronounced the chirp is.
class SpringReverb {
 public:
  static constexpr int kMaxStages = 48;
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    for (auto& s : springs_) { s.line.prepare(static_cast<size_t>(sample_rate * 0.12)); for (auto& a : s.chain) a.set(2400.0, sample_rate); s.damp.set(3500.0, sample_rate); s.feedback = 0.0f; }
    dc_.set(10.0, sample_rate);
    input_hp_.set(150.0, sample_rate);
  }
  void clear() { for (auto& s : springs_) { s.line.clear(); s.feedback = 0.0f; for (auto& a : s.chain) a.reset(); } }
  float process(float in, double mix, double tension, double drip) {
    if (mix <= 0.0) return in;
    const int stages = 8 + static_cast<int>(std::lround(drip * (kMaxStages - 8)));
    const float x = input_hp_.high(in);
    float wet = 0.0f;
    for (size_t i = 0; i < springs_.size(); ++i) {
      Spring& s = springs_[i];
      const double seconds = (0.028 + 0.05 * (1.0 - tension)) * (i == 0 ? 1.0 : 1.37);
      float y = x * 0.5f + s.feedback;
      for (int k = 0; k < stages; ++k) y = s.chain[static_cast<size_t>(k)].process(y);
      s.line.push(y);
      const float delayed = s.damp.low(s.line.read(seconds * rate_));
      s.feedback = std::tanh(delayed * 0.72f);
      wet += delayed;
    }
    wet = dc_.process(wet * 0.6f);
    return in * static_cast<float>(1.0 - mix) + wet * static_cast<float>(mix);
  }
 private:
  struct Spring { DelayLine line; std::array<Allpass1, kMaxStages> chain{}; OnePole damp; float feedback = 0.0f; };
  double rate_ = 48000.0;
  std::array<Spring, 2> springs_{};
  DcBlocker dc_;
  OnePole input_hp_;
};

// --- chamber ---------------------------------------------------------------------------------------
//
// Eight parallel damped combs into four series allpasses per channel, with Freeverb's public
// domain tunings (Jezar at Dreampoint) and a 23-sample right-channel offset for width.
class Chamber {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    const double scale = sample_rate / 44100.0;
    for (int ch = 0; ch < 2; ++ch)
      for (size_t i = 0; i < 8; ++i) {
        combs_[ch][i].line.prepare(static_cast<size_t>((kCombs[i] + (ch ? 23 : 0)) * scale) + 2);
        combs_[ch][i].length = static_cast<size_t>((kCombs[i] + (ch ? 23 : 0)) * scale);
        combs_[ch][i].filter = 0.0f;
      }
    for (int ch = 0; ch < 2; ++ch)
      for (size_t i = 0; i < 4; ++i) {
        allpasses_[ch][i].line.prepare(static_cast<size_t>((kAllpasses[i] + (ch ? 23 : 0)) * scale) + 2);
        allpasses_[ch][i].length = static_cast<size_t>((kAllpasses[i] + (ch ? 23 : 0)) * scale);
      }
    predelay_.prepare(static_cast<size_t>(sample_rate * 0.25) + 2);
  }
  void clear() {
    for (auto& ch : combs_) for (auto& c : ch) { c.line.clear(); c.filter = 0.0f; }
    for (auto& ch : allpasses_) for (auto& a : ch) a.line.clear();
    predelay_.clear();
  }
  struct Stereo { float left, right; };
  static double comb_feedback(double size) { return 0.7 + 0.28 * size; }
  Stereo process(float in, double mix, double size, double damping, double predelay_ms) {
    if (mix <= 0.0) { predelay_.push(in); return {in, in}; }
    predelay_.push(in);
    const float x = predelay_.read(predelay_ms * 0.001 * rate_) * 0.015f;
    const float feedback = static_cast<float>(comb_feedback(size));
    const float damp = static_cast<float>(damping * 0.8);
    float out[2];
    for (int ch = 0; ch < 2; ++ch) {
      float sum = 0.0f;
      for (auto& c : combs_[ch]) {
        const float y = c.line.read_int(c.length - 1);
        c.filter = y * (1.0f - damp) + c.filter * damp;
        c.line.push(x + c.filter * feedback);
        sum += y;
      }
      for (auto& a : allpasses_[ch]) {
        const float y = a.line.read_int(a.length - 1);
        const float v = sum + y * 0.5f;
        a.line.push(v);
        sum = y - v * 0.5f;
      }
      out[ch] = sum;
    }
    const float m = static_cast<float>(mix), d = 1.0f - m;
    return {in * d + out[0] * m * 1.5f, in * d + out[1] * m * 1.5f};
  }
 private:
  static constexpr int kCombs[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
  static constexpr int kAllpasses[4] = {556, 441, 341, 225};
  struct Comb { DelayLine line; size_t length = 1; float filter = 0.0f; };
  struct AllpassDelay { DelayLine line; size_t length = 1; };
  double rate_ = 48000.0;
  std::array<std::array<Comb, 8>, 2> combs_{};
  std::array<std::array<AllpassDelay, 4>, 2> allpasses_{};
  DelayLine predelay_;
};

// --- shimmer ----------------------------------------------------------------------------------------
//
// The shimmer reverb (Eno and Lanois's octave-up feedback, later every shoegaze pedalboard):
// the chamber's own output is pitched up an octave and fed back into the chamber, so every
// pass of the tail climbs another octave and the room fills with a glassy halo. The shifter is
// the classic two-head delay-line one: two read heads sweep through a 60 ms window at twice
// tape speed, each faded in and out with a Hann window half a cycle apart so the splices do
// not click. The octave is heard directly (the halo) and fed back (the climb). The fed-back
// share is band-limited (no rumble below, no fizz above), soft limited, and scaled by how far
// the chamber's combs are from ringing forever, so a bigger room gets a gentler push and the
// loop dies away at every size and damping.
class PitchShifter {
 public:
  void prepare(double sample_rate, double window_s = 0.06) {
    window_ = std::max(16.0, window_s * sample_rate);
    line_.prepare(static_cast<size_t>(window_) + 4);
    phase_ = 0.0;
  }
  void clear() { line_.clear(); phase_ = 0.0; }
  // ratio > 1 shifts up (2 is an octave); ratio 1 is a plain delay of about half a window.
  float process(float in, double ratio) {
    line_.push(in);
    phase_ += (ratio - 1.0) / window_;
    phase_ -= std::floor(phase_);
    float out = 0.0f;
    for (int head = 0; head < 2; ++head) {
      double p = phase_ + 0.5 * head;
      p -= std::floor(p);
      const double fade = std::sin(kPi * p);
      out += line_.read((1.0 - p) * window_) * static_cast<float>(fade * fade);
    }
    return out;
  }
 private:
  DelayLine line_;
  double window_ = 2880.0, phase_ = 0.0;
};

class Shimmer {
 public:
  void prepare(double sample_rate) {
    shifter_.prepare(sample_rate);
    hp_.set(300.0, sample_rate);
    lp_.set(6000.0, sample_rate);
  }
  void clear() { shifter_.clear(); }
  struct Out { float feed, halo; };
  // Takes the chamber's (mono) output; `feed` goes back into its input, `halo` is heard.
  Out process(float wet, double amount, double size) {
    if (amount <= 0.0) { shifter_.process(0.0f, 2.0); return {0.0f, 0.0f}; }
    const float up = lp_.low(hp_.high(shifter_.process(wet, 2.0)));
    const double loop = kLoopGain * (1.0 - Chamber::comb_feedback(size)) * amount;
    return {0.5f * std::tanh(up * static_cast<float>(loop) * 2.0f), up * static_cast<float>(kHaloGain * amount)};
  }
  static constexpr double kLoopGain = 30.0;
  // The halo sits above the chamber's own level: at full shimmer the octave is a voice, not a hint.
  static constexpr double kHaloGain = 4.0;
 private:
  PitchShifter shifter_;
  OnePole hp_, lp_;
};

// --- wall of sound ----------------------------------------------------------------------------------
//
// Spector at Gold Star: everything doubled and tripled, the whole room pumped through an echo
// chamber, then squashed. Here: three detuned/delayed copies of the signal added to itself
// (the doubling), a share of it sent to the chamber (routed by the engine), and a slow, deep
// compressor on the sum. `amount` scales all three; `chamber` and `squash` weight the last two.
class WallOfSound {
 public:
  void prepare(double sample_rate) {
    rate_ = sample_rate;
    line_.prepare(static_cast<size_t>(sample_rate * 0.06) + 4);
    for (auto& l : lfo_) l.prepare(sample_rate);
    envelope_ = 0.0f;
    attack_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.008 * sample_rate)));
    release_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.25 * sample_rate)));
  }
  // Returns the doubled signal; `send` receives what should go to the chamber.
  float process(float in, double amount, double chamber, double squash, float* send) {
    if (amount <= 0.0) { line_.push(in); if (send) *send = 0.0f; return in; }
    line_.push(in);
    float doubled = in;
    static constexpr double kOffsets[3] = {0.017, 0.029, 0.041};
    for (size_t i = 0; i < 3; ++i) {
      const double wobble = 0.0006 * lfo_[i].next(0.3 + 0.17 * static_cast<double>(i), LfoShape::Triangle);
      doubled += line_.read((kOffsets[i] + wobble) * rate_) * 0.7f;
    }
    doubled *= 0.5f;
    if (send) *send = doubled * static_cast<float>(amount * chamber);
    // Squash: a program-dependent compressor with a lot of make-up, ratio deepening with the knob.
    // The detector listens to the dry input so the doubling's comb response cannot fool it.
    const float level = std::abs(in);
    envelope_ += (level > envelope_ ? attack_ : release_) * (level - envelope_);
    const double ratio = 1.0 + 7.0 * squash * amount;
    const double threshold = 0.05;
    double gain = 1.0;
    if (envelope_ > threshold) gain = std::pow(threshold / envelope_, 1.0 - 1.0 / ratio);
    const float squashed = doubled * static_cast<float>(gain * (1.0 + 1.2 * squash * amount));
    return in * static_cast<float>(1.0 - amount) + squashed * static_cast<float>(amount);
  }
 private:
  double rate_ = 48000.0;
  DelayLine line_;
  std::array<Lfo, 3> lfo_{};
  float envelope_ = 0.0f, attack_ = 0.01f, release_ = 0.001f;
};

}  // namespace fuzzboi::dsp
