# The preset quality bar

Every factory preset is judged from audio, not from its settings. `build/honkr-preset-audit
<dir>` renders each preset with the same phrase (a held middle C with the E and G above it, which a solo
player slurs to and a section plays as a chord, then three short melody notes, then the preset's
own tail; every instrument plays it inside its own compass), writes a stereo WAV per preset,
and prints the measurements below. A preset round is done when every preset meets the bar, the
flagged columns are empty, and the spectrograms (`ffmpeg -lavfi showspectrumpic`) look like the
instrument they claim to be.

The measurable part of the bar is enforced by `engine_tests`
(`every_preset_meets_the_quality_bar`) with the same phrase (`tests/audit_phrase.hpp`), so a
preset cannot ship below it.

| Measurement | Bar | Why |
|---|---|---|
| Loudness (the loudest 400 ms of the phrase, BS.1770 momentary, K-weighted, mono sum) | -18 LUFS, within 2 dB | Presets sit at one level under the fingers: the ear judges a phrase by its loudest moment, not by its peak sample, so an ocarina's whisper and a screaming lead trumpet are matched here |
| Limiter | the output limiter (ceiling -2 dBFS, instant attack, 50 ms release) reduces by no more than 9 dB anywhere in the phrase; the clip flag stays clear | A tongued fortissimo reaches the target by letting the limiter catch its attack; a preset that needs more than that is being squashed and should be set quieter |
| Level of the held note | audible (above -60 dBFS RMS) once it has spoken | A note that drops out is not playable |
| Melody notes | within 16 dB of the held note's peak | Short notes must not vanish next to a long one |
| DC offset of the held note | below -40 dBFS | Valves and jets leak DC that thumps on note edges |
| Idle output before the first note | below -55 dBFS RMS | Noise floors stack across tracks |
| Tail | within the engine's advertised tail; below -50 dBFS once it has passed | Hosts stop calling `process` after the tail |
| Finite | every sample | |
| Character | the spectrogram shows the thing named: a clarinet's missing even harmonics, a stopped pipe's odd ones, a fortissimo trumpet's harmonics reaching past 5 kHz, a Harmon mute's single peak | Numbers cannot see this; a person looks |

Measurements are over the mono sum: the held level over 0.3 s to 1.4 s after the first note's onset,
brightness as the spectral centroid of the same span, width as 1 minus the left/right
correlation, tail as the peak over the last 100 ms of the render.

## Instrument levels

Presets are only comparable if the instruments behind them are. `build/honkr-preset-audit
--levels` plays one C4 at a firm touch through every instrument with the default settings and
master at 0 dB, and prints the peak, the held RMS and the trim each would still need to reach
-16 dBFS RMS held or -3 dBFS peak, whichever comes first. The trims live in `levels::` in
`src/engine.hpp` and are held by `engine_tests` (`every_instrument_plays_one_note_at_a_comparable_level`:
held -22 to -10 dBFS RMS, peak under the ceiling). When a model's level changes, re-run
`--levels` and move its trim, not the presets.

## How a round goes

1. Build, run the audit, and read the table.
2. Fix the model before fixing the preset: a preset that only sounds right at one level is a
   model that is wrong at the others.
3. Re-render, look at the spectrograms of anything changed, repeat until the flags column is
   empty.
4. Run the complete suite.

## Against recordings

The bar above says a preset is level and clean; it cannot say it sounds like the instrument it
names. For that, every note of 35 recordings of real instruments (the University of Iowa's, 12
instruments at pp, mf and ff) is replayed through HONKR and compared feature by feature:
harmonic profile, brightness, odd/even balance, noise, and how much louder and brighter the
instrument gets from pianissimo to fortissimo (references/README.md). `reference_tests` holds
every instrument to its last score (`references/baseline.tsv`), so the match can only improve;
`build-release/honkr-reference match` prints where each instrument stands. The scores are not yet
small: they are where HONKR is, not where it should be. The first round of work against them
(the dynamics, docs/PHYSICS.md) cut the error in the gain from pianissimo to fortissimo from 6 to
14 dB to 1 to 6 dB on every reed, flute and horn, at the cost of 0.6 to 1.5 dB on a few
instruments' odd/even balance and noise, accepted when the baseline was rewritten. The second
(the harmonic profiles: the cone's apex share and the fitted roll-off, docs/PHYSICS.md) cut the
profile error from 12.6 to 5.3 dB on the horn, 14.5 to 8.9 on the trumpet, 11.1 to 7.5 on the
flute, 12.4 to 7.8 on the alto flute, 14.7 to 10.3 on the bassoon, 14.3 to 10.7 on the oboe, 12.1
to 10.0 on the clarinet and 11.3 to 10.3 on the alto sax, and the bassoon's odd/even error from
16.0 to 7.6 dB. The third fixed the noise measure: it had counted harmonics above the tenth and
any pitch drift as noise (a trumpet's fortissimo F#5 measured +4.8 dB, more noise than tone).
Measured between the first ten harmonics instead, the recordings read -45 dB (the saxophones) to
-27 dB (a flute at fortissimo), and every model instrument was 5 to 26 dB too clean; a breath
turbulence added to the radiated sound and fitted per instrument (docs/PHYSICS.md) brought every
instrument within 2 to 7 dB. The fourth fitted the brass bells' radiation corners with their
roll-off and level laws (docs/PHYSICS.md): the tuba's profile error fell from 10.2 to 2.3 dB and
its brightness error from 1.20 to 0.13 octaves (its fortissimo had its strongest harmonic at the
fundamental; the recording's is the 4th or 5th), the trombone's error in the gain from pianissimo
to mezzo-forte from 6.3 to 1.2 dB and its profile's from 6.6 to 5.9, the horn's and trumpet's
profiles by 0.3 dB; the trombone's noise rose 0.6 dB, accepted. The reference notes are now
played 12 dB under the output limiter, which had been squeezing the trumpet's fortissimo. A
trombone's steepened wave peaks 15 dB over its RMS, so instrument trims aim the peak at -3 dBFS
rather than -4. The fifth refitted the cone reeds' radiation and laws the same way: the
bassoon's profile error fell from 10.3 to 8.0 dB, its brightness error from 0.29 to 0.11 octaves
and its odd/even error from 7.6 to 6.3; the oboe's profile from 10.6 to 9.4, its brightness from
0.43 to 0.26 and its brightening from pianissimo to fortissimo from 0.59 to 0.20; the soprano and
alto saxophones' profiles by 0.6 and 0.4 dB. Accepted: the oboe's odd/even error rose 0.7 dB and
the bassoon's dynamics error 0.3. The comb that the cone's apex line cuts into the reeds' spectra
remains (docs/PHYSICS.md says what was tried). The sixth replaced the reeds' waveguides with a
modal bore (docs/PHYSICS.md), refitted against the recordings with a guard against squeals: the
profile error fell to 6.4 dB on the alto saxophone (from 9.8), 6.3 on the soprano (9.0), 6.8 on the
bassoon (8.0), 8.3 on the clarinet (10.0) and 9.9 on the bass clarinet (11.7), the odd/even error to
5.8, 6.8, 3.0, 5.5 and 4.8 dB (from 8.3, 8.8, 6.3, 6.7 and 7.0), and the bass clarinet's brightening
from pianissimo to fortissimo from 0.56 to 0.19 octaves off. Accepted: the alto's dynamics error
rose 0.3 dB, the bass clarinet's 0.9 and its noise 0.8. The advertised test of the instrument switch
now leaves the fundamental out of its odd/even measure, as the recordings' measure does.
