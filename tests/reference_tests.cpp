// Executable scenario for features/references.feature: HONKR against recordings of the real
// instruments (references/README.md). Every reference note is played through the engine and its
// features compared with the recording's; no instrument may fall behind references/baseline.tsv.
// Runs from the repository root (CTest sets the working directory); needs no network: the
// recordings' features are committed, the recordings are not.
#include <fstream>
#include <set>

#include "reference_match.hpp"
#include "spec.hpp"

SCENARIO(a_quiet_recording_is_cut_into_every_note_it_holds) {
  GIVEN("twelve 1.4 s notes 0.6 s apart at -60 dBFS over a hiss floor at -75 dBFS, 15 dB under the notes: the University of Iowa's pianissimo tuba");
  constexpr double rate = 44100.0;
  std::vector<float> x(static_cast<size_t>(25.0 * rate));
  honkr::dsp::Rng rng(7);
  for (size_t i = 0; i < x.size(); ++i) {
    const double t = static_cast<double>(i) / rate, in_note = std::fmod(t - 0.5, 2.0);
    const bool sounding = t >= 0.5 && t < 24.5 && in_note < 1.4;
    x[i] = static_cast<float>((sounding ? 0.001 * std::sqrt(2.0) * std::sin(honkr::dsp::kTau * 65.4 * t) : 0.0) + 0.000178 * std::sqrt(3.0) * rng.bipolar());
  }
  const auto notes = timbre::segment(x, rate);
  THEN("the segmenter finds the twelve notes, each about 1.4 s long");
  CHECK(notes.size() == 12, spec::fmt("%.0f notes", static_cast<double>(notes.size())));
  for (const auto& n : notes) CHECK_NEAR(static_cast<double>(n.length) / rate, 1.4, 0.1, "note length, s");
}

SCENARIO(every_recording_in_the_manifest_gives_notes) {
  GIVEN("the recordings references/manifest.tsv lists, by instrument and dynamic, and the features extracted from them");
  std::set<std::string> listed, measured;
  std::ifstream manifest("references/manifest.tsv");
  std::string line;
  while (std::getline(manifest, line)) {
    if (line.empty() || line[0] == '#') continue;
    const size_t a = line.find('\t'), b = line.find('\t', a + 1);
    listed.insert(line.substr(0, a) + " " + line.substr(a + 1, b - a - 1));
  }
  for (const auto& n : reference::load_features("references/features.tsv")) measured.insert(n.instrument + " " + n.dynamic);
  THEN("every instrument at every listed dynamic has notes among the features");
  CHECK(listed.size() == 35, spec::fmt("%.0f recordings listed", static_cast<double>(listed.size())));
  for (const auto& r : listed) CHECK(measured.count(r) == 1, r + " has notes");
}

SCENARIO(no_instrument_falls_behind_its_match_to_the_recordings) {
  GIVEN("the features of every note of the University of Iowa recordings, and HONKR playing the same notes at the same dynamics");
  const auto refs = reference::load_features("references/features.tsv");
  const auto baseline = reference::load_baseline("references/baseline.tsv");
  CHECK(refs.size() > 100, spec::fmt("%.0f reference notes", static_cast<double>(refs.size())));
  const auto scores = reference::score(refs, 48000.0);
  THEN("every instrument with a baseline scores no worse on any measure: harmonic profile, brightness, odd/even balance, noise, and the dynamics' level and brightening");
  for (const auto& [name, then] : baseline) {
    const auto it = scores.find(name);
    CHECK(it != scores.end(), name + " still has reference notes it can play");
    if (it == scores.end()) continue;
    const auto now = reference::values(it->second), was = reference::values(then);
    for (size_t i = 0; i < now.size(); ++i)
      CHECK(now[i] <= was[i] + reference::kSlack[i], name + " " + reference::kScoreNames[i] + spec::fmt(": %.3f, baseline %.3f", now[i], was[i]));
  }
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
