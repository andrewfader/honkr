Feature: The instruments are fingered as they are, and played as players play them
  Executable: tests/winds_tests.cpp (CTest winds_tests).

  # the_trumpet_is_fingered_as_its_chart
  Scenario: The trumpet is fingered as its chart
    Given the B-flat trumpet, its notes at concert pitch
    Then each note takes the valves and partial a trumpeter uses

  # the_horn_speaks_in_its_own_intonation_unless_the_player_lips_it
  Scenario: The horn speaks in its own intonation unless the player lips it
    Given the trumpet's C4 (1+3) and D5 (the open 5th partial), with lipping at none and at full
    Then unlipped, 1+3 is about 30 cents sharp and the 5th partial 14 flat; lipped, both are tempered

  # the_trombone_slide_and_its_trigger
  Scenario: The trombone slide and its trigger
    Given the tenor trombone
    Then Bb2 is first position, E2 seventh, F3 first on the 3rd partial, and C2, in the gap below E2, needs the F trigger

  # the_double_horn_switches_to_its_b_flat_side
  Scenario: The double horn switches to its b flat side
    Given the double horn, below and above the thumb valve's switch point
    Then the F side plays the lower note on a high partial of F; the Bb side the upper one on a partial of Bb

  # a_natural_horn_plays_only_its_partials
  Scenario: A natural horn plays only its partials
    Given the bugle in G and the alphorn in F#, played on every key, unlipped
    Then every key sounds a partial of the open horn; the alphorn's 11th falls between B and C, a quarter-tone over its scale's fourth, the famous 'alphorn fa'

  # the_chanter_has_nine_notes
  Scenario: The chanter has nine notes
    Given the Highland chanter, played on every key from G4 to A5
    Then only its nine notes sound: low G, low A, B, C#, D, E, F#, high G, high A

  # a_note_beyond_the_compass_sounds_inside_it
  Scenario: A note beyond the compass sounds inside it
    Given the flute (C4 to A6) asked for C2 and C8, the tuba (D1 to F4) for C6
    Then each sounds an octave (or more) inside its compass

  # a_solo_player_slurs_without_tonguing_again
  Scenario: A solo player slurs without tonguing again
    Given a solo alto sax holding C4, then G4 pressed with C4 still held, then G4 let go
    Then one player plays throughout: the slur keeps sounding (no gap) and lands on G4, and letting G4 go returns to C4

  # a_section_plays_chords_and_doubles_in_unison
  Scenario: A section plays chords and doubles in unison
    Given the trumpet in Section mode with C4 and G4 held, then in Solo mode with three players on C4
    Then both notes of the chord sound; three players sound the unison

  # the_breath_can_come_from_a_controller
  Scenario: The breath can come from a controller
    Given a held clarinet note whose breath comes from a controller, at none, then at full
    Then with no breath the held key is silent; with full breath it sounds

  # the_pipes_drone_while_the_bag_is_squeezed
  Scenario: The pipes drone while the bag is squeezed
    Given the Highland pipes playing E5 with the drones on, then off
    Then the bass drone's A2 sounds under the chanter only when the drones are on
