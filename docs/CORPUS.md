# Fitting presets to the song corpus

The song corpus (`song-corpus/`, outside this repository) holds htdemucs_6s stems of 45 s
excerpts of the user's own home-recorded records, and `features_ref.json`, every stem measured by
the corpus's one extractor, `song-corpus/scripts/features.py`. Only numbers come into this
repository: `tools/corpus_reference.py` turns `features_ref.json` into `tests/corpus_reference.hpp`
(the 25th percentile, median and 75th percentile of four features, per group of excerpts and stem).

    python3 -I tools/corpus_reference.py /path/to/song-corpus/features_ref.json > tests/corpus_reference.hpp

## What is measured

`tests/corpus_gate.hpp` measures a render as `features.py` measures a stem: the spectral centroid
and 85% roll-off of the long-term spectrum (librosa 1.0's STFT, zero-padded), the zero-lag L/R
correlation, and the tape wobble: the pitch tracked by librosa's pyin (ported exactly in
`tests/corpus_pyin.hpp`, Viterbi decoding included), the standard deviation of each sustained
run's pitch about its own 21-frame moving average, the median over runs. Its pyin quantises to
10-cent bins, so a dead-steady horn measures under a cent and a vibrato alone a few cents.
`corpus_gate_tests` checks the measures against `features.py`'s readings of the same synthetic
signals, and every gated preset was cross-checked by rendering it with `build/honkr-corpus-render`,
converting to FLAC and running `features.py`: all four features agree to the printed digit.

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

## The presets

Every one is recorded in a bedroom on a tape with 18 cents of wow and flutter and a 3 kHz bandwidth.
The sections play three lines in counterpoint (`tests/corpus_phrases.hpp`) and must sit inside both
`other` groups; the tuba plays a bass line and answers to the `bass` stems.

| preset | instruments | centroid Hz | roll-off Hz | correlation | wobble cents |
|---|---|---|---|---|---|
| Home-taped horn section | trumpet over French horns (split F#4), microphones at 0.6 m | 840 | 1389 | 0.65 | 6.01 |
| Home-taped low brass | trombones over a tuba (split D3), microphones at 0.6 m | 860 | 1400 | 0.62 | 6.13 |
| Home-taped tuba | tuba, cardioid pair at 0.1 m | 254 | 458 | 0.9987 | 7.15 |

Without the tape's wow and flutter, the sections' wobble falls to 0.6 and 0.8 cents (most of it the
light 6-cent vibrato) and the tuba's to 4.0, under the bass stems' range: the records' wobble is the
tape's, not the players'.

The tuba does not reach the bass stems' centroid and roll-off, and the gate states the gap as an
allowance rather than hiding it. A tuba radiates its upper partials: the University of Iowa tuba's
C2 to A2 at ff centre at 450 to 540 Hz, the fundamental 15 to 24 dB under the strongest partial
(`references/features.tsv`). The corpus's bass stems are mostly bass guitar and what the separator
keeps of a bass line, the low partials. A cardioid's proximity effect at 10 cm brings the tuba down
to 254 Hz; only a low-pass near 300 Hz, which no tape deck is, would bring it to 121. Separating a
solo tuba render with htdemucs_6s does not help either: with no band around it, the separator puts
most of it in `guitar` and `other`.
