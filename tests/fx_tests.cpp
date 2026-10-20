// Executable scenarios for features/effects.feature: each effect does what its name and knobs
// advertise, measured from audio.
#include "../src/dsp/fx/echoes.hpp"
#include "../src/dsp/fx/master.hpp"
#include "../src/dsp/fx/pedals.hpp"
#include "audio_measure.hpp"
#include "spec.hpp"

#include <vector>

using namespace fuzzboi::dsp;
using measure::Span;

namespace {
constexpr double kRate = 48000.0;
bool finite(const std::vector<float>& x) { for (float v : x) if (!std::isfinite(v)) return false; return true; }
std::vector<float> sine(double hz, size_t n, double amplitude = 0.5) {
  std::vector<float> out(n);
  for (size_t i = 0; i < n; ++i) out[i] = static_cast<float>(amplitude * std::sin(kTau * hz * static_cast<double>(i) / kRate));
  return out;
}
}  // namespace

SCENARIO(the_wah_sweeps_its_peak_from_heel_to_toe) {
  GIVEN("the wah fully in, held at the heel and then at the toe, fed sines from 200 Hz to 5 kHz in twelfth-octave steps");
  THEN("its peak sits within 10% of 450 Hz at the heel and of 2.2 kHz at the toe, as Keen measured the Cry Baby, and stands over the dry level");
  for (auto [pedal, want] : {std::pair{0.0, 450.0}, std::pair{1.0, 2200.0}}) {
    double best_hz = 0.0, best = 0.0;
    for (double hz = 200.0; hz <= 5000.0; hz *= std::exp2(1.0 / 12.0)) {
      Wah wah; wah.prepare(kRate);
      const auto in = sine(hz, static_cast<size_t>(0.1 * kRate), 0.4);
      std::vector<float> out(in.size());
      for (size_t i = 0; i < in.size(); ++i) out[i] = wah.process(in[i], 1.0, pedal, 0.0);
      const double gain = measure::rms(Span(out, out.size() / 2)) / measure::rms(Span(in, in.size() / 2));
      if (gain > best) { best = gain; best_hz = hz; }
    }
    CHECK(std::abs(best_hz / want - 1.0) <= 0.10, spec::fmt("pedal %.0f: peak at %.0f Hz", pedal, best_hz));
    CHECK(best > 1.0, spec::fmt("peak gain %.2f over the dry level", best));
  }
}

SCENARIO(fuzz_adds_harmonics_and_flattens_dynamics) {
  GIVEN("a clean 220 Hz sine at two levels, through each fuzz mode");
  for (int mode = 0; mode < 5; ++mode) {
    Fuzz fuzz; fuzz.prepare(kRate);
    const auto loud_in = sine(220.0, 24000, 0.5), quiet_in = sine(220.0, 24000, 0.1);
    std::vector<float> loud(24000), quiet(24000), bypass(24000);
    for (size_t i = 0; i < 24000; ++i) loud[i] = fuzz.process(loud_in[i], 1.0, 0.5, 0.0, static_cast<FuzzMode>(mode));
    fuzz.prepare(kRate);
    for (size_t i = 0; i < 24000; ++i) quiet[i] = fuzz.process(quiet_in[i], 1.0, 0.5, 0.0, static_cast<FuzzMode>(mode));
    fuzz.prepare(kRate);
    for (size_t i = 0; i < 24000; ++i) bypass[i] = fuzz.process(loud_in[i], 0.0, 0.5, 0.0, static_cast<FuzzMode>(mode));
    THEN("harmonics appear, a 14 dB input change becomes much less, bypass is bit-exact, output is bounded");
    const Span body(loud, 8000, 8192);
    const auto h = measure::harmonics(body, kRate, 220.0, 7);
    double upper = 0.0; for (size_t k = 1; k < h.size(); ++k) upper += h[k];
    CHECK(upper > 0.3 * h[0], spec::fmt("mode %d: harmonics %.3f vs fundamental %.3f", mode, upper, h[0]));
    const double squash = measure::db(measure::rms(Span(loud, 8000))) - measure::db(measure::rms(Span(quiet, 8000)));
    CHECK(squash < 10.0, spec::fmt("mode %d: 14 dB in becomes %.1f dB out", mode, squash));
    CHECK(bypass == loud_in, "bypass at zero");
    CHECK(finite(loud) && measure::peak(Span(loud)) < 2.0, "bounded");
  }
}

SCENARIO(fuzz_tone_tilts_the_spectrum) {
  GIVEN("the Muff on a saw-like buzz, tone dark then bright");
  const auto centroid = [](double tone) {
    Fuzz fuzz; fuzz.prepare(kRate);
    std::vector<float> out(24000);
    for (size_t i = 0; i < out.size(); ++i) out[i] = fuzz.process(static_cast<float>(0.4 * (2.0 * std::fmod(110.0 * static_cast<double>(i) / kRate, 1.0) - 1.0)), 1.0, tone, 0.0, FuzzMode::Muff);
    return measure::spectral_centroid(Span(out, 8000), kRate);
  };
  THEN("the bright setting has the higher centroid");
  CHECK(centroid(1.0) > 1.5 * centroid(0.0), "tone works");
}

SCENARIO(phaser_moves_notches_and_bypasses_cleanly) {
  GIVEN("a phaser on white noise at 0.5 Hz with eight stages");
  Phaser phaser; phaser.prepare(kRate);
  Rng rng(3);
  std::vector<float> in(96000), out(96000);
  for (auto& x : in) x = rng.bipolar() * 0.3f;
  for (size_t i = 0; i < in.size(); ++i) out[i] = phaser.process(in[i], 1.0, 0.5, 1.0, 0.5, 3);
  THEN("the spectrum differs between two moments a second apart (the notches moved)");
  const auto a = measure::power_spectrum(Span(out, 0, 16384), 12), b = measure::power_spectrum(Span(out, 48000, 16384), 12);
  double diff = 0.0, total = 0.0;
  for (size_t k = 2; k < a.size() / 4; ++k) { diff += std::abs(std::log(a[k] + 1e-12) - std::log(b[k] + 1e-12)); total += 1.0; }
  CHECK(diff / total > 0.3, spec::fmt("mean log-spectral change %.2f", diff / total));
  CHECK(finite(out), "finite");
  WHEN("mix is zero");
  Phaser dry; dry.prepare(kRate);
  bool exact = true;
  for (size_t i = 0; i < 4800; ++i) exact = exact && dry.process(in[i], 0.0, 0.5, 1.0, 0.5, 3) == in[i];
  THEN("the input passes untouched");
  CHECK(exact, "bypass");
}

SCENARIO(rotary_spins_the_level_at_the_horn_speed) {
  GIVEN("a rotary speaker at 6 Hz on a bright steady tone");
  Rotary rotary; rotary.prepare(kRate);
  const auto in = sine(2000.0, 96000, 0.5);
  std::vector<float> left(in.size()), right(in.size());
  for (size_t i = 0; i < in.size(); ++i) { const auto s = rotary.process(in[i], 1.0, 6.0); left[i] = s.left; right[i] = s.right; }
  THEN("the left channel's level pulses at about 6 Hz and the channels differ");
  const auto contour = measure::contour_db(left, kRate, 0.005, 0.010);
  std::vector<double> linear(contour.begin() + 100, contour.end());
  for (double& c : linear) c = std::pow(10.0, c / 20.0);
  CHECK_NEAR(measure::modulation_hz(linear, 0.005, 3.0, 12.0), 6.0, 0.7, "horn speed");
  CHECK(measure::correlation(Span(left, 48000), Span(right, 48000)) < 0.99, "stereo movement");
}

SCENARIO(tremolo_depth_and_rate_are_what_the_knobs_say) {
  GIVEN("a sine tremolo at 5 Hz, depth 1, on a 1 kHz tone");
  Tremolo trem; trem.prepare(kRate);
  const auto in = sine(1000.0, 96000, 0.5);
  std::vector<float> out(in.size());
  for (size_t i = 0; i < in.size(); ++i) out[i] = trem.process(in[i], 1.0, 5.0, TremoloShape::Sine);
  THEN("the level swings at 5 Hz over a wide range");
  const auto contour = measure::contour_db(out, kRate, 0.005, 0.010);
  std::vector<double> linear;
  for (double c : contour) linear.push_back(std::pow(10.0, c / 20.0));
  CHECK_NEAR(measure::modulation_hz(linear, 0.005, 2.0, 12.0), 5.0, 0.5, "rate");
  double lo = 1e9, hi = -1e9;
  for (size_t i = 100; i < contour.size(); ++i) { lo = std::min(lo, contour[i]); hi = std::max(hi, contour[i]); }
  CHECK(hi - lo > 20.0, spec::fmt("depth swing %.1f dB", hi - lo));
  WHEN("depth is zero");
  std::vector<float> dry(4800);
  for (size_t i = 0; i < dry.size(); ++i) dry[i] = trem.process(in[i], 0.0, 5.0, TremoloShape::Square);
  THEN("nothing changes");
  CHECK(std::equal(dry.begin(), dry.end(), in.begin()), "bypass");
}

SCENARIO(tape_echo_repeats_at_the_set_time_and_decays_by_feedback) {
  GIVEN("a tape echo at 300 ms, 50% feedback, head 1, no wow, fed one impulse");
  TapeEcho echo; echo.prepare(kRate);
  // Let the time smoother settle on 300 ms before the impulse.
  for (int i = 0; i < 48000; ++i) echo.process(0.0f, 1.0, 0.3, 0.5, 0, 0.0, 0.0, 0.0);
  std::vector<float> out(96000);
  for (size_t i = 0; i < out.size(); ++i) out[i] = echo.process(i == 0 ? 1.0f : 0.0f, 1.0, 0.3, 0.5, 0, 0.0, 0.0, 0.0);
  THEN("the first repeat lands at 300 ms and the second, quieter, at 600 ms");
  size_t first = 0, second = 0;
  for (size_t i = 1000; i < out.size(); ++i) if (std::abs(out[i]) > 0.2f) { first = i; break; }
  for (size_t i = first + 2000; i < out.size(); ++i) if (std::abs(out[i]) > 0.08f) { second = i; break; }
  CHECK_NEAR(static_cast<double>(first) / kRate, 0.3, 0.004, "first repeat");
  CHECK_NEAR(static_cast<double>(second) / kRate, 0.6, 0.006, "second repeat");
  CHECK(std::abs(out[second]) < std::abs(out[first]), "repeats decay");
  WHEN("heads 1+2+3 are selected");
  TapeEcho three; three.prepare(kRate);
  for (int i = 0; i < 48000; ++i) three.process(0.0f, 1.0, 0.3, 0.0, 6, 0.0, 0.0, 0.0);
  std::vector<float> multi(48000);
  for (size_t i = 0; i < multi.size(); ++i) multi[i] = three.process(i == 0 ? 1.0f : 0.0f, 1.0, 0.3, 0.0, 6, 0.0, 0.0, 0.0);
  THEN("taps appear at 300, 450 and 600 ms");
  const auto tap = [&](double s) { return measure::peak(Span(multi, static_cast<size_t>(s * kRate) - 3, 8)) > 0.1; };
  CHECK(tap(0.3) && tap(0.45) && tap(0.6), "three heads");
}

SCENARIO(tape_echo_runaway_feedback_stays_bounded) {
  GIVEN("feedback above unity with saturation");
  TapeEcho echo; echo.prepare(kRate);
  std::vector<float> out(96000);
  for (size_t i = 0; i < out.size(); ++i) out[i] = echo.process(i < 100 ? 0.5f : 0.0f, 1.0, 0.1, 1.1, 0, 0.0, 1.0, 0.0);
  THEN("the loop keeps sounding but never blows up");
  CHECK(finite(out) && measure::peak(Span(out)) < 3.0, "bounded");
  CHECK(measure::rms(Span(out, 80000)) > 0.01, "still oscillating after 1.6 s");
}

SCENARIO(spring_reverb_produces_a_dispersive_tail) {
  GIVEN("a spring reverb fed an impulse");
  SpringReverb spring; spring.prepare(kRate);
  std::vector<float> out(96000);
  for (size_t i = 0; i < out.size(); ++i) out[i] = spring.process(i == 0 ? 1.0f : 0.0f, 1.0, 0.5, 0.5);
  THEN("there is energy well after the impulse that dies away, and it is finite");
  CHECK(finite(out), "finite");
  CHECK(measure::rms(Span(out, 4800, 9600)) > 1e-4, "early tail");
  CHECK(measure::rms(Span(out, 72000, 24000)) < measure::rms(Span(out, 4800, 9600)), "decays");
  WHEN("the drip is at maximum");
  SpringReverb drippy; drippy.prepare(kRate);
  std::vector<float> drip(96000);
  for (size_t i = 0; i < drip.size(); ++i) drip[i] = drippy.process(i == 0 ? 1.0f : 0.0f, 1.0, 0.5, 1.0);
  THEN("the first echo is smeared longer in time than with no drip");
  SpringReverb dry; dry.prepare(kRate);
  std::vector<float> tight(96000);
  for (size_t i = 0; i < tight.size(); ++i) tight[i] = dry.process(i == 0 ? 1.0f : 0.0f, 1.0, 0.5, 0.0);
  const auto width = [&](const std::vector<float>& x) {
    const auto c = measure::contour_db(x, kRate, 0.002, 0.004);
    const long first = measure::first_at_or_above(c, -40.0, 5), last = measure::last_at_or_above(c, -40.0);
    return static_cast<double>(last - first);
  };
  CHECK(width(drip) >= width(tight), "more dispersion with more drip");
}

SCENARIO(chamber_reverb_tail_length_follows_size) {
  GIVEN("the chamber at small and large size, fed an impulse");
  const auto rt = [](double size) {
    Chamber ch; ch.prepare(kRate);
    std::vector<float> out(144000);
    for (size_t i = 0; i < out.size(); ++i) out[i] = ch.process(i == 0 ? 1.0f : 0.0f, 1.0, size, 0.4, 0.0).left;
    const auto c = measure::contour_db(out, kRate, 0.01, 0.02);
    double peak = -200.0; for (double v : c) peak = std::max(peak, v);
    return static_cast<double>(measure::last_at_or_above(c, peak - 40.0)) * 0.01;
  };
  THEN("the large room rings longer than the small one, and both have a tail");
  const double small = rt(0.0), large = rt(1.0);
  CHECK(small > 0.1, spec::fmt("small tail %.2f s", small));
  CHECK(large > 1.5 * small, spec::fmt("large tail %.2f s vs small %.2f s", large, small));
  WHEN("predelay is 100 ms");
  Chamber ch; ch.prepare(kRate);
  std::vector<float> out(24000);
  for (size_t i = 0; i < out.size(); ++i) out[i] = ch.process(i == 0 ? 1.0f : 0.0f, 1.0, 0.5, 0.4, 100.0).left;
  THEN("nothing arrives before 100 ms");
  CHECK(measure::peak(Span(out, 1, 4700)) == 0.0f, "silent before the predelay");
  CHECK(measure::peak(Span(out, 4800, 4000)) > 0.0f, "reverb after it");
}

SCENARIO(wall_of_sound_doubles_and_squashes) {
  GIVEN("the wall on a noise bed that drops 20 dB halfway");
  WallOfSound wall; wall.prepare(kRate);
  Rng rng(5);
  std::vector<float> in(96000), out(96000);
  for (size_t i = 0; i < in.size(); ++i) in[i] = static_cast<float>((i < 48000 ? 0.5 : 0.05) * rng.bipolar());
  float send = 0.0f, max_send = 0.0f;
  for (size_t i = 0; i < in.size(); ++i) { out[i] = wall.process(in[i], 1.0, 1.0, 1.0, &send); max_send = std::max(max_send, std::abs(send)); }
  THEN("the drop is reduced by the squash and the chamber send carries signal");
  const double drop_in = measure::db(measure::rms(Span(in, 24000, 16000))) - measure::db(measure::rms(Span(in, 72000, 16000)));
  const double drop_out = measure::db(measure::rms(Span(out, 24000, 16000))) - measure::db(measure::rms(Span(out, 72000, 16000)));
  CHECK(drop_out < drop_in - 6.0, spec::fmt("drop in %.1f dB, out %.1f dB", drop_in, drop_out));
  CHECK(max_send > 0.05f, "send to the chamber");
  CHECK(finite(out), "finite");
}

SCENARIO(lofi_quantises_and_holds_samples) {
  GIVEN("lo-fi at 4 bits and 8 kHz on a slow ramp");
  LoFi lofi; lofi.prepare(kRate);
  std::vector<float> out(48000);
  for (size_t i = 0; i < out.size(); ++i) out[i] = lofi.process(static_cast<float>(i) / 48000.0f, 4, 8000.0);
  THEN("the output takes only a handful of distinct values and changes at most 8000 times a second");
  std::vector<float> distinct;
  int changes = 0;
  for (size_t i = 1; i < out.size(); ++i) {
    if (out[i] != out[i - 1]) ++changes;
    if (std::find(distinct.begin(), distinct.end(), out[i]) == distinct.end()) distinct.push_back(out[i]);
  }
  CHECK(distinct.size() <= 9, spec::fmt("%d distinct levels", static_cast<double>(distinct.size())));
  CHECK(changes <= 8000, spec::fmt("%d changes in one second", changes));
  WHEN("set to 16 bits at full rate");
  bool exact = true;
  for (size_t i = 0; i < 1000; ++i) exact = exact && lofi.process(0.123f, 16, kRate) == 0.123f;
  THEN("it is bypassed exactly");
  CHECK(exact, "bypass");
}

SCENARIO(backwards_plays_each_chunk_reversed) {
  GIVEN("backwards with 100 ms chunks on a rising ramp");
  Backwards back; back.prepare(kRate);
  std::vector<float> out(48000);
  for (size_t i = 0; i < out.size(); ++i) out[i] = back.process(static_cast<float>(i) / 48000.0f, 1.0, 100.0);
  THEN("inside a chunk the output falls while the input rises");
  int falling = 0, rising = 0;
  for (size_t i = 24000 + 500; i < 24000 + 4300; ++i) { if (out[i] < out[i - 1]) ++falling; else if (out[i] > out[i - 1]) ++rising; }
  CHECK(falling > 5 * rising, spec::fmt("falling %d, rising %d", falling, rising));
  CHECK(finite(out), "finite");
}

SCENARIO(feedback_voice_swells_in_at_a_harmonic_of_the_note) {
  GIVEN("feedback at full amount on the 2nd harmonic of A3 with a 400 ms swell, fed a quiet tone");
  FeedbackVoice fb; fb.prepare(kRate);
  std::vector<float> out(96000);
  for (size_t i = 0; i < out.size(); ++i) out[i] = fb.process(static_cast<float>(0.05 * std::sin(kTau * 220.0 * static_cast<double>(i) / kRate)), 1.0, 1, 400.0, 220.0);
  THEN("it grows louder over the first second and the loudest component is at 440 Hz");
  CHECK(measure::rms(Span(out, 60000, 16000)) > 3.0 * measure::rms(Span(out, 1000, 4000)), "swell");
  const Span late(out, 60000, 16384);
  const double howl_hz = measure::pitch_hz(late, kRate);
  CHECK_NEAR(measure::cents(howl_hz, 440.0), 0.0, 25.0, "howl lands on the second harmonic");
  CHECK(finite(out) && measure::peak(Span(out)) < 2.0, "bounded");
  WHEN("the amount is zero");
  FeedbackVoice off; off.prepare(kRate);
  bool exact = true;
  for (size_t i = 0; i < 4800; ++i) { const float x = static_cast<float>(0.3 * std::sin(kTau * 220.0 * static_cast<double>(i) / kRate)); exact = exact && off.process(x, 0.0, 1, 400.0, 220.0) == x; }
  THEN("the signal is untouched");
  CHECK(exact, "bypass");
}

SCENARIO(tape_machine_limits_bandwidth_and_generations_dull_the_sound) {
  GIVEN("the master tape machine at 5 kHz bandwidth on white noise");
  Rng rng(9);
  std::vector<float> in(48000);
  for (auto& x : in) x = rng.bipolar() * 0.3f;
  TapeMachine machine; machine.prepare(kRate);
  std::vector<float> out(in.size());
  for (size_t i = 0; i < in.size(); ++i) out[i] = machine.process(in[i], 0.0, 0.0, 0.0, 0.0, 5000.0, 0);
  THEN("energy above 10 kHz is well down compared with the input");
  const double in_high = measure::band_fraction(Span(in, 8000), kRate, 10000.0, 20000.0);
  const double out_high = measure::band_fraction(Span(out, 8000), kRate, 10000.0, 20000.0);
  CHECK(out_high < 0.3 * in_high, spec::fmt("high band %.3f -> %.3f", in_high, out_high));
  WHEN("six cassette generations are stacked at full bandwidth");
  TapeMachine copies; copies.prepare(kRate);
  std::vector<float> dub(in.size());
  for (size_t i = 0; i < in.size(); ++i) dub[i] = copies.process(in[i], 0.0, 0.0, 0.0, 0.0, 20000.0, 6);
  THEN("the result is duller still and remains finite");
  CHECK(measure::spectral_centroid(Span(dub, 8000), kRate) < 0.6 * measure::spectral_centroid(Span(in, 8000), kRate), "generations dull");
  CHECK(finite(dub), "finite");
  WHEN("everything is at zero and a 1 kHz tone goes through");
  TapeMachine clean; clean.prepare(kRate);
  const auto tone = sine(1000.0, 12000, 0.5);
  bool close = true;
  for (size_t i = 0; i < tone.size(); ++i) { const float y = clean.process(tone[i], 0.0, 0.0, 0.0, 0.0, 20000.0, 0); if (i > 4000) close = close && std::abs(y - tone[i]) < 0.03f; }
  THEN("the tone passes nearly unchanged (only the gentle head loss and DC blocker remain)");
  CHECK(close, "near bypass");
}

SCENARIO(chorus_shimmers_the_pitch_and_widens_the_image) {
  GIVEN("a Juno I chorus at 0.5 Hz, full depth and mix, on a 440 Hz tone");
  Chorus chorus; chorus.prepare(kRate);
  const auto in = sine(440.0, 144000, 0.5);
  std::vector<float> left(in.size()), right(in.size()), wet(in.size());
  for (size_t i = 0; i < in.size(); ++i) {
    const auto out = chorus.process(in[i], in[i], 1.0, 0.5, 1.0, ChorusMode::JunoI);
    left[i] = out.left; right[i] = out.right;
    wet[i] = out.left - in[i] * 0.7f;  // what the chorus added on the left
  }
  THEN("the added copy's pitch sweeps above and below the tone, the two sides differ, and it is finite");
  const auto track = measure::pitch_track(wet, kRate, 0.01, 0.04, 300.0, 600.0);
  double lowest = 1e9, highest = 0.0;
  for (size_t i = 20; i < track.size(); ++i) if (track[i] > 0.0) { lowest = std::min(lowest, track[i]); highest = std::max(highest, track[i]); }
  CHECK(measure::cents(highest, 440.0) > 4.0 && measure::cents(lowest, 440.0) < -4.0, spec::fmt("wet pitch sweeps %.1f to %.1f Hz", lowest, highest));
  CHECK(measure::correlation(Span(left, 48000, 48000), Span(right, 48000, 48000)) < 0.995, "sides differ");
  CHECK(finite(left) && finite(right) && measure::peak(Span(left)) < 1.5, "bounded");
  WHEN("the mix is zero");
  Chorus off; off.prepare(kRate);
  bool exact = true;
  for (size_t i = 0; i < 4800; ++i) { const auto out = off.process(in[i], in[i], 0.0, 0.5, 1.0, ChorusMode::Ensemble); exact = exact && out.left == in[i] && out.right == in[i]; }
  THEN("the input passes untouched");
  CHECK(exact, "bypass");
  WHEN("the Ensemble mode is chosen instead");
  Chorus ensemble; ensemble.prepare(kRate);
  std::vector<float> ens(in.size());
  for (size_t i = 0; i < in.size(); ++i) ens[i] = ensemble.process(in[i], in[i], 1.0, 0.5, 1.0, ChorusMode::Ensemble).left;
  THEN("the result differs from Juno I and stays bounded");
  CHECK(finite(ens) && measure::peak(Span(ens)) < 1.5, "bounded");
  CHECK(measure::correlation(Span(ens, 48000, 48000), Span(left, 48000, 48000)) < 0.999, "a different chorus");
}

SCENARIO(pump_ducks_on_every_beat_and_recovers_before_the_next) {
  GIVEN("a pump at full depth on a half-second beat, fed a steady level");
  Pump pump; pump.prepare(kRate);
  std::vector<float> gain(96000);
  for (size_t i = 0; i < gain.size(); ++i) gain[i] = pump.process(1.0f, 1.0, 0.5);
  THEN("the level dives right after each beat and is back near full just before the next");
  const size_t beat = 24000;
  CHECK(gain[beat + 240] < 0.3f, spec::fmt("gain %.2f just after the beat", gain[beat + 240]));
  CHECK(gain[2 * beat - 240] > 0.85f, spec::fmt("gain %.2f just before the next beat", gain[2 * beat - 240]));
  CHECK(gain[2 * beat + 240] < 0.3f && gain[3 * beat - 240] > 0.85f, "the pattern repeats every beat");
  // The duck is a swell, not a step: no sample-to-sample jump larger than a few percent.
  float jump = 0.0f;
  for (size_t i = 1; i < gain.size(); ++i) jump = std::max(jump, std::abs(gain[i] - gain[i - 1]));
  CHECK(jump < 0.05f, spec::fmt("largest step %.3f", jump));
  WHEN("the depth is zero");
  Pump off; off.prepare(kRate);
  bool exact = true;
  for (size_t i = 0; i < 48000; ++i) exact = exact && off.process(0.25f, 0.0, 0.5) == 0.25f;
  THEN("the signal passes untouched");
  CHECK(exact, "bypass");
}

SCENARIO(cabinet_rolls_off_the_top_and_bypasses_cleanly) {
  GIVEN("a British cabinet at full mix on white noise");
  Cabinet cab; cab.prepare(kRate);
  Rng rng(0x5ca81e7u);
  std::vector<float> in(96000), out(96000), tweed(96000);
  for (auto& x : in) x = rng.bipolar() * 0.3f;
  for (size_t i = 0; i < in.size(); ++i) out[i] = cab.process(in[i], 1.0, CabType::British);
  THEN("the energy above 8 kHz is well down, the presence region is not, and it is finite");
  const double top_in = measure::band_fraction(Span(in, 4800), kRate, 8000.0, 20000.0), top_out = measure::band_fraction(Span(out, 4800), kRate, 8000.0, 20000.0);
  CHECK(top_out < 0.25 * top_in, spec::fmt("top fraction %.3f -> %.3f", top_in, top_out));
  CHECK(measure::band_fraction(Span(out, 4800), kRate, 2000.0, 4000.0) > measure::band_fraction(Span(in, 4800), kRate, 2000.0, 4000.0), "presence stays");
  CHECK(finite(out), "finite");
  WHEN("the mix is zero");
  Cabinet off; off.prepare(kRate);
  bool exact = true;
  for (size_t i = 0; i < 4800; ++i) exact = exact && off.process(in[i], 0.0, CabType::Tweed) == in[i];
  THEN("the input passes untouched");
  CHECK(exact, "bypass");
  WHEN("the tweed voicing is chosen instead");
  Cabinet warm; warm.prepare(kRate);
  for (size_t i = 0; i < in.size(); ++i) tweed[i] = warm.process(in[i], 1.0, CabType::Tweed);
  THEN("it is darker than the British one");
  CHECK(measure::spectral_centroid(Span(tweed, 4800), kRate) < measure::spectral_centroid(Span(out, 4800), kRate), "tweed darker");
}

// A 220 Hz sine through one fuzz mode at full amount, with the harmonic amplitudes of the body.
std::vector<double> fuzz_harmonics(FuzzMode mode, double amplitude) {
  Fuzz fuzz; fuzz.prepare(kRate);
  const auto in = sine(220.0, 24000, amplitude);
  std::vector<float> out(in.size());
  for (size_t i = 0; i < in.size(); ++i) out[i] = fuzz.process(in[i], 1.0, 0.5, 0.0, mode);
  return measure::harmonics(Span(out, 8000, 8192), kRate, 220.0, 4);
}

SCENARIO(octave_fuzz_puts_the_octave_above_the_note_on_top) {
  GIVEN("a 220 Hz sine through the Octave fuzz and through the Muff");
  const auto octave = fuzz_harmonics(FuzzMode::Octave, 0.5), muff = fuzz_harmonics(FuzzMode::Muff, 0.5);
  THEN("the Octave's 440 Hz component is at least twice its 220 Hz one, where the Muff's is far below");
  CHECK(octave[1] > 2.0 * octave[0], spec::fmt("octave: 440 Hz %.3f vs 220 Hz %.3f", octave[1], octave[0]));
  CHECK(muff[1] < 0.5 * muff[0], spec::fmt("muff: 440 Hz %.3f vs 220 Hz %.3f", muff[1], muff[0]));
}

SCENARIO(plexi_adds_even_harmonics_and_keeps_more_dynamics) {
  GIVEN("a 220 Hz sine through the Plexi and through the symmetric Muff, loud and 14 dB quieter");
  const auto plexi = fuzz_harmonics(FuzzMode::Plexi, 0.5), muff = fuzz_harmonics(FuzzMode::Muff, 0.5);
  THEN("the biased triodes give a second harmonic within 20 dB of the fundamental, the Muff's is lower still");
  const double plexi_2nd = measure::db(plexi[1] / plexi[0]), muff_2nd = measure::db(muff[1] / muff[0]);
  CHECK(plexi_2nd > -20.0, spec::fmt("plexi 2nd harmonic %.1f dB", plexi_2nd));
  CHECK(plexi_2nd > muff_2nd + 6.0, spec::fmt("plexi %.1f dB vs muff %.1f dB", plexi_2nd, muff_2nd));
  const auto squash = [](FuzzMode mode) {
    Fuzz fuzz; fuzz.prepare(kRate);
    const auto loud_in = sine(220.0, 24000, 0.5), quiet_in = sine(220.0, 24000, 0.1);
    std::vector<float> loud(24000), quiet(24000);
    for (size_t i = 0; i < 24000; ++i) loud[i] = fuzz.process(loud_in[i], 0.6, 0.5, 0.0, mode);
    fuzz.prepare(kRate);
    for (size_t i = 0; i < 24000; ++i) quiet[i] = fuzz.process(quiet_in[i], 0.6, 0.5, 0.0, mode);
    return measure::db(measure::rms(Span(loud, 8000))) - measure::db(measure::rms(Span(quiet, 8000)));
  };
  WHEN("both are at 60% and the input drops 14 dB");
  THEN("the amp keeps more of the drop than the pedal: it cleans up with the guitar's volume");
  CHECK(squash(FuzzMode::Plexi) > squash(FuzzMode::Muff) + 1.0, spec::fmt("plexi keeps %.1f dB, muff %.1f dB", squash(FuzzMode::Plexi), squash(FuzzMode::Muff)));
}

SCENARIO(flanger_sweeps_a_deep_comb) {
  GIVEN("the flanger at zero depth (a fixed 0.3 ms delay) on white noise");
  Rng rng(0xf1a9u);
  std::vector<float> in(96000);
  for (auto& x : in) x = rng.bipolar() * 0.3f;
  Chorus fixed; fixed.prepare(kRate);
  std::vector<float> still(in.size());
  for (size_t i = 0; i < in.size(); ++i) still[i] = fixed.process(in[i], in[i], 1.0, 0.25, 0.0, ChorusMode::Flanger).left;
  THEN("the comb's first notch, at 1 / (2 x 0.3 ms) = 1.67 kHz, sits more than 15 dB under the peak at 3.3 kHz");
  // Power per hertz in a 300 Hz band around each, so the two bands weigh the same.
  const double notch = measure::band_fraction(Span(still, 4800), kRate, 1520.0, 1820.0), peak = measure::band_fraction(Span(still, 4800), kRate, 3180.0, 3480.0);
  const double depth_db = 10.0 * std::log10(peak / std::max(notch, 1e-12));
  CHECK(depth_db > 15.0, spec::fmt("peak/notch %.1f dB", depth_db));
  WHEN("the depth is full and the sweep runs at 0.25 Hz");
  Chorus swept; swept.prepare(kRate);
  std::vector<float> out(in.size());
  for (size_t i = 0; i < in.size(); ++i) out[i] = swept.process(in[i], in[i], 1.0, 0.25, 1.0, ChorusMode::Flanger).left;
  THEN("the spectrum two seconds apart (half a sweep) has moved, and it is finite and bounded");
  const auto a = measure::power_spectrum(Span(out, 0, 4096), 12), b = measure::power_spectrum(Span(out, 96000 - 4096 - 1, 4096), 12);
  double diff = 0.0, total = 0.0;
  for (size_t k = 2; k < a.size() / 4; ++k) { diff += std::abs(std::log(a[k] + 1e-12) - std::log(b[k] + 1e-12)); total += 1.0; }
  CHECK(diff / total > 0.3, spec::fmt("mean log-spectral change %.2f", diff / total));
  CHECK(finite(out) && measure::peak(Span(out)) < 1.5, "bounded");
}

SCENARIO(pitch_shifter_moves_a_tone_up_an_octave) {
  GIVEN("a 300 Hz sine through the shimmer's pitch shifter at a ratio of 2");
  PitchShifter shifter; shifter.prepare(kRate);
  const auto in = sine(300.0, 48000, 0.5);
  std::vector<float> out(in.size());
  for (size_t i = 0; i < in.size(); ++i) out[i] = shifter.process(in[i], 2.0);
  THEN("the output's pitch is 600 Hz to within 15 cents, and the 600 Hz line beats the 300 Hz one by 20 dB");
  const double hz = measure::pitch_hz(Span(out, 9600, 16384), kRate, 100.0, 2000.0);
  CHECK(std::abs(measure::cents(hz, 600.0)) < 15.0, spec::fmt("pitch %.1f Hz", hz));
  const double up = measure::tone_amplitude(Span(out, 9600, 16384), kRate, 600.0), left = measure::tone_amplitude(Span(out, 9600, 16384), kRate, 300.0);
  CHECK(measure::db(up / std::max(left, 1e-9)) > 20.0, spec::fmt("600 Hz %.4f vs 300 Hz %.4f", up, left));
}

// The engine's routing of the chamber and the shimmer loop, for one 220 Hz note and its tail.
std::vector<float> shimmer_room(double amount, double seconds, double size = 0.9, double damping = 0.3) {
  Chamber chamber; chamber.prepare(kRate);
  Shimmer shimmer; shimmer.prepare(kRate);
  const auto note = sine(220.0, 48000, 0.5);
  std::vector<float> out(static_cast<size_t>(seconds * kRate));
  float back = 0.0f;
  for (size_t i = 0; i < out.size(); ++i) {
    const float in = i < note.size() ? note[i] : 0.0f;
    const auto wet = chamber.process(in + back, 1.0, size, damping, 20.0);
    const auto octave = shimmer.process((wet.left + wet.right) * 0.5f, amount, size);
    back = octave.feed;
    out[i] = (wet.left + wet.right) * 0.5f + octave.halo;
  }
  return out;
}

SCENARIO(shimmer_climbs_octaves_in_the_tail_and_still_dies_away) {
  GIVEN("a one-second 220 Hz note into a large chamber, without and with full shimmer");
  const auto plain = shimmer_room(0.0, 20.0), shimmer = shimmer_room(1.0, 20.0);
  THEN("in the tail the octave (440 Hz) and the octave above it (880 Hz) are each at least 10 dB stronger");
  const Span plain_tail(plain, 72000, 32768), shimmer_tail(shimmer, 72000, 32768);
  for (double hz : {440.0, 880.0}) {
    const double gain = measure::db(measure::tone_amplitude(shimmer_tail, kRate, hz) / std::max(measure::tone_amplitude(plain_tail, kRate, hz), 1e-9));
    CHECK(gain > 10.0, spec::fmt("%.0f Hz rose %.1f dB", hz, gain));
  }
  THEN("the loop never runs away: the last second is 50 dB under the tail's start, and every sample is finite");
  const double start = measure::rms(Span(shimmer, 48000, 48000)), end = measure::rms(Span(shimmer, shimmer.size() - 48000, 48000));
  CHECK(measure::db(end / start) < -50.0, spec::fmt("last second %.1f dB under the start", measure::db(end / start)));
  CHECK(finite(shimmer), "finite");
  WHEN("the room is the largest and undamped, the case closest to ringing forever");
  const auto worst = shimmer_room(1.0, 30.0, 1.0, 0.0);
  THEN("it still dies away: the last second is 50 dB under the tail's start");
  const double worst_drop = measure::db(measure::rms(Span(worst, worst.size() - 48000, 48000)) / measure::rms(Span(worst, 48000, 48000)));
  CHECK(worst_drop < -50.0 && finite(worst), spec::fmt("last second %.1f dB under the start", worst_drop));
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
