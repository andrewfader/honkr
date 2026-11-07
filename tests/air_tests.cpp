// Executable scenarios for features/air.feature: the room and its air against the physics
// (the speed of sound, ISO 9613-1 absorption, Sabine's time), and a real instrument heard
// through that air (a trumpet across a field).
#include "../src/dsp/air.hpp"
#include "../src/engine.hpp"
#include "audio_measure.hpp"
#include "spec.hpp"

#include <vector>

using namespace honkr::dsp;
using measure::Span;

namespace {
constexpr double kRate = 48000.0;

std::vector<float> click_through(air::Room& room, size_t length, double hz = 0.0) {
  std::vector<float> out(length);
  for (size_t i = 0; i < length; ++i) {
    float l = hz > 0.0 ? static_cast<float>(0.5 * std::sin(kTau * hz * static_cast<double>(i) / kRate)) : (i == 100 ? 1.0f : 0.0f), r = l;
    room.process(l, r);
    out[i] = l;
  }
  return out;
}
}  // namespace

SCENARIO(sound_reaches_the_microphone_at_the_speed_of_sound) {
  GIVEN("a microphone 5 m from the source in the open, in air at 0 C and at 30 C");
  for (double celsius : {0.0, 30.0}) {
    air::Room room;
    room.prepare(kRate);
    air::Room::Settings s; s.distance_m = 5.0; s.celsius = celsius;
    room.configure(s);
    const auto out = click_through(room, 24000);
    size_t at = 0;
    for (size_t i = 0; i < out.size(); ++i) if (std::abs(out[i]) > 0.3f) { at = i; break; }
    THEN("the click arrives 5 / (331.3 sqrt(1 + T / 273.15)) s later, within a sample");
    CHECK_NEAR(static_cast<double>(at - 100), 5.0 / air::speed_of_sound(celsius) * kRate, 1.0, ::spec::fmt("at %.0f C", celsius));
  }
}

SCENARIO(the_air_takes_the_top_end_off_as_iso_9613_gives) {
  GIVEN("a microphone 10 m away in the open, in air at 20 C and 20% humidity");
  air::Room room;
  room.prepare(kRate);
  air::Room::Settings s; s.distance_m = 10.0; s.celsius = 20.0; s.humidity = 0.2;
  room.configure(s);
  THEN("an 8 kHz sine loses what ISO 9613-1 gives over 10 m within 1 dB, and humid air takes less");
  const auto loss = [&](double hz, double humidity) {
    air::Room r2; r2.prepare(kRate);
    air::Room::Settings s2 = s; s2.humidity = humidity; r2.configure(s2);
    const auto out = click_through(r2, 24000, hz);
    return -measure::db(measure::tone_amplitude(Span(out, 8000, 8192), kRate, hz) / 0.5);
  };
  CHECK_NEAR(loss(8000.0, 0.2), 10.0 * air::absorption_db_per_m(8000.0, 20.0, 0.2), 1.0, "8 kHz over 10 m");
  CHECK(loss(8000.0, 0.7) < loss(8000.0, 0.2), "humid air takes less");
}

SCENARIO(a_room_rings_for_its_sabine_time) {
  GIVEN("a 3000 cubic-metre hall with walls of absorption 0.2, the microphone 3 m away");
  air::Room room;
  room.prepare(kRate);
  air::Room::Settings s; s.volume_m3 = 3000.0; s.absorption = 0.2; s.distance_m = 3.0;
  room.configure(s);
  const auto out = click_through(room, static_cast<size_t>(kRate * 4.0));
  THEN("its impulse response in the 500 Hz octave decays within 25% of Sabine's time");
  const auto level = [&](double from_s) {
    double e = 0.0;
    const Span span(out, static_cast<size_t>(from_s * kRate), static_cast<size_t>(0.3 * kRate));
    for (size_t i = 0; i < span.size(); ++i) e += span[i] * span[i];
    return 10.0 * std::log10(std::max(e, 1e-20));
  };
  const double slope = (level(0.5) - level(1.5)) / 1.0;  // dB per second
  CHECK_NEAR(60.0 / slope, room.rt60_low(), 0.25 * room.rt60_low(), ::spec::fmt("T60 %.2f s against Sabine's %.2f", 60.0 / slope, room.rt60_low()));
}

namespace {
// A held trumpet C5 without vibrato or breath noise, heard by an omnidirectional microphone in the
// open at `distance` metres, in air at 20 C and `humidity`; the left channel after it has settled.
std::vector<float> trumpet_heard_at(double distance, double humidity) {
  honkr::Engine engine;
  engine.prepare(kRate);
  honkr::Settings s = honkr::Settings::defaults();
  namespace P = honkr::params;
  s.v[P::kInstrument] = P::kTrumpet;
  s.v[P::kVibrato] = 0.0;
  s.v[P::kBreathNoise] = 0.0;
  s.v[P::kKeyNoise] = 0.0;
  s.v[P::kMicDistance] = distance;
  s.v[P::kHumidity] = humidity;
  s.v[P::kTemperature] = 20.0;
  engine.note_on(s, 0, 72, -1, 0.9);
  std::vector<float> left(static_cast<size_t>(kRate * 1.2)), right(left.size());
  for (size_t at = 0; at < left.size(); at += 256) engine.render(s, left.data() + at, right.data() + at, static_cast<uint32_t>(std::min<size_t>(256, left.size() - at)));
  return std::vector<float>(left.begin() + static_cast<long>(kRate * 0.6), left.end());
}
}  // namespace

SCENARIO(a_trumpet_across_a_field_loses_its_top_to_the_air) {
  GIVEN("a trumpet holding C5, heard by a microphone on the bell and by one 10 m away in the open, in air at 20 C and 20% humidity");
  const auto near = trumpet_heard_at(0.0, 0.2), far = trumpet_heard_at(10.0, 0.2), far_humid = trumpet_heard_at(10.0, 0.7);
  const Span near_span(near, 0, 16384), far_span(far, 0, 16384), humid_span(far_humid, 0, 16384);
  const double f0 = measure::peak_hz_near(near_span, kRate, 523.25, 60.0);
  // The harmonic nearest 8 kHz, where the air's loss is heard and fitted.
  const int k = static_cast<int>(std::lround(8000.0 / f0));
  const double fk = measure::peak_hz_near(near_span, kRate, k * f0, 20.0);
  const auto level = [&](const Span& span, double hz) { return measure::db(measure::tone_amplitude(span, kRate, hz)); };
  THEN("the fundamental arrives at the level it left (the preamp is set for the direct sound)");
  CHECK_NEAR(level(far_span, f0) - level(near_span, f0), 0.0, 0.5, "fundamental level across 10 m, dB");
  THEN("the harmonic near 8 kHz loses what ISO 9613-1 gives over 10 m, within 0.5 dB, and humid air takes less of it");
  const double loss = (level(near_span, fk) - level(near_span, f0)) - (level(far_span, fk) - level(far_span, f0));
  const double humid_loss = (level(near_span, fk) - level(near_span, f0)) - (level(humid_span, fk) - level(humid_span, f0));
  const double expected = 10.0 * (air::absorption_db_per_m(fk, 20.0, 0.2) - air::absorption_db_per_m(f0, 20.0, 0.2));
  std::printf("    harmonic %d at %.0f Hz: lost %.2f dB in dry air (ISO %.2f), %.2f dB in humid air\n", k, fk, loss, expected, humid_loss);
  CHECK(level(near_span, fk) - level(near_span, f0) > -60.0, "the trumpet has a harmonic near 8 kHz to lose");
  CHECK_NEAR(loss, expected, 0.5, ::spec::fmt("harmonic %.0f (%.0f Hz) over 10 m, dB", k, fk));
  CHECK(humid_loss < loss - 1.0, ::spec::fmt("humid air takes less: %.2f dB against %.2f", humid_loss, loss));
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
