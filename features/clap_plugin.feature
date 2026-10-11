Feature: FUZZBOI is a well-behaved CLAP instrument
  The built .clap is loaded the way a host loads it and driven only through the public CLAP
  ABI: discovery, instantiation, activation, processing, sample-accurate events, parameters
  and state streams.

  Executables: tests/plugin_smoke.cpp (plugin_smoke), tests/note_tests.cpp (note_tests),
  tests/state_tests.cpp (state_tests).

  # the_module_exports_a_compatible_entry_and_one_instrument
  Scenario: The module exports a compatible entry and one instrument
    Given the built plug-in module
    Then its descriptor names FUZZBOI as a stereo instrument

  # an_instance_offers_the_core_extensions_and_ports
  Scenario: An instance offers the core extensions and ports
    Given a created and initialised instance
    Then audio out, note in, params, state, tail, latency and voice info are all there

  # parameters_are_consistent_with_their_declared_ranges
  Scenario: Parameters are consistent with their declared ranges
    Given the parameter list as the host sees it
    Then every parameter has a unique id and name, a default inside its range, and formats and parses its default

  # the_instance_activates_processes_silence_and_sleeps
  Scenario: The instance activates and processes silence
    Given an activated instance with no notes
    Then a block of silence comes out and the output is finite

  # two_instances_are_independent
  Scenario: Two instances are independent
    Given two instances from one module
    Then a change in one does not touch the other

  # a_clap_note_on_makes_sound_and_note_off_stops_it
  Scenario: A CLAP note on makes sound and note off stops it
    Given an instance with a 50 ms release, given a CLAP note on
    Then audio appears
    When the note is released by id
    Then it falls silent and the plug-in asks to sleep

  # midi_notes_controllers_and_bend_are_honoured
  Scenario: MIDI notes, sustain, bend and all-notes-off are honoured
    Given MIDI note on, then pitch bend up, then note off
    Then the pitch is C4, then an octave up with the wheel fully up
    When the note is released while the sustain pedal is down, then the pedal comes up
    Then the note ends only when the pedal is released
    When all notes off (CC 123) arrives during a chord
    Then nothing is left sounding

  # events_are_applied_at_their_sample_offset
  Scenario: Events are applied at their sample offset
    Given a note on scheduled 200 samples into a 256-sample block with an instant attack
    Then the first 200 samples are silent and sound starts right after

  # parameter_automation_changes_the_sound_and_flush_works_offline
  Scenario: Automation changes the sound and flush works offline
    Given the master level set by a parameter event inside process, then by flush
    Then the level drops by about 24 dB
    When a parameter arrives through flush
    Then the plug-in reports it
    When an out-of-range value arrives
    Then it is clamped to the declared range

  # choosing_a_preset_rewrites_the_other_parameters
  Scenario: Choosing a preset rewrites the other parameters
    Given the preset parameter set to 'Electric jug'
    Then the source becomes the jug and the preset's effects are set
    When a parameter is edited afterwards and the same preset index is sent again
    Then the edit survives (re-sending the current preset is not a reload)

  # every_source_sounds_through_the_plugin
  Scenario: Every source sounds through the plug-in
    Given each sound source selected by parameter, with one note
    Then all thirteen are audible and finite

  # tail_reports_longer_with_echo_and_reverb
  Scenario: The tail grows with echo and reverb
    Given the tail extension with and without a long echo
    Then the wet tail is longer and both are sane

  # state_round_trips_every_parameter
  Scenario: State round-trips every parameter
    Given an instance with every parameter set away from its default
    When the state is loaded into a fresh instance
    Then every value matches and saving again gives identical bytes

  # state_round_trip_is_audio_invariant
  Scenario: A reloaded state sounds the same
    Given a preset rendered, saved, and loaded into a new instance
    Then the two renders are the same sound

  # samples_persist_in_state
  Scenario: Loaded samples persist in state
    Given a WAV loaded into slot 0 through the environment, with the collage source
    Then the sample plays at its root pitch from the file name
    When the state is saved and loaded into an instance that never saw the file
    Then it plays the same sample

  # malformed_state_is_rejected_and_leaves_the_instance_usable
  Scenario: Malformed state is rejected and the instance stays usable
    Given a valid state and several corruptions of it
    Then an empty stream, a truncated stream, a bad magic and a future version are all refused
    When the good state is then loaded

  # older_states_with_fewer_parameters_load_with_defaults
  Scenario: Older states with fewer parameters load with defaults for the rest
    Given a state whose header claims fewer parameters than this build has
    Then it loads, the stored values apply and the rest return to defaults
