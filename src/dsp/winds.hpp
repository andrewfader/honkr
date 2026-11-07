// The wind section: which instruments there are, how each one's notes are fingered (keys, valves,
// a slide, the natural harmonic series of a horn without valves, a bagpipe chanter's nine
// notes), and the players who blow them: tonguing, breath, vibrato, growl, mutes and the hand in
// the bell, alone and legato or several at once. Each player plays one horn (horn.hpp).
//
// Brass fingering is the instrument's own arithmetic. A valve adds a crook of tubing that lowers
// the open horn by its interval (the second valve a semitone, the first a tone, the third three
// semitones, a fourth valve a fourth); valves pressed together add their tubing, which is too
// short for the combined interval, so 1+3 sounds about 30 cents sharp and 1+2+3 about 50 (Benade
// ch. 20; Campbell, Gilbert and Myers, The Science of Brass Instruments ch. 6). The player's
// lips choose the partial of the harmonic series, whose 5th and 7th partials sit 14 and 31
// cents flat of equal temperament. "Lipping" is how far the player bends each note back to the
// tempered pitch; at none, the horn's own intonation is heard. A trombone's slide is
// continuous, so its positions are in tune, and its F attachment fills the gap below the second
// partial. A natural horn (bugle, alphorn, shofar) has only its harmonic series: a key plays the
// partial nearest it.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "core.hpp"
#include "envelopes.hpp"
#include "filters.hpp"
#include "horn.hpp"
#include "temperament.hpp"

namespace honkr::dsp::winds {

using horn::Bore;
using horn::Mouth;

enum class Fingering : uint8_t { Keys, Valves, Slide, Natural, Chanter };
// How the player's vibrato is made: the jaw on a reed or the lips (pitch), or the breath (the
// flute's, mostly loudness).
enum class Family : uint8_t { Woodwind, Flute, Brass };

struct Formant { double hz = 1000.0, q = 1.0, db = 0.0; };

struct Instrument {
  const char* name;
  Family family;
  horn::Acoustics acoustics;
  Fingering fingering;
  int low, high;                 // the compass, at concert pitch
  double f1 = 0.0;               // brass: the open bore's first resonance, at A = 440
  int valves = 3;                // 3 or 4; for a slide, the semitones its trigger adds (0 for none)
  int min_partial = 2, max_partial = 8;
  double pressure_soft = 0.5;    // the mouth's pressure for pianissimo, in the model's units
  double pressure_loud = 0.9;    // and for fortissimo
  double noise = 0.02;           // the breath's turbulence against the pressure
  double stretch_cents = 0.0;    // natural horns: how far each partial up sits sharp of the series
  std::array<Formant, 2> body{}; // the body's and bell's radiation colour
  double click_hz = 800.0;       // where its key pads or valves click
  double f1_high = 0.0;          // a double horn's Bb side (0: none)
  int switch_key = 128;          // and the note from which the thumb valve takes it
  bool takes_mute = true;        // brass: whether a mute or a hand goes in its bell
};

// How an instrument's sound grows from pianissimo to fortissimo beyond what its loop does on its
// own (the loops saturate within a few decibels of the blowing threshold, while a player also
// shapes the dynamic with the lips): the radiated level as the mouth's pressure to a power, and
// a treble shelf opening from pianissimo to fortissimo, and a low-pass whose cutoff (in
// harmonics of the note) opens from pianissimo to fortissimo, for the loops that radiate more
// upper harmonics than the real instrument (the brass lips' valve shuts on every cycle however
// softly it is blown). All are fitted to the University of Iowa recordings
// (references/README.md); instruments without a recording take their family's.
struct Dynamics { double level_exponent = 0.0; double tilt_db = 0.0; double treble_db = 0.0; double lp_pp = 0.0; double lp_ff = 0.0; int lp_poles = 2; double hiss = 0.0; };
inline double g_dyn_k = -1.0, g_dyn_tilt = -1.0, g_dyn_treble = -100.0, g_dyn_lp_pp = -1.0, g_dyn_lp_ff = -1.0;  // experiment overrides
inline int g_dyn_lp_order = -1;  // experiment override
inline double g_hiss = -1.0;  // experiment override

constexpr int kInstrumentCount = 27;

// Builders keep the table readable: an acoustics value with the fields that differ.
inline horn::Acoustics cylinder_reed(double zeta, double reed_hz, double bell_hz, double loss) {
  horn::Acoustics a; a.mouth = Mouth::Reed; a.bore = Bore::Cylinder; a.zeta = zeta; a.valve_hz = reed_hz; a.valve_q = 3.0;
  a.bell_hz = bell_hz; a.loss = loss; a.radiate_hz = 250.0; return a;
}
inline horn::Acoustics cone_reed(double slope, double apex, double bell_hz, double loss, double radiate_hz = 200.0, int radiate_poles = 1) {
  horn::Acoustics a; a.mouth = Mouth::Reed; a.bore = Bore::Cone; a.reed_slope = slope; a.apex = apex;
  a.bell_hz = bell_hz; a.loss = loss; a.radiate_hz = radiate_hz; a.radiate_poles = radiate_poles; return a;
}
// A reed on a bore given as its resonances (horn.hpp, modal).
inline horn::Acoustics modal_reed(double zeta, double reed_hz, double peak, double q, double tilt, double cutoff_hz, double even, double radiate_hz, int radiate_poles = 1) {
  horn::Acoustics a; a.mouth = Mouth::Reed; a.bore = Bore::Modal; a.zeta = zeta; a.valve_hz = reed_hz; a.valve_q = 3.0;
  a.modal_peak = peak; a.modal_q = q; a.modal_tilt = tilt; a.modal_cutoff = cutoff_hz; a.modal_even = even;
  a.radiate_hz = radiate_hz; a.radiate_poles = radiate_poles; a.loss = 1.0; return a;
}
inline horn::Acoustics lips(double zeta, double bell_hz, double loss, double radiate_hz, double steepening_us, int radiate_poles = 1) {
  horn::Acoustics a; a.mouth = Mouth::Lips; a.bore = Bore::Brass; a.zeta = zeta; a.valve_q = 30.0; a.lip_gain = 3.0; a.steepening_us = steepening_us;
  a.bell_hz = bell_hz; a.loss = loss; a.radiate_hz = radiate_hz; a.radiate_poles = radiate_poles; return a;
}
inline horn::Acoustics jet(Bore bore, double ratio, double drive, double offset, double bell_hz, double loss) {
  horn::Acoustics a; a.mouth = Mouth::Jet; a.bore = bore; a.jet_ratio = ratio; a.jet_drive = drive; a.jet_offset = offset;
  a.bell_hz = bell_hz; a.loss = loss; a.radiate_hz = 150.0; return a;
}
inline horn::Acoustics vessel(double q) {
  horn::Acoustics a; a.mouth = Mouth::Vessel; a.bore = Bore::Helmholtz; a.jet_ratio = 0.5; a.jet_drive = 0.9; a.jet_offset = 0.8;
  a.vessel_q = q; a.radiate_hz = 150.0; return a;
}

inline const std::array<Instrument, kInstrumentCount>& instruments() {
  using F = Fingering;
  static const std::array<Instrument, kInstrumentCount> table{{
      // --- single reeds -------------------------------------------------------------------------------
      {"Clarinet", Family::Woodwind, modal_reed(0.591, 1730, 67.5, 20, 1.1, 1220, 0.0, 204, 2), F::Keys, 50, 84, 0, 0, 0, 0, 0.64, 0.95, 0.03, 0, {{{1500, 1.2, 2.0}, {3200, 1.5, -3.0}}}, 900},
      {"Bass clarinet", Family::Woodwind, modal_reed(0.40, 2250, 30, 8.89, 0.2, 900, 0.05, 250), F::Keys, 34, 72, 0, 0, 0, 0, 0.52, 0.65, 0.03, 0, {{{600, 1.0, 3.0}, {2000, 1.5, -3.0}}}, 500},
      {"Soprano sax", Family::Woodwind, modal_reed(0.178, 2450, 55, 5.9, 1.1, 2000, 1.0, 142, 2), F::Keys, 56, 88, 0, 0, 0, 0, 0.61, 0.96, 0.01, 0, {{{1200, 1.2, 3.0}, {3000, 1.5, 2.0}}}, 900},
      {"Alto sax", Family::Woodwind, modal_reed(0.30, 8880, 45, 9.85, 0.8, 3740, 1.0, 11), F::Keys, 49, 81, 0, 0, 0, 0, 0.7, 0.9, 0.03, 0, {{{800, 1.2, 3.0}, {2500, 1.5, 2.0}}}, 700},
      {"Tenor sax", Family::Woodwind, modal_reed(0.30, 6300, 45, 9.85, 0.8, 2800, 1.0, 11), F::Keys, 44, 76, 0, 0, 0, 0, 0.7, 0.9, 0.035, 0, {{{600, 1.2, 3.0}, {2000, 1.5, 2.0}}}, 550},
      {"Baritone sax", Family::Woodwind, modal_reed(0.30, 4700, 45, 9.85, 0.8, 2000, 1.0, 11), F::Keys, 36, 68, 0, 0, 0, 0, 0.7, 0.9, 0.035, 0, {{{400, 1.2, 3.0}, {1500, 1.5, 2.0}}}, 400},
      // --- double reeds and the pipes ------------------------------------------------------------------
      {"Oboe", Family::Woodwind, cone_reed(0.22, 0.38, 4000, 0.94, 820, 2), F::Keys, 58, 91, 0, 0, 0, 0, 0.95, 1.25, 0.02, 0, {{{1100, 2.5, 7.0}, {2800, 2.0, 4.0}}}, 1200},
      {"Bassoon", Family::Woodwind, modal_reed(0.192, 1800, 67.5, 30, 1.02, 747, 1.0, 417, 2), F::Keys, 34, 75, 0, 0, 0, 0, 0.66, 0.82, 0.02, 0, {{{480, 2.0, 7.0}, {1200, 2.0, 4.0}}}, 500},
      {"Highland bagpipe", Family::Woodwind, cone_reed(0.26, 0.35, 5000, 0.95), F::Chanter, 67, 81, 0, 0, 0, 0, 0.8, 0.95, 0.01, 0, {{{1300, 2.0, 6.0}, {3000, 2.0, 4.0}}}, 1500},
      // --- flutes --------------------------------------------------------------------------------------
      {"Flute", Family::Flute, jet(Bore::Open, 0.93, 0.9, 0.8, 14000, 0.85), F::Keys, 60, 90, 0, 0, 0, 0, 0.4, 1.8, 0.06, 0, {{{1000, 1.0, 0.0}, {3000, 1.0, 0.0}}}, 2000},
      {"Piccolo", Family::Flute, jet(Bore::Open, 0.93, 0.9, 0.8, 20000, 0.85), F::Keys, 74, 98, 0, 0, 0, 0, 0.8, 1.04, 0.05, 0, {{{2500, 1.0, 0.0}, {5000, 1.0, 0.0}}}, 2800},
      {"Alto flute", Family::Flute, jet(Bore::Open, 0.93, 0.9, 0.8, 9000, 0.85), F::Keys, 59, 86, 0, 0, 0, 0, 0.4, 1.8, 0.08, 0, {{{700, 1.0, 1.0}, {2000, 1.0, -2.0}}}, 1500},
      {"Recorder", Family::Flute, jet(Bore::Open, 0.93, 1.0, 0.5, 14000, 0.85), F::Keys, 65, 91, 0, 0, 0, 0, 0.78, 1.02, 0.03, 0, {{{1500, 1.0, 2.0}, {3500, 1.0, 0.0}}}, 0},
      {"Tin whistle", Family::Flute, jet(Bore::Open, 0.93, 1.0, 0.6, 16000, 0.85), F::Keys, 74, 96, 0, 0, 0, 0, 0.78, 1.02, 0.04, 0, {{{2500, 1.0, 2.0}, {5000, 1.0, 0.0}}}, 0},
      {"Pan flute", Family::Flute, jet(Bore::Stopped, 0.93, 0.9, 0.5, 10000, 0.88), F::Keys, 55, 91, 0, 0, 0, 0, 0.85, 1.12, 0.09, 0, {{{1000, 1.0, 0.0}, {3000, 1.0, 0.0}}}, 0},
      {"Shakuhachi", Family::Flute, jet(Bore::Open, 0.93, 0.9, 0.9, 9000, 0.85), F::Keys, 62, 86, 0, 0, 0, 0, 0.8, 1.04, 0.16, 0, {{{900, 1.0, 1.0}, {2500, 1.0, 0.0}}}, 0},
      {"Ocarina", Family::Flute, vessel(80.0), F::Keys, 69, 89, 0, 0, 0, 0, 0.92, 1.1, 0.04, 0, {{{1000, 1.0, 0.0}, {3000, 1.0, 0.0}}}, 0},
      // --- brass ---------------------------------------------------------------------------------------
      {"Trumpet", Family::Brass, lips(0.5, 2500, 0.90, 1140, 400, 2), F::Valves, 52, 84, 116.54, 3, 2, 8, 0.3, 1.0, 0.01, 0, {{{1200, 1.0, 3.0}, {2600, 1.2, 2.0}}}, 1100},
      {"Flugelhorn", Family::Brass, lips(0.5, 1300, 0.92, 500, 200), F::Valves, 52, 82, 116.54, 3, 2, 8, 0.3, 1.0, 0.015, 0, {{{800, 1.0, 3.0}, {2000, 1.2, -4.0}}}, 1000},
      {"French horn", Family::Brass, lips(0.5, 1600, 0.95, 710, 350), F::Valves, 35, 77, 43.65, 3, 2, 16, 0.3, 1.0, 0.01, 0, {{{450, 1.0, 3.0}, {1800, 0.8, -8.0}}}, 900, 58.27, 61},
      {"Trombone", Family::Brass, lips(0.5, 1800, 0.92, 670, 250), F::Slide, 34, 77, 58.27, 5, 1, 10, 0.3, 1.0, 0.01, 0, {{{600, 1.0, 3.0}, {1800, 1.2, 2.0}}}, 0},
      {"Euphonium", Family::Brass, lips(0.5, 1100, 0.93, 300, 250), F::Valves, 29, 75, 58.27, 4, 1, 9, 0.3, 1.0, 0.01, 0, {{{400, 1.0, 3.0}, {1500, 1.0, -3.0}}}, 700},
      {"Tuba", Family::Brass, lips(0.5, 700, 0.94, 490, 250, 2), F::Valves, 26, 65, 29.14, 4, 1, 8, 0.3, 1.0, 0.012, 0, {{{220, 1.0, 3.0}, {900, 1.0, -3.0}}}, 500},
      {"Bugle", Family::Brass, lips(0.5, 2000, 0.90, 700, 350), F::Natural, 55, 79, 98.0, 0, 2, 8, 0.3, 1.0, 0.012, 0, {{{1100, 1.0, 3.0}, {2400, 1.2, 1.0}}}, 0},
      {"Alphorn", Family::Brass, lips(0.5, 1000, 0.94, 250, 150), F::Natural, 42, 90, 92.5, 0, 2, 12, 0.3, 1.0, 0.01, 0, {{{500, 1.0, 3.0}, {1500, 1.0, -2.0}}}, 0, 0.0, 128, false},
      {"Shofar", Family::Brass, lips(0.8, 2200, 0.85, 800, 200), F::Natural, 60, 81, 175.0, 0, 2, 4, 0.36, 0.95, 0.05, 28.0, {{{1500, 1.5, 5.0}, {3200, 1.5, 3.0}}}, 0, 0.0, 128, false},
      {"Didgeridoo", Family::Brass, lips(0.6, 600, 0.92, 120, 0), F::Keys, 33, 47, 0, 0, 1, 1, 0.3, 1.0, 0.02, 0, {{{350, 3.0, 9.0}, {1300, 3.0, 9.0}}}, 0, 0.0, 128, false},
  }};
  return table;
}
inline const Instrument& instrument(int i) { return instruments()[static_cast<size_t>(std::clamp(i, 0, kInstrumentCount - 1))]; }

// --- fingering -------------------------------------------------------------------------------------

// What the player does for one note: which partial, which valves (or slide position), and the
// pitch the horn then sounds before any lipping.
struct Fingered {
  int partial = 1;
  int drop = 0;           // semitones the valves or slide lower the open horn
  int valve_mask = 0;     // bit 0 first valve, bit 1 second, bit 2 third, bit 3 fourth (slide: position-1 in bits)
  double natural_hz = 0;  // what the horn sounds with that fingering, by its own intonation
  double hz = 0;          // what the player sounds, after lipping
  int key = 60;           // the note played (a chanter's or natural horn's may differ from the key asked)
};

// The valves for a drop in semitones, as players finger it: second for one, first for two,
// first and second for three (in tune, rather than the third alone), and so on; a fourth valve
// replaces 1+3 for five.
inline int valve_combination(int drop, int valves) {
  static constexpr int kThree[7] = {0, 2, 1, 3, 6, 5, 7};
  if (valves < 4 || drop <= 4) return drop >= 0 && drop <= 6 ? kThree[drop] : 7;
  // With four valves: 5 = 4th, 6 = 2+4, 7 = 1+4, 8 = 1+2+4 (3+4 is sharper), 9 = 2+3+4, 10 = 1+3+4, 11 = all.
  static constexpr int kFour[12] = {0, 2, 1, 3, 6, 8, 10, 9, 11, 14, 13, 15};
  return kFour[std::clamp(drop, 0, 11)];
}
// The tubing's length against the open horn's for a set of valves: each crook is cut for its
// own interval alone, so pressed together they come up short.
inline double valve_length(int mask) {
  static constexpr int kInterval[4] = {2, 1, 3, 5};
  double length = 1.0;
  for (int v = 0; v < 4; ++v) if (mask & (1 << v)) length += std::exp2(kInterval[v] / 12.0) - 1.0;
  return length;
}
// A partial a player avoids when another fingering will do: the 7th, 11th, 13th and 14th are
// far out of tune with the tempered scale.
inline bool awkward_partial(int n) { return n == 7 || n == 11 || n == 13 || n == 14; }

// The Highland chanter's nine notes from the low G (Mixolydian on A, with C# and F#).
constexpr int kChanter[9] = {67, 69, 71, 73, 74, 76, 78, 79, 81};

// `hz_of(key)` gives the tempered pitch of any key (the temperament, the reference A).
// `lipping` (0..1) is how far the player bends the horn's own intonation to it.
template <class HzOf>
Fingered finger(const Instrument& in, int key, double a4, double lipping, HzOf hz_of) {
  Fingered f;
  f.key = key;
  const double scale = a4 / 440.0;
  switch (in.fingering) {
    case Fingering::Keys:
      f.hz = f.natural_hz = hz_of(key);
      return f;
    case Fingering::Chanter: {
      int best = kChanter[0];
      for (int k : kChanter) if (std::abs(k - key) < std::abs(best - key)) best = k;
      f.key = best;
      f.hz = f.natural_hz = hz_of(best);
      return f;
    }
    case Fingering::Natural: {
      const double f1 = in.f1 * scale;
      const double wanted = hz_of(key);
      int n = std::clamp(static_cast<int>(std::lround(wanted / f1)), in.min_partial, in.max_partial);
      // The partial nearest the key in pitch, not in hertz.
      for (int m = in.min_partial; m <= in.max_partial; ++m)
        if (std::abs(std::log2(m * f1 / wanted)) < std::abs(std::log2(n * f1 / wanted))) n = m;
      f.partial = n;
      f.natural_hz = n * f1 * cents_ratio(in.stretch_cents * (n - in.min_partial));
      // The nearest tempered note to what the horn sounds, which lipping bends it toward.
      f.key = static_cast<int>(std::lround(69.0 + 12.0 * std::log2(f.natural_hz / a4)));
      f.hz = f.natural_hz * std::pow(hz_of(f.key) / f.natural_hz, std::clamp(lipping, 0.0, 1.0));
      return f;
    }
    case Fingering::Valves:
    case Fingering::Slide: {
      const bool slide = in.fingering == Fingering::Slide;
      const double f1 = (in.f1_high > 0.0 && key >= in.switch_key ? in.f1_high : in.f1) * scale;
      const double wanted = hz_of(key);
      const int max_drop = slide ? 6 + in.valves : (in.valves >= 4 ? 11 : 6);
      int best_n = -1, best_drop = 99;
      for (int pass = 0; pass < 2 && best_n < 0; ++pass)
        for (int n = in.max_partial; n >= in.min_partial; --n) {
          if (pass == 0 && awkward_partial(n)) continue;
          const double steps = 12.0 * std::log2(n * f1 / wanted);
          const int drop = static_cast<int>(std::lround(steps));
          if (drop < 0 || drop > max_drop || std::abs(steps - drop) > 0.6) continue;
          if (drop < best_drop) { best_drop = drop; best_n = n; }
        }
      if (best_n < 0) {  // out of the horn's reach: the nearest partial, open
        best_n = std::clamp(static_cast<int>(std::lround(wanted / f1)), in.min_partial, in.max_partial);
        best_drop = 0;
      }
      f.partial = best_n;
      f.drop = best_drop;
      double length;
      if (slide) {
        // The slide is continuous and in tune; past the 6th position the F attachment's tubing
        // (a fourth) is in, with the slide set to make up the rest.
        length = std::exp2(best_drop / 12.0);
        f.valve_mask = best_drop;  // the position, less one
      } else {
        f.valve_mask = valve_combination(best_drop, in.valves);
        length = valve_length(f.valve_mask);
      }
      f.natural_hz = best_n * f1 / length;
      f.hz = f.natural_hz * std::pow(wanted / f.natural_hz, std::clamp(lipping, 0.0, 1.0));
      return f;
    }
  }
  return f;
}

// --- tuning the loops -------------------------------------------------------------------------------

// The lips for a partial: tuned a little below the note, where an outward-striking valve sounds
// on the bore's resonance, and tighter (a higher Q) the higher the partial, so they pick it out
// of resonances that crowd closer together up the series. Big low lips are slack and quick.
inline double g_lip_a = 40.0, g_lip_b = 0.0, g_lip_c = 0.04, g_lip_d = 0.0;
inline double g_lip_qf = 1e9, g_trial_s = 0.6;
inline double lip_q(int partial, double hz) { return std::min(g_lip_a + g_lip_b * std::max(1, partial), std::max(4.0, g_lip_qf * hz)); }
inline double lip_ratio(int partial) { return 1.0 - g_lip_c / std::pow(static_cast<double>(std::max(1, partial)), g_lip_d); }

// The loop's nominal delay for a sounding pitch: half a period for a cylinder closed at the reed
// and for a stopped pipe; a period for a cone, an open pipe; for brass, the bore's round trip,
// which is the partial's number of periods.
// The breath's range, with the experiments' overrides (tools/fit; unset below zero).
inline double g_pressure_soft = -1.0, g_pressure_loud = -1.0;
// How far the Reed parameter moves the reed's opening, in octaves from soft to hard.
inline double g_reed_range = 1.6;
inline double soft_of(const Instrument& in) { return g_pressure_soft > 0.0 ? g_pressure_soft : in.pressure_soft; }
inline double loud_of(const Instrument& in) { return g_pressure_loud > 0.0 ? g_pressure_loud : in.pressure_loud; }

inline double nominal_bore(const Instrument& in, double rate, double hz, int partial) {
  const double period = rate / hz;
  switch (in.acoustics.bore) {
    case Bore::Cylinder: case Bore::Stopped: return 0.5 * period;
    case Bore::Brass: return period * std::max(1, partial);
    default: return period;
  }
}

// The phase delay, in samples, that the loop's own filters add at a pitch: the bell's two poles
// of low-pass and the DC blocker. Exact, so a bore can be cut to length for them.
inline double filter_delay(const horn::Acoustics& a, double rate, double hz) {
  const double w = kTau * hz / rate;
  const double bell = std::exp(-kTau * std::clamp(a.bell_hz, 50.0, rate * 0.45) / rate);
  const double dc = 1.0 - kTau * horn::g_dc_hz / rate;
  // arg of (1 - bell)^2 / (1 - bell e^-jw)^2 and of (1 - e^-jw) / (1 - dc e^-jw)
  const double lp = -2.0 * std::atan2(bell * std::sin(w), 1.0 - bell * std::cos(w));
  const double num = std::atan2(std::sin(w), 1.0 - std::cos(w));
  const double den = std::atan2(dc * std::sin(w), 1.0 - dc * std::cos(w));
  return -(lp + num - den) / w;
}

// The loop's filters and the valve add their own delay: the bore is shortened until the horn
// speaks on its note. Found once per sample rate by playing every instrument at points across
// its compass, then interpolated between them (in cents) for every key.
// For experiments (tools/fit): calibrate only this instrument, the rest left uncorrected.
inline int g_calibrate_only = -1;

class Calibration {
 public:
  static constexpr int kStep = 2;
  // The correction (bore against nominal) for an instrument at a pitch on a partial. For brass
  // the filters' delay is computed exactly for the note and only the lips' pull, which varies
  // smoothly with pitch, is interpolated; elsewhere the whole correction is.
  double correction(int instrument, double hz, int partial = 1) const {
    const int i = std::clamp(instrument, 0, kInstrumentCount - 1);
    const auto& row = table_[static_cast<size_t>(i)];
    const double key = 69.0 + 12.0 * std::log2(std::max(hz, 1.0) / 440.0);
    const double pos = std::clamp((key - row.first) / kStep, 0.0, static_cast<double>(row.points.size() - 1));
    const size_t a = std::min(static_cast<size_t>(pos), row.points.size() - 1);
    const size_t b = std::min(a + 1, row.points.size() - 1);
    const double t = pos - static_cast<double>(a);
    const double residual = row.points[a] * std::pow(row.points[b] / row.points[a], t);
    return residual * analytic(instruments()[static_cast<size_t>(i)], rate_, hz, partial);
  }
  // The part of the correction that is exact: the loop filters' delay, for brass.
  static double analytic(const Instrument& in, double rate, double hz, int partial) {
    if (in.acoustics.bore != Bore::Brass) return 1.0;
    const double nominal = nominal_bore(in, rate, hz, partial);
    return (nominal - filter_delay(in.acoustics, rate, hz)) / nominal;
  }
  double rate() const { return rate_; }

  // One calibration per sample rate, shared by every engine in the process.
  static const Calibration& at(double rate) {
    std::lock_guard<std::mutex> guard(cache_lock());
    for (const auto& c : cache()) if (c->rate_ == rate) return *c;
    cache().push_back(std::make_unique<Calibration>(rate));
    return *cache().back();
  }
  // For experiments that change the acoustics (tools/fit): calibrate afresh. Engines holding
  // the old calibration must be prepared again after.
  static void forget() {
    std::lock_guard<std::mutex> guard(cache_lock());
    for (auto& c : cache()) retired().push_back(std::move(c));
    cache().clear();
  }

  // Two threads share the instruments: calibration runs once, when a plug-in is first
  // activated at a sample rate, off the audio thread.
  explicit Calibration(double rate) : rate_(rate) {
    const auto work = [this](int first) {
      for (int i = first; i < kInstrumentCount; i += 2) {
        const Instrument& in = instruments()[static_cast<size_t>(i)];
        Row& row = table_[static_cast<size_t>(i)];
        row.first = in.low;
        for (int key = in.low; key < in.high + kStep; key += kStep) row.points.push_back(g_calibrate_only >= 0 && g_calibrate_only != i ? 1.0 : fit(in, std::min(key, in.high + 2)));
      }
    };
    std::thread other(work, 1);
    work(0);
    other.join();
  }

  // Plays a horn alone at a pitch for a moment and reads what it sounds.
  static double play(const Instrument& in, double rate, double hz, int partial, double correction, double* level = nullptr) {
    horn::Horn h;
    h.prepare(rate, 0.09);
    h.set_acoustics(in.acoustics);
    horn::Tuning t;
    t.bore = nominal_bore(in, rate, hz, partial) * correction;
    t.jet = in.acoustics.jet_ratio * rate / hz;
    if (in.acoustics.mouth == Mouth::Lips) h.set_valve(hz * lip_ratio(partial), lip_q(partial, hz));
    if (in.acoustics.mouth == Mouth::Vessel) { h.set_vessel(hz * correction); t.jet = in.acoustics.jet_ratio * rate / hz; }
    const double p = 0.5 * (soft_of(in) + loud_of(in));
    // Long enough for the slowest (lowest) notes to settle: lips of a high Q at a low pitch
    // take a while to find their partial.
    const int n = static_cast<int>(rate * g_trial_s);
    std::vector<float> x(static_cast<size_t>(n));
    Rng rng(0x51f15eedu);
    for (int i = 0; i < n; ++i) {
      const double env = std::min(1.0, i / (0.01 * rate));
      x[static_cast<size_t>(i)] = h.tick(t, static_cast<float>(p * env), static_cast<float>(0.002 * rng.bipolar()));
    }
    const size_t from = x.size() - static_cast<size_t>(0.15 * rate);
    if (level) { double e = 0; for (size_t i = from; i < x.size(); ++i) e += x[i] * x[i]; *level = std::sqrt(e / static_cast<double>(x.size() - from)); }
    return pitch(x.data() + from, x.size() - from, rate, hz);
  }


 private:
  struct Row { int first = 0; std::vector<double> points; };
  static std::mutex& cache_lock() { static std::mutex m; return m; }
  static std::vector<std::unique_ptr<Calibration>>& cache() { static std::vector<std::unique_ptr<Calibration>> c; return c; }
  static std::vector<std::unique_ptr<Calibration>>& retired() { static std::vector<std::unique_ptr<Calibration>> c; return c; }

  double fit(const Instrument& in, int key) const {
    const double hz = 440.0 * std::exp2((key - 69) / 12.0);
    // Brass is calibrated per partial; the correction depends on the pitch, not on the partial
    // much, so the middle of the instrument's partials stands for all.
    int partial = 1;
    if (in.acoustics.bore == Bore::Brass) {
      const Fingered f = finger(in, key, 440.0, 1.0, [](int k) { return 440.0 * std::exp2((k - 69) / 12.0); });
      partial = f.partial;
    }
    // The filters' delay is known exactly; the lips' (or reed's, or jet's) pull is what is left.
    const bool brass = in.acoustics.bore == Bore::Brass;
    const double exact = Calibration::analytic(in, rate_, hz, partial);
    double c = exact;
    for (int trial = 0; trial < 3; ++trial) {
      const double sounded = play(in, rate_, hz, partial, c);
      if (sounded <= 0.0) break;
      // A trial that locked onto another resonance says nothing about this one.
      if (brass && std::abs(std::log2(sounded / hz)) > 0.06) break;
      // A longer bore sounds lower; a vessel's resonance is raised to raise the note.
      c *= in.acoustics.mouth == Mouth::Vessel ? hz / sounded : sounded / hz;
      if (std::abs(1200.0 * std::log2(sounded / hz)) < 1.0) break;
    }
    return brass ? std::clamp(c / exact, 0.96, 1.04) : std::clamp(c, 0.5, 1.5);
  }

  // The period by normalised autocorrelation over lags within a fourth of the expected one.
  static double pitch(const float* x, size_t n, double rate, double expected) {
    const double period = rate / expected;
    const size_t lo = static_cast<size_t>(std::max(2.0, period * 0.75)), hi = static_cast<size_t>(period * 1.33) + 1;
    if (hi * 2 >= n) return 0.0;
    const size_t len = n - hi;
    double best = -1.0; size_t at = 0;
    std::vector<double> r(hi + 2, 0.0);
    for (size_t lag = lo - 1; lag <= hi + 1; ++lag) {
      double num = 0, e0 = 0, e1 = 0;
      for (size_t i = 0; i < len; ++i) { num += x[i] * x[i + lag]; e0 += x[i] * x[i]; e1 += x[i + lag] * x[i + lag]; }
      r[lag] = num / std::sqrt(std::max(e0 * e1, 1e-30));
    }
    for (size_t lag = lo; lag <= hi; ++lag) if (r[lag] > best) { best = r[lag]; at = lag; }
    if (best < 0.5 || at == 0) return 0.0;
    const double a = r[at - 1], b = r[at], c = r[at + 1];
    const double d = a - 2.0 * b + c;
    const double shift = std::abs(d) > 1e-12 ? std::clamp(0.5 * (a - c) / d, -0.5, 0.5) : 0.0;
    return rate / (static_cast<double>(at) + shift);
  }

  double rate_;
  std::array<Row, kInstrumentCount> table_{};
};

// --- the players ------------------------------------------------------------------------------------

// Experiment knobs for the dynamic embouchure (to be fixed per family once fitted).


enum class Mute : int { Open, Straight, Cup, Harmon, Bucket };
constexpr int kMuteCount = 5;

struct Setup {
  int instrument = 0;
  double a4 = 440.0;
  int temperament = 0, root = 0;
  int transpose = 0;
  double fine_cents = 0.0;
  bool solo = true;           // one note at a time, legato
  double glide_ms = 0.0;
  double bend_range = 2.0;    // semitones
  int players = 1;
  double spread_cents = 8.0;  // the section's tuning spread
  double timing_ms = 15.0;    // and its ragged entries
  double breath = 1.0;        // the player's pressure against the instrument's
  double dynamics = 1.0;      // how far velocity reaches the breath
  double attack_ms = 30.0;
  double release_ms = 60.0;
  double tongue = 0.5;        // the tongue's attack
  double breath_noise = 1.0;  // the turbulence against the instrument's
  double growl = 0.0;
  double reed = 0.5;          // reed stiffness (soft .. hard)
  double embouchure = 0.5;    // the lips' or jet's set against the instrument's (0.5 = its own)
  double lip_cents = 0.0;     // lips: their tuning against the note
  double lipping = 1.0;       // how far the player bends the horn's intonation to the tempered pitch
  double hand = 0.0;          // the hand in the bell, or a plunger
  int mute = 0;
  double key_noise = 0.3;
  double throat = 0.3;        // didgeridoo: the vowel the throat shapes (u .. a .. i)
  bool drones = true;
  double vibrato_cents = 0.0, vibrato_hz = 5.5, vibrato_delay_ms = 250.0;
  double width = 0.6, pan = 0.0;
};

// The fitted dynamics per instrument (see Dynamics), in the order of instruments().
// {level exponent, pianissimo's treble cut dB, treble dB at every dynamic, low-pass cutoff at
// pianissimo and at fortissimo in harmonics (0: none), its poles, the breath's turbulence
// added to the radiated sound}. The level and shelf are
// fitted by matching the gain from pp to ff and the harmonics' centroid at pp and at ff to the
// recordings, at one note in the middle of each recorded octave; the low-pass by the harmonic
// profiles of every recorded note (tools/fit/fitlp.cpp); the rest are their family's.
inline const std::array<Dynamics, kInstrumentCount>& dynamics_table() {
  static const std::array<Dynamics, kInstrumentCount> table{{
      {1.550, 9.4, 12.7, 0.94, 315.0, 2, 0.03},   // clarinet (fitted)
      {0.000, 9.9, 11.8, 0.0, 0.0, 2, 0.05},   // bass clarinet (fitted)
      {0.300, 10.8, 18.0, 2.0, 635.0, 2, 0.01},  // soprano sax (fitted)
      {2.860, 16.0, 5.7, 3.1, 197.0, 2, 0.02},  // alto sax (fitted)
      {2.860, 16.0, 5.7, 3.1, 197.0, 2, 0.015},  // tenor sax (the alto's)
      {2.860, 16.0, 5.7, 3.1, 197.0, 2, 0.015},  // baritone sax (the alto's)
      {4.310, 8.5, 26.0, 1.2, 38.0, 2, 0.03},   // oboe (fitted)
      {1.600, 5.2, 33.0, 3.6, 21.1, 2, 0.07},   // bassoon (fitted)
      {0.000, 0.0, 0.0, 0.0, 0.0, 2, 0.07},    // Highland bagpipe: the bag's pressure is steady, no dynamics
      {0.000, 24.0, 10.9, 3.0, 3.0, 2, 2.0},  // flute (fitted)
      {3.000, 24.0, 10.9, 3.0, 3.0, 2, 1.5},  // piccolo (the flute's brightening; its narrow breath window needs the level law)
      {0.322, 14.0, 13.2, 3.0, 3.0, 2, 1.5},  // alto flute (fitted)
      {2.000, 12.0, 5.0, 3.0, 3.0, 2, 0.75},   // recorder (the flutes', with the narrower range a fipple allows)
      {2.000, 12.0, 5.0, 3.0, 3.0, 2, 0.75},   // tin whistle (as the recorder)
      {3.000, 24.0, 10.9, 3.0, 3.0, 2, 2.0},  // pan flute (the flute's brightening; its narrow breath window needs the level law)
      {3.000, 24.0, 10.9, 3.0, 3.0, 2, 3.0},  // shakuhachi (the flute's brightening; its narrow breath window needs the level law)
      {2.000, 10.0, 0.0, 0.0, 0.0, 2, 0.5},   // ocarina (its breath window is narrow: the level carries the dynamic)
      {0.000, 0.0, -3.0, 2.4, 2.7, 2, 0.008},    // trumpet (fitted)
      {0.000, 0.0, -3.0, 2.4, 2.7, 2, 0.01},    // flugelhorn (the trumpet's)
      {0.170, 9.6, 1.5, 2.7, 7.1, 4, 0.025},   // French horn (fitted)
      {1.050, 6.2, 3.3, 7.1, 68.0, 4, 0.02},    // trombone (fitted)
      {1.050, 6.2, 3.3, 7.1, 68.0, 4, 0.03},    // euphonium (the trombone's)
      {0.000, 24.0, 21.3, 8.0, 8.0, 2, 0.14},    // tuba (fitted at fortissimo, the only dynamic recorded; its pianissimo treble set to the trombone's)
      {0.000, 0.0, -3.0, 2.4, 2.7, 2, 0.01},    // bugle (the trumpet's)
      {0.170, 9.6, 1.5, 2.7, 7.1, 4, 0.025},   // alphorn (the horn's)
      {0.000, 0.0, -3.0, 2.4, 2.7, 2, 0.01},    // shofar (the trumpet's)
      {0.000, 0.0, 0.0},    // didgeridoo: a drone, its dynamics are its own
  }};
  return table;
}
inline Dynamics dynamics_of(int instrument) { return dynamics_table()[static_cast<size_t>(std::clamp(instrument, 0, kInstrumentCount - 1))]; }

// The tempered pitch of a key under a setup's temperament, reference and fine tuning.
struct HzOf {
  int temperament = 0, root = 0;
  double a4 = 440.0, fine_cents = 0.0;
  double operator()(int key) const { return temperament::hz(temperament, root, a4, key) * cents_ratio(fine_cents); }
};

class Section {
 public:
  static constexpr int kMaxPlayers = 24;
  static constexpr int kMaxNotes = 128;
  static constexpr double kLongestBore = 0.075;  // seconds: the tuba's longest crook, with room

  void prepare(double rate) {
    rate_ = rate;
    cal_ = &Calibration::at(rate);
    for (auto& p : players_) p.horn.prepare(rate, kLongestBore);
    for (auto& d : drones_) d.horn.prepare(rate, kLongestBore);
    vibrato_.prepare(rate);
    stack_.reserve(kMaxNotes + 1);
    configured_ = false;
    reset();
  }
  void reset() {
    for (auto& p : players_) silence(p);
    for (auto& d : drones_) silence(d);
    stack_.clear();
    held_.fill(false);
    counter_ = 0;
    controller_ = -1.0;
  }
  void configure(const Setup& s) {
    const bool changed = !configured_ || s.instrument != setup_.instrument;
    setup_ = s;
    configured_ = true;
    if (changed) { for (auto& p : players_) silence(p); for (auto& d : drones_) silence(d); stack_.clear(); held_.fill(false); }
    in_ = &instrument(setup_.instrument);
  }
  const Setup& setup() const { return setup_; }
  const Instrument& current() const { return *in_; }

  // The breath from a controller (breath, expression, the wheel, pressure), 0..1, or negative
  // when the breath comes from the key's velocity.
  void set_controller(double value) { controller_ = value; }
  void set_bend(double semitones) { bend_ = semitones; }
  void set_wheel_vibrato(double cents) { wheel_vibrato_ = cents; }

  void note_on(int key, double velocity, uint32_t seed) {
    if (key < 0 || key > 127) return;
    rng_.seed(seed);
    held_[static_cast<size_t>(key)] = true;
    remove_from_stack(key);
    stack_.push_back(key);
    if (setup_.solo) {
      // Legato: if anyone is still blowing, the fingers move and the breath carries on.
      bool slur = false;
      for (auto& p : players_) if (p.awake && p.held) { slur = true; retarget(p, key, false); }
      if (!slur) start_note(key, velocity);
      else { for (auto& p : players_) if (p.awake && p.held) p.velocity = velocity; }
    } else {
      start_note(key, velocity);
    }
  }
  void note_off(int key) {
    if (key < 0 || key > 127) return;
    held_[static_cast<size_t>(key)] = false;
    remove_from_stack(key);
    if (setup_.solo) {
      bool playing_it = false;
      for (auto& p : players_) if (p.awake && p.held && p.key == key) playing_it = true;
      if (!playing_it) return;
      if (!stack_.empty()) { for (auto& p : players_) if (p.awake && p.held) retarget(p, stack_.back(), false); return; }
      for (auto& p : players_) if (p.awake && p.held) p.held = false;
    } else {
      for (auto& p : players_) if (p.awake && p.held && p.key == key) p.held = false;
    }
  }
  void all_notes_off() { for (auto& p : players_) p.held = false; stack_.clear(); held_.fill(false); }
  void all_sound_off() { reset(); }

  bool awake() const {
    for (const auto& p : players_) if (p.awake) return true;
    for (const auto& d : drones_) if (d.awake) return true;
    return false;
  }
  int sounding() const { int n = 0; for (const auto& p : players_) n += p.awake; return n; }
  bool key_sounding(int key) const { for (const auto& p : players_) if (p.awake && p.key == key) return true; return false; }
  bool any_held() const { for (bool h : held_) if (h) return true; return false; }

  // The pitch a player is sounding on a key, for tests: the fingering after lipping.
  Fingered fingering_for(int key) const { return finger(*in_, fold(key + setup_.transpose), setup_.a4, setup_.lipping, hz_of()); }
  // A note beyond the instrument's compass sounds an octave (or more) inside it.
  int fold(int key) const {
    while (key < in_->low) key += 12;
    while (key > in_->high) key -= 12;
    return key;
  }
  // Read-only views for the editor and tests.
  struct View { bool awake; int key; double pressure; int partial; int valve_mask; double opening; };
  View view(int i) const {
    const auto& p = players_[static_cast<size_t>(std::clamp(i, 0, kMaxPlayers - 1))];
    return {p.awake, p.key, p.pressure, p.fingered.partial, p.fingered.valve_mask, p.horn.opening()};
  }

  void render(float& left, float& right) {
    left = right = 0.0f;
    const double vib_base = setup_.vibrato_cents + wheel_vibrato_;
    const double vib_lfo = vibrato_.next(setup_.vibrato_hz);
    for (auto& p : players_) {
      if (!p.awake) continue;
      const float out = tick(p, vib_base, vib_lfo);
      left += out * p.gain_l;
      right += out * p.gain_r;
    }
    if (in_->fingering == Fingering::Chanter) render_drones(left, right);
    left *= static_cast<float>(kOutputScale); right *= static_cast<float>(kOutputScale);
  }

  static constexpr double kOutputScale = 1.0;

 private:
  struct Player {
    horn::Horn horn;
    bool awake = false, held = false;
    int key = -1;
    double velocity = 0.7;
    Fingered fingered{};
    double bore = 100.0, bore_target = 100.0, bore_step = 0.0;  // the loop's length, gliding
    double bend_seen = 1.0, bend_hz_seen = 0.0, bend_factor = 1.0;  // the calibration at the bent pitch
    double lip_hz = 440.0, lip_target = 440.0, lip_q = 20.0;
    double vessel_hz = 440.0;
    double jet_ratio = 0.93;
    double pressure = 0.0;      // the mouth's pressure, rising and falling
    double detune = 1.0;        // this player's tuning in the section
    double breath_scale = 1.0;
    int delay = 0;              // samples before this player comes in
    int age = 0, since_change = 0;
    double vibrato_phase = 0.0, growl_phase = 0.0;
    float gain_l = 0.7f, gain_r = 0.7f;
    float env = 0.0f;
    float click = 0.0f;
    Biquad body1, body2, mute1, mute2, hand, throat1, throat2, tilt, roll1, roll2, hiss;
    float noise_lp = 0.0f, click_bp1 = 0.0f, click_bp2 = 0.0f;
    uint64_t order = 0;
    int section_index = 0;
    int mute = 0;
  };


  HzOf hz_of() const { return HzOf{setup_.temperament, setup_.root, setup_.a4, setup_.fine_cents}; }

  void remove_from_stack(int key) {
    for (size_t i = 0; i < stack_.size(); ++i) if (stack_[i] == key) { stack_.erase(stack_.begin() + static_cast<long>(i)); --i; }
  }

  // The breath the player wants, 0..1: the key's velocity scaled by Dynamics, or a controller.
  double breath_level(const Player& p) const {
    if (controller_ >= 0.0) return controller_;
    return lerp(0.6, std::clamp(p.velocity, 0.0, 1.0), std::clamp(setup_.dynamics, 0.0, 1.0));
  }
  // The mouth's pressure for a breath level: silence at none, the instrument's pianissimo to
  // fortissimo above.
  double soft_pressure() const { return soft_of(*in_) * setup_.breath; }
  double loud_pressure() const { return loud_of(*in_) * setup_.breath; }
  double pressure_for(double b) const {
    const double soft = soft_pressure() / setup_.breath, loud = loud_pressure() / setup_.breath;
    const double p = b < 0.15 ? soft * b / 0.15 : lerp(soft, loud, (b - 0.15) / 0.85);
    return p * setup_.breath;
  }

  Player* free_player() {
    Player* best = nullptr;
    for (auto& p : players_) if (!p.awake) return &p;
    // Steal the oldest released player, then the oldest of all.
    for (auto& p : players_) if (!p.held && (!best || p.order < best->order)) best = &p;
    if (best) return best;
    for (auto& p : players_) if (!best || p.order < best->order) best = &p;
    return best;
  }

  void start_note(int key, double velocity) {
    const int n = std::clamp(setup_.players, 1, 4);
    if (setup_.solo) for (auto& p : players_) if (p.awake) p.held = false;
    for (int i = 0; i < n; ++i) {
      Player* p = free_player();
      if (!p) return;
      silence(*p);
      p->awake = true; p->held = true;
      p->velocity = velocity;
      p->order = ++counter_;
      p->section_index = i;
      // A section is never quite together: each player a few cents and milliseconds apart.
      const double spread = n > 1 ? (i - 0.5 * (n - 1)) / std::max(1.0, 0.5 * (n - 1)) : 0.0;
      p->detune = cents_ratio(setup_.spread_cents * spread + (n > 1 ? 2.0 * rng_.bipolar() : 0.0));
      p->delay = i == 0 ? 0 : static_cast<int>(std::abs(rng_.bipolar()) * setup_.timing_ms * 0.001 * rate_);
      p->breath_scale = n > 1 ? 1.0 + 0.04 * rng_.bipolar() : 1.0;
      const double pan = std::clamp(setup_.pan + setup_.width * (n > 1 ? 0.8 * spread : 0.0) + (setup_.solo ? 0.0 : setup_.width * 0.25 * ((key % 12) / 5.5 - 1.0)), -1.0, 1.0);
      const double angle = (pan * 0.5 + 0.5) * kPi * 0.5;
      p->gain_l = static_cast<float>(std::cos(angle) * std::sqrt(2.0) / std::sqrt(static_cast<double>(n)));
      p->gain_r = static_cast<float>(std::sin(angle) * std::sqrt(2.0) / std::sqrt(static_cast<double>(n)));
      p->horn.set_acoustics(in_->acoustics);
      set_voicing(*p);
      retarget(*p, key, true);
      p->vibrato_phase = 0.0;
      p->growl_phase = rng_.unipolar();
    }
  }

  // The filters that colour what a player radiates: the body, a mute or the hand, the throat.
  void set_voicing(Player& p) {
    const auto& b = in_->body;
    p.body1.set(Biquad::Type::Peak, b[0].hz, b[0].q, b[0].db, rate_);
    p.body2.set(Biquad::Type::Peak, b[1].hz, b[1].q, b[1].db, rate_);
    p.body1.reset(); p.body2.reset();
    set_mute(p);
    p.hand.reset();
    p.throat1.reset(); p.throat2.reset();
    p.tilt.reset(); p.roll1.reset(); p.roll2.reset(); p.hiss.reset();
  }
  void set_mute(Player& p) const {
    p.mute = setup_.mute;
    const Mute m = in_->family == Family::Brass && in_->takes_mute ? static_cast<Mute>(std::clamp(setup_.mute, 0, kMuteCount - 1)) : Mute::Open;
    switch (m) {
      case Mute::Straight: p.mute1.set(Biquad::Type::HighPass, 350.0, 0.7, 0.0, rate_); p.mute2.set(Biquad::Type::Peak, 1800.0, 1.5, 8.0, rate_); break;
      case Mute::Cup: p.mute1.set(Biquad::Type::LowPass, 1400.0, 0.7, 0.0, rate_); p.mute2.set(Biquad::Type::Peak, 700.0, 1.0, 4.0, rate_); break;
      case Mute::Harmon: p.mute1.set(Biquad::Type::HighPass, 900.0, 0.8, 0.0, rate_); p.mute2.set(Biquad::Type::Peak, 1700.0, 2.5, 12.0, rate_); break;
      case Mute::Bucket: p.mute1.set(Biquad::Type::LowPass, 1000.0, 0.6, 0.0, rate_); p.mute2.set(Biquad::Type::Peak, 400.0, 1.0, 2.0, rate_); break;
      default: p.mute1.set(Biquad::Type::Peak, 1000.0, 1.0, 0.0, rate_); p.mute2.set(Biquad::Type::Peak, 1000.0, 1.0, 0.0, rate_); break;
    }
  }
  static double mute_gain(Mute m) {
    switch (m) {
      case Mute::Straight: return db_to_gain(-7.0);
      case Mute::Cup: return db_to_gain(-6.0);
      case Mute::Harmon: return db_to_gain(-10.0);
      case Mute::Bucket: return db_to_gain(-3.0);
      default: return 1.0;
    }
  }

  // The fingers move to a new note: the bore and the lips are retuned, over the time the
  // mechanism takes (a key, a valve, a slide) or the glide asked for.
  void retarget(Player& p, int key, bool first) {
    const bool changed = p.key != key;
    p.key = key;
    const Fingered before = p.fingered;
    p.fingered = finger(*in_, fold(key + setup_.transpose), setup_.a4, setup_.lipping, hz_of());
    const double hz = p.fingered.hz * p.detune;
    const int partial = p.fingered.partial;
    p.lip_target = hz * lip_ratio(partial) * cents_ratio(setup_.lip_cents);
    p.lip_q = lip_q(partial, hz);
    p.jet_ratio = in_->acoustics.jet_ratio * lerp(1.12, 0.88, std::clamp(setup_.embouchure, 0.0, 1.0));
    p.bore_target = nominal_bore(*in_, rate_, hz, partial) * cal_->correction(setup_.instrument, hz, partial);
    p.vessel_hz = hz * cal_->correction(setup_.instrument, hz);
    if (first) {
      p.bore = p.bore_target; p.lip_hz = p.lip_target; p.bore_step = 0.0;
      if (in_->acoustics.mouth == Mouth::Lips) p.horn.set_valve(p.lip_hz, p.lip_q);
      p.horn.set_vessel(p.vessel_hz);
    } else {
      double mechanism_ms = 6.0;
      if (in_->fingering == Fingering::Valves) mechanism_ms = 10.0;
      if (in_->fingering == Fingering::Slide) mechanism_ms = 45.0;
      if (in_->fingering == Fingering::Chanter) mechanism_ms = 3.0;
      const double ms = std::max(setup_.glide_ms, mechanism_ms);
      p.bore_step = (p.bore_target - p.bore) / std::max(1.0, ms * 0.001 * rate_);
      p.since_change = 0;
      p.horn.set_vessel(p.vessel_hz);
      // The keys or valves clack as they move.
      const bool moved = changed && (in_->fingering == Fingering::Keys || in_->fingering == Fingering::Valves || in_->fingering == Fingering::Chanter);
      if (moved && (in_->fingering != Fingering::Valves || before.valve_mask != p.fingered.valve_mask)) p.click = 1.0f;
    }
    if (first && (in_->fingering == Fingering::Keys || in_->fingering == Fingering::Valves)) p.click = 0.6f;
  }

  void silence(Player& p) {
    p.awake = p.held = false;
    p.key = -1;
    p.pressure = 0.0;
    p.horn.reset();
    p.env = 0.0f; p.click = 0.0f; p.age = 0; p.since_change = 1 << 20;
    p.noise_lp = p.click_bp1 = p.click_bp2 = 0.0f;
  }

  float tick(Player& p, double vib_base, double vib_lfo) {
    (void)vib_lfo;
    if (p.delay > 0) { --p.delay; return 0.0f; }
    ++p.age; ++p.since_change;
    // The fingers' glide.
    if (p.bore_step != 0.0) {
      p.bore += p.bore_step;
      if ((p.bore_step > 0.0 && p.bore >= p.bore_target) || (p.bore_step < 0.0 && p.bore <= p.bore_target)) { p.bore = p.bore_target; p.bore_step = 0.0; }
    }
    if (p.lip_hz != p.lip_target) {
      // The lips move to the next partial within a few milliseconds.
      p.lip_hz += (p.lip_target - p.lip_hz) * (1.0 - std::exp(-1.0 / (0.004 * rate_)));
      if (std::abs(p.lip_hz - p.lip_target) < 1e-3) p.lip_hz = p.lip_target;
    }
    // The breath: the tongue lets the pressure behind it go (an overshoot), the lungs hold it,
    // and on release it falls away.
    const double level = breath_level(p) * p.breath_scale;
    double target = p.held ? pressure_for(level) : 0.0;
    const bool chanter = in_->fingering == Fingering::Chanter;
    const double tongue = chanter ? 0.0 : std::clamp(setup_.tongue, 0.0, 1.0);
    const double age_s = p.age / rate_;
    // (A jet overshoots less: a flautist's tongue releases air, not a reed or the lips.)
    if (p.held) target *= 1.0 + (in_->family == Family::Flute ? 0.12 : 0.35) * tongue * std::exp(-age_s / 0.03);
    // A reed pressed past its closing pressure chokes: the player never blows that hard.
    if (in_->acoustics.mouth == Mouth::Reed) target = std::min(target, loud_of(*in_) * 1.06 * setup_.breath);
    const double tau_ms = p.held ? std::max(1.0, setup_.attack_ms) / 3.0 : std::max(1.0, setup_.release_ms) / 3.0;
    p.pressure += (target - p.pressure) * (1.0 - std::exp(-1.0 / (tau_ms * 0.001 * rate_)));
    // Vibrato: after its delay, fading in over a quarter second; the jaw or lips bend the pitch,
    // a flautist's breath mostly the loudness.
    const double onset = std::clamp((age_s * 1000.0 - setup_.vibrato_delay_ms) / 250.0, 0.0, 1.0);
    p.vibrato_phase += setup_.vibrato_hz / rate_;
    if (p.vibrato_phase >= 1.0) p.vibrato_phase -= 1.0;
    const double vib = std::sin(kTau * p.vibrato_phase) * onset * vib_base;
    const bool flute = in_->family == Family::Flute;
    const double pitch_cents = flute ? 0.3 * vib : vib;
    double pressure = p.pressure * (flute ? 1.0 + 0.012 * vib : 1.0);
    // Growl or flutter-tongue: the throat or tongue rattling at 25 to 35 Hz.
    if (setup_.growl > 0.0) {
      p.growl_phase += (27.0 + 6.0 * rng_.unipolar()) / rate_;
      if (p.growl_phase >= 1.0) p.growl_phase -= 1.0;
      const double g = std::clamp(setup_.growl, 0.0, 1.0);
      pressure *= 1.0 + 0.45 * g * std::sin(kTau * p.growl_phase) + 0.15 * g * rng_.bipolar();
    }
    // The tuning this sample: bend, vibrato.
    const double bend = std::exp2(bend_ * setup_.bend_range / 12.0);
    const double pitch = cents_ratio(pitch_cents) * bend;
    // A bent note is tuned as a fingered one at that pitch would be: the calibration's
    // correction (the loop filters' delay, the reed's pull) differs a few cents a semitone.
    {
      const double hz = p.fingered.hz * p.detune;
      if (bend != p.bend_seen || hz != p.bend_hz_seen) {
        p.bend_seen = bend; p.bend_hz_seen = hz;
        p.bend_factor = bend == 1.0 ? 1.0 : cal_->correction(setup_.instrument, hz * bend, p.fingered.partial) / cal_->correction(setup_.instrument, hz, p.fingered.partial);
      }
    }
    horn::Tuning t;
    t.bore = p.bore / pitch * p.bend_factor;
    t.jet = p.jet_ratio * t.bore * (in_->acoustics.bore == Bore::Stopped ? 2.0 : (in_->acoustics.bore == Bore::Brass ? 1.0 / std::max(1, p.fingered.partial) : 1.0));
    if (in_->acoustics.mouth == Mouth::Vessel) t.jet = p.jet_ratio * rate_ / (p.fingered.hz * p.detune * pitch);
    // A flautist blowing harder brings the lips closer to the edge, so the jet's travel (which
    // the harder jet would otherwise shorten, sharpening the note toward its octave) stays a
    // constant share of the period across the dynamic: as Fletcher measured, players keep the
    // jet's travel time near the period's whatever the pressure.
    if (in_->acoustics.mouth == Mouth::Jet) {
      const double mid = 0.5 * (soft_of(*in_) + loud_of(*in_));
      t.jet *= std::pow(std::max(pressure, 0.02) / mid, 0.25);
    }
    if (in_->acoustics.mouth == Mouth::Lips && (p.age & 15) == 0) p.horn.set_valve(p.lip_hz * pitch, p.lip_q);
    if (in_->acoustics.mouth == Mouth::Vessel && pitch != 1.0 && (p.age & 15) == 0) p.horn.set_vessel(p.vessel_hz * pitch * p.bend_factor);
    if ((p.age & 15) == 1) apply_embouchure(p);
    // The breath's turbulence, more at the tongue's release.
    const double tongue_burst = tongue * std::exp(-age_s / 0.012);
    const float white = rng_.bipolar();
    p.noise_lp += 0.6f * (white - p.noise_lp);
    const float noise = static_cast<float>((in_->noise * setup_.breath_noise * (1.0 + 2.0 * setup_.growl) + 0.15 * tongue_burst) * std::max(pressure, 0.0)) * p.noise_lp;
    float out = p.horn.tick(t, static_cast<float>(std::max(pressure, 0.0)), noise);
    {
      // The dynamic's level and treble beyond the loop's own (see Dynamics).
      const Dynamics dyn = dynamics_of(setup_.instrument);
      const double k = g_dyn_k >= 0.0 ? g_dyn_k : dyn.level_exponent, tilt = g_dyn_tilt >= 0.0 ? g_dyn_tilt : dyn.tilt_db;
      const double treble = g_dyn_treble > -99.0 ? g_dyn_treble : dyn.treble_db;
      if (k > 0.0) out *= static_cast<float>(std::pow(std::max(pressure, 1e-4) / loud_pressure(), k));
      if (tilt != 0.0 || treble != 0.0) {
        if ((p.age & 63) == 1) p.tilt.set(Biquad::Type::HighShelf, std::min(1.5 * p.fingered.hz, 0.4 * rate_), 0.7, treble - tilt * (1.0 - dynamic(p)), rate_);
        out = p.tilt.process(out);
      }
      const double lp_pp = g_dyn_lp_pp >= 0.0 ? g_dyn_lp_pp : dyn.lp_pp, lp_ff = g_dyn_lp_ff >= 0.0 ? g_dyn_lp_ff : dyn.lp_ff;
      if (lp_pp > 0.0) {
        // Fitted to the steady tone only: the tongue's burst opens it while it lasts.
        if ((p.age & 15) == 1) {
          const double harmonic = lp_pp * std::pow(lp_ff / lp_pp, dynamic(p)) * (1.0 + 20.0 * tongue_burst);
          const double hz = std::min(harmonic * p.fingered.hz, 0.45 * rate_);
          p.roll1.set(Biquad::Type::LowPass, hz, 0.707, 0.0, rate_);
          p.roll2.set(Biquad::Type::LowPass, hz, 0.707, 0.0, rate_);
        }
        out = p.roll1.process(out);
        if ((g_dyn_lp_order > 0 ? g_dyn_lp_order : dyn.lp_poles) >= 4) out = p.roll2.process(out);
      }
      // The breath's turbulence as heard: a jet's radiated straight from the embouchure, a reed's
      // or the lips' through the bore. It is added here rather than in the loop, where noise
      // enough to match the recordings jitters the pitch. Turbulence's power grows steeply with
      // the flow's speed, so its amplitude goes as the pressure squared. It is fitted up to the
      // tenth harmonic, where the recordings measure it, and kept below there.
      const double hiss = g_hiss >= 0.0 ? g_hiss : dyn.hiss;
      if (hiss > 0.0) {
        if ((p.age & 63) == 1) p.hiss.set(Biquad::Type::LowPass, std::min(10.0 * p.fingered.hz, 0.45 * rate_), 0.707, 0.0, rate_);
        const double rel = std::max(pressure, 0.0) / loud_pressure();
        out += p.hiss.process(static_cast<float>(hiss * setup_.breath_noise * rel * rel) * p.noise_lp);
      }
    }
    // The body and bell, the mute or the hand, the throat.
    out = p.body2.process(p.body1.process(out));
    if (in_->family == Family::Brass && in_->takes_mute) {
      // A mute put in or taken out mid-note changes the sound at once.
      if (p.mute != setup_.mute) set_mute(p);
      out = p.mute2.process(p.mute1.process(out)) * static_cast<float>(mute_gain(static_cast<Mute>(std::clamp(setup_.mute, 0, kMuteCount - 1))));
      if (setup_.hand > 0.0) {
        if ((p.age & 63) == 1) p.hand.set(Biquad::Type::LowPass, 8000.0 * std::pow(700.0 / 8000.0, std::clamp(setup_.hand, 0.0, 1.0)), 0.9, 0.0, rate_);
        out = p.hand.process(out);
      }
    }
    if (setup_.instrument == kDidgeridoo) {
      if ((p.age & 63) == 1) set_throat(p);
      out = p.throat2.process(p.throat1.process(out));
    }
    // The key pads' or valves' click.
    if (p.click > 1e-4f && in_->click_hz > 0.0) {
      const float c = rng_.bipolar() * p.click * static_cast<float>(setup_.key_noise * 0.05);
      const float w = static_cast<float>(kTau * in_->click_hz / rate_);
      p.click_bp1 += w * (c - p.click_bp1);
      p.click_bp2 += w * (p.click_bp1 - p.click_bp2);
      out += (p.click_bp1 - p.click_bp2) * 6.0f;
      p.click *= static_cast<float>(std::exp(-1.0 / (0.004 * rate_)));
    }
    // A breath's outward rush is audible even before the reed or lips speak.
    p.env = std::max(std::abs(out), p.env * 0.9995f);
    if (!p.held && p.pressure < 1e-4 && p.env < 1e-5f) silence(p);
    return out;
  }

  // The embouchure for this note: a reed's stiffness, a jet's aim, a valve's opening.
  // Where the note sits between the instrument's pianissimo and fortissimo, 0..1.
  double dynamic(const Player& p) const {
    const double soft = soft_pressure(), loud = loud_pressure();
    return std::clamp((p.pressure - soft) / std::max(loud - soft, 1e-6), 0.0, 1.0);
  }
  void apply_embouchure(Player& p) {
    const double e = std::clamp(setup_.embouchure, 0.0, 1.0);
    const double d = dynamic(p);
    const auto& a = in_->acoustics;
    switch (a.mouth) {
      case Mouth::Reed:
        // (On a cylinder the player's lip also closes the reed down for a pianissimo, to a third
        // of its opening, and lets it open for a fortissimo: less flow, a quieter, darker note.)
        // A softer reed closes more readily under the same pressure; a harder one stands open.
        if (a.bore == Bore::Cone) {
          p.horn.set_reed_slope(a.reed_slope * std::exp2(0.8 * (0.5 - std::clamp(setup_.reed, 0.0, 1.0))));
        } else p.horn.set_zeta((a.bore == Bore::Modal && horn::g_modal_zeta > 0.0 ? horn::g_modal_zeta : a.zeta) * std::exp2(g_reed_range * (0.5 - std::clamp(setup_.reed, 0.0, 1.0))) * lerp(0.5, 1.0, d));
        break;
      // Loose lips open wider and pass more air; a firm embouchure narrows the aperture.
      case Mouth::Lips: p.horn.set_zeta(a.zeta * std::exp2(1.0 * (0.5 - e))); break;
      default: p.horn.set_jet_offset(a.jet_offset * lerp(0.4, 1.6, e)); break;
    }
  }

  // The didgeridoo player's throat: two formants moved from "oo" through "ah" to "ee".
  void set_throat(Player& p) const {
    const double t = std::clamp(setup_.throat, 0.0, 1.0);
    const double f1 = t < 0.5 ? lerp(300.0, 750.0, t * 2.0) : lerp(750.0, 280.0, (t - 0.5) * 2.0);
    const double f2 = t < 0.5 ? lerp(800.0, 1200.0, t * 2.0) : lerp(1200.0, 2300.0, (t - 0.5) * 2.0);
    p.throat1.set(Biquad::Type::Peak, f1, 4.0, 12.0, rate_);
    p.throat2.set(Biquad::Type::Peak, f2, 4.0, 12.0, rate_);
  }

  // The bagpipe's three drones (a bass an octave and a half below the chanter's A, two tenors
  // an octave below), cylindrical reeds that sound while the bag is squeezed.
  struct Drone { horn::Horn horn; bool awake = false; double pressure = 0.0; float env = 0.0f; double bore = 100.0; float gain = 0.3f; Biquad body; };
  void silence(Drone& d) { d.awake = false; d.pressure = 0.0; d.env = 0.0f; d.horn.reset(); }
  void render_drones(float& left, float& right) {
    const bool squeezed = setup_.drones && std::any_of(players_.begin(), players_.end(), [](const Player& p) { return p.awake && p.held; });
    static constexpr int kDroneKeys[3] = {45, 57, 57};
    for (int i = 0; i < 3; ++i) {
      Drone& d = drones_[static_cast<size_t>(i)];
      if (!d.awake && !squeezed) continue;
      if (!d.awake) {
        d.awake = true;
        d.horn.set_acoustics(instrument(kClarinetForDrones).acoustics);
        const double hz = temperament::hz(setup_.temperament, setup_.root, setup_.a4, kDroneKeys[i]) * cents_ratio(setup_.fine_cents + (i == 2 ? 1.5 : 0.0));
        d.bore = nominal_bore(instrument(kClarinetForDrones), rate_, hz, 1) * cal_->correction(kClarinetForDrones, hz);
        d.body.set(Biquad::Type::Peak, 1200.0, 1.5, 4.0, rate_);
      }
      const double target = squeezed ? 0.62 : 0.0;
      d.pressure += (target - d.pressure) * (1.0 - std::exp(-1.0 / (0.05 * rate_)));
      horn::Tuning t; t.bore = d.bore;
      const float out = d.body.process(d.horn.tick(t, static_cast<float>(d.pressure), 0.0f)) * (i == 0 ? 0.35f : 0.25f);
      left += out * (i == 1 ? 0.9f : 0.6f);
      right += out * (i == 2 ? 0.9f : 0.6f);
      d.env = std::max(std::abs(out), d.env * 0.9995f);
      if (!squeezed && d.pressure < 1e-4 && d.env < 1e-5f) silence(d);
    }
  }

  static constexpr int kDidgeridoo = 26;
  static constexpr int kClarinetForDrones = 1;  // the bass clarinet's calibration covers the drones' pitches

  double rate_ = 48000.0;
  const Calibration* cal_ = nullptr;
  const Instrument* in_ = &instruments()[0];
  Setup setup_{};
  bool configured_ = false;
  std::array<Player, kMaxPlayers> players_{};
  std::array<Drone, 3> drones_{};
  std::vector<int> stack_ = std::vector<int>(0);
  std::array<bool, 128> held_{};
  uint64_t counter_ = 0;
  double controller_ = -1.0, bend_ = 0.0, wheel_vibrato_ = 0.0;
  Lfo vibrato_;
  Rng rng_{0x2545f491u};
};

}  // namespace honkr::dsp::winds
