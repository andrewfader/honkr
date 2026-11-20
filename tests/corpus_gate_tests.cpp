// Executable scenarios for features/corpus_gate.feature: the presets fitted to the song corpus,
// each played as the records use it and measured as the corpus measured the records' separated
// stems (tests/corpus_gate.hpp), held inside the interquartile ranges of the excerpts it answers
// to (tests/corpus_reference.hpp, generated from the corpus by tools/corpus_reference.py).
#include "corpus_gate.hpp"
#include "spec.hpp"

using namespace honkr;

SCENARIO(the_corpus_measures_match_the_corpus_extractor_on_known_signals) {
  GIVEN("a 440 Hz sine on the left only, and a 220 Hz sine whose pitch swings 20 cents at 6 Hz, in both channels, sounding 0.8 s of every second");
  const size_t n = static_cast<size_t>(4.0 * corpus::kRate);
  std::vector<float> l(n), r(n, 0.0f), fl(n);
  double phase = 0.0;
  for (size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / corpus::kRate;
    l[i] = static_cast<float>(0.3 * std::sin(dsp::kTau * 440.0 * t));
    phase += dsp::kTau * 220.0 * std::exp2(20.0 / 1200.0 * std::sin(dsp::kTau * 6.0 * t)) / corpus::kRate;
    fl[i] = std::fmod(t, 1.0) < 0.8 ? static_cast<float>(0.3 * std::sin(phase)) : 0.0f;
  }
  const corpus::Features one = corpus::measure(l, r, corpus::kRate), swung = corpus::measure(fl, fl, corpus::kRate);
  THEN("they read as features.py reads the same signals: centroid 439.98 Hz and roll-off 441.43 Hz (the bin above), correlation 0 against silence and 1 between equal channels, wobble 11.08 cents");
  CHECK_NEAR(one.centroid_hz, 439.98, 0.05, "centroid, Hz");
  CHECK_NEAR(one.rolloff85_hz, 441.43, 0.01, "roll-off, Hz");
  CHECK_NEAR(one.corr, 0.0, 1e-6, "correlation");
  CHECK_NEAR(swung.corr, 1.0, 1e-6, "identical channels");
  CHECK_NEAR(swung.wobble_cents, 11.08, 0.3, "wobble, cents");
}

SCENARIO(every_corpus_preset_sits_inside_the_records_ranges) {
  GIVEN("each preset fitted to the corpus playing its phrase: a horn section's three lines in counterpoint, or a bass horn's bass line, 16 s at 44.1 kHz");
  THEN("its spectral centroid, 85% roll-off, L/R correlation and tape wobble lie inside the 25th to 75th percentile of every group of excerpts it answers to, measured as that stem, but for a stated allowance");
  for (const corpus::Case& c : corpus::cases()) {
    const long index = corpus::preset_index(c.preset);
    if (!CHECK(index >= 0, std::string("a preset named ") + c.preset)) continue;
    Settings s = Settings::defaults();
    presets::apply(static_cast<size_t>(index), s.v.data());
    const corpus::Audio a = corpus::play(s, c.phrase);
    const corpus::Features f = corpus::measure(a.left, a.right, corpus::kRate);
    std::printf("    %-28s as %-5s centroid %6.0f Hz  roll-off %6.0f Hz  corr %5.2f  wobble %5.2f ct\n", c.preset, c.stem, f.centroid_hz, f.rolloff85_hz, f.corr, f.wobble_cents);
    for (const char* group : c.groups) {
      const corpus::Reference* ref = corpus::reference(group, c.stem);
      if (!CHECK(ref != nullptr, std::string("reference ") + group + " " + c.stem)) continue;
      const std::string out = corpus::outside(f, *ref, c);
      CHECK(out.empty(), std::string(c.preset) + ": " + out);
    }
  }
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
