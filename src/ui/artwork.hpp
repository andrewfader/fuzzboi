// The per-page artwork above the cards: the liquid-light scope, the drawbars, the filter
// response, the FM routing, the sample slots, the fuzz transfer curve, the echo heads and the
// tape reels. Drawn from parameter values plus a LiveState snapshot, so the plug-in and the
// offscreen preview tool render exactly the same pictures.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../dsp/core.hpp"
#include "../dsp/organ.hpp"
#include "../params.hpp"
#include "editor.hpp"
#include "layout.hpp"

namespace fuzzboi::ui {

// How much of the output the plug-in keeps for the scope and the visualizer: every sample, so
// the scope shows the real waveform rather than an aliased decimation of it.
constexpr size_t kScopeSamples = 4096;

// What the artwork needs from the running instrument.
struct LiveState {
  std::array<float, kScopeSamples> scope{};  // recent output samples, oldest first
  double sample_rate = 48000.0;
  float energy = 0.0f;              // RMS of the scope
  double phase = 0.0;               // advances while audio runs; drives the lava and the reels
  std::array<bool, 16> slot_loaded{};
  std::string first_slot_name;      // name, root and length of the first loaded slot
  double first_slot_root_hz = 0.0, first_slot_seconds = 0.0;
  int first_slot = -1;
};

// The scope's picture of the output: 20 ms starting at the latest rising zero crossing, so a
// steady note holds still, amplified so its peak fills most of the frame. The gain is limited
// to +24 dB, so silence and hiss stay flat, and is shown beside the trace.
struct ScopeTrace {
  std::vector<float> points;  // -1 .. 1, oldest first
  double gain = 1.0;
};

inline ScopeTrace scope_trace(const std::array<float, kScopeSamples>& s, double sample_rate, int count) {
  ScopeTrace out;
  float peak = 0.0f;
  for (float x : s) peak = std::max(peak, std::abs(x));
  out.gain = peak > 1e-4f ? std::clamp(0.9 / peak, 1.0, 16.0) : 1.0;
  const size_t n = s.size();
  const size_t window = std::clamp<size_t>(static_cast<size_t>(sample_rate * 0.02), 64, n / 2);
  size_t start = n - window;
  for (size_t i = n - window; i > n - window - n / 2 && i > 0; --i)
    if (s[i - 1] < 0.0f && s[i] >= 0.0f) { start = i; break; }
  out.points.resize(static_cast<size_t>(std::max(2, count)));
  for (size_t k = 0; k < out.points.size(); ++k) {
    const double pos = static_cast<double>(start) + static_cast<double>(k) * static_cast<double>(window - 1) / static_cast<double>(out.points.size() - 1);
    const size_t i = std::min(static_cast<size_t>(pos), n - 2);
    const double frac = pos - static_cast<double>(i);
    out.points[k] = static_cast<float>(std::clamp((s[i] + (s[i + 1] - s[i]) * frac) * out.gain, -1.0, 1.0));
  }
  return out;
}

namespace strip {
inline Rect slot_tile(const Rect& area, int slot) { return {area.x + 20 + slot * 56, area.y + 44, 50, 52}; }
}  // namespace strip

class Artwork {
 public:
  Artwork(const EditorHost& host, const LiveState& live) : host_(host), live_(live) {}

  void draw(int page, Painter& g, const Rect& r) const {
    switch (page) {
      case 0: draw_liquid_light(g, r); break;
      case 1: draw_drawbars(g, r); break;
      case 2: draw_filter(g, r); break;
      case 3: draw_fm_routing(g, r); break;
      case 4: draw_slots(g, r); break;
      case 6: draw_fuzz_curve(g, r); break;
      case 7: draw_echo_heads(g, r); break;
      case 8: draw_reels(g, r); break;
      default: break;
    }
  }

 private:
  double value(params::ParamId id) const { return host_.value(static_cast<int>(id)); }
  int source() const { return static_cast<int>(value(params::kSource)); }

  // The liquid light show: overlapping discs of oil colour that drift with the audio's energy,
  // under the output scope. The phase only advances while there is sound, so the picture
  // settles when the instrument is quiet.
  void draw_liquid_light(Painter& g, const Rect& r) const {
    using namespace theme;
    static constexpr uint32_t kOils[] = {0x8a1c6a, 0xb8450f, 0x4f1f8f, 0x12605a, 0x9a7a10};
    const double energy = live_.energy;
    for (int i = 0; i < 9; ++i) {
      const double t = live_.phase * (0.6 + 0.11 * i) + i * 1.7;
      const int d = 70 + static_cast<int>(40.0 * std::sin(t * 0.7 + i)) + static_cast<int>(90.0 * energy);
      const int cx = r.x + 40 + static_cast<int>((r.w - 120) * (0.5 + 0.5 * std::sin(t * 0.37 + i * 0.9)));
      const int cy = r.y + 10 + static_cast<int>((r.h - 20 - d) * (0.5 + 0.5 * std::cos(t * 0.29 + i * 1.3)));
      g.circle(cx, cy, d, mix(panel, kOils[i % 5], 0.55 + 0.4 * energy));
    }
    g.text(r.x + 20, r.y + 24, "OUTPUT SCOPE", soft, 200, TextSize::Small);
    const int mid = r.y + r.h / 2 + 8, amp = r.h / 2 - 24;
    const ScopeTrace trace = scope_trace(live_.scope, live_.sample_rate, 384);
    std::vector<Point> scope;
    const int last = static_cast<int>(trace.points.size()) - 1;
    for (int i = 0; i <= last; ++i)
      scope.push_back({r.x + 20 + i * (r.w - 40) / last, mid - static_cast<int>(trace.points[static_cast<size_t>(i)] * static_cast<float>(amp))});
    g.polyline(scope, text, 3);
    char gain[48]{};
    std::snprintf(gain, sizeof(gain), "\xc3\x97%.1f  (+%.0f dB)", trace.gain, 20.0 * std::log10(trace.gain));
    g.text(r.x + 200, r.y + 24, gain, muted, 220, TextSize::Small);
    char name[96]{};
    params::format_value(params::kSource, value(params::kSource), name, sizeof(name));
    g.text_right(r.right() - 20, r.y + 24, name, amber, 400, TextSize::Normal);
  }

  void draw_drawbars(Painter& g, const Rect& r) const {
    using namespace theme;
    g.text(r.x + 20, r.y + 24, "FOOTAGES", muted, 200, TextSize::Small);
    static constexpr const char* kNames[6] = {"16'", "8'", "4'", "2 2/3'", "2'", "1'"};
    const bool relevant = source() == params::kSourceOrgan;
    for (int i = 0; i < 6; ++i) {
      const int x = r.x + 40 + i * 70, bottom = r.bottom() - 18, h = r.h - 60;
      const int level = static_cast<int>(h * std::clamp(value(static_cast<params::ParamId>(params::kDraw16 + i)), 0.0, 1.0));
      g.rect(x, bottom - h, 28, h, border);
      g.rect(x, bottom - level, 28, level, relevant ? amber : faint);
      g.text_centered(x + 14, r.bottom() - 2, kNames[i], relevant ? soft : faint, 60, TextSize::Small);
    }
    // One cycle of the summed footages, flute/reed blended, so the drawbars read as a shape.
    const int wx = r.x + 520, ww = r.w - 560, mid = r.y + r.h / 2 + 10, amp = r.h / 2 - 28;
    g.text(wx, r.y + 24, "TONE", muted, 100, TextSize::Small);
    std::vector<Point> points;
    const double reed = value(params::kOrganReed);
    for (int i = 0; i < 200; ++i) {
      const double phase = i / 199.0;
      double y = 0.0;
      for (int f = 0; f < 6; ++f) {
        const double ratio = dsp::kFootageRatios[static_cast<size_t>(f)];
        const double ph = std::fmod(phase * ratio * 2.0, 1.0);
        y += value(static_cast<params::ParamId>(params::kDraw16 + f)) * ((1.0 - reed) * std::sin(dsp::kTau * ph) + reed * 0.7 * (ph < 0.5 ? 1.0 : -1.0));
      }
      points.push_back({wx + i * ww / 199, mid - static_cast<int>(std::clamp(y / 2.5, -1.2, 1.2) * amp)});
    }
    g.polyline(points, relevant ? amber : faint, 3);
  }

  void draw_filter(Painter& g, const Rect& r) const {
    using namespace theme;
    const bool relevant = source() == params::kSourceSynth;
    const int wx = r.x + 20, ww = 700, mid = r.y + r.h / 2 + 10, amp = r.h / 2 - 28;
    g.text(wx, r.y + 24, "OSCILLATORS", muted, 200, TextSize::Small);
    const auto shape = [](int wave, double phase, double width) {
      switch (wave) {
        case 0: return 2.0 * phase - 1.0;
        case 1: return phase < width ? 1.0 : -1.0;
        case 2: return phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase;
        default: return std::sin(dsp::kTau * phase);
      }
    };
    std::vector<Point> points;
    for (int i = 0; i < 240; ++i) {
      const double phase = std::fmod(i / 239.0 * 2.0, 1.0);
      const double y = (1.0 - value(params::kOscMix)) * shape(static_cast<int>(value(params::kOsc1Wave)), phase, value(params::kPulseWidth)) +
                       value(params::kOscMix) * shape(static_cast<int>(value(params::kOsc2Wave)), std::fmod(phase * std::exp2(value(params::kOsc2Semi) / 12.0), 1.0), value(params::kPulseWidth));
      points.push_back({wx + i * ww / 239, mid - static_cast<int>(std::clamp(y, -1.2, 1.2) * amp)});
    }
    g.polyline(points, relevant ? green : faint, 3);
    const int fx = r.x + 780, fw = r.w - 820, base = r.bottom() - 16;
    g.text(fx, r.y + 24, "FILTER RESPONSE", muted, 240, TextSize::Small);
    const double cutoff = value(params::kCutoff), res = value(params::kResonance);
    std::vector<Point> response;
    for (int i = 0; i < 120; ++i) {
      const double hz = 30.0 * std::pow(600.0, i / 119.0), ratio = hz / std::max(30.0, cutoff);
      const double gain = 1.0 / std::sqrt(1.0 + std::pow(ratio, 8.0)) *
                          std::max(0.25, 1.0 + res * 2.0 * std::exp(-std::pow(std::log(std::max(0.001, ratio)) / 0.25, 2.0)));
      response.push_back({fx + i * fw / 119, base - static_cast<int>(std::clamp(gain, 0.0, 2.2) * (r.h - 70) / 2.2)});
    }
    g.polyline(response, relevant ? amber : faint, 2);
  }

  void draw_fm_routing(Painter& g, const Rect& r) const {
    using namespace theme;
    const bool relevant = source() == params::kSourceFm;
    // The patch table's algorithms: 4 (two pairs), 0 (serial), 7 (parallel), 5 (one modulator into three).
    static constexpr int kAlgorithmOfPatch[8] = {4, 4, 0, 4, 4, 7, 4, 5};
    const int patch = std::clamp(static_cast<int>(value(params::kFmPatch)), 0, 7);
    const int algorithm = kAlgorithmOfPatch[patch];
    g.text(r.x + 20, r.y + 24, "OPERATOR ROUTING", muted, 260, TextSize::Small);
    struct Edge { int from, to; };
    std::vector<Edge> edges;
    bool carrier[5] = {};
    switch (algorithm) {
      case 0: edges = {{1, 2}, {2, 3}, {3, 4}}; carrier[4] = true; break;
      case 4: edges = {{1, 2}, {3, 4}}; carrier[2] = carrier[4] = true; break;
      case 5: edges = {{1, 2}, {1, 3}, {1, 4}}; carrier[2] = carrier[3] = carrier[4] = true; break;
      default: carrier[1] = carrier[2] = carrier[3] = carrier[4] = true; break;
    }
    const auto centre_x = [&](int op) { return r.x + 40 + (op - 1) * 120 + 25; };
    const int y = r.y + 40, cy = y + 25;
    for (const Edge& e : edges) {
      const int x0 = centre_x(e.from), x1 = centre_x(e.to);
      if (std::abs(e.from - e.to) == 1) g.line(std::min(x0, x1) + 25, cy, std::max(x0, x1) - 25, cy, relevant ? pink : faint, 3);
      else {
        const int lift = 22 + 10 * std::abs(e.from - e.to);
        g.polyline({{x0, cy - 25}, {x0, cy - lift}, {x1, cy - lift}, {x1, cy - 25}}, relevant ? pink : faint, 3);
      }
    }
    for (int i = 1; i <= 4; ++i) {
      g.circle(centre_x(i) - 25, y, 50, relevant ? (carrier[i] ? amber : pink) : faint);
      char op[8]{};
      std::snprintf(op, sizeof(op), "%d", i);
      g.text_centered(centre_x(i), y + 33, op, bg, 30, TextSize::Normal);
    }
    char line[160]{};
    std::snprintf(line, sizeof(line), "%s  •  algorithm %d  •  amber operators reach the output", params::kFmPatchNames[patch], algorithm);
    g.text(r.x + 560, r.y + 70, line, relevant ? text : faint, r.w - 600);
  }

  void draw_slots(Painter& g, const Rect& r) const {
    using namespace theme;
    g.text(r.x + 20, r.y + 24, "SAMPLE SLOTS  (left-click load  •  right-click clear  •  empty = found sound)", muted, 900, TextSize::Small);
    const int root = static_cast<int>(value(params::kSampleRoot));
    static const char* const kKeys[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    for (int i = 0; i < 16; ++i) {
      const Rect tile = strip::slot_tile(r, i);
      const bool loaded = live_.slot_loaded[static_cast<size_t>(i)];
      g.round_rect(tile, 6, loaded ? cyan : border);
      char n[8]{};
      std::snprintf(n, sizeof(n), "%d", i + 1);
      g.text_centered(tile.x + tile.w / 2, tile.y + 22, n, loaded ? bg : muted, tile.w, TextSize::Small);
      const int key = root + i;
      char key_name[16]{};
      std::snprintf(key_name, sizeof(key_name), "%s%d", kKeys[((key % 12) + 12) % 12], key / 12 - 1);
      g.text_centered(tile.x + tile.w / 2, tile.y + 44, key_name, loaded ? bg : faint, tile.w, TextSize::Small);
    }
    if (live_.first_slot >= 0) {
      char line[200]{};
      std::snprintf(line, sizeof(line), "slot %d: %s  •  %.0f Hz root  •  %.1f s", live_.first_slot + 1, live_.first_slot_name.c_str(), live_.first_slot_root_hz, live_.first_slot_seconds);
      g.text(r.x + 940, r.y + 70, line, soft, r.w - 980, TextSize::Small);
    }
  }

  void draw_fuzz_curve(Painter& g, const Rect& r) const {
    using namespace theme;
    const double amount = value(params::kFuzz);
    g.text(r.x + 20, r.y + 24, "FUZZ TRANSFER", muted, 200, TextSize::Small);
    const int cx = r.x + 20, cw = 300, mid = r.y + r.h / 2 + 10, amp = r.h / 2 - 28;
    g.line(cx, mid, cx + cw, mid, border);
    std::vector<Point> curve;
    const double gain = 1.0 + 12.0 * amount;
    const int mode = static_cast<int>(value(params::kFuzzMode));
    for (int i = 0; i < 120; ++i) {
      const double x = (i / 119.0) * 2.0 - 1.0;
      // The Plexi's biased triode clips one side first; the Octave folds the bottom half up.
      double shaped = std::tanh(x * gain) / std::tanh(gain);
      if (mode == 3) shaped = (std::tanh(x * gain + 0.4) - std::tanh(0.4)) / (std::tanh(gain + 0.4) - std::tanh(0.4));
      if (mode == 4) shaped = 2.0 * std::abs(shaped) - 1.0;
      const double y = shaped * (0.3 + 0.7 * amount) + x * (1.0 - amount) * 0.7;
      curve.push_back({cx + i * cw / 119, mid - static_cast<int>(std::clamp(y, -1.0, 1.0) * amp)});
    }
    g.polyline(curve, amount > 0.0 ? red : faint, 3);
    // The pedalboard: four stomp boxes, lit when engaged.
    static constexpr const char* kPedals[4] = {"FUZZ", "PHASER", "ROTARY", "TREMOLO"};
    const double levels[4] = {value(params::kFuzz), value(params::kPhaserMix), value(params::kRotaryMix), value(params::kTremoloDepth)};
    static constexpr uint32_t kColours[4] = {red, violet, cyan, amber};
    for (int i = 0; i < 4; ++i) {
      const Rect box{r.x + 400 + i * 190, r.y + 20, 160, r.h - 40};
      g.round_rect(box, 10, levels[i] > 0.0 ? mix(panel_hi, kColours[i], 0.35) : panel_hi);
      g.circle(box.x + box.w / 2 - 9, box.y + 14, 18, levels[i] > 0.0 ? kColours[i] : border);
      g.text_centered(box.x + box.w / 2, box.bottom() - 16, kPedals[i], levels[i] > 0.0 ? text : muted, box.w - 10, TextSize::Small);
    }
  }

  void draw_echo_heads(Painter& g, const Rect& r) const {
    using namespace theme;
    g.text(r.x + 20, r.y + 24, "TAPE LOOP  •  record head, three playback heads", muted, 500, TextSize::Small);
    const int y = r.y + r.h / 2 + 14, x0 = r.x + 40, x1 = r.right() - 40;
    g.rect(x0, y - 3, x1 - x0, 6, border);
    static constexpr int kMasks[7] = {1, 2, 4, 3, 6, 5, 7};
    const int mask = kMasks[std::clamp(static_cast<int>(value(params::kEchoHeads)), 0, 6)];
    const bool on = value(params::kEchoMix) > 0.0;
    g.circle(x0 - 12, y - 12, 24, on ? red : faint);
    g.text_centered(x0, y + 44, "REC", muted, 60, TextSize::Small);
    static constexpr double kDistances[3] = {0.33, 0.5, 0.66};
    for (int h = 0; h < 3; ++h) {
      const int x = x0 + static_cast<int>((x1 - x0) * kDistances[h]);
      const bool heard = (mask & (1 << h)) != 0;
      g.circle(x - 12, y - 12, 24, on && heard ? amber : (on ? border : faint));
      char label[8]{};
      std::snprintf(label, sizeof(label), "%d", h + 1);
      g.text_centered(x, y + 44, label, on && heard ? text : muted, 40, TextSize::Small);
    }
    char line[96]{};
    std::snprintf(line, sizeof(line), "%.0f ms  •  feedback %.0f%%", value(params::kEchoTime), value(params::kEchoFeedback) * 100.0);
    g.text_right(x1, r.y + 24, line, on ? text : faint, 400, TextSize::Small);
  }

  void draw_reels(Painter& g, const Rect& r) const {
    using namespace theme;
    g.text(r.x + 20, r.y + 24, "MASTER TAPE", muted, 200, TextSize::Small);
    const double phase = live_.phase * 0.7;
    for (int reel = 0; reel < 2; ++reel) {
      const int d = r.h - 36, cx = r.x + 60 + reel * 260, cy = r.y + 18;
      g.circle(cx, cy, d, panel_hi);
      g.circle(cx + d / 2 - 14, cy + d / 2 - 14, 28, border);
      for (int spoke = 0; spoke < 3; ++spoke) {
        const double a = phase * (reel ? 1.3 : 1.0) + spoke * 2.094;
        g.line(cx + d / 2 + static_cast<int>(std::cos(a) * 16), cy + d / 2 + static_cast<int>(std::sin(a) * 16),
               cx + d / 2 + static_cast<int>(std::cos(a) * (d / 2 - 8)), cy + d / 2 + static_cast<int>(std::sin(a) * (d / 2 - 8)),
               live_.energy > 0.001f ? cyan : faint, 4);
      }
    }
    g.rect(r.x + 60 + (r.h - 36) / 2, r.y + 18 + (r.h - 36) / 2 + 34, 260, 3, border);
    char line[160]{};
    std::snprintf(line, sizeof(line), "generation %d  •  bandwidth %.0f Hz  •  %d bits", static_cast<int>(value(params::kGenerations)),
                  value(params::kMachineBandwidth), static_cast<int>(value(params::kLofiBits)));
    g.text(r.x + 420, r.y + 70, line, soft, r.w - 460);
  }

  const EditorHost& host_;
  const LiveState& live_;
};

}  // namespace fuzzboi::ui
