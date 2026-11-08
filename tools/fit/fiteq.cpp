// The body response fit behind docs/PHYSICS.md, from the repository root:
//   g++ -O2 -std=c++20 -o build/dumpprof tools/fit/dumpprof.cpp -lpthread && build/dumpprof > build/prof_dump.tsv
//   g++ -O2 -std=c++20 -o build/fiteq tools/fit/fiteq.cpp && build/fiteq build/prof_dump.tsv 0.005
#include "../../src/dsp/filters.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
// fiteq prof_dump.tsv : per instrument, the gains of a half-octave peaking-filter bank (125 Hz to
// 8 kHz) that best close the gap between the model's harmonic profiles and the recordings'.
using honkr::dsp::Biquad;
constexpr int B = 13; constexpr double Q = 1.4, R = 48000;
struct Note { std::string dyn; int key; double f0; std::array<double,10> m, r; };
int main(int argc, char** argv) {
  std::ifstream in(argv[1]); std::string line; std::map<std::string, std::vector<Note>> by;
  while (std::getline(in, line)) { std::stringstream s(line); std::string inst, dyn, f; std::getline(s, inst, '\t'); std::getline(s, dyn, '\t'); std::getline(s, f, '\t');
    Note n; n.dyn = dyn; n.key = std::stoi(f); std::getline(s, f, '\t'); n.f0 = std::stod(f); for (auto& v : n.m) { std::getline(s, f, '\t'); v = std::stod(f); } for (auto& v : n.r) { std::getline(s, f, '\t'); v = std::stod(f); } by[inst].push_back(n); }
  std::array<double,B> hz; for (int k = 0; k < B; ++k) hz[k] = 125.0 * std::pow(2.0, k / 2.0);
  const double lambda = argc > 2 ? std::atof(argv[2]) : 0.05;
  for (auto& [inst, ns] : by) {
    // unit response of each band at every harmonic of every note: dB of a +6 dB peak, scaled (near-linear in gain)
    auto eq_db = [&](const std::array<double,B>& g, double f) { double e = 0; for (int k = 0; k < B; ++k) if (g[k] != 0) { Biquad b; b.set(Biquad::Type::Peak, hz[k], Q, g[k], R); e += b.response_db(f, R); } return e; };
    auto centroid = [](const std::array<double,10>& p) { double num = 0, den = 0; for (int h = 0; h < 10; ++h) { const double e = std::pow(10.0, p[h] / 10.0); num += (h + 1) * e; den += e; } return num / den; };
    auto obj = [&](const std::array<double,B>& g, double* prof) {
      double tot = 0, br = 0; std::vector<double> cm(ns.size()), cr(ns.size());
      for (size_t i = 0; i < ns.size(); ++i) { const Note& n = ns[i]; std::array<double,10> p; double mx = -1e9;
        for (int h = 0; h < 10; ++h) { const double f = n.f0 * (h + 1); p[h] = n.m[h] + (f < R * 0.45 ? eq_db(g, f) : 0); mx = std::max(mx, p[h]); }
        for (auto& v : p) v = std::max(-60.0, v - mx);
        double sq = 0; for (int h = 0; h < 8; ++h) { const double d = p[h] - n.r[h]; sq += d * d; } tot += std::sqrt(sq / 8);
        cm[i] = centroid(p); cr[i] = centroid(n.r); br += std::abs(std::log2(cm[i] / cr[i])); }
      tot /= ns.size(); br /= ns.size();
      double db = 0; int pairs = 0;
      for (size_t i = 0; i < ns.size(); ++i) if (ns[i].dyn == "pp") { int loud = -1;
        for (size_t j = 0; j < ns.size(); ++j) if (ns[j].key == ns[i].key && (ns[j].dyn == "ff" || (loud < 0 && ns[j].dyn == "mf"))) loud = int(j);
        if (loud >= 0) { db += std::abs(std::log2((cm[loud] / cm[i]) / (cr[loud] / cr[i]))); ++pairs; } }
      if (pairs) db /= pairs;
      if (prof) { prof[0] = tot; prof[1] = br; prof[2] = db; }
      double smooth = 0; for (int k = 1; k < B; ++k) smooth += (g[k] - g[k-1]) * (g[k] - g[k-1]);
      double ridge = 0; for (int k = 0; k < B; ++k) ridge += g[k] * g[k];
      return tot + 10 * br + 10 * db + lambda * smooth + 0.2 * lambda * ridge;
    };
    std::array<double,B> g{}; double p0[3]; double best = obj(g, p0);
    for (double step : {6.0, 3.0, 1.5, 0.75, 0.3}) for (int pass = 0; pass < 6; ++pass) { bool moved = false;
      for (int k = 0; k < B; ++k) for (double d : {step, -step}) { auto t = g; t[k] = std::clamp(t[k] + d, -18.0, 18.0); double o = obj(t, nullptr); if (o < best - 1e-4) { best = o; g = t; moved = true; } }
      if (!moved) break; }
    double p1[3]; obj(g, p1);
    std::printf("%-14s profile %5.2f -> %5.2f  bright %.3f -> %.3f  dynb %.3f -> %.3f  gains", inst.c_str(), p0[0], p1[0], p0[1], p1[1], p0[2], p1[2]); for (double v : g) std::printf(" %+5.1f", v); std::printf("\n");
  }
}
