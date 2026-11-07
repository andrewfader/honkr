// Fitting tool behind docs/PHYSICS.md. Build and run from the repository root:
//   g++ -O2 -std=c++20 -o build/fitbrass tools/fit/fitbrass.cpp -lpthread && build/fitbrass "Alto sax"
#include "../../tests/reference_match.hpp"
#include <cstdio>
// fitbrass "instrument" : coordinate descent over the bell's radiation corner and slope and the
// fitted dynamics laws (k, tilt, treble, lp_pp, lp_ff, lp_poles), against the recordings.
using namespace honkr::dsp::winds;
namespace hh = honkr::dsp::horn;
int main(int argc, char** argv) {
  auto all = reference::load_features("references/features.tsv");
  std::vector<reference::Note> refs;
  for (auto& n : all) if (n.instrument == argv[1]) refs.push_back(n);
  const int inst = reference::instrument_index(argv[1]);
  g_calibrate_only = inst;
  const auto& in = instrument(inst);
  const Dynamics d = dynamics_of(inst);
  bool has_pp = false;
  for (auto& n : refs) has_pp |= n.dynamic == "pp";
  // x: rad_hz, rad_poles, k, tilt, treble, lp_pp, lp_ff, lp_poles
  std::array<double, 8> x{in.acoustics.radiate_hz, 1, d.level_exponent, d.tilt_db, d.treble_db, d.lp_pp, d.lp_ff, double(d.lp_poles)};
  if (argc > 2) for (int i = 0; i < 8 && i + 2 < argc; ++i) x[i] = std::atof(argv[i + 2]);
  auto apply = [&](const std::array<double, 8>& v) {
    hh::g_radiate_hz = v[0]; hh::g_radiate_poles = int(v[1]);
    g_dyn_k = v[2]; g_dyn_tilt = v[3]; g_dyn_treble = v[4];
    g_dyn_lp_pp = v[5]; g_dyn_lp_ff = has_pp ? v[6] : v[5]; g_dyn_lp_order = int(v[7]);
  };
  reference::Score best_s;
  auto eval = [&](const std::array<double, 8>& v, reference::Score* out) {
    apply(v);
    auto s = reference::score(refs, 48000).begin()->second;
    if (out) *out = s;
    return s.profile_db + 10 * s.brightness + 10 * s.dynamic_bright + 0.3 * s.odd_even_db + 0.5 * s.dynamic_db;
  };
  double best = eval(x, &best_s);
  auto show = [&](const char* tag) {
    std::printf("%-12s %s rad %6.0f/%d k %.2f tilt %5.1f treble %5.1f lp %5.1f..%5.1f/%d  obj %6.2f profile %5.2f bright %.3f oe %5.2f noise %5.2f dyn %5.2f dynb %.3f\n", argv[1], tag,
                x[0], int(x[1]), x[2], x[3], x[4], x[5], x[6], int(x[7]), best, best_s.profile_db, best_s.brightness, best_s.odd_even_db, best_s.noise_db, best_s.dynamic_db, best_s.dynamic_bright);
    std::fflush(stdout);
  };
  show("start");
  if (getenv("NOFIT")) return 0;
  // multiplicative steps for rad, lp; additive for k, tilt, treble
  std::array<double, 8> step{1.6, 0, 0.6, 4, 4, 1.6, 1.6, 0};
  for (int round = 0; round < 6; ++round) {
    bool moved = false;
    for (int i = 0; i < 8; ++i) {
      if (!has_pp && (i == 2 || i == 3 || i == 6)) continue;
      std::vector<std::array<double, 8>> tries;
      if (i == 1) { auto t = x; t[1] = x[1] == 1 ? 2 : 1; tries.push_back(t); }
      else if (i == 7) { if (x[5] <= 0) continue; auto t = x; t[7] = x[7] == 2 ? 4 : 2; tries.push_back(t); }
      else if (i == 0 || i == 5 || i == 6) {
        double base = x[i];
        if (base <= 0) { if (i == 5) { for (double v : {2.0, 4.0, 8.0}) { auto t = x; t[5] = v; t[6] = std::max(v, x[6] > 0 ? x[6] : 30.0); tries.push_back(t); } } else continue; }
        else { auto a = x, b = x; a[i] = base * step[i]; b[i] = base / step[i]; tries.push_back(a); tries.push_back(b); if (i == 5) { auto c = x; c[5] = 0; c[6] = 0; tries.push_back(c); } }
      } else { auto a = x, b = x; a[i] += step[i]; b[i] = std::max(i == 2 ? 0.0 : -30.0, b[i] - step[i]); tries.push_back(a); tries.push_back(b); }
      for (auto& t : tries) {
        if (t[5] > 0 && t[6] < t[5]) t[6] = t[5];
        reference::Score s;
        double o = eval(t, &s);
        if (o < best - 1e-3) { best = o; best_s = s; x = t; moved = true; }
      }
    }
    char tag[16]; std::snprintf(tag, sizeof tag, "r%d", round); show(tag);
    if (!moved) { for (int i : {0, 5, 6}) step[i] = std::sqrt(step[i]); for (int i : {2, 3, 4}) step[i] /= 2; if (step[0] < 1.05) break; }
  }
  show("final");
}
