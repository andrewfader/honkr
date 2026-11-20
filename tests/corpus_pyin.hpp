// librosa's pyin (librosa 1.0.0, librosa/core/pitch.py), as song-corpus/scripts/features.py
// calls it: so the corpus gate measures a render's tape wobble with the tracker the records were
// measured with. Probabilistic YIN (Mauch and Dixon 2014) as librosa implements it: zero-padded
// centred frames, the cumulative mean normalised difference, parabolic shifts, a Beta(2, 18)
// prior over 100 thresholds and a Boltzmann prior over troughs, 10-cent pitch bins, and Viterbi
// decoding over voiced and unvoiced bins with a triangular transition band and a 1e-4 floor on
// the transitions searched. Returns each frame's pitch bin frequency, or 0 where unvoiced.
#pragma once

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

#include "audio_measure.hpp"

namespace honkr::corpus::pyin {

inline std::vector<double> track(const std::vector<float>& signal, double sr, double fmin, double fmax, int frame_length, int hop) {
  const int n_thresholds = 100, bins_per_semitone = 10;
  const double boltzmann = 2.0, switch_prob = 0.01, no_trough_prob = 0.01, max_transition_rate = 35.92, min_transition = 1e-4;
  std::vector<double> y(static_cast<size_t>(frame_length / 2), 0.0);
  y.insert(y.end(), signal.begin(), signal.end());
  y.insert(y.end(), static_cast<size_t>(frame_length / 2), 0.0);
  const long frames = 1 + (static_cast<long>(y.size()) - frame_length) / hop;
  const int min_period = static_cast<int>(std::floor(sr / fmax));
  const int max_period = std::min(static_cast<int>(std::ceil(sr / fmin)), frame_length - 1);
  const int width = max_period - min_period + 1;
  const int bins = static_cast<int>(std::floor(12.0 * bins_per_semitone * std::log2(fmax / fmin))) + 1;
  const int states = 2 * bins;

  std::vector<double> thresholds(static_cast<size_t>(n_thresholds + 1)), beta(static_cast<size_t>(n_thresholds));
  for (int i = 0; i <= n_thresholds; ++i) thresholds[static_cast<size_t>(i)] = static_cast<double>(i) / n_thresholds;
  const auto beta_cdf = [](double x) { return 1.0 - std::pow(1.0 - x, 19.0) - 19.0 * x * std::pow(1.0 - x, 18.0); };
  for (int i = 0; i < n_thresholds; ++i) beta[static_cast<size_t>(i)] = beta_cdf(thresholds[static_cast<size_t>(i + 1)]) - beta_cdf(thresholds[static_cast<size_t>(i)]);

  std::vector<double> log_obs(static_cast<size_t>(frames) * static_cast<size_t>(states));
  size_t n_fft = 1;
  while (n_fft < static_cast<size_t>(2 * frame_length - 1)) n_fft <<= 1;
  std::vector<std::complex<double>> spectrum(n_fft);
  std::vector<double> acf(static_cast<size_t>(max_period + 1)), d(static_cast<size_t>(max_period + 1)), cmnd(static_cast<size_t>(width)),
      shift(static_cast<size_t>(width)), probs, obs(static_cast<size_t>(states));
  std::vector<int> troughs;
  for (long t = 0; t < frames; ++t) {
    const double* x = y.data() + t * hop;
    for (size_t i = 0; i < n_fft; ++i) spectrum[i] = i < static_cast<size_t>(frame_length) ? x[i] : 0.0;
    measure::fft(spectrum);
    for (auto& v : spectrum) v = std::norm(v);
    measure::fft(spectrum);
    for (int k = 0; k <= max_period; ++k) acf[static_cast<size_t>(k)] = spectrum[static_cast<size_t>(k)].real() / static_cast<double>(n_fft);
    double energy = 0.0;
    d[0] = 0.0;
    for (int k = 1; k <= max_period; ++k) {
      energy += x[k - 1] * x[k - 1];
      d[static_cast<size_t>(k)] = 2.0 * (acf[0] - acf[static_cast<size_t>(k)]) - energy;
    }
    double running = 0.0;
    for (int k = 1; k <= max_period; ++k) {
      running += d[static_cast<size_t>(k)];
      if (k >= min_period) cmnd[static_cast<size_t>(k - min_period)] = d[static_cast<size_t>(k)] / (running / k + DBL_MIN);
    }
    for (int i = 0; i < width; ++i) {
      shift[static_cast<size_t>(i)] = 0.0;
      if (i == 0 || i == width - 1) continue;
      const double a = cmnd[static_cast<size_t>(i + 1)] + cmnd[static_cast<size_t>(i - 1)] - 2.0 * cmnd[static_cast<size_t>(i)];
      const double b = 0.5 * (cmnd[static_cast<size_t>(i + 1)] - cmnd[static_cast<size_t>(i - 1)]);
      if (std::abs(b) < std::abs(a)) shift[static_cast<size_t>(i)] = -b / a;
    }
    troughs.clear();
    for (int i = 0; i < width; ++i) {
      bool trough;
      if (i == 0) trough = cmnd[0] < cmnd[1];
      else if (i == width - 1) trough = cmnd[static_cast<size_t>(i)] < cmnd[static_cast<size_t>(i - 1)];
      else trough = cmnd[static_cast<size_t>(i)] < cmnd[static_cast<size_t>(i - 1)] && cmnd[static_cast<size_t>(i)] <= cmnd[static_cast<size_t>(i + 1)];
      if (trough) troughs.push_back(i);
    }
    std::fill(obs.begin(), obs.end(), 0.0);
    if (!troughs.empty()) {
      probs.assign(troughs.size(), 0.0);
      for (int th = 0; th < n_thresholds; ++th) {
        const double limit = thresholds[static_cast<size_t>(th + 1)];
        int below = 0;
        for (int tr : troughs) below += cmnd[static_cast<size_t>(tr)] < limit;
        if (below == 0) continue;
        const double norm = (1.0 - std::exp(-boltzmann)) / (1.0 - std::exp(-boltzmann * below));
        int position = 0;
        for (size_t j = 0; j < troughs.size(); ++j)
          if (cmnd[static_cast<size_t>(troughs[j])] < limit) probs[j] += norm * std::exp(-boltzmann * position++) * beta[static_cast<size_t>(th)];
      }
      size_t global = 0;
      for (size_t j = 1; j < troughs.size(); ++j) if (cmnd[static_cast<size_t>(troughs[j])] < cmnd[static_cast<size_t>(troughs[global])]) global = j;
      double unreached = 0.0;
      for (int th = 0; th < n_thresholds; ++th) if (!(cmnd[static_cast<size_t>(troughs[global])] < thresholds[static_cast<size_t>(th + 1)])) unreached += beta[static_cast<size_t>(th)];
      probs[global] += no_trough_prob * unreached;
      for (size_t j = 0; j < troughs.size(); ++j) {
        if (probs[j] == 0.0) continue;
        const double period = min_period + troughs[j] + shift[static_cast<size_t>(troughs[j])];
        const double bin = std::clamp(std::nearbyint(12.0 * bins_per_semitone * std::log2(sr / period / fmin)), 0.0, static_cast<double>(bins));
        if (bin < bins) obs[static_cast<size_t>(bin)] = probs[j];
      }
    }
    double voiced = 0.0;
    for (int b = 0; b < bins; ++b) voiced += obs[static_cast<size_t>(b)];
    voiced = std::clamp(voiced, 0.0, 1.0);
    for (int b = bins; b < states; ++b) obs[static_cast<size_t>(b)] = (1.0 - voiced) / bins;
    for (int s = 0; s < states; ++s) log_obs[static_cast<size_t>(t) * states + static_cast<size_t>(s)] = std::log(obs[static_cast<size_t>(s)] + DBL_MIN);
  }

  // The transitions: within a voicing, a triangle 2 * 5 semitones + 1 bins wide, row-normalised;
  // across voicings the same band scaled by the switch probability.
  const int band = static_cast<int>(std::lround(max_transition_rate * 12.0 * hop / sr)) * bins_per_semitone + 1;
  std::vector<std::vector<double>> local(static_cast<size_t>(bins));
  std::vector<int> first(static_cast<size_t>(bins));
  for (int i = 0; i < bins; ++i) {
    const int lo = std::max(0, i - band / 2), hi = std::min(bins - 1, i + band / 2);
    double sum = 0.0;
    for (int j = lo; j <= hi; ++j) sum += 2.0 * (band / 2 + 1 - std::abs(j - i)) / (band + 1);
    first[static_cast<size_t>(i)] = lo;
    for (int j = lo; j <= hi; ++j) local[static_cast<size_t>(i)].push_back(2.0 * (band / 2 + 1 - std::abs(j - i)) / (band + 1) / sum);
  }
  const double log_floor = std::log(min_transition + DBL_MIN);
  struct Pred { int from; double log_p; };
  std::vector<std::vector<Pred>> preds(static_cast<size_t>(states));
  for (int to = 0; to < states; ++to) {
    const int jb = to % bins;
    for (int from = 0; from < states; ++from) {
      const int ib = from % bins;
      const int k = jb - first[static_cast<size_t>(ib)];
      if (k < 0 || k >= static_cast<int>(local[static_cast<size_t>(ib)].size())) continue;
      const double p = ((from < bins) == (to < bins) ? 1.0 - switch_prob : switch_prob) * local[static_cast<size_t>(ib)][static_cast<size_t>(k)];
      const double lp = std::log(p + DBL_MIN);
      if (lp >= log_floor) preds[static_cast<size_t>(to)].push_back({from, lp});
    }
  }
  std::vector<double> value(static_cast<size_t>(states)), next(static_cast<size_t>(states));
  std::vector<uint16_t> ptr(static_cast<size_t>(frames) * static_cast<size_t>(states));
  const double log_init = std::log(1.0 / states + DBL_MIN);
  for (int s = 0; s < states; ++s) value[static_cast<size_t>(s)] = log_obs[static_cast<size_t>(s)] + log_init;
  for (long t = 1; t < frames; ++t) {
    for (int j = 0; j < states; ++j) {
      double best = -INFINITY;
      int arg = 0;
      for (const Pred& p : preds[static_cast<size_t>(j)]) {
        const double cost = value[static_cast<size_t>(p.from)] + p.log_p;
        if (cost > best) { best = cost; arg = p.from; }
      }
      ptr[static_cast<size_t>(t) * states + static_cast<size_t>(j)] = static_cast<uint16_t>(arg);
      next[static_cast<size_t>(j)] = log_obs[static_cast<size_t>(t) * states + static_cast<size_t>(j)] + best;
    }
    value.swap(next);
  }
  std::vector<double> f0(static_cast<size_t>(frames));
  int state = static_cast<int>(std::max_element(value.begin(), value.end()) - value.begin());
  for (long t = frames - 1; t >= 0; --t) {
    f0[static_cast<size_t>(t)] = state < bins ? fmin * std::exp2(static_cast<double>(state) / (12.0 * bins_per_semitone)) : 0.0;
    if (t > 0) state = ptr[static_cast<size_t>(t) * states + static_cast<size_t>(state)];
  }
  return f0;
}

}  // namespace honkr::corpus::pyin
