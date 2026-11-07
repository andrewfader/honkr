// The features a single sustained note is judged on against a recording of the real instrument
// (see references/README.md and docs/QUALITY.md): its harmonic profile, brightness, odd/even
// balance, noise and attack, all in units that do not depend on the recording's level, sample
// rate or microphone gain, so a model's note and a recorded one can be compared directly. Level
// enters only as differences between dynamics, which the recordings keep (their input gain was
// never changed between pp, mf and ff).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "audio_measure.hpp"

namespace timbre {

constexpr int kHarmonics = 10;

struct Features {
  double f0 = 0.0;                         // Hz
  std::array<double, kHarmonics> profile{}; // dB of each harmonic against the strongest, floored at -60
  double centroid = 0.0;                   // the harmonics' centroid, in harmonic numbers
  double odd_even_db = 0.0;                // odd harmonics' energy over the even ones' (2..10), dB
  double noise_db = 0.0;                   // the noise between the first ten harmonics against them, dB (see noise_db)
  double attack_ms = 0.0;                  // 10% to 90% of the steady level
  double level_db = 0.0;                   // the steady part's RMS, dBFS (only differences mean anything)
};

inline double cents(double a, double b) { return 1200.0 * std::log2(a / b); }

// The noise against the harmonics, dB, over the band of the first ten: the spectrum's mean
// power between the harmonics (more than 0.35 of their spacing from any), taken as the noise's
// level across the band, against the rest. Harmonics above the tenth do not count as noise, and
// a player's slow pitch drift stays in its harmonics' lobes (the frame resolves a twelfth of the
// spacing); the noise floor of a recording's room still counts.
inline double noise_db(const measure::Span& steady, double rate, double f0) {
  int order = 13;
  while (order < 15 && rate / static_cast<double>(size_t{1} << order) > f0 / 12.0) ++order;
  const auto power = measure::power_spectrum(steady, order);
  const double df = rate / static_cast<double>(2 * power.size());
  double between = 0.0, band = 0.0;
  int between_bins = 0, band_bins = 0;
  for (size_t k = 1; k < power.size(); ++k) {
    const double hz = static_cast<double>(k) * df, r = hz / f0;
    if (r < 0.5 || r > 10.5 || hz > 0.45 * rate) continue;
    band += power[k];
    ++band_bins;
    if (std::abs(r - std::round(r)) >= 0.35) { between += power[k]; ++between_bins; }
  }
  if (between_bins == 0 || band <= 0.0) return -100.0;
  const double noise = between / between_bins * band_bins;
  return 10.0 * std::log10(std::max(noise, 1e-30) / std::max(band - noise, band * 1e-6));
}

// Features of a note: `onset` is where it starts, `length` how long it lasts (samples).
inline Features analyse(const std::vector<float>& x, double rate, size_t onset, size_t length, double expected_hz) {
  Features f;
  const size_t steady_from = onset + length / 4, steady_len = std::min<size_t>(length / 2, static_cast<size_t>(rate * 0.8));
  const measure::Span steady(x, steady_from, steady_len);
  f.f0 = measure::pitch_hz(steady, rate, expected_hz * 0.8, expected_hz * 1.25);
  if (f.f0 <= 0.0) f.f0 = expected_hz;
  const auto amps = measure::harmonics(steady, rate, f.f0, kHarmonics);
  double strongest = 1e-12, harmonic_energy = 0.0, weighted = 0.0, odd = 1e-12, even = 1e-12;
  for (int n = 0; n < kHarmonics; ++n) {
    const double a = (n + 1) * f.f0 < rate * 0.45 ? amps[static_cast<size_t>(n)] : 0.0;
    strongest = std::max(strongest, a);
    harmonic_energy += a * a;
    weighted += (n + 1) * a * a;
    if (n > 0) ((n + 1) % 2 ? odd : even) += a * a;
  }
  for (int n = 0; n < kHarmonics; ++n)
    f.profile[static_cast<size_t>(n)] = std::max(-60.0, measure::db(((n + 1) * f.f0 < rate * 0.45 ? amps[static_cast<size_t>(n)] : 0.0) / strongest));
  f.centroid = weighted / std::max(harmonic_energy, 1e-24);
  f.odd_even_db = 10.0 * std::log10(odd / even);
  f.noise_db = noise_db(steady, rate, f.f0);
  f.level_db = measure::db(measure::rms(steady));
  // The attack: the 5 ms envelope from the onset until it first reaches 10% and 90% of the
  // steady level.
  const double target = measure::rms(steady);
  const size_t hop = static_cast<size_t>(rate * 0.002), win = static_cast<size_t>(rate * 0.005);
  double t10 = -1.0, t90 = -1.0;
  for (size_t p = onset; p + win < steady_from + steady_len; p += hop) {
    const double e = measure::rms(measure::Span(x, p, win));
    if (t10 < 0.0 && e >= 0.1 * target) t10 = static_cast<double>(p - onset) / rate;
    if (t90 < 0.0 && e >= 0.9 * target) { t90 = static_cast<double>(p - onset) / rate; break; }
  }
  f.attack_ms = t10 >= 0.0 && t90 >= 0.0 ? 1000.0 * (t90 - t10) : 0.0;
  return f;
}

// The notes of a recorded chromatic scale: stretches louder than 35 dB under the file's peak,
// at least 0.25 s long, separated by silence.
struct Segment { size_t onset, length; };
inline std::vector<Segment> segment(const std::vector<float>& x, double rate) {
  const size_t hop = static_cast<size_t>(rate * 0.01);
  std::vector<double> env;
  for (size_t p = 0; p + hop <= x.size(); p += hop) env.push_back(measure::rms(measure::Span(x, p, hop)));
  double peak = 1e-12;
  for (double e : env) peak = std::max(peak, e);
  const double threshold = peak * std::pow(10.0, -35.0 / 20.0);
  std::vector<Segment> out;
  size_t i = 0;
  while (i < env.size()) {
    while (i < env.size() && env[i] < threshold) ++i;
    const size_t start = i;
    size_t quiet = 0, end = i;
    while (i < env.size() && quiet < 8) { if (env[i] < threshold) ++quiet; else { quiet = 0; end = i; } ++i; }
    if (end > start && (end - start) * hop >= static_cast<size_t>(rate * 0.25)) out.push_back({start * hop, (end - start + 1) * hop});
  }
  return out;
}

// One line of references/features.tsv.
inline std::string to_line(const std::string& instrument, const std::string& dynamic, int key, const Features& f) {
  char buf[512];
  int n = std::snprintf(buf, sizeof(buf), "%s\t%s\t%d\t%.2f\t%.3f\t%.2f\t%.2f\t%.1f\t%.2f", instrument.c_str(), dynamic.c_str(), key, f.f0, f.centroid, f.odd_even_db, f.noise_db, f.attack_ms, f.level_db);
  for (double p : f.profile) n += std::snprintf(buf + n, sizeof(buf) - static_cast<size_t>(n), "\t%.1f", p);
  return buf;
}

}  // namespace timbre
