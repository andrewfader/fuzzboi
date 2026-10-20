// The air and the room the instrument is played in, from their physics: what the sound loses on
// its way through the air to the microphone, the walls it reflects from, and the room's tail.
//
// - Air absorbs sound by viscosity and heat conduction (growing as the frequency squared) and by
//   the relaxation of its oxygen and nitrogen molecules, whose relaxation frequencies are set by
//   how much water vapour there is. ISO 9613-1:1993 gives the attenuation in dB per metre from
//   the temperature, the relative humidity and the pressure; that is used here unchanged. Dry air
//   at room temperature takes the top end off fastest, very humid air least.
// - Sound travels at 331.3 sqrt(1 + T/273.15) m/s, so the distance to the microphone is a delay.
// - The microphone's preamp is set for the direct sound: the direct path is held at unity, and
//   the distance changes what reaches the microphone with it (the air's loss, the delay, the room
//   against the direct sound), as moving a microphone back from an amplifier does.
// - The room is a box with Bolt's 1 : 1.4 : 1.9 proportions. Its first- and second-order
//   reflections are images of the source in the walls (Allen and Berkley), each its own path
//   length (so its own delay, spreading loss and air loss) and losing sqrt(1 - a) of its pressure
//   at each wall of absorption coefficient a. The late tail is an eight-line feedback delay
//   network (Jot) decaying at the Sabine reverberation time 0.161 V / (S a + 4 m V), the air's own
//   absorption m included, at 500 Hz and at 8 kHz; it sits at the level diffuse-field theory gives
//   against the direct sound at that distance, 16 pi r^2 / A, less what the early reflections
//   already carry.
// - Wood takes up water from the air until it reaches its equilibrium moisture content
//   (Hailwood-Horrobin, as fitted in the USDA Wood Handbook ch. 4); a wetter top is less stiff,
//   heavier and lossier, so an acoustic body's resonances fall and broaden in humid air. See
//   wood_body() for the figures used.
//
// - The microphone is omnidirectional, cardioid or figure-of-eight: a share a of pressure and
//   1 - a of pressure gradient, so it hears sound arriving at angle t from its axis by
//   a + (1 - a) cos t, and a third of a diffuse field's energy when directional (a directivity
//   factor of 3). Near a source the gradient part sees the spherical wave's extra slope:
//   1 + (1 - a) c / (j w d), the proximity effect, which lifts the bass of a close microphone by
//   15 dB at 100 Hz for a cardioid 5 cm from a speaker; the capsule and preamp roll it off below
//   35 Hz, and nothing sits closer than the 5 cm from grille to capsule.
//
// Not modelled: light (it does not reach the sound), air pressure (fixed at one atmosphere),
// wind and temperature gradients, and the walls' frequency dependence (one coefficient).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "core.hpp"

namespace fuzzboi::dsp::air {

constexpr double kReferencePressure = 101.325;  // kPa
constexpr double kT0 = 293.15;                  // K, ISO 9613-1's reference temperature
constexpr double kTriple = 273.16;              // K, triple point of water

inline double speed_of_sound(double celsius) { return 331.3 * std::sqrt(1.0 + celsius / 273.15); }

// ISO 9613-1 attenuation coefficient, dB per metre, at `hz`, `celsius` and relative humidity
// `rh` (0..1), at one atmosphere.
inline double absorption_db_per_m(double hz, double celsius, double rh) {
  const double t = celsius + 273.15;
  const double psat_ratio = std::pow(10.0, -6.8346 * std::pow(kTriple / t, 1.261) + 4.6151);
  const double h = 100.0 * std::clamp(rh, 0.0, 1.0) * psat_ratio;  // molar concentration of water vapour, %
  const double fro = 24.0 + 4.04e4 * h * (0.02 + h) / (0.391 + h);
  const double frn = std::pow(t / kT0, -0.5) * (9.0 + 280.0 * h * std::exp(-4.170 * (std::pow(t / kT0, -1.0 / 3.0) - 1.0)));
  const double f2 = hz * hz;
  return 8.686 * f2 * (1.84e-11 * std::sqrt(t / kT0) +
                       std::pow(t / kT0, -2.5) * (0.01275 * std::exp(-2239.1 / t) / (fro + f2 / fro) +
                                                  0.1068 * std::exp(-3352.0 / t) / (frn + f2 / frn)));
}

// Equilibrium moisture content of wood (percent) in air at `celsius` and relative humidity `rh`:
// the Hailwood-Horrobin fit of the Wood Handbook (temperatures in Fahrenheit, as published).
inline double wood_emc(double rh, double celsius) {
  const double f = celsius * 1.8 + 32.0;
  const double w = 330.0 + 0.452 * f + 0.00415 * f * f;
  const double k = 0.791 + 4.63e-4 * f - 8.44e-7 * f * f;
  const double k1 = 6.34 + 7.75e-4 * f - 9.35e-5 * f * f;
  const double k2 = 1.09 + 2.84e-2 * f - 9.04e-5 * f * f;
  const double h = std::clamp(rh, 0.0, 0.98);
  const double kh = k * h;
  return 1800.0 / w * (kh / (1.0 - kh) + (k1 * kh + 2.0 * k1 * k2 * kh * kh) / (1.0 + k1 * kh + k1 * k2 * kh * kh));
}

// How a spruce top at the moisture content air at (rh, celsius) brings it to differs from the same
// top at 45% and 20 C, the workshop conditions guitars are built and measured in.
//  - Stiffness: below fibre saturation a property P goes as P12 (P12 / Pg)^((12 - M) / (Mp - 12))
//    (Wood Handbook ch. 5), with the modulus of elasticity's dry-to-green ratio about 1.3 and Mp,
//    the moisture content at which it starts to change, about 25%.
//  - Mass: the water adds its own weight; the top swells by about 0.37% in volume per percent.
//  - A plate's mode frequencies go as sqrt(E / rho).
//  - Damping: the loss factor of spruce along the grain rises by several percent per percent of
//    moisture (Obataya, Norimoto and Gril, 1998); taken here as 5% per percent.
struct WoodBody { double frequency_ratio = 1.0, q_ratio = 1.0; };
inline WoodBody wood_body(double rh, double celsius) {
  const auto modulus = [](double m) { return std::pow(1.3, (12.0 - m) / 13.0); };
  const auto density = [](double m) { return (1.0 + m / 100.0) / (1.0 + 0.0037 * m); };
  const double m = wood_emc(rh, celsius), m0 = wood_emc(0.45, 20.0);
  WoodBody b;
  b.frequency_ratio = std::sqrt((modulus(m) / modulus(m0)) / (density(m) / density(m0)));
  b.q_ratio = 1.0 / std::max(0.2, 1.0 + 0.05 * (m - m0));
  return b;
}

// Gain of a one-pole low-pass y = a x + b y1 at angular frequency w (radians per sample).
inline double one_pole_gain(double b, double w) { return (1.0 - b) / std::sqrt(1.0 - 2.0 * b * std::cos(w) + b * b); }

// The pole of a one-pole low-pass losing `db` at `hz`: what the air does over a path. A one-pole's
// loss rises as the frequency squared below its corner, as the air's does over most of the band.
inline double pole_for_loss(double db, double hz, double rate) {
  if (db <= 1e-4) return 0.0;
  const double w = kTau * std::min(hz, rate * 0.45) / rate, want = std::pow(10.0, -db / 20.0);
  double lo = 0.0, hi = 0.9999;
  for (int i = 0; i < 60; ++i) {
    const double mid = 0.5 * (lo + hi);
    if (one_pole_gain(mid, w) > want) lo = mid; else hi = mid;
  }
  return 0.5 * (lo + hi);
}

// The stage the engine runs: the microphone in the room, hearing the instrument through the air.
class Room {
 public:
  struct Settings {
    double volume_m3 = 0.0;      // 0: no room, a microphone in the open
    double absorption = 0.25;    // the walls' mean absorption coefficient
    double distance_m = 0.0;     // 0: the microphone on the instrument (the stage is bypassed)
    double humidity = 0.45;      // relative, 0..1
    double celsius = 20.0;
    int pattern = 0;             // 0 omnidirectional, 1 cardioid, 2 figure-of-eight
    bool operator==(const Settings&) const = default;
  };
  // The pressure share of each pattern.
  static double omni_share(int pattern) { return pattern == 1 ? 0.5 : pattern == 2 ? 0.0 : 1.0; }

  static constexpr int kLines = 8;
  static constexpr int kMaxTaps = 25;       // the images up to second order, the direct path among them
  static constexpr double kMaxPathS = 0.6;  // longest second-order path in the largest room
  static constexpr double kMaxLineS = 0.3;

  void prepare(double rate) {
    rate_ = rate;
    for (auto& d : direct_) d.prepare(static_cast<size_t>(rate * 0.05) + 8);
    paths_.prepare(static_cast<size_t>(rate * kMaxPathS) + 8);
    for (auto& l : lines_) l.prepare(static_cast<size_t>(rate * kMaxLineS) + 8);
    configured_ = false;
    reset();
  }

  void reset() {
    for (auto& d : direct_) d.clear();
    paths_.clear();
    for (auto& l : lines_) l.clear();
    direct_lp_.fill(0.0f);
    proximity_state_.fill(0.0f);
    tap_lp_.fill(0.0f);
    line_lp_.fill(0.0f);
  }

  bool active() const { return active_; }
  // The arrival time of the direct sound, seconds: the distance over the speed of sound.
  double direct_delay_s() const { return direct_delay_ / rate_; }
  // The Sabine reverberation time (s) at 500 Hz and 8 kHz, for the tail and for the tests.
  double rt60_low() const { return rt60_low_; }
  double rt60_high() const { return rt60_high_; }
  // How far the microphone actually is from the source: what was asked for, unless the far wall
  // is nearer.
  double reach_m() const { return reach_; }

  void configure(const Settings& in) {
    if (configured_ && in == settings_) return;
    settings_ = in;
    configured_ = true;
    const Settings& s = settings_;
    active_ = s.distance_m > 0.0 || s.volume_m3 > 0.0;
    if (!active_) return;
    const double c = speed_of_sound(s.celsius);
    const double distance = std::max(0.02, s.distance_m);
    // Whole samples: a fractional read interpolates, and linear interpolation is itself a low-pass
    // (a dB at 8 kHz) the air does not have. A sample is 7 mm of travel.
    direct_delay_ = std::round(distance / c * rate_);
    // The air's loss on a path, fitted at 8 kHz where it is heard.
    const double per_m_8k = absorption_db_per_m(8000.0, s.celsius, s.humidity);
    direct_pole_ = static_cast<float>(pole_for_loss(per_m_8k * distance, 8000.0, rate_));
    const double share = omni_share(s.pattern);
    proximity_ = static_cast<float>((1.0 - share) * c / std::max(0.05, distance) / rate_);
    proximity_leak_ = static_cast<float>(std::exp(-kTau * 35.0 / rate_));
    taps_ = 0;
    late_gain_ = 0.0f;
    if (s.volume_m3 <= 0.0) return;
    // The room: Bolt's proportions, the source a quarter of the way along its length at a player's
    // height and a little off the middle, the microphone `distance` away down the length of the
    // room (as far as the far wall allows).
    const double a = std::cbrt(s.volume_m3 / (1.0 * 1.4 * 1.9));
    const std::array<double, 3> size{1.9 * a, 1.4 * a, a};
    const std::array<double, 3> src{0.25 * size[0], 0.43 * size[1], std::min(1.0, 0.45 * size[2])};
    const double reach = std::min(distance, 0.9 * (size[0] - src[0]) - 0.3);
    const std::array<double, 3> mic{src[0] + reach, src[1], src[2]};
    reach_ = reach;
    const double surface = 2.0 * (size[0] * size[1] + size[0] * size[2] + size[1] * size[2]);
    const double alpha = std::clamp(s.absorption, 0.01, 0.99);
    const double reflect = std::sqrt(1.0 - alpha);
    // Which way the microphone faces (at the source), for placing reflections left and right.
    const double fx = -1.0, fy = 0.0;
    double early_energy = 0.0;
    for (int nx = -2; nx <= 2; ++nx)
      for (int ny = -2; ny <= 2; ++ny)
        for (int nz = -2; nz <= 2; ++nz) {
          const int order = std::abs(nx) + std::abs(ny) + std::abs(nz);
          if (order == 0 || order > 2 || taps_ >= kMaxTaps) continue;
          const auto image = [](int n, double length, double at) {
            return n % 2 == 0 ? n * length + at : (n + 1) * length - at;
          };
          const double ix = image(nx, size[0], src[0]), iy = image(ny, size[1], src[1]), iz = image(nz, size[2], src[2]);
          const double dx = ix - mic[0], dy = iy - mic[1], dz = iz - mic[2];
          const double r = std::sqrt(dx * dx + dy * dy + dz * dz);
          Tap& t = tap_[static_cast<size_t>(taps_++)];
          t.delay = std::round(std::min(r / c * rate_, kMaxPathS * rate_));
          // Arriving from (dx, dy, dz) away; the microphone faces the source, down -x.
          const double facing = std::clamp(-dx / std::max(r, 1e-3), -1.0, 1.0);
          const double gain = reach / std::max(r, 1e-3) * std::pow(reflect, order) * (share + (1.0 - share) * facing);
          // Left or right of where the microphone faces: the cross product's sign and size.
          const double side = std::clamp((fx * dy - fy * dx) / std::max(r, 1e-3), -1.0, 1.0);
          t.left = static_cast<float>(gain * std::sqrt(0.5 * (1.0 + side)));
          t.right = static_cast<float>(gain * std::sqrt(0.5 * (1.0 - side)));
          t.pole = static_cast<float>(pole_for_loss(per_m_8k * r, 8000.0, rate_));
          early_energy += gain * gain;
        }
    // Sabine, the air's absorption in it: m is the intensity attenuation per metre.
    const auto sabine = [&](double hz) {
      const double m = absorption_db_per_m(hz, s.celsius, s.humidity) / 4.343;
      return 0.161 * s.volume_m3 / (surface * alpha + 4.0 * m * s.volume_m3);
    };
    rt60_low_ = sabine(500.0);
    rt60_high_ = sabine(8000.0);
    // The diffuse field against the direct sound at this distance (omnidirectional source):
    // 16 pi r^2 / A, where A is the room's absorption area.
    const double area = surface * alpha + 4.0 * (absorption_db_per_m(500.0, s.celsius, s.humidity) / 4.343) * s.volume_m3;
    const double diffuse = 16.0 * kPi * reach * reach / area;
    // A directional microphone hears a third of the diffuse field's energy.
    const double late = std::max(0.0, diffuse * (share < 1.0 ? 1.0 / 3.0 : 1.0) - early_energy);
    // The lines: lengths around the mean free path 4V/S, mutually spread and capped.
    const double mean_free = 4.0 * s.volume_m3 / surface / c;
    static constexpr std::array<double, kLines> spread{0.62, 0.71, 0.83, 0.93, 1.05, 1.17, 1.31, 1.46};
    double gain_sum = 0.0;
    for (int i = 0; i < kLines; ++i) {
      const double seconds = std::clamp(mean_free * spread[static_cast<size_t>(i)], 0.004, kMaxLineS * 0.95);
      Line& l = line_[static_cast<size_t>(i)];
      l.delay = std::round(seconds * rate_);
      const double low = std::pow(10.0, -3.0 * seconds / std::max(rt60_low_, 0.02));
      const double high = std::pow(10.0, -3.0 * seconds / std::max(rt60_high_, 0.02));
      l.gain = static_cast<float>(low);
      // The loop's extra loss at 8 kHz: a one-pole whose gain there is high / low.
      l.pole = static_cast<float>(pole_for_loss(-20.0 * std::log10(std::min(1.0, high / low)), 8000.0, rate_));
      gain_sum += low * low;
    }
    // A unit impulse into an orthonormal network that loses g^2 of its energy per pass puts
    // 1 / (1 - g^2) of energy through the lines in all, an eighth through each; each channel sums
    // four lines, so it carries half of it. Scaled so each channel's tail carries `late` against
    // its direct sound of one.
    const double mean_g2 = gain_sum / kLines;
    late_gain_ = static_cast<float>(std::sqrt(2.0 * late * (1.0 - mean_g2)));
    late_start_ = direct_delay_;
    for (int i = 0; i < taps_; ++i) late_start_ = std::max(late_start_, tap_[static_cast<size_t>(i)].delay);
  }

  // Takes the stereo signal at the instrument and returns what the microphone hears.
  void process(float& l, float& r) {
    if (!active_) return;
    direct_[0].push(l); direct_[1].push(r);
    const float mono = 0.5f * (l + r);
    paths_.push(mono);
    float out_l = direct_[0].read_int(static_cast<size_t>(direct_delay_)), out_r = direct_[1].read_int(static_cast<size_t>(direct_delay_));
    const float a = 1.0f - direct_pole_;
    direct_lp_[0] = a * out_l + direct_pole_ * direct_lp_[0];
    direct_lp_[1] = a * out_r + direct_pole_ * direct_lp_[1];
    out_l = direct_lp_[0]; out_r = direct_lp_[1];
    if (proximity_ > 0.0f) {
      // The gradient part's extra slope near the source: an integral of the pressure, leaked
      // below 35 Hz.
      proximity_state_[0] = proximity_leak_ * proximity_state_[0] + out_l;
      proximity_state_[1] = proximity_leak_ * proximity_state_[1] + out_r;
      out_l += proximity_ * proximity_state_[0];
      out_r += proximity_ * proximity_state_[1];
    }
    if (taps_ > 0) {
      for (int i = 0; i < taps_; ++i) {
        const Tap& t = tap_[static_cast<size_t>(i)];
        float& state = tap_lp_[static_cast<size_t>(i)];
        state = (1.0f - t.pole) * paths_.read_int(static_cast<size_t>(t.delay)) + t.pole * state;
        out_l += t.left * state;
        out_r += t.right * state;
      }
      // The late tail: the eight lines read, filtered, mixed by a Hadamard matrix and fed back,
      // with the room's sound entering after the last early reflection.
      std::array<float, kLines> y{};
      for (int i = 0; i < kLines; ++i) {
        const Line& line = line_[static_cast<size_t>(i)];
        float& state = line_lp_[static_cast<size_t>(i)];
        state = (1.0f - line.pole) * lines_[static_cast<size_t>(i)].read_int(static_cast<size_t>(line.delay)) + line.pole * state;
        y[static_cast<size_t>(i)] = line.gain * state;
      }
      hadamard(y);
      const float in = paths_.read_int(static_cast<size_t>(late_start_));
      float tail_l = 0.0f, tail_r = 0.0f;
      for (int i = 0; i < kLines; ++i) {
        lines_[static_cast<size_t>(i)].push(y[static_cast<size_t>(i)] + in * 0.35355339f);
        (i % 2 == 0 ? tail_l : tail_r) += y[static_cast<size_t>(i)];
      }
      out_l += late_gain_ * tail_l;
      out_r += late_gain_ * tail_r;
    }
    l = out_l; r = out_r;
  }

 private:
  struct Tap { double delay = 0.0; float left = 0.0f, right = 0.0f, pole = 0.0f; };
  struct Line { double delay = 1.0; float gain = 0.0f, pole = 0.0f; };

  // The orthonormal 8x8 Hadamard mix, in place.
  static void hadamard(std::array<float, kLines>& y) {
    for (size_t h = 1; h < kLines; h <<= 1)
      for (size_t i = 0; i < kLines; i += h << 1)
        for (size_t j = i; j < i + h; ++j) { const float a = y[j], b = y[j + h]; y[j] = a + b; y[j + h] = a - b; }
    for (auto& v : y) v *= 0.35355339f;  // 1 / sqrt(8)
  }

  double rate_ = 48000.0;
  Settings settings_{};
  bool configured_ = false, active_ = false;
  double direct_delay_ = 0.0, late_start_ = 0.0, rt60_low_ = 0.0, rt60_high_ = 0.0, reach_ = 0.0;
  float direct_pole_ = 0.0f, late_gain_ = 0.0f, proximity_ = 0.0f, proximity_leak_ = 0.0f;
  std::array<float, 2> proximity_state_{};
  int taps_ = 0;
  std::array<DelayLine, 2> direct_;
  DelayLine paths_;
  std::array<DelayLine, kLines> lines_;
  std::array<Tap, kMaxTaps> tap_{};
  std::array<Line, kLines> line_{};
  std::array<float, 2> direct_lp_{};
  std::array<float, kMaxTaps> tap_lp_{};
  std::array<float, kLines> line_lp_{};
};

}  // namespace fuzzboi::dsp::air
