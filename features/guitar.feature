Feature: The physical guitar is a real guitar, simulated
  The Physical guitar source is six strings on a neck over a body, simulated from their physics
  (src/dsp/guitar.hpp): the maker's string data, equal-tempered frets on a real scale length with
  the action and nut that make fretted notes go sharp, a saddle compensated as a luthier sets it,
  stiffness that stretches the partials, losses fitted to the string's physics, two planes of
  vibration that beat and exchange energy, a pluck laid on the string as the shape a finger or
  pick leaves it in, tension that rises with a hard pluck, sympathetic strings, an acoustic body
  that radiates or magnetic pickups that listen. One guitar is shared by every voice, as a
  guitarist has one guitar. Each scenario is proved from the model's numbers and its audio.
  Recordings of real guitars are the yardstick for tools/guitar_critic.py, a review tool rather
  than a test (see docs/GUITAR.md).

  Executable: tests/guitar_tests.cpp (CTest guitar_tests).

  # the_strings_are_the_makers_strings
  Scenario: The strings are the maker's strings
    Given the three string sets from D'Addario's tension chart, at its 25.5-inch scale
    Then each string's unit weight gives the chart's tension within 1%
    When the strings are on the guitars, tuned to pitch
    Then the tension at pitch is within 5% of the chart's, scaled to the guitar's scale

  # every_open_string_in_every_tuning_sounds_at_its_pitch
  Scenario: Every open string in every tuning sounds at its pitch
    Given the single-coil electric in each of the eight tunings, and the other guitars in standard
    Then a soft pluck of each open string reads within 3 cents of its note on a tuner

  # the_bass_vi_is_a_guitar_an_octave_down
  Scenario: The Bass VI is a guitar an octave down
    Given the Bass VI, a 30-inch neck strung with D'Addario's XLB095 and NW072 to NW024, in standard tuning
    Then its open strings are E1 A1 D2 G2 B2 E3 and each reads within 3 cents of its note on a tuner
    And each string pulls what its unit weight gives at pitch, 26 to 32 lb on every string
    When a bass note below a guitar's range is asked for
    Then the bass plays it where it is, where a guitar takes it up an octave

  # a_twelve_string_sounds_every_course_twice
  Scenario: A twelve-string sounds every course twice
    Given the electric twelve-string (EXL150, pairs reversed) and the acoustic twelve-string (EJ38), in standard tuning
    Then each second string reads its course's note plus the course interval and the player's few cents, within 3 cents
    When a key plucks a course, against each of its two strings plucked alone as the pick meets them
    Then the course is the two strings together, within 15 dB (octave and unison pairs couple most through the bridge), and the second string carries within 12 dB of the first
    When one course is plucked, against the string the pick meets first plucked alone
    Then the second string joins 1 to 4 ms later: the main string first on the electric, the octave first on the acoustic
    When the key is let go with a 50 ms release
    Then both strings of the course are damped, more than 50 dB down 0.3 s later

  # frets_are_equal_tempered_and_the_saddle_is_compensated_like_a_real_one
  Scenario: Frets are equal-tempered and the saddle is compensated like a real one
    Given each guitar's neck, action, nut height and string stiffness
    Then the 12th fret is an exact octave, frets up to the 15th are within 10 cents, and the first fret reads sharp
    And the saddle setbacks fall where luthiers set them, the plain B and G further back than their neighbours
    When fretted notes are played and read by ear
    Then they sound at the pitch the physics gives, within 3 cents

  # partials_are_stretched_by_the_strings_stiffness
  Scenario: Partials are stretched by the string's stiffness
    Given the electric's low E and plain G, open, plucked softly
    Then the inharmonicity measured on partials 2 to 10 is the strings' B = pi^2 E I / (T L^2), within 40%

  # upper_partials_die_first_and_an_electric_rings_longest
  Scenario: Upper partials die first, and an electric rings longest
    Given the low E plucked open on the electric and on the dreadnought
    Then on each the 8th partial dies faster than the fundamental
    And the solid body's fundamental rings for well over ten seconds, longer than the acoustic's

  # the_pluck_point_notches_its_harmonics
  Scenario: The pluck point notches its harmonics
    Given the classical's D string at the 3rd fret, plucked at its middle and near the saddle
    Then plucked at the middle the second harmonic is 30 dB under the same pluck near the saddle

  # a_hard_pluck_starts_sharp_and_settles
  Scenario: A hard pluck starts sharp and settles
    Given the electric's low E plucked as hard as it goes, and softly
    Then the hard pluck starts 4 to 30 cents sharp and falls back toward pitch; the soft one starts in tune

  # notes_are_fingered_like_a_guitarist_and_a_string_plays_one_note
  Scenario: Notes are fingered like a guitarist, and a string plays one note
    Given an open E major chord played into a standard-tuned guitar one note at a time
    Then all six land on different strings in the open E shape
    When a note outside the instrument is asked for
    Then it moves by octaves into range
    When a ringing open A string is fretted at the 2nd fret and plucked again
    Then the A is choked and the string sounds B2

  # a_released_note_is_damped_and_open_strings_ring_in_sympathy
  Scenario: A released note is damped, and open strings ring in sympathy
    Given A2 fretted on the low E string of the dreadnought, the open A string untouched
    When with the sustain pedal down the low E alone is stopped by the hand
    Then the A string, driven through the bridge, still sounds A2 and falls slowly as a free string
    Given the same note without the pedal
    Then the hands mute every string and a second later the guitar is 50 dB down

  # pickups_hear_the_string_and_a_body_radiates_it
  Scenario: Pickups hear the string, and a body radiates it
    Given A3 through each pickup of the single-coil
    Then the bridge pickup is brighter than the middle, which is brighter than the neck
    And the humbucker's neck pickup is darker than the single coil's
    Given the dreadnought's low E, below the body's air resonance
    Then the body radiates its second harmonic more strongly than its fundamental

  # a_legato_note_on_the_same_string_is_a_hammer_on
  Scenario: A legato note on the same string is a hammer-on
    Given G3 ringing on the open G string, then a hammer-on to the 2nd fret
    Then the string plays A3 without a new pluck

  # the_whole_instrument_is_stable_in_every_tuning
  Scenario: The whole instrument is stable in every tuning
    Given every guitar in every tuning with all six strings plucked hard at once
    Then everything stays finite and dies away

  # the_engine_plays_the_guitar_as_one_instrument
  Scenario: The engine plays the guitar as one instrument
    Given the engine on the physical guitar with a three-voice unison and one key
    Then the three unison voices take three different strings
    When the key is released
    Then the strings are damped and let go, and the engine falls silent
