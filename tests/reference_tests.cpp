// Executable scenario for features/references.feature: HONKR against recordings of the real
// instruments (references/README.md). Every reference note is played through the engine and its
// features compared with the recording's; no instrument may fall behind references/baseline.tsv.
// Runs from the repository root (CTest sets the working directory); needs no network: the
// recordings' features are committed, the recordings are not.
#include "reference_match.hpp"
#include "spec.hpp"

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
