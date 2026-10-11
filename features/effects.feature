Feature: Each effect does what its name and knobs advertise
  The pedalboard (fuzz, cabinet, phaser, chorus and flanger, rotary, tremolo), the echoes (tape echo, spring,
  chamber and its shimmer, wall of sound) and the master tape section (lo-fi, backwards, feedback, pump, tape machine)
  are each proved from audio.

  Executable: tests/fx_tests.cpp (CTest fx_tests).

  # fuzz_adds_harmonics_and_flattens_dynamics
  Scenario: Every fuzz mode (Muff, Face, Bender, Plexi, Octave) adds harmonics, squashes dynamics and bypasses at zero
    Given a clean 220 Hz sine at two levels, through each fuzz mode
    Then harmonics appear, a 14 dB input change becomes much less, bypass is bit-exact, output is bounded

  # fuzz_tone_tilts_the_spectrum
  Scenario: The fuzz tone control tilts the spectrum
    Given the Muff on a saw-like buzz, tone dark then bright
    Then the bright setting has the higher centroid

  # phaser_moves_notches_and_bypasses_cleanly
  Scenario: The phaser sweeps its notches
    Given a phaser on white noise at 0.5 Hz with eight stages
    Then the spectrum differs between two moments a second apart (the notches moved)
    When mix is zero
    Then the input passes untouched

  # rotary_spins_the_level_at_the_horn_speed
  Scenario: The rotary speaker spins at the horn speed
    Given a rotary speaker at 6 Hz on a bright steady tone
    Then the left channel's level pulses at about 6 Hz and the channels differ

  # tremolo_depth_and_rate_are_what_the_knobs_say
  Scenario: Tremolo depth and rate are what the knobs say
    Given a sine tremolo at 5 Hz, depth 1, on a 1 kHz tone
    Then the level swings at 5 Hz over a wide range
    When depth is zero
    Then nothing changes

  # tape_echo_repeats_at_the_set_time_and_decays_by_feedback
  Scenario: The tape echo repeats at its time and its heads sit at 1x, 1.5x, 2x
    Given a tape echo at 300 ms, 50% feedback, head 1, no wow, fed one impulse
    Then the first repeat lands at 300 ms and the second, quieter, at 600 ms
    When heads 1+2+3 are selected
    Then taps appear at 300, 450 and 600 ms

  # tape_echo_runaway_feedback_stays_bounded
  Scenario: Runaway feedback self-oscillates without blowing up
    Given feedback above unity with saturation
    Then the loop keeps sounding but never blows up

  # spring_reverb_produces_a_dispersive_tail
  Scenario: The spring reverb has a dispersive tail that drip deepens
    Given a spring reverb fed an impulse
    Then there is energy well after the impulse that dies away, and it is finite
    When the drip is at maximum
    Then the first echo is smeared longer in time than with no drip

  # chamber_reverb_tail_length_follows_size
  Scenario: The chamber's tail follows its size and predelay
    Given the chamber at small and large size, fed an impulse
    Then the large room rings longer than the small one, and both have a tail
    When predelay is 100 ms
    Then nothing arrives before 100 ms

  # wall_of_sound_doubles_and_squashes
  Scenario: The wall of sound doubles, sends to the chamber and squashes
    Given the wall on a noise bed that drops 20 dB halfway
    Then the drop is reduced by the squash and the chamber send carries signal

  # lofi_quantises_and_holds_samples
  Scenario: Lo-fi quantises levels and holds samples
    Given lo-fi at 4 bits and 8 kHz on a slow ramp
    Then the output takes only a handful of distinct values and changes at most 8000 times a second
    When set to 16 bits at full rate
    Then it is bypassed exactly

  # backwards_plays_each_chunk_reversed
  Scenario: Backwards plays each chunk reversed
    Given backwards with 100 ms chunks on a rising ramp
    Then inside a chunk the output falls while the input rises

  # feedback_voice_swells_in_at_a_harmonic_of_the_note
  Scenario: The feedback voice swells in on the chosen harmonic
    Given feedback at full amount on the 2nd harmonic of A3 with a 400 ms swell, fed a quiet tone
    Then it grows louder over the first second and the loudest component is at 440 Hz
    When the amount is zero
    Then the signal is untouched

  # tape_machine_limits_bandwidth_and_generations_dull_the_sound
  Scenario: The tape machine limits bandwidth and generations dull the sound
    Given the master tape machine at 5 kHz bandwidth on white noise
    Then energy above 10 kHz is well down compared with the input
    When six cassette generations are stacked at full bandwidth
    Then the result is duller still and remains finite
    When everything is at zero and a 1 kHz tone goes through
    Then the tone passes nearly unchanged (only the gentle head loss and DC blocker remain)

  # chorus_shimmers_the_pitch_and_widens_the_image
  Scenario: The Juno chorus shimmers the pitch and widens the image
    Given a Juno I chorus at 0.5 Hz, full depth and mix, on a 440 Hz tone
    Then the added copy's pitch sweeps above and below the tone, the two sides differ, and it is finite
    When the mix is zero
    Then the input passes untouched
    When the Ensemble mode is chosen instead
    Then the result differs from Juno I and stays bounded

  # cabinet_rolls_off_the_top_and_bypasses_cleanly
  Scenario: The speaker cabinet rolls off the top and bypasses cleanly
    Given a British cabinet at full mix on white noise
    Then the energy above 8 kHz is well down, the presence region is not, and it is finite
    When the mix is zero
    Then the input passes untouched
    When the tweed voicing is chosen instead
    Then it is darker than the British one

  # pump_ducks_on_every_beat_and_recovers_before_the_next
  Scenario: The pump ducks on every beat and recovers before the next
    Given a pump at full depth on a half-second beat, fed a steady level
    Then the level dives right after each beat and is back near full just before the next
    When the depth is zero
    Then the signal passes untouched

  # octave_fuzz_puts_the_octave_above_the_note_on_top
  Scenario: The Octave fuzz puts the octave above the note on top
    Given a 220 Hz sine through the Octave fuzz and through the Muff
    Then the Octave's 440 Hz component is at least twice its 220 Hz one, where the Muff's is far below

  # plexi_adds_even_harmonics_and_keeps_more_dynamics
  Scenario: The Plexi amp adds even harmonics and cleans up with the input level
    Given a 220 Hz sine through the Plexi and through the symmetric Muff, loud and 14 dB quieter
    Then the biased triodes give a second harmonic within 20 dB of the fundamental, the Muff's is lower still
    When both are at 60% and the input drops 14 dB
    Then the amp keeps more of the drop than the pedal: it cleans up with the guitar's volume

  # flanger_sweeps_a_deep_comb
  Scenario: The flanger is a deep comb that sweeps
    Given the flanger at zero depth (a fixed 0.3 ms delay) on white noise
    Then the comb's first notch, at 1 / (2 x 0.3 ms) = 1.67 kHz, sits more than 15 dB under the peak at 3.3 kHz
    When the depth is full and the sweep runs at 0.25 Hz
    Then the spectrum two seconds apart (half a sweep) has moved, and it is finite and bounded

  # pitch_shifter_moves_a_tone_up_an_octave
  Scenario: The shimmer's pitch shifter moves a tone up an octave
    Given a 300 Hz sine through the shimmer's pitch shifter at a ratio of 2
    Then the output's pitch is 600 Hz to within 15 cents, and the 600 Hz line beats the 300 Hz one by 20 dB

  # shimmer_climbs_octaves_in_the_tail_and_still_dies_away
  Scenario: The shimmer climbs octaves in the reverb tail and still dies away
    Given a one-second 220 Hz note into a large chamber, without and with full shimmer
    Then in the tail the octave (440 Hz) and the octave above it (880 Hz) are each at least 10 dB stronger
    Then the loop never runs away: the last second is 50 dB under the tail's start, and every sample is finite
    When the room is the largest and undamped, the case closest to ringing forever
    Then it still dies away: the last second is 50 dB under the tail's start
