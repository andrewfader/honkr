Feature: HONKR is held to recordings of the real instruments
  Executable: tests/reference_tests.cpp (CTest reference_tests).

  # no_instrument_falls_behind_its_match_to_the_recordings
  Scenario: No instrument falls behind its match to the recordings
    Given the features of every note of the University of Iowa recordings, and HONKR playing the same notes at the same dynamics
    Then every instrument with a baseline scores no worse on any measure: harmonic profile, brightness, odd/even balance, noise, and the dynamics' level and brightening
