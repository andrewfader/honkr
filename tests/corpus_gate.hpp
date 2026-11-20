// The song corpus's gate: the presets fitted to the records' separated stems, what each plays,
// the stem and groups of excerpts it answers to, and the features it is measured on, computed as
// song-corpus/scripts/features.py computes them. Shared by corpus_gate_tests and
// tools/corpus_render.cpp.
//
//   spectral: the long-term power spectrum of the mono sum (STFT n_fft 8192, hop 2048, periodic
//             Hann, centred frames zero-padded as librosa 1.0 pads them), averaged over frames
//             within 40 dB of the loudest; its centroid and 85% roll-off from 20 Hz.
//   corr:     the zero-lag correlation of the left and right channels, full band.
//   wobble:   the mono sum resampled to 22.05 kHz and tracked by librosa's pyin
//             (tests/corpus_pyin.hpp) every 256 samples over 2048-sample frames between 65 and 1050 Hz; in every run of at
//             least 30 voiced frames that move less than 60 cents frame to frame, the standard
//             deviation of the pitch in cents about its own 21-frame moving average; the median
//             over runs. The resampler differs from librosa's (soxr), so the figure is checked
//             against features.py's on every gated preset (docs/CORPUS.md).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstring>
#include <string>
#include <vector>

#include "../src/presets.hpp"
#include "audio_measure.hpp"
#include "corpus_phrases.hpp"
#include "corpus_pyin.hpp"
#include "corpus_reference.hpp"

namespace honkr::corpus {

struct Features { double centroid_hz = 0.0, rolloff85_hz = 0.0, corr = 1.0, wobble_cents = 0.0; };

namespace detail {

inline std::vector<std::vector<double>> stft_power(const std::vector<float>& y, int n_fft, int hop) {
  const int pad = n_fft / 2;
  const auto at = [&](long i) -> double { return i >= 0 && i < static_cast<long>(y.size()) ? y[static_cast<size_t>(i)] : 0.0; };
  const long frames = 1 + static_cast<long>(y.size()) / hop;
  std::vector<double> window(static_cast<size_t>(n_fft));
  for (int i = 0; i < n_fft; ++i) window[static_cast<size_t>(i)] = 0.5 - 0.5 * std::cos(2.0 * dsp::kPi * i / n_fft);
  std::vector<std::vector<double>> out;
  std::vector<std::complex<double>> buffer(static_cast<size_t>(n_fft));
  for (long f = 0; f < frames; ++f) {
    for (int i = 0; i < n_fft; ++i) buffer[static_cast<size_t>(i)] = at(f * hop - pad + i) * window[static_cast<size_t>(i)];
    measure::fft(buffer);
    std::vector<double> p(static_cast<size_t>(n_fft / 2 + 1));
    for (size_t k = 0; k < p.size(); ++k) p[k] = std::norm(buffer[k]);
    out.push_back(std::move(p));
  }
  return out;
}

inline double median(std::vector<double> v) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  const size_t n = v.size();
  return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

inline double stdev(const std::vector<double>& v) {
  double mean = 0.0, sum = 0.0;
  for (double x : v) mean += x;
  mean /= static_cast<double>(v.size());
  for (double x : v) sum += (x - mean) * (x - mean);
  return std::sqrt(sum / static_cast<double>(v.size()));
}

// Halves the rate: a 255-tap Blackman-windowed sinc low-pass at 10.5 kHz, then every other sample.
inline std::vector<float> to_22050(const std::vector<float>& y) {
  constexpr int kTaps = 255;
  std::array<double, kTaps> h{};
  double sum = 0.0;
  for (int i = 0; i < kTaps; ++i) {
    const double t = i - (kTaps - 1) / 2.0, fc = 10500.0 / 44100.0;
    const double sinc = t == 0.0 ? 2.0 * fc : std::sin(2.0 * dsp::kPi * fc * t) / (dsp::kPi * t);
    h[static_cast<size_t>(i)] = sinc * (0.42 - 0.5 * std::cos(2.0 * dsp::kPi * i / (kTaps - 1)) + 0.08 * std::cos(4.0 * dsp::kPi * i / (kTaps - 1)));
    sum += h[static_cast<size_t>(i)];
  }
  std::vector<float> out;
  out.reserve(y.size() / 2 + 1);
  for (size_t n = 0; n < y.size(); n += 2) {
    double acc = 0.0;
    for (int i = 0; i < kTaps; ++i) {
      const long j = static_cast<long>(n) - i + (kTaps - 1) / 2;
      if (j >= 0 && j < static_cast<long>(y.size())) acc += h[static_cast<size_t>(i)] * y[static_cast<size_t>(j)];
    }
    out.push_back(static_cast<float>(acc / sum));
  }
  return out;
}

// features.py's statistic, run for run: a run still open when the excerpt ends is not counted.
inline double wobble_cents(const std::vector<float>& mono) {
  const std::vector<double> f0 = pyin::track(to_22050(mono), 22050.0, 65.0, 1050.0, 2048, 256);
  std::vector<double> wob, run;
  for (double hz : f0) {
    const double x = hz > 0.0 ? 1200.0 * std::log2(hz / 440.0) : NAN;
    if (std::isfinite(x) && (run.empty() || std::abs(x - run.back()) < 60.0)) { run.push_back(x); continue; }
    if (run.size() >= 30) {
      constexpr size_t k = 21;
      std::vector<double> residual;
      for (size_t i = 0; i + k <= run.size(); ++i) {
        double mean = 0.0;
        for (size_t j = i; j < i + k; ++j) mean += run[j];
        residual.push_back(run[i + k / 2] - mean / k);
      }
      wob.push_back(stdev(residual));
    }
    run.clear();
    if (std::isfinite(x)) run.push_back(x);
  }
  return median(wob);
}

}  // namespace detail

constexpr int kSpectrumFft = 8192;

// The long-term power spectrum of a mono signal, bin k at k rate / kSpectrumFft Hz: the frames
// within 40 dB of the loudest, summed.
inline std::vector<double> long_term_spectrum(const std::vector<float>& y) {
  const auto S = detail::stft_power(y, kSpectrumFft, 2048);
  std::vector<double> e(S.size());
  double emax = 0.0;
  for (size_t t = 0; t < S.size(); ++t) { for (double v : S[t]) e[t] += v; emax = std::max(emax, e[t]); }
  std::vector<double> P(S[0].size(), 0.0);
  for (size_t t = 0; t < S.size(); ++t) if (e[t] > emax * 1e-4) for (size_t k = 0; k < P.size(); ++k) P[k] += S[t][k];
  return P;
}

inline Features measure(const std::vector<float>& left, const std::vector<float>& right, double rate) {
  Features f;
  std::vector<float> y(left.size());
  for (size_t i = 0; i < y.size(); ++i) y[i] = 0.5f * (left[i] + right[i]);
  {
    const std::vector<double> P = long_term_spectrum(y);
    const auto hz = [&](size_t k) { return static_cast<double>(k) * rate / kSpectrumFft; };
    double sum = 0.0, weighted = 0.0;
    for (size_t k = 0; k < P.size(); ++k) if (hz(k) >= 20.0) { sum += P[k]; weighted += P[k] * hz(k); }
    f.centroid_hz = weighted / sum;
    double running = 0.0;
    for (size_t k = 0; k < P.size(); ++k) if (hz(k) >= 20.0) { running += P[k]; if (running / sum >= 0.85) { f.rolloff85_hz = hz(k); break; } }
  }
  {
    double ll = 0.0, rr = 0.0, lr = 0.0;
    for (size_t i = 0; i < left.size(); ++i) { ll += static_cast<double>(left[i]) * left[i]; rr += static_cast<double>(right[i]) * right[i]; lr += static_cast<double>(left[i]) * right[i]; }
    f.corr = lr / (std::sqrt(ll * rr) + 1e-20);
  }
  f.wobble_cents = detail::wobble_cents(y);
  return f;
}

// The recording a home-taped tuba is held to: the University of Iowa tuba playing the tuba line
// (references/), passed through this bedroom, microphone pair and tape (tools/corpus_render.cpp
// iowa-tuba), measured by features.py (tools/corpus_reference.py). The preset gated against it
// records the same way, so the two differ only in the instrument.
struct Transfer { int room; double distance_m; int pattern; double mic_setup, wow_cents, bandwidth_hz; };
inline constexpr Transfer kHomeTape{2, 0.3, 1, 0.0, 18.0, 3000.0};

inline void record_as(const Transfer& t, Settings& s) {
  s.v[params::kRoomSize] = t.room;
  s.v[params::kMicDistance] = t.distance_m;
  s.v[params::kMicPattern] = t.pattern;
  s.v[params::kMicSetup] = t.mic_setup;
  s.v[params::kWowFlutter] = t.wow_cents;
  s.v[params::kTapeBandwidth] = t.bandwidth_hz;
}

// A group of excerpts or takes a preset answers to, and what of theirs it answers to: their
// spectrum (centroid and roll-off), their placement (L/R correlation and tape wobble), or both.
struct Answer { const char* group; bool spectrum = true, placement = true; };

// A gated preset: what it plays, the stem it is measured as, and the ranges it must sit inside
// (all of them).
struct Case { const char* preset; Phrase phrase; const char* stem; std::vector<Answer> answers; };

inline const std::vector<Case>& cases() {
  static const std::vector<Case> c{
      {"Home-taped horn section", Phrase::Counterpoint, "other", {{"OTC_winds"}, {"OTC"}}},
      {"Home-taped low brass", Phrase::Counterpoint, "other", {{"OTC_winds"}, {"OTC"}}},
      // The records' bass stems hold no tuba: a real one, separated by htdemucs_6s with or
      // without a band around it, lands in `bass` at mezzo-forte with its partials kept (the
      // stem's centroid rising from 120 Hz to 280 or more) and in `guitar` at fortissimo
      // (docs/CORPUS.md). Its spectrum answers to the real tuba recorded the same way; its
      // placement and the tape's wobble to the records.
      {"Home-taped tuba", Phrase::TubaLine, "bass", {{"Iowa_tuba_taped", true, false}, {"OTC", false, true}}},
  };
  return c;
}

inline const Reference* reference(const char* group, const char* stem) {
  for (const auto& r : kReferences) if (std::strcmp(r.group, group) == 0 && std::strcmp(r.stem, stem) == 0) return &r;
  return nullptr;
}

inline long preset_index(const char* name) {
  const auto& all = presets::all();
  for (size_t i = 0; i < all.size(); ++i) if (std::strcmp(all[i].name, name) == 0) return static_cast<long>(i);
  return -1;
}

// What lies outside the ranges a preset answers to in one group, one clause per feature; empty
// when inside.
inline std::string outside(const Features& f, const Reference& r, const Answer& a) {
  std::string out;
  const auto within = [&](const char* name, double value, const Stat& s) {
    if (value >= s.q25 && value <= s.q75) return;
    char line[160];
    std::snprintf(line, sizeof(line), "%s %s %.4g outside %.4g..%.4g; ", r.group, name, value, s.q25, s.q75);
    out += line;
  };
  if (a.spectrum) {
    within("centroid Hz", f.centroid_hz, r.centroid_hz);
    within("roll-off Hz", f.rolloff85_hz, r.rolloff85_hz);
  }
  if (a.placement) {
    within("L/R correlation", f.corr, r.corr);
    within("wobble cents", f.wobble_cents, r.wobble_cents);
  }
  return out;
}

// Every range of every group a case answers to that the features fall outside; empty when inside.
inline std::string outside(const Features& f, const Case& c) {
  std::string out;
  for (const Answer& a : c.answers) {
    const Reference* r = reference(a.group, c.stem);
    out += r ? outside(f, *r, a) : std::string("no reference ") + a.group + " " + c.stem + "; ";
  }
  return out;
}

}  // namespace honkr::corpus
