# Third-party notices

## CLAP

The CLAP headers (free-audio/clap, tag 1.2.7) are fetched at configure time. MIT License,
Copyright (c) 2021 Alexandre BIQUE.

## Editor engine and test harness

The platform-neutral editor (`src/ui/editor.hpp`, the canvases and window backends), the CLAP
shell, the CLAP test harness, the audio measurements, the temperaments and the room and air model
(`src/dsp/air.hpp`) are adapted from the same author's SCHRODR
(https://github.com/andrewfader/schrodr), FUZZBOI (https://github.com/andrewfader/fuzzboi) and
YANES (https://github.com/andrewfader/yanes) projects, under the same source-available licence as
this repository.

## Published data

The instruments' compasses, valve intervals, harmonic series and the physical models' forms are
taken from the published literature named in each source file's header comment and in
docs/PHYSICS.md (Fletcher and Rossing, Benade, Chaigne and Kergomard, Guillemain, Kergomard and
Voinier, Smith, Scavone, Cook, Adachi and Sato, Elliott and Bowsher, Hirschberg et al., Msallam et
al., Campbell, Gilbert and Myers, Verge, Coltman); none of it is code.

## Reference recordings

The reference recordings are the University of Iowa Electronic Music Studios' Musical Instrument
Samples by Lawrence Fritts (https://theremin.music.uiowa.edu/MIS.html), freely available and
usable "for any projects, without restrictions". They are downloaded by
`tools/references/fetch.py` and not redistributed; `references/features.tsv` holds numbers
measured from them.
