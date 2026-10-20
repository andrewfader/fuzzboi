Feature: The instrument is played in a room, through air
  Every source plays into a room (src/dsp/air.hpp) and is heard by a microphone at a distance:
  through air that absorbs the top end as ISO 9613-1 gives for its temperature and humidity, at
  the speed of sound for its temperature, with the walls' reflections (image sources to second
  order) and a late tail decaying at the Sabine time. The room's air reaches the guitar too: the
  wood of an acoustic top takes up its moisture, and the strings and neck expand with its
  temperature. Light, air pressure, wind and the walls' frequency dependence are not modelled.

  Executable: tests/air_tests.cpp (CTest air_tests).

  # sound_reaches_the_microphone_at_the_speed_of_sound
  Scenario: Sound reaches the microphone at the speed of sound
    Given a microphone 5 m from the source in the open, in air at 0 C and at 30 C
    Then the click arrives 5 / (331.3 sqrt(1 + T / 273.15)) s later, within a sample

  # the_air_takes_the_top_end_off_as_iso_9613_gives
  Scenario: The air takes the top end off as ISO 9613-1 gives
    Given a microphone 10 m away in the open, in air at 20 C and 20% humidity
    Then sines at 2, 4 and 8 kHz lose what ISO 9613-1 gives over 10 m, within 1 dB
    And humid air takes less than dry air at 8 kHz

  # a_room_rings_for_its_sabine_time_and_the_air_shortens_the_top
  Scenario: A room rings for its Sabine time and the air shortens the top
    Given a 3000 cubic-metre hall with walls of absorption 0.2, the microphone 3 m away
    Then its impulse response decays in the 500 Hz octave within 25% of Sabine's time
    And the 8 kHz octave dies sooner, as the air in the hall soaks up the top

  # the_further_the_microphone_the_more_room_it_hears
  Scenario: The further the microphone, the more room it hears
    Given a 150 cubic-metre studio room, one microphone channel 0.5 m and then 2 m from the source
    Then everything after the direct sound stands where diffuse-field theory puts it, 16 pi r^2 / A, within 4 dB
    And four times the distance gives the room 12 dB more against the direct sound, within 3 dB

  # humid_air_lowers_a_flat_tops_voice
  Scenario: Humid air lowers a flat-top's voice
    Given the dreadnought at 30% and at 80% humidity, every note from D3 to C#4 plucked in turn
    Then the note whose fundamental the body radiates most is 1 to 5 semitones lower in the humid room

  # a_warm_room_flattens_the_strings_and_nylon_most
  Scenario: A warm room flattens the strings, and nylon most
    Given the single-coil electric and the classical, tuned at 20 C, played at 35 C
    Then the plain steel high E goes 10 to 20 cents flat, as heard by ear within 3 cents of the physics
    And the nylon treble E more than twice as far
