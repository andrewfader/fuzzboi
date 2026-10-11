// A MilkDrop/projectM-style visualizer, patterned after the one on LOAMP's visualizer page.
// Every frame the previous picture is warped (zoomed, rotated, rippled), dimmed a little, and
// the current waveform is drawn into it, so the sound leaves trails that tunnel, spiral and
// melt. Bass, mid and treble, each measured against its own recent average as MilkDrop does,
// push the motion. Presets cycle the way LOAMP's do: previous, next, random, and an auto mode
// that moves on every so often, blending from one preset's motion into the next.
//
// It is main-thread editor code, platform-neutral and deterministic: it is stepped with the
// output samples and an explicit time step, and drawn with filled rectangles and polylines, so
// the tests and the preview tool see exactly what the plug-in shows.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "editor.hpp"
#include "layout.hpp"

namespace fuzzboi::ui {

enum class VisualWave : uint8_t { Line, Circle, Lissajous, Spokes };
enum class VisualSymmetry : uint8_t { Plain, MirrorX, Quad };

// The continuous part of a preset, which blends when presets change. Rates are per frame at
// 30 frames per second and are rescaled to the real time step.
struct VisualMotion {
  double zoom;        // > 1 flies into the picture, < 1 pulls away from it
  double zoom_beat;   // extra zoom per unit of bass above its average
  double rotation;    // radians per frame
  double warp;        // ripple amplitude
  double warp_speed;  // ripple speed
  double decay;       // brightness kept per frame
  double rise;        // upward drift, picture heights per frame
  double hue_speed;   // turns of the colour wheel per second
  double saturation;
};

struct VisualPreset {
  const char* name;
  VisualMotion motion;
  VisualWave wave;
  VisualSymmetry symmetry;
  double hue;  // starting colour, in turns
};

inline constexpr VisualPreset kVisualPresets[] = {
    {"Liquid Tunnel", {1.035, 0.040, 0.004, 0.6, 0.8, 0.955, 0.000, 0.030, 0.80}, VisualWave::Circle, VisualSymmetry::Plain, 0.85},
    {"Kaleidoscope", {1.010, 0.030, 0.035, 0.3, 1.3, 0.950, 0.000, 0.060, 0.90}, VisualWave::Lissajous, VisualSymmetry::Quad, 0.10},
    {"Lava Lamp", {1.000, 0.010, 0.000, 1.6, 0.35, 0.975, 0.006, 0.020, 0.95}, VisualWave::Line, VisualSymmetry::Plain, 0.02},
    {"Starburst", {1.070, 0.080, 0.000, 0.1, 1.0, 0.900, 0.000, 0.080, 0.85}, VisualWave::Spokes, VisualSymmetry::Plain, 0.55},
    {"Oil Wheel", {1.015, 0.020, -0.030, 0.8, 0.6, 0.965, 0.000, 0.015, 1.00}, VisualWave::Circle, VisualSymmetry::Plain, 0.33},
    {"Echo Mirror", {1.025, 0.030, 0.000, 0.4, 0.9, 0.950, 0.000, 0.050, 0.80}, VisualWave::Line, VisualSymmetry::MirrorX, 0.70},
    {"Paisley Drift", {0.985, 0.020, 0.020, 0.9, 0.7, 0.970, 0.000, 0.040, 0.90}, VisualWave::Lissajous, VisualSymmetry::Plain, 0.92},
    {"Strobe Garden", {1.050, 0.120, 0.010, 0.2, 1.1, 0.880, 0.000, 0.070, 0.90}, VisualWave::Spokes, VisualSymmetry::Quad, 0.18},
};
inline constexpr int kVisualPresetCount = static_cast<int>(sizeof(kVisualPresets) / sizeof(kVisualPresets[0]));

class Visualizer {
 public:
  static constexpr int grid_w = 120, grid_h = 64;
  static constexpr double blend_seconds = 1.5;
  static constexpr double auto_seconds = 20.0;
  enum Button { kPrevious, kNext, kRandom, kAuto, kButtonCount };

  Visualizer() { reset(); }

  void reset() {
    cells_.assign(static_cast<size_t>(grid_w * grid_h * 3), 0.0f);
    time_ = since_change_ = 0.0;
    bass_ = mid_ = treble_ = 1.0;
    bass_avg_ = mid_avg_ = treble_avg_ = 0.0;
    wave_.clear();
    from_ = kVisualPresets[preset_].motion;
    blend_ = 1.0;
  }

  // --- presets ---
  int preset() const { return preset_; }
  const char* preset_name() const { return kVisualPresets[preset_].name; }
  bool auto_cycle() const { return auto_; }
  void set_auto_cycle(bool on) { auto_ = on; since_change_ = 0.0; }
  void set_preset(int index) {
    from_ = motion();
    preset_ = ((index % kVisualPresetCount) + kVisualPresetCount) % kVisualPresetCount;
    blend_ = 0.0;
    since_change_ = 0.0;
  }
  void next() { set_preset(preset_ + 1); }
  void previous() { set_preset(preset_ - 1); }
  // A different preset from the current one, from a fixed-seed generator so runs repeat.
  void random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    set_preset(preset_ + 1 + static_cast<int>(rng_ % static_cast<uint32_t>(kVisualPresetCount - 1)));
  }

  // --- what the tests read ---
  double bass() const { return bass_; }
  double mid() const { return mid_; }
  double treble() const { return treble_; }
  // Mean brightness of the picture, 0 (black) .. 1 (white).
  double brightness() const {
    double sum = 0.0;
    for (float c : cells_) sum += std::min(1.0f, c);
    return sum / static_cast<double>(cells_.size());
  }
  const std::vector<float>& cells() const { return cells_; }

  // Advances the picture by dt seconds. `samples` are the most recent output samples, oldest
  // first; the last few thousand are used.
  void step(const float* samples, size_t count, double sample_rate, double dt) {
    dt = std::clamp(dt, 0.0, 0.1);
    analyse(samples, count, sample_rate, dt);
    if (dt <= 0.0) return;
    time_ += dt;
    since_change_ += dt;
    blend_ = std::min(1.0, blend_ + dt / blend_seconds);
    if (auto_ && since_change_ >= auto_seconds) random();
    warp(dt);
    draw_wave_into_cells();
    apply_symmetry();
  }

  // The strip: a title row with the preset and its buttons, then the picture.
  void draw(Painter& g, const Rect& r) const {
    using namespace theme;
    g.text(r.x + 20, r.y + 30, "VISUALIZER", muted, 180, TextSize::Small);
    char name[96]{};
    std::snprintf(name, sizeof(name), "%s   %d / %d", preset_name(), preset_ + 1, kVisualPresetCount);
    g.text(r.x + 170, r.y + 31, name, amber, std::max(0, button_rect(r, kPrevious).x - r.x - 190), TextSize::Normal);
    static constexpr const char* kLabels[kButtonCount] = {"\xe2\x97\x80", "\xe2\x96\xb6", "RANDOM", "AUTO"};
    for (int b = 0; b < kButtonCount; ++b) {
      const Rect br = button_rect(r, b);
      const bool lit = b == kAuto && auto_;
      g.round_rect(br, 6, lit ? mix(panel_hi, cyan, 0.45) : panel_hi);
      g.text_centered(br.x + br.w / 2, br.y + 22, kLabels[b], lit ? text : soft, br.w - 6, TextSize::Small);
    }
    const Rect pic = picture_rect(r);
    if (pic.w <= 0 || pic.h <= 0) return;
    // Runs of equal colour along a row go out as one rectangle; each overlaps its neighbours by
    // a unit so scaling never leaves seams.
    for (int y = 0; y < grid_h; ++y) {
      const int y0 = pic.y + y * pic.h / grid_h, y1 = pic.y + (y + 1) * pic.h / grid_h;
      int run_start = 0;
      uint32_t run_colour = colour_at(0, y);
      for (int x = 1; x <= grid_w; ++x) {
        const uint32_t c = x < grid_w ? colour_at(x, y) : ~run_colour;
        if (c == run_colour) continue;
        const int x0 = pic.x + run_start * pic.w / grid_w, x1 = pic.x + x * pic.w / grid_w;
        g.rect(x0, y0, x1 - x0 + (x < grid_w ? 1 : 0), y1 - y0 + (y + 1 < grid_h ? 1 : 0), run_colour);
        run_start = x;
        run_colour = c;
      }
    }
    // The live waveform on top, crisp, in the colour now being painted into the trails.
    if (wave_.size() >= 2) {
      std::vector<Point> points;
      points.reserve(wave_.size());
      for (const auto& p : wave_) points.push_back(to_screen(pic, p[0], p[1]));
      g.polyline(points, mix(wave_colour(0.0), 0xffffff, 0.35), 2);
    }
  }

  // Buttons, then the picture itself (left-click next, right-click previous).
  bool pointer(int button, int x, int y, const Rect& r) {
    for (int b = 0; b < kButtonCount; ++b) {
      if (!button_rect(r, b).contains(x, y) || button != 1) continue;
      switch (b) {
        case kPrevious: previous(); break;
        case kNext: next(); break;
        case kRandom: random(); break;
        default: set_auto_cycle(!auto_); break;
      }
      return true;
    }
    if (!picture_rect(r).contains(x, y)) return false;
    if (button == 1) { next(); return true; }
    if (button == 3) { previous(); return true; }
    return false;
  }

  static Rect button_rect(const Rect& r, int b) {
    static constexpr int kWidths[kButtonCount] = {48, 48, 104, 80};
    int x = r.right() - 16;
    for (int i = kButtonCount - 1; i >= b; --i) x -= kWidths[i] + (i == b ? 0 : 8);
    return {x, r.y + 8, kWidths[b], 32};
  }
  static Rect picture_rect(const Rect& r) { return {r.x + 12, r.y + 48, r.w - 24, r.h - 60}; }

 private:
  // Grid coordinates: u across in [-aspect, aspect], v up in [-1, 1].
  static constexpr double aspect = static_cast<double>(grid_w) / grid_h;

  VisualMotion motion() const {
    const VisualMotion& to = kVisualPresets[preset_].motion;
    const double t = blend_ * blend_ * (3.0 - 2.0 * blend_);
    const auto l = [t](double a, double b) { return a + (b - a) * t; };
    return {l(from_.zoom, to.zoom), l(from_.zoom_beat, to.zoom_beat), l(from_.rotation, to.rotation), l(from_.warp, to.warp),
            l(from_.warp_speed, to.warp_speed), l(from_.decay, to.decay), l(from_.rise, to.rise), l(from_.hue_speed, to.hue_speed),
            l(from_.saturation, to.saturation)};
  }

  // Bass (< 150 Hz), mid and treble (> 2.5 kHz) levels over the last 40 ms, each relative to a
  // slow average of itself: 1 is ordinary, above 1 is a hit. The waveform is taken from the same
  // window and amplified (up to +24 dB) so quiet playing still draws.
  void analyse(const float* samples, size_t count, double sample_rate, double dt) {
    wave_.clear();
    if (!samples || count < 64 || sample_rate <= 0.0) return;
    const size_t window = std::min(count, static_cast<size_t>(std::max(256.0, sample_rate * 0.04)));
    const size_t warm = std::min(count - window, window);
    const float* s = samples + (count - window - warm);
    const double a_low = 1.0 - std::exp(-dsp_tau * 150.0 / sample_rate);
    const double a_high = 1.0 - std::exp(-dsp_tau * 2500.0 / sample_rate);
    double low = 0.0, high = 0.0, e_bass = 0.0, e_mid = 0.0, e_treble = 0.0, peak = 0.0;
    for (size_t i = 0; i < warm + window; ++i) {
      const double x = s[i];
      low += a_low * (x - low);
      high += a_high * (x - high);
      if (i < warm) continue;
      e_bass += low * low;
      e_mid += (high - low) * (high - low);
      e_treble += (x - high) * (x - high);
      peak = std::max(peak, std::abs(x));
    }
    const double n = static_cast<double>(window);
    const double levels[3] = {std::sqrt(e_bass / n), std::sqrt(e_mid / n), std::sqrt(e_treble / n)};
    double* avgs[3] = {&bass_avg_, &mid_avg_, &treble_avg_};
    double* rel[3] = {&bass_, &mid_, &treble_};
    const double follow = 1.0 - std::exp(-dt / 0.8);
    for (int i = 0; i < 3; ++i) {
      *rel[i] = std::clamp((levels[i] + 1e-3) / (*avgs[i] + 1e-3), 0.0, 3.0);
      *avgs[i] += (levels[i] - *avgs[i]) * follow;
    }
    // Silence draws nothing, so the picture can go dark rather than keep painting a flat line.
    if (peak <= 1e-4) return;
    const double gain = std::clamp(0.85 / peak, 1.0, 16.0);
    const float* w = samples + (count - window);
    const int points = 256;
    const VisualWave mode = kVisualPresets[preset_].wave;
    for (int k = 0; k < points; ++k) {
      const auto at = [&](int index) {
        return std::clamp(static_cast<double>(w[static_cast<size_t>(index % points) * (window - 1) / (points - 1)]) * gain, -1.0, 1.0);
      };
      const double v = at(k), phase = static_cast<double>(k) / (points - 1);
      switch (mode) {
        case VisualWave::Line: wave_.push_back({(phase * 2.0 - 1.0) * aspect * 0.9, 0.45 * v}); break;
        case VisualWave::Circle: {
          const double radius = 0.45 + 0.25 * v, a = dsp_tau * phase;
          wave_.push_back({radius * std::cos(a), radius * std::sin(a)});
          break;
        }
        case VisualWave::Lissajous: wave_.push_back({0.8 * v * aspect * 0.6, 0.8 * at(k + points / 4)}); break;
        case VisualWave::Spokes: {
          const double radius = 0.12 + 0.7 * std::abs(v), a = dsp_tau * phase;
          wave_.push_back({radius * std::cos(a), radius * std::sin(a)});
          break;
        }
      }
    }
  }

  // Each cell takes its colour from where the motion says it came from, dimmed.
  void warp(double dt) {
    const VisualMotion m = motion();
    const double frames = dt * 30.0;
    const double zoom = std::pow(std::max(0.5, m.zoom + m.zoom_beat * std::max(0.0, bass_ - 1.0)), frames);
    const double angle = (m.rotation + 0.01 * std::sin(time_ * 0.3) * (treble_ - 1.0)) * frames;
    const double ca = std::cos(angle), sa = std::sin(angle);
    const double ripple = 0.012 * m.warp * frames * (0.6 + 0.4 * mid_);
    const double keep = std::pow(m.decay, frames);
    std::vector<float>& next = scratch_;
    next.assign(cells_.size(), 0.0f);
    for (int y = 0; y < grid_h; ++y)
      for (int x = 0; x < grid_w; ++x) {
        const double u = ((x + 0.5) / grid_w * 2.0 - 1.0) * aspect, v = 1.0 - (y + 0.5) / grid_h * 2.0;
        double su = (ca * u + sa * v) / zoom, sv = (-sa * u + ca * v) / zoom;
        su += ripple * std::sin(time_ * m.warp_speed * 1.3 + v * 3.1 + u * 0.7);
        sv += ripple * std::cos(time_ * m.warp_speed * 1.1 + u * 2.7 - v * 0.5) - 2.0 * m.rise * frames;
        const double gx = (su / aspect * 0.5 + 0.5) * grid_w - 0.5, gy = (0.5 - sv * 0.5) * grid_h - 0.5;
        const int x0 = static_cast<int>(std::floor(gx)), y0 = static_cast<int>(std::floor(gy));
        const double fx = gx - x0, fy = gy - y0;
        for (int c = 0; c < 3; ++c) {
          const double value = (1 - fx) * (1 - fy) * at(x0, y0, c) + fx * (1 - fy) * at(x0 + 1, y0, c) +
                               (1 - fx) * fy * at(x0, y0 + 1, c) + fx * fy * at(x0 + 1, y0 + 1, c);
          next[index(x, y, c)] = static_cast<float>(value * keep);
        }
      }
    cells_.swap(next);
  }

  void draw_wave_into_cells() {
    if (wave_.size() < 2) return;
    const bool spokes = kVisualPresets[preset_].wave == VisualWave::Spokes;
    for (size_t k = 0; k + 1 < wave_.size(); ++k) {
      const double colour_phase = static_cast<double>(k) / static_cast<double>(wave_.size());
      const auto rgb = wave_rgb(colour_phase);
      if (spokes) splat_segment(0.0, 0.0, wave_[k][0], wave_[k][1], rgb, 0.03);
      else splat_segment(wave_[k][0], wave_[k][1], wave_[k + 1][0], wave_[k + 1][1], rgb, 0.18);
    }
  }

  void splat_segment(double u0, double v0, double u1, double v1, const std::array<double, 3>& rgb, double amount) {
    const double cell = 2.0 / grid_h;
    const int steps = std::clamp(static_cast<int>(std::hypot(u1 - u0, v1 - v0) / (cell * 0.5)) + 1, 1, 400);
    for (int i = 0; i <= steps; ++i) {
      const double t = static_cast<double>(i) / steps;
      const double u = u0 + (u1 - u0) * t, v = v0 + (v1 - v0) * t;
      const int x = static_cast<int>(std::floor((u / aspect * 0.5 + 0.5) * grid_w));
      const int y = static_cast<int>(std::floor((0.5 - v * 0.5) * grid_h));
      if (x < 0 || y < 0 || x >= grid_w || y >= grid_h) continue;
      for (int c = 0; c < 3; ++c) {
        float& cell_value = cells_[index(x, y, c)];
        cell_value = std::min(1.0f, cell_value + static_cast<float>(rgb[static_cast<size_t>(c)] * amount));
      }
    }
  }

  void apply_symmetry() {
    const VisualSymmetry s = kVisualPresets[preset_].symmetry;
    if (s == VisualSymmetry::Plain) return;
    for (int y = 0; y < grid_h; ++y)
      for (int x = grid_w / 2; x < grid_w; ++x)
        for (int c = 0; c < 3; ++c) cells_[index(x, y, c)] = cells_[index(grid_w - 1 - x, y, c)];
    if (s != VisualSymmetry::Quad) return;
    for (int y = grid_h / 2; y < grid_h; ++y)
      for (int x = 0; x < grid_w; ++x)
        for (int c = 0; c < 3; ++c) cells_[index(x, y, c)] = cells_[index(x, grid_h - 1 - y, c)];
  }

  std::array<double, 3> wave_rgb(double phase) const {
    const VisualMotion m = motion();
    const double hue = kVisualPresets[preset_].hue + m.hue_speed * time_ + 0.15 * phase;
    std::array<double, 3> rgb{};
    for (int c = 0; c < 3; ++c) {
      const double h = std::fmod(hue + (2 - c) / 3.0 + 10.0, 1.0);
      const double ramp = std::clamp(std::abs(h * 6.0 - 3.0) - 1.0, 0.0, 1.0);
      rgb[static_cast<size_t>(c)] = 1.0 - m.saturation + m.saturation * ramp;
    }
    return rgb;
  }
  uint32_t wave_colour(double phase) const {
    const auto rgb = wave_rgb(phase);
    return (static_cast<uint32_t>(rgb[0] * 255.0) << 16U) | (static_cast<uint32_t>(rgb[1] * 255.0) << 8U) | static_cast<uint32_t>(rgb[2] * 255.0);
  }

  // A cell's display colour: a near-black base plus the light, quantised to 32 levels a channel
  // so dark stretches merge into long runs.
  uint32_t colour_at(int x, int y) const {
    static constexpr int kBase[3] = {0x0a, 0x06, 0x10};
    uint32_t rgb = 0;
    for (int c = 0; c < 3; ++c) {
      const double light = std::clamp(static_cast<double>(cells_[index(x, y, c)]), 0.0, 1.0);
      const int level = std::clamp(kBase[c] + static_cast<int>(std::pow(light, 0.8) * 255.0), 0, 255) & 0xf8;
      rgb = (rgb << 8U) | static_cast<uint32_t>(level);
    }
    return rgb;
  }

  Point to_screen(const Rect& pic, double u, double v) const {
    return {pic.x + static_cast<int>(std::clamp(u / aspect * 0.5 + 0.5, 0.0, 1.0) * pic.w),
            pic.y + static_cast<int>(std::clamp(0.5 - v * 0.5, 0.0, 1.0) * pic.h)};
  }

  static size_t index(int x, int y, int c) { return (static_cast<size_t>(y) * grid_w + static_cast<size_t>(x)) * 3 + static_cast<size_t>(c); }
  double at(int x, int y, int c) const {
    if (x < 0 || y < 0 || x >= grid_w || y >= grid_h) return 0.0;
    return cells_[index(x, y, c)];
  }

  static constexpr double dsp_tau = 6.283185307179586;

  std::vector<float> cells_, scratch_;
  std::vector<std::array<double, 2>> wave_;
  int preset_ = 0;
  bool auto_ = false;
  uint32_t rng_ = 0x9e3779b9u;
  double time_ = 0.0, since_change_ = 0.0, blend_ = 1.0;
  double bass_ = 1.0, mid_ = 1.0, treble_ = 1.0;
  double bass_avg_ = 0.0, mid_avg_ = 0.0, treble_avg_ = 0.0;
  VisualMotion from_{};
};

}  // namespace fuzzboi::ui
