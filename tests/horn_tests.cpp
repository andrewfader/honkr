// Executable scenarios for features/horn.feature: the physics of the sound-producing loop. A
// reed on a cylinder sounds odd harmonics and speaks at about a third of its closing pressure; a
// cone sounds every harmonic; lips choose a partial of the harmonic series; a jet overblows; a
// stopped pipe and a vessel sound what their acoustics allow; valves pressed together come up
// short; and every instrument, calibrated, plays in tune across its compass.
#include "../src/dsp/horn.hpp"
#include "../src/dsp/winds.hpp"
#include "audio_measure.hpp"
#include "spec.hpp"

#include <vector>

using namespace honkr::dsp;
using namespace honkr::dsp::winds;
using measure::Span;

namespace {
constexpr double kRate = 48000.0;

// One horn alone at a pitch, blown at a steady pressure; the settled second half of it.
struct Played { std::vector<float> x; Span settled() const { return Span(x, x.size() / 2, x.size() / 2); } };
Played blow(const horn::Acoustics& a, double bore, double jet, double pressure, double seconds = 0.6, double lip_hz = 0.0, double lip_q = 40.0, double vessel_hz = 0.0) {
  horn::Horn h;
  h.prepare(kRate, 0.09);
  h.set_acoustics(a);
  if (lip_hz > 0.0) h.set_valve(lip_hz, lip_q);
  if (vessel_hz > 0.0) h.set_vessel(vessel_hz);
  horn::Tuning t; t.bore = bore; t.jet = jet;
  Rng rng(11);
  Played out;
  out.x.resize(static_cast<size_t>(seconds * kRate));
  for (size_t i = 0; i < out.x.size(); ++i) {
    const double env = std::min(1.0, static_cast<double>(i) / (0.01 * kRate));
    out.x[i] = h.tick(t, static_cast<float>(pressure * env), 0.002f * rng.bipolar());
  }
  return out;
}
const Instrument& named(const char* name) {
  for (const auto& in : instruments()) if (std::string(in.name) == name) return in;
  return instruments()[0];
}
}  // namespace

SCENARIO(a_reed_on_a_cylinder_sounds_odd_harmonics) {
  GIVEN("the clarinet's reed on its bore's resonances (a cylinder's: the even ones all but absent), blown at mezzo-forte for 233 Hz");
  const auto& cl = named("Clarinet");
  const Played p = blow(cl.acoustics, nominal_bore(cl, kRate, 233.0, 1), 0.0, 0.7);
  const double f0 = measure::pitch_hz(p.settled(), kRate, 100.0, 600.0);
  const auto h = measure::harmonics(p.settled(), kRate, f0, 5);
  THEN("it sounds the note, and the even harmonics sit well below their odd neighbours: the closed end's quarter-wave modes");
  const double h1 = measure::db(h[0]), h2 = measure::db(h[1]), h3 = measure::db(h[2]), h4 = measure::db(h[3]), h5 = measure::db(h[4]);
  CHECK_NEAR(measure::cents(f0, 233.0), 0.0, 60.0, spec::fmt("pitch %.1f Hz", f0));
  CHECK(h2 < h1 - 15.0 && h2 < h3 - 8.0, spec::fmt("2nd harmonic %.1f dB under the 1st, %.1f under the 3rd", h1 - h2, h3 - h2));
  CHECK(h4 < h3 - 8.0 && h4 < h5 - 3.0, spec::fmt("4th harmonic %.1f dB under the 3rd, %.1f under the 5th", h3 - h4, h5 - h4));
}

SCENARIO(the_reed_speaks_above_its_threshold) {
  GIVEN("the clarinet's reed, blown at rising fractions of the pressure that shuts it");
  const auto& cl = named("Clarinet");
  const auto level = [&](double gamma) { return measure::db(measure::rms(blow(cl.acoustics, nominal_bore(cl, kRate, 233.0, 1), 0.0, gamma).settled())); };
  const double lossless = level(0.36), above = level(0.6), forte = level(0.9);
  THEN("it does not speak at a third of it (the lossless threshold, raised by the bore's losses); above a half it does, louder as the breath rises");
  CHECK(lossless < -60.0, spec::fmt("at 0.36 of the closing pressure: %.1f dB", lossless));
  CHECK(above > -30.0, spec::fmt("at 0.6: %.1f dB", above));
  CHECK(forte > above, spec::fmt("louder at 0.9 (%.1f dB) than at 0.6", forte));
}

SCENARIO(a_cone_sounds_every_harmonic) {
  GIVEN("the alto saxophone's cone and reed on a bore for 233 Hz");
  const auto& sx = named("Alto sax");
  const Played p = blow(sx.acoustics, nominal_bore(sx, kRate, 233.0, 1), 0.0, 0.8);
  const double f0 = measure::pitch_hz(p.settled(), kRate, 100.0, 600.0);
  const auto h = measure::harmonics(p.settled(), kRate, f0, 4);
  THEN("it sounds the bore's pitch, with its second harmonic strong (a cone's modes are a full harmonic series)");
  CHECK_NEAR(measure::cents(f0, 233.0), 0.0, 60.0, spec::fmt("pitch %.1f Hz", f0));
  CHECK(measure::db(h[1]) > measure::db(h[0]) - 12.0, spec::fmt("2nd harmonic %.1f dB against the 1st", measure::db(h[1]) - measure::db(h[0])));
}

SCENARIO(the_lips_choose_the_partial) {
  GIVEN("the trumpet's open bore (its harmonic series on 116.5 Hz), the lips set for the 2nd to the 6th partial");
  const auto& tp = named("Trumpet");
  const double f1 = 116.54;
  bool all = true;
  for (int n = 2; n <= 6; ++n) {
    const double hz = n * f1;
    const double bore = kRate / f1 * Calibration::analytic(tp, kRate, hz, n);
    const Played p = blow(tp.acoustics, bore, 0.0, 0.6, 0.6, hz * lip_ratio(n), lip_q(n, hz));
    const double sounded = measure::pitch_hz(p.settled(), kRate, f1 * 1.2, f1 * 9.0);
    const double off = sounded > 0.0 ? measure::cents(sounded, hz) : 9999.0;
    if (std::abs(off) > 30.0) { all = false; std::printf("    partial %d: %.1f Hz (%+.0f cents)\n", n, sounded, off); }
  }
  THEN("the horn sounds each partial of the one bore, within 30 cents");
  CHECK(all, "lips select partials 2..6");
}

SCENARIO(valves_pressed_together_come_up_short) {
  GIVEN("the valve tubing: each crook cut for its own interval alone");
  THEN("1+3 lowers the horn by less than five semitones (about 30 cents sharp) and 1+2+3 by less than six (about 50 sharp)");
  const double one_three = 1200.0 * std::log2(std::exp2(5.0 / 12.0) / valve_length(0b101));
  const double all_three = 1200.0 * std::log2(std::exp2(6.0 / 12.0) / valve_length(0b111));
  CHECK_NEAR(one_three, 30.0, 4.0, spec::fmt("1+3: %.1f cents sharp", one_three));
  CHECK_NEAR(all_three, 50.0, 6.0, spec::fmt("1+2+3: %.1f cents sharp", all_three));
  CHECK_NEAR(1200.0 * std::log2(valve_length(0b001)), 200.0, 0.01, "first valve alone: a tone");
  CHECK_NEAR(1200.0 * std::log2(valve_length(0b010)), 100.0, 0.01, "second valve alone: a semitone");
}

SCENARIO(a_jet_overblows_to_the_octave) {
  GIVEN("the flute's pipe for 523 Hz, the jet crossing the mouth in most of a period, then in half of one");
  const auto& fl = named("Flute");
  const double period = kRate / 523.25;
  const Played normal = blow(fl.acoustics, period, 0.93 * period, 0.95);
  const Played overblown = blow(fl.acoustics, period, 0.5 * period, 0.95);
  const double f_normal = measure::pitch_hz(normal.settled(), kRate, 200.0, 3000.0);
  const double f_over = measure::pitch_hz(overblown.settled(), kRate, 200.0, 3000.0);
  THEN("the long jet sounds the fundamental, the short one the pipe's second mode, an octave up");
  CHECK_NEAR(measure::cents(f_normal, 523.25), 0.0, 60.0, spec::fmt("fundamental %.1f Hz", f_normal));
  CHECK_NEAR(measure::cents(f_over, 523.25), 1200.0, 60.0, spec::fmt("overblown %.1f Hz", f_over));
}

SCENARIO(a_stopped_pipe_and_a_vessel_sound_what_their_acoustics_allow) {
  GIVEN("the pan flute's stopped pipe and the ocarina's vessel, both for 440 Hz");
  const auto& pan = named("Pan flute");
  const Played stopped = blow(pan.acoustics, 0.5 * kRate / 440.0, 0.93 * kRate / 440.0, 0.95);
  const double fs = measure::pitch_hz(stopped.settled(), kRate, 200.0, 2000.0);
  const auto hs = measure::harmonics(stopped.settled(), kRate, fs, 3);
  const auto& oc = named("Ocarina");
  const Played vessel = blow(oc.acoustics, 0.0, 0.5 * kRate / 440.0, 1.0, 0.6, 0.0, 40.0, 440.0);
  const double fv = measure::pitch_hz(vessel.settled(), kRate, 200.0, 2000.0);
  const auto hv = measure::harmonics(vessel.settled(), kRate, fv, 3);
  THEN("the stopped pipe's even harmonics are weaker than its odd ones; the vessel's one mode is nearly a pure sine");
  CHECK(measure::db(hs[1]) < measure::db(hs[2]) - 3.0, spec::fmt("stopped pipe: 2nd %.1f dB, 3rd %.1f dB", measure::db(hs[1]), measure::db(hs[2])));
  CHECK(measure::db(hv[1]) < measure::db(hv[0]) - 25.0 && measure::db(hv[2]) < measure::db(hv[0]) - 25.0,
        spec::fmt("vessel: 2nd %.1f dB, 3rd %.1f dB under the fundamental", measure::db(hv[0]) - measure::db(hv[1]), measure::db(hv[0]) - measure::db(hv[2])));
}

SCENARIO(loud_brass_blares) {
  GIVEN("the trumpet's lips on its 4th partial at pianissimo and at fortissimo, with the bore's wave steepening and without it");
  const auto& tp = named("Trumpet");
  const double hz = 4 * 116.54;
  const auto centroid = [&](const horn::Acoustics& a, double p) {
    return measure::spectral_centroid(blow(a, kRate / 116.54 * Calibration::analytic(tp, kRate, hz, 4), 0.0, p, 0.6, hz * lip_ratio(4), lip_q(4, hz)).settled(), kRate, 12);
  };
  horn::Acoustics linear = tp.acoustics; linear.steepening_us = 0.0;
  const double soft = centroid(tp.acoustics, 0.3), loud = centroid(tp.acoustics, 1.2);
  const double soft_linear = centroid(linear, 0.3), loud_linear = centroid(linear, 1.2);
  THEN("the loud note's spectrum reaches far higher, and most of that is the steepening: the pianissimo is untouched by it");
  CHECK(loud > 1.4 * soft, spec::fmt("centroid %.0f Hz pianissimo, %.0f Hz fortissimo", soft, loud));
  CHECK(loud_linear < 1.2 * soft_linear, spec::fmt("without steepening: %.0f Hz to %.0f Hz", soft_linear, loud_linear));
  CHECK_NEAR(soft, soft_linear, 0.08 * soft_linear, spec::fmt("pianissimo %.0f Hz with steepening, %.0f Hz without", soft, soft_linear));
}

SCENARIO(the_filters_delay_is_known_exactly) {
  GIVEN("the loop's two poles of bell low-pass and its DC blocker at a pitch");
  const auto& tp = named("Trumpet");
  THEN("their phase delay is positive and grows as the bell's cutoff falls");
  const double d1 = filter_delay(tp.acoustics, kRate, 300.0);
  horn::Acoustics low = tp.acoustics; low.bell_hz = 800.0;
  const double d2 = filter_delay(low, kRate, 300.0);
  CHECK(d1 > 0.0 && d2 > d1, spec::fmt("%.2f samples at the trumpet's bell, %.2f at 800 Hz", d1, d2));
}

SCENARIO(every_instrument_plays_in_tune_across_its_compass) {
  GIVEN("every instrument, calibrated, playing its lowest, middle and highest notes at mezzo-forte through the player");
  bool all = true;
  int notes = 0;
  for (int i = 0; i < kInstrumentCount; ++i) {
    const Instrument& in = instruments()[static_cast<size_t>(i)];
    for (int key : {in.low + 1, (in.low + in.high) / 2, in.high - 1}) {
      Section s;
      s.prepare(kRate);
      Setup st; st.instrument = i; st.vibrato_cents = 0.0;
      s.configure(st);
      s.note_on(key, 0.6, 7);
      std::vector<float> x(static_cast<size_t>(0.7 * kRate));
      float l, r;
      for (auto& v : x) { s.render(l, r); v = 0.5f * (l + r); }
      const Fingered f = s.fingering_for(key);
      const Span settled(x, x.size() / 2, x.size() / 2);
      const double hz = measure::pitch_hz(settled, kRate, f.hz * 0.7, f.hz * 1.4);
      const double off = hz > 0.0 ? measure::cents(hz, f.hz) : 9999.0;
      // The lowest notes of the tuba and the flute family's dynamic sharpening are allowed a
      // little more, as their players allow it.
      const double allowed = (in.family == Family::Flute || key < 32) ? 35.0 : 25.0;
      ++notes;
      if (std::abs(off) > allowed) { all = false; std::printf("    %s key %d: %.1f Hz against %.1f (%+.0f cents)\n", in.name, key, hz, f.hz, off); }
    }
  }
  THEN("every note sounds within 25 cents of its pitch (35 for the flutes and the lowest brass)");
  CHECK(all, spec::fmt("%.0f notes in tune", notes));
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
