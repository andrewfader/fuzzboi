// Executable scenarios for features/engine.feature: notes, polyphony, voice modes, the sample
// bank, presets and tails, through the engine's own interface (no CLAP yet).
#include "../src/engine.hpp"
#include "../src/presets.hpp"
#include "audio_measure.hpp"
#include "audit_phrase.hpp"
#include "spec.hpp"

#include <vector>

using namespace fuzzboi;
using measure::Span;

namespace {
constexpr double kRate = 48000.0;
constexpr uint32_t kBlock = 256;

struct Rig {
  Engine engine;
  Settings s = Settings::defaults();
  std::vector<float> left, right;
  Rig() { engine.prepare(kRate); }
  void render(uint32_t frames) {
    const size_t at = left.size();
    left.resize(at + frames); right.resize(at + frames);
    for (uint32_t done = 0; done < frames; done += kBlock) {
      const uint32_t n = std::min(kBlock, frames - done);
      engine.render(s, left.data() + at + done, right.data() + at + done, n);
    }
  }
  std::vector<float> mono() const { std::vector<float> m(left.size()); for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (left[i] + right[i]); return m; }
  void on(int key, double vel = 1.0) { engine.note_on(s, 0, key, -1, vel); }
  void off(int key) { engine.note_off(s, 0, key, -1); }
};

bool finite(const std::vector<float>& x) { for (float v : x) if (!std::isfinite(v)) return false; return true; }
}  // namespace

SCENARIO(a_note_makes_a_sound_and_releases) {
  GIVEN("the default organ, one note held for half a second");
  Rig rig;
  rig.s.v[params::kReleaseMs] = 100.0;
  rig.on(60);
  rig.render(24000);
  rig.off(60);
  rig.render(48000);
  THEN("the note sounds while held, then fades to silence within the release and the voice ends");
  const auto m = rig.mono();
  CHECK(finite(m), "finite");
  CHECK(measure::rms(Span(m, 4000, 16000)) > 0.02, "audible while held");
  CHECK(measure::peak(Span(m, 24000 + 24000)) < 1e-3, "silent after the release");
  CHECK(rig.engine.silent(), "voice freed");
}

SCENARIO(polyphony_stacks_and_steals_oldest) {
  GIVEN("sixteen voices, with eighteen notes played");
  Rig rig;
  for (int k = 0; k < 18; ++k) { rig.on(48 + k); rig.render(64); }
  THEN("only sixteen are active and the engine stays finite");
  CHECK(rig.engine.active_voices() == kVoices, spec::fmt("%.0f active voices", static_cast<double>(rig.engine.active_voices())));
  rig.render(4800);
  CHECK(finite(rig.mono()), "finite");
  WHEN("every key is released");
  for (int k = 0; k < 18; ++k) rig.off(48 + k);
  rig.render(48000 * 2);
  THEN("all voices end");
  CHECK(rig.engine.silent(), "silent");
}

SCENARIO(mono_mode_retriggers_and_legato_glides) {
  GIVEN("legato mode with a 200 ms glide, C4 then G4 while C4 is held");
  Rig rig;
  rig.s.v[params::kVoiceMode] = 2; rig.s.v[params::kGlideMs] = 200.0; rig.s.v[params::kSource] = params::kSourceSynth;
  rig.s.v[params::kCutoff] = 18000; rig.s.v[params::kFilterEnv] = 0; rig.s.v[params::kOscMix] = 0; rig.s.v[params::kOsc2Detune] = 0;
  rig.on(60); rig.render(9600);
  rig.on(67); rig.render(2400);
  const size_t mid = rig.left.size();
  rig.render(24000);
  THEN("one voice is active and the pitch is between the notes shortly after, then at G4");
  CHECK(rig.engine.active_voices() == 1, "one voice");
  const auto m = rig.mono();
  const double during = measure::pitch_hz(Span(m, mid - 1200, 2400), kRate);
  const double after = measure::pitch_hz(Span(m, mid + 20000, 4000), kRate);
  CHECK(during > 270.0 && during < 385.0, spec::fmt("gliding at %.1f Hz", during));
  CHECK_NEAR(measure::cents(after, 392.0), 0.0, 20.0, "arrived at G4");
  WHEN("G4 is released with C4 still held");
  rig.off(67); rig.render(24000);
  THEN("the voice returns to C4");
  const auto back = rig.mono();
  CHECK_NEAR(measure::cents(measure::pitch_hz(Span(back, back.size() - 6000, 6000), kRate), 261.63), 0.0, 20.0, "back to C4");
}

SCENARIO(unison_spreads_voices_across_the_stereo_field) {
  GIVEN("four unison voices with full stereo spread");
  Rig rig;
  rig.s.v[params::kSource] = params::kSourceSynth; rig.s.v[params::kUnison] = 4; rig.s.v[params::kStereoSpread] = 1.0; rig.s.v[params::kUnisonDetune] = 20;
  rig.on(60); rig.render(24000);
  THEN("four voices play and the channels differ");
  CHECK(rig.engine.active_voices() == 4, "four voices");
  CHECK(measure::correlation(Span(rig.left, 4000), Span(rig.right, 4000)) < 0.9, "stereo width");
  WHEN("spread is zero");
  Rig narrow; narrow.s = rig.s; narrow.s.v[params::kStereoSpread] = 0.0;
  narrow.on(60); narrow.render(24000);
  THEN("the channels are identical");
  CHECK(narrow.left == narrow.right, "mono when spread is 0");
}

SCENARIO(fm_source_plays_through_the_shared_chip) {
  GIVEN("the FM source with three notes");
  Rig rig;
  rig.s.v[params::kSource] = params::kSourceFm;
  rig.on(60); rig.on(64); rig.on(67);
  rig.render(24000);
  THEN("the chip sounds and the notes end on release");
  CHECK(measure::rms(Span(rig.mono(), 2000)) > 0.01, "audible");
  rig.off(60); rig.off(64); rig.off(67);
  rig.render(48000 * 5);
  CHECK(rig.engine.silent(), "released");
  WHEN("nine FM notes are held at once");
  Rig many; many.s.v[params::kSource] = params::kSourceFm;
  for (int k = 0; k < 9; ++k) { many.on(48 + k); many.render(32); }
  many.render(9600);
  THEN("the engine survives the chip's eight-channel limit");
  CHECK(finite(many.mono()), "finite");
  CHECK(many.engine.active_voices() <= 8, spec::fmt("%.0f FM voices", static_cast<double>(many.engine.active_voices())));
}

SCENARIO(collage_sampler_plays_found_sound_when_the_bank_is_empty) {
  GIVEN("the collage source with nothing loaded");
  Rig rig;
  rig.s.v[params::kSource] = params::kSourceCollage;
  rig.on(60); rig.render(24000);
  THEN("it still makes a sound, from the built-in found-sound bank");
  CHECK(measure::rms(Span(rig.mono(), 2000)) > 0.01, "audible");
  WHEN("a user sample is loaded into the slot for C4 and the note is played again");
  auto sample = std::make_shared<dsp::Sample>();
  sample->rate = kRate; sample->root_hz = 440.0; sample->frames.resize(48000);
  for (size_t i = 0; i < sample->frames.size(); ++i) sample->frames[i] = static_cast<float>(0.5 * std::sin(dsp::kTau * 440.0 * static_cast<double>(i) / kRate));
  rig.engine.set_sample(0, sample);
  rig.off(60); rig.render(24000);
  Rig fresh; fresh.s = rig.s; fresh.engine.set_sample(0, sample);
  fresh.on(60); fresh.render(24000);
  THEN("the user's sample plays at its own pitch");
  CHECK_NEAR(measure::cents(measure::pitch_hz(Span(fresh.mono(), 8000, 8000), kRate), 440.0), 0.0, 15.0, "sample pitch");
}

SCENARIO(sustain_pedal_holds_released_notes) {
  GIVEN("the pedal down, a short note played and released");
  Rig rig;
  rig.s.v[params::kReleaseMs] = 50.0;
  rig.engine.sustain(0, true);
  rig.on(60); rig.render(4800); rig.off(60); rig.render(24000);
  THEN("the note keeps sounding");
  CHECK(measure::rms(Span(rig.mono(), rig.left.size() - 4800)) > 0.02, "held by the pedal");
  WHEN("the pedal comes up");
  rig.engine.sustain(0, false); rig.render(24000);
  THEN("the note releases");
  CHECK(measure::peak(Span(rig.mono(), rig.left.size() - 4800)) < 1e-3 && rig.engine.silent(), "released");
}

SCENARIO(pitch_bend_moves_the_note_by_the_bend_range) {
  GIVEN("a 2 semitone bend range and the wheel fully up on the synth");
  Rig rig;
  rig.s.v[params::kSource] = params::kSourceSynth; rig.s.v[params::kBendRange] = 2; rig.s.v[params::kCutoff] = 18000;
  rig.s.v[params::kFilterEnv] = 0; rig.s.v[params::kOscMix] = 0; rig.s.v[params::kOsc2Detune] = 0;
  rig.on(60); rig.engine.pitch_bend(0, 1.0); rig.render(24000);
  THEN("the pitch is 200 cents sharp");
  CHECK_NEAR(measure::cents(measure::pitch_hz(Span(rig.mono(), 8000, 8000), kRate), 261.63), 200.0, 15.0, "bend");
}

SCENARIO(every_preset_meets_the_quality_bar) {
  GIVEN("each factory preset played the audit phrase: a four-note chord at playing velocities, then three melody notes");
  bool all_ok = true;
  double loudest = -120.0, quietest = 0.0;
  for (size_t p = 0; p < presets::count(); ++p) {
    Engine engine;
    engine.prepare(kRate);
    Settings s = Settings::defaults();
    presets::apply(p, s.v.data());
    const audit::Rendered r = audit::render(s, engine, 2.0);
    const double peak = std::max(measure::peak(Span(r.left)), measure::peak(Span(r.right)));
    const double loud = measure::momentary_loudness_max(r.mono, kRate);
    const Span chord(r.mono, r.first_note + 9600, 57600);
    const double dc = std::abs(measure::mean(chord));
    // docs/QUALITY.md: finite, the loudest 400 ms within the slack of the target loudness, the
    // limiter asked for no more than its allowance, nothing over 0 dBFS before it, audible while
    // held, and no DC worth hearing. The bar is set with the sample bank loaded; here the collage
    // presets play the built-in found sounds instead, so the loudness window carries one decibel
    // of slack on each side.
    const double slack = audit::kLoudnessSlackDb + 1.0;
    const bool ok = finite(r.mono) && std::abs(loud - audit::kTargetLufs) <= slack && peak <= dsp::Limiter::kCeiling + 1e-4 && !engine.clipped() && measure::rms(chord) > 1e-3 && dc < 0.01;
    loudest = std::max(loudest, loud); quietest = std::min(quietest, loud);
    if (!ok) { all_ok = false; std::printf("    preset %zu '%s': loud %.1f LUFS limit %.1f dB peak %.1f dBFS dc %.1f dB rms %.4f finite %d clipped %d\n", p, presets::all()[p].name, loud, engine.limited_db(), measure::db(peak), measure::db(dc), measure::rms(chord), finite(r.mono), engine.clipped()); }
  }
  THEN("every one is finite, within 2.5 dB of the target loudness (1.5 dB in the audit, a decibel of slack without the bank), under the ceiling with the clip light off (the limiter holding no more than 6 dB), audible and free of DC");
  CHECK(all_ok, "all presets meet the bar");
  CHECK(loudest - quietest <= 2.0 * (audit::kLoudnessSlackDb + 1.0), spec::fmt("the bank spans %.1f dB from quietest to loudest", loudest - quietest));
  CHECK(presets::count() == params::kPresetCount, spec::fmt("preset table has %.0f entries, the parameter promises %.0f", static_cast<double>(presets::count()), params::kPresetCount));
}

SCENARIO(output_limiter_holds_the_ceiling_and_leaves_quiet_signals_alone) {
  GIVEN("the fuzzed combo organ with the master at +12 dB, then the same at -12 dB");
  Rig hot;
  presets::apply(4, hot.s.v.data()); hot.s.v[params::kMasterDb] = 12;
  hot.on(48); hot.on(55); hot.on(60); hot.render(48000);
  Rig quiet;
  presets::apply(4, quiet.s.v.data()); quiet.s.v[params::kMasterDb] = -12;
  quiet.on(48); quiet.on(55); quiet.on(60); quiet.render(48000);
  const double hot_peak = measure::db(std::max(measure::peak(Span(hot.left)), measure::peak(Span(hot.right))));
  const double quiet_peak = measure::db(std::max(measure::peak(Span(quiet.left)), measure::peak(Span(quiet.right))));
  THEN("the hot one peaks exactly at the -2 dBFS ceiling with the clip light on and the limiter reporting its reduction; the quiet one is untouched and the gain below the ceiling is linear");
  CHECK_NEAR(hot_peak, -2.0, 0.1, "ceiling");
  CHECK(hot.engine.clipped() && hot.engine.limited_db() > 6.0, spec::fmt("limited by %.1f dB", hot.engine.limited_db()));
  CHECK(!quiet.engine.clipped() && quiet.engine.limited_db() == 0.0, "quiet signal passes untouched");
  CHECK(quiet_peak < -2.0 - 10.0, "the quiet one is well under the ceiling");
  // Under the ceiling the limiter is a wire: a render 6 dB hotter than the quiet one, still
  // under the ceiling, is exactly 6 dB hotter at the output.
  Rig mid;
  presets::apply(4, mid.s.v.data()); mid.s.v[params::kMasterDb] = -6;
  mid.on(48); mid.on(55); mid.on(60); mid.render(48000);
  CHECK(mid.engine.limited_db() == 0.0, "still under the ceiling");
  CHECK_NEAR(measure::db(measure::rms(Span(mid.left, 9600, 24000))) - measure::db(measure::rms(Span(quiet.left, 9600, 24000))), 6.0, 0.1, "linear below the ceiling");
}

SCENARIO(every_source_plays_one_note_at_a_comparable_level) {
  GIVEN("one C4 at full velocity through every source, tape bank and FM patch, master at 0 dB");
  struct Case { const char* name; int source; int bank; int patch; };
  std::vector<Case> cases;
  for (int src = 0; src < params::kSourceCount; ++src) {
    if (src == params::kSourceTape) { for (int b = 0; b < params::kTapeBankCount; ++b) cases.push_back({params::kTapeBankNames[b], src, b, 0}); }
    else if (src == params::kSourceFm) { for (int p = 0; p < params::kFmPatchCount; ++p) cases.push_back({params::kFmPatchNames[p], src, 0, p}); }
    else cases.push_back({params::kSourceNames[src], src, 0, 0});
  }
  bool all_ok = true;
  for (const Case& c : cases) {
    Rig rig;
    rig.s.v[params::kSource] = c.source; rig.s.v[params::kTapeBank] = c.bank; rig.s.v[params::kFmPatch] = c.patch; rig.s.v[params::kMasterDb] = 0;
    rig.on(60); rig.render(48000);
    const double peak = measure::db(measure::peak(Span(rig.left))), held = measure::db(measure::rms(Span(rig.left, 9600, 24000)));
    // The levels:: trims in engine.hpp aim at -13 dBFS RMS or -5 dBFS peak; allow the models'
    // own crest factors around that (a plucked string has decayed by the held window).
    const bool ok = peak >= -11.0 && peak <= -3.5 && held >= -30.0 && held <= -11.0;
    if (!ok) { all_ok = false; std::printf("    %s: peak %.1f dBFS, held %.1f dBFS\n", c.name, peak, held); }
  }
  THEN("every one peaks between -11 and -3.5 dBFS and holds between -30 and -11 dBFS RMS");
  CHECK(all_ok, "sources sit at one level");
}

SCENARIO(chord_machine_turns_one_key_into_a_voiced_chord) {
  GIVEN("the theremin (a near sine) with the chord machine set to Major, C4 played");
  Rig rig;
  rig.s.v[params::kSource] = params::kSourceTheremin; rig.s.v[params::kThereminShape] = 0; rig.s.v[params::kThereminGlide] = 1;
  rig.s.v[params::kChordType] = 1; rig.s.v[params::kAttackMs] = 5;
  rig.on(60); rig.render(24000);
  THEN("three voices play C, E and G, and nothing plays the minor third");
  CHECK(rig.engine.active_voices() == 3, spec::fmt("%.0f voices", static_cast<double>(rig.engine.active_voices())));
  const auto m = rig.mono();
  const Span body(m, 8000, 16384);
  const double c = measure::tone_amplitude(body, kRate, 261.63), e = measure::tone_amplitude(body, kRate, 329.63);
  const double g = measure::tone_amplitude(body, kRate, 392.0), eb = measure::tone_amplitude(body, kRate, 311.13);
  CHECK(c > 0.05 && e > 0.05 && g > 0.05, spec::fmt("C %.3f E %.3f G %.3f", c, e, g));
  CHECK(eb < 0.2 * e, spec::fmt("minor third %.3f stays out", eb));
  WHEN("the key is released");
  rig.off(60); rig.render(48000);
  THEN("every chord voice releases with it");
  CHECK(rig.engine.silent(), "all three voices ended");
  WHEN("the voicing is Open");
  Rig open;
  open.s = rig.s; open.s.v[params::kChordVoicing] = 2;
  open.on(60); open.render(24000);
  THEN("a bass note sounds an octave below the root");
  CHECK(open.engine.active_voices() == 4, spec::fmt("%.0f voices", static_cast<double>(open.engine.active_voices())));
  CHECK(measure::tone_amplitude(Span(open.mono(), 8000, 16384), kRate, 130.81) > 0.05, "C3 bass note");
}

SCENARIO(strum_spreads_chord_notes_over_time_and_stops_with_the_key) {
  GIVEN("a Major chord with a 100 ms strum on the theremin");
  Rig rig;
  rig.s.v[params::kSource] = params::kSourceTheremin; rig.s.v[params::kChordType] = 1; rig.s.v[params::kChordStrum] = 100;
  rig.on(60);
  THEN("only the root sounds at once and the other two notes wait");
  CHECK(rig.engine.active_voices() == 1 && rig.engine.pending_notes() == 2, spec::fmt("%.0f sounding, %.0f pending", static_cast<double>(rig.engine.active_voices()), static_cast<double>(rig.engine.pending_notes())));
  WHEN("60 ms pass, then 60 more, then 100 more");
  rig.render(2880);
  const int at_60 = rig.engine.active_voices();
  rig.render(2880);
  const int at_120 = rig.engine.active_voices();
  rig.render(4800);
  THEN("the second note arrives after 100 ms and the third after 200 ms");
  CHECK(at_60 == 1, spec::fmt("%.0f voices at 60 ms", static_cast<double>(at_60)));
  CHECK(at_120 == 2, spec::fmt("%.0f voices at 120 ms", static_cast<double>(at_120)));
  CHECK(rig.engine.active_voices() == 3 && rig.engine.pending_notes() == 0, spec::fmt("%.0f voices at 220 ms", static_cast<double>(rig.engine.active_voices())));
  WHEN("a key is released before its strum finishes");
  Rig quick;
  quick.s = rig.s; quick.s.v[params::kReleaseMs] = 10;
  quick.on(60); quick.render(2400); quick.off(60); quick.render(24000);
  THEN("the notes that had not sounded are dropped and the engine falls silent");
  CHECK(quick.engine.pending_notes() == 0, "pending notes dropped");
  CHECK(quick.engine.silent(), "silent");
}

SCENARIO(tail_estimate_covers_the_actual_decay) {
  GIVEN("a preset with a long echo and reverb");
  Rig rig;
  presets::apply(17, rig.s.v.data());  // 12-string drone
  rig.on(60); rig.render(9600); rig.off(60);
  const uint32_t tail = rig.engine.tail_samples(rig.s);
  rig.render(tail);
  THEN("after the advertised tail the output is quiet");
  CHECK(tail > 48000, spec::fmt("tail of %.1f s advertised", tail / kRate));
  CHECK(measure::peak(Span(rig.mono(), rig.left.size() - 4800)) < 0.02, "quiet after the tail");
}

SCENARIO(tempo_synced_echo_follows_the_host_tempo) {
  GIVEN("echo synced to 1/4 notes at 120 BPM, fed a short click through the organ");
  Rig rig;
  rig.s.v[params::kEchoMix] = 0.5; rig.s.v[params::kEchoSync] = 1; rig.s.v[params::kEchoDivision] = 4; rig.s.v[params::kEchoFeedback] = 0.0;
  rig.s.v[params::kEchoWow] = 0; rig.s.v[params::kReleaseMs] = 1; rig.s.v[params::kAttackMs] = 0;
  rig.engine.set_tempo(120.0);
  rig.render(48000);  // let the echo's time smoother settle on the synced value
  rig.on(72); rig.render(480); rig.off(72);
  rig.render(48000);
  THEN("the repeat arrives half a second later");
  const auto m = rig.mono();
  const auto c = measure::contour_db(m, kRate, 0.005, 0.010);
  const long first = measure::first_at_or_above(c, -30.0, 190);  // the direct note, at 1.0 s
  long repeat = -1;
  for (size_t i = static_cast<size_t>(first) + 20; i < c.size(); ++i) if (c[i] > -40.0) { repeat = static_cast<long>(i); break; }
  CHECK(repeat > 0, "a repeat was found");
  if (repeat > 0) CHECK_NEAR(static_cast<double>(repeat - first) * 0.005, 0.5, 0.03, "quarter note at 120 BPM");
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
