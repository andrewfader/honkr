Feature: The instrument is played in a room, through air
  Every instrument plays into a room (src/dsp/air.hpp) and is heard by a microphone at a
  distance: through air that absorbs the top end as ISO 9613-1 gives for its temperature and
  humidity, at the speed of sound, with the walls' reflections and a late tail decaying at the
  Sabine time. What the microphone hears of a real instrument across that air is what the
  physics says it loses.

  Executable: tests/air_tests.cpp (CTest air_tests).

  # sound_reaches_the_microphone_at_the_speed_of_sound
  Scenario: Sound reaches the microphone at the speed of sound
    Given a microphone 5 m from the source in the open, in air at 0 C and at 30 C
    Then the click arrives 5 / (331.3 sqrt(1 + T / 273.15)) s later, within a sample

  # the_air_takes_the_top_end_off_as_iso_9613_gives
  Scenario: The air takes the top end off as ISO 9613-1 gives
    Given a microphone 10 m away in the open, in air at 20 C and 20% humidity
    Then an 8 kHz sine loses what ISO 9613-1 gives over 10 m within 1 dB, and humid air takes less

  # a_room_rings_for_its_sabine_time
  Scenario: A room rings for its Sabine time
    Given a 3000 cubic-metre hall with walls of absorption 0.2, the microphone 3 m away
    Then its impulse response in the 500 Hz octave decays within 25% of Sabine's time

  # a_trumpet_across_a_field_loses_its_top_to_the_air
  Scenario: A trumpet across a field loses its top to the air
    Given a trumpet holding C5, heard by a microphone on the bell and by one 10 m away in the open, in air at 20 C and 20% humidity
    Then the fundamental arrives at the level it left (the preamp is set for the direct sound)
    And the harmonic near 8 kHz loses what ISO 9613-1 gives over 10 m, within 0.5 dB, and humid air takes less of it
