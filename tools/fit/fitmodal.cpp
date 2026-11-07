// Fitting tool behind docs/PHYSICS.md. Build and run from the repository root:
//   g++ -O2 -std=c++20 -o build/fitmodal tools/fit/fitmodal.cpp -lpthread && build/fitmodal "Alto sax"
#include "../../tests/reference_match.hpp"
#include <cstdio>
// fitmodal "instrument" : coordinate descent over the modal bore, reed, bell and the dynamics laws.
using namespace honkr::dsp::winds;
namespace hh = honkr::dsp::horn;
constexpr int N = 16;
const char* names[N] = {"zeta","peak","q","tilt","cutoff","even","reed","rad","radp","k","dtilt","treble","lp_pp","lp_ff","psoft","ploud"};
int main(int argc, char** argv) {
  auto all = reference::load_features("references/features.tsv");
  std::vector<reference::Note> refs;
  for (auto& n : all) if (n.instrument == argv[1]) refs.push_back(n);
  const int inst = reference::instrument_index(argv[1]);
  g_calibrate_only = inst;
  const auto& a = instrument(inst).acoustics;
  const Dynamics d = dynamics_of(inst);
  bool has_pp = false; for (auto& n : refs) has_pp |= n.dynamic == "pp";
  std::array<double, N> x{a.zeta, a.modal_peak, a.modal_q, a.modal_tilt, a.modal_cutoff, a.modal_even, a.valve_hz, a.radiate_hz, double(a.radiate_poles), d.level_exponent, d.tilt_db, d.treble_db, d.lp_pp, d.lp_ff, instrument(inst).pressure_soft, instrument(inst).pressure_loud};
  for (int i = 2; i < argc && i - 2 < N; ++i) if (std::string(argv[i]) != "-") x[i - 2] = std::atof(argv[i]);
  const bool fix_even = getenv("FIXEVEN");
  auto apply = [&](const std::array<double, N>& v) {
    hh::g_modal_zeta = v[0]; hh::g_modal_peak = v[1]; hh::g_modal_q = v[2]; hh::g_modal_tilt = v[3]; hh::g_modal_cutoff = v[4]; hh::g_modal_even = v[5]; hh::g_modal_reed_hz = v[6];
    hh::g_radiate_hz = v[7]; hh::g_radiate_poles = int(v[8]);
    g_dyn_k = v[9]; g_dyn_tilt = v[10]; g_dyn_treble = v[11]; g_dyn_lp_pp = v[12]; g_dyn_lp_ff = has_pp ? v[13] : v[12]; g_dyn_lp_order = 2; g_pressure_soft = v[14]; g_pressure_loud = v[15];
  };
  reference::Score bs; int boff = 0;
  // The bare loop over the compass at the breath's range and bent a semitone either way: notes
  // that leave their pitch (a squeal, an octave) are penalised as heavily as a missed recording.
  auto squeals = [&]() {
    const auto& in = instrument(inst); const auto& cal = Calibration::at(48000); int bad = 0;
    for (int key = in.low; key <= in.high; key += 3) for (double pr : {soft_of(in) + 0.02, 0.5 * (soft_of(in) + loud_of(in)), loud_of(in)}) for (double bend : {-1.0, 0.0, 1.0}) {
      const double hz = 440 * std::exp2((key - 69) / 12.0); const double c = cal.correction(inst, hz); const double f0 = hz * std::exp2(bend / 12);
      hh::Horn h; h.prepare(48000, 0.09); h.set_acoustics(in.acoustics); hh::Tuning t; t.bore = 48000 / f0 * c;
      std::vector<float> xs(24000); honkr::dsp::Rng rng(11);
      for (size_t i = 0; i < xs.size(); ++i) xs[i] = h.tick(t, float(pr * std::min(1.0, i / 480.0)), 0.002f * rng.bipolar());
      measure::Span sp(xs, 12000, 12000); const double f = measure::pitch_hz(sp, 48000, f0 * 0.5, f0 * 2.2);
      if (measure::rms(sp) > 1e-3 && (f <= 0 || std::abs(1200 * std::log2(f / f0)) > 100)) ++bad;
    }
    return bad;
  };
  auto eval = [&](const std::array<double, N>& v, reference::Score* out, int* offp) {
    apply(v); Calibration::forget();
    std::vector<reference::Played> det;
    auto s = reference::score(refs, 48000, &det).begin()->second;
    int off = 0; for (auto& p : det) if (p.model.f0 <= 0 || std::abs(timbre::cents(p.model.f0, 440.0 * std::exp2((p.reference.key - 69) / 12.0))) > 40) ++off;
    off += squeals();
    if (out) *out = s; if (offp) *offp = off;
    return s.profile_db + 10 * s.brightness + 10 * s.dynamic_bright + 0.5 * s.odd_even_db + 0.5 * s.dynamic_db + 0.3 * s.noise_db + 20.0 * off;
  };
  double best = eval(x, &bs, &boff);
  auto show = [&](const char* tag) {
    std::printf("%-12s %s", argv[1], tag); for (int i = 0; i < N; ++i) std::printf(" %s %.3g", names[i], x[i]);
    std::printf("\n    obj %6.2f off %d profile %5.2f bright %.3f oe %5.2f noise %5.2f dyn %5.2f dynb %.3f\n", best, boff, bs.profile_db, bs.brightness, bs.odd_even_db, bs.noise_db, bs.dynamic_db, bs.dynamic_bright);
    std::fflush(stdout);
  };
  show("start");
  if (getenv("NOFIT")) return 0;
  // multiplicative steps except tilt, k, dtilt, treble (additive); radp toggles
  std::array<double, N> step{1.3, 1.5, 1.5, 0.3, 1.5, 0.6, 1.5, 1.5, 0, 0.6, 4, 4, 1.6, 1.6, 0.06, 0.06};
  const bool mult[N] = {1,1,1,0,1,0,1,1,0,0,0,0,1,1,0,0};
  for (int round = 0; round < 8; ++round) {
    bool moved = false;
    for (int i = 0; i < N; ++i) {
      if (!has_pp && (i == 9 || i == 10 || i == 13)) continue;
      if (fix_even && i == 5) continue;
      std::vector<std::array<double, N>> tries;
      if (i == 8) { auto t = x; t[8] = x[8] == 1 ? 2 : 1; tries.push_back(t); }
      else if (i == 12) {
        if (x[12] <= 0) for (double v : {2.0, 4.0, 8.0}) { auto t = x; t[12] = v; t[13] = std::max(v, 30.0); tries.push_back(t); }
        else { auto p = x, q = x, z = x; p[12] *= step[12]; q[12] /= step[12]; z[12] = 0; z[13] = 0; tries.push_back(p); tries.push_back(q); tries.push_back(z); }
      } else if (i == 13) { if (x[12] <= 0) continue; auto p = x, q = x; p[13] *= step[13]; q[13] /= step[13]; tries.push_back(p); tries.push_back(q); }
      else if (mult[i]) { auto p = x, q = x; p[i] *= step[i]; q[i] /= step[i]; tries.push_back(p); tries.push_back(q); }
      else { auto p = x, q = x; p[i] += step[i]; q[i] -= step[i]; if (i == 9 || i == 3 || i == 5) q[i] = std::max(0.0, q[i]); if (i == 5) p[i] = std::min(1.0, p[i]); if (i == 15) p[i] = std::min(1.0, p[i]); if (i == 14) p[i] = std::min(p[i], x[15] - 0.05); if (i == 15) q[i] = std::max(q[i], x[14] + 0.05); tries.push_back(p); tries.push_back(q); }
      for (auto& t : tries) {
        if (t[12] > 0 && t[13] < t[12]) t[13] = t[12];
        reference::Score s; int off;
        const double o = eval(t, &s, &off);
        if (o < best - 1e-3) { best = o; bs = s; boff = off; x = t; moved = true; }
      }
    }
    char tag[16]; std::snprintf(tag, sizeof tag, "r%d", round); show(tag);
    if (!moved) { bool any = false; for (int i = 0; i < N; ++i) { if (mult[i] && step[i] > 1.05) { step[i] = std::sqrt(step[i]); any = true; } else if (!mult[i] && step[i] > 0.05) { step[i] /= 2; any = true; } } if (!any) break; }
  }
  show("final");
}
