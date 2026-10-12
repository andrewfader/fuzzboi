// Executable scenarios for features/clap_plugin.feature (notes and automation): sound comes out
// when and only when notes say so, through both the CLAP and MIDI dialects, with sample-accurate
// event timing and automation.
#include "../src/params.hpp"
#include "audio_measure.hpp"
#include "clap_harness.hpp"
#include "spec.hpp"

#include <vector>

using namespace harness;
using measure::Span;
namespace P = fuzzboi::params;

namespace {
const char* g_path = nullptr;
constexpr double kRate = 48000.0;

// Renders `blocks` blocks collecting the mono output.
std::vector<float> collect(Runner& runner, int blocks, Events* events = nullptr) {
  std::vector<float> out;
  for (int b = 0; b < blocks; ++b) {
    runner.run(b == 0 ? events : nullptr);
    for (uint32_t i = 0; i < runner.frames(); ++i) out.push_back(0.5f * (runner.left()[i] + runner.right()[i]));
  }
  return out;
}
}  // namespace

SCENARIO(a_clap_note_on_makes_sound_and_note_off_stops_it) {
  GIVEN("an instance with a 50 ms release, given a CLAP note on");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  set_param(plugin, P::kReleaseMs, 50.0);
  Runner runner(plugin, kRate, 256);
  Events on; on.push(note_event(CLAP_EVENT_NOTE_ON, 0, 60, 7, 0.9));
  const auto held = collect(runner, 40, &on);
  THEN("audio appears");
  CHECK(measure::rms(Span(held, 2000)) > 0.02, "sound while held");
  WHEN("the note is released by id");
  Events off; off.push(note_event(CLAP_EVENT_NOTE_OFF, 0, 60, 7, 0.0));
  const auto tail = collect(runner, 40, &off);
  THEN("it falls silent and the plug-in asks to sleep");
  CHECK(measure::peak(Span(tail, 6000)) < 1e-3, "silent after the release");
  CHECK(runner.run().peak < 1e-4f, "stays silent");
  plugin->destroy(plugin);
}

SCENARIO(midi_notes_controllers_and_bend_are_honoured) {
  GIVEN("MIDI note on, then pitch bend up, then note off");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  set_param(plugin, P::kSource, P::kSourceSynth);
  set_param(plugin, P::kCutoff, 18000.0); set_param(plugin, P::kFilterEnv, 0.0); set_param(plugin, P::kOscMix, 0.0); set_param(plugin, P::kOsc2Detune, 0.0);
  set_param(plugin, P::kReleaseMs, 30.0); set_param(plugin, P::kBendRange, 12.0);
  Runner runner(plugin, kRate, 256);
  Events on; on.push(midi_event(0x90, 60, 100));
  const auto plain = collect(runner, 60, &on);
  Events bend; bend.push(midi_event(0xe0, 0x7f, 0x7f));
  const auto bent = collect(runner, 60, &bend);
  THEN("the pitch is C4, then an octave up with the wheel fully up");
  CHECK_NEAR(measure::cents(measure::pitch_hz(Span(plain, 4000, 8000), kRate), 261.63), 0.0, 15.0, "C4");
  CHECK_NEAR(measure::cents(measure::pitch_hz(Span(bent, 6000, 8000), kRate), 523.25), 0.0, 20.0, "bent up an octave");
  WHEN("the note is released while the sustain pedal is down, then the pedal comes up");
  Events pedal; pedal.push(midi_event(0xb0, 64, 127)); pedal.push(midi_event(0x80, 60, 0));
  const auto sustained = collect(runner, 40, &pedal);
  CHECK(measure::rms(Span(sustained, 6000)) > 0.02, "sustain pedal holds the note");
  Events up; up.push(midi_event(0xb0, 64, 0));
  const auto released = collect(runner, 40, &up);
  THEN("the note ends only when the pedal is released");
  CHECK(measure::peak(Span(released, 6000)) < 1e-3, "released with the pedal");
  WHEN("all notes off (CC 123) arrives during a chord");
  Events chord; chord.push(midi_event(0x90, 60, 100)); chord.push(midi_event(0x90, 64, 100)); chord.push(midi_event(0xb0, 123, 0));
  const auto silenced = collect(runner, 40, &chord);
  THEN("nothing is left sounding");
  CHECK(measure::peak(Span(silenced, 6000)) < 1e-3, "all notes off");
  plugin->destroy(plugin);
}

SCENARIO(events_are_applied_at_their_sample_offset) {
  GIVEN("a note on scheduled 200 samples into a 256-sample block with an instant attack");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  set_param(plugin, P::kAttackMs, 0.0); set_param(plugin, P::kOrganClick, 0.0);
  Runner runner(plugin, kRate, 256);
  Events events;
  auto on = note_event(CLAP_EVENT_NOTE_ON, 0, 72, -1, 1.0);
  on.header.time = 200;
  events.push(on);
  runner.run(&events);
  THEN("the first 200 samples are silent and sound starts right after");
  bool silent_before = true;
  for (uint32_t i = 0; i < 200; ++i) silent_before = silent_before && runner.left()[i] == 0.0f;
  bool sound_after = false;
  for (uint32_t i = 200; i < 256; ++i) sound_after = sound_after || std::abs(runner.left()[i]) > 1e-4f;
  CHECK(silent_before, "silent before the event");
  CHECK(sound_after, "sound after the event");
  plugin->destroy(plugin);
}

SCENARIO(parameter_automation_changes_the_sound_and_flush_works_offline) {
  GIVEN("the master level set by a parameter event inside process, then by flush");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  Runner runner(plugin, kRate, 256);
  Events on; on.push(note_event(CLAP_EVENT_NOTE_ON, 0, 60, -1, 1.0)); on.push(param_event(P::kMasterDb, 0.0));
  const auto loud = collect(runner, 30, &on);
  Events quieter; quieter.push(param_event(P::kMasterDb, -24.0));
  const auto quiet = collect(runner, 30, &quieter);
  THEN("the level drops by about 24 dB");
  const double drop = measure::db(measure::rms(Span(loud, 4000))) - measure::db(measure::rms(Span(quiet, 4000)));
  CHECK_NEAR(drop, 24.0, 2.0, "automation applied");
  CHECK(param(plugin, P::kMasterDb) == -24.0, "value readable through the params extension");
  WHEN("a parameter arrives through flush");
  set_param(plugin, P::kMasterDb, -6.0);
  THEN("the plug-in reports it");
  CHECK(param(plugin, P::kMasterDb) == -6.0, "flush applied");
  WHEN("an out-of-range value arrives");
  set_param(plugin, P::kMasterDb, 500.0);
  THEN("it is clamped to the declared range");
  CHECK(param(plugin, P::kMasterDb) == P::spec(P::kMasterDb).max, "clamped");
  plugin->destroy(plugin);
}

SCENARIO(choosing_a_preset_rewrites_the_other_parameters) {
  GIVEN("the preset parameter set to 'Electric jug'");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  double index = -1.0;
  CHECK(params_of(plugin)->text_to_value(plugin, P::kPreset, "Electric jug", &index), "preset found by name");
  set_param(plugin, P::kPreset, index);
  THEN("the source becomes the jug and the preset's effects are set");
  CHECK(param(plugin, P::kSource) == static_cast<double>(P::kSourceJug), "source switched");
  CHECK(param(plugin, P::kSpringMix) > 0.0, "preset effect applied");
  char text[64]{};
  params_of(plugin)->value_to_text(plugin, P::kPreset, index, text, sizeof(text));
  CHECK(std::string(text) == "Electric jug", "preset name formats back");
  WHEN("a parameter is edited afterwards and the same preset index is sent again");
  set_param(plugin, P::kSpringMix, 0.9);
  set_param(plugin, P::kPreset, index);
  THEN("the edit survives (re-sending the current preset is not a reload)");
  CHECK(param(plugin, P::kSpringMix) == 0.9, "edit kept");
  plugin->destroy(plugin);
}

SCENARIO(every_source_sounds_through_the_plugin) {
  GIVEN("each sound source selected by parameter, with one note");
  Library lib(g_path);
  bool all = true;
  for (int source = 0; source < P::kSourceCount; ++source) {
    const clap_plugin_t* plugin = lib.create();
    set_param(plugin, P::kSource, source);
    Runner runner(plugin, kRate, 256);
    Events on; on.push(note_event(CLAP_EVENT_NOTE_ON, 0, 60, -1, 1.0));
    const auto out = collect(runner, 40, &on);
    bool finite = true;
    for (float v : out) finite = finite && std::isfinite(v);
    const double rms = measure::rms(Span(out, 1000));
    if (!finite || rms < 1e-3) { all = false; std::printf("    source %d (%s): rms %.5f finite %d\n", source, P::kSourceNames[source], rms, finite); }
    plugin->destroy(plugin);
  }
  THEN("all thirteen are audible and finite");
  CHECK(all, "every source sounds");
}

SCENARIO(tail_reports_longer_with_echo_and_reverb) {
  GIVEN("the tail extension with and without a long echo");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  const auto* tail = static_cast<const clap_plugin_tail_t*>(plugin->get_extension(plugin, CLAP_EXT_TAIL));
  Runner runner(plugin, kRate, 256);
  runner.run();
  const uint32_t dry = tail->get(plugin);
  set_param(plugin, P::kEchoMix, 0.5); set_param(plugin, P::kEchoFeedback, 0.8); set_param(plugin, P::kVerbMix, 0.5);
  runner.run();
  const uint32_t wet = tail->get(plugin);
  THEN("the wet tail is longer and both are sane");
  CHECK(dry > 0 && dry < 48000 * 10, "dry tail is the release plus a margin");
  CHECK(wet > dry * 2, spec::fmt("wet tail %.1f s exceeds dry %.1f s", wet / kRate, dry / kRate));
  plugin->destroy(plugin);
}

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: note_tests <FUZZBOI.clap> [filter]\n"); return 2; }
  g_path = argv[1];
  return spec::run(argc, argv, 2);
}
