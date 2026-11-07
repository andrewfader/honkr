Feature: The sound-producing loop: reeds, lips and jets driving their bores
  Executable: tests/horn_tests.cpp (CTest horn_tests).

  # a_reed_on_a_cylinder_sounds_odd_harmonics
  Scenario: A reed on a cylinder sounds odd harmonics
    Given the clarinet's cylinder and reed, blown at mezzo-forte on a bore for 233 Hz
    Then the even harmonics sit well below their odd neighbours: the closed end's quarter-wave modes

  # the_reed_speaks_above_its_threshold_and_chokes_near_its_closing_pressure
  Scenario: The reed speaks above its threshold and chokes near its closing pressure
    Given the clarinet's reed, blown at rising fractions of the pressure that shuts it
    Then it does not speak at a third (the lossless threshold) nor just under a half (the bore's losses raise it); above, it does; pressed past its closing pressure it chokes

  # a_harder_breath_is_brighter
  Scenario: A harder breath is brighter
    Given the clarinet at 0.5 and at 0.9 of its closing pressure
    Then the reed beats as the breath rises and its spectrum reaches higher

  # a_cone_sounds_every_harmonic
  Scenario: A cone sounds every harmonic
    Given the alto saxophone's cone and reed on a bore for 233 Hz
    Then it sounds the bore's pitch, with its second harmonic strong (a cone's modes are a full harmonic series)

  # the_lips_choose_the_partial
  Scenario: The lips choose the partial
    Given the trumpet's open bore (its harmonic series on 116.5 Hz), the lips set for the 2nd to the 6th partial
    Then the horn sounds each partial of the one bore, within 30 cents

  # valves_pressed_together_come_up_short
  Scenario: Valves pressed together come up short
    Given the valve tubing: each crook cut for its own interval alone
    Then 1+3 lowers the horn by less than five semitones (about 30 cents sharp) and 1+2+3 by less than six (about 50 sharp)

  # a_jet_overblows_to_the_octave
  Scenario: A jet overblows to the octave
    Given the flute's pipe for 523 Hz, the jet crossing the mouth in most of a period, then in half of one
    Then the long jet sounds the fundamental, the short one the pipe's second mode, an octave up

  # a_stopped_pipe_and_a_vessel_sound_what_their_acoustics_allow
  Scenario: A stopped pipe and a vessel sound what their acoustics allow
    Given the pan flute's stopped pipe and the ocarina's vessel, both for 440 Hz
    Then the stopped pipe's even harmonics are weaker than its odd ones; the vessel's one mode is nearly a pure sine

  # loud_brass_blares
  Scenario: Loud brass blares
    Given the trumpet's lips on its 4th partial at pianissimo and at fortissimo, with the bore's wave steepening and without it
    Then the loud note's spectrum reaches far higher, and most of that is the steepening: the pianissimo is untouched by it

  # the_filters_delay_is_known_exactly
  Scenario: The filters delay is known exactly
    Given the loop's two poles of bell low-pass and its DC blocker at a pitch
    Then their phase delay is positive and grows as the bell's cutoff falls

  # every_instrument_plays_in_tune_across_its_compass
  Scenario: Every instrument plays in tune across its compass
    Given every instrument, calibrated, playing its lowest, middle and highest notes at mezzo-forte through the player
    Then every note sounds within 25 cents of its pitch (35 for the flutes and the lowest brass)
