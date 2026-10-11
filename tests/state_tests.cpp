// Executable scenarios for features/clap_plugin.feature (state): a project saves and reloads
// every parameter and every loaded sample, byte for byte and sound for sound, and refuses
// state it cannot trust.
#include "../src/params.hpp"
#include "../src/wav.hpp"
#include "audio_measure.hpp"
#include "clap_harness.hpp"
#include "platform_test.hpp"
#include "spec.hpp"

#include <filesystem>
#include <vector>

using namespace harness;
using measure::Span;
namespace P = fuzzboi::params;

namespace {
const char* g_path = nullptr;
constexpr double kRate = 48000.0;

std::vector<float> render_note(const clap_plugin_t* plugin, int blocks) {
  Runner runner(plugin, kRate, 256);
  Events on; on.push(note_event(CLAP_EVENT_NOTE_ON, 0, 60, -1, 1.0));
  std::vector<float> out;
  for (int b = 0; b < blocks; ++b) {
    runner.run(b == 0 ? &on : nullptr);
    for (uint32_t i = 0; i < runner.frames(); ++i) out.push_back(runner.left()[i]);
  }
  return out;
}
}  // namespace

SCENARIO(state_round_trips_every_parameter) {
  GIVEN("an instance with every parameter set away from its default");
  Library lib(g_path);
  const clap_plugin_t* a = lib.create();
  for (clap_id id = 0; id < P::kParamCount; ++id) {
    if (id == P::kPreset) continue;
    const auto& s = P::kSpecs[id];
    set_param(a, id, s.stepped ? (s.def == s.max ? s.min : s.max) : s.min + (s.max - s.min) * 0.37);
  }
  StateMemory saved;
  CHECK(save_state(a, &saved), "save succeeds");
  WHEN("the state is loaded into a fresh instance");
  const clap_plugin_t* b = lib.create();
  CHECK(load_state(b, saved), "load succeeds");
  THEN("every value matches and saving again gives identical bytes");
  bool same = true;
  for (clap_id id = 0; id < P::kParamCount; ++id) same = same && param(a, id) == param(b, id);
  CHECK(same, "values identical");
  StateMemory again;
  save_state(b, &again);
  CHECK(again.bytes == saved.bytes, "byte-identical re-save");
  a->destroy(a); b->destroy(b);
}

SCENARIO(state_round_trip_is_audio_invariant) {
  GIVEN("a preset rendered, saved, and loaded into a new instance");
  Library lib(g_path);
  const clap_plugin_t* a = lib.create();
  set_param(a, P::kPreset, 19.0);  // Fuzzed saw riff
  set_param(a, P::kFineCents, 13.0);
  const auto first = render_note(a, 30);
  StateMemory saved;
  save_state(a, &saved);
  const clap_plugin_t* b = lib.create();
  load_state(b, saved);
  const auto second = render_note(b, 30);
  THEN("the two renders are the same sound");
  CHECK(measure::correlation(Span(first, 1000), Span(second, 1000)) > 0.99, "audio identical after reload");
  a->destroy(a); b->destroy(b);
}

SCENARIO(samples_persist_in_state) {
  GIVEN("a WAV loaded into slot 0 through the environment, with the collage source");
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("fuzzboi-state-" + std::to_string(test_platform::process_id()));
  std::filesystem::create_directories(dir);
  const std::filesystem::path wav = dir / "tone_A4.wav";
  std::vector<float> tone(24000);
  for (size_t i = 0; i < tone.size(); ++i) tone[i] = static_cast<float>(0.5 * std::sin(6.283185307 * 440.0 * static_cast<double>(i) / kRate));
  CHECK(fuzzboi::wav::write_file(wav.string().c_str(), {tone}, kRate), "fixture written");
  test_platform::set_environment("FUZZBOI_SAMPLES", wav.string());
  Library lib(g_path);
  const clap_plugin_t* a = lib.create();
  test_platform::unset_environment("FUZZBOI_SAMPLES");
  set_param(a, P::kSource, P::kSourceCollage);
  set_param(a, P::kSampleRoot, 60.0);
  const auto first = render_note(a, 20);
  THEN("the sample plays at its root pitch from the file name");
  CHECK_NEAR(measure::cents(measure::pitch_hz(Span(first, 2000, 2000), kRate), 440.0), 0.0, 15.0, "A4 from the name");
  WHEN("the state is saved and loaded into an instance that never saw the file");
  StateMemory saved;
  save_state(a, &saved);
  const clap_plugin_t* b = lib.create();
  CHECK(load_state(b, saved), "load");
  const auto second = render_note(b, 20);
  THEN("it plays the same sample");
  CHECK(measure::correlation(Span(first, 1000), Span(second, 1000)) > 0.99, "sample restored");
  CHECK(saved.bytes.size() > 24000 * 2, "the sample's PCM is in the state");
  a->destroy(a); b->destroy(b);
  std::filesystem::remove_all(dir);
}

SCENARIO(malformed_state_is_rejected_and_leaves_the_instance_usable) {
  GIVEN("a valid state and several corruptions of it");
  Library lib(g_path);
  const clap_plugin_t* a = lib.create();
  set_param(a, P::kMasterDb, -12.0);
  StateMemory saved;
  save_state(a, &saved);
  const clap_plugin_t* b = lib.create();
  THEN("an empty stream, a truncated stream, a bad magic and a future version are all refused");
  CHECK(!load_state(b, StateMemory{}), "empty");
  CHECK(!load_state(b, slice(saved, 10)), "truncated header");
  CHECK(!load_state(b, slice(saved, saved.bytes.size() / 2)), "truncated values");
  StateMemory bad_magic = saved; bad_magic.bytes[0] ^= 0xff;
  CHECK(!load_state(b, bad_magic), "bad magic");
  StateMemory future = saved; future.bytes[4] = 0xff;
  CHECK(!load_state(b, future), "future version");
  WHEN("the good state is then loaded");
  CHECK(load_state(b, saved) && param(b, P::kMasterDb) == -12.0, "still usable");
  a->destroy(a); b->destroy(b);
}

SCENARIO(older_states_with_fewer_parameters_load_with_defaults) {
  GIVEN("a state whose header claims fewer parameters than this build has");
  Library lib(g_path);
  const clap_plugin_t* a = lib.create();
  set_param(a, P::kMasterDb, -9.0);
  StateMemory saved;
  save_state(a, &saved);
  // Header layout: magic, version, param_count, gui_w, gui_h, sample_count (6 x uint32), then doubles.
  const uint32_t fewer = 20;
  std::memcpy(saved.bytes.data() + 8, &fewer, 4);
  StateMemory shortened;
  shortened.bytes.assign(saved.bytes.begin(), saved.bytes.begin() + 24 + fewer * 8);
  const clap_plugin_t* b = lib.create();
  set_param(b, P::kFuzz, 0.7);
  THEN("it loads, the stored values apply and the rest return to defaults");
  CHECK(load_state(b, shortened), "loads");
  CHECK(param(b, P::kMasterDb) == -9.0, "stored value applied");
  CHECK(param(b, P::kFuzz) == P::kSpecs[P::kFuzz].def, "missing parameter reset to default");
  a->destroy(a); b->destroy(b);
}

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: state_tests <FUZZBOI.clap> [filter]\n"); return 2; }
  g_path = argv[1];
  return spec::run(argc, argv, 2);
}
