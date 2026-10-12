// Renders every factory preset with the same short phrase and measures it against the quality
// bar in docs/QUALITY.md, so a preset round can be judged from audio rather than from the
// settings. Writes one stereo WAV per preset for listening and spectrograms, and prints one
// line per preset: peak, momentary loudness, limiter reduction, held level, DC, idle noise,
// brightness, stereo width, tail, and the master trim still wanted to reach the target loudness.
//
//   fuzzboi-preset-audit OUTPUT_DIR [SAMPLES_DIR] [PRESET_FILTER]
//   fuzzboi-preset-audit --levels
//
// `--levels` prints the per-source table behind the trims in engine.hpp (levels::): one C4 at
// full velocity through every source, tape bank and FM patch, with the trim each would still
// need to reach -13 dBFS RMS held or -5 dBFS peak, whichever comes first.
//
// The phrase (tests/audit_phrase.hpp, shared with engine_tests): a bar of a held C major chord
// with a bass note, then three melody notes, then silence long enough for the tail, at 120 BPM
// so tempo-synced echoes land on the grid. The engine is driven directly (no CLAP).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "../src/engine.hpp"
#include "../src/presets.hpp"
#include "../src/wav.hpp"
#include "../tests/audio_measure.hpp"
#include "../tests/audit_phrase.hpp"

namespace {

using namespace fuzzboi;
using measure::Span;
using audit::kRate;
using audit::kBlock;

using audit::Rendered;
using audit::render;

bool finite(const std::vector<float>& x) { for (float v : x) if (!std::isfinite(v)) return false; return true; }

std::string slug(const char* name) {
  std::string out;
  for (const char* c = name; *c; ++c) out.push_back(std::isalnum(static_cast<unsigned char>(*c)) ? static_cast<char>(std::tolower(static_cast<unsigned char>(*c))) : '_');
  return out;
}

void load_bank(Engine& engine, const std::string& dir) {
  std::vector<std::string> files;
  for (const auto& entry : std::filesystem::directory_iterator(dir))
    if (entry.is_regular_file() && entry.path().extension() == ".wav") files.push_back(entry.path().string());
  std::sort(files.begin(), files.end());
  for (size_t i = 0; i < files.size() && i < static_cast<size_t>(kSampleSlots); ++i) {
    wav::Audio audio;
    if (!wav::read_file(files[i].c_str(), audio) || audio.mono.empty()) continue;
    auto sample = std::make_shared<dsp::Sample>();
    sample->frames = std::move(audio.mono);
    sample->rate = audio.rate;
    sample->name = std::filesystem::path(files[i]).stem().string();
    engine.set_sample(static_cast<int>(i), sample);
  }
}

// One C4 at full velocity, default settings, master at 0 dB: what the level trims are set from.
void report_level(const char* what, Settings s) {
  Engine engine;
  engine.prepare(kRate);
  s.v[params::kMasterDb] = 0;
  std::vector<float> l(48000), r(48000);
  engine.note_on(s, 0, 60, -1, 1.0);
  for (size_t b = 0; b < l.size() / kBlock; ++b) engine.render(s, l.data() + b * kBlock, r.data() + b * kBlock, kBlock);
  const double peak = measure::db(measure::peak(Span(l))), rms = measure::db(measure::rms(Span(l, 9600, 24000)));
  std::printf("%-22s peak %6.1f dBFS  held rms %6.1f dBFS  trim still wanted %+5.1f dB\n", what, peak, rms, std::min(-13.0 - rms, -5.0 - peak));
}

int print_levels() {
  for (int src = 0; src < params::kSourceCount; ++src) {
    if (src == params::kSourceFm || src == params::kSourceTape || src == params::kSourceGuitar) continue;
    Settings s = Settings::defaults(); s.v[params::kSource] = src; report_level(params::kSourceNames[src], s);
  }
  for (int bank = 0; bank < params::kTapeBankCount; ++bank) { Settings s = Settings::defaults(); s.v[params::kSource] = params::kSourceTape; s.v[params::kTapeBank] = bank; report_level(params::kTapeBankNames[bank], s); }
  for (int patch = 0; patch < params::kFmPatchCount; ++patch) { Settings s = Settings::defaults(); s.v[params::kSource] = params::kSourceFm; s.v[params::kFmPatch] = patch; report_level(params::kFmPatchNames[patch], s); }
  for (int model = 0; model < params::kGuitarModelCount; ++model) { Settings s = Settings::defaults(); s.v[params::kSource] = params::kSourceGuitar; s.v[params::kGuitarModel] = model; report_level(params::kGuitarModelNames[model], s); }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && std::strcmp(argv[1], "--levels") == 0) return print_levels();
  if (argc < 2) { std::fprintf(stderr, "usage: fuzzboi-preset-audit OUTPUT_DIR [SAMPLES_DIR] [PRESET_FILTER]\n"); return 2; }
  const std::string out_dir = argv[1];
  const std::string samples_dir = argc >= 3 ? argv[2] : "";
  const std::string filter = argc >= 4 ? argv[3] : "";
  std::filesystem::create_directories(out_dir);
  std::printf("%-3s %-30s %6s %6s %6s %6s %6s %6s %6s %6s %7s %5s %6s %5s %s\n", "#", "preset", "peak", "loud", "limit", "held", "melody", "hold", "dc", "idle", "bright", "width", "tail", "want", "flags");
  int failures = 0;
  for (size_t p = 0; p < presets::count(); ++p) {
    const char* name = presets::all()[p].name;
    if (!filter.empty() && std::string(name).find(filter) == std::string::npos) continue;
    Engine engine;
    engine.prepare(kRate);
    if (!samples_dir.empty()) load_bank(engine, samples_dir);
    Settings s = Settings::defaults();
    presets::apply(p, s.v.data());
    const Rendered r = render(s, engine);
    const double peak = measure::db(std::max(measure::peak(Span(r.left)), measure::peak(Span(r.right))));
    // The level presets are matched on: the loudest 400 ms of the phrase, K-weighted.
    const double loud = measure::momentary_loudness_max(r.mono, kRate);
    const double limit = engine.limited_db();
    // The held chord after its attack settles: sustaining sources must still be there.
    const Span chord(r.mono, r.first_note + 9600, static_cast<size_t>(1.2 * kRate));
    const double held = measure::db(measure::rms(chord));
    const double dc = measure::db(std::abs(measure::mean(chord)));
    // Idle: before the first note, stopping short of the block the note lands in.
    const double idle = measure::db(measure::rms(Span(r.mono, 2400, r.first_note - 2400 - 512)));
    // Melody notes relative to the chord, and how much of the chord is left after a second.
    const double melody = measure::db(measure::peak(Span(r.mono, static_cast<size_t>(2.1 * kRate), static_cast<size_t>(1.0 * kRate))));
    const double early = measure::db(measure::rms(Span(r.mono, r.first_note + 2400, 4800)));
    const double later = measure::db(measure::rms(Span(r.mono, r.first_note + 48000, 4800)));
    const double hold = later - early;
    const double bright = measure::spectral_centroid(chord, kRate, 12);
    const double width = 1.0 - measure::correlation(Span(r.left, r.first_note + 9600, 48000), Span(r.right, r.first_note + 9600, 48000));
    const double tail = measure::db(measure::peak(Span(r.mono, r.mono.size() - 4800)));
    const double want = audit::kTargetLufs - loud;
    std::string flags;
    if (!finite(r.left) || !finite(r.right)) flags += " NONFINITE";
    if (peak > -1.5) flags += " HOT";
    if (loud > audit::kTargetLufs + audit::kLoudnessSlackDb) flags += " LOUD";
    if (loud < audit::kTargetLufs - audit::kLoudnessSlackDb) flags += " QUIET";
    if (melody < peak - 14.0) flags += " MELODYLOST";
    if (dc > -40.0) flags += " DC";
    if (idle > -55.0) flags += " NOISY";
    if (tail > -50.0) flags += " TAIL";
    if (engine.clipped()) flags += " CLIP";
    if (!flags.empty()) ++failures;
    std::printf("%-3zu %-30s %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %7.0f %5.2f %6.1f %+5.1f%s\n", p, name, peak, loud, limit, held, melody, hold, dc, idle, bright, width, tail, want, flags.c_str());
    char file[64];
    std::snprintf(file, sizeof(file), "%02zu_%s.wav", p, slug(name).c_str());
    wav::write_file((out_dir + "/" + file).c_str(), {r.left, r.right}, kRate);
  }
  std::printf("%d preset(s) flagged\n", failures);
  return 0;
}
