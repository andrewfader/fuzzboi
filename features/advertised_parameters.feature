Feature: Every parameter does what it advertises
  The plug-in advertises each parameter to the host as a name, a unit and a range. Each one is
  proved from audio rendered by the built FUZZBOI.clap through the public CLAP ABI: a musical
  context (a source, a few settings, a note), the parameter at two values, and a measurement in
  the advertised unit comparing the two renders: cents for pitch, dB for level, Hz for rates,
  duty as a fraction, spectral centroid for brightness, tail energy for length, or at the least
  that the audio changed. The proof table is indexed by parameter ID, so the list is closed by
  construction.

  Executable: tests/advertised_tests.cpp (CTest advertised_tests, against the built FUZZBOI.clap).

  # proofs (table), checked by main
  Scenario: Every parameter has a proof
    Given the parameter table the plug-in advertises
    When the proof table is built
    Then every parameter ID has a proof, and a parameter without one fails the suite

  # proofs (table), checked by main
  Scenario: Each parameter moves the sound in its advertised unit
    Given a parameter's musical context, rendered with the parameter at its low and high values
    When the two renders are measured in the parameter's unit
    Then the change is the one the parameter advertises, within its tolerance
    And the render at the high value is finite
