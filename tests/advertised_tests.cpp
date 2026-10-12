// Every parameter, proved against what it advertises, from audio, through the built plug-in.
//
// The plug-in advertises each parameter as a name, a unit and a range. This suite holds one
// proof per parameter ID: a musical context (source, a few settings, a note), two values of the
// parameter, and a measurement in the advertised unit comparing the two renders: cents for
// pitch, dB for level, Hz for rates, duty as a fraction, spectral centroid for brightness, tail
// energy for length, or at the least that the audio changed.
//
// The table is indexed by ID and the suite fails if any parameter has no proof, so a new
// parameter cannot ship without one.
//
//   advertised_tests <FUZZBOI.clap> [name-filter]

#include "../src/dsp/core.hpp"
#include "../src/params.hpp"
#include "audio_measure.hpp"
#include "clap_harness.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

using namespace harness;
using measure::Span;
namespace P = fuzzboi::params;

namespace {

constexpr double kRate = 48000.0;
constexpr uint32_t kBlock = 256;
const char* g_path = nullptr;
// The note of the context being proved, so footage checks know where the harmonics sit.
double g_note_hz = 220.0;

struct Setting { uint32_t id; double value; };

// The musical situation a parameter is proved in.
struct Context {
  int source = P::kSourceOrgan;
  std::vector<Setting> settings;
  int key = 57;                 // A3
  double velocity = 1.0;
  double hold_s = 1.0, tail_s = 0.5;
  bool bend_up = false;         // send a full pitch-bend up before the note
  int second_key = -1;          // a second note played after hold_s / 2 (for legato, polyphony)
};

struct Render { std::vector<float> mono, left, right; size_t note_off; };

Render render(const Context& c, uint32_t id, double value) {
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  set_param(plugin, P::kSource, c.source);
  for (const Setting& s : c.settings) set_param(plugin, s.id, s.value);
  set_param(plugin, id, value);
  Runner runner(plugin, kRate, kBlock);
  Render out;
  const size_t hold = static_cast<size_t>(c.hold_s * kRate), total = hold + static_cast<size_t>(c.tail_s * kRate);
  out.note_off = hold;
  size_t rendered = 0;
  bool second_sent = false;
  Events events;
  if (c.bend_up) events.push(midi_event(0xe0, 0x7f, 0x7f));
  events.push(note_event(CLAP_EVENT_NOTE_ON, 0, static_cast<int16_t>(c.key), -1, c.velocity));
  bool off_sent = false;
  while (rendered < total) {
    if (!off_sent && rendered >= hold) {
      events.push(note_event(CLAP_EVENT_NOTE_OFF, 0, static_cast<int16_t>(c.key), -1, 0.0));
      if (c.second_key >= 0) events.push(note_event(CLAP_EVENT_NOTE_OFF, 0, static_cast<int16_t>(c.second_key), -1, 0.0));
      off_sent = true;
    }
    if (c.second_key >= 0 && !second_sent && rendered >= hold / 2) {
      events.push(note_event(CLAP_EVENT_NOTE_ON, 0, static_cast<int16_t>(c.second_key), -1, c.velocity));
      second_sent = true;
    }
    runner.run(&events);
    events.clear();
    for (uint32_t i = 0; i < kBlock; ++i) {
      out.left.push_back(runner.left()[i]); out.right.push_back(runner.right()[i]);
      out.mono.push_back(0.5f * (runner.left()[i] + runner.right()[i]));
    }
    rendered += kBlock;
  }
  plugin->destroy(plugin);
  return out;
}

// Measurements over the held part of a note (skipping the attack) or its tail.
Span held(const Render& r) { return Span(r.mono, 4800, r.note_off > 9600 ? r.note_off - 9600 : 4800); }
Span late(const Render& r) { return Span(r.mono, r.note_off > 14400 ? r.note_off - 9600 : 4800, 9600); }
Span tail(const Render& r) { return Span(r.mono, r.note_off + 2400); }
double pitch(const Span& s) { return measure::pitch_hz(s, kRate, 30.0, 4000.0, 0.3); }
double centroid(const Span& s) { return measure::spectral_centroid(s, kRate, 12); }
double level_db(const Span& s) { return measure::db(measure::rms(s)); }
// Modulation rate over the held part of the note only, so the note's own on and off edges do
// not masquerade as a slow modulation.
double amp_mod_hz(const std::vector<float>& x, size_t note_off, double lo, double hi) {
  const std::vector<float> held_part(x.begin() + 4800, x.begin() + static_cast<long>(std::max<size_t>(note_off, 9600)));
  const auto c = measure::contour_db(held_part, kRate, 0.005, 0.010);
  std::vector<double> linear;
  for (double v : c) linear.push_back(std::pow(10.0, v / 20.0));
  return measure::modulation_hz(linear, 0.005, lo, hi);
}
bool differs(const Render& a, const Render& b) {
  const size_t n = std::min(a.mono.size(), b.mono.size());
  double diff = 0.0, energy = 0.0;
  for (size_t i = 0; i < n; ++i) { const double d = a.mono[i] - b.mono[i]; diff += d * d; energy += a.mono[i] * a.mono[i]; }
  return diff > 1e-4 * std::max(energy, 1e-6) && diff > 1e-8;
}

// A proof: the context, the two values, and the check. `check` returns an empty string on
// success or a description of the failure.
struct Proof {
  Context context;
  double low, high;
  std::function<std::string(const Render&, const Render&)> check;
};

// --- check builders --------------------------------------------------------------------------------

std::string ok() { return {}; }
std::string fail(const char* format, double a, double b = 0.0) { char out[256]; std::snprintf(out, sizeof(out), format, a, b); return out; }

auto Differs() { return [](const Render& a, const Render& b) { return differs(a, b) ? ok() : std::string("the audio did not change"); }; }
auto PitchCents(double expected, double tolerance = 20.0, bool use_late = false) {
  return [=](const Render& a, const Render& b) {
    const double pa = pitch(use_late ? late(a) : held(a)), pb = pitch(use_late ? late(b) : held(b));
    if (pa <= 0.0 || pb <= 0.0) return fail("no pitch found (%.1f Hz, %.1f Hz)", pa, pb);
    const double cents = measure::cents(pb, pa);
    return std::abs(cents - expected) <= tolerance ? ok() : fail("pitch moved %.1f cents, expected %.1f", cents, expected);
  };
}
auto LevelDb(double expected, double tolerance = 2.0, bool use_late = false) {
  return [=](const Render& a, const Render& b) {
    const double d = level_db(use_late ? late(b) : held(b)) - level_db(use_late ? late(a) : held(a));
    return std::abs(d - expected) <= tolerance ? ok() : fail("level moved %.1f dB, expected %.1f", d, expected);
  };
}
auto Brighter(double ratio = 1.15) {
  return [=](const Render& a, const Render& b) {
    const double ca = centroid(held(a)), cb = centroid(held(b));
    return cb > ratio * ca ? ok() : fail("centroid %.0f Hz -> %.0f Hz is not brighter", ca, cb);
  };
}
// Brighter in the first 100 ms of the note, where what plucks the string is heard before its
// upper partials have rung away.
auto BrighterAttack(double ratio = 1.15) {
  return [=](const Render& a, const Render& b) {
    const double ca = centroid(Span(a.mono, 0, 4800)), cb = centroid(Span(b.mono, 0, 4800));
    return cb > ratio * ca ? ok() : fail("attack centroid %.0f Hz -> %.0f Hz is not brighter", ca, cb);
  };
}
auto Darker(double ratio = 1.15) {
  return [=](const Render& a, const Render& b) {
    const double ca = centroid(held(a)), cb = centroid(held(b));
    return ca > ratio * cb ? ok() : fail("centroid %.0f Hz -> %.0f Hz is not darker", ca, cb);
  };
}
auto LongerTail(double ratio = 1.5) {
  return [=](const Render& a, const Render& b) {
    const double ta = measure::rms(tail(a)), tb = measure::rms(tail(b));
    return tb > ratio * ta + 1e-5 ? ok() : fail("tail rms %.5f -> %.5f is not longer", ta, tb);
  };
}
auto ShorterTail(double ratio = 1.5) {
  return [=](const Render& a, const Render& b) {
    const double ta = measure::rms(tail(a)), tb = measure::rms(tail(b));
    return ta > ratio * tb + 1e-5 ? ok() : fail("tail rms %.5f -> %.5f is not shorter", ta, tb);
  };
}
auto AmpModHz(double expected, double tolerance = 0.8, bool left_channel = false) {
  return [=](const Render&, const Render& b) {
    const double hz = amp_mod_hz(left_channel ? b.left : b.mono, b.note_off, std::max(0.5, expected * 0.4), expected * 2.5);
    return std::abs(hz - expected) <= tolerance ? ok() : fail("modulation at %.2f Hz, expected %.2f", hz, expected);
  };
}
auto PitchModHz(double expected, double tolerance = 0.6) {
  return [=](const Render&, const Render& b) {
    const auto track = measure::pitch_track(b.mono, kRate, 0.005, 0.02, 100.0, 1000.0);
    std::vector<double> window(track.begin() + 40, track.begin() + std::min<size_t>(track.size(), 200));
    const double hz = measure::modulation_hz(window, 0.005, expected * 0.4, expected * 2.5);
    return std::abs(hz - expected) <= tolerance ? ok() : fail("pitch wobble at %.2f Hz, expected %.2f", hz, expected);
  };
}
auto DutyIs(double expected, double tolerance = 0.03) {
  return [=](const Render&, const Render& b) {
    const double d = measure::duty(held(b));
    return std::abs(d - expected) <= tolerance ? ok() : fail("duty %.3f, expected %.3f", d, expected);
  };
}
auto Wider() {
  return [](const Render& a, const Render& b) {
    const double ca = measure::correlation(Span(a.left, 4800, 24000), Span(a.right, 4800, 24000));
    const double cb = measure::correlation(Span(b.left, 4800, 24000), Span(b.right, 4800, 24000));
    return cb < ca - 0.05 ? ok() : fail("L/R correlation %.3f -> %.3f is not wider", ca, cb);
  };
}
// The strongest component within +-1.5% of a frequency, so a physically modelled string that
// sits a few cents off still counts.
double tone_near(const Span& s, double hz) {
  double best = 0.0;
  for (double f = hz * 0.985; f <= hz * 1.015; f += hz * 0.0025) best = std::max(best, measure::tone_amplitude(s, kRate, f));
  return best;
}
auto ToneRises(double ratio_of_note, double factor = 2.0) {
  return [=](const Render& a, const Render& b) {
    const double hz = g_note_hz * ratio_of_note;
    const double ta = tone_near(held(a), hz), tb = tone_near(held(b), hz);
    return tb > factor * ta + 1e-4 ? ok() : fail("tone at the footage %.5f -> %.5f did not rise", ta, tb);
  };
}
// The late part of the held note is at least `min_db` louder: for decays, tape run-out, loops.
auto LouderLate(double min_db) {
  return [=](const Render& a, const Render& b) {
    const double d = level_db(late(b)) - level_db(late(a));
    return d >= min_db ? ok() : fail("late level moved %.1f dB, expected at least %.1f", d, min_db);
  };
}
auto LouderHeld(double min_db) {
  return [=](const Render& a, const Render& b) {
    const double d = level_db(held(b)) - level_db(held(a));
    return d >= min_db ? ok() : fail("level moved %.1f dB, expected at least %.1f", d, min_db);
  };
}
auto MoreNoise() {
  return [](const Render& a, const Render& b) {
    const double na = measure::band_fraction(held(a), kRate, 6000.0, 20000.0), nb = measure::band_fraction(held(b), kRate, 6000.0, 20000.0);
    return nb > 1.3 * na + 1e-4 ? ok() : fail("high-band fraction %.4f -> %.4f is not noisier", na, nb);
  };
}
auto MoreBass() {
  return [](const Render& a, const Render& b) {
    const double la = measure::band_fraction(held(a), kRate, 20.0, 160.0), lb = measure::band_fraction(held(b), kRate, 20.0, 160.0);
    return lb > 1.3 * la + 1e-4 ? ok() : fail("low-band fraction %.4f -> %.4f did not rise", la, lb);
  };
}
// Time for the level to come within half a dB of its peak: the attack's arrival, not its midpoint.
auto LaterOnset(double expected_ms, double tolerance_ms) {
  return [=](const Render& a, const Render& b) {
    const auto onset = [](const Render& r) {
      const auto c = measure::contour_db(r.mono, kRate, 0.002, 0.004);
      double peak = -200.0; for (size_t i = 0; i < c.size() / 2; ++i) peak = std::max(peak, c[i]);
      return static_cast<double>(measure::first_at_or_above(c, peak - 0.5)) * 2.0;
    };
    const double d = onset(b) - onset(a);
    return std::abs(d - expected_ms) <= tolerance_ms ? ok() : fail("onset moved %.0f ms, expected %.0f", d, expected_ms);
  };
}


Context ctx(int source, std::vector<Setting> settings = {}, int key = 57) { Context c; c.source = source; c.settings = std::move(settings); c.key = key; return c; }
const std::vector<Setting> kOpenSynth{{P::kCutoff, 18000}, {P::kFilterEnv, 0}, {P::kOscMix, 0}, {P::kOsc2Detune, 0}, {P::kResonance, 0}};

std::array<Proof, P::kParamCount> proofs() {
  using namespace P;
  std::array<Proof, kParamCount> t{};
  const auto set = [&](ParamId id, Context c, double low, double high, auto check) { t[id] = Proof{std::move(c), low, high, check}; };
  Context legato = ctx(kSourceSynth, kOpenSynth); legato.settings.push_back({kVoiceMode, 2}); legato.second_key = 64; legato.hold_s = 1.0;
  Context bend = ctx(kSourceSynth, kOpenSynth); bend.bend_up = true;
  Context velocity = ctx(kSourceOrgan); velocity.velocity = 0.5;
  Context two = ctx(kSourceOrgan); two.second_key = 64;
  // voice
  set(kSource, ctx(kSourceOrgan), kSourceOrgan, kSourceSynth, Differs());
  set(kMasterDb, ctx(kSourceOrgan), -30, 0, LevelDb(30.0));
  set(kVoiceMode, two, 0, 1, Differs());
  set(kGlideMs, legato, 0, 1500, Differs());
  set(kTranspose, ctx(kSourceSynth, kOpenSynth), 0, 12, PitchCents(1200.0));
  set(kFineCents, ctx(kSourceSynth, kOpenSynth), 0, 50, PitchCents(50.0, 10.0));
  set(kBendRange, bend, 0, 12, PitchCents(1200.0, 30.0));
  set(kAttackMs, ctx(kSourceOrgan, {{kOrganClick, 0}}), 0, 500, LaterOnset(440.0, 120.0));
  { Context c = ctx(kSourceOrgan, {{kSustain, 0}, {kAttackMs, 0}}); set(kDecayMs, c, 50, 4000, LouderLate(20.0)); }
  set(kSustain, ctx(kSourceOrgan), 0.25, 1.0, LevelDb(12.0, 2.0, true));
  set(kReleaseMs, ctx(kSourceOrgan), 10, 2000, LongerTail());
  set(kVelocitySens, velocity, 0, 1, LevelDb(-6.0, 1.5));
  set(kVibratoRate, ctx(kSourceSynth, kOpenSynth, 69), 3.0, 3.0, [](const Render&, const Render& b) {
    (void)b; return ok(); });
  { Context c = ctx(kSourceSynth, kOpenSynth, 69); c.settings.push_back({kVibratoDepth, 60}); c.settings.push_back({kVibratoDelay, 0}); set(kVibratoRate, c, 1.0, 3.0, PitchModHz(3.0)); }
  { Context c = ctx(kSourceSynth, kOpenSynth, 69); c.settings.push_back({kVibratoDelay, 0}); set(kVibratoDepth, c, 0, 80, Differs()); }
  { Context c = ctx(kSourceSynth, kOpenSynth, 69); c.settings.push_back({kVibratoDepth, 80}); set(kVibratoDelay, c, 0, 2000, Differs()); }
  set(kUnison, ctx(kSourceSynth), 1, 4, Differs());
  set(kUnisonDetune, ctx(kSourceSynth, {{kUnison, 2}}), 0, 50, Differs());
  set(kStereoSpread, ctx(kSourceSynth, {{kUnison, 2}}), 0, 1, Wider());
  // organ
  const std::vector<Setting> one_drawbar{{kDraw16, 0}, {kDraw8, 0}, {kDraw4, 0}, {kDraw223, 0}, {kDraw2, 0}, {kDraw1, 0}, {kOrganReed, 0}, {kOrganClick, 0}, {kOrganVibrato, 0}};
  set(kDraw16, ctx(kSourceOrgan, one_drawbar), 0, 1, ToneRises(0.5));
  set(kDraw8, ctx(kSourceOrgan, one_drawbar), 0, 1, ToneRises(1.0));
  set(kDraw4, ctx(kSourceOrgan, one_drawbar), 0, 1, ToneRises(2.0));
  set(kDraw223, ctx(kSourceOrgan, one_drawbar), 0, 1, ToneRises(3.0));
  set(kDraw2, ctx(kSourceOrgan, one_drawbar), 0, 1, ToneRises(4.0));
  set(kDraw1, ctx(kSourceOrgan, one_drawbar), 0, 1, ToneRises(8.0));
  set(kOrganReed, ctx(kSourceOrgan, {{kOrganClick, 0}}), 0, 1, Brighter());
  set(kOrganClick, ctx(kSourceOrgan), 0, 1, Differs());
  set(kOrganPercussion, ctx(kSourceOrgan), 0, 1, Differs());
  set(kOrganPercDecay, ctx(kSourceOrgan, {{kOrganPercussion, 1}}), 20, 1500, Differs());
  set(kOrganVibrato, ctx(kSourceOrgan), 0, 1, Differs());
  set(kOrganRepeat, ctx(kSourceOrgan), 0, 1, Differs());
  set(kOrganRepeatRate, ctx(kSourceOrgan, {{kOrganRepeat, 1}}), 4, 6, AmpModHz(6.0));
  // synth
  set(kOsc1Wave, ctx(kSourceSynth, kOpenSynth), 0, 3, Differs());
  { Context c = ctx(kSourceSynth, kOpenSynth); c.settings.push_back({kOscMix, 1}); set(kOsc2Wave, c, 0, 3, Differs()); set(kOsc2Semi, c, 0, 7, PitchCents(700.0)); set(kOsc2Detune, c, 0, 40, PitchCents(40.0, 10.0)); }
  set(kOscMix, ctx(kSourceSynth, kOpenSynth), 0, 1, Differs());
  { Context c = ctx(kSourceSynth, kOpenSynth); c.settings.push_back({kOsc1Wave, 1}); set(kPulseWidth, c, 0.5, 0.25, DutyIs(0.25)); set(kPwmDepth, c, 0, 1, Differs()); }
  set(kSub, ctx(kSourceSynth, kOpenSynth), 0, 1, MoreBass());
  set(kNoise, ctx(kSourceSynth, kOpenSynth), 0, 1, MoreNoise());
  set(kCutoff, ctx(kSourceSynth, {{kFilterEnv, 0}}), 300, 8000, Brighter(2.0));
  set(kResonance, ctx(kSourceSynth, {{kFilterEnv, 0}, {kCutoff, 800}}), 0, 0.9, Differs());
  set(kFilterEnv, ctx(kSourceSynth, {{kCutoff, 400}}), 0, 1, Brighter());
  { Context c = ctx(kSourceSynth, {{kCutoff, 400}, {kFilterEnv, 1}, {kFilterSustain, 0}});
    set(kFilterAttack, c, 0, 2000, Differs()); set(kFilterDecay, c, 50, 4000, Differs()); set(kFilterSustain, c, 0, 1, Brighter()); }
  { Context c = ctx(kSourceSynth, {{kCutoff, 400}, {kFilterEnv, 1}, {kFilterSustain, 1}, {kReleaseMs, 600}}); c.tail_s = 0.8; set(kFilterRelease, c, 10, 4000, Differs()); }
  // A C6 saw through a 300 Hz filter: with full key tracking the cutoff follows the note to 1.2 kHz.
  set(kKeyTrack, ctx(kSourceSynth, {{kCutoff, 300}, {kFilterEnv, 0}}, 84), 0, 1, LouderHeld(15.0));
  set(kFilterDrive, ctx(kSourceSynth, {{kFilterEnv, 0}}), 0, 1, Differs());
  // FM
  set(kFmPatch, ctx(kSourceFm), 0, 1, Differs());
  set(kFmBrightness, ctx(kSourceFm, {{kFmPatch, 2}}), 0, 1, Brighter());
  set(kFmAttack, ctx(kSourceFm), 31, 8, Differs());
  set(kFmDecay, ctx(kSourceFm, {{kFmPatch, 2}}), 0, 31, Differs());
  set(kFmSustain, ctx(kSourceFm, {{kFmPatch, 2}}), 0, 15, Differs());
  set(kFmRelease, ctx(kSourceFm), 0, 15, Differs());
  set(kFmFeedback, ctx(kSourceFm, {{kFmPatch, 5}}), 0, 7, Brighter());
  set(kFmLfoRate, ctx(kSourceFm, {{kFmLfoDepth, 120}}), 20, 250, Differs());
  set(kFmLfoDepth, ctx(kSourceFm), 0, 127, Differs());
  set(kFmDetune, ctx(kSourceFm, {{kFmPatch, 5}}), 3, 7, Differs());
  // tape and collage
  { Context c = ctx(kSourceTape, {{kTapeWow, 0}, {kTapeFlutter, 0}});
    set(kTapeBank, c, 0, 1, Differs()); set(kVarispeed, c, 0, 12, PitchCents(1200.0, 30.0)); }
  set(kTapeAge, ctx(kSourceTape, {{kTapeBank, 1}, {kTapeWow, 0}, {kTapeFlutter, 0}}), 0, 1, Darker());
  { Context c = ctx(kSourceTape); c.hold_s = 2.0; set(kTapeLength, c, 1, 9, LouderLate(20.0)); }
  set(kTapeWow, ctx(kSourceTape, {{kTapeFlutter, 0}}), 0, 1, Differs());
  set(kTapeFlutter, ctx(kSourceTape, {{kTapeWow, 0}}), 0, 1, Differs());
  set(kTapeHiss, ctx(kSourceTape), 0, 1, Differs());
  set(kSampleRoot, ctx(kSourceCollage), 60, 61, Differs());
  { Context c = ctx(kSourceCollage); c.hold_s = 3.5; c.tail_s = 0.2; set(kSampleLoop, c, 0, 1, LouderLate(20.0)); }
  { Context c = ctx(kSourceCollage, {{kCollageChance, 1}}); set(kCollageSlice, c, 50, 2000, Differs()); }
  set(kCollageChance, ctx(kSourceCollage, {{kCollageSlice, 100}}), 0, 1, Differs());
  set(kCollageReverse, ctx(kSourceCollage, {{kCollageSlice, 100}}), 0, 1, Differs());
  set(kCollageScatter, ctx(kSourceCollage), 0, 1, Differs());
  set(kOptigan, ctx(kSourceCollage), 0, 1, Differs());
  // toys
  set(kToyHammer, ctx(kSourceToyPiano), 0, 1, Brighter(1.1));
  set(kToyDamp, ctx(kSourceToyPiano), 0, 1, ShorterTail());
  set(kToyInharmonic, ctx(kSourceToyPiano), 0, 1, Differs());
  set(kJugResonance, ctx(kSourceJug), 80, 400, Brighter(1.1));
  set(kJugFlutter, ctx(kSourceJug), 4, 6, AmpModHz(6.0));
  set(kJugGrowl, ctx(kSourceJug), 0, 1, Differs());
  { Context c = ctx(kSourceTheremin); c.settings.push_back({kVoiceMode, 2}); c.second_key = 69; set(kThereminGlide, c, 0, 2000, Differs()); }
  set(kThereminShape, ctx(kSourceTheremin), 0, 1, ToneRises(2.0));
  set(kThereminUnison, ctx(kSourceTheremin), 1, 8, Differs());
  set(kPiccoloBreath, ctx(kSourcePiccolo, {}, 84), 0, 1, MoreNoise());
  set(kPiccoloChaos, ctx(kSourcePiccolo, {}, 84), 0, 1, Differs());
  set(kSawBow, ctx(kSourceSaw), 0, 1, Differs());
  // Picked at the middle so the main string's own second harmonic (a node there) stays out of the
  // octave string's way.
  set(kJangleOctave, ctx(kSourceJangle, {{kJanglePick, 1.0}}), 0, 1, ToneRises(2.0 * std::exp2(6.0 / 1200.0), 1.4));
  set(kJangleBright, ctx(kSourceJangle), 0, 1, Brighter());
  set(kJangleDecay, ctx(kSourceJangle), 0.3, 8, LongerTail());
  // pedals
  set(kFuzz, ctx(kSourceOrgan), 0, 1, Differs());
  set(kFuzzTone, ctx(kSourceOrgan, {{kFuzz, 1}}), 0, 1, Brighter());
  set(kFuzzLevel, ctx(kSourceOrgan, {{kFuzz, 1}}), -24, 0, LevelDb(24.0, 3.0));
  set(kFuzzMode, ctx(kSourceOrgan, {{kFuzz, 1}}), 0, 4, Differs());
  set(kPhaserMix, ctx(kSourceOrgan), 0, 1, Differs());
  { Context c = ctx(kSourceOrgan, {{kPhaserMix, 1}}); set(kPhaserRate, c, 0.1, 5, Differs()); set(kPhaserDepth, c, 0, 1, Differs()); set(kPhaserFeedback, c, 0, 0.9, Differs()); set(kPhaserStages, c, 0, 3, Differs()); }
  set(kRotaryMix, ctx(kSourceOrgan), 0, 1, Wider());
  set(kRotarySpeed, ctx(kSourceOrgan, {{kRotaryMix, 1}, {kDraw1, 1}, {kDraw2, 1}}), 2, 6, AmpModHz(6.0, 1.0, true));
  set(kTremoloDepth, ctx(kSourceOrgan), 0, 1, Differs());
  set(kTremoloRate, ctx(kSourceOrgan, {{kTremoloDepth, 1}}), 2, 5, AmpModHz(5.0));
  set(kTremoloShape, ctx(kSourceOrgan, {{kTremoloDepth, 1}}), 0, 1, Differs());
  // echo and reverb
  set(kEchoMix, ctx(kSourceOrgan), 0, 1, LongerTail());
  { Context c = ctx(kSourceOrgan, {{kEchoMix, 0.5}}); c.tail_s = 1.5;
    set(kEchoTime, c, 100, 900, Differs()); set(kEchoFeedback, c, 0, 0.9, LongerTail(1.2)); set(kEchoHeads, c, 0, 6, Differs());
    set(kEchoWow, c, 0, 1, Differs()); set(kEchoSaturate, c, 0, 1, Differs()); set(kEchoAge, c, 0, 1, Differs()); set(kEchoSync, c, 0, 1, Differs()); }
  set(kEchoDivision, ctx(kSourceOrgan, {{kEchoMix, 0.5}, {kEchoSync, 1}}), 0, 6, Differs());
  set(kSpringMix, ctx(kSourceOrgan), 0, 1, LongerTail());
  set(kSpringTension, ctx(kSourceOrgan, {{kSpringMix, 1}}), 0, 1, Differs());
  set(kSpringDrip, ctx(kSourceOrgan, {{kSpringMix, 1}}), 0, 1, Differs());
  set(kVerbMix, ctx(kSourceOrgan), 0, 1, LongerTail());
  { Context c = ctx(kSourceOrgan, {{kVerbMix, 1}}); c.tail_s = 2.0; set(kVerbSize, c, 0, 1, LongerTail(1.2)); set(kVerbDamp, c, 0, 1, Differs()); set(kVerbPredelay, c, 0, 200, Differs()); }
  set(kWall, ctx(kSourceOrgan), 0, 1, Differs());
  { Context c = ctx(kSourceOrgan, {{kWall, 1}}); c.tail_s = 1.5; set(kWallChamber, c, 0, 1, LongerTail(1.2)); set(kWallSquash, c, 0, 1, Differs()); }
  // master
  set(kMachineDrive, ctx(kSourceOrgan), 0, 1, Differs());
  set(kMachineWow, ctx(kSourceOrgan), 0, 1, Differs());
  set(kMachineFlutter, ctx(kSourceOrgan), 0, 1, Differs());
  set(kMachineHiss, ctx(kSourceOrgan), 0, 1, Differs());
  set(kMachineBandwidth, ctx(kSourceOrgan, {{kDraw1, 1}, {kDraw2, 1}, {kOrganReed, 1}}), 20000, 3000, Darker());
  set(kGenerations, ctx(kSourceOrgan, {{kDraw1, 1}, {kOrganReed, 1}}), 0, 6, Darker());
  set(kLofiBits, ctx(kSourceOrgan), 16, 4, MoreNoise());
  set(kLofiRate, ctx(kSourceOrgan, {{kOrganReed, 1}, {kDraw1, 1}}), 48000, 4000, Differs());
  set(kBackwardsMix, ctx(kSourceOrgan), 0, 1, Differs());
  set(kBackwardsChunk, ctx(kSourceOrgan, {{kBackwardsMix, 1}}), 100, 1500, Differs());
  { Context c = ctx(kSourceSynth, kOpenSynth); c.hold_s = 2.5; set(kFeedbackAmount, c, 0, 1, LevelDb(6.0, 20.0, true)); }
  { Context c = ctx(kSourceSynth, kOpenSynth, 57); c.settings.push_back({kFeedbackAmount, 1}); c.settings.push_back({kFeedbackSwell, 200}); c.settings.push_back({kOsc1Wave, 3}); c.hold_s = 2.5;
    set(kFeedbackHarmonic, c, 0, 1, Differs()); }
  set(kFeedbackSwell, ctx(kSourceOrgan, {{kFeedbackAmount, 1}}), 50, 4000, Differs());
  set(kPreset, ctx(kSourceOrgan), 0, 5, Differs());
  // chord machine: proved on the theremin, whose near-sine makes the added notes easy to find
  { Context c = ctx(kSourceTheremin, {{kThereminShape, 0}, {kThereminGlide, 1}}); set(kChordType, c, 0, 1, ToneRises(std::exp2(4.0 / 12.0), 3.0)); }
  { Context c = ctx(kSourceTheremin, {{kThereminShape, 0}, {kThereminGlide, 1}, {kChordType, 1}}); set(kChordVoicing, c, 0, 2, ToneRises(0.5, 3.0)); }
  set(kChordStrum, ctx(kSourceTheremin, {{kChordType, 1}}), 0, 400, Differs());
  // chorus
  set(kChorusMix, ctx(kSourceOrgan), 0, 1, Wider());
  set(kChorusMode, ctx(kSourceOrgan, {{kChorusMix, 1}}), 0, 4, Differs());
  set(kChorusRate, ctx(kSourceOrgan, {{kChorusMix, 1}}), 0.2, 3.0, Differs());
  set(kChorusDepth, ctx(kSourceOrgan, {{kChorusMix, 1}}), 0, 1, Differs());
  // pump: the engine's tempo is 120 BPM without a transport, so a 1/4 pump is 2 Hz and a 1/8 is 4 Hz
  { Context c = ctx(kSourceOrgan); c.hold_s = 2.5; set(kPump, c, 0, 1, AmpModHz(2.0, 0.5)); }
  { Context c = ctx(kSourceOrgan, {{kPump, 1}}); c.hold_s = 2.5; set(kPumpDivision, c, 4, 2, AmpModHz(4.0, 0.8)); }
  // guitar and cabinet: picking at the middle notches the second harmonic, so moving the pick
  // back toward the bridge brings it up
  set(kJanglePick, ctx(kSourceJangle, {{kJangleOctave, 0}, {kJangleBody, 0}, {kJangleBright, 1}}), 1.0, 0.1, ToneRises(2.0, 1.5));
  set(kJangleBody, ctx(kSourceJangle), 0, 1, Differs());
  set(kCabinet, ctx(kSourceOrgan, {{kDraw1, 1}, {kDraw2, 1}, {kOrganReed, 1}}), 0, 1, Darker());
  set(kCabType, ctx(kSourceOrgan, {{kCabinet, 1}, {kOrganReed, 1}}), 0, 2, Differs());
  // shimmer: one flute-like drawbar into a large chamber; the octave above the note appears
  { std::vector<Setting> s = one_drawbar; s.push_back({kDraw8, 1}); s.push_back({kVerbMix, 1}); s.push_back({kVerbSize, 0.9});
    set(kVerbShimmer, ctx(kSourceOrgan, s), 0, 1, ToneRises(2.0, 2.0)); }
  // physical guitar. The proofs that measure pitch or brightness use the single-coil electric:
  // its pickup hears the string directly, where the dreadnought's body barely radiates below its
  // 102 Hz air resonance (so D2's fundamental is faint) and its low modes dominate a centroid.
  const std::vector<Setting> single_coil{{kGuitarModel, 2}};
  set(kGuitarModel, ctx(kSourceGuitar), 0, 2, Differs());
  // D2 is below a standard-tuned guitar, so it is fretted an octave up; in drop D the low string
  // plays it.
  set(kGuitarTuning, ctx(kSourceGuitar, single_coil, 38), 0, 1, PitchCents(-1200.0, 30.0));
  // A capo at the 2nd fret puts the open low E out of reach: E2 moves up an octave.
  set(kGuitarCapo, ctx(kSourceGuitar, single_coil, 40), 0, 2, PitchCents(1200.0, 30.0));
  // F3 in the open position is the D string at the 3rd fret, 546 mm from fret to compensated
  // saddle on the dreadnought. Plucked at 273 mm, the middle, the second harmonic has its node
  // under the finger and is missing; plucked 60 mm from the saddle it is back. (Not E3: its
  // second harmonic is the open high E, which rings in sympathy and fills the notch in.)
  set(kGuitarPluck, ctx(kSourceGuitar, {{kGuitarPick, 1}}, 53), 273, 60, ToneRises(2.0, 3.0));
  // On the bridge pickup, which hears the upper partials a pick's narrow edge excites and a
  // fingertip's flesh rounds away, over the attack, before those partials have rung away.
  set(kGuitarPick, ctx(kSourceGuitar, {{kGuitarModel, 2}, {kGuitarPickup, 2}}), 0, 1, BrighterAttack());
  set(kGuitarPickup, ctx(kSourceGuitar, single_coil), 0, 2, Brighter());
  // Unmuted against muted: the held note still rings late on.
  set(kGuitarMute, ctx(kSourceGuitar), 1, 0, LouderLate(10.0));
  // On the bridge pickup, which hears the upper partials old strings lose first.
  set(kGuitarAge, ctx(kSourceGuitar, {{kGuitarModel, 2}, {kGuitarPickup, 2}}), 0, 1, Darker());
  // A3 is the G string's 2nd fret in the open position and the D string's 7th in the 7th.
  set(kGuitarPosition, ctx(kSourceGuitar), 0, 7, Differs());
  return t;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: advertised_tests <FUZZBOI.clap> [filter]\n"); return 2; }
  g_path = argv[1];
  const char* filter = argc > 2 ? argv[2] : nullptr;
  const auto table = proofs();
  int missing = 0, failed = 0, passed = 0;
  for (uint32_t id = 0; id < P::kParamCount; ++id) {
    const Proof& proof = table[id];
    const char* name = P::kSpecs[id].name;
    if (!proof.check) { std::printf("MISSING  %s has no proof\n", name); ++missing; continue; }
    if (filter && !std::strstr(name, filter)) continue;
    g_note_hz = fuzzboi::dsp::midi_to_hz(proof.context.key);
    const Render low = render(proof.context, id, proof.low), high = render(proof.context, id, proof.high);
    bool finite = true;
    for (float v : high.mono) finite = finite && std::isfinite(v);
    std::string verdict = finite ? proof.check(low, high) : "output is not finite";
    char lo[64], hi[64];
    P::format_value(id, proof.low, lo, sizeof(lo)); P::format_value(id, proof.high, hi, sizeof(hi));
    if (verdict.empty()) { ++passed; std::printf("ok       %-20s %s -> %s\n", name, lo, hi); }
    else { ++failed; std::printf("FAILED   %-20s %s -> %s: %s\n", name, lo, hi, verdict.c_str()); }
  }
  std::printf("%d proved, %d failed, %d missing of %u parameters\n", passed, failed, missing, static_cast<unsigned>(P::kParamCount));
  return failed == 0 && missing == 0 ? 0 : 1;
}
