# Fitting presets to the song corpus

The song corpus (`song-corpus/`, outside this repository) holds htdemucs_6s stems of 45 s
excerpts of the user's own home-recorded records, and `features_ref.json`, every stem measured by
the corpus's one extractor, `song-corpus/scripts/features.py`. Only numbers come into this
repository: `tools/corpus_reference.py` turns `features_ref.json` into `tests/corpus_reference.hpp`
(the 25th percentile, median and 75th percentile of four features, per group of excerpts and stem),
and measures with the same extractor the real tuba the home-taped tuba answers to (below).

    build/honkr-corpus-render iowa-tuba ~/.cache/honkr-references /tmp/iowa-tuba
    .../.claude-gpu-audio/bin/python -I tools/corpus_reference.py /path/to/song-corpus/features_ref.json /tmp/iowa-tuba > tests/corpus_reference.hpp

## What is measured

`tests/corpus_gate.hpp` measures a render as `features.py` measures a stem: the spectral centroid
and 85% roll-off of the long-term spectrum (librosa 1.0's STFT, zero-padded), the zero-lag L/R
correlation, and the tape wobble: the pitch tracked by librosa's pyin (ported exactly in
`tests/corpus_pyin.hpp`, Viterbi decoding included), the standard deviation of each sustained
run's pitch about its own 21-frame moving average, the median over runs. Its pyin quantises to
10-cent bins, so a dead-steady horn measures under a cent and a vibrato alone a few cents.
`corpus_gate_tests` checks the measures against `features.py`'s readings of the same synthetic
signals, and every gated preset was cross-checked by rendering it with `build/honkr-corpus-render`
and running `features.py` on the render: all four features agree to the printed digit.

## Which excerpts carry horns

Horns land in the `other` stem, but not every excerpt's `other` stem holds them. Without listening,
each OTC excerpt's `other` stem was judged on its share of the mix, its pyin voicing and median f0,
harmonic over percussive energy (HPSS), spectral flatness and roll-off. Five plausibly carry winds
(`OTC_winds`): present (over -30 dB of the mix), tonal (flatness under -75 dB, harmonic over
percussive by more than 4 dB), pitched in a horn section's register (median f0 90 to 1000 Hz), and
not in the bass register (roll-off over 400 Hz). The seven left out, and why, are listed in
`tools/corpus_reference.py`.

| group | stem | excerpts | centroid Hz | roll-off Hz | L/R correlation | wobble cents |
|---|---|---|---|---|---|---|
| OTC_winds | other | 5 | 788 .. 987 | 1104 .. 1486 | 0.46 .. 0.83 | 4.52 .. 7.71 |
| OTC | other | 12 | 590 .. 1779 | 962 .. 3090 | 0.45 .. 0.85 | 4.81 .. 8.13 |
| OTC | bass | 12 | 98 .. 148 | 135 .. 209 | 0.87 .. 1.00 | 5.67 .. 12.08 |
| Iowa_tuba_taped | bass | 2 takes | 351 .. 639 | 468 .. 969 | 0.95 .. 0.97 | 6.08 .. 6.34 |

## The records' bass stems are not a tuba

A real tuba, separated as the corpus was separated, does not look like OTC's `bass` stems.
`tools/tuba_separation.py` runs the University of Iowa tuba (`references/`), its notes spliced into
the tuba line (`tools/references/tuba_line.hpp`), through htdemucs_6s exactly as
`song-corpus/scripts/separate_file.py` does: alone, and added to each of the twelve OTC excerpts (its
six stems summed back into a mix, in memory; only numbers come out) at 6 and 12 dB under the mix.

| the tuba line | in `bass` | in `guitar` | the `bass` stem's centroid, without / with the tuba |
|---|---|---|---|
| mezzo-forte, alone | 100% | 0% | 366 Hz (the line itself: 398) |
| fortissimo, alone | 9% | 90% | 1139 Hz |
| mezzo-forte, -6 dB in the mix | 84% (67 .. 93) | 7% | 120 / 330 Hz |
| mezzo-forte, -12 dB | 63% (10 .. 86) | 17% | 125 / 281 Hz |
| fortissimo, -6 dB | 15% (1 .. 85) | 73% | 125 / 511 Hz |
| fortissimo, -12 dB | 14% (0 .. 70) | 64% | 126 / 245 Hz |

(Medians over the twelve excerpts, the range in brackets; a pianissimo line alone, spliced the same
way, went 96% to `bass`.) A tuba at mezzo-forte lands in `bass` with its partials kept, and the
stem's centroid rises from 120 Hz to 280 to 330; at fortissimo most of it goes to `guitar`, and
where the rest reaches `bass` the stem centres at 245 to 511 Hz. Either way no tuba leaves a `bass`
stem centred at OTC's 98 to 148 Hz (the five horn-bearing excerpts' centre at 76 to 182), so those
stems are not a tuba's spectrum, and the home-taped tuba is
not held to the records' bass stems for its spectrum, only for where it sits (its L/R correlation)
and the tape's wobble, which a separated stem keeps.

## The real tuba, taped

What the home-taped tuba's spectrum answers to is the real instrument recorded the same way. The
Iowa tuba's mezzo-forte and fortissimo notes (anechoic, one cardioid at 1.5 m) are spliced into the
tuba line, the bass line a fifth up and folded into C2 to B2, the octave Iowa recorded
(`tests/corpus_phrases.hpp`), then passed through the engine's own bedroom (35 m³), a cardioid pair
at 0.3 m (`corpus::kHomeTape` in `tests/corpus_gate.hpp`) and the tape the preset claims (18 cents of
wow and flutter, 3 kHz), and measured by `features.py`. The preset records exactly so (a test
holds it), so the two differ only in the instrument; the line's velocities (0.65 and 0.7) lie
between HONKR's mezzo-forte and fortissimo (0.55 and 0.95), so the preset sits between the two takes.

| take | centroid Hz | roll-off Hz | L/R correlation | wobble cents |
|---|---|---|---|---|
| mezzo-forte, dry | 398 | 490 | 1 | 2.78 |
| mezzo-forte, taped | 351 | 468 | 0.97 | 6.08 |
| fortissimo, dry | 662 | 964 | 1 | 2.85 |
| fortissimo, taped | 639 | 969 | 0.95 | 6.34 |

The pair's distance hardly moves the spectrum (at 0.3, 0.5, 0.75 and 1 m the taped mezzo-forte
centres at 351, 357, 354 and 339 Hz, the fortissimo at 639, 645, 643 and 663) but sets the
correlation: 0.97 and 0.95 at 0.3 m, inside the bass stems' 0.87 to 1.00, and 0.86 to 0.65 farther
off, outside it. The room is the engine's, applied to a recording already made at 1.5 m, so the
cardioid's proximity lift at 0.3 m (about 5 dB at 65 Hz and 3 dB at 100 Hz by docs/PHYSICS.md's
1 + (1 - a) c / (j w d)) is applied to both.

## The presets

Every one is recorded in a bedroom on a tape with 18 cents of wow and flutter and a 3 kHz bandwidth.
The sections play three lines in counterpoint (`tests/corpus_phrases.hpp`) and must sit inside both
`other` groups; the tuba plays the tuba line, its spectrum inside the real tuba's taped takes and
its correlation and wobble inside the `bass` stems'.

| preset | instruments | centroid Hz | roll-off Hz | correlation | wobble cents |
|---|---|---|---|---|---|
| Home-taped horn section | trumpet over French horns (split F#4), microphones at 0.6 m | 840 | 1389 | 0.65 | 6.01 |
| Home-taped low brass | trombones over a tuba (split D3), microphones at 0.6 m | 847 | 1394 | 0.62 | 6.01 |
| Home-taped tuba | tuba, cardioid pair at 0.3 m | 508 | 689 | 0.96 | 6.23 |

`features.py` reads the same figures from `build/honkr-corpus-render`'s renders, to the printed digit.
Without the tape's wow and flutter, the sections' wobble falls to 0.6 and 0.8 cents (most of it the
light 6-cent vibrato) and the tuba's to 2.6, under the bass stems' range: the records' wobble is the
tape's, not the players'.

The tuba was first fitted to the bass stems' spectrum, close-miked (a cardioid pair at 10 cm, its
proximity lift taking it down to 254 Hz) and still 106 Hz over the top of their centroid's
quartiles, which the gate carried as an allowance (116 Hz). With the bass stems shown to hold no tuba, the allowance is gone: the
tuba answers to the real one taped the same way, and its pair stands at 0.3 m.

`corpus_gate_tests` also holds what is not a taped tuba outside the tuba's ranges: the studio "Tuba"
preset (no tape, a hall: correlation 0.79 and wobble 2.8 cents), the home-taped tuba with its tape
taken off (wobble 2.6 cents), and a bass clarinet recorded as the home-taped tuba is (centroid
216 Hz). A trombone in the tuba's octave, recorded the same way, passes (567 Hz, 904 Hz): these four
measures tell a tuba from a reed or an untaped horn, not from its neighbour in the brass.

Dry, HONKR's tuba plays the tuba line where the recording does to within 0.3 octave
(`corpus_gate_tests`): at velocity 0.55 it centres at 452 Hz with its roll-off at 592 (the
mezzo-forte take: 398 and 490), at 0.95 at 655 and 888 (fortissimo: 662 and 964). Before the tuba
was refitted to all three of its recorded dynamics (docs/PHYSICS.md) its fortissimo centred at
481 Hz with its roll-off at 651, half an octave dull.
