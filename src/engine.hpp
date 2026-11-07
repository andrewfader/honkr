// The instrument: a wind player (or a section of them) blowing one of the instruments in
// dsp/winds.hpp, heard in a room through the air, through the master level and an output
// limiter. The engine knows nothing about CLAP: it takes a snapshot of parameter values per block
// and note events with sample offsets, and it never allocates while rendering.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "dsp/air.hpp"
#include "dsp/core.hpp"
#include "dsp/winds.hpp"
#include "params.hpp"

namespace honkr {

// A block's worth of parameter values, copied out of the plug-in's atomics once per process
// call so everything in the block sees the same settings.
struct Settings {
  std::array<double, params::kParamCount> v{};
  double operator[](params::ParamId id) const { return v[static_cast<size_t>(id)]; }
  int instrument() const { return std::clamp(static_cast<int>(std::lround(v[params::kInstrument])), 0, params::kInstrumentCount - 1); }
  int option(params::ParamId id) const { return static_cast<int>(std::lround(v[static_cast<size_t>(id)])); }
  bool on(params::ParamId id) const { return v[static_cast<size_t>(id)] >= 0.5; }
  static Settings defaults() {
    Settings s;
    for (size_t i = 0; i < params::kParamCount; ++i) s.v[i] = params::kSpecs[i].def;
    return s;
  }
};

// Level trims that put every instrument on the same footing: one middle-register note at a
// firm breath with the default settings lands near -16 dBFS RMS once it has spoken (see
// docs/QUALITY.md; held there by engine_tests). A flute's jet makes far more of the model's
// pressure than a reed does; the trims let a preset's Master be a musical choice.
namespace levels {
constexpr std::array<double, params::kInstrumentCount> kInstrumentDb{
    -14.0,  // clarinet
    -11.8,  // bass clarinet
    -15.7,  // soprano sax
    -13.4,  // alto sax
    -13.4,  // tenor sax
    -13.3,  // baritone sax
    -11.6,  // oboe
    -21.6,  // bassoon
    -14.2,  // highland bagpipe
    -34.6,  // flute
    -32.9,  // piccolo
    -35.4,  // alto flute
    -33.3,  // recorder
    -32.9,  // tin whistle
    -35.1,  // pan flute
    -33.5,  // shakuhachi
    -29.9,  // ocarina
    +8.0,  // trumpet
    -3.7,  // flugelhorn
    +0.7,  // french horn
    -8.1,  // trombone
    -5.5,  // euphonium
    -10.5,  // tuba
    -1.1,  // bugle
    -7.9,  // alphorn
    -2.6,  // shofar
    -13.8,  // didgeridoo
};
inline double gain(int instrument) { return dsp::db_to_gain(kInstrumentDb[static_cast<size_t>(std::clamp(instrument, 0, params::kInstrumentCount - 1))]); }
// The note each instrument is levelled on: the middle of its compass.
inline int reference_key(int instrument) {
  const auto& in = dsp::winds::instrument(instrument);
  return (in.low + in.high) / 2;
}
}  // namespace levels

// The output limiter: nothing leaves above the ceiling. Instant attack on one detector for both
// channels, recovery over about 50 ms: a peak catcher, not a compressor.
class Limiter {
 public:
  static constexpr float kCeiling = 0.794f;  // -2 dBFS
  static constexpr double kClipDb = 9.0;     // reduction beyond which the squash is plainly audible
  void prepare(double rate) { release_ = std::exp(-1.0 / (0.050 * rate)); reset(); }
  void reset() { gain_ = 1.0; held_db_ = 0.0; }
  float gain(float l, float r) {
    const double peak = std::max(std::abs(l), std::abs(r));
    const double wanted = peak > kCeiling ? kCeiling / peak : 1.0;
    if (wanted < gain_) gain_ = wanted;
    else gain_ += (wanted - gain_) * (1.0 - release_);
    if (gain_ < 1.0) held_db_ = std::max(held_db_, -20.0 * std::log10(gain_));
    return static_cast<float>(gain_);
  }
  double held_db() const { return held_db_; }
  void reset_meter() { held_db_ = 0.0; }
 private:
  double release_ = 0.9998, gain_ = 1.0, held_db_ = 0.0;
};

class Engine {
 public:
  void prepare(double rate) {
    rate_ = rate;
    section_.prepare(rate);
    room_.prepare(rate);
    limiter_.prepare(rate);
    master_.set_time(10.0, rate);
    master_.snap(dsp::db_to_gain(params::kSpecs[params::kMasterDb].def));
    controller_smooth_.set_time(8.0, rate);
    reset();
  }
  void reset() {
    section_.reset();
    room_.reset();
    limiter_.reset();
    bend_.fill(0.0); wheel_.fill(0.0); pressure_.fill(0.0); breath_cc_.fill(0.0); expression_.fill(0.0);
    seen_.fill(false);
    peak_l_ = peak_r_ = 0.0f;
    clipped_ = false;
    controller_smooth_.snap(0.0);
  }

  // --- events ---
  void note_on(const Settings& s, int16_t channel, int key, int32_t note_id, double velocity) {
    (void)channel; (void)note_id;
    if (key < 0 || key > 127) return;
    configure(s);
    const uint32_t seed = static_cast<uint32_t>(++note_counter_ * 2654435761u);
    section_.note_on(key, std::clamp(velocity, 0.0, 1.0), seed);
  }
  void note_off(const Settings& s, int16_t channel, int key, int32_t note_id) {
    (void)channel; (void)note_id;
    if (key < 0) { all_notes_off(); return; }
    if (key > 127) return;
    configure(s);
    section_.note_off(key);
  }
  void all_notes_off() { section_.all_notes_off(); }
  void all_sound_off() { section_.all_sound_off(); }
  // The player's controllers, per MIDI channel: breath (CC 2), expression (CC 11), the mod wheel
  // (CC 1), pressure, the bend.
  void breath(int16_t channel, double v) { breath_cc_[ch(channel)] = std::clamp(v, 0.0, 1.0); seen_[params::kFromBreathCc] = true; }
  void expression(int16_t channel, double v) { expression_[ch(channel)] = std::clamp(v, 0.0, 1.0); seen_[params::kFromExpression] = true; }
  void mod_wheel(int16_t channel, double v) { wheel_[ch(channel)] = std::clamp(v, 0.0, 1.0); seen_[params::kFromModWheel] = true; }
  void aftertouch(int16_t channel, double v) { pressure_[ch(channel)] = std::clamp(v, 0.0, 1.0); seen_[params::kFromAftertouch] = true; }
  void pitch_bend(int16_t channel, double amount) { bend_[ch(channel)] = std::clamp(amount, -1.0, 1.0); }
  void set_tempo(double bpm) { if (std::isfinite(bpm) && bpm > 0.0) tempo_ = bpm; }
  double tempo() const { return tempo_; }

  int active_voices() const { return section_.sounding(); }
  bool silent() const { return !section_.awake(); }
  const dsp::winds::Section& section() const { return section_; }
  float peak_left() const { return peak_l_; }
  float peak_right() const { return peak_r_; }
  bool clipped() const { return clipped_; }
  double limited_db() const { return limiter_.held_db(); }
  void reset_meters() { peak_l_ = peak_r_ = 0.0f; clipped_ = false; limiter_.reset_meter(); }
  int sounding_keys(bool* out) const {
    int n = 0;
    for (int k = 0; k < 128; ++k) {
      const bool on = section_.key_sounding(k);
      if (out) out[k] = on;
      n += on;
    }
    return n;
  }

  // The breath a controller gives, or negative when it comes from the keys. A controller that
  // has not been touched yet leaves the breath to the keys, so choosing one before the player
  // has picked up the breath controller does not silence the instrument.
  double controller_breath(const Settings& s) const {
    const int source = s.option(params::kBreathSource);
    if (source <= params::kFromVelocity || source >= params::kBreathSourceCount || !seen_[static_cast<size_t>(source)]) return -1.0;
    switch (source) {
      case params::kFromBreathCc: return max_of(breath_cc_);
      case params::kFromExpression: return max_of(expression_);
      case params::kFromModWheel: return max_of(wheel_);
      default: return max_of(pressure_);
    }
  }

  void configure(const Settings& s) {
    section_.configure(section_setup(s));
    const double c = controller_breath(s);
    if (c < 0.0) section_.set_controller(-1.0);
    else { controller_smooth_.target(c); section_.set_controller(controller_smooth_.value()); }
    section_.set_bend(max_abs(bend_));
    // The wheel adds vibrato unless it is the breath.
    const bool wheel_is_breath = s.option(params::kBreathSource) == params::kFromModWheel;
    section_.set_wheel_vibrato(wheel_is_breath ? 0.0 : 30.0 * max_of(wheel_));
  }

  // --- rendering ---
  void render(const Settings& s, float* left, float* right, uint32_t frames) {
    configure(s);
    master_.target(dsp::db_to_gain(s[params::kMasterDb]));
    room_.configure(room_settings(s));
    const float trim = static_cast<float>(levels::gain(s.instrument()));
    const bool controlled = controller_breath(s) >= 0.0;
    for (uint32_t i = 0; i < frames; ++i) {
      if (controlled) { controller_smooth_.next(); if ((i & 15) == 0) section_.set_controller(controller_smooth_.value()); }
      float l = 0.0f, r = 0.0f;
      section_.render(l, r);
      l *= trim; r *= trim;
      room_.process(l, r);
      const float gain = static_cast<float>(master_.next());
      l *= gain; r *= gain;
      if (!std::isfinite(l)) l = 0.0f;
      if (!std::isfinite(r)) r = 0.0f;
      const float limit = limiter_.gain(l, r);
      l *= limit; r *= limit;
      if (limiter_.held_db() > Limiter::kClipDb) clipped_ = true;
      peak_l_ = std::max(peak_l_, std::abs(l)); peak_r_ = std::max(peak_r_, std::abs(r));
      left[i] = l; right[i] = r;
    }
  }

  // How long after the last note the output can still be sounding, in samples.
  uint32_t tail_samples(const Settings& s) const {
    double seconds = 0.3 + 3.0 * s[params::kRelease] * 0.001 + 0.1;
    if (s.option(params::kRoomSize) >= 1) {
      dsp::air::Room probe;
      probe.prepare(1000.0);
      probe.configure(room_settings(s));
      seconds += probe.rt60_low() + 0.6;
    }
    seconds += s[params::kMicDistance] / 300.0 + s[params::kTiming] * 0.001;
    return static_cast<uint32_t>(std::min(seconds * rate_, 2.0e9));
  }

  static dsp::winds::Setup section_setup(const Settings& s) {
    dsp::winds::Setup w;
    w.instrument = s.instrument();
    w.a4 = s[params::kPitchA4];
    w.temperament = s.option(params::kTemperament);
    w.root = s.option(params::kTemperamentRoot);
    w.transpose = s.option(params::kTranspose);
    w.fine_cents = s[params::kFineTune];
    w.solo = s.option(params::kPlayMode) == 0;
    w.glide_ms = s[params::kGlide];
    w.bend_range = s[params::kBendRange];
    w.players = s.option(params::kPlayers);
    w.spread_cents = s[params::kSpread];
    w.timing_ms = s[params::kTiming];
    w.breath = s[params::kBreath];
    w.dynamics = s[params::kDynamics];
    w.attack_ms = s[params::kAttack];
    w.release_ms = s[params::kRelease];
    w.tongue = s[params::kTongue];
    w.breath_noise = s[params::kBreathNoise];
    w.growl = s[params::kGrowl];
    w.reed = s[params::kReed];
    w.embouchure = s[params::kEmbouchure];
    w.lip_cents = s[params::kLipTension];
    w.lipping = s[params::kLipping];
    w.hand = s[params::kHand];
    w.mute = s.option(params::kMute);
    w.key_noise = s[params::kKeyNoise];
    w.throat = s[params::kThroat];
    w.drones = s.on(params::kDrones);
    w.vibrato_cents = s[params::kVibrato];
    w.vibrato_hz = s[params::kVibratoRate];
    w.vibrato_delay_ms = s[params::kVibratoDelay];
    w.width = s[params::kStereoWidth];
    w.pan = s[params::kPan];
    return w;
  }
  static dsp::air::Room::Settings room_settings(const Settings& s) {
    dsp::air::Room::Settings r;
    r.volume_m3 = params::kRoomVolumes[static_cast<size_t>(std::clamp(s.option(params::kRoomSize), 0, params::kRoomSizeCount - 1))];
    r.absorption = s[params::kRoomAbsorption];
    r.distance_m = s[params::kMicDistance];
    r.humidity = s[params::kHumidity];
    r.celsius = s[params::kTemperature];
    r.pattern = s.option(params::kMicPattern);
    r.mono = s[params::kMicSetup] >= 0.5;
    return r;
  }

 private:
  static size_t ch(int16_t channel) { return static_cast<size_t>(std::clamp<int>(channel, 0, 15)); }
  static double max_of(const std::array<double, 16>& a) { double m = 0.0; for (double v : a) m = std::max(m, v); return m; }
  static double max_abs(const std::array<double, 16>& a) { double m = 0.0; for (double v : a) if (std::abs(v) > std::abs(m)) m = v; return m; }

  double rate_ = 48000.0, tempo_ = 120.0;
  dsp::winds::Section section_;
  dsp::air::Room room_;
  Limiter limiter_;
  dsp::Smoother master_, controller_smooth_;
  std::array<double, 16> bend_{}, wheel_{}, pressure_{}, breath_cc_{}, expression_{};
  std::array<bool, params::kBreathSourceCount> seen_{};
  uint64_t note_counter_ = 0;
  float peak_l_ = 0.0f, peak_r_ = 0.0f;
  bool clipped_ = false;
};

}  // namespace honkr
