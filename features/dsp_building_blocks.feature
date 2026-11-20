Feature: The building blocks measure as the physics and the treatises say
  Executable: tests/dsp_tests.cpp (CTest dsp_tests).

  # temperaments_lay_out_the_octave_as_the_treatises_give
  Scenario: Temperaments lay out the octave as the treatises give
    Given the six temperaments laid out from C with A at 440 Hz
    Then equal temperament puts every note on its equal-tempered pitch
    And Pythagorean fifths are pure (702 cents) and its major third wide (408)
    And quarter-comma meantone's major third C-E is pure (386 cents) and its wolf G#-Eb howls (737)
    And Werckmeister III keeps C-E at 390 cents and no fifth narrower than 696
    And in every temperament and key, A4 stays on the fork
    When meantone is laid out from D instead of C
    Then D-F# is the pure third and the wolf moves

  # the_limiter_holds_the_ceiling_and_recovers
  Scenario: The limiter holds the ceiling and recovers
    Given the output limiter fed a sine 6 dB over the ceiling, then one 6 dB under
    Then nothing leaves above the ceiling, the meter reads the 6 dB it took, and the quiet signal passes untouched once the gain has recovered

  # the_body_and_mute_filters_shape_the_spectrum_as_designed
  Scenario: The body and mute filters shape the spectrum as designed
    Given an RBJ peaking filter of +8 dB at 1800 Hz, Q 1.5 (a straight mute's nasal peak)
    Then a sine at the centre gains 8 dB and one far below it passes untouched
    Given a Butterworth low-pass (Q 0.707) at 700 Hz (a hand closing the bell)
    Then it is 3 dB down at its corner, flat an octave and more below, and falls at least 12 dB per octave above, as the bilinear Butterworth gives

  # a_delay_line_delays_by_its_fractional_length
  Scenario: A delay line delays by its fractional length
    Given a delay line holding an impulse, read 10.25 samples back
    Then the impulse comes out split between samples 10 and 11 as 0.75 and 0.25: its centre at 10.25
    And a whole-sample read is exact, and a read past the capacity is held at the capacity

  # the_tapes_wow_and_flutter_swing_every_pitch_by_the_stated_cents
  Scenario: The tape's wow and flutter swing every pitch by the stated cents
    Given a 1 kHz sine recorded on the tape with 20 cents of wow and flutter, tracked every 5 ms for four seconds
    Then the pitch swings 10 cents either way at the wow's 0.73 Hz and 10 at the flutter's 6.1 Hz: together they reach the 20 cents
    When the tape runs true
    Then the sine passes through untouched, sample for sample

  # the_tapes_bandwidth_is_3_db_down_at_its_corner
  Scenario: The tape's bandwidth is 3 dB down at its corner
    Given the tape's bandwidth at 3 kHz: a fourth-order Butterworth low-pass
    Then a sine at 3 kHz is 3 dB down, one two octaves below passes within 0.1 dB, and an octave above falls as the bilinear Butterworth gives, 24 dB an octave and steeper
    When the bandwidth is open, at 20 kHz
    Then a 15 kHz sine passes untouched
