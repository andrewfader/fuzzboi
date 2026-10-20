// The physical guitar (src/dsp/guitar.hpp), proved from its physics and its audio: real string
// data, pitch by ear in every tuning, intonation and saddle setbacks against the luthiers'
// figures, stiffness stretch measured on the partials, the decay of the partials, the pluck
// point's notch, the tension glide of a hard pluck, the fingering and the choke, damping and
// sympathetic ringing, pickups and body, hammer-ons, and stability with every string ringing.
// Scenarios: features/guitar.feature.

#include <algorithm>
#include <cmath>
#include <vector>

#include "audio_measure.hpp"
#include "dsp/guitar.hpp"
#include "engine.hpp"
#include "spec.hpp"

using namespace fuzzboi::dsp;
using namespace fuzzboi::dsp::guitar;
using measure::Span;

namespace {

constexpr double kRate = 48000.0;

std::vector<float> run(Guitar& g, double seconds) {
  std::vector<float> out(static_cast<size_t>(seconds * kRate));
  for (auto& x : out) x = g.render();
  return out;
}

Guitar make(int model, int tuning = 0) {
  Guitar g;
  g.prepare(kRate);
  Setup s;
  s.model = model;
  s.tuning = tuning;
  g.configure(s);
  return g;
}

// The strongest component within +-span cents of `near`, scanned in 0.25-cent steps with a Hann
// window: a tuner's reading.
double peak_hz(const std::vector<float>& x, double from_s, double length_s, double near, double span = 40.0) {
  const size_t a = static_cast<size_t>(from_s * kRate), n = static_cast<size_t>(length_s * kRate);
  double best = -1.0, best_hz = near;
  for (double c = -span; c <= span; c += 0.25) {
    const double f = near * std::exp2(c / 1200.0);
    double re = 0.0, im = 0.0;
    const double step = kTau * f / kRate;
    for (size_t i = 0; i < n && a + i < x.size(); ++i) {
      const double w = 0.5 - 0.5 * std::cos(kTau * static_cast<double>(i) / static_cast<double>(n - 1));
      re += w * x[a + i] * std::cos(step * static_cast<double>(i));
      im -= w * x[a + i] * std::sin(step * static_cast<double>(i));
    }
    const double m = re * re + im * im;
    if (m > best) { best = m; best_hz = f; }
  }
  return best_hz;
}

double level_at(const std::vector<float>& x, double from_s, double length_s, double hz) {
  double best = 0.0;
  for (double c = -20.0; c <= 20.0; c += 2.0)
    best = std::max(best, measure::tone_amplitude(Span(x, static_cast<size_t>(from_s * kRate), static_cast<size_t>(length_s * kRate)), kRate, hz * std::exp2(c / 1200.0)));
  return best;
}

bool finite(const std::vector<float>& x) {
  for (float v : x) if (!std::isfinite(v)) return false;
  return true;
}

}  // namespace

SCENARIO(the_strings_are_the_makers_strings) {
  GIVEN("the three string sets from D'Addario's tension chart, at its 25.5-inch scale");
  // The chart's tensions (lb) for each string at standard pitch.
  struct Row { const std::array<StringSpec, kStrings>* set; std::array<double, kStrings> lb; const char* name; };
  const Row rows[] = {
      {&kPhosphorBronzeLights, {26.0, 29.9, 30.5, 30.2, 23.3, 23.3}, "EJ16"},
      {&kNickelRegularLights, {17.5, 19.5, 18.4, 16.6, 15.4, 16.2}, "EXL110"},
      {&kNylonNormal, {14.0, 15.0, 15.6, 12.1, 11.6, 15.3}, "EJ45"},
  };
  const int open[kStrings] = {40, 45, 50, 55, 59, 64};
  THEN("each string's unit weight gives the chart's tension within 1%");
  bool all = true;
  for (const Row& r : rows)
    for (int i = 0; i < kStrings; ++i) {
      const double mu = (*r.set)[static_cast<size_t>(i)].unit_weight * kPoundPerInch;
      const double t = mu * std::pow(2.0 * 25.5 * kInch * midi_to_hz(open[i]), 2.0) / 4.44822;
      const bool ok = std::abs(t / r.lb[static_cast<size_t>(i)] - 1.0) < 0.01;
      if (!ok) std::printf("    %s string %d: %.2f lb against %.2f\n", r.name, 6 - i, t, r.lb[static_cast<size_t>(i)]);
      all = all && ok;
    }
  CHECK(all, "every string matches the chart");
  WHEN("the strings are on the guitars, tuned to pitch");
  THEN("the tension at pitch (over the compensated length, with the stiffness term) is within 5% of the chart's, scaled to the guitar's scale");
  bool close = true;
  for (int model : {kDreadnought, kSingleCoil, kClassical}) {
    Guitar g = make(model);
    const Row& r = rows[model == kDreadnought ? 0 : model == kSingleCoil ? 1 : 2];
    for (int i = 0; i < kStrings; ++i) {
      const double scale = std::pow(g.guitar().scale_m / (25.5 * kInch), 2.0);
      const double want = r.lb[static_cast<size_t>(i)] * 4.44822 * scale;
      close = close && std::abs(g.physics(i).tension / want - 1.0) < 0.05;
    }
  }
  CHECK(close, "tensions at pitch");
}

SCENARIO(every_open_string_in_every_tuning_sounds_at_its_pitch) {
  GIVEN("the single-coil electric in each of the eight tunings, and the other guitars in standard");
  THEN("a soft pluck of each open string reads within 3 cents of its note on a tuner");
  double worst = 0.0;
  const auto check_string = [&](int model, int tuning, int string) {
    Guitar g = make(model, tuning);
    g.pluck(string, 0, 0.1, 3);
    const auto out = run(g, 1.3);
    const double want = midi_to_hz(kTunings[static_cast<size_t>(tuning)].notes[static_cast<size_t>(string)]);
    const double cents = 1200.0 * std::log2(peak_hz(out, 0.3, 1.0, want) / want);
    if (std::abs(cents) > 3.0) std::printf("    model %d tuning %s string %d: %+.1f cents\n", model, kTunings[static_cast<size_t>(tuning)].name, 6 - string, cents);
    worst = std::max(worst, std::abs(cents));
  };
  for (int tuning = 0; tuning < kTuningCount; ++tuning) for (int s = 0; s < kStrings; ++s) check_string(kSingleCoil, tuning, s);
  for (int model : {kDreadnought, kClassical, kHumbucker}) for (int s = 0; s < kStrings; ++s) check_string(model, 0, s);
  CHECK(worst <= 3.0, spec::fmt("worst open string %.1f cents", worst));
}

SCENARIO(the_bass_vi_is_a_guitar_an_octave_down) {
  GIVEN("the Bass VI: a 30-inch neck strung with D'Addario's XLB095 and NW072 to NW024, in standard tuning");
  Guitar g = make(kBassVI);
  const int open[kStrings] = {28, 33, 38, 43, 47, 52};
  THEN("its open strings are E1 A1 D2 G2 B2 E3, and a soft pluck of each reads within 3 cents of that note on a tuner");
  double worst = 0.0;
  for (int s = 0; s < kStrings; ++s) {
    Guitar b = make(kBassVI);
    b.pluck(s, 0, 0.1, 3);
    const auto out = run(b, 2.0);
    const double want = midi_to_hz(open[s]);
    const double cents = 1200.0 * std::log2(peak_hz(out, 0.3, 1.6, want) / want);
    if (std::abs(cents) > 3.0) std::printf("    string %d: %+.1f cents\n", 6 - s, cents);
    worst = std::max(worst, std::abs(cents));
    CHECK(g.open_note(s) == open[s], spec::fmt("string %d open note", 6 - s));
  }
  CHECK(worst <= 3.0, spec::fmt("worst open string %.1f cents", worst));
  THEN("each string pulls what its unit weight gives at pitch over the 30-inch scale, within 5%: 26 to 32 lb on every string");
  bool close = true, bass_like = true;
  for (int s = 0; s < kStrings; ++s) {
    const double mu = kNickelBassVI[static_cast<size_t>(s)].unit_weight * kPoundPerInch;
    const double want = mu * std::pow(2.0 * 30.0 * kInch * midi_to_hz(open[s]), 2.0);
    const double lb = g.physics(s).tension / 4.44822;
    if (std::abs(g.physics(s).tension / want - 1.0) >= 0.05) std::printf("    string %d: %.1f N against %.1f\n", 6 - s, g.physics(s).tension, want);
    close = close && std::abs(g.physics(s).tension / want - 1.0) < 0.05;
    bass_like = bass_like && lb > 25.0 && lb < 34.0;
  }
  CHECK(close, "tensions at pitch");
  CHECK(bass_like, "tensions in a six-string bass's range");
  WHEN("a bass note below a guitar's range is asked for");
  THEN("the bass plays A1 on its A string, where a guitar takes it up an octave");
  const auto a1 = g.finger(33);
  CHECK(a1.string == 1 && a1.fret == 0 && a1.note == 33, spec::fmt("bass fingers A1 at string %d fret %d", 6 - a1.string, a1.fret));
  CHECK(make(kSingleCoil).finger(33).note == 45, "a guitar moves A1 up to A2");
  Guitar b = make(kBassVI);
  b.pluck(a1.string, a1.fret, 0.5, 7);
  const auto out = run(b, 1.5);
  const double hz = peak_hz(out, 0.2, 1.2, 55.0, 100.0);
  CHECK(std::abs(1200.0 * std::log2(hz / 55.0)) < 5.0, spec::fmt("A1 sounds at %.2f Hz", hz));
}

// Magnitude of the strongest component within +-span cents of `hz` (the peak_hz scan's value).
double level_near(const std::vector<float>& x, double from_s, double length_s, double hz, double span = 15.0) {
  const size_t a = static_cast<size_t>(from_s * kRate), n = static_cast<size_t>(length_s * kRate);
  double best = 0.0;
  for (double c = -span; c <= span; c += 0.5) {
    const double f = hz * std::exp2(c / 1200.0), step = kTau * f / kRate;
    double re = 0.0, im = 0.0;
    for (size_t i = 0; i < n && a + i < x.size(); ++i) {
      const double w = 0.5 - 0.5 * std::cos(kTau * static_cast<double>(i) / static_cast<double>(n - 1));
      re += w * x[a + i] * std::cos(step * static_cast<double>(i));
      im -= w * x[a + i] * std::sin(step * static_cast<double>(i));
    }
    best = std::max(best, std::sqrt(re * re + im * im));
  }
  return best;
}

SCENARIO(a_twelve_string_sounds_every_course_twice) {
  GIVEN("the electric twelve-string (EXL150, pairs reversed) and the acoustic twelve-string (EJ38), in standard tuning");
  THEN("each second string, plucked alone, reads on a tuner its course's note plus the course interval and the few cents the player left it off, within 3 cents");
  double worst = 0.0;
  for (int model : {kElectric12, kAcoustic12})
    for (int c = 0; c < kStrings; ++c) {
      Guitar g = make(model);
      const GuitarSpec& spec = g.guitar();
      g.pluck_string(c + kStrings, 0, 0.1, 9);
      const auto out = run(g, 1.6);
      const double want = midi_to_hz(g.open_note(c) + spec.course_interval[static_cast<size_t>(c)]) * cents_ratio(spec.course_cents[static_cast<size_t>(c)]);
      const double cents = 1200.0 * std::log2(peak_hz(out, 0.3, 1.2, want) / want);
      if (std::abs(cents) > 3.0) std::printf("    model %d course %d second string: %+.1f cents\n", model, c, cents);
      worst = std::max(worst, std::abs(cents));
    }
  CHECK(worst <= 3.0, spec::fmt("worst second string %.1f cents", worst));
  WHEN("a key plucks a course, and separately each of its two strings is plucked alone: the first as the pick meets it, the second as it meets it later and a little softer");
  THEN("the course is the two strings sounding together: the sum of the two alone is within 15 dB of the course (pairs tuned in octaves and unisons are the strings that couple most through the bridge, so not closer), and the second string carries within 12 dB of the first's level");
  bool second = true;
  for (int model : {kElectric12, kAcoustic12})
    for (int c = 0; c < kStrings; ++c) {
      Guitar course = make(model), first = make(model), later = make(model);
      const GuitarSpec& spec = course.guitar();
      const int first_string = spec.octave_first ? c + kStrings : c, later_string = spec.octave_first ? c : c + kStrings;
      const double velocity = 0.6;
      course.pluck(c, 0, velocity, 11);
      first.pluck_string(first_string, 0, velocity, 11);
      const auto a = run(course, 1.0), f = run(first, 1.0);
      const size_t gap = static_cast<size_t>(std::max(1, static_cast<int>(spec.course_gap_s * (1.3 - 0.6 * velocity) * kRate)));
      const auto lead = run(later, static_cast<double>(gap) / kRate);
      later.pluck_string(later_string, 0, 0.85 * velocity, 11u ^ 0x9e3779b9u);
      auto l = run(later, 1.0 - static_cast<double>(lead.size()) / kRate);
      l.insert(l.begin(), lead.begin(), lead.end());
      std::vector<float> residual(a.size());
      for (size_t i = 0; i < a.size() && i < l.size(); ++i) residual[i] = a[i] - f[i] - l[i];
      const double fit = measure::db(measure::rms(Span(residual, 2400, 24000))) - measure::db(measure::rms(Span(a, 2400, 24000)));
      const double level = measure::db(measure::rms(Span(l, 2400, 24000))) - measure::db(measure::rms(Span(f, 2400, 24000)));
      if (fit > -15.0 || level < -12.0) std::printf("    model %d course %d: residual %+.1f dB, second string %+.1f dB\n", model, c, fit, level);
      second = second && fit <= -15.0 && level >= -12.0;
    }
  CHECK(second, "the second strings sound");
  WHEN("one course is plucked, and separately the string the pick meets first is plucked alone with the same pluck");
  THEN("the two renders agree sample for sample until the pick reaches the second string 1 to 4 ms later: the main string first on the electric (Rickenbacker's reversed pairs), the octave string first on the acoustic");
  for (auto [model, first] : {std::pair{kElectric12, 2}, std::pair{kAcoustic12, 2 + kStrings}}) {
    Guitar course = make(model), alone = make(model);
    course.pluck(2, 0, 0.6, 21);
    alone.pluck_string(first, 0, 0.6, 21);
    const auto a = run(course, 0.02), b = run(alone, 0.02);
    size_t split = a.size();
    for (size_t i = 0; i < a.size(); ++i) if (std::abs(a[i] - b[i]) > 1e-7f) { split = i; break; }
    const double ms = 1000.0 * static_cast<double>(split) / kRate;
    CHECK(ms >= 1.0 && ms <= 4.0, spec::fmt("model %.0f: second string joins at %.2f ms", static_cast<double>(model), ms));
  }
  WHEN("the key is let go half a second in, with a 50 ms release");
  THEN("both strings of the course are damped: 0.3 s later the guitar is more than 50 dB down on the note");
  Guitar g = make(kElectric12);
  g.pluck(3, 2, 0.8, 4);
  const auto held = run(g, 0.5);
  g.release(3, 0.05);
  const auto after = run(g, 0.5);
  const double drop = measure::db(measure::rms(Span(after, after.size() - 4800, 4800))) - measure::db(measure::rms(Span(held, 4800, 9600)));
  CHECK(drop < -50.0, spec::fmt("course down %.1f dB", drop));
}

SCENARIO(frets_are_equal_tempered_and_the_saddle_is_compensated_like_a_real_one) {
  GIVEN("each guitar's neck: frets at the equal-tempered positions, the action and nut height, the strings' stiffness");
  THEN("the 12th fret is an exact octave, every fret up to the 15th is within 10 cents of equal temperament, and the first fret reads sharp (the nut's height)");
  bool octave = true, tempered = true, sharp_first = true;
  for (int model = 0; model < kModelCount; ++model) {
    Guitar g = make(model);
    for (int s = 0; s < kStrings; ++s) {
      const double open = g.pitch_at(s, 0);
      octave = octave && std::abs(1200.0 * std::log2(g.pitch_at(s, 12) / (2.0 * open))) < 0.5;
      for (int f = 1; f <= 15; ++f) tempered = tempered && std::abs(1200.0 * std::log2(g.pitch_at(s, f) / (open * std::exp2(f / 12.0)))) < 10.0;
      sharp_first = sharp_first && g.pitch_at(s, 1) > open * std::exp2(1.0 / 12.0) * cents_ratio(0.5);
    }
  }
  CHECK(octave, "octaves at the 12th");
  CHECK(tempered, "frets within 10 cents");
  CHECK(sharp_first, "first fret sharp");
  THEN("the saddle setback the octave needs falls where luthiers set it: 1 to 3 mm on a steel treble string, 4 to 8 mm on the steel bass, a few mm on nylon, and the plain B and G further back than their neighbours");
  Guitar d = make(kDreadnought), c = make(kClassical), e = make(kSingleCoil);
  const auto mm = [](const Guitar& g, int s) { return g.physics(s).compensation * 1000.0; };
  CHECK(mm(d, 5) > 1.0 && mm(d, 5) < 3.0, spec::fmt("dreadnought high E %.2f mm", mm(d, 5)));
  CHECK(mm(d, 0) > 4.0 && mm(d, 0) < 8.0, spec::fmt("dreadnought low E %.2f mm", mm(d, 0)));
  CHECK(mm(d, 4) > mm(d, 3) && mm(d, 4) > mm(d, 5), spec::fmt("dreadnought plain B %.2f mm behind its neighbours", mm(d, 4)));
  CHECK(mm(e, 3) > mm(e, 2) && mm(e, 3) > mm(e, 4), spec::fmt("electric plain G %.2f mm behind its neighbours", mm(e, 3)));
  CHECK(mm(c, 0) > 0.5 && mm(c, 0) < 5.0 && mm(c, 5) > 0.5 && mm(c, 5) < 5.0, spec::fmt("classical %.2f and %.2f mm", mm(c, 0), mm(c, 5)));
  WHEN("fretted notes are played and read by ear");
  THEN("they sound at the pitch the physics gives, within 3 cents");
  double worst = 0.0;
  for (int f : {3, 7, 12}) for (int s : {0, 3, 5}) {
    Guitar g = make(kSingleCoil);
    g.pluck(s, f, 0.1, 5);
    const auto out = run(g, 1.3);
    worst = std::max(worst, std::abs(1200.0 * std::log2(peak_hz(out, 0.3, 1.0, g.pitch_at(s, f)) / g.pitch_at(s, f))));
  }
  CHECK(worst <= 3.0, spec::fmt("worst fretted note %.1f cents", worst));
}

SCENARIO(partials_are_stretched_by_the_strings_stiffness) {
  GIVEN("the electric's low E and plain G, open, plucked softly");
  THEN("the inharmonicity measured on partials 2 to 10 is the strings' B = pi^2 E I / (T L^2), within 40%");
  for (int s : {0, 3}) {
    Guitar g = make(kSingleCoil);
    g.pluck(s, 0, 0.1, 9);
    const auto out = run(g, 1.0);
    const double f1 = peak_hz(out, 0.05, 0.6, g.pitch_at(s, 0));
    const double b = g.inharmonicity_at(s, 0);
    double num = 0.0, den = 0.0;
    for (int n = 2; n <= 10; ++n) {
      const double fn = peak_hz(out, 0.05, 0.6, n * f1 * std::sqrt(1.0 + b * n * n), 25.0);
      const double y = std::pow(fn / (n * f1), 2.0) - 1.0;
      num += (n * n - 1) * y; den += static_cast<double>(n * n - 1) * (n * n - 1);
    }
    const double measured = num / den;
    CHECK(measured > 0.6 * b && measured < 1.4 * b, spec::fmt("string %.0f: measured B %.2e, physics %.2e", 6 - s, measured, b));
  }
}

SCENARIO(upper_partials_die_first_and_an_electric_rings_longest) {
  GIVEN("the low E plucked open on the electric and on the dreadnought");
  const auto t60 = [](const std::vector<float>& x, double hz) {
    const double a = level_at(x, 0.3, 0.3, hz), b = level_at(x, 2.3, 0.3, hz);
    return 60.0 * 2.0 / std::max(0.01, measure::db(a / b));
  };
  Guitar e = make(kSingleCoil), d = make(kDreadnought);
  e.pluck(0, 0, 0.5, 1); d.pluck(0, 0, 0.5, 1);
  const auto eo = run(e, 2.7), dout = run(d, 2.7);
  const double f = e.pitch_at(0, 0);
  const double e1 = t60(eo, f), e8 = t60(eo, 8.0 * f * std::sqrt(1.0 + 64.0 * e.inharmonicity_at(0, 0)));
  const double d1 = t60(dout, f), d8 = t60(dout, 8.0 * f * std::sqrt(1.0 + 64.0 * d.inharmonicity_at(0, 0)));
  THEN("on each, the 8th partial dies faster than the fundamental");
  CHECK(e8 < e1, spec::fmt("electric T60: fundamental %.1f s, 8th partial %.1f s", e1, e8));
  CHECK(d8 < d1, spec::fmt("dreadnought T60: fundamental %.1f s, 8th partial %.1f s", d1, d8));
  THEN("the solid body's fundamental rings for well over ten seconds, longer than the acoustic's, whose top drinks the energy");
  CHECK(e1 > 12.0 && e1 > d1, spec::fmt("electric %.1f s against acoustic %.1f s", e1, d1));
}

SCENARIO(the_pluck_point_notches_its_harmonics) {
  GIVEN("the classical's D string at the 3rd fret, plucked with a pick at the middle and then near the saddle");
  const auto second = [](double pluck_mm) {
    Guitar g = make(kClassical);
    Setup s = g.setup(); s.pluck_m = pluck_mm * 0.001; s.hardness = 1.0; g.configure(s);
    g.pluck(2, 3, 0.7, 2);
    std::vector<float> shape;
    g.string_shape(2, shape);
    // Mode amplitudes of the plucked shape, the second over the first.
    const int n = static_cast<int>(shape.size()) - 1;
    double b1 = 0.0, b2 = 0.0;
    for (int i = 0; i <= n; ++i) { b1 += shape[static_cast<size_t>(i)] * std::sin(kPi * i / n); b2 += shape[static_cast<size_t>(i)] * std::sin(2.0 * kPi * i / n); }
    return std::abs(b2 / b1);
  };
  Guitar probe = make(kClassical);
  const double middle = probe.speaking_length(2, 3) * 500.0;
  const double at_middle = second(middle), near_saddle = second(60.0);
  THEN("plucked at the middle, the second harmonic (whose node is there) is 30 dB under the same pluck near the saddle");
  CHECK(measure::db(near_saddle / std::max(at_middle, 1e-9)) > 30.0, spec::fmt("2nd/1st: %.4f at %.0f mm, %.4f at 60 mm", at_middle, middle, near_saddle));
}

SCENARIO(a_hard_pluck_starts_sharp_and_settles) {
  GIVEN("the electric's low E plucked as hard as it goes, and softly");
  Guitar hard = make(kSingleCoil), soft = make(kSingleCoil);
  hard.pluck(0, 0, 1.0, 4); soft.pluck(0, 0, 0.05, 4);
  const auto h = run(hard, 3.0), s = run(soft, 3.0);
  const double want = hard.pitch_at(0, 0);
  const double h_early = 1200.0 * std::log2(peak_hz(h, 0.02, 0.25, want) / want), h_late = 1200.0 * std::log2(peak_hz(h, 2.0, 0.9, want) / want);
  const double s_early = 1200.0 * std::log2(peak_hz(s, 0.02, 0.25, want) / want);
  THEN("the hard pluck stretches the string: it starts 4 to 30 cents sharp and falls back toward pitch as the energy goes (on a solid body, which rings for most of a minute, slowly); the soft one starts in tune");
  CHECK(h_early > 4.0 && h_early < 30.0, spec::fmt("hard pluck starts %+.1f cents", h_early));
  CHECK(h_late < 6.0 && h_late < h_early - 3.0, spec::fmt("and settles to %+.1f cents", h_late));
  CHECK(std::abs(s_early) < 2.5, spec::fmt("soft pluck %+.1f cents", s_early));
}

SCENARIO(notes_are_fingered_like_a_guitarist_and_a_string_plays_one_note) {
  GIVEN("an open E major chord, E2 B2 E3 G#3 B3 E4, played into a standard-tuned guitar one note at a time");
  Guitar g = make(kDreadnought);
  std::vector<int> strings;
  for (int note : {40, 47, 52, 56, 59, 64}) {
    const auto f = g.finger(note);
    strings.push_back(f.string);
    g.pluck(f.string, f.fret, 0.5, 1);
  }
  std::vector<int> sorted = strings;
  std::sort(sorted.begin(), sorted.end());
  THEN("all six land on different strings, low to high, in the open position");
  CHECK(std::unique(sorted.begin(), sorted.end()) == sorted.end(), "six strings");
  CHECK(strings == std::vector<int>({0, 1, 2, 3, 4, 5}), "in string order");
  CHECK(g.fret(1) == 2 && g.fret(2) == 2 && g.fret(3) == 1 && g.fret(0) == 0, "the open E shape: 0 2 2 1 0 0");
  WHEN("a note below the instrument (C2) or above it is asked for");
  CHECK(g.finger(36).note == 48 && g.finger(100).note <= 64 + 20, "it moves by octaves into range");
  WHEN("A2 rings on the open A string and the same string is then fretted at the 2nd fret (B2)");
  Guitar c = make(kSingleCoil);
  c.pluck(1, 0, 0.6, 1);
  const auto before = run(c, 0.5);
  c.pluck(1, 2, 0.6, 2);
  const auto after = run(c, 1.0);
  THEN("the A is choked: the string now sounds B2 and the A is 30 dB down");
  const double a_before = level_at(before, 0.2, 0.25, 110.0), a_after = level_at(after, 0.5, 0.25, 110.0), b_after = level_at(after, 0.5, 0.25, c.pitch_at(1, 2));
  CHECK(measure::db(a_before / std::max(a_after, 1e-9)) > 30.0, spec::fmt("A2 fell %.1f dB", measure::db(a_before / std::max(a_after, 1e-9))));
  CHECK(b_after > 10.0 * a_after, "B2 sounds");
}

SCENARIO(a_released_note_is_damped_and_open_strings_ring_in_sympathy) {
  GIVEN("A2 fretted on the low E string (5th fret) of the dreadnought, the open A string beside it untouched");
  Guitar g = make(kDreadnought);
  g.pluck(0, 5, 0.8, 1);
  const auto held = run(g, 1.0);
  const double played = level_at(held, 0.6, 0.3, 110.0);
  WHEN("with the sustain pedal down (other strings left to ring), the low E alone is stopped by the hand");
  g.pluck(5, 0, 0.0, 9);  // a feather touch on the high E: the pedal is holding it, so nothing gets muted
  g.let_ring(5);
  g.release(0, 0.05);
  const auto tail = run(g, 1.5);
  THEN("the A string, driven through the bridge while the note rang, is still sounding A2 half a second later, below the note it rang with, and falls slowly as a free open string does");
  const double at_half = level_at(tail, 0.5, 0.3, 110.0), at_one = level_at(tail, 1.1, 0.3, 110.0);
  CHECK(at_half > played * 1e-4 && at_half < played * 0.3, spec::fmt("open A at %.1f dB against the played note", measure::db(at_half / played)));
  CHECK(measure::db(at_half / std::max(at_one, 1e-12)) < 12.0, spec::fmt("free string falls %.1f dB in 0.6 s", measure::db(at_half / std::max(at_one, 1e-12))));
  GIVEN("the same note without the pedal: the key comes up and nothing is left ringing");
  Guitar h = make(kDreadnought);
  h.pluck(0, 5, 0.8, 1);
  const auto held2 = run(h, 1.0);
  h.release(0, 0.1);
  const auto tail2 = run(h, 1.5);
  THEN("the hands mute every string: a second later the guitar is 50 dB down");
  CHECK(measure::db(measure::rms(Span(held2, 28800, 9600)) / std::max(measure::rms(Span(tail2, 48000, 9600)), 1e-12)) > 50.0, "muted");
  CHECK(finite(tail) && finite(tail2), "finite");
}

SCENARIO(pickups_hear_the_string_and_a_body_radiates_it) {
  GIVEN("A3 on the G string's 2nd fret through each pickup of the single-coil, and on the dreadnought");
  const auto centroid = [](int model, int pickup) {
    Guitar g = make(model);
    Setup s = g.setup(); s.pickup = pickup; g.configure(s);
    g.pluck(3, 2, 0.6, 1);
    const auto out = run(g, 0.8);
    return measure::spectral_centroid(Span(out, 4800, 33600), kRate, 12);
  };
  const double neck = centroid(kSingleCoil, kNeckPickup), middle = centroid(kSingleCoil, kMiddlePickup), bridge = centroid(kSingleCoil, kBridgePickup);
  THEN("the bridge pickup is brighter than the middle, which is brighter than the neck: the closer to the saddle, the less the low partials move under it");
  CHECK(bridge > middle && middle > neck, spec::fmt("neck %.0f, middle %.0f, bridge %.0f Hz", neck, middle, bridge));
  THEN("the humbucker's neck pickup, its two coils 18.5 mm apart and a lower resonance, is darker than the single coil's");
  CHECK(centroid(kHumbucker, kNeckPickup) < neck, "humbucker darker");
  GIVEN("the dreadnought's low E, whose 82 Hz fundamental lies below the body's 102 Hz air resonance");
  Guitar d = make(kDreadnought);
  d.pluck(0, 0, 0.6, 1);
  const auto out = run(d, 1.0);
  const double f = d.pitch_at(0, 0);
  THEN("the body radiates its second and third harmonics more strongly than its fundamental, as a real flat-top does");
  CHECK(level_at(out, 0.1, 0.4, 2.0 * f) > level_at(out, 0.1, 0.4, f), "2nd harmonic over the fundamental");
}

SCENARIO(a_legato_note_on_the_same_string_is_a_hammer_on) {
  GIVEN("G3 ringing on the open G string of the electric, then a hammer-on to the 2nd fret");
  Guitar g = make(kSingleCoil);
  g.pluck(3, 0, 0.7, 1);
  const auto before = run(g, 0.5);
  g.legato(3, 2, 0.7, false);
  const auto after = run(g, 0.8);
  THEN("the string plays A3 without a new pluck: no attack louder than the note it came from");
  CHECK(std::abs(1200.0 * std::log2(peak_hz(after, 0.1, 0.6, g.pitch_at(3, 2)) / g.pitch_at(3, 2))) < 5.0, "A3");
  CHECK(measure::peak(Span(after, 0, 2400)) < 3.0 * measure::peak(Span(before, 21600, 2400)), "no fresh attack");
}

SCENARIO(the_whole_instrument_is_stable_in_every_tuning) {
  GIVEN("every guitar in every tuning with all six strings plucked as hard as they go at once (the bridge coupling every string to every other)");
  bool all_finite = true, all_decay = true;
  for (int model = 0; model < kModelCount; ++model)
    for (int tuning = 0; tuning < kTuningCount; ++tuning) {
      Guitar g = make(model, tuning);
      for (int s = 0; s < kStrings; ++s) { g.pluck(s, 0, 1.0, static_cast<uint32_t>(s + 1)); g.let_ring(s); }
      const auto out = run(g, 6.0);
      all_finite = all_finite && finite(out);
      const double early = measure::rms(Span(out, 0, 24000)), late = measure::rms(Span(out, out.size() - 24000, 24000));
      if (!(late < early * 0.5)) std::printf("    model %d tuning %d: %.5f -> %.5f\n", model, tuning, early, late);
      all_decay = all_decay && late < early * 0.5;
    }
  THEN("everything stays finite and dies away: nothing feeds itself through the bridge");
  CHECK(all_finite, "finite");
  CHECK(all_decay, "decays");
}

SCENARIO(the_engine_plays_the_guitar_as_one_instrument) {
  GIVEN("the engine on the physical guitar with a three-voice unison and one key");
  fuzzboi::Engine engine;
  engine.prepare(kRate);
  auto s = fuzzboi::Settings::defaults();
  s.v[fuzzboi::params::kSource] = fuzzboi::params::kSourceGuitar;
  s.v[fuzzboi::params::kUnison] = 3;
  engine.note_on(s, 0, 57, -1, 1.0);
  std::vector<float> l(4800), r(4800);
  engine.render(s, l.data(), r.data(), 4800);
  THEN("the three unison voices take three different strings, as a guitarist doubles a note");
  int held = 0;
  for (int i = 0; i < kStrings; ++i) held += engine.guitar().held(i);
  CHECK(held == 3, spec::fmt("%.0f strings held", held));
  WHEN("the key is released");
  engine.note_off(s, 0, 57, -1);
  std::vector<float> l2(48000), r2(48000);
  engine.render(s, l2.data(), r2.data(), 48000);
  THEN("the strings are damped and let go, and within a second the engine reports silence");
  held = 0;
  for (int i = 0; i < kStrings; ++i) held += engine.guitar().held(i);
  CHECK(held == 0, "no string held");
  std::vector<float> l3(96000), r3(96000);
  engine.render(s, l3.data(), r3.data(), 96000);
  CHECK(engine.silent(), "silent");
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
