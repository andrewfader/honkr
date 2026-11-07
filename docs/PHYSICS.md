# What HONKR simulates

Every instrument is one loop: a **mouthpiece** (a reed, the lips, an air jet) driving the **air
column** of the bore, which returns its pressure wave a round trip later and radiates the rest
from the bell or the open tone holes. The air column is a digital waveguide (a delay line of the
round trip's length with the bell's reflection in it); the mouthpiece is the nonlinearity that
feeds it. The physics lives in `src/dsp/horn.hpp`; the instruments, their fingering, the
calibration and the players in `src/dsp/winds.hpp`.

## The mouthpieces

### The reed (clarinets, saxophones, bassoon, the drones)

The reed is a pressure-controlled valve that the mouth's pressure closes. With pressures in units
of the pressure that shuts the reed against the lay and the pressure difference `dp` across it,
the volume flow is Bernoulli's through the reed's opening:

    u = zeta (1 - x) sqrt(dp)          (x: the reed's displacement, 1 = shut)

where `zeta` is the embouchure parameter (the opening's size against the bore's impedance;
Kergomard; Chaigne and Kergomard, *Acoustics of Musical Instruments*, ch. 9). The reed is a mass on
a spring resonant at a few kilohertz (2.6 kHz for the clarinet) with a Q of 3: its displacement is
last sample's pressure difference through that resonance. Given the opening, the mouthpiece
pressure `p = 2 p- + u(p)` (the incoming wave doubled at the closed end, plus the flow) has a closed
form in `sqrt|dp|` (Guillemain, Kergomard and Voinier 2005), so it is solved exactly each sample
with no iteration.

For these instruments the bore is given as its acoustic resonances, a modal bore (Silva,
Kergomard, Vergez and Gilbert 2014; Chaigne and Kergomard ch. 9): the n-th resonance at n times
the note, a damped resonator whose peak impedance is `peak n^-tilt` (on a cylinder, closed at the
reed and open at the bell, the even ones all but absent: its quarter-wave modes are the odd
harmonics), falling as `(f / cutoff)^-4` past the tone holes' cutoff, with a Q of `q sqrt(n)` (the
walls' losses grow as the root of the frequency). Each resonance is a complex one-pole, exact at
its own frequency. The mouthpiece's pressure is then what the resonances carry over from the last
sample plus their instantaneous response to this sample's flow, `p = v + zi u`, which has the same
closed form as above, so the reed is still solved exactly. Up to 32 resonances below 0.42 of the
sample rate, retuned every 16 samples as the pitch moves.

The modal bore replaced two waveguides: the clarinets' cylinder and the saxophones' and bassoon's
cone. The cone's waveguide (below) notches the harmonics near multiples of the loop over its apex
part, which no recording shows, and nothing short of a different model removed them; the
resonances sit on the harmonics and cancel none. Their peaks, Q, tilt and cutoff, the reed's
opening and resonance, the breath's range and the bell's corner were fitted to the recordings per
instrument by coordinate descent (`tools/fit/fitmodal.cpp`), with every candidate also blown bare
across the compass at pianissimo, mezzo-forte and fortissimo, in tune and bent a semitone either
way, and penalised for every note that left its pitch: an earlier fit had put the bassoon's reed
resonance near its second resonance, and a few of its notes jumped the octave. Against the
waveguides, the profile error fell from 9.8 to 6.4 dB on the alto saxophone, 9.0 to 6.3 on the
soprano, 8.0 to 6.8 on the bassoon, 10.0 to 8.3 on the clarinet and 11.7 to 9.9 on the bass
clarinet; the odd/even balance's from 8.3 to 5.8, 8.8 to 6.8, 6.3 to 3.0, 6.7 to 5.5 and 7.0 to
4.8. The oboe's fit was marginal and sat at the edge of choking, so the oboe keeps the cone. The
tenor and baritone saxophones take the alto's, with lower cutoffs for their larger bores
(unverified).

The clarinet's even harmonics sit 15 dB and more below their neighbours (`horn_tests`). It starts
to sound between a third and a half of the closing pressure (a lossless cylinder's threshold is a
third; the losses raise it). Unlike the waveguide's, the modal reed does not choke when blown past
the closing pressure: its resonances swing the pressure far enough that the reed reopens on every
cycle. The engine never blows a reed past 1.06 of its fortissimo, so no player reaches it. Blown
alone, the modal reed brightens little with the breath; the brightening the recordings show comes
from the fitted dynamics laws (`engine_tests` holds the clarinet, alto saxophone and bassoon to a
fortissimo 15% brighter than their pianissimo). The Reed parameter moves the reed's opening over
1.6 octaves: a softer reed opens wider, passes more air and buzzes brighter.

### A reed on a cone waveguide (oboe, the chanter)

A cone's wave also travels toward its missing apex. The loop is split at the reed into the part
toward the bell (which reflects inverted) and the part toward the apex (Cook's and Scavone's
conical waveguide); the reed is Smith's reflection-coefficient table (its opening linear in the
pressure difference, clipped). The round trip is a whole period and every harmonic sounds; where
the reed sits along the loop shapes the spectrum: the apex part notches the harmonics near
multiples of the loop over its own length. That share is set per instrument against the
recordings (0.43 of the loop for the saxophones, 0.38 for the oboe, 0.42 for the bassoon), which
moves the notches off the harmonics the real instrument sounds strongest; below about 0.3 the
loop no longer holds its fundamental and jumps to an upper register. The notches are a fault of
the model, not of the instrument: the short apex line is a pipe, whose impedance comes back to
zero every half wavelength, where a real cone's missing apex is closer to an inertance. Three
cures were tried against the recordings and rejected. A fixed apex time (as a mouthpiece of fixed
size would give) put the notches at fixed frequencies the recordings do not have. Damping the
apex's return stopped the reed sounding. The apex as an inertance (a first-order all-pass
reflection, -1 with the apex's delay low down and no comb above) gave the alto and soprano
saxophones 1.5 dB better odd/even balance and profiles, but squealed on a tenth of their low
notes, in tune and bent, where the present loop squeals on none. The cone with the physical
Bernoulli valve was tried and rejected: it did not hold its fundamental. Double reeds are stiffer (a smaller slope),
need more breath to speak, and carry strong formants (the oboe's near 1.1 kHz, the bassoon's near
500 Hz).

### The lips (brass, shofar, didgeridoo)

The lips are an outward-striking valve: the mouth's pressure blows them open. Their opening is
the pressure difference through a resonance the player sets 4% below the note with a Q of 40
(Helmholtz; Elliott and Bowsher 1982; Adachi and Sato 1996), times the lips' compliance; the flow
is the same Bernoulli flow, solved the same exact way. A brass bore's resonances are drawn by the
bell and mouthpiece into nearly a complete harmonic series on the open horn's first resonance
(Benade), so the loop returns its wave upright and is one period of that series long; the lips
choose which partial sounds. `horn_tests` sets the lips for each of the 2nd to 6th partials on one
trumpet bore and hears each within 30 cents.

**Brassiness.** In a long narrow bore a loud wave steepens as it travels, its crests (compressed,
warmer air) running ahead of its troughs, until at fortissimo it is close to a shock at the bell
(Hirschberg, Gilbert, Msallam and Wijnands 1996). The radiated wave is read through a delay
shortened at its crests and lengthened at its troughs (Msallam et al. 2000), never running
backwards (where a crest has caught its trough it is a shock and steepens no further), with
four-point interpolation (a linear read's dull treble at a fractional delay muted the pianissimo
that the steepening should leave alone). The amount is the bore's: 400 µs per unit of pressure
for the trumpet, 250 for the trombone and tuba, 150 for the
conical alphorn, none for the didgeridoo. The trumpet's spectral centroid rises by more than 40%
from pianissimo to fortissimo with it and by less than 20% without it, while its pianissimo is
unchanged (`horn_tests`).

### The jet (flutes, recorder, whistle, shakuhachi, pan pipes)

An air jet leaves the player's lips (or a windway), is deflected by the acoustic flow in the mouth
of the pipe, and arrives at the edge a travel time later, where it switches its flow into or out of
the pipe through a saturating curve, off-centre so the even harmonics come (Fletcher; Verge;
Coltman; Cook). The travel takes most of a period for the fundamental; the jet's speed goes as the
root of the pressure, so harder blowing shortens it and raises the pitch (about 20 cents over a
flute's dynamic range, as players know), and a jet half as long overblows the pipe to its octave
(`horn_tests`). An open pipe reflects inverted at both ends (every harmonic); a stopped pipe once
upright (the pan flute's odd harmonics).

### The vessel (ocarina)

The same jet drives a Helmholtz resonator: one air mass in the vessel's mouth bouncing on the air
spring of its volume, a single resonance with a Q of 80, so the tone is nearly a pure sine (its 2nd
and 3rd harmonics 25 dB and more down). Like the real thing, it is breath-sensitive: about 40 cents
from pianissimo to fortissimo.

## The bell and the body

The bell's reflection is two poles of low-pass at the instrument's cutoff (above it the bore
stops reflecting and radiates) and the walls' loss. A bell radiates its low frequencies poorly:
the radiated wave rises toward a corner of its own (a high-pass, one or two poles), fitted on the
brass to the recordings' harmonic profiles. The tuba's is at 490 Hz with two poles, so at
fortissimo its fundamental is 18 dB under the 4th and 5th harmonics, as recorded; the
trombone's at 670 Hz, the horn's at 710, the trumpet's at 1140 with two. The body's colour is two peaking filters per instrument. Mutes are
filters on what the brass radiates (straight: high-passed with a nasal peak near 1.8 kHz; cup:
covered; Harmon: thin with a strong peak near 1.7 kHz; bucket: velvety) with their losses; the
hand in a horn's bell, or a plunger, closes a low-pass from 8 kHz down to 700 Hz.

## Fingering

Woodwind keys and the flutes' holes are cut in tune: the bore is the sounding note's length.

A valve adds a crook of tubing cut to lower the open horn by its own interval (the second valve a
semitone, the first a tone, the third three semitones, a fourth valve a fourth). Pressed together
their crooks add, which is too little for the combined interval: 1+3 comes up 30 cents sharp and
1+2+3 about 50 (Benade ch. 20; Campbell, Gilbert and Myers ch. 6). Each note takes the fingering a
player uses: the highest partial that the fewest semitones of valves reach, skipping the 7th, 11th,
13th and 14th partials, which are far out of tune, unless nothing else will do (the trumpet's chart
in `winds_tests`). The double horn plays on its F side below C#4 and on its B-flat side above.

The trombone's slide is continuous, so its seven positions are in tune; its F attachment adds a
fourth of tubing, which fills the gap between the pedal B-flat and the second partial's E.

The harmonic series is not tempered: the 5th partial is 14 cents flat, the 7th 31. **Lipping** is
how far the player bends each note to the tempered pitch: at full (the default) the lips do it, at
none you hear the horn's own intonation and the valves' compromise (`winds_tests`).

Natural horns (bugle in G, alphorn in F#, shofar) have no valves: a key plays the partial nearest
it. The alphorn's 11th partial is its "fa", a quarter-tone above the fourth of its scale; the
shofar's partials are stretched, each 28 cents sharper than the series. The Highland chanter has
nine notes, low G to high A with C# and F#; other keys play the nearest.

## Calibration

The loop's own filters (the bell's low-pass and the DC blocker) add phase delay, and the reed,
lips or jet pull the pitch. For brass the filters' delay is computed exactly for each note
(`filter_delay`); what is left, and for the other instruments all of it, is measured: when a
sample rate is first used, every instrument is played for 0.6 s every two semitones across its
compass at a mezzo-forte breath, its pitch read by autocorrelation, and its bore lengthened or
shortened until it sounds on its note (three passes). Brass residuals are interpolated in pitch;
the filters' part is exact per partial, so a change of partial between two calibrated notes does
not throw the tuning off. The calibration takes about a second at 48 kHz in a release build (on two
threads, at activation, never on the audio thread) and is shared by every instance in the process.
`horn_tests` plays every instrument's lowest, middle and highest notes through the player and
holds them within 25 cents (35 for the flutes, whose pitch rises with the breath as a flute's
does, and for the tuba's lowest notes).

## Dynamics

Compared with the University of Iowa recordings (references/README.md), real instruments get 8
to 14 dB louder and 1.2 to 2.4 times brighter (the harmonics' centroid) from pianissimo to
fortissimo. The loops alone do not: a reed or jet loop jumps to nearly its full amplitude just
above its blowing threshold, so on breath pressure alone a reed or a flute gained 0 to 4 dB and
grew duller. What players actually do, and what HONKR now does:

- **A clarinettist closes the reed down with the lip for a pianissimo.** On the cylinder reeds
  the opening (zeta) falls to half at the softest breath and returns at fortissimo: less flow, a
  quieter, darker note. (Further than half and the threshold rises above a pianissimo breath and
  the upper notes stop speaking.)
- **A flautist keeps the jet's travel constant.** Blowing harder speeds the jet, which shortens
  its travel across the mouth and would drag the pitch sharp and soon overblow; players bring the
  lips closer to the edge to compensate (Fletcher). HONKR scales the jet's travel back by the
  pressure's fourth root, which lets the flute and alto flute use a breath window of 0.4 to 1.8
  (from 0.8 to 1.04) and still play in tune.
- **The jet's offset from the edge grows with the jet.** Held fixed, a hard-blown jet saturates
  symmetrically and turns odd-harmonic; in proportion to the jet, a fortissimo keeps the even
  harmonics a flute has.

Those are physics. Three more laws are **fitted to the recordings**, not derived, because the
loops' saturation is a property of these minimal models rather than of the instruments: the
radiated level grows as the mouth's pressure to a fitted power; a treble shelf at 1.5 times
the note's pitch is set to a fitted level at every dynamic and cut by a fitted amount at
pianissimo; and a low-pass (two or four poles) whose cutoff, in harmonics of the note, opens
from a fitted value at pianissimo to one at fortissimo. The low-pass is there because some loops
radiate far more upper harmonics than the instrument: the brass lips' valve is shut at rest and
slaps shut on every cycle however softly it is blown, so its tone is as buzzy just above the
threshold as at fortissimo (a rest opening was tried: the lips then either buzz as before or
fall into a chaotic regime), and the flutes' jet sounds its upper harmonics 15 to 20 dB too
strong. It is fitted to every recorded note's harmonic profile (the trumpet's and flutes' is a
fixed 3 harmonics, the trumpet's 2.4 to 2.7; the horn's opens from 2.7 to 7, the trombone's from
7 to beyond the tenth, the clarinet's from 1.5 to 30) and opens while
the tongue's burst lasts, since only the steady tone was fitted. The level and shelf are fitted
per instrument (`dynamics_table` in `src/dsp/winds.hpp`) at one note
in the middle of each recorded octave, to the recordings' gain from pp to ff and their brightness
at pp and at ff; instruments without a recording take their family's. The brass laws and bell
corners were fitted together, by coordinate descent over all of them per instrument
(`tools/fit/fitbrass.cpp`). The tuba was recorded at fortissimo only: its pianissimo treble is
set to the trombone's, unverified. The cone reeds' radiation corners and laws were refitted the
same way: the bassoon's corner is 510 Hz and the oboe's 820, both two-pole, with a treble shelf of
26 dB, which brings their fortissimo brightness to the recordings' (the bassoon within 0.11
octaves, the oboe 0.26); the saxophones' corners are 285 Hz (alto) and 320 (soprano), one-pole,
and the tenor's and baritone's (240 and 200) follow the bell's size, unverified. The Highland pipes
have no dynamics (the bag's pressure is steady); the didgeridoo keeps its own.

## The players

The breath is the mouth's pressure: from the key's velocity (scaled by Dynamics) or a controller
held for the whole note, mapped from silence through the instrument's pianissimo to its
fortissimo, times the Breath parameter. The tongue lets the pressure behind it go (an overshoot of
up to 35%, 12% on a flute, decaying over 30 ms, with a burst of turbulence); a reed's pressure is
never pushed past its choke. Attack and release are the breath's rise and fall. Vibrato comes in
after its delay and fades in over a quarter second; a reed's or the lips' vibrato bends the pitch,
a flute's breath vibrato is mostly loudness. Growl and flutter-tonguing rattle the pressure at 25
to 35 Hz with turbulence. Key pads and valves click (band-passed noise at the instrument's own
frequency) as the fingers move.

The breath's turbulence is heard two ways. Some enters the loop at the mouth, where it stirs the
oscillation; that is kept small, since noise enough to sound as breathy as the recordings jitters
the pitch (a bent alto sax wandered 18 cents). The rest is added to the radiated sound: a jet's
turbulence radiated straight from the embouchure, a reed's or the lips' through the bore. Its
amplitude goes as the pressure squared (turbulence's power grows steeply with the flow's speed),
it is low-passed at the tenth harmonic, and its level is fitted per instrument to the recordings'
noise between the harmonics (the `hiss` of `dynamics_table`); the Breath noise parameter scales
both.

A bend (the wheel, over the Bend range) is tuned as a fingered note at the bent pitch would be:
the calibration's correction for the loop filters' delay and the reed's pull changes a few cents a
semitone, and a 7-semitone bend that ignored it came out 25 cents flat.

Solo play is one player: a key pressed while another is held is slurred, the bore retuned over the
time the mechanism takes (6 ms for keys, 10 for valves, 45 for the slide, or the Glide if longer)
with no new tongue; releasing the top key returns to the one still held. Section play gives every
key its own player. Up to four players double each note, each tuned up to the Spread apart (plus a
couple of cents of their own), entering up to the Timing apart, breathing a few percent
differently, and seated across the stereo width.

## What is not simulated

- Tone holes as acoustic elements: the bore is the sounding note's length, not a pipe with open
  and closed holes, so cross-fingering, half-holing and multiphonics are not there, and registers
  are not overblown (except a flute's jet, which can be).
- A real conical bore with a physical reed: the cone uses the reflection-coefficient reed (see
  above); its threshold and choke are those of that table, not of Bernoulli flow, and its
  fortissimo is too pure (see Dynamics).
- A real pianissimo: the level, treble and roll-off laws of Dynamics are fitted, not simulated;
  the brass lips have no rest opening, so they never play the near-sine a real pianissimo is.
- The vocal tract's coupling to the reed or lips (except the didgeridoo's throat, which is a pair
  of formant filters on the output); bends made in the throat; the player's ear correcting the
  pitch as a note goes.
- The bell's directivity, the mute's change to the bore's resonances (mutes are filters), the
  shofar's irregular, curved bore beyond its stretched partials.
- Air temperature and humidity inside the instrument (the room's air is simulated; the breath is
  not).
