Feature: Every parameter does what it advertises
  The plug-in advertises each parameter to the host as a name, a unit and a range, and the
  editor's help text says what it does. Each one is proved from audio rendered by the built
  HONKR.clap through the public CLAP ABI, at 48 kHz: a musical context (an instrument, a few
  settings, the keys and controllers played), the parameter at two values, and a measurement in
  the advertised unit comparing the two renders: cents for pitch, dB for level, Hz for rates,
  milliseconds for times, a count of players, or the percent change of a measured quantity. The
  proof table is indexed by parameter ID, so the list is closed by construction.

  Executable: tests/advertised_tests.cpp (CTest advertised_tests, against the built HONKR.clap).

  # proofs (table), checked by main
  Scenario: Every parameter has a proof
    Given the parameter table the plug-in advertises
    When the proof table is built
    Then every parameter ID has a proof, and a parameter without one fails the suite

  # proofs (table), checked by main
  Scenario Outline: Each parameter moves the sound in its advertised unit
    Given <context>
    When it is rendered with <parameter> at <low> and at <high>
    Then <measurement> is <expected>
    And both renders are finite

    Examples: the instrument and its tuning
      | parameter       | context                                   | low       | high          | measurement                                         | expected                                   |
      | Instrument      | G3 held                                   | Alto sax  | Clarinet      | even harmonics' energy against the odd ones'        | at least 10 dB less (the cylinder)         |
      | Master          | the alto's C4                             | -30 dB    | -6 dB         | held level                                          | +24 dB +- 0.3                              |
      | Transpose       | the alto's C4                             | 0 st      | +12 st        | pitch                                               | +1200 cents +- 5                           |
      | Pitch A4        | the alto's A4                             | 415 Hz    | 440 Hz        | pitch                                               | +101.3 cents +- 3                          |
      | Temperament     | the alto's G#4                            | Equal     | Meantone 1/4  | pitch                                               | five narrow fifths, -17.1 cents +- 2       |
      | Temperament key | the alto's Eb4 in meantone                | C         | D             | pitch                                               | -41.1 cents +- 2 (Eb becomes D#)           |
      | Fine tune       | the alto's C4                             | -50 ct    | +50 ct        | pitch                                               | +100 cents +- 3                            |

    Examples: how it is played
      | parameter       | context                                   | low       | high          | measurement                                         | expected                                   |
      | Play mode       | C4 held, E4 pressed half-way              | Solo      | Section       | C4 against E4 after E4 is pressed                   | gone in Solo, on at least 20 dB more in Section |
      | Glide           | a slur from C4 to G4                      | 0 ms      | 400 ms        | time for the pitch to arrive within 25 cents of G4  | 391 ms +- 40 later                         |
      | Bend range      | the wheel full up before C4               | 2 st      | 7 st          | pitch                                               | +500 cents +- 5                            |

    Examples: the breath
      | parameter       | context                                   | low       | high          | measurement                                         | expected                                   |
      | Breath source   | CC2 at 127, dropped to 10 half-way        | Velocity  | Breath CC2    | level change when CC2 drops                         | none (+-1.5 dB), then at least -20 dB      |
      | Breath          | the alto's C4                             | 50%       | 100%          | held level                                          | silent under the threshold, then at least 20 dB louder |
      | Dynamics        | a soft trumpet key (velocity 0.1)         | 100%      | 0%            | held level                                          | at least 6 dB louder (mezzo-forte)         |
      | Attack          | the alto's C4, no tongue                  | 5 ms      | 300 ms        | rise to -3 dB of the held level                     | under 80 ms, then 150 to 450 ms later      |
      | Release         | the trumpet's F4                          | 200 ms    | 1600 ms       | fall of 40 dB after the key is let go               | ln(100)/3 of the release more: 2149 ms +- 10% |
      | Tongue          | the trumpet's Bb4                         | 0%        | 100%          | energy above 3 kHz in the first 30 ms               | at least 6 dB more                         |
      | Breath noise    | the flute's C5                            | 100%      | 400%          | noise between the harmonics; the whole level        | +12.0 dB +- 1.5; unchanged (+-0.5 dB)      |
      | Growl           | the tenor's G3                            | 0%        | 100%          | the level's modulation                              | at 25 to 35 Hz, at least 15 dB deeper      |

    Examples: the mouth and the bell
      | parameter       | context                                   | low       | high          | measurement                                         | expected                                   |
      | Reed            | the clarinet's D4                         | 10%       | 90%           | spectral centroid                                   | at least 10% darker                        |
      | Embouchure      | the flute's C5                            | 50%       | 100%          | sounding pitch                                      | on the note, then overblown at least an octave |
      | Lip tension     | the French horn's D5 (10th partial)       | 0 ct      | +100 ct       | pitch                                               | the 11th partial, +165 cents +- 25         |
      | Lipping         | the trumpet's C4 (1+3, 3rd partial)       | 0%        | 100%          | pitch                                               | the valves' sharpness taken out, -32.3 cents +- 4 |
      | Hand / plunger  | the trumpet's Bb4                         | 0%        | 100%          | share of the energy above 2 kHz                     | at least 15 dB less                        |
      | Mute            | the trumpet's Bb4                         | Open      | Harmon        | the fundamental against the whole                   | at least 10 dB less (thin)                 |
      | Key noise       | an alto key too soft to speak             | 25%       | 100%          | the click's level in the first 40 ms                | +12.0 dB +- 1                              |
      | Throat          | the didgeridoo's D2                       | 0% (oo)   | 100% (ee)     | 2.0-2.65 kHz against 0.7-0.92 kHz                   | at least 12 dB more                        |
      | Drones          | the pipes' D5                             | Off       | On            | the bass drone's A2; the tenors' pitch              | at least 20 dB more; 220 Hz +- 5 cents     |

    Examples: vibrato
      | parameter       | context                                   | low       | high          | measurement                                         | expected                                   |
      | Vibrato         | the alto's A4, no delay                   | 0 ct      | 25 ct         | the pitch's swing either side                       | under 3 cents, then 25 +- 5                |
      | Vibrato rate    | the alto's A4, 25 cents                   | 4 Hz      | 7 Hz          | the pitch's own modulation rate                     | 4 +- 0.4 Hz, then 7 +- 0.5 Hz              |
      | Vibrato delay   | the alto's A4, 30 cents                   | 300 ms    | 1100 ms       | when the swing first reaches half its depth         | 800 ms +- 60 later                         |

    Examples: the section and the stereo picture
      | parameter       | context                                   | low       | high          | measurement                                         | expected                                   |
      | Players         | C4, Spread 30 cents                       | 1         | 3             | separate tones at the 4th harmonic                  | 1, then 3                                  |
      | Spread          | C4, three players                         | 0 ct      | 24 ct         | the outer players' tones either side                | under 4 cents, then 24 +- 4                |
      | Timing          | C4, three players 30 cents apart          | 0 ms      | 60 ms         | the latest player's entry after the first           | under 10 ms, then 25 to 70 ms              |
      | Stereo width    | C4, three players                         | 0%        | 100%          | L/R correlation                                     | 1, then at least 0.1 less                  |
      | Pan             | the alto's C4                             | L 50      | R 50          | right over left                                     | -7.66 dB, then +7.66 dB (equal power) +- 0.3 |

    Examples: the room and the air
      | parameter       | context                                   | low       | high          | measurement                                         | expected                                   |
      | Room            | the alto, microphone at 2 m               | Off       | Studio        | level 50 to 600 ms after release                    | at least 10 dB more                        |
      | Absorption      | the alto in the hall, microphone at 3 m   | 8%        | 60%           | level 50 to 600 ms after release                    | at least 6 dB less                         |
      | Mic distance    | the alto's C4                             | 0 m       | 10 m          | the note's arrival                                  | 10 m / 343.2 m/s, 29.14 ms +- 0.2 later    |
      | Humidity        | the oboe's C5, microphone at 12 m         | 10%       | 70%           | its 15th harmonic near 7.8 kHz                      | what ISO 9613-1 gives, +1.19 dB +- 0.6     |
      | Temperature     | the alto, microphone at 10 m              | -10 C     | 40 C          | the note's arrival                                  | 2.56 ms +- 0.1 sooner (speed of sound)     |
      | Mic pattern     | the bass clarinet's G2, microphone at 5 cm | Omni     | Cardioid      | the fundamental                                     | the proximity effect, +15.1 dB +- 1.5      |
      | Mic setup       | the alto in the studio, microphone at 2 m | Stereo pair | Mono        | L/R correlation                                     | under 0.95, then 1                         |
      | Preset          | A4                                        | Oboe, orchestral | Oboe, baroque | pitch                                       | A = 415: -101.3 cents +- 4                 |
