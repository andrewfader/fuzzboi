// Tries a preset against a group's stem with some parameters changed, without rebuilding the
// preset table: plays the phrase, measures it as the stem gate does (tests/stem_gate.hpp) and
// prints the verdict; with wav=PATH also writes what it measured, for song-corpus/scripts/features.py
// to measure beside it. A tuning tool, not a test.
//
//   fuzzboi-stem-gate-try "Preset" GROUP STEM PHRASE [Param=value ...] [wav=out.wav]
//   (PHRASE: strum power arpeggio lead fingerpick drone bassline melody air pad)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "stem_gate.hpp"
#include "wav.hpp"

int main(int argc, char** argv) {
  using namespace stem_gate;
  if (argc < 5) { std::fprintf(stderr, "usage: fuzzboi-stem-gate-try PRESET GROUP STEM PHRASE [Param=value ...] [wav=out.wav]\n"); return 2; }
  const long index = tone_gate::preset_index(argv[1]);
  const stem::Reference* ref = reference(argv[2], argv[3]);
  static const char* const kPhrases[] = {"strum", "power", "arpeggio", "lead", "fingerpick", "drone", "bassline", "melody", "air", "pad"};
  int phrase = -1;
  for (int i = 0; i < 10; ++i) if (std::strcmp(argv[4], kPhrases[i]) == 0) phrase = i;
  if (index < 0 || !ref || phrase < 0) { std::fprintf(stderr, "unknown preset, group/stem or phrase\n"); return 2; }
  auto s = fuzzboi::Settings::defaults();
  fuzzboi::presets::apply(static_cast<size_t>(index), s.v.data());
  const char* wav = nullptr;
  for (int i = 5; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg.rfind("wav=", 0) == 0) { wav = argv[i] + 4; continue; }
    const size_t eq = arg.find('=');
    bool found = false;
    for (uint32_t id = 0; id < fuzzboi::params::kParamCount && eq != std::string::npos; ++id)
      if (arg.substr(0, eq) == fuzzboi::params::kSpecs[id].name) { s.v[id] = fuzzboi::params::clamp_to_spec(id, std::atof(arg.c_str() + eq + 1)); found = true; }
    if (!found) { std::fprintf(stderr, "unknown parameter in %s\n", argv[i]); return 2; }
  }
  const auto audio = render_case(s, static_cast<tone::Phrase>(phrase));
  if (wav && !fuzzboi::wav::write_file(wav, {audio.left, audio.right}, tone_gate::kRate)) { std::perror(wav); return 1; }
  const stem::Features f = stem::measure(audio.left, audio.right, tone_gate::kRate);
  const std::string failures = judge(f, *ref);
  std::printf("%s as %s %s: cent %.0f [%.0f..%.0f] roll %.0f [%.0f..%.0f] hf %.1f low %.1f crest %.1f corr %.2f [%.2f..%.2f] wobble %.2f [%.2f..%.2f] (%d runs)\n",
              argv[1], ref->group, ref->stem, f.centroid_hz, ref->centroid_hz.q25, ref->centroid_hz.q75, f.rolloff85_hz, ref->rolloff85_hz.q25,
              ref->rolloff85_hz.q75, f.hf_share_db, f.low_share_db, f.crest100_db, f.corr, ref->corr.q25, ref->corr.q75, f.wobble_cents,
              ref->wobble_cents.q25, ref->wobble_cents.q75, f.wobble_runs);
  std::printf("  %s\nLOSS %.4f\n", failures.empty() ? "PASS" : failures.c_str(), loss(f, *ref));
  return failures.empty() ? 0 : 1;
}
