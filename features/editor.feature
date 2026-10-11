Feature: The editor is a faithful, gesture-correct view of the parameter table
  The native editor is written once against a canvas and a host interface, so the whole of it
  can be driven by a fake host that enforces CLAP's gesture rules and a canvas that records
  what is drawn. The same page table drives the real window on Linux, Windows and macOS.

  Executable: tests/ui_tests.cpp (CTest ui_tests). Visual review: build/fuzzboi-ui-preview.

  # coordinates_scale_to_any_window_and_back
  Scenario: Design coordinates scale to any window size and back
    Given several window sizes from half to double the design
    Then design coordinates survive the trip and type never drops below legibility

  # every_parameter_is_placed_exactly_once
  Scenario: Every parameter is reachable from exactly one place
    Given the page table
    Then each parameter is reachable from exactly one place

  # widgets_suit_their_parameters
  Scenario: Widgets suit their parameters
    Given every control on every page
    Then toggles are two-valued, segments short lists, menus long lists

  # every_page_fits_without_overlap
  Scenario: Every page fits between the tabs and the footer
    Given each page with every card open
    Then everything sits between the tabs and the footer, nothing overlaps, every control is visible

  # tabs_switch_pages_with_an_animation_that_settles
  Scenario: Tabs switch pages with an animation that settles
    Given a click on the fourth tab
    Then the page changes, animates, and comes to rest without any parameter edit

  # sections_collapse_and_the_column_reflows
  Scenario: Cards collapse and the column reflows
    Given the voice page's Envelope card, clicked closed
    Then the card shrinks to its header, the card below moves up, and the state survives a page change

  # knobs_drag_fine_tune_reset_and_wheel
  Scenario: Knobs drag, fine-tune, reset and respond to the wheel inside gestures
    Given the attack knob on the voice page
    When it is dragged up by the full drag range
    Then it reaches its maximum inside one gesture
    When Shift is held
    Then the same movement is a tenth as large
    When it is right-clicked, Ctrl-clicked and double-clicked
    Then each resets to the default
    When the wheel turns over it
    Then the value nudges by the wheel fraction

  # toggles_segments_and_menus_choose_options
  Scenario: Toggles, segments and menus choose options
    Given the voice mode segments and the source menu
    Then the menu opens without an edit and lists every source
    When the second item is clicked
    Then the source changes inside one gesture and the menu closes
    When the header preset menu is opened from another page and the wheel turns

  # lost_releases_and_pointer_leave_never_leak_gestures
  Scenario: Lost releases and pointer leave never leak gestures
    Given a drag whose release is lost
    Then the next press closes the old gesture before opening a new one

  # strip_clicks_reach_the_host_and_the_size_button_works
  Scenario: Artwork clicks reach the host and the size button steps
    Given the tape page's artwork strip
    Then the click reaches the host with its page and button
    Given a host that can resize
    Then clicks step the size down and wrap, right-click restores 100%, none of it is an edit

  # every_page_draws_inside_the_editor_with_every_label
  Scenario: Every page draws inside the editor with every label
    Given each page, settled, collapsed, hovered, and at extreme values
    Then every label is drawn and nothing is drawn outside the design space

  # short_names_fit_a_cell_and_ellipsis_respects_utf8
  Scenario: Short names fit a cell and the ellipsis respects UTF-8
    Given every short name and a multibyte string
    Then names are at most 14 characters and the ellipsis cuts between characters

  # presets_have_unique_names_that_fit_the_header
  Scenario: Presets have unique names and valid settings
    Given the preset table
    Then names are unique and short, and every setting is inside its parameter's range

  # scope_holds_still_and_amplifies_quiet_output
  Scenario: The output scope holds the wave still and amplifies quiet output
    Given a 220 Hz sine at -24 dBFS, captured at two unrelated moments
    Then it is amplified by the 23.1 dB that brings its peak to 0.9 of the frame, and says so
    Then the trace starts on a rising zero crossing, so both captures draw the same picture
    Then the window is 20 ms: 4.4 cycles of 220 Hz
    Given silence, hiss at -90 dBFS, a quiet sine at -40 dBFS and a full-scale sine
    Then silence and hiss are not blown up, the gain stops at +24 dB, and loud output is never turned down

  # visualizer_lights_up_with_sound_and_its_trails_fade_in_silence
  Scenario: The visualizer lights up with sound and its trails fade in silence
    Given the visualizer on each preset, fed two seconds of silence
    When a 220 Hz sine at -24 dBFS plays for two seconds
    When it stops
    Then silence stays dark, sound lights the picture, and the light trails on then fades away

  # visualizer_hears_bass_and_treble_hits
  Scenario: The visualizer hears bass and treble hits against their averages
    Given the visualizer settled on a steady 440 Hz tone at -20 dBFS
    Then bass and treble read as ordinary, about 1
    When a 60 Hz kick at -12 dBFS lands
    Then bass jumps well above its average and treble does not
    When the kick has passed and a 6 kHz hit lands instead
    Then treble jumps and bass does not

  # visualizer_presets_cycle_like_loamps
  Scenario: Visualizer presets cycle like LOAMP's: previous, next, random and auto
    Given the visualizer in its strip
    When next, previous and previous again are clicked
    Then the preset steps forward, back, and wraps to the last
    When the picture is left- and right-clicked, and something outside it is clicked
    Then left-click moves on, right-click goes back, outside clicks are not taken
    When random is clicked twenty times
    Then each pick is a different preset and the picks range over the list
    When auto is switched on and 21 seconds pass
    Then it has moved on by itself
    Given every preset fed the same three seconds of music
    Then each draws a finite picture inside the editor, unlike every other preset's

  # the_plugins_scope_and_visualizer_follow_its_output (frontend_tests)
  Scenario: The plug-in's scope and visualizer follow its real output
    Given a plug-in that has rendered 40 blocks of a held note
    Then the scope ring holds the last 4096 samples at full rate, exactly what the plug-in output
    When the visualizer page draws
    Then the note lights the picture
    When its next button is clicked
    Then the click reaches the visualizer and is not a parameter edit
