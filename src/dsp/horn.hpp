// One player's instrument as a sound-producing loop: a valve or a jet at the mouthpiece (a cane
// reed, the player's lips, an air jet over an edge) driving the air column of the bore, which
// returns its wave to the mouthpiece a round trip later and radiates the rest from the bell or
// the open tone holes. The air column is a digital waveguide; the valve is the nonlinearity that
// keeps it going. Everything a horn needs per sample lives here; which notes an instrument can
// play and how its player blows it live in winds.hpp.
//
// The mouthpieces:
//   Reed on a cylinder (no instrument now; the modal bore below replaced it): a cane reed beating against the lay, a valve
//     that the mouth's pressure closes. The flow through it is Bernoulli's through the reed's
//     opening, u = zeta (1 - dp) sqrt(dp), with the pressure difference dp in units of the
//     pressure that shuts the reed (Kergomard; Chaigne and Kergomard, Acoustics of Musical
//     Instruments ch. 9). The reed itself is a mass on a spring, resonant well above the note
//     (a squeak when it takes over). The mouthpiece's pressure is solved exactly each sample
//     (Guillemain, Kergomard and Voinier 2005). The cylinder is closed at the reed and open at
//     the bell, so its wave comes back inverted and it sounds mostly odd harmonics. The note
//     starts at about a third of the closing pressure and the reed chokes as the mouth nears it.
//   Reed on a modal bore (clarinets, saxophones, bassoon, the drones): the same reed and flow,
//     the bore given as its resonances, a damped resonator at each harmonic of the note (Silva,
//     Kergomard, Vergez and Gilbert 2014); the cylinder's even ones all but absent.
//   Reed on a cone (oboe, the bagpipe chanter): the wave also travels toward
//     the cone's missing apex; the loop is split at the reed into the part toward the bell and
//     the part toward the apex (Cook's and Scavone's conical waveguide), and the reed is
//     Smith's reflection-coefficient table. Every harmonic sounds.
//   Lips (brass, shofar, didgeridoo): the player's lips, an outward-striking valve that the
//     mouth's pressure blows open, a mass on a spring tuned by the player a little below the
//     note (Helmholtz; Elliott and Bowsher 1982; Adachi and Sato 1996). The same Bernoulli flow
//     through their opening. The lips choose which of the bore's resonances sounds, as a brass
//     player's embouchure does; the bore's resonances, drawn by the bell into nearly a complete
//     harmonic series (Benade), come back upright on each round trip.
//   Jet (flutes, recorder, whistle, shakuhachi, pan pipes): an air jet across an edge, deflected
//     by the acoustic flow at the mouth and arriving at the edge a travel time later, switching
//     its flow into or out of the pipe through a saturating curve (Fletcher; Verge; Coltman).
//   Vessel (ocarina): the same jet driving a Helmholtz resonator, one air mass bouncing on the
//     air spring of the vessel: one mode, so the tone is nearly a pure sine.
//
// Sources: Fletcher and Rossing, The Physics of Musical Instruments (2nd ed.), chs. 8, 13-17;
// Benade, Fundamentals of Musical Acoustics; Chaigne and Kergomard, Acoustics of Musical
// Instruments (2016); Smith, Physical Audio Signal Processing (2010); Scavone, "An acoustic
// analysis of single-reed woodwind instruments" (1997); Cook, "A meta-wind-instrument physical
// model" (1992).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "core.hpp"

namespace honkr::dsp::horn {
inline double g_dc_hz = 8.0;

enum class Mouth : uint8_t { Reed, Lips, Jet, Vessel };
enum class Bore : uint8_t { Cylinder, Cone, Brass, Open, Stopped, Helmholtz, Modal };

// The acoustics of one instrument, independent of the note.
struct Acoustics {
  Mouth mouth = Mouth::Reed;
  Bore bore = Bore::Cylinder;
  double zeta = 0.35;         // valve: the opening's size against the bore (the embouchure parameter)
  double valve_hz = 2500.0;   // reed: its own resonance (lips are tuned per note)
  double valve_q = 3.0;       // the valve's resonance Q
  double lip_gain = 3.0;      // lips: how far the pressure difference blows them open
  double reed_slope = 0.3;    // cone reed: how far the pressure difference closes it
  double reed_offset = 0.7;   // cone reed: its rest opening
  double apex = 0.4;          // cone: the share of the loop between the reed and the apex
  double jet_ratio = 0.93;    // jet: travel time across the mouth against the period
  double jet_drive = 0.9;     // jet: how far the mouth's flow bends it
  double jet_offset = 0.8;    // jet: where it rests against the edge (off-centre: even harmonics)
  double bell_hz = 2000.0;    // the bell's (or tone-hole lattice's) cutoff: above it the bore stops reflecting
  double loss = 0.95;         // the bore's reflection at low frequencies (walls and radiation)
  double radiate_hz = 300.0;  // radiation from the bell rises below this corner
  int radiate_poles = 1;      // and how steeply: 1 (6 dB an octave) or 2 (12)
  double vessel_q = 30.0;     // vessel: the Helmholtz resonance's Q
  double steepening_us = 0.0; // brass: how far a crest of unit pressure gains on the wave down the bore, in microseconds
  // A modal bore (a cone as its resonances): the n-th at n times the note, its peak impedance
  // modal_peak n^-modal_tilt (times modal_even when n is even), falling past the tone holes'
  // cutoff as (f / cutoff)^-4, its Q modal_q sqrt(n) (the walls' losses grow as the root of the
  // frequency).
  double modal_peak = 30.0;
  double modal_q = 30.0;
  double modal_tilt = 0.5;
  double modal_cutoff = 1500.0;
  double modal_even = 1.0;
};

// Experiments' overrides of the modal bore (tools/fit); unset below zero.
inline double g_modal_peak = -1.0, g_modal_q = -1.0, g_modal_tilt = -1.0, g_modal_cutoff = -1.0, g_modal_even = -1.0, g_modal_zeta = -1.0, g_modal_reed_hz = -1.0;

// Experiments' overrides of every instrument's radiation (tools/fit); unset below zero.
inline double g_radiate_hz = -1.0;
inline int g_radiate_poles = -1;

// A delay line with linear interpolation, sized once.
class Line {
 public:
  void size(size_t n) { buf_.assign(std::max<size_t>(n, 8), 0.0f); w_ = 0; }
  void clear() { std::fill(buf_.begin(), buf_.end(), 0.0f); }
  size_t capacity() const { return buf_.size(); }
  void push(float x) { buf_[w_] = x; w_ = (w_ + 1) % buf_.size(); }
  // The sample pushed `d` samples ago, d >= 1.
  float read(double d) const {
    const size_t n = buf_.size();
    d = std::clamp(d, 1.0, static_cast<double>(n - 2));
    const size_t i = static_cast<size_t>(d);
    const float f = static_cast<float>(d - static_cast<double>(i));
    const float a = buf_[(w_ + n - i) % n], b = buf_[(w_ + n - i - 1) % n];
    return a + (b - a) * f;
  }
  // The same with four-point (Catmull-Rom) interpolation, d >= 2: flat to far higher in the
  // treble than the linear read, for a delay that moves on every sample.
  float read_cubic(double d) const {
    const size_t n = buf_.size();
    d = std::clamp(d, 2.0, static_cast<double>(n - 3));
    const size_t i = static_cast<size_t>(d);
    const float f = static_cast<float>(d - static_cast<double>(i));
    const float y0 = buf_[(w_ + n - i + 1) % n], y1 = buf_[(w_ + n - i) % n], y2 = buf_[(w_ + n - i - 1) % n], y3 = buf_[(w_ + n - i - 2) % n];
    return y1 + 0.5f * f * (y2 - y0 + f * (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 + f * (3.0f * (y1 - y2) + y3 - y0)));
  }
 private:
  std::vector<float> buf_;
  size_t w_ = 0;
};

// The note-dependent tuning of the loop.
struct Tuning {
  double bore = 100.0;     // the loop's delay, in samples
  double jet = 50.0;       // jet: the travel time, in samples
};

class Horn {
 public:
  void prepare(double rate, double longest_s) {
    rate_ = rate;
    const size_t n = static_cast<size_t>(rate * longest_s) + 16;
    a_.size(n); b_.size(n);
    steep_.size(static_cast<size_t>(rate * 0.0008) + 16);
    jet_.size(static_cast<size_t>(rate / 30.0) + 16);
    dc_ = static_cast<float>(1.0 - kTau * g_dc_hz / rate_);
    reset();
  }
  void reset() {
    a_.clear(); b_.clear(); jet_.clear(); steep_.clear();
    steep_delay_ = 0.0004 * rate_;
    lp_ = lp2_ = 0.0f; dc_x_ = dc_y_ = 0.0f; rad_ = rad2_ = 0.0f; out_ = 0.0f;
    x1_ = x2_ = 0.0; dp_ = 0.0;
    v1_ = v2_ = 0.0f;
    mre_.fill(0.0f); mim_.fill(0.0f); modal_bore_ = -1.0; modal_age_ = 0;
  }

  // The acoustics change between notes, never during one.
  void set_acoustics(const Acoustics& a) {
    ac_ = a;
    set_bell(a.bell_hz);
    if (g_radiate_hz > 0.0) ac_.radiate_hz = g_radiate_hz;
    if (g_radiate_poles > 0) ac_.radiate_poles = g_radiate_poles;
    radiate_ = static_cast<float>(std::exp(-kTau * std::clamp(ac_.radiate_hz, 10.0, rate_ * 0.45) / rate_));
    if (a.bore == Bore::Modal) {
      if (g_modal_peak > 0.0) ac_.modal_peak = g_modal_peak;
      if (g_modal_q > 0.0) ac_.modal_q = g_modal_q;
      if (g_modal_tilt >= 0.0) ac_.modal_tilt = g_modal_tilt;
      if (g_modal_cutoff > 0.0) ac_.modal_cutoff = g_modal_cutoff;
      if (g_modal_even >= 0.0) ac_.modal_even = g_modal_even;
      if (g_modal_zeta > 0.0) ac_.zeta = g_modal_zeta;
      if (g_modal_reed_hz > 0.0) ac_.valve_hz = g_modal_reed_hz;
      modal_bore_ = -1.0;
    }
    if (a.mouth == Mouth::Reed) set_valve(ac_.valve_hz);
  }
  // The bell's cutoff moves with a mute or the player's hand.
  void set_bell(double hz) { bell_ = static_cast<float>(std::exp(-kTau * std::clamp(hz, 50.0, rate_ * 0.45) / rate_)); }
  const Acoustics& acoustics() const { return ac_; }
  // The valve's resonance (the reed's own, or the lips' as the player sets them for the note):
  // a two-pole low-pass of unity gain at DC, so its displacement is the pressure difference
  // itself below resonance.
  void set_valve(double hz) { set_valve(hz, ac_.valve_q); }
  void set_valve(double hz, double q) {
    const double w = kTau * std::clamp(hz, 10.0, rate_ * 0.45) / rate_;
    const double r = std::exp(-w / (2.0 * std::max(0.5, q)));
    va1_ = -2.0 * r * std::cos(w);
    va2_ = r * r;
    vb0_ = 1.0 + va1_ + va2_;
  }
  void set_vessel(double hz) {
    const double w = kTau * std::clamp(hz, 20.0, rate_ * 0.45) / rate_;
    const double r = std::exp(-w / (2.0 * std::max(2.0, ac_.vessel_q)));
    ha1_ = static_cast<float>(-2.0 * r * std::cos(w));
    ha2_ = static_cast<float>(r * r);
    hg_ = static_cast<float>(1.0 - r);
  }
  // The embouchure for the coming samples: the valve's opening (zeta), the cone reed's
  // stiffness, the jet's offset against the edge.
  void set_zeta(double z) { ac_.zeta = std::clamp(z, 0.02, 2.0); }
  void set_reed_slope(double s) { ac_.reed_slope = std::clamp(s, 0.05, 1.0); }
  void set_reed_offset(double o) { ac_.reed_offset = std::clamp(o, 0.05, 1.0); }
  void set_jet_offset(double o) { ac_.jet_offset = std::clamp(o, 0.0, 2.0); }

  // One sample. `p` is the mouth's (or the jet's) pressure in model units, `noise` the breath's
  // turbulence already scaled. Returns the pressure radiated from the bell.
  float tick(const Tuning& t, float p, float noise) {
    float bore_out = 0.0f;
    switch (ac_.mouth) {
      case Mouth::Reed: bore_out = ac_.bore == Bore::Cone ? reed_cone(t, p + noise) : ac_.bore == Bore::Modal ? modal(t, p + noise) : valve(t, p + noise, false); break;
      case Mouth::Lips: bore_out = valve(t, p + noise, true); break;
      case Mouth::Jet: bore_out = jet(t, p, noise); break;
      case Mouth::Vessel: bore_out = vessel(t, p, noise); break;
    }
    if (!std::isfinite(bore_out)) { reset(); bore_out = 0.0f; }
    bore_out = steepen(bore_out);
    // The bell radiates the time derivative of the flow: a gentle high-pass.
    rad_ = radiate_ * rad_ + (1.0f - radiate_) * bore_out;
    out_ = bore_out - rad_;
    if (ac_.radiate_poles >= 2) {
      rad2_ = radiate_ * rad2_ + (1.0f - radiate_) * out_;
      out_ -= rad2_;
    }
    return out_;
  }
  float last() const { return out_; }
  // The valve's opening as last computed (0 shut): for tests and pictures.
  double opening() const { return opening_; }

 private:
  // The bell keeps what is below its cutoff and lets the rest out: two poles of low-pass in the
  // loop, and the walls' loss.
  float reflect(float x) {
    lp_ = bell_ * lp_ + (1.0f - bell_) * x;
    lp2_ = bell_ * lp2_ + (1.0f - bell_) * lp_;
    return lp2_ * static_cast<float>(ac_.loss);
  }
  // Brassiness: in a long narrow bore a loud wave steepens as it travels, its crests (where the
  // air is compressed and warmer) running ahead of its troughs, until at fortissimo it is close
  // to a shock wave at the bell (Hirschberg, Gilbert, Msallam and Wijnands 1996). The radiated
  // wave is read through a delay shortened at its crests and lengthened at its troughs
  // (Msallam et al. 2000): quiet notes pass unchanged, loud ones gain their high harmonics.
  float steepen(float x) {
    if (ac_.steepening_us <= 0.0) return x;
    steep_.push(x);
    const double base = 0.0004 * rate_;
    const double shift = std::clamp(static_cast<double>(x), -1.5, 1.5) * ac_.steepening_us * 1e-6 * rate_;
    // Where the crest has caught its trough the wave is a shock and steepens no further: the
    // read never runs backwards in time.
    const double delay = std::clamp(std::min(base - shift, steep_delay_ + 0.95), 2.0, 2.0 * base);
    steep_delay_ = delay;
    return steep_.read_cubic(delay);
  }
  float block_dc(float x) {
    const float y = x - dc_x_ + dc_ * dc_y_;
    dc_x_ = x; dc_y_ = y;
    return y;
  }

  // A pressure-controlled valve at a closed end: a reed (inward-striking, shut by the mouth's
  // pressure) or the lips (outward-striking, blown open by it). The valve moves on last
  // sample's pressure difference; given its opening, the mouthpiece pressure p = 2 p- + u(p)
  // has a closed form in sqrt|dp| (a quadratic), so no iteration is needed.
  float valve(const Tuning& t, float gamma, bool lips) {
    const double x = vb0_ * dp_ - va1_ * x1_ - va2_ * x2_;
    x2_ = x1_; x1_ = x;
    const double h = lips ? std::max(0.0, ac_.lip_gain * x) : std::max(0.0, 1.0 - x);
    opening_ = h;
    // The wave returning to the mouthpiece: inverted from the open end of a cylinder; upright
    // in a brass bore, whose resonances the bell has drawn into a harmonic series.
    const float arriving = a_.read(t.bore);
    const float back = (ac_.bore == Bore::Brass ? 1.0f : -1.0f) * reflect(arriving);
    const double w = static_cast<double>(gamma) - 2.0 * back;  // the difference were no air to flow
    const double k = ac_.zeta * h;
    double root, dp;
    if (w >= 0.0) { root = 0.5 * (-k + std::sqrt(k * k + 4.0 * w)); dp = root * root; }
    else { root = 0.5 * (-k + std::sqrt(k * k - 4.0 * w)); dp = -root * root; }
    const double p = gamma - dp;
    dp_ = dp;
    a_.push(block_dc(static_cast<float>(p - back)));
    return arriving;
  }

  // The bore as its resonances (Silva, Kergomard, Vergez and Gilbert 2014), driven by the same
  // reed and Bernoulli flow as the cylinder's valve. Each mode is a complex one-pole, exact for
  // its own resonance; the pressure is what the modes carry over from the last sample plus the
  // instantaneous response to this sample's flow, p = v + zi u, so the flow is solved exactly as
  // on the cylinder. Unlike a waveguide cone, nothing cancels a harmonic: the modes sit on them.
  float modal(const Tuning& t, float gamma) {
    if (modal_bore_ < 0.0 || ((modal_age_++ & 15) == 0 && std::abs(t.bore - modal_bore_) > 1e-6 * modal_bore_)) set_modes(t.bore);
    const double x = vb0_ * dp_ - va1_ * x1_ - va2_ * x2_;
    x2_ = x1_; x1_ = x;
    const double h = std::max(0.0, 1.0 - x);
    opening_ = h;
    double v = 0.0;
    for (int n = 0; n < modes_; ++n) {
      const float re = pre_[n] * mre_[n] - pim_[n] * mim_[n], im = pre_[n] * mim_[n] + pim_[n] * mre_[n];
      mre_[n] = re; mim_[n] = im;
      v += 2.0 * re;
    }
    const double w = static_cast<double>(gamma) - v;
    const double k = ac_.zeta * h * zi_;
    double root, dp;
    if (w >= 0.0) { root = 0.5 * (-k + std::sqrt(k * k + 4.0 * w)); dp = root * root; }
    else { root = 0.5 * (-k + std::sqrt(k * k - 4.0 * w)); dp = -root * root; }
    dp_ = dp;
    const double u = ac_.zeta * h * (w >= 0.0 ? root : -root);
    for (int n = 0; n < modes_; ++n) mre_[n] += static_cast<float>(0.5 * gain_[n] * u);
    return static_cast<float>(gamma - dp);
  }
  void set_modes(double bore) {
    modal_bore_ = bore;
    const double w1 = kTau / std::max(bore, 2.0);
    zi_ = 0.0;
    modes_ = 0;
    for (int n = 1; n <= kModes; ++n) {
      const double w = n * w1;
      if (w > kTau * 0.42) break;
      const double hz = w * rate_ / kTau;
      const double q = ac_.modal_q * std::sqrt(static_cast<double>(n));
      const double alpha = w / (2.0 * q);
      const double peak = ac_.modal_peak * std::pow(static_cast<double>(n), -ac_.modal_tilt) * (n % 2 == 0 ? ac_.modal_even : 1.0) / (1.0 + std::pow(hz / std::max(ac_.modal_cutoff, 50.0), 4.0));
      const double r = std::exp(-alpha);
      pre_[modes_] = static_cast<float>(r * std::cos(w));
      pim_[modes_] = static_cast<float>(r * std::sin(w));
      gain_[modes_] = peak * alpha;
      zi_ += gain_[modes_];
      ++modes_;
    }
    for (int n = modes_; n < kModes; ++n) mre_[n] = mim_[n] = 0.0f;
  }

  // A truncated cone: the wave from the reed runs to the bell (line a) and comes back inverted;
  // it also runs to the cone's apex and back (line b). The reed sits between the two.
  float reed_cone(const Tuning& t, float p) {
    const double to_apex = std::max(1.0, t.bore * ac_.apex), to_bell = std::max(1.0, t.bore - to_apex);
    const float bell = -reflect(a_.read(to_bell));
    const float bore = bell - b_.read(to_apex);
    const float pd = p - bore;
    const float opening = std::clamp(static_cast<float>(ac_.reed_offset + ac_.reed_slope * pd), -1.0f, 1.0f);
    opening_ = opening;
    a_.push(block_dc(p - pd * opening - bell));
    b_.push(bell);
    return bore;
  }

  // The jet over the edge: what comes back from the pipe bends the jet; the jet, a travel time
  // later, switches its flow into or out of the pipe (a hard, off-centre curve).
  float jet(const Tuning& t, float p, float noise) {
    float back = reflect(a_.read(t.bore));
    if (ac_.bore == Bore::Stopped) back = -back;
    back = block_dc(back);
    jet_.push(back + noise);
    const float wind = std::max(0.02f, p);
    // The jet's speed goes as the root of the pressure: harder blowing shortens its travel, and
    // far enough the pipe jumps to its octave (overblowing).
    const double travel = t.jet / std::sqrt(std::sqrt(static_cast<double>(wind)));
    const float bend = static_cast<float>(ac_.jet_drive) * wind * jet_.read(travel);
    // The jet's offset from the edge is a share of the jet itself, which grows as it is blown
    // harder: kept in proportion, the jet stays asymmetric (even harmonics) at fortissimo rather
    // than saturating symmetrically into odd ones.
    const float off = static_cast<float>(ac_.jet_offset) * wind;
    const float flow = std::tanh(bend + off) - std::tanh(off);
    opening_ = flow;
    a_.push(back + 1.6f * std::sqrt(wind) * flow + 0.5f * noise);
    return back + noise;
  }
  // The jet over a vessel's mouth: one resonance instead of a bore.
  float vessel(const Tuning& t, float p, float noise) {
    const float wind = std::max(0.02f, p);
    const double travel = t.jet / std::sqrt(std::sqrt(static_cast<double>(wind)));
    const float bend = static_cast<float>(ac_.jet_drive) * wind * jet_.read(travel);
    const float off = static_cast<float>(ac_.jet_offset);
    const float flow = std::tanh(bend + off) - std::tanh(off);
    opening_ = flow;
    const float drive = -std::sqrt(wind) * flow + noise;
    const float y = hg_ * drive - ha1_ * v1_ - ha2_ * v2_;
    v2_ = v1_; v1_ = y;
    const float out = block_dc(y * 8.0f);
    jet_.push(out + noise);
    return out;
  }

  double rate_ = 48000.0;
  Acoustics ac_{};
  Line a_, b_, jet_, steep_;
  float bell_ = 0.5f, radiate_ = 0.9f, dc_ = 0.999f;
  float lp_ = 0.0f, lp2_ = 0.0f, dc_x_ = 0.0f, dc_y_ = 0.0f, rad_ = 0.0f, rad2_ = 0.0f, out_ = 0.0f;
  double va1_ = 0.0, va2_ = 0.0, vb0_ = 1.0, x1_ = 0.0, x2_ = 0.0, dp_ = 0.0, opening_ = 0.0, steep_delay_ = 19.2;
  float ha1_ = 0.0f, ha2_ = 0.0f, hg_ = 0.0f, v1_ = 0.0f, v2_ = 0.0f;
  static constexpr int kModes = 32;
  std::array<float, kModes> mre_{}, mim_{}, pre_{}, pim_{};
  std::array<double, kModes> gain_{};
  int modes_ = 0, modal_age_ = 0;
  double zi_ = 0.0, modal_bore_ = -1.0;
};

}  // namespace honkr::dsp::horn
