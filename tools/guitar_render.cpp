// Renders one note of the physical guitar straight from its model (no voice, no effects), for
// the guitar critic (tools/guitar_critic.py) to set beside a recording of the same string and
// fret on a real instrument. A review tool, not a test.
//
//   fuzzboi-guitar-render <out.wav> model=0..3 string=1..6|0 fret=N note=MIDI velocity=0..1
//                         pluck_mm=90 pick=0..1 pickup=0..3 seconds=4 tuning=0 age=0 mute=0
//                         seed=1 rate=48000
//
// `string` counts guitar-style (1 the high e, 6 the low E); 0 lets the fingering choose. With a
// string given, `fret` is used as is; otherwise `note` is fingered. Prints the string, fret and
// physical pitch it played.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "dsp/guitar.hpp"
#include "wav.hpp"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: fuzzboi-guitar-render <out.wav> key=value ...\n");
    return 2;
  }
  std::map<std::string, double> a{{"model", 0}, {"string", 0}, {"fret", 0}, {"note", 57}, {"velocity", 0.8},
                                   {"pluck_mm", 90}, {"pick", 0.6}, {"pickup", 0}, {"seconds", 4}, {"tuning", 0},
                                   {"age", 0}, {"mute", 0}, {"seed", 1}, {"rate", 48000}, {"slip", 1}};
  for (int i = 2; i < argc; ++i) {
    const char* eq = std::strchr(argv[i], '=');
    if (!eq) { std::fprintf(stderr, "bad argument %s\n", argv[i]); return 2; }
    const std::string key(argv[i], static_cast<size_t>(eq - argv[i]));
    if (!a.count(key)) { std::fprintf(stderr, "unknown key %s\n", key.c_str()); return 2; }
    a[key] = std::atof(eq + 1);
  }
  using namespace fuzzboi::dsp::guitar;
  const double rate = a["rate"];
  Guitar g;
  g.prepare(rate);
  Setup s;
  s.model = static_cast<int>(a["model"]);
  s.tuning = static_cast<int>(a["tuning"]);
  s.pluck_m = a["pluck_mm"] * 0.001;
  s.hardness = a["pick"];
  s.pickup = static_cast<int>(a["pickup"]);
  s.age = a["age"];
  s.mute = a["mute"];
  s.slip_scale = a["slip"];
  g.configure(s);
  int string = -1, fret = 0;
  if (a["string"] >= 1.0) {
    string = kStrings - static_cast<int>(a["string"]);
    fret = static_cast<int>(a["fret"]);
  } else {
    const auto f = g.finger(static_cast<int>(a["note"]));
    string = f.string;
    fret = f.fret;
  }
  g.pluck(string, fret, a["velocity"], static_cast<uint32_t>(a["seed"]));
  std::vector<float> out(static_cast<size_t>(a["seconds"] * rate));
  float peak = 0.0f;
  for (auto& x : out) { x = g.render(); peak = std::max(peak, std::abs(x)); }
  // Written as 16-bit PCM, so normalised to -1 dBFS to keep the decay's dynamic range; the
  // critic compares shapes, not levels.
  if (peak > 0.0f) for (auto& x : out) x *= 0.891f / peak;
  if (!fuzzboi::wav::write_file(argv[1], {out}, rate)) { std::fprintf(stderr, "cannot write %s\n", argv[1]); return 1; }
  std::printf("string=%d fret=%d hz=%.3f B=%.3e\n", kStrings - string, fret, g.pitch_at(string, fret), g.inharmonicity_at(string, fret));
  return 0;
}
