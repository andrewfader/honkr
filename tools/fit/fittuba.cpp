// Fitting tool behind docs/PHYSICS.md's tuba. Build and run from the repository root:
//   g++ -O2 -std=c++20 -Isrc -Itests -o build/fittuba tools/fit/fittuba.cpp && build/fittuba ~/.cache/honkr-references [x0..x10]
// Coordinate descent over the tuba's bell radiation corner and slope and its fitted dynamics laws
// and the breath's range and the treble shelf's corner (as tools/fit/fitbrass.cpp, with the
// breath the other brass had refitted), against every recorded note's harmonic profile (the first ten
// harmonics) and, beyond the tenth, the tuba line's long-term spectrum: HONKR playing it dry at
// velocity 0.55 and 0.95 against the recording's mezzo-forte and fortissimo spliced into it,
// third-octave bands 100 Hz to 2.5 kHz, centroid and roll-off as the song corpus measures them;
// penalised for any score worse than references/baseline.tsv's. LINE_WEIGHT (60) weighs the line's
// centroid and roll-off against the profiles.
#include <cstdio>

#include "../../tests/corpus_gate.hpp"
#include "../../tests/reference_match.hpp"
#include "../references/tuba_line.hpp"

using namespace honkr;
using namespace honkr::dsp::winds;
namespace hh = honkr::dsp::horn;

namespace {

std::vector<double> bands_db(const std::vector<float>& y) {
  const std::vector<double> P = corpus::long_term_spectrum(y);
  std::vector<double> out;
  double total = 0.0;
  for (int k = -10; k <= 4; ++k) {
    const double fc = 1000.0 * std::exp2(k / 3.0);
    double b = 0.0;
    for (size_t i = 0; i < P.size(); ++i) {
      const double hz = static_cast<double>(i) * corpus::kRate / corpus::kSpectrumFft;
      if (hz >= fc * std::exp2(-1.0 / 6.0) && hz < fc * std::exp2(1.0 / 6.0)) b += P[i];
    }
    out.push_back(b);
    total += b;
  }
  for (double& b : out) b = 10.0 * std::log10(std::max(b / total, 1e-20));
  return out;
}

struct Line { std::vector<double> bands; corpus::Features f; };

Line line_of(const std::vector<float>& mono) { return {bands_db(mono), corpus::measure(mono, mono, corpus::kRate)}; }

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: fittuba CACHE_DIR [x0..x10]\n"); return 2; }
  auto all = reference::load_features("references/features.tsv");
  std::vector<reference::Note> refs;
  for (auto& n : all) if (n.instrument == "Tuba") refs.push_back(n);
  const int inst = params::kTuba;
  g_calibrate_only = inst;
  const double line_weight = std::getenv("LINE_WEIGHT") ? std::atof(std::getenv("LINE_WEIGHT")) : 60.0;
  const reference::Score baseline = reference::load_baseline("references/baseline.tsv").at("Tuba");
  std::array<Line, 2> recorded;
  const std::array<const char*, 2> dynamics{"mf", "ff"};
  const std::array<double, 2> velocities{0.55, 0.95};
  for (size_t d = 0; d < 2; ++d) {
    std::string error;
    const auto line = references::tuba_line(argv[1], dynamics[d], &error);
    if (line.empty()) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
    recorded[d] = line_of(line);
  }
  const auto& in = instrument(inst);
  const Dynamics dy = dynamics_of(inst);
  // x: rad_hz, rad_poles, k, tilt, treble, lp_pp, lp_ff, lp_poles, the breath's softest and loudest pressure, the shelf's corner in harmonics
  using X = std::array<double, 11>;
  X x{in.acoustics.radiate_hz, double(in.acoustics.radiate_poles), dy.level_exponent, dy.tilt_db, dy.treble_db, dy.lp_pp, dy.lp_ff, double(dy.lp_poles), in.pressure_soft, in.pressure_loud, dy.shelf_harmonic};
  for (int i = 0; i < 11 && i + 2 < argc; ++i) x[static_cast<size_t>(i)] = std::atof(argv[i + 2]);
  auto apply = [&](const X& v) {
    hh::g_radiate_hz = v[0]; hh::g_radiate_poles = int(v[1]);
    g_dyn_k = v[2]; g_dyn_tilt = v[3]; g_dyn_treble = v[4];
    g_dyn_lp_pp = v[5]; g_dyn_lp_ff = v[6]; g_dyn_lp_order = int(v[7]);
    g_pressure_soft = v[8]; g_pressure_loud = v[9]; g_dyn_shelf = v[10];
  };
  struct Eval { double obj, line_db, line_oct; reference::Score s; std::array<corpus::Features, 2> f; };
  auto eval = [&](const X& v) {
    apply(v);
    Eval e{};
    e.s = reference::score(refs, 48000).begin()->second;
    for (size_t d = 0; d < 2; ++d) {
      Settings s = Settings::defaults();
      s.v[params::kInstrument] = inst;
      s.v[params::kVibrato] = 0.0;
      s.v[params::kMasterDb] = -18.0;
      const corpus::Audio a = corpus::play(s, corpus::Phrase::TubaLine, corpus::kSeconds, velocities[d]);
      std::vector<float> mono(a.left.size());
      for (size_t i = 0; i < mono.size(); ++i) mono[i] = 0.5f * (a.left[i] + a.right[i]);
      const Line l = line_of(mono);
      double sq = 0.0;
      for (size_t b = 0; b < l.bands.size(); ++b) sq += (l.bands[b] - recorded[d].bands[b]) * (l.bands[b] - recorded[d].bands[b]);
      e.line_db += std::sqrt(sq / static_cast<double>(l.bands.size())) / 2.0;
      e.line_oct += (std::abs(std::log2(l.f.centroid_hz / recorded[d].f.centroid_hz)) + std::abs(std::log2(l.f.rolloff85_hz / recorded[d].f.rolloff85_hz))) / 2.0;
      e.f[d] = l.f;
    }
    e.obj = e.s.profile_db + 10 * e.s.brightness + 10 * e.s.dynamic_bright + 0.3 * e.s.odd_even_db + 0.5 * e.s.dynamic_db + e.line_db + line_weight * e.line_oct;
    // Held to the ratchet: no score may end worse than references/baseline.tsv's.
    const auto now = reference::values(e.s), was = reference::values(baseline);
    for (size_t i = 0; i < now.size(); ++i) e.obj += 10.0 * std::max(0.0, now[i] - was[i]);
    return e;
  };
  Eval best = eval(x);
  auto show = [&](const char* tag) {
    std::printf("%-6s rad %4.0f/%d k %.2f tilt %5.1f treble %5.1f lp %5.2f..%6.2f/%d breath %.3f..%.3f shelf %.2f  obj %6.2f | profile %4.2f bright %.3f oe %4.2f dyn %4.2f dynb %.3f | line %4.1f dB %.3f oct: mf %3.0f/%3.0f Hz ff %3.0f/%3.0f Hz\n", tag,
                x[0], int(x[1]), x[2], x[3], x[4], x[5], x[6], int(x[7]), x[8], x[9], x[10], best.obj, best.s.profile_db, best.s.brightness, best.s.odd_even_db, best.s.dynamic_db, best.s.dynamic_bright,
                best.line_db, best.line_oct, best.f[0].centroid_hz, best.f[0].rolloff85_hz, best.f[1].centroid_hz, best.f[1].rolloff85_hz);
    std::fflush(stdout);
  };
  std::printf("recorded: mf %3.0f/%3.0f Hz ff %3.0f/%3.0f Hz\n", recorded[0].f.centroid_hz, recorded[0].f.rolloff85_hz, recorded[1].f.centroid_hz, recorded[1].f.rolloff85_hz);
  show("start");
  if (std::getenv("NOFIT")) {
    std::vector<reference::Played> detail;
    apply(x);
    reference::score(refs, 48000, &detail);
    for (const auto& p : detail)
      std::printf("%s %d  level %6.1f / %6.1f dB  centroid %5.2f / %5.2f\n", p.reference.dynamic.c_str(), p.reference.key, p.model.level_db, p.reference.f.level_db, p.model.centroid, p.reference.f.centroid);
    return 0;
  }
  X step{1.6, 0, 0.6, 4, 4, 1.6, 1.6, 0, 1.15, 1.15, 1.5};
  for (int round = 0; round < 8; ++round) {
    bool moved = false;
    for (size_t i = 0; i < x.size(); ++i) {
      std::vector<X> tries;
      if (i == 1) { auto t = x; t[1] = x[1] == 1 ? 2 : 1; tries.push_back(t); }
      else if (i == 7) { auto t = x; t[7] = x[7] == 2 ? 4 : 2; tries.push_back(t); }
      else if (i == 0 || i == 5 || i == 6 || i == 8 || i == 9 || i == 10) { auto a = x, b = x; a[i] *= step[i]; b[i] /= step[i]; tries.push_back(a); tries.push_back(b); }
      else { auto a = x, b = x; a[i] += step[i]; b[i] = std::max(i == 2 ? 0.0 : -30.0, b[i] - step[i]); tries.push_back(a); tries.push_back(b); }
      for (auto& t : tries) {
        if (t[6] < t[5]) t[6] = t[5];
        if (t[9] < t[8] * 1.2) continue;
        const Eval e = eval(t);
        if (e.obj < best.obj - 1e-3) { best = e; x = t; moved = true; }
      }
    }
    char tag[16]; std::snprintf(tag, sizeof tag, "r%d", round); show(tag);
    if (!moved) { for (int i : {0, 5, 6, 8, 9, 10}) step[static_cast<size_t>(i)] = std::sqrt(step[static_cast<size_t>(i)]); for (int i : {2, 3, 4}) step[static_cast<size_t>(i)] /= 2; if (step[0] < 1.05) break; }
  }
  show("final");
}
