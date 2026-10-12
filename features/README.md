# Features

Every user-visible behaviour of FUZZBOI has a scenario here, in Given/When/Then form, and an
executable test mapped to it. The feature file names the test binary and the scenario function
that proves each scenario; `ctest` runs them all. A scenario without a mapped test is a bug in
this directory, and a test without a scenario is a bug in the test.

| Feature | Executable |
|---|---|
| dsp_building_blocks.feature | `dsp_tests` |
| sound_sources.feature | `sources_tests` |
| effects.feature | `fx_tests` |
| engine.feature | `engine_tests` |
| guitar.feature | `guitar_tests` |
| clap_plugin.feature | `plugin_smoke`, `note_tests`, `state_tests` |
| advertised_parameters.feature | `advertised_tests` |
| editor.feature | `ui_tests`, `frontend_tests` (Linux) |
| samples.feature | `harvest_tests`, `state_tests` |
