// The recording machine after the room: what a home tape deck does to what its microphones gave
// it. Not the tape's own physics (no magnetisation, bias, saturation or hiss), only the two
// things that a cheap machine is heard by and that a measurement of its records shows.
//
// - Wow and flutter: the tape's speed swings, so every pitch on it swings together, in both
//   channels. A slow wow at 0.73 Hz (a capstan or a reel turning) and a flutter at 6.1 Hz (the
//   tape juddering over the heads), each swinging up to half the stated depth, so together they
//   reach it where they coincide. A changing speed is a changing delay: read d(t) seconds late,
//   the tape plays at 1 - d'(t) of its speed, so a swing of c cents at f Hz is a delay of
//   A (1 - cos 2 pi f t) with A = (2^(c / 1200) - 1) / (2 pi f), read with four-point Hermite
//   interpolation two samples behind the newest.
// - Bandwidth: the heads' and the tape's loss of treble, a fourth-order Butterworth low-pass whose
//   -3 dB point is the stated bandwidth. At 20 kHz the tape passes everything; with no wow or
//   flutter it adds no delay.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "core.hpp"
#include "filters.hpp"

namespace honkr::dsp::tape {

class Tape {
 public:
  static constexpr double kWowHz = 0.73, kFlutterHz = 6.1, kOpenHz = 20000.0;

  void prepare(double rate) {
    rate_ = rate;
    size_t n = 4;
    while (n < static_cast<size_t>(0.02 * rate) + 8) n <<= 1;
    for (auto& b : buffer_) b.assign(n, 0.0f);
    depth_.set_time(50.0, rate);
    bandwidth_ = -1.0;
    reset();
  }
  void reset() {
    for (auto& b : buffer_) std::fill(b.begin(), b.end(), 0.0f);
    write_ = 0;
    wow_phase_ = flutter_phase_ = 0.0;
    depth_.snap(cents_);
    for (auto& f : low_) for (auto& q : f) q.reset();
  }
  void configure(double wow_flutter_cents, double bandwidth_hz) {
    cents_ = std::max(0.0, wow_flutter_cents);
    depth_.target(cents_);
    if (bandwidth_hz != bandwidth_) {
      bandwidth_ = bandwidth_hz;
      for (auto& f : low_) {
        f[0].set(Biquad::Type::LowPass, bandwidth_, 0.5412, 0.0, rate_);
        f[1].set(Biquad::Type::LowPass, bandwidth_, 1.3066, 0.0, rate_);
      }
    }
  }
  void process(float& l, float& r) {
    const size_t n = buffer_[0].size();
    buffer_[0][write_] = l; buffer_[1][write_] = r;
    const double cents = depth_.next();
    if (cents > 1e-6 || cents_ > 0.0) {
      const double delay = 2.0 + rate_ * (amplitude_s(0.5 * cents, kWowHz) * (1.0 - std::cos(wow_phase_)) +
                                          amplitude_s(0.5 * cents, kFlutterHz) * (1.0 - std::cos(flutter_phase_)));
      wow_phase_ = std::fmod(wow_phase_ + kTau * kWowHz / rate_, kTau);
      flutter_phase_ = std::fmod(flutter_phase_ + kTau * kFlutterHz / rate_, kTau);
      const size_t whole = static_cast<size_t>(delay);
      const float t = static_cast<float>(delay - static_cast<double>(whole));
      const auto at = [&](const std::vector<float>& b, size_t back) { return b[(write_ + n - back) % n]; };
      const auto hermite = [&](const std::vector<float>& b) {
        const float xm1 = at(b, whole - 1), x0 = at(b, whole), x1 = at(b, whole + 1), x2 = at(b, whole + 2);
        const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
      };
      l = hermite(buffer_[0]); r = hermite(buffer_[1]);
    }
    write_ = (write_ + 1) % n;
    if (bandwidth_ < kOpenHz) {
      l = low_[0][1].process(low_[0][0].process(l));
      r = low_[1][1].process(low_[1][0].process(r));
    }
  }

 private:
  static double amplitude_s(double cents, double hz) { return (std::exp2(cents / 1200.0) - 1.0) / (kTau * hz); }

  double rate_ = 48000.0, cents_ = 0.0, bandwidth_ = kOpenHz, wow_phase_ = 0.0, flutter_phase_ = 0.0;
  std::array<std::vector<float>, 2> buffer_;
  size_t write_ = 0;
  Smoother depth_;
  std::array<std::array<Biquad, 2>, 2> low_;
};

}  // namespace honkr::dsp::tape
