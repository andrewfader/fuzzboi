// Executable scenarios for features/dsp_building_blocks.feature: the oscillators, filters,
// envelopes and helpers are proved by measuring what they output, in the units their
// parameters are stated in.
#include "../src/dsp/air.hpp"
#include "../src/dsp/core.hpp"
#include "../src/dsp/envelopes.hpp"
#include "../src/dsp/filters.hpp"
#include "../src/dsp/oscillators.hpp"
#include "../src/dsp/sampler.hpp"
#include "../src/wav.hpp"
#include "audio_measure.hpp"
#include "spec.hpp"

#include <limits>
#include <vector>

using namespace fuzzboi::dsp;
using measure::Span;

namespace {
constexpr double kRate = 48000.0;

std::vector<float> render_wave(Wave wave, double hz, size_t n, double duty = 0.5) {
  Oscillator osc;
  std::vector<float> out(n);
  for (auto& s : out) s = osc.next(wave, hz, kRate, duty);
  return out;
}
}  // namespace

SCENARIO(conversions_round_trip) {
  GIVEN("MIDI note 69 and a 6 dB gain");
  THEN("they map to 440 Hz and a factor of 2 and back");
  CHECK_NEAR(midi_to_hz(69), 440.0, 1e-9, "A4");
  CHECK_NEAR(hz_to_midi(880.0), 81.0, 1e-9, "A5 back to MIDI");
  CHECK_NEAR(db_to_gain(6.0206), 2.0, 1e-3, "+6 dB");
  CHECK_NEAR(gain_to_db(0.5), -6.0206, 1e-3, "half scale");
  CHECK_NEAR(cents_ratio(1200), 2.0, 1e-12, "an octave in cents");
}

SCENARIO(oscillators_play_the_pitch_asked_for) {
  for (const Wave wave : {Wave::Saw, Wave::Pulse, Wave::Triangle, Wave::Sine}) {
    GIVEN("an oscillator asked for 220 Hz");
    const auto out = render_wave(wave, 220.0, 24000);
    THEN("the measured pitch is within 2 cents and the level is near full scale");
    const double hz = measure::pitch_hz(Span(out, 4000, 16000), kRate);
    CHECK_NEAR(measure::cents(hz, 220.0), 0.0, 2.0, "pitch in cents");
    CHECK(measure::peak(Span(out, 4000)) > 0.8 && measure::peak(Span(out, 4000)) < 1.3, "peak level");
  }
}

SCENARIO(pulse_duty_is_the_fraction_asked_for) {
  GIVEN("a pulse at 25% duty");
  const auto out = render_wave(Wave::Pulse, 110.0, 48000, 0.25);
  THEN("the signal is high a quarter of the time");
  CHECK_NEAR(measure::duty(Span(out, 1000)), 0.25, 0.01, "duty");
}

SCENARIO(band_limited_saw_has_little_aliasing) {
  GIVEN("a saw at 4 kHz, where a naive saw would alias badly");
  const auto out = render_wave(Wave::Saw, 4000.0, 32768);
  THEN("the energy below the fundamental is tiny compared with the fundamental");
  const double f0 = measure::tone_amplitude(Span(out, 1000, 16384), kRate, 4000.0);
  const double below = measure::band_fraction(Span(out, 1000, 16384), kRate, 100.0, 3800.0);
  CHECK(f0 > 0.4, "fundamental present");
  CHECK(below < 0.02, "aliasing below the fundamental under 2% of the power");
}

SCENARIO(ladder_filter_cutoff_attenuates_above_and_passes_below) {
  GIVEN("a ladder filter at 1 kHz with no resonance");
  LadderFilter filter;
  filter.prepare(kRate);
  filter.set(1000.0, 0.0, 1.0);
  const auto probe = [&](double hz) {
    filter.reset();
    std::vector<float> out(16384);
    double phase = 0.0;
    for (auto& s : out) { s = filter.process(static_cast<float>(0.5 * std::sin(phase))); phase += kTau * hz / kRate; }
    return measure::tone_amplitude(Span(out, 4096), kRate, hz) / 0.5;
  };
  THEN("100 Hz passes, 1 kHz sits near -3 dB per pole, 8 kHz is well down");
  CHECK(probe(100.0) > 0.85, "pass band");
  const double at_cutoff = measure::db(probe(1000.0));
  CHECK(at_cutoff < -6.0 && at_cutoff > -16.0, "near the cutoff: four poles each about -3 dB");
  CHECK(measure::db(probe(8000.0)) < -40.0, "stop band at least 40 dB down three octaves up");
}

SCENARIO(ladder_resonance_self_oscillates_at_the_cutoff) {
  GIVEN("a ladder filter at 440 Hz with resonance at maximum and a tiny kick");
  LadderFilter filter;
  filter.prepare(kRate);
  filter.set(440.0, 1.0, 1.0);
  std::vector<float> out(48000);
  for (size_t i = 0; i < out.size(); ++i) out[i] = filter.process(i < 10 ? 0.5f : 0.0f);
  THEN("it rings on at the cutoff frequency");
  const double hz = measure::pitch_hz(Span(out, 24000, 16000), kRate);
  CHECK(measure::peak(Span(out, 24000)) > 0.05, "still ringing half a second later");
  CHECK_NEAR(measure::cents(hz, 440.0), 0.0, 60.0, "ring pitch within 60 cents of the cutoff");
}

SCENARIO(state_variable_band_pass_peaks_at_its_centre) {
  GIVEN("a state-variable filter at 500 Hz, Q 10");
  StateVariable svf;
  svf.prepare(kRate);
  svf.set(500.0, 10.0);
  const auto probe = [&](double hz) {
    svf.reset();
    std::vector<float> out(16384);
    double phase = 0.0;
    for (auto& s : out) { s = svf.band(static_cast<float>(0.5 * std::sin(phase))); phase += kTau * hz / kRate; }
    return measure::tone_amplitude(Span(out, 4096), kRate, hz) / 0.5;
  };
  THEN("the centre passes and an octave either side is at least 12 dB down");
  CHECK(probe(500.0) > 0.7, "centre");
  CHECK(measure::db(probe(250.0)) < -12.0 && measure::db(probe(1000.0)) < -12.0, "skirts");
}

SCENARIO(biquad_peak_boosts_only_its_band) {
  GIVEN("a +12 dB peak at 2 kHz, Q 2");
  Biquad eq;
  eq.set(Biquad::Type::Peak, 2000.0, 2.0, 12.0, kRate);
  const auto probe = [&](double hz) {
    eq.reset();
    std::vector<float> out(16384);
    double phase = 0.0;
    for (auto& s : out) { s = eq.process(static_cast<float>(0.25 * std::sin(phase))); phase += kTau * hz / kRate; }
    return measure::db(measure::tone_amplitude(Span(out, 4096), kRate, hz) / 0.25);
  };
  THEN("2 kHz gains about 12 dB and 200 Hz stays flat");
  CHECK_NEAR(probe(2000.0), 12.0, 0.5, "peak gain");
  CHECK_NEAR(probe(200.0), 0.0, 0.5, "far below the peak");
}

SCENARIO(allpass_keeps_level_and_shifts_phase) {
  GIVEN("a first-order allpass at 1 kHz");
  Allpass1 ap;
  ap.set(1000.0, kRate);
  std::vector<float> out(8192);
  double phase = 0.0;
  for (auto& s : out) { s = ap.process(static_cast<float>(std::sin(phase))); phase += kTau * 1000.0 / kRate; }
  THEN("a 1 kHz sine comes out at the same level");
  CHECK_NEAR(measure::tone_amplitude(Span(out, 2048), kRate, 1000.0), 1.0, 0.02, "unity magnitude");
}

SCENARIO(adsr_times_match_their_settings) {
  GIVEN("an ADSR at 100 ms attack, 200 ms decay, 50% sustain, 300 ms release");
  Adsr env;
  env.prepare(kRate);
  env.set(100.0, 200.0, 0.5, 300.0);
  env.note_on();
  std::vector<double> levels;
  size_t reached_full = 0, reached_sustain = 0;
  for (size_t i = 0; i < 48000; ++i) {
    levels.push_back(env.next());
    if (!reached_full && levels.back() >= 1.0) reached_full = i;
    if (!reached_sustain && env.stage() == Adsr::Stage::Sustain) reached_sustain = i;
  }
  THEN("full level arrives at about 100 ms and sustain at about 300 ms");
  CHECK_NEAR(static_cast<double>(reached_full) / kRate * 1000.0, 100.0, 5.0, "attack time");
  CHECK(reached_sustain > reached_full, "decay follows attack");
  CHECK_NEAR(levels[24000], 0.5, 1e-3, "sustain level at half a second");
  WHEN("the note is released");
  env.note_off();
  size_t silent = 0;
  for (size_t i = 0; i < 48000; ++i) { env.next(); if (!env.active()) { silent = i; break; } }
  THEN("it is 60 dB down at 300 ms and silent (-80 dB) shortly after");
  CHECK(silent > 0, "envelope ends");
  // From the 50% sustain level, -80 dB (1e-4) is ln(0.5 / 1e-4) time constants away.
  CHECK_NEAR(static_cast<double>(silent) / kRate * 1000.0, 300.0 * std::log(0.5 / 1e-4) / 6.9, 10.0, "time to -80 dB");
}

SCENARIO(delay_line_reads_back_what_was_written) {
  GIVEN("a delay line holding 100 samples");
  DelayLine line;
  line.prepare(100);
  for (int i = 0; i < 200; ++i) line.push(static_cast<float>(i));
  THEN("integer and fractional reads land where expected");
  CHECK_NEAR(line.read_int(0), 199.0, 1e-6, "most recent");
  CHECK_NEAR(line.read_int(10), 189.0, 1e-6, "ten back");
  CHECK_NEAR(line.read(10.5), 188.5, 1e-4, "fractional interpolates");
  CHECK_NEAR(line.read(1000.0), 99.0, 1e-4, "clamped to capacity");
}

SCENARIO(smoother_reaches_its_target_in_time) {
  GIVEN("a 10 ms smoother stepping from 0 to 1");
  Smoother s;
  s.set_time(10.0, kRate);
  s.snap(0.0);
  s.target(1.0);
  double at_10ms = 0.0;
  for (int i = 0; i < 480; ++i) at_10ms = s.next();
  THEN("after 10 ms it has covered about 63% of the step");
  CHECK_NEAR(at_10ms, 0.632, 0.02, "one time constant");
  for (int i = 0; i < 4800; ++i) s.next();
  CHECK(s.settled(1e-3), "settled after ten time constants");
}

SCENARIO(dc_blocker_removes_offset_and_keeps_audio) {
  GIVEN("a DC blocker at 10 Hz fed a 0.5 offset plus a 1 kHz sine");
  DcBlocker dc;
  dc.set(10.0, kRate);
  std::vector<float> out(48000);
  double phase = 0.0;
  for (auto& s : out) { s = dc.process(static_cast<float>(0.5 + 0.25 * std::sin(phase))); phase += kTau * 1000.0 / kRate; }
  THEN("the mean goes to zero and the sine is untouched");
  CHECK_NEAR(measure::mean(Span(out, 24000)), 0.0, 0.005, "offset removed");
  CHECK_NEAR(measure::tone_amplitude(Span(out, 24000, 8192), kRate, 1000.0), 0.25, 0.01, "tone level kept");
}

SCENARIO(rng_is_deterministic_and_bounded) {
  GIVEN("two generators with the same seed");
  Rng a(42), b(42);
  bool same = true, bounded = true;
  for (int i = 0; i < 10000; ++i) {
    const float x = a.bipolar(), y = b.bipolar();
    same = same && x == y;
    bounded = bounded && x >= -1.0f && x < 1.0f && a.unipolar() >= 0.0f;
    b.unipolar();
  }
  THEN("they agree sample for sample and stay in range");
  CHECK(same, "deterministic");
  CHECK(bounded, "bounded");
}

SCENARIO(wav_parser_handles_malformed_chunks_and_non_finite_floats) {
  GIVEN("a RIFF/WAVE with a chunk size that would wrap 32-bit unsigned arithmetic");
  std::vector<uint8_t> wrap = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E',
                               'J', 'U', 'N', 'K', 0xf7, 0xff, 0xff, 0xff};
  fuzzboi::wav::Audio audio;
  std::string err;
  THEN("parsing terminates and rejects it rather than looping forever");
  CHECK(!fuzzboi::wav::parse(wrap, audio, &err), "32-bit chunk-size wrap rejected");

  GIVEN("a 16-bit IEEE float WAVE (invalid bit depth for format 3)");
  std::vector<uint8_t> bad_float = {
      'R', 'I', 'F', 'F', 40, 0, 0, 0, 'W', 'A', 'V', 'E',
      'f', 'm', 't', ' ', 16, 0, 0, 0, 3, 0, 1, 0, 0x80, 0xbb, 0, 0, 0, 0x77, 1, 0, 2, 0, 16, 0,
      'd', 'a', 't', 'a', 4, 0, 0, 0, 0, 0, 0, 0};
  CHECK(!fuzzboi::wav::parse(bad_float, audio, &err), "16-bit float rejected");

  GIVEN("a 32-bit float WAVE containing NaN and +Inf samples");
  std::vector<uint8_t> nan_float = {
      'R', 'I', 'F', 'F', 48, 0, 0, 0, 'W', 'A', 'V', 'E',
      'f', 'm', 't', ' ', 16, 0, 0, 0, 3, 0, 1, 0, 0x80, 0xbb, 0, 0, 0, 0xee, 2, 0, 4, 0, 32, 0,
      'd', 'a', 't', 'a', 12, 0, 0, 0};
  const float vals[3] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 0.25f};
  for (float v : vals) {
    uint8_t b[4]; std::memcpy(b, &v, 4);
    nan_float.insert(nan_float.end(), b, b + 4);
  }
  CHECK(fuzzboi::wav::parse(nan_float, audio, &err), "32-bit float parses");
  CHECK(audio.mono.size() == 3 && audio.mono[0] == 0.0f && audio.mono[1] == 0.0f && audio.mono[2] == 0.25f,
        "non-finite floats sanitised to zero");
}

SCENARIO(sampler_stays_in_bounds_on_short_reverse_loop) {
  GIVEN("a 2-frame sample played in reverse at a high ratio");
  auto smp = std::make_shared<Sample>();
  smp->frames = {0.5f, -0.5f};
  smp->rate = 96000.0;
  smp->root_hz = 110.0;
  SamplerVoice voice;
  voice.prepare(kRate);
  SamplerSettings s;
  s.loop = true;
  s.reverse_chance = 1.0;
  s.varispeed_semitones = 12.0;
  voice.note_on(smp, s, 1u);
  bool all_finite = true;
  for (int i = 0; i < 2000; ++i) {
    const float y = voice.next(s, 3520.0, true);
    all_finite = all_finite && std::isfinite(y) && std::abs(y) <= 1.0f;
  }
  THEN("every output sample is finite and bounded");
  CHECK(all_finite, "reverse loop stays in bounds");
}

SCENARIO(room_sabine_rt60_matches_configured_room) {
  GIVEN("a configured room");
  air::Room::Settings s;
  s.volume_m3 = 600.0;
  s.absorption = 0.25;
  s.distance_m = 2.0;
  air::Room room;
  room.prepare(kRate);
  room.configure(s);
  THEN("the allocation-free Sabine RT60 matches the room's low-frequency RT60");
  CHECK_NEAR(air::Room::sabine_rt60(s, 500.0), room.rt60_low(), 1e-9, "sabine_rt60 matches");
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
