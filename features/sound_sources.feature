Feature: Every sound source plays in tune and carries the character it claims
  FUZZBOI's sources are small models of real instruments: a transistor combo organ, a two
  oscillator analog synth, a YM2151 FM chip, Mellotron-style tape keys, a toy piano and music
  box built from inharmonic bar modes, the 13th Floor Elevators' electric jug, a theremin, a
  singing saw, a plastic piccolo, a Stylophone, a 12-string's plucked courses and a collage
  sampler. Each is proved from rendered audio.

  Executable: tests/sources_tests.cpp (CTest sources_tests).

  # combo_organ_plays_in_tune_with_every_footage
  Scenario: The combo organ is in tune and its drawbars isolate footages
    Given a combo organ at A3 with the default drawbars
    Then it is in tune, non-silent and finite, and the footages show up as harmonics
    When the 2 2/3' drawbar is pulled out and the others pushed in
    Then the twelfth dominates the unison

  # organ_reed_tab_adds_harmonics_and_repeat_gates_the_note
  Scenario: The reed tab brightens and marimba repeat pulses at its rate
    Given the organ with only the 8' drawbar, flute then reed
    Then the reed setting has far more energy above the fundamental
    When marimba repeat is switched on at 8 Hz
    Then the level pulses at 8 Hz

  # analog_synth_tracks_pitch_and_filter_sweeps_with_its_envelope
  Scenario: The analog synth tracks pitch and its filter envelope sweeps
    Given the synth with a saw into a 400 Hz filter and a big positive filter envelope
    Then the note is A2 and the spectrum darkens as the filter envelope falls

  # analog_synth_second_oscillator_interval_and_sub_are_audible
  Scenario: Oscillator 2's interval and the sub-octave are audible
    Given osc 2 alone, a fifth up, and then the sub-octave square alone
    Then the sub puts the period an octave below

  # fm_chip_plays_pitch_per_channel_and_releases
  Scenario: The FM chip plays at pitch, releases, and every patch sounds
    Given the YM2151 playing the electric piano patch on channel 0 at A4
    Then the output is at pitch, audible and finite
    When the key is released with a short release
    Then the sound dies away
    When every patch is auditioned on its own channel
    Then each patch makes a sound

  # fm_brightness_changes_the_spectrum
  Scenario: FM brightness drives the modulators harder
    Given the FM bass patch, dull and then bright
    Then brightness 1 has a higher centroid than brightness 0

  # tape_key_runs_out_after_its_length
  Scenario: A tape key runs out after its length, or never when endless
    Given a Mellotron key with 8 seconds of tape
    Then it goes silent at 8 seconds and the gain stays at 1 until the last 60 ms
    When the length is 9 (endless)
    Then it never stops

  # tape_banks_play_in_tune_and_differ_in_character
  Scenario: The flute, strings and choir banks are in tune and distinct
    Given the three synthesized tape banks at C4
    Then each is at pitch and finite, flute is the purest, strings the brightest

  # toy_piano_rings_with_inharmonic_bar_partials
  Scenario: The toy piano rings with clamped-bar partials, not harmonics
    Given a toy piano struck at A4 with full inharmonicity
    Then the fundamental and the clamped-bar partials at 2.80x and 5.49x are present, 3x is not
    When damping is turned up
    Then the note decays faster

  # music_box_is_brighter_and_shorter_than_the_toy_piano
  Scenario: The music box is brighter and shorter than the toy piano
    Given the same strike as a toy piano and as a music box tooth
    Then the music box has the higher centroid and the shorter tail

  # electric_jug_resonates_where_set_and_flutters_at_its_rate
  Scenario: The electric jug resonates where set and flutters at its rate
    Given a jug tuned to 160 Hz fluttering at 9 Hz, sung at 110 Hz
    Then the output is finite and audible, pulses at 9 Hz, and the band around 160 Hz is strong
    When the resonance moves to 320 Hz
    Then the spectral centroid rises

  # theremin_glides_between_notes_and_stacks_detuned_copies
  Scenario: The theremin glides and stacks a ton of theremins
    Given a theremin with a 250 ms glide, jumping from A3 to A4 legato
    Then the pitch is still below the target after 50 ms and has arrived by 1 second
    When eight theremins are stacked
    Then the output is finite, still centred on the note, and no louder than one

  # singing_saw_and_piccolo_and_stylophone_hold_their_pitch
  Scenario: The saw, the piccolo and the Stylophone hold their pitch
    Given a singing saw at E4
    Given a plastic piccolo at C5 with no chaos
    When chaos is turned up, repeated notes land on different pitches
    Given a Stylophone at A3

  # jangle_strings_ring_in_pairs_and_decay_as_set
  Scenario: The 12-string course rings in a pair and decays as set
    Given a 12-string course plucked at A3 with a 3 s decay
    Then the pitch is A3, the octave string is heard, and the level falls over time
    When the decay is set to 0.4 s
    Then the note is gone within a second

  # jangle_bends_with_its_pitch_and_the_pick_position_shapes_the_tone
  Scenario: The jangle bends with its pitch and the pick position shapes the tone
    Given a 12-string course plucked at A3, then asked for B3 while it rings
    Then the ringing string follows: it plays A3 first and B3 afterwards
    When the pick strikes at the middle of the string instead of near the bridge
    Then the second harmonic, which has a node there, is notched

  # sampler_plays_at_the_right_speed_loops_and_collages
  Scenario: The sampler plays at the right speed, loops, stops, and collages
    Given a 1 kHz sine sample recorded at 48 kHz with root C4
    Then at the root note it plays back at 1 kHz and loops past its end
    When played an octave up
    When looping is off
    Then it stops at the end
    When collage mode chops with certainty into 50 ms slices with a reverse chance
    Then the output jumps at slice boundaries rather than ramping smoothly

  # found_sound_generator_fills_an_empty_slot
  Scenario: The found-sound generator gives an empty slot something to play
    Given the built-in found-sound generator with two seeds
    Then both are audible, finite, bounded and different from each other

  # wow_and_flutter_modulate_speed_within_their_depths
  Scenario: Wow and flutter stay within their depths
    Given a wow/flutter generator at full depth
    Then the speed stays within about 1.5% of nominal and does swing both ways
    When depths are zero
    Then the speed is exactly 1
