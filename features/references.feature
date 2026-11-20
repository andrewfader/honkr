Feature: HONKR is held to recordings of the real instruments
  Executable: tests/reference_tests.cpp (CTest reference_tests).

  # a_quiet_recording_is_cut_into_every_note_it_holds
  Scenario: A quiet recording is cut into every note it holds
    Given twelve 1.4 s notes 0.6 s apart at -60 dBFS over a hiss floor at -75 dBFS, 15 dB under the notes: the University of Iowa's pianissimo tuba
    Then the segmenter finds the twelve notes, each about 1.4 s long

  # every_recording_in_the_manifest_gives_notes
  Scenario: Every recording in the manifest gives notes
    Given the recordings references/manifest.tsv lists, by instrument and dynamic, and the features extracted from them
    Then every instrument at every listed dynamic has notes among the features

  # no_instrument_falls_behind_its_match_to_the_recordings
  Scenario: No instrument falls behind its match to the recordings
    Given the features of every note of the University of Iowa recordings, and HONKR playing the same notes at the same dynamics
    Then every instrument with a baseline scores no worse on any measure: harmonic profile, brightness, odd/even balance, noise, and the dynamics' level and brightening
