// Temperaments: how the twelve notes of the octave are laid out against equal temperament, in
// cents, from a chain of fifths or a published table. Historical keyboards were tuned to these,
// and their music was written for them: a quarter-comma meantone major third is pure (386 cents)
// where equal temperament's is 14 cents sharp, and its wolf fifth (G# to Eb) howls.
//
// Every temperament is given relative to its key (the note the chain starts from) and
// normalised so that A, the note a tuner takes from the fork, sits at the reference pitch.
//
// Sources: the fifth chains are arithmetic; Werckmeister III (1691), Kirnberger III (1779) and
// Vallotti (1754) are the tables as given in Barbour, "Tuning and Temperament" (1951), rounded
// to a tenth of a cent.
#pragma once

#include <array>
#include <cmath>

#include "core.hpp"

namespace honkr::dsp::temperament {

enum Kind : int { kEqual, kPythagorean, kMeantone, kWerckmeister, kKirnberger, kVallotti, kCount };

// Deviation of each pitch class (C = 0 .. B = 11) from equal temperament, in cents, with the
// chain of fifths running from Eb (three fifths below the key) up to G#, so the wolf lies
// between G# and Eb as in common practice.
inline std::array<double, 12> deviations(int kind) {
  std::array<double, 12> d{};
  const auto chain = [&](double fifth_cents) {
    // Position of each pitch class in the chain of fifths from C: C 0, G 1, D 2, A 3, E 4, B 5,
    // F# 6, C# 7, G# 8; F -1, Bb -2, Eb -3.
    static constexpr int kSteps[12] = {0, 7, 2, -3, 4, -1, 6, 1, 8, 3, -2, 5};
    for (int pc = 0; pc < 12; ++pc) d[static_cast<size_t>(pc)] = kSteps[pc] * (fifth_cents - 700.0);
  };
  switch (kind) {
    case kPythagorean: chain(1200.0 * std::log2(1.5)); break;           // pure fifths, 701.955 cents
    case kMeantone: chain(1200.0 * std::log2(std::pow(5.0, 0.25))); break;  // 696.578: four of them make a pure third
    case kWerckmeister:
      d = {0.0, -9.8, -7.8, -5.9, -9.8, 2.0, -7.8, -3.9, -7.8, -11.7, -3.9, -7.8};
      break;
    case kKirnberger:
      d = {0.0, -6.8, -6.8, -2.0, -13.7, 2.0, -8.8, -3.4, -4.9, -10.3, 0.0, -11.7};
      break;
    case kVallotti:
      d = {5.9, 0.0, 2.0, 3.9, -2.0, 7.8, -2.0, 3.9, 2.0, 0.0, 5.9, -3.9};
      break;
    default: break;
  }
  return d;
}

// The cents a MIDI note sits from equal temperament, with the temperament laid out from `root`
// (0 = C) and A at the reference: the deviation of the note's pitch class in the transposed
// table, less the deviation A itself would have had.
inline double offset_cents(int kind, int root, int note) {
  if (kind <= kEqual || kind >= kCount) return 0.0;
  const std::array<double, 12> d = deviations(kind);
  const auto at = [&](int pc) { return d[static_cast<size_t>(((pc - root) % 12 + 12) % 12)]; };
  return at(((note % 12) + 12) % 12) - at(9);
}

// The frequency of a note before any instrument's own stretch: the reference pitch for A4
// and the temperament's offset.
inline double hz(int kind, int root, double a4, int note) {
  return a4 * std::exp2((note - 69) / 12.0) * cents_ratio(offset_cents(kind, root, note));
}

}  // namespace honkr::dsp::temperament
