Feature: The editor is a faithful, gesture-correct view of the parameter table
  The native editor is written once against a canvas and a host interface, so the whole of it
  can be driven by a fake host that enforces CLAP's gesture rules and a canvas that records
  what is drawn. The same page table drives the real window on Linux, Windows and macOS.

  Executables: tests/ui_tests.cpp (CTest ui_tests), tests/frontend_tests.cpp (CTest frontend_tests,
  Linux). Visual review: build/honkr-ui-preview.

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
  Scenario: Cards collapse
    Given the instrument page's Tuning card, clicked closed
    Then the card shrinks to its header and the state survives a page change

  # knobs_drag_fine_tune_reset_and_wheel
  Scenario: Knobs drag, fine-tune, reset and respond to the wheel inside gestures
    Given the master knob on the instrument page
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
    Given the play-mode segments on the instrument page and the instrument menu
    Then each segment selects its option, and the menu opens without an edit and lists every instrument
    When the second item is clicked
    Then the instrument changes inside one gesture and the menu closes
    When the header preset menu is opened from another page and the wheel turns
    Then the catalogue stays inside the window and the wheel steps the preset
    Given the drones toggle on the mouth page
    Then it flips

  # lost_releases_and_pointer_leave_never_leak_gestures
  Scenario: Lost releases and pointer leave never leak gestures
    Given a drag whose release is lost
    Then the next press closes the old gesture before opening a new one

  # strip_clicks_reach_the_host_and_the_size_button_works
  Scenario: Artwork clicks reach the host and the size button steps
    Given the room page's artwork strip
    Then the click reaches the host with its page and button, and every page has its strip
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
    Then it is amplified to fill the frame and says so, starts on a rising zero crossing, and shows 20 ms
    Given silence, hiss, a quiet sine and a full-scale sine
    Then silence and hiss are not blown up, the gain stops at +24 dB, and loud output is never turned down

  # the_host_describes_every_parameter_with_help_and_labels (frontend_tests)
  Scenario: The host describes every parameter with help and labels
    Given the plug-in's editor host
    Then every parameter has matching info, a label, a short label, help, and formats its default

  # relevance_follows_the_selected_instrument_and_switches (frontend_tests)
  Scenario: Relevance follows the selected instrument and switches
    Given the alto sax selected
    Then the reed and the keys are live, the lips, mute, hand, drones and throat are dimmed, and dependent controls follow their switches
    When the trumpet is selected
    Then the lips, lipping, mute and hand light up and the reed dims
    When the Highland pipes are selected
    Then the drones are live and the tongue dimmed
    When the flute is selected
    Then the embouchure is live and the reed and lips are dimmed
    And no instrument dims the whole editor

  # editor_gestures_reach_the_host_as_clap_events (frontend_tests)
  Scenario: Editor gestures reach the host as CLAP events
    Given a drag of the master level through the editor host
    Then the plug-in holds the value, asked for a flush, and marked the state dirty
    When the host flushes
    Then it receives begin, two values and end, in order
    When the preset 'Concert flute' is chosen through the editor
    Then the whole preset is applied and the host is asked to rescan on its main thread
    When the queue overflows
    Then events are dropped rather than the editor blocking, and the value is still right

  # page_artwork_draws_from_the_running_plugin (frontend_tests)
  Scenario: Page artwork draws from the running plug-in
    Given a plug-in that has rendered a note
    Then every page's strip and the meters draw inside the editor, the keyboard strip names the alto sax and lights the sounding key, and the fingering names the note

  # the_plugins_scope_follows_its_output (frontend_tests)
  Scenario: The plug-in's scope follows its real output
    Given a plug-in that has rendered 40 blocks of a held note
    Then the scope ring holds the last 4096 samples at full rate, exactly what the plug-in output

  # the_whole_editor_draws_and_ticks_without_a_window (frontend_tests)
  Scenario: The whole editor draws and ticks without a window
    Given the plug-in's editor, created on demand
    Then it draws every page from live plug-in state and takes pointer input
    When the editor asks for a size the host cannot grant
    Then the window size stays as it was
