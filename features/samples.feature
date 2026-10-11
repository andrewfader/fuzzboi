Feature: Samples load from files, persist in projects, and never leave a slot silent
  Executables: tests/state_tests.cpp (samples_persist_in_state), tests/engine_tests.cpp
  (collage_sampler_plays_found_sound_when_the_bank_is_empty), tests/harvest_tests.py
  (harvest_tool_filters_licences_and_writes_a_manifest).

  Scenario: A WAV named with a note loads at that pitch
    Given a WAV loaded into slot 0 through the environment, with the collage source
    Then the sample plays at its root pitch from the file name

  Scenario: Samples persist in state
    When the state is saved and loaded into an instance that never saw the file
    Then it plays the same sample

  Scenario: An empty slot plays found sound
    Given the collage source with nothing loaded
    Then it still makes a sound, from the built-in found-sound bank

  # harvest_tool_filters_licences_and_writes_a_manifest
  Scenario: The harvest tool accepts only permissive licences and records attribution
    Given search results under CC0, CC BY, CC BY-NC and no licence
    When the tool runs against a fake archive
    Then only the CC0 and CC BY items are downloaded, converted and listed in the manifest with their licences
