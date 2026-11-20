// Executable scenarios for features/corpus_gate.feature: the presets fitted to the song corpus,
// each played as the records use it and measured as the corpus measured the records' separated
// stems (tests/corpus_gate.hpp), held inside the interquartile ranges of the excerpts it answers
// to (tests/corpus_reference.hpp, generated from the corpus by tools/corpus_reference.py).
#include <cstring>

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
  GIVEN("each preset fitted to the corpus playing its phrase: a horn section's three lines in counterpoint, or the tuba's line, 16 s at 44.1 kHz");
  THEN("its spectral centroid, 85% roll-off, L/R correlation and tape wobble lie inside the 25th to 75th percentile of every group of excerpts it answers to, measured as that stem; the tuba's spectrum inside the range of the real tuba's taped takes");
  for (const corpus::Case& c : corpus::cases()) {
    const long index = corpus::preset_index(c.preset);
    if (!CHECK(index >= 0, std::string("a preset named ") + c.preset)) continue;
    Settings s = Settings::defaults();
    presets::apply(static_cast<size_t>(index), s.v.data());
    const corpus::Audio a = corpus::play(s, c.phrase);
    const corpus::Features f = corpus::measure(a.left, a.right, corpus::kRate);
    std::printf("    %-28s as %-5s centroid %6.0f Hz  roll-off %6.0f Hz  corr %5.2f  wobble %5.2f ct\n", c.preset, c.stem, f.centroid_hz, f.rolloff85_hz, f.corr, f.wobble_cents);
    const std::string out = corpus::outside(f, c);
    CHECK(out.empty(), std::string(c.preset) + ": " + out);
  }
}

SCENARIO(the_home_taped_tuba_is_recorded_as_the_real_tuba_it_answers_to) {
  GIVEN("the Home-taped tuba preset, and the bedroom, microphone pair and tape the Iowa tuba's taped takes went through");
  Settings s = Settings::defaults();
  presets::apply(static_cast<size_t>(corpus::preset_index("Home-taped tuba")), s.v.data());
  Settings as_reference = s;
  corpus::record_as(corpus::kHomeTape, as_reference);
  THEN("its room, microphone distance, pattern and setup, wow and flutter and tape bandwidth are those, so the two differ only in the instrument");
  for (params::ParamId id : {params::kRoomSize, params::kMicDistance, params::kMicPattern, params::kMicSetup, params::kWowFlutter, params::kTapeBandwidth})
    CHECK_NEAR(s[id], as_reference[id], 1e-9, params::spec(id).name);
}

SCENARIO(the_tuba_played_dry_sounds_where_the_recorded_tuba_does) {
  GIVEN("HONKR's tuba playing the tuba line dry (no room, the microphone on the bell, no vibrato, the master 18 dB down so the output limiter never acts) at velocity 0.55 and 0.95, and the University of Iowa tuba's mezzo-forte and fortissimo notes spliced into the same line (anechoic, 1.5 m)");
  THEN("measured as the corpus measures, each dynamic's spectral centroid and 85% roll-off lie within 0.3 octave of the recording's");
  for (const corpus::Take& take : corpus::kIowaTuba) {
    if (take.taped) continue;
    Settings s = Settings::defaults();
    s.v[params::kInstrument] = params::kTuba;
    s.v[params::kVibrato] = 0.0;
    s.v[params::kMasterDb] = -18.0;
    const double velocity = std::strcmp(take.dynamic, "mf") == 0 ? 0.55 : 0.95;
    const corpus::Audio a = corpus::play(s, corpus::Phrase::TubaLine, corpus::kSeconds, velocity);
    const corpus::Features f = corpus::measure(a.left, a.right, corpus::kRate);
    std::printf("    %s  centroid %4.0f Hz (recorded %4.0f)  roll-off %4.0f Hz (recorded %4.0f)\n", take.dynamic, f.centroid_hz, take.centroid_hz, f.rolloff85_hz, take.rolloff85_hz);
    CHECK_NEAR(std::log2(f.centroid_hz / take.centroid_hz), 0.0, 0.3, std::string(take.dynamic) + " centroid, octaves from the recording's");
    CHECK_NEAR(std::log2(f.rolloff85_hz / take.rolloff85_hz), 0.0, 0.3, std::string(take.dynamic) + " roll-off, octaves from the recording's");
  }
}

SCENARIO(what_is_not_a_taped_tuba_fails_the_tubas_ranges) {
  GIVEN("the tuba's line played by a studio tuba (the Tuba preset: no tape, a hall), by the Home-taped tuba with its tape taken off, and by a bass clarinet recorded as the Home-taped tuba is");
  const corpus::Case& tuba = corpus::cases().back();
  struct Impostor { const char* preset; std::vector<std::pair<params::ParamId, double>> changes; };
  const std::vector<Impostor> impostors{
      {"Tuba", {}},
      {"Home-taped tuba", {{params::kWowFlutter, 0.0}, {params::kTapeBandwidth, 20000.0}}},
      {"Home-taped tuba", {{params::kInstrument, params::kBassClarinet}}},
  };
  THEN("each falls outside a range the Home-taped tuba answers to");
  for (const Impostor& imp : impostors) {
    Settings s = Settings::defaults();
    presets::apply(static_cast<size_t>(corpus::preset_index(imp.preset)), s.v.data());
    for (const auto& [id, value] : imp.changes) s.v[id] = value;
    const corpus::Audio a = corpus::play(s, corpus::Phrase::TubaLine);
    const corpus::Features f = corpus::measure(a.left, a.right, corpus::kRate);
    const std::string out = corpus::outside(f, tuba);
    std::printf("    %-16s %-14s centroid %4.0f Hz  roll-off %4.0f Hz  corr %4.2f  wobble %5.2f ct: %s\n", imp.preset, params::kInstrumentNames[s.instrument()], f.centroid_hz, f.rolloff85_hz, f.corr, f.wobble_cents, out.c_str());
    CHECK(!out.empty(), std::string(imp.preset) + " (" + params::kInstrumentNames[s.instrument()] + ") is told apart");
  }
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
