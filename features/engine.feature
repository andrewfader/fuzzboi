Feature: The engine plays notes polyphonically, in mono and legato, with samples and presets
  Executable: tests/engine_tests.cpp (CTest engine_tests).

  # a_note_makes_a_sound_and_releases
  Scenario: A note sounds while held and ends after its release
    Given the default organ, one note held for half a second
    Then the note sounds while held, then fades to silence within the release and the voice ends

  # polyphony_stacks_and_steals_oldest
  Scenario: Sixteen voices stack and the oldest is stolen
    Given sixteen voices, with eighteen notes played
    Then only sixteen are active and the engine stays finite
    When every key is released
    Then all voices end

  # mono_mode_retriggers_and_legato_glides
  Scenario: Legato glides between held notes and returns to the held key
    Given legato mode with a 200 ms glide, C4 then G4 while C4 is held
    Then one voice is active and the pitch is between the notes shortly after, then at G4
    When G4 is released with C4 still held
    Then the voice returns to C4

  # unison_spreads_voices_across_the_stereo_field
  Scenario: Unison voices spread across the stereo field
    Given four unison voices with full stereo spread
    Then four voices play and the channels differ
    When spread is zero
    Then the channels are identical

  # fm_source_plays_through_the_shared_chip
  Scenario: FM voices share one eight-channel chip
    Given the FM source with three notes
    Then the chip sounds and the notes end on release
    When nine FM notes are held at once
    Then the engine survives the chip's eight-channel limit

  # collage_sampler_plays_found_sound_when_the_bank_is_empty
  Scenario: The collage sampler always has something to play
    Given the collage source with nothing loaded
    Then it still makes a sound, from the built-in found-sound bank
    When a user sample is loaded into the slot for C4 and the note is played again
    Then the user's sample plays at its own pitch

  # sustain_pedal_holds_released_notes
  Scenario: The sustain pedal holds released notes
    Given the pedal down, a short note played and released
    Then the note keeps sounding
    When the pedal comes up
    Then the note releases

  # pitch_bend_moves_the_note_by_the_bend_range
  Scenario: Pitch bend moves the note by the bend range
    Given a 2 semitone bend range and the wheel fully up on the synth
    Then the pitch is 200 cents sharp

  # every_preset_meets_the_quality_bar
  Scenario: Every factory preset meets the quality bar (docs/QUALITY.md)
    Given each factory preset played the audit phrase: a four-note chord at playing velocities, then three melody notes
    Then every one is finite, within 2.5 dB of the target loudness (1.5 dB in the audit, a decibel of slack without the bank), under the ceiling with the clip light off (the limiter holding no more than 9 dB), audible and free of DC

  # output_limiter_holds_the_ceiling_and_leaves_quiet_signals_alone
  Scenario: The output limiter holds the ceiling and leaves quiet signals alone
    Given the fuzzed combo organ with the master at +12 dB, then the same at -12 dB
    Then the hot one peaks exactly at the -2 dBFS ceiling with the clip light on and the limiter reporting its reduction; the quiet one is untouched and the gain below the ceiling is linear

  # every_source_plays_one_note_at_a_comparable_level
  Scenario: Every source plays one note at a comparable level
    Given one C4 at full velocity through every source, tape bank and FM patch, master at 0 dB
    Then every one peaks between -11 and -3.5 dBFS and holds between -30 and -11 dBFS RMS

  # chord_machine_turns_one_key_into_a_voiced_chord
  Scenario: The chord machine turns one key into a voiced chord
    Given the theremin (a near sine) with the chord machine set to Major, C4 played
    Then three voices play C, E and G, and nothing plays the minor third
    When the key is released
    Then every chord voice releases with it
    When the voicing is Open
    Then a bass note sounds an octave below the root

  # strum_spreads_chord_notes_over_time_and_stops_with_the_key
  Scenario: A strum spreads the chord's notes over time and stops with the key
    Given a Major chord with a 100 ms strum on the theremin
    Then only the root sounds at once and the other two notes wait
    When 60 ms pass, then 60 more, then 100 more
    Then the second note arrives after 100 ms and the third after 200 ms
    When a key is released before its strum finishes
    Then the notes that had not sounded are dropped and the engine falls silent

  # tail_estimate_covers_the_actual_decay
  Scenario: The advertised tail covers the decay
    Given a preset with a long echo and reverb
    Then after the advertised tail the output is quiet

  # tempo_synced_echo_follows_the_host_tempo
  Scenario: Tempo-synced echo follows the host tempo
    Given echo synced to 1/4 notes at 120 BPM, fed a short click through the organ
    Then the repeat arrives half a second later
