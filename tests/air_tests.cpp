// The air and the room (src/dsp/air.hpp), and what the room's air does to the guitar, proved from
// audio against the physics they are built from: the speed of sound, ISO 9613-1's absorption,
// Sabine's reverberation time, diffuse-field theory's direct-to-reverberant ratio, the wood's
// moisture, and the strings' thermal expansion.
// Scenarios: features/air.feature.

#include <algorithm>
#include <cmath>
#include <vector>

#include "audio_measure.hpp"
#include "dsp/air.hpp"
#include "dsp/guitar.hpp"
#include "spec.hpp"

using namespace fuzzboi::dsp;
using measure::Span;

namespace {

constexpr double kRate = 48000.0;

air::Room room(double volume, double absorption, double distance, double humidity = 0.45, double celsius = 20.0) {
  air::Room r;
  r.prepare(kRate);
  air::Room::Settings s;
  s.volume_m3 = volume; s.absorption = absorption; s.distance_m = distance; s.humidity = humidity; s.celsius = celsius;
  r.configure(s);
  return r;
}

// The response of a room to a unit impulse, mono.
std::vector<float> impulse(air::Room& r, double seconds) {
  std::vector<float> out(static_cast<size_t>(seconds * kRate));
  for (size_t i = 0; i < out.size(); ++i) {
    float l = i == 0 ? 1.0f : 0.0f, rr = l;
    r.process(l, rr);
    out[i] = 0.5f * (l + rr);
  }
  return out;
}

// Steady-state level (dB) of a sine at `hz` after the room.
double sine_db(air::Room& r, double hz) {
  const size_t n = static_cast<size_t>(0.5 * kRate);
  std::vector<float> out(n);
  for (size_t i = 0; i < n; ++i) {
    float l = static_cast<float>(std::sin(kTau * hz * static_cast<double>(i) / kRate)), rr = l;
    r.process(l, rr);
    out[i] = l;
  }
  return measure::db(measure::rms(Span(out, n / 2, n / 2)) * std::sqrt(2.0));
}

// Schroeder's backward integration of an impulse response, band-limited: the decay curve in dB.
std::vector<double> schroeder(const std::vector<float>& x, double lo, double hi) {
  Biquad hp, lp;
  hp.set(Biquad::Type::HighPass, lo, 0.7, 0.0, kRate);
  lp.set(Biquad::Type::LowPass, hi, 0.7, 0.0, kRate);
  std::vector<double> e(x.size());
  for (size_t i = 0; i < x.size(); ++i) { const double y = lp.process(hp.process(x[i])); e[i] = y * y; }
  for (size_t i = e.size() - 1; i-- > 0;) e[i] += e[i + 1];
  const double total = e[0];
  for (auto& v : e) v = 10.0 * std::log10(std::max(v / total, 1e-30));
  return e;
}

// T30: the time the decay curve takes from -5 to -35 dB, doubled to 60 dB.
double t30(const std::vector<double>& curve) {
  size_t a = 0, b = 0;
  while (a < curve.size() && curve[a] > -5.0) ++a;
  b = a;
  while (b < curve.size() && curve[b] > -35.0) ++b;
  return 2.0 * static_cast<double>(b - a) / kRate;
}

std::vector<float> run(guitar::Guitar& g, double seconds) {
  std::vector<float> out(static_cast<size_t>(seconds * kRate));
  for (auto& x : out) x = g.render();
  return out;
}

guitar::Guitar make(int model, double humidity, double celsius) {
  guitar::Guitar g;
  g.prepare(kRate);
  guitar::Setup s;
  s.model = model; s.humidity = humidity; s.celsius = celsius;
  g.configure(s);
  return g;
}

}  // namespace

SCENARIO(sound_reaches_the_microphone_at_the_speed_of_sound) {
  GIVEN("a microphone 5 m from the source in the open, in air at 0 C and at 30 C");
  THEN("the click arrives 5 / (331.3 sqrt(1 + T / 273.15)) s later, within a sample: 15.09 ms and 14.36 ms");
  for (double celsius : {0.0, 30.0}) {
    air::Room r = room(0.0, 0.25, 5.0, 0.45, celsius);
    const auto out = impulse(r, 0.05);
    size_t at = 0;
    for (size_t i = 0; i < out.size(); ++i) if (std::abs(out[i]) > std::abs(out[at])) at = i;
    const double want = 5.0 / air::speed_of_sound(celsius) * kRate;
    CHECK(std::abs(static_cast<double>(at) - want) <= 1.5, spec::fmt("%.0f C: arrives at sample %.0f, expected %.1f", celsius, static_cast<double>(at), want));
  }
}

SCENARIO(the_air_takes_the_top_end_off_as_iso_9613_gives) {
  GIVEN("a microphone 10 m away in the open, in air at 20 C and 20% humidity");
  THEN("sines at 2, 4 and 8 kHz lose what ISO 9613-1 gives over 10 m, within 1 dB (the path is fitted at 8 kHz; its loss rises as the frequency squared, as the air's does)");
  for (double hz : {2000.0, 4000.0, 8000.0}) {
    air::Room r = room(0.0, 0.25, 10.0, 0.2, 20.0);
    const double loss = -sine_db(r, hz);
    const double want = 10.0 * air::absorption_db_per_m(hz, 20.0, 0.2);
    CHECK(std::abs(loss - want) <= 1.0, spec::fmt("%.0f Hz: lost %.2f dB, ISO gives %.2f", hz, loss, want));
  }
  THEN("humid air takes less than dry air at 8 kHz: 70% humidity loses less than 20%");
  air::Room dry = room(0.0, 0.25, 10.0, 0.2, 20.0), humid = room(0.0, 0.25, 10.0, 0.7, 20.0);
  CHECK(sine_db(humid, 8000.0) > sine_db(dry, 8000.0) + 0.5, "humid air is clearer");
}

SCENARIO(a_room_rings_for_its_sabine_time_and_the_air_shortens_the_top) {
  GIVEN("a 3000 cubic-metre hall with walls of absorption 0.2, the microphone 3 m from the source");
  air::Room r = room(3000.0, 0.2, 3.0);
  const auto ir = impulse(r, 4.0);
  THEN("its impulse response decays, by Schroeder's backward integration in the 500 Hz octave, within 25% of Sabine's 0.161 V / (S a + 4 m V)");
  const double low = t30(schroeder(ir, 354.0, 707.0));
  CHECK(std::abs(low / r.rt60_low() - 1.0) <= 0.25, spec::fmt("T30 %.2f s at 500 Hz, Sabine %.2f s", low, r.rt60_low()));
  THEN("the 8 kHz octave dies sooner than the 500 Hz one: the air in the hall soaks up the top as the sound crosses it again and again");
  const double high = t30(schroeder(ir, 5657.0, 11314.0));
  CHECK(high < 0.8 * low, spec::fmt("T30 %.2f s at 8 kHz against %.2f s at 500 Hz", high, low));
}

SCENARIO(the_further_the_microphone_the_more_room_it_hears) {
  GIVEN("a 150 cubic-metre studio room with walls of absorption 0.25, one microphone channel 0.5 m and then 2 m from the source");
  const auto reverberant = [](double distance) {
    air::Room r = room(150.0, 0.25, distance);
    std::vector<float> ir(static_cast<size_t>(3.0 * kRate));
    for (size_t i = 0; i < ir.size(); ++i) { float l = i == 0 ? 1.0f : 0.0f, rr = l; r.process(l, rr); ir[i] = l; }
    const size_t arrival = static_cast<size_t>(std::llround(r.direct_delay_s() * kRate));
    double direct = 0.0, rest = 0.0;
    for (size_t i = 0; i < ir.size(); ++i) {
      const double e = static_cast<double>(ir[i]) * ir[i];
      if (i + 1 >= arrival && i <= arrival + 24) direct += e; else if (i > arrival + 24) rest += e;
    }
    return 10.0 * std::log10(rest / direct);
  };
  // The room's absorption area A = S a + 4 m V, Bolt's box of 150 cubic metres.
  const double side = std::cbrt(150.0 / (1.0 * 1.4 * 1.9));
  const double area = 2.0 * (1.9 * 1.4 + 1.9 + 1.4) * side * side * 0.25 + 4.0 * air::absorption_db_per_m(500.0, 20.0, 0.45) / 4.343 * 150.0;
  const auto theory = [&](double r) { return 10.0 * std::log10(16.0 * kPi * r * r / area); };
  const double near = reverberant(0.5), far = reverberant(2.0);
  THEN("everything after the direct sound stands against it where diffuse-field theory puts it, 16 pi r^2 / A, within 4 dB at both distances");
  CHECK(std::abs(near - theory(0.5)) <= 4.0, spec::fmt("0.5 m: %.1f dB against the theory's %.1f", near, theory(0.5)));
  CHECK(std::abs(far - theory(2.0)) <= 4.0, spec::fmt("2 m: %.1f dB against the theory's %.1f", far, theory(2.0)));
  THEN("four times the distance gives the room 12 dB more against the direct sound, within 3 dB");
  CHECK(std::abs((far - near) - 12.0) <= 3.0, spec::fmt("%.1f dB more at 2 m than at 0.5 m", far - near));
}

SCENARIO(humid_air_lowers_a_flat_tops_voice) {
  GIVEN("the dreadnought in a room at 30% humidity and at 80% (its top at 6% and 16% moisture), every note from D3 to C#4 plucked in turn");
  THEN("the note whose fundamental the body radiates most, where the top's main modes are, is 1 to 5 semitones lower in the humid room: a wetter top is softer and heavier");
  const auto loudest = [](double humidity) {
    int best_note = 0;
    double best = -1e9;
    for (int note = 50; note <= 61; ++note) {
      guitar::Guitar g = make(guitar::kDreadnought, humidity, 21.0);
      const int string = note < 55 ? 1 : 2, fret = note - g.open_note(string);
      g.pluck(string, fret, 0.5, 7);
      const auto out = run(g, 1.25);
      const double level = measure::db(measure::tone_amplitude(Span(out, 4800, 48000), kRate, g.pitch_at(string, fret)));
      if (level > best) { best = level; best_note = note; }
    }
    return best_note;
  };
  const int dry = loudest(0.3), humid = loudest(0.8);
  CHECK(dry - humid >= 1 && dry - humid <= 5, spec::fmt("loudest at MIDI %.0f dry, %.0f humid", static_cast<double>(dry), static_cast<double>(humid)));
}

SCENARIO(a_warm_room_flattens_the_strings_and_nylon_most) {
  GIVEN("the single-coil electric and the classical, tuned at 20 C, played in a room at 35 C");
  THEN("the plain steel high E goes 10 to 20 cents flat (about a cent a degree), and the nylon treble E more than twice as far");
  const auto flat = [](int model) {
    guitar::Guitar cold = make(model, 0.45, 20.0), warm = make(model, 0.45, 35.0);
    return 1200.0 * std::log2(warm.pitch_at(5, 0) / cold.pitch_at(5, 0));
  };
  const auto heard = [](int model, double celsius) {
    guitar::Guitar g = make(model, 0.45, celsius);
    g.pluck(5, 0, 0.1, 5);
    const auto out = run(g, 1.5);
    return measure::pitch_hz(Span(out, 14400, 48000), kRate, 200.0, 600.0, 0.3);
  };
  const double steel = flat(guitar::kSingleCoil), nylon = flat(guitar::kClassical);
  const double steel_heard = 1200.0 * std::log2(heard(guitar::kSingleCoil, 35.0) / heard(guitar::kSingleCoil, 20.0));
  CHECK(steel <= -10.0 && steel >= -20.0, spec::fmt("steel %.1f cents", steel));
  CHECK(std::abs(steel_heard - steel) <= 3.0, spec::fmt("by ear the steel string moved %.1f cents", steel_heard));
  CHECK(nylon < 2.0 * steel, spec::fmt("nylon %.1f cents", nylon));
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
