// Executable scenarios for features/sound_sources.feature: every sound source plays the pitch
// it is asked for (where it has one), carries the character it claims, and stays finite.
#include "../src/dsp/analog_synth.hpp"
#include "../src/dsp/fm_chip.hpp"
#include "../src/dsp/organ.hpp"
#include "../src/dsp/sampler.hpp"
#include "../src/dsp/strings.hpp"
#include "../src/dsp/tape.hpp"
#include "../src/dsp/tape_keys.hpp"
#include "../src/dsp/toys.hpp"
#include "audio_measure.hpp"
#include "spec.hpp"

#include <vector>

using namespace fuzzboi::dsp;
using measure::Span;

namespace {
constexpr double kRate = 48000.0;

bool finite(const std::vector<float>& x) { for (float v : x) if (!std::isfinite(v)) return false; return true; }

template <typename Fn>
std::vector<float> render(size_t n, Fn fn) {
  std::vector<float> out(n);
  for (size_t i = 0; i < n; ++i) out[i] = fn(i);
  return out;
}

void expect_pitch(const std::vector<float>& out, double hz, double tolerance_cents, const char* what, size_t from = 8000) {
  const double measured = measure::pitch_hz(Span(out, from, 16000), kRate, 30.0, 4000.0, 0.3);
  CHECK(measured > 0.0, std::string(what) + ": pitch detected");
  if (measured > 0.0) CHECK_NEAR(measure::cents(measured, hz), 0.0, tolerance_cents, std::string(what) + " pitch (cents)");
}
}  // namespace

SCENARIO(combo_organ_plays_in_tune_with_every_footage) {
  GIVEN("a combo organ at A3 with the default drawbars");
  OrganSettings s;
  OrganVoice organ; organ.prepare(kRate); organ.note_on(s);
  const auto out = render(24000, [&](size_t) { return organ.next(s, 220.0); });
  THEN("it is in tune, non-silent and finite, and the footages show up as harmonics");
  CHECK(finite(out), "finite");
  CHECK(measure::rms(Span(out, 4000)) > 0.05, "audible");
  // The 8' footage is loudest, so a pitch tracker may report 220 or the 16' period at 110.
  const double organ_hz = measure::pitch_hz(Span(out, 8000, 16000), kRate);
  const double organ_cents = measure::cents(organ_hz, 110.0);
  CHECK(std::abs(organ_cents) < 15.0 || std::abs(organ_cents - 1200.0) < 15.0, spec::fmt("organ in tune (%.0f cents from 110 Hz)", organ_cents));
  const double unison = measure::tone_amplitude(Span(out, 8000, 8192), kRate, 220.0);
  const double twelfth = measure::tone_amplitude(Span(out, 8000, 8192), kRate, 660.0);
  CHECK(unison > 0.05 && twelfth > 0.01, "8' and 2 2/3' footages present");
  WHEN("the 2 2/3' drawbar is pulled out and the others pushed in");
  s.drawbars = {0, 0, 0, 1, 0, 0}; s.reed = 0.0;
  const auto only_twelfth = render(24000, [&](size_t) { return organ.next(s, 220.0); });
  THEN("the twelfth dominates the unison");
  CHECK(measure::tone_amplitude(Span(only_twelfth, 8000, 8192), kRate, 660.0) >
        5.0 * measure::tone_amplitude(Span(only_twelfth, 8000, 8192), kRate, 220.0), "footage isolation");
}

SCENARIO(organ_reed_tab_adds_harmonics_and_repeat_gates_the_note) {
  GIVEN("the organ with only the 8' drawbar, flute then reed");
  OrganSettings s; s.drawbars = {0, 1, 0, 0, 0, 0}; s.click = 0.0; s.vibrato = 0.0;
  OrganVoice organ; organ.prepare(kRate); organ.note_on(s);
  s.reed = 0.0;
  const auto flute = render(24000, [&](size_t) { return organ.next(s, 220.0); });
  s.reed = 1.0;
  const auto reed = render(24000, [&](size_t) { return organ.next(s, 220.0); });
  THEN("the reed setting has far more energy above the fundamental");
  CHECK(measure::band_fraction(Span(reed, 8000), kRate, 500.0, 8000.0) >
        4.0 * measure::band_fraction(Span(flute, 8000), kRate, 500.0, 8000.0), "reed is brighter");
  WHEN("marimba repeat is switched on at 8 Hz");
  s.repeat = true; s.repeat_hz = 8.0;
  const auto repeated = render(48000, [&](size_t) { return organ.next(s, 220.0); });
  THEN("the level pulses at 8 Hz");
  const auto contour = measure::contour_db(repeated, kRate, 0.005, 0.010);
  std::vector<double> linear; for (double c : contour) linear.push_back(std::pow(10.0, c / 20.0));
  CHECK_NEAR(measure::modulation_hz(linear, 0.005, 2.0, 20.0), 8.0, 0.8, "repeat rate");
}

SCENARIO(analog_synth_tracks_pitch_and_filter_sweeps_with_its_envelope) {
  GIVEN("the synth with a saw into a 400 Hz filter and a big positive filter envelope");
  SynthVoice synth; synth.prepare(kRate);
  SynthSettings s; s.osc2_cents = 0; s.mix = 0.0; s.cutoff_hz = 400.0; s.resonance = 0.1; s.filter_env = 1.0;
  s.filter_attack_ms = 0; s.filter_decay_ms = 600; s.filter_sustain = 0.0; s.key_track = 0.0;
  synth.note_on(1.0);
  const auto out = render(48000, [&](size_t) { return synth.next(s, 110.0, 45.0); });
  THEN("the note is A2 and the spectrum darkens as the filter envelope falls");
  CHECK(finite(out), "finite");
  expect_pitch(out, 110.0, 15.0, "synth", 2000);
  const double early = measure::band_fraction(Span(out, 500, 8192), kRate, 2000.0, 20000.0);
  const double late = measure::band_fraction(Span(out, 36000, 8192), kRate, 2000.0, 20000.0);
  CHECK(early > 3.0 * late, spec::fmt("energy above 2 kHz falls: early %.3f, late %.3f", early, late));
}

SCENARIO(analog_synth_second_oscillator_interval_and_sub_are_audible) {
  GIVEN("osc 2 alone, a fifth up, and then the sub-octave square alone");
  SynthVoice synth; synth.prepare(kRate);
  SynthSettings s; s.mix = 1.0; s.osc2_semitones = 7; s.osc2_cents = 0; s.cutoff_hz = 18000; s.filter_env = 0; s.resonance = 0;
  synth.note_on(1.0);
  const auto fifth = render(24000, [&](size_t) { return synth.next(s, 220.0, 57.0); });
  expect_pitch(fifth, 220.0 * std::exp2(7.0 / 12.0), 20.0, "osc 2 a fifth up");
  s.mix = 0.0; s.osc1 = Wave::Sine; s.sub = 1.0;
  const auto sub = render(24000, [&](size_t) { return synth.next(s, 220.0, 57.0); });
  THEN("the sub puts the period an octave below");
  expect_pitch(sub, 110.0, 20.0, "sub octave");
}

SCENARIO(fm_chip_plays_pitch_per_channel_and_releases) {
  GIVEN("the YM2151 playing the electric piano patch on channel 0 at A4");
  FmChip chip; chip.prepare(kRate);
  FmControls c; c.patch = 0;
  chip.note_on(0, 440.0, c);
  const auto held = render(24000, [&](size_t) { return chip.render(); });
  THEN("the output is at pitch, audible and finite");
  CHECK(finite(held), "finite");
  CHECK(measure::peak(Span(held, 0, 4000)) > 0.05, "audible attack");
  expect_pitch(held, 440.0, 25.0, "FM pitch", 500);
  WHEN("the key is released with a short release");
  chip.note_off(0);
  const auto tail = render(48000, [&](size_t) { return chip.render(); });
  THEN("the sound dies away");
  CHECK(measure::peak(Span(tail, 40000)) < 0.01, "released");
  WHEN("every patch is auditioned on its own channel");
  bool all_sound = true;
  for (int p = 0; p < 8; ++p) {
    FmChip solo; solo.prepare(kRate);
    FmControls pc; pc.patch = p;
    solo.note_on(p, 261.63, pc);
    const auto out = render(12000, [&](size_t) { return solo.render(); });
    all_sound = all_sound && finite(out) && measure::peak(Span(out)) > 0.02;
  }
  THEN("each patch makes a sound");
  CHECK(all_sound, "all eight patches audible");
}

SCENARIO(fm_brightness_changes_the_spectrum) {
  GIVEN("the FM bass patch, dull and then bright");
  const auto centroid = [](double brightness) {
    FmChip chip; chip.prepare(kRate);
    FmControls c; c.patch = 2; c.brightness = brightness;
    chip.note_on(0, 110.0, c);
    const auto out = render(16000, [&](size_t) { return chip.render(); });
    return measure::spectral_centroid(Span(out, 2000, 8192), kRate);
  };
  THEN("brightness 1 has a higher centroid than brightness 0");
  const double dull = centroid(0.0), bright = centroid(1.0);
  CHECK(bright > 1.3 * dull, spec::fmt("centroid dull %.0f Hz, bright %.0f Hz", dull, bright));
}

SCENARIO(tape_key_runs_out_after_its_length) {
  GIVEN("a Mellotron key with 8 seconds of tape");
  TapeKey key; key.prepare(kRate); key.start();
  size_t stopped = 0;
  for (size_t i = 0; i < 48000 * 10; ++i) if (key.advance(1.0, 8.0) <= 0.0) { stopped = i; break; }
  THEN("it goes silent at 8 seconds and the gain stays at 1 until the last 60 ms");
  CHECK_NEAR(static_cast<double>(stopped) / kRate, 8.0, 0.01, "tape end");
  TapeKey second; second.prepare(kRate); second.start();
  bool unity = true;
  for (size_t i = 0; i < static_cast<size_t>(kRate * 7.9); ++i) unity = unity && second.advance(1.0, 8.0) == 1.0;
  CHECK(unity, "full level while tape remains");
  WHEN("the length is 9 (endless)");
  TapeKey endless; endless.prepare(kRate); endless.start();
  bool still = true;
  for (size_t i = 0; i < 48000 * 12; ++i) still = still && endless.advance(1.0, 9.0) == 1.0;
  THEN("it never stops");
  CHECK(still, "endless tape");
}

SCENARIO(tape_banks_play_in_tune_and_differ_in_character) {
  GIVEN("the three synthesized tape banks at C4");
  TapeToneGenerator gen; gen.prepare(kRate); gen.note_on();
  std::vector<std::vector<float>> banks;
  for (int b = 0; b < 3; ++b) {
    gen.prepare(kRate); gen.note_on();
    banks.push_back(render(24000, [&](size_t) { return gen.next(static_cast<TapeBank>(b), 261.63); }));
  }
  THEN("each is at pitch and finite, flute is the purest, strings the brightest");
  for (size_t b = 0; b < 3; ++b) { CHECK(finite(banks[b]), "finite"); expect_pitch(banks[b], 261.63, 30.0, (b == 0 ? "flute" : (b == 1 ? "strings" : "choir"))); }
  const double flute_high = measure::band_fraction(Span(banks[0], 8000), kRate, 1000.0, 10000.0);
  const double strings_high = measure::band_fraction(Span(banks[1], 8000), kRate, 1000.0, 10000.0);
  CHECK(strings_high > 2.0 * flute_high, "strings carry more upper harmonics than flute");
}

SCENARIO(toy_piano_rings_with_inharmonic_bar_partials) {
  GIVEN("a toy piano struck at A4 with full inharmonicity");
  ModalVoice toy; toy.prepare(kRate);
  ModalSettings s; s.inharmonic = 1.0; s.damping = 0.0; s.hammer = 0.8;
  toy.note_on(s, 440.0, 1.0);
  const auto out = render(24000, [&](size_t) { return toy.next(s); });
  THEN("the fundamental and the clamped-bar partials at 2.80x and 5.49x are present, 3x is not");
  const Span body(out, 1000, 16384);
  const double f1 = measure::tone_amplitude(body, kRate, 440.0);
  const double f280 = measure::tone_amplitude(body, kRate, 440.0 * 17.55 / 6.267);
  const double f549 = measure::tone_amplitude(body, kRate, 440.0 * 34.39 / 6.267);
  const double f3 = measure::tone_amplitude(body, kRate, 1320.0);
  CHECK(f1 > 0.01, "fundamental");
  CHECK(f280 > 0.2 * f1, spec::fmt("2.80x partial present (%.4f vs fundamental %.4f)", f280, f1));
  CHECK(f549 > 0.02 * f1, "5.49x partial present");
  CHECK(f3 < 0.15 * f280, "no harmonic third");
  WHEN("damping is turned up");
  ModalSettings damped = s; damped.damping = 1.0;
  ModalVoice toy2; toy2.prepare(kRate); toy2.note_on(damped, 440.0, 1.0);
  const auto short_out = render(24000, [&](size_t) { return toy2.next(damped); });
  THEN("the note decays faster");
  CHECK(measure::rms(Span(short_out, 12000, 8000)) < 0.3 * measure::rms(Span(out, 12000, 8000)), "shorter ring");
}

SCENARIO(music_box_is_brighter_and_shorter_than_the_toy_piano) {
  GIVEN("the same strike as a toy piano and as a music box tooth");
  ModalSettings piano; ModalSettings box; box.music_box = true;
  ModalVoice a; a.prepare(kRate); a.note_on(piano, 523.25, 1.0);
  ModalVoice b; b.prepare(kRate); b.note_on(box, 523.25, 1.0);
  const auto pa = render(24000, [&](size_t) { return a.next(piano); });
  const auto pb = render(24000, [&](size_t) { return b.next(box); });
  THEN("the music box has the higher centroid and the shorter tail");
  CHECK(measure::spectral_centroid(Span(pb, 500, 8192), kRate) > measure::spectral_centroid(Span(pa, 500, 8192), kRate), "brighter");
  CHECK(measure::rms(Span(pb, 16000)) < measure::rms(Span(pa, 16000)), "shorter");
}

SCENARIO(electric_jug_resonates_where_set_and_flutters_at_its_rate) {
  GIVEN("a jug tuned to 160 Hz fluttering at 9 Hz, sung at 110 Hz");
  JugVoice jug; jug.prepare(kRate); jug.note_on();
  JugSettings s; s.resonance_hz = 160.0; s.flutter_hz = 9.0;
  const auto out = render(96000, [&](size_t) { return jug.next(s, 110.0); });
  THEN("the output is finite and audible, pulses at 9 Hz, and the band around 160 Hz is strong");
  CHECK(finite(out), "finite");
  CHECK(measure::rms(Span(out, 4000)) > 0.02, "audible");
  const auto contour = measure::contour_db(out, kRate, 0.005, 0.010);
  std::vector<double> linear; for (double c : contour) linear.push_back(std::pow(10.0, c / 20.0));
  CHECK_NEAR(measure::modulation_hz(linear, 0.005, 3.0, 16.0), 9.0, 1.0, "flutter rate");
  const double near = measure::band_fraction(Span(out, 4000), kRate, 120.0, 220.0);
  const double far = measure::band_fraction(Span(out, 4000), kRate, 600.0, 1200.0);
  CHECK(near > far, "Helmholtz band dominates the upper band");
  WHEN("the resonance moves to 320 Hz");
  s.resonance_hz = 320.0;
  const auto high = render(48000, [&](size_t) { return jug.next(s, 110.0); });
  THEN("the spectral centroid rises");
  CHECK(measure::spectral_centroid(Span(high, 4000), kRate) > measure::spectral_centroid(Span(out, 4000), kRate), "centroid follows resonance");
}

SCENARIO(theremin_glides_between_notes_and_stacks_detuned_copies) {
  GIVEN("a theremin with a 250 ms glide, jumping from A3 to A4 legato");
  ThereminVoice th; th.prepare(kRate);
  ThereminSettings s; s.glide_ms = 250.0; s.unison = 1;
  th.note_on(220.0, false);
  for (int i = 0; i < 4800; ++i) th.next(s, 220.0);
  th.note_on(440.0, true);
  const auto glide = render(48000, [&](size_t) { return th.next(s, 440.0); });
  THEN("the pitch is still below the target after 50 ms and has arrived by 1 second");
  const double early = measure::pitch_hz(Span(glide, 1200, 2400), kRate);
  const double late = measure::pitch_hz(Span(glide, 40000, 6000), kRate);
  CHECK(early > 230.0 && early < 420.0, spec::fmt("mid-glide pitch %.1f Hz", early));
  CHECK_NEAR(measure::cents(late, 440.0), 0.0, 20.0, "arrived");
  WHEN("eight theremins are stacked");
  s.unison = 8;
  ThereminVoice stack; stack.prepare(kRate); stack.note_on(440.0, false);
  const auto thick = render(24000, [&](size_t) { return stack.next(s, 440.0); });
  THEN("the output is finite, still centred on the note, and no louder than one");
  CHECK(finite(thick), "finite");
  CHECK(measure::peak(Span(thick, 8000)) < 1.2, "bounded");
  CHECK(measure::band_fraction(Span(thick, 8000), kRate, 400.0, 480.0) > 0.8, "energy around the note");
}

SCENARIO(singing_saw_and_piccolo_and_stylophone_hold_their_pitch) {
  GIVEN("a singing saw at E4");
  SingingSawVoice saw; saw.prepare(kRate); saw.note_on(329.63, false);
  SawSettings ss;
  const auto saw_out = render(48000, [&](size_t) { return saw.next(ss, 329.63); });
  expect_pitch(saw_out, 329.63, 40.0, "singing saw (wide vibrato)", 20000);
  GIVEN("a plastic piccolo at C5 with no chaos");
  PiccoloVoice pic; pic.prepare(kRate);
  PiccoloSettings ps; ps.chaos = 0.0;
  pic.note_on(ps);
  const auto pic_out = render(24000, [&](size_t) { return pic.next(ps, 523.25); });
  expect_pitch(pic_out, 523.25, 15.0, "piccolo");
  CHECK(measure::tone_amplitude(Span(pic_out, 8000, 8192), kRate, 523.25 * 3.0) >
        measure::tone_amplitude(Span(pic_out, 8000, 8192), kRate, 523.25 * 2.0), "odd harmonics dominate the even");
  WHEN("chaos is turned up, repeated notes land on different pitches");
  ps.chaos = 1.0;
  std::vector<double> landings;
  for (int n = 0; n < 4; ++n) {
    pic.note_on(ps);
    const auto o = render(12000, [&](size_t) { return pic.next(ps, 523.25); });
    landings.push_back(measure::pitch_hz(Span(o, 2000, 8000), kRate));
  }
  double spread = 0.0;
  for (double l : landings) for (double m : landings) spread = std::max(spread, std::abs(measure::cents(l, m)));
  CHECK(spread > 10.0, spec::fmt("chaos spreads the landings by %.0f cents", spread));
  GIVEN("a Stylophone at A3");
  StylophoneVoice sty; sty.prepare(kRate);
  const auto sty_out = render(24000, [&](size_t) { return sty.next(220.0, 0.0); });
  expect_pitch(sty_out, 220.0, 10.0, "stylophone");
  const double buzz = measure::band_fraction(Span(sty_out, 8000), kRate, 1000.0, 12000.0);
  CHECK(buzz > 0.05, spec::fmt("buzzy: %.3f of the power above 1 kHz", buzz));
}

SCENARIO(jangle_strings_ring_in_pairs_and_decay_as_set) {
  GIVEN("a 12-string course plucked at A3 with a 3 s decay");
  JangleVoice jangle; jangle.prepare(kRate);
  JangleSettings s; s.decay_s = 3.0; s.octave = 0.7;
  jangle.note_on(s, 220.0, 57.0);
  const auto out = render(96000, [&](size_t) { return jangle.next(s, 220.0); });
  THEN("the pitch is A3, the octave string is heard, and the level falls over time");
  CHECK(finite(out), "finite");
  expect_pitch(out, 220.0, 25.0, "jangle", 2000);
  CHECK(measure::tone_amplitude(Span(out, 2000, 8192), kRate, 440.0 * cents_ratio(6.0)) > 0.005, "octave string");
  CHECK(measure::rms(Span(out, 60000, 16000)) < 0.5 * measure::rms(Span(out, 4000, 16000)), "decays");
  WHEN("the decay is set to 0.4 s");
  s.decay_s = 0.4;
  JangleVoice fast; fast.prepare(kRate); fast.note_on(s, 220.0, 57.0);
  const auto quick = render(48000, [&](size_t) { return fast.next(s, 220.0); });
  THEN("the note is gone within a second");
  CHECK(measure::rms(Span(quick, 40000)) < 0.1 * measure::rms(Span(quick, 2000, 8000)), "short decay");
}

SCENARIO(jangle_bends_with_its_pitch_and_the_pick_position_shapes_the_tone) {
  GIVEN("a 12-string course plucked at A3, then asked for B3 while it rings");
  JangleVoice jangle; jangle.prepare(kRate);
  JangleSettings s; s.octave = 0.0; s.pick = 0.3; s.body = 0.0;
  jangle.note_on(s, 220.0, 57.0, 1.0);
  std::vector<float> out(48000);
  for (size_t i = 0; i < out.size(); ++i) out[i] = jangle.next(s, i < 12000 ? 220.0 : 246.94);
  THEN("the ringing string follows: it plays A3 first and B3 afterwards");
  expect_pitch(out, 220.0, 25.0, "before the bend", 2000);
  const double after = measure::pitch_hz(Span(out, 16000, 12000), kRate, 30.0, 4000.0, 0.3);
  CHECK(after > 0.0 && std::abs(measure::cents(after, 246.94)) < 30.0, spec::fmt("after the bend %.1f Hz", after));
  WHEN("the pick strikes at the middle of the string instead of near the bridge");
  const auto second_harmonic_ratio = [](double pick) {
    JangleVoice j; j.prepare(kRate);
    JangleSettings t; t.octave = 0.0; t.pick = pick; t.body = 0.0; t.bright = 1.0;
    j.note_on(t, 220.0, 57.0, 1.0);
    std::vector<float> o(24000);
    for (size_t i = 0; i < o.size(); ++i) o[i] = j.next(t, 220.0);
    const auto h = measure::harmonics(Span(o, 1000, 8192), kRate, 220.0, 2);
    return h[1] / std::max(h[0], 1e-6);
  };
  const double bridge = second_harmonic_ratio(0.1), middle = second_harmonic_ratio(1.0);
  THEN("the second harmonic, which has a node there, is notched");
  CHECK(middle < 0.5 * bridge, spec::fmt("2nd/1st harmonic %.3f at the bridge, %.3f at the middle", bridge, middle));
}

SCENARIO(sampler_plays_at_the_right_speed_loops_and_collages) {
  GIVEN("a 1 kHz sine sample recorded at 48 kHz with root C4");
  auto sample = std::make_shared<Sample>();
  sample->rate = kRate; sample->root_hz = 261.63;
  sample->frames.resize(4800);
  for (size_t i = 0; i < sample->frames.size(); ++i) sample->frames[i] = static_cast<float>(0.5 * std::sin(kTau * 1000.0 * static_cast<double>(i) / kRate));
  SamplerVoice voice; voice.prepare(kRate);
  SamplerSettings s; s.loop = true;
  voice.note_on(sample, s, 1);
  const auto at_root = render(24000, [&](size_t) { return voice.next(s, 261.63, false); });
  THEN("at the root note it plays back at 1 kHz and loops past its end");
  expect_pitch(at_root, 1000.0, 10.0, "root playback");
  CHECK(measure::rms(Span(at_root, 12000)) > 0.3, "still playing after the sample length (looping)");
  WHEN("played an octave up");
  voice.note_on(sample, s, 1);
  const auto octave = render(24000, [&](size_t) { return voice.next(s, 523.25, false); });
  expect_pitch(octave, 2000.0, 10.0, "octave up doubles the speed");
  WHEN("looping is off");
  s.loop = false;
  voice.note_on(sample, s, 1);
  const auto once = render(24000, [&](size_t) { return voice.next(s, 261.63, false); });
  THEN("it stops at the end");
  CHECK(measure::peak(Span(once, 6000)) == 0.0 && voice.finished(), "one shot ends");
  WHEN("collage mode chops with certainty into 50 ms slices with a reverse chance");
  auto ramp = std::make_shared<Sample>(*sample);
  for (size_t i = 0; i < ramp->frames.size(); ++i) ramp->frames[i] = static_cast<float>(i) / 4800.0f;
  s.loop = true; s.slice_ms = 50.0; s.slice_chance = 1.0; s.reverse_chance = 0.5;
  voice.note_on(ramp, s, 7);
  const auto chopped = render(24000, [&](size_t) { return voice.next(s, 261.63, true); });
  THEN("the output jumps at slice boundaries rather than ramping smoothly");
  int jumps = 0;
  for (size_t i = 1; i < chopped.size(); ++i) if (std::abs(chopped[i] - chopped[i - 1]) > 0.05f) ++jumps;
  CHECK(jumps >= 5, spec::fmt("%d discontinuities from re-slicing", jumps));
  CHECK(finite(chopped), "finite");
}

SCENARIO(found_sound_generator_fills_an_empty_slot) {
  GIVEN("the built-in found-sound generator with two seeds");
  const auto a = generate_found_sound(1, kRate, 2.0), b = generate_found_sound(2, kRate, 2.0);
  THEN("both are audible, finite, bounded and different from each other");
  CHECK(a->frames.size() == static_cast<size_t>(kRate * 2.0), "length");
  CHECK(finite(a->frames) && measure::peak(Span(a->frames)) <= 1.0, "bounded");
  CHECK(measure::rms(Span(a->frames)) > 0.02, "audible");
  CHECK(measure::correlation(Span(a->frames), Span(b->frames)) < 0.9, "seeds differ");
  CHECK(measure::peak(Span(a->frames, a->frames.size() - 100)) < 0.05, "tape stop at the end");
}

SCENARIO(wow_and_flutter_modulate_speed_within_their_depths) {
  GIVEN("a wow/flutter generator at full depth");
  WowFlutter wf; wf.prepare(kRate);
  double lo = 2.0, hi = 0.0;
  for (int i = 0; i < 480000; ++i) { const double s = wf.next(1.0, 1.0); lo = std::min(lo, s); hi = std::max(hi, s); }
  THEN("the speed stays within about 1.5% of nominal and does swing both ways");
  CHECK(hi < 1.016 && lo > 0.984, spec::fmt("range %.4f .. %.4f", lo, hi));
  CHECK(hi > 1.008 && lo < 0.992, "actually modulates");
  WHEN("depths are zero");
  bool steady = true;
  for (int i = 0; i < 48000; ++i) steady = steady && wf.next(0.0, 0.0) == 1.0;
  THEN("the speed is exactly 1");
  CHECK(steady, "no modulation");
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
