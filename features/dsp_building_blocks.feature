Feature: DSP building blocks are measured against their stated units
  The oscillators, filters, envelopes and helpers underneath every sound are proved by
  rendering them and measuring the output in the unit the parameter is stated in: Hz and
  cents for pitch, dB for level, milliseconds for time.

  Executable: tests/dsp_tests.cpp (CTest dsp_tests).

  # conversions_round_trip
  Scenario: Note, gain and cents conversions round trip
    Given MIDI note 69 and a 6 dB gain
    Then they map to 440 Hz and a factor of 2 and back

  # oscillators_play_the_pitch_asked_for
  Scenario: Every oscillator shape plays the pitch asked for
    Given an oscillator asked for 220 Hz
    Then the measured pitch is within 2 cents and the level is near full scale

  # pulse_duty_is_the_fraction_asked_for
  Scenario: Pulse duty is the fraction asked for
    Given a pulse at 25% duty
    Then the signal is high a quarter of the time

  # band_limited_saw_has_little_aliasing
  Scenario: The band-limited saw does not alias
    Given a saw at 4 kHz, where a naive saw would alias badly
    Then the energy below the fundamental is tiny compared with the fundamental

  # ladder_filter_cutoff_attenuates_above_and_passes_below
  Scenario: The ladder filter passes below its cutoff and stops above
    Given a ladder filter at 1 kHz with no resonance
    Then 100 Hz passes, 1 kHz sits near -3 dB per pole, 8 kHz is well down

  # ladder_resonance_self_oscillates_at_the_cutoff
  Scenario: Full resonance self-oscillates at the cutoff
    Given a ladder filter at 440 Hz with resonance at maximum and a tiny kick
    Then it rings on at the cutoff frequency

  # state_variable_band_pass_peaks_at_its_centre
  Scenario: The state-variable band-pass peaks at its centre
    Given a state-variable filter at 500 Hz, Q 10
    Then the centre passes and an octave either side is at least 12 dB down

  # biquad_peak_boosts_only_its_band
  Scenario: A peaking equaliser boosts only its band
    Given a +12 dB peak at 2 kHz, Q 2
    Then 2 kHz gains about 12 dB and 200 Hz stays flat

  # allpass_keeps_level_and_shifts_phase
  Scenario: An allpass keeps the level
    Given a first-order allpass at 1 kHz
    Then a 1 kHz sine comes out at the same level

  # adsr_times_match_their_settings
  Scenario: ADSR stage times are the milliseconds set
    Given an ADSR at 100 ms attack, 200 ms decay, 50% sustain, 300 ms release
    Then full level arrives at about 100 ms and sustain at about 300 ms
    When the note is released
    Then it is 60 dB down at 300 ms and silent (-80 dB) shortly after

  # delay_line_reads_back_what_was_written
  Scenario: The delay line reads back exactly and interpolates
    Given a delay line holding 100 samples
    Then integer and fractional reads land where expected

  # smoother_reaches_its_target_in_time
  Scenario: The parameter smoother follows its time constant
    Given a 10 ms smoother stepping from 0 to 1
    Then after 10 ms it has covered about 63% of the step

  # dc_blocker_removes_offset_and_keeps_audio
  Scenario: The DC blocker removes offset and keeps the audio
    Given a DC blocker at 10 Hz fed a 0.5 offset plus a 1 kHz sine
    Then the mean goes to zero and the sine is untouched

  # rng_is_deterministic_and_bounded
  Scenario: The random generator is repeatable
    Given two generators with the same seed
    Then they agree sample for sample and stay in range
