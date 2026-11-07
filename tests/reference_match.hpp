// HONKR against recordings of the real instruments. references/features.tsv holds the features
// (tests/timbre.hpp) of every note of the reference recordings, extracted once by
// `honkr-reference extract`; this plays the same instrument, note and dynamic through the
// engine, dry, extracts the same features and scores the difference per instrument.
// references/baseline.tsv holds the scores HONKR last reached; reference_tests fails if any
// instrument falls behind it, so the match can only get better (see docs/QUALITY.md).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../src/engine.hpp"
#include "timbre.hpp"

namespace reference {

struct Note { std::string instrument, dynamic; int key; timbre::Features f; };

inline std::vector<Note> load_features(const std::string& path) {
  std::vector<Note> out;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::stringstream s(line);
    Note n;
    std::string field;
    std::getline(s, n.instrument, '\t');
    std::getline(s, n.dynamic, '\t');
    std::getline(s, field, '\t'); n.key = std::stoi(field);
    double* numbers[] = {&n.f.f0, &n.f.centroid, &n.f.odd_even_db, &n.f.noise_db, &n.f.attack_ms, &n.f.level_db};
    for (double* d : numbers) { std::getline(s, field, '\t'); *d = std::stod(field); }
    for (double& p : n.f.profile) { std::getline(s, field, '\t'); p = std::stod(field); }
    out.push_back(n);
  }
  return out;
}

// The breath a player gives each marked dynamic, as a key velocity.
inline double velocity(const std::string& dynamic) { return dynamic == "pp" ? 0.2 : dynamic == "mf" ? 0.55 : 0.95; }

inline int instrument_index(const std::string& name) {
  for (int i = 0; i < honkr::params::kInstrumentCount; ++i) if (name == honkr::params::kInstrumentNames[i]) return i;
  return -1;
}

// One note through the engine: the instrument's defaults without vibrato (the references are
// played without it), no room, the microphone on the bell, 1.6 s held. The master sits 12 dB
// down so a fortissimo never reaches the output limiter: only levels relative to one another
// are compared.
inline timbre::Features play(honkr::Engine& engine, int instrument, int key, double vel, double rate) {
  using namespace honkr;
  Settings s = Settings::defaults();
  s.v[params::kInstrument] = instrument;
  s.v[params::kVibrato] = 0.0;
  s.v[params::kMasterDb] = -12.0;
  engine.reset();
  const size_t length = static_cast<size_t>(1.6 * rate), tail = static_cast<size_t>(0.3 * rate);
  std::vector<float> l(length + tail), r(length + tail);
  engine.note_on(s, 0, key, -1, vel);
  for (size_t at = 0; at < l.size(); at += 256) {
    const uint32_t n = static_cast<uint32_t>(std::min<size_t>(256, l.size() - at));
    if (at <= length && length < at + n) engine.note_off(s, 0, key, -1);
    engine.render(s, l.data() + at, r.data() + at, n);
  }
  std::vector<float> m(l.size());
  for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (l[i] + r[i]);
  return timbre::analyse(m, rate, 0, length, 440.0 * std::exp2((key - 69) / 12.0));
}

// How far HONKR is from the recordings for one instrument, every score an error (0 is a match).
struct Score {
  int notes = 0;
  double profile_db = 0.0;     // RMS difference of the first eight harmonics' profile, dB, averaged over notes
  double brightness = 0.0;     // |log2| of the harmonics' centroid against the recording's, averaged
  double odd_even_db = 0.0;    // |difference| of the odd/even balance, dB
  double noise_db = 0.0;       // |difference| of the noise against the harmonics, dB (floored at -55)
  double dynamic_db = 0.0;     // |difference| of the level gained from the softest to the loudest dynamic, dB
  double dynamic_bright = 0.0; // |log2| of the brightening from the softest to the loudest dynamic, against the recording's
};
constexpr const char* kScoreNames[] = {"profile_db", "brightness", "odd_even_db", "noise_db", "dynamic_db", "dynamic_bright"};
inline std::array<double, 6> values(const Score& s) { return {s.profile_db, s.brightness, s.odd_even_db, s.noise_db, s.dynamic_db, s.dynamic_bright}; }

struct Played { Note reference; timbre::Features model; };

inline std::map<std::string, Score> score(const std::vector<Note>& refs, double rate, std::vector<Played>* detail = nullptr) {
  honkr::Engine engine;
  engine.prepare(rate);
  std::vector<Played> played;
  for (const Note& n : refs) {
    const int inst = instrument_index(n.instrument);
    if (inst < 0) continue;
    const auto& in = honkr::dsp::winds::instrument(inst);
    if (n.key < in.low || n.key > in.high) continue;  // outside HONKR's compass: nothing to compare
    played.push_back({n, play(engine, inst, n.key, velocity(n.dynamic), rate)});
  }
  std::map<std::string, Score> out;
  for (const Played& p : played) {
    Score& s = out[p.reference.instrument];
    double sq = 0.0;
    for (int h = 0; h < 8; ++h) { const double d = p.model.profile[static_cast<size_t>(h)] - p.reference.f.profile[static_cast<size_t>(h)]; sq += d * d; }
    s.profile_db += std::sqrt(sq / 8.0);
    s.brightness += std::abs(std::log2(p.model.centroid / p.reference.f.centroid));
    s.odd_even_db += std::abs(p.model.odd_even_db - p.reference.f.odd_even_db);
    s.noise_db += std::abs(std::max(p.model.noise_db, -55.0) - std::max(p.reference.f.noise_db, -55.0));
    ++s.notes;
  }
  // The dynamics: for each key recorded at its softest and loudest marking, the level gained and
  // the brightening, HONKR's against the recording's.
  for (auto& [name, s] : out) {
    int keys = 0;
    for (const Played& soft : played) {
      if (soft.reference.instrument != name || soft.reference.dynamic != "pp") continue;
      const Played* loud = nullptr;
      for (const Played& q : played)
        if (q.reference.instrument == name && q.reference.key == soft.reference.key && (q.reference.dynamic == "ff" || (!loud && q.reference.dynamic == "mf"))) loud = &q;
      if (!loud) continue;
      const double ref_gain = loud->reference.f.level_db - soft.reference.f.level_db, model_gain = loud->model.level_db - soft.model.level_db;
      const double ref_bright = loud->reference.f.centroid / soft.reference.f.centroid, model_bright = loud->model.centroid / soft.model.centroid;
      s.dynamic_db += std::abs(model_gain - ref_gain);
      s.dynamic_bright += std::abs(std::log2(model_bright / ref_bright));
      ++keys;
    }
    if (keys) { s.dynamic_db /= keys; s.dynamic_bright /= keys; }
    if (s.notes) { s.profile_db /= s.notes; s.brightness /= s.notes; s.odd_even_db /= s.notes; s.noise_db /= s.notes; }
  }
  if (detail) *detail = played;
  return out;
}

inline std::map<std::string, Score> load_baseline(const std::string& path) {
  std::map<std::string, Score> out;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::stringstream s(line);
    std::string name, field;
    std::getline(s, name, '\t');
    Score sc;
    std::getline(s, field, '\t'); sc.notes = std::stoi(field);
    double* numbers[] = {&sc.profile_db, &sc.brightness, &sc.odd_even_db, &sc.noise_db, &sc.dynamic_db, &sc.dynamic_bright};
    for (double* d : numbers) { std::getline(s, field, '\t'); *d = std::stod(field); }
    out[name] = sc;
  }
  return out;
}

inline void write_baseline(const std::string& path, const std::map<std::string, Score>& scores) {
  std::ofstream out(path);
  out << "# HONKR's distance from the reference recordings, per instrument (tests/reference_match.hpp).\n"
         "# Written by `honkr-reference match --update-baseline`; reference_tests fails if any score gets worse.\n"
         "# instrument\tnotes\tprofile_db\tbrightness\todd_even_db\tnoise_db\tdynamic_db\tdynamic_bright\n";
  for (const auto& [name, s] : scores) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s\t%d\t%.3f\t%.4f\t%.3f\t%.3f\t%.3f\t%.4f\n", name.c_str(), s.notes, s.profile_db, s.brightness, s.odd_even_db, s.noise_db, s.dynamic_db, s.dynamic_bright);
    out << buf;
  }
}

// How much worse than its baseline a score may get before the test fails: rounding and
// platform noise, not a real change.
constexpr std::array<double, 6> kSlack{0.3, 0.02, 0.3, 0.5, 0.3, 0.02};

}  // namespace reference
