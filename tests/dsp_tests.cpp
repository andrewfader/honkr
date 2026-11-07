// Executable scenarios for features/dsp_building_blocks.feature: the temperaments, the limiter,
// the body and mute filters and the delay line, measured in cents, dB and samples.
#include "../src/dsp/core.hpp"
#include "../src/dsp/filters.hpp"
#include "../src/dsp/temperament.hpp"
#include "../src/engine.hpp"
#include "audio_measure.hpp"
#include "spec.hpp"

#include <vector>

using namespace honkr::dsp;

SCENARIO(temperaments_lay_out_the_octave_as_the_treatises_give) {
  GIVEN("the six temperaments laid out from C with A at 440 Hz");
  THEN("equal temperament puts every note on its equal-tempered pitch");
  bool equal = true;
  for (int n = 36; n <= 96; ++n) equal = equal && std::abs(temperament::hz(temperament::kEqual, 0, 440.0, n) - midi_to_hz(n)) < 1e-6;
  CHECK(equal, "equal");
  THEN("Pythagorean fifths are pure (702 cents) and its major third wide (408)");
  const auto cents = [](int kind, int root, int lo, int hi) { return 1200.0 * std::log2(temperament::hz(kind, root, 440.0, hi) / temperament::hz(kind, root, 440.0, lo)); };
  CHECK_NEAR(cents(temperament::kPythagorean, 0, 60, 67), 701.955, 0.1, "pure fifth");
  CHECK_NEAR(cents(temperament::kPythagorean, 0, 60, 64), 407.82, 0.1, "Pythagorean third");
  THEN("quarter-comma meantone's major third C-E is pure (386 cents) and its wolf G#-Eb howls (737)");
  CHECK_NEAR(cents(temperament::kMeantone, 0, 60, 64), 386.31, 0.1, "pure third");
  CHECK_NEAR(cents(temperament::kMeantone, 0, 68, 75), 737.6, 0.5, "the wolf");
  THEN("Werckmeister III keeps C-E at 390 cents and no fifth narrower than 696");
  CHECK_NEAR(cents(temperament::kWerckmeister, 0, 60, 64), 390.2, 0.3, "Werckmeister third");
  double narrowest = 800.0;
  for (int n = 60; n < 72; ++n) narrowest = std::min(narrowest, cents(temperament::kWerckmeister, 0, n, n + 7));
  CHECK(narrowest > 695.5, spec::fmt("narrowest fifth %.1f cents", narrowest));
  THEN("in every temperament and key, A4 stays on the fork");
  bool a_stays = true;
  for (int kind = 0; kind < temperament::kCount; ++kind) for (int root = 0; root < 12; ++root) a_stays = a_stays && std::abs(temperament::hz(kind, root, 440.0, 69) - 440.0) < 1e-6;
  CHECK(a_stays, "A4 at 440");
  WHEN("meantone is laid out from D instead of C");
  THEN("D-F# is the pure third and the wolf moves");
  CHECK_NEAR(cents(temperament::kMeantone, 2, 62, 66), 386.31, 0.1, "pure third from D");
  CHECK(std::abs(cents(temperament::kMeantone, 2, 68, 75) - 737.6) > 10.0, "the wolf is elsewhere");
}

SCENARIO(the_limiter_holds_the_ceiling_and_recovers) {
  GIVEN("the output limiter fed a sine 6 dB over the ceiling, then one 6 dB under");
  honkr::Limiter lim;
  lim.prepare(48000.0);
  std::vector<float> hot(24000), cool(24000);
  for (size_t i = 0; i < hot.size(); ++i) { const float x = static_cast<float>(2.0 * honkr::Limiter::kCeiling * std::sin(kTau * 440.0 * static_cast<double>(i) / 48000.0)); hot[i] = x * lim.gain(x, x); }
  const double held = lim.held_db();
  for (size_t i = 0; i < cool.size(); ++i) { const float x = static_cast<float>(0.5 * honkr::Limiter::kCeiling * std::sin(kTau * 440.0 * static_cast<double>(i) / 48000.0)); cool[i] = x * lim.gain(x, x); }
  THEN("nothing leaves above the ceiling, the meter reads the 6 dB it took, and the quiet signal passes untouched once the gain has recovered");
  CHECK(measure::peak(measure::Span(hot)) <= honkr::Limiter::kCeiling + 1e-4, "ceiling held");
  CHECK_NEAR(held, 6.0, 0.3, "reduction metered");
  CHECK_NEAR(measure::db(measure::peak(measure::Span(cool, 12000))), measure::db(0.5 * honkr::Limiter::kCeiling), 0.1, "quiet signal untouched");
}

namespace {
// The steady amplitude, in dB against the input, of a sine at `hz` through `filter`.
template <typename F>
double sine_gain_db(F& filter, double hz) {
  std::vector<float> out(48000);
  for (size_t i = 0; i < out.size(); ++i) out[i] = filter.process(static_cast<float>(0.25 * std::sin(kTau * hz * static_cast<double>(i) / 48000.0)));
  return measure::db(measure::tone_amplitude(measure::Span(out, 24000, 16384), 48000.0, hz) / 0.25);
}
}  // namespace

SCENARIO(the_body_and_mute_filters_shape_the_spectrum_as_designed) {
  GIVEN("an RBJ peaking filter of +8 dB at 1800 Hz, Q 1.5 (a straight mute's nasal peak)");
  THEN("a sine at the centre gains 8 dB and one far below it passes untouched");
  Biquad peak;
  peak.set(Biquad::Type::Peak, 1800.0, 1.5, 8.0, 48000.0);
  CHECK_NEAR(sine_gain_db(peak, 1800.0), 8.0, 0.1, "gain at the centre, dB");
  peak.reset();
  CHECK_NEAR(sine_gain_db(peak, 60.0), 0.0, 0.1, "gain two decades below, dB");
  GIVEN("a Butterworth low-pass (Q 0.707) at 700 Hz (a hand closing the bell)");
  THEN("it is 3 dB down at its corner, flat an octave and more below, and falls at least 12 dB per octave above, as the bilinear Butterworth gives");
  Biquad low;
  low.set(Biquad::Type::LowPass, 700.0, 0.7071, 0.0, 48000.0);
  CHECK_NEAR(sine_gain_db(low, 700.0), -3.01, 0.1, "corner, dB");
  low.reset();
  CHECK_NEAR(sine_gain_db(low, 100.0), 0.0, 0.05, "passband, dB");
  low.reset();
  const double at_4 = sine_gain_db(low, 2800.0);
  low.reset();
  const double at_8 = sine_gain_db(low, 5600.0);
  // The bilinear transform's Butterworth: |H|^2 = 1 / (1 + W^4) at the prewarped W = tan(pi f / fs)
  // / tan(pi fc / fs), which tends to 12 dB per octave and steepens toward Nyquist.
  const auto warped_db = [](double hz) { const double w = std::tan(kPi * hz / 48000.0) / std::tan(kPi * 700.0 / 48000.0); return -10.0 * std::log10(1.0 + w * w * w * w); };
  CHECK_NEAR(at_4, warped_db(2800.0), 0.1, "two octaves above, dB");
  CHECK_NEAR(at_4 - at_8, warped_db(2800.0) - warped_db(5600.0), 0.1, "the third octave's slope, dB");
  CHECK(at_4 - at_8 > 12.0, "at least 12 dB per octave");
}

SCENARIO(a_delay_line_delays_by_its_fractional_length) {
  GIVEN("a delay line holding an impulse, read 10.25 samples back");
  DelayLine line;
  line.prepare(64);
  std::vector<float> out;
  for (int n = 0; n < 32; ++n) { line.push(n == 0 ? 1.0f : 0.0f); out.push_back(line.read(10.25)); }
  THEN("the impulse comes out split between samples 10 and 11 as 0.75 and 0.25: its centre at 10.25");
  CHECK_NEAR(static_cast<double>(out[10]), 0.75, 1e-6, "sample 10");
  CHECK_NEAR(static_cast<double>(out[11]), 0.25, 1e-6, "sample 11");
  double sum = 0.0, moment = 0.0;
  for (size_t i = 0; i < out.size(); ++i) { sum += out[i]; moment += static_cast<double>(i) * out[i]; }
  CHECK_NEAR(sum, 1.0, 1e-6, "energy neither lost nor gained");
  CHECK_NEAR(moment / sum, 10.25, 1e-6, "centre of the delayed impulse, samples");
  THEN("a whole-sample read is exact, and a read past the capacity is held at the capacity");
  CHECK(line.read_int(31) == 1.0f && line.read_int(30) == 0.0f, "whole-sample read");
  CHECK(line.capacity() == 64 && line.read(1000.0) == line.read(64.0), "clamped to the capacity");
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
