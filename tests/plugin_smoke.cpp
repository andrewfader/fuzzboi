// Executable scenarios for features/clap_plugin.feature (discovery and lifecycle): the built
// .clap is loaded the way a host loads it and driven only through the public CLAP ABI.
#include "clap_harness.hpp"
#include "spec.hpp"

#include <set>
#include <string>

using namespace harness;

namespace {
const char* g_path = nullptr;
}

SCENARIO(the_module_exports_a_compatible_entry_and_one_instrument) {
  GIVEN("the built plug-in module");
  Library lib(g_path);
  THEN("its descriptor names FUZZBOI as a stereo instrument");
  CHECK(std::string(lib.descriptor->id) == "org.fuzzboi.fuzzboi", "id");
  CHECK(std::string(lib.descriptor->name) == "FUZZBOI", "name");
  CHECK(lib.descriptor->version && *lib.descriptor->version, "version string");
  std::set<std::string> features;
  for (const char* const* f = lib.descriptor->features; f && *f; ++f) features.insert(*f);
  CHECK(features.count(CLAP_PLUGIN_FEATURE_INSTRUMENT) && features.count(CLAP_PLUGIN_FEATURE_STEREO), "features");
  CHECK(lib.entry->get_factory("not-a-factory") == nullptr, "unknown factory ids are refused");
  CHECK(lib.factory->create_plugin(lib.factory, &kHost, "org.someone.else") == nullptr, "unknown plug-in ids are refused");
}

SCENARIO(an_instance_offers_the_core_extensions_and_ports) {
  GIVEN("a created and initialised instance");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  THEN("audio out, note in, params, state, tail, latency and voice info are all there");
  for (const char* ext : {CLAP_EXT_AUDIO_PORTS, CLAP_EXT_NOTE_PORTS, CLAP_EXT_PARAMS, CLAP_EXT_STATE, CLAP_EXT_TAIL, CLAP_EXT_LATENCY, CLAP_EXT_VOICE_INFO})
    CHECK(plugin->get_extension(plugin, ext) != nullptr, std::string("extension ") + ext);
  CHECK(plugin->get_extension(plugin, "clap.not-real") == nullptr, "unknown extension");
  const auto* audio = static_cast<const clap_plugin_audio_ports_t*>(plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));
  clap_audio_port_info_t port{};
  CHECK(audio->count(plugin, false) == 1 && audio->count(plugin, true) == 0, "one output, no input");
  CHECK(audio->get(plugin, 0, false, &port) && port.channel_count == 2 && (port.flags & CLAP_AUDIO_PORT_IS_MAIN), "stereo main out");
  const auto* notes = static_cast<const clap_plugin_note_ports_t*>(plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS));
  clap_note_port_info_t note_port{};
  CHECK(notes->count(plugin, true) == 1 && notes->get(plugin, 0, true, &note_port), "one note input");
  CHECK((note_port.supported_dialects & CLAP_NOTE_DIALECT_CLAP) && (note_port.supported_dialects & CLAP_NOTE_DIALECT_MIDI), "CLAP and MIDI dialects");
  const auto* voices = static_cast<const clap_plugin_voice_info_t*>(plugin->get_extension(plugin, CLAP_EXT_VOICE_INFO));
  clap_voice_info_t vi{};
  CHECK(voices->get(plugin, &vi) && vi.voice_count == 16, "sixteen voices");
  plugin->destroy(plugin);
}

SCENARIO(parameters_are_consistent_with_their_declared_ranges) {
  GIVEN("the parameter list as the host sees it");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  const auto* params = params_of(plugin);
  const uint32_t count = params->count(plugin);
  THEN("every parameter has a unique id and name, a default inside its range, and formats and parses its default");
  std::set<std::string> names;
  std::set<clap_id> ids;
  bool ok = true;
  for (uint32_t i = 0; i < count; ++i) {
    clap_param_info_t info{};
    ok = ok && params->get_info(plugin, i, &info);
    ok = ok && info.min_value <= info.default_value && info.default_value <= info.max_value && info.min_value < info.max_value;
    ok = ok && names.insert(info.name).second && ids.insert(info.id).second;
    double value = 0.0;
    ok = ok && params->get_value(plugin, info.id, &value) && value == info.default_value;
    char text[128]{};
    ok = ok && params->value_to_text(plugin, info.id, info.default_value, text, sizeof(text)) && text[0] != '\0';
    double parsed = 0.0;
    const bool parses = params->text_to_value(plugin, info.id, text, &parsed);
    if (!parses || std::abs(parsed - info.default_value) > 1e-6 * std::max(1.0, std::abs(info.default_value)) + 1e-6) {
      std::printf("    '%s': default %g formats as '%s' which parses to %g (%s)\n", info.name, info.default_value, text, parsed, parses ? "ok" : "unparseable");
      ok = false;
    }
  }
  CHECK(count > 100, "a rich parameter set");
  CHECK(ok, "every parameter consistent");
  clap_param_info_t none{};
  CHECK(!params->get_info(plugin, count, &none), "out-of-range index refused");
  plugin->destroy(plugin);
}

SCENARIO(the_instance_activates_processes_silence_and_sleeps) {
  GIVEN("an activated instance with no notes");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  {
    Runner runner(plugin, 48000.0, 256);
    THEN("a block of silence comes out and the output is finite");
    const Block block = runner.run();
    CHECK(block.finite && block.peak == 0.0f, "silent and finite");
    CHECK(!plugin->activate(plugin, 1.0, 1, 1), "absurd sample rates are refused");
  }
  CHECK(plugin->activate(plugin, 44100.0, 1, 512), "re-activation at another rate");
  plugin->deactivate(plugin);
  plugin->destroy(plugin);
}

SCENARIO(two_instances_are_independent) {
  GIVEN("two instances from one module");
  Library lib(g_path);
  const clap_plugin_t* a = lib.create();
  const clap_plugin_t* b = lib.create();
  set_param(a, 1, -20.0);  // Master
  double va = 0.0, vb = 0.0;
  params_of(a)->get_value(a, 1, &va);
  params_of(b)->get_value(b, 1, &vb);
  THEN("a change in one does not touch the other");
  CHECK(va == -20.0 && vb != -20.0, "independent parameters");
  a->destroy(a);
  b->destroy(b);
}

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: plugin_smoke <FUZZBOI.clap> [filter]\n"); return 2; }
  g_path = argv[1];
  return spec::run(argc, argv, 2);
}
