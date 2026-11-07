# Reference recordings

HONKR is compared, note by note, with recordings of the real instruments, so "it sounds like a
clarinet" is a measurement and not an opinion.

**The recordings** are the University of Iowa Electronic Music Studios' Musical Instrument
Samples (Lawrence Fritts, https://theremin.music.uiowa.edu/MIS.html), which "may be downloaded
and used for any projects, without restrictions". They were made in an anechoic chamber with one
Neumann KM 84 at 5 ft, at 16 bit/44.1 kHz, as chromatic scales at pp, mf and ff with the input
level never changed between dynamics, so the level differences between dynamics are real. One
octave in the middle of each instrument is used, without vibrato where Iowa offers it:
`manifest.tsv` lists the 35 files (12 instruments; Iowa has no ff trombone). They cover the
clarinet, bass clarinet, soprano and alto sax, oboe, bassoon, flute, alto flute, French horn,
trumpet, trombone and tuba. The other fifteen HONKR instruments have no reference yet.

**Files here**

| File | What | Committed |
|---|---|---|
| `manifest.tsv` | which recordings, at which dynamic | yes |
| `checksums.sha256` | their SHA-256, pinned | yes |
| `features.tsv` | the features of every note of them (375 notes) | yes: numbers measured from the recordings, small |
| `baseline.tsv` | HONKR's scores against them | yes |
| the recordings | 151 MB of AIFF | no: `~/.cache/honkr-references` |

**The features** (`tests/timbre.hpp`), each on the steady middle half of a note: the first ten
harmonics' levels against the strongest; their centroid in harmonic numbers (brightness); the odd
harmonics' energy over the even ones'; the noise against the harmonics, from the spectrum's mean
power between the first ten harmonics (more than 0.35 of their spacing from any), so neither
harmonics above the tenth nor a player's slow pitch drift count as noise;
the attack (10% to 90%); and the level, used only as differences between dynamics.

**The scores** (`tests/reference_match.hpp`), per instrument, each an error where 0 is a match:
the harmonic profile's RMS difference (dB), brightness (|log2| of the centroid ratio), the odd/even
balance (dB), noise (dB, both floored at -55), and the dynamics: how much louder (dB) and brighter (log2) the loudest
dynamic is than the softest, against the recording. HONKR plays each note dry (no room, the
microphone on the bell), with the instrument's defaults and no vibrato, at velocity 0.2 for pp,
0.55 for mf and 0.95 for ff.

**The ratchet.** `reference_tests` (in `ctest`) replays every note and fails if any instrument
scores worse than `baseline.tsv` on any measure (beyond a small slack for rounding). It needs no
network. When a change improves the match, refresh the baseline so the gain is kept:

    build-release/honkr-reference match              # the table, marked better/same/WORSE against the baseline
    build-release/honkr-reference match --notes      # every note, HONKR / recording
    build-release/honkr-reference match --update-baseline

To change the recordings: edit `manifest.tsv`, then

    tools/references/fetch.py --pin                  # download into the cache, pin the checksums
    build-release/honkr-reference extract            # rewrite features.tsv
    build-release/honkr-reference match --update-baseline

`fetch.py` without `--pin` verifies the downloads against the pinned checksums.
