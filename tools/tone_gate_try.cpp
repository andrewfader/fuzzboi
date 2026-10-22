// Tries a quality-gate preset with some parameters changed, without rebuilding the preset table:
// plays its phrase, measures it as the gate does and prints the verdict and the band deviations.
// A tuning tool, not a test.
//
//   fuzzboi-tone-gate-try "Preset name" [Param=value ...]   (parameter names as the editor shows them)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "tone_gate.hpp"

int main(int argc, char** argv) {
  using namespace tone_gate;
  if (argc < 2) { std::fprintf(stderr, "usage: fuzzboi-tone-gate-try PRESET [Param=value ...]\n"); return 2; }
  const Case* c = nullptr;
  for (const Case& k : cases()) if (std::strcmp(k.preset, argv[1]) == 0) c = &k;
  const long index = preset_index(argv[1]);
  if (!c || index < 0) { std::fprintf(stderr, "no gated preset %s\n", argv[1]); return 2; }
  auto s = fuzzboi::Settings::defaults();
  fuzzboi::presets::apply(static_cast<size_t>(index), s.v.data());
  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    const size_t eq = arg.find('=');
    bool found = false;
    for (uint32_t id = 0; id < fuzzboi::params::kParamCount && eq != std::string::npos; ++id)
      if (arg.substr(0, eq) == fuzzboi::params::kSpecs[id].name) { s.v[id] = fuzzboi::params::clamp_to_spec(id, std::atof(arg.c_str() + eq + 1)); found = true; }
    if (!found) { std::fprintf(stderr, "unknown parameter in %s\n", argv[i]); return 2; }
  }
  const tone::Reference* ref = reference(c->group);
  const tone::Features f = measure_case(s, c->phrase, ref->stem_rms_dbfs);
  const Verdict v = judge(f, *ref, *c);
  std::printf("%s vs %s: cent %.0f (%.0f) roll %.0f (%.0f) hf %.1f (%.1f) flat %.1f (%.1f) crest %.1f (%.1f) corr %.2f (%.2f) shape %.1f\n",
              c->preset, c->group, f.centroid_hz, ref->centroid_hz.median, f.rolloff85_hz, ref->rolloff85_hz.median, f.hf_share_db,
              ref->hf_share_db.median, f.flatness_db, ref->flatness_db.median, f.crest100_db, ref->crest100_db.median, f.stereo_corr,
              ref->stereo_corr.median, v.shape_db);
  std::printf("  bands - records:");
  for (int b = 2; b <= 21; ++b) std::printf(" %+.0f", f.band_db[static_cast<size_t>(b)] - ref->band_db[static_cast<size_t>(b)]);
  std::printf("\n  %s\nLOSS %.4f\n", v.failures.empty() ? "PASS" : v.failures.c_str(), loss(f, *ref, v.shape_db));
  return v.failures.empty() ? 0 : 1;
}
