Feature: The engine plays one instrument (or a section) from the keys, breath and controllers
  Executable: tests/engine_tests.cpp (CTest engine_tests).

  # a_note_sounds_while_held_and_stops_on_release
  Scenario: A note sounds while held and stops on release
    Given the default alto sax, one note held for half a second with a 70 ms release
    Then the note sounds while held, stops on release, and the engine reports silence within a second

  # a_breath_controller_takes_over_once_it_moves
  Scenario: A breath controller takes over once it moves
    Given the breath from CC 2, a note played before the controller has moved, then the controller at none and at full
    Then the velocity blows the note until the controller moves; then no breath is silence and full breath is loud

  # dynamics_scales_the_breath
  Scenario: Dynamics scales the breath
    Given a soft and a hard note on the trumpet with Dynamics at full and at none
    Then at full the two differ by more than 10 dB; at none they are the same mezzo-forte

  # switching_instruments_silences_the_old_one
  Scenario: Switching instruments silences the old one
    Given a held trumpet note, then the instrument switched to the flute
    Then the trumpet stops at once and the flute plays the next note

  # the_wheel_adds_vibrato_and_the_bend_bends
  Scenario: The wheel adds vibrato and the bend bends
    Given a held clarinet note without vibrato, then the mod wheel full up, then the bend wheel full up
    Then the wheel swings the pitch by tens of cents; the bend raises it by the bend range, two semitones

  # a_flute_keeps_its_even_harmonics_as_it_is_blown_harder
  Scenario: A flute keeps its even harmonics as it is blown harder
    Given the flute's D5 and the alto flute's G4, each held at a pianissimo and at a fortissimo touch
    Then the fortissimo is less than 6 dB more odd-harmonic than the pianissimo: the player moves the jet across the edge as the breath rises

  # a_soft_trumpet_is_nearly_pure_above_its_lowest_harmonics
  Scenario: A soft trumpet is nearly pure above its lowest harmonics
    Given the trumpet's E5 held at a pianissimo and at a fortissimo touch
    Then the pianissimo's 5th harmonic lies more than 28 dB under its strongest, and the fortissimo's at least 6 dB nearer it

  # every_instrument_plays_one_note_at_a_comparable_level
  Scenario: Every instrument plays one note at a comparable level
    Given one note in the middle of every instrument's compass at a firm breath, master at 0 dB, 1.2 s of it
    Then every one holds between -22 and -10 dBFS RMS and peaks under the ceiling

  # a_brass_section_voices_its_low_notes_on_trombones
  Scenario: A brass section voices its low notes on trombones
    Given the 'Brass section, chords' preset playing E3 G#3 B3 E4 at a firm touch, and the same with the low instrument Off
    Then every key below the split is played by trombones and the others by trumpets, two players each
    Then each low note alone holds within 3 dB of the top note's level, where on the trumpets E3 is at least 8 dB under
    And each low note is what the trombones play on their own: the same level within 0.5 dB and spectral centroid within 2%

  # every_preset_meets_the_quality_bar
  Scenario: Every preset meets the quality bar
    Given each factory preset played the audit phrase: a held note (a chord for a section), then three melody notes
    Then every one is finite, within 2 dB of the target loudness, under the ceiling with the clip light off, audible and free of DC

  # the_output_limiter_holds_the_ceiling_and_leaves_quiet_signals_alone
  Scenario: The output limiter holds the ceiling and leaves quiet signals alone
    Given a fortissimo brass chord with the master at +12 dB, then at -24 dB
    Then the hot one peaks at the -2 dBFS ceiling with the clip light on; the quiet one, peaking under the ceiling, is not reduced at all

  # the_advertised_tail_covers_the_decay
  Scenario: The advertised tail covers the decay
    Given the French horn preset in a hall with a long release, a note released
    Then after the advertised tail the output is quiet
