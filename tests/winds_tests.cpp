// Executable scenarios for features/winds.feature: how the instruments are fingered and how the
// players play them. Brass fingering follows the instrument's own arithmetic (valves, a slide
// with an F trigger, a double horn's thumb valve, a natural horn's partials); a bagpipe chanter
// has nine notes; a note beyond the compass sounds inside it; a solo player slurs; a section
// plays chords and doubles in unison; the breath can come from a controller; the pipes drone.
#include "../src/dsp/winds.hpp"
#include "audio_measure.hpp"
#include "spec.hpp"

#include <vector>

using namespace honkr::dsp;
using namespace honkr::dsp::winds;
using measure::Span;

namespace {
constexpr double kRate = 48000.0;
int index_of(const char* name) {
  for (int i = 0; i < kInstrumentCount; ++i) if (std::string(instruments()[static_cast<size_t>(i)].name) == name) return i;
  return 0;
}
HzOf equal() { return HzOf{}; }
Fingered finger_named(const char* name, int key, double lipping = 1.0) { return finger(instrument(index_of(name)), key, 440.0, lipping, equal()); }

struct Rig {
  Section s;
  Setup st;
  std::vector<float> left, right;
  explicit Rig(const char* name) { s.prepare(kRate); st.instrument = index_of(name); st.vibrato_cents = 0.0; s.configure(st); }
  void apply() { s.configure(st); }
  void run(double seconds) {
    const size_t n = static_cast<size_t>(seconds * kRate);
    float l, r;
    for (size_t i = 0; i < n; ++i) { s.render(l, r); left.push_back(l); right.push_back(r); }
  }
  std::vector<float> mono() const { std::vector<float> m(left.size()); for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (left[i] + right[i]); return m; }
  Span last(double seconds, const std::vector<float>& m) const { const size_t n = static_cast<size_t>(seconds * kRate); return Span(m, m.size() - n, n); }
};
}  // namespace

SCENARIO(the_trumpet_is_fingered_as_its_chart) {
  GIVEN("the B-flat trumpet, its notes at concert pitch");
  struct Row { int key; int partial; int mask; const char* what; };
  // Written pitch is a tone above: concert C4 is written D4, fingered 1+3, and so on.
  const Row chart[] = {{52, 2, 0b111, "concert E3: 1+2+3 on the 2nd partial"}, {58, 2, 0, "Bb3: open, 2nd"}, {60, 3, 0b101, "C4: 1+3, 3rd"},
                       {62, 3, 0b011, "D4: 1+2, 3rd"}, {65, 3, 0, "F4: open, 3rd"}, {67, 4, 0b011, "G4: 1+2, 4th"}, {69, 4, 0b010, "A4: 2, 4th"},
                       {70, 4, 0, "Bb4: open, 4th"}, {74, 5, 0, "D5: open, 5th"}, {77, 6, 0, "F5: open, 6th"}, {82, 8, 0, "Bb5: open, 8th (not the 7th)"}};
  THEN("each note takes the valves and partial a trumpeter uses");
  for (const Row& r : chart) {
    const Fingered f = finger_named("Trumpet", r.key);
    CHECK(f.partial == r.partial && f.valve_mask == r.mask, spec::fmt(r.what, 0) + spec::fmt(" (got partial %.0f, valves %.0f)", f.partial, f.valve_mask));
  }
}

SCENARIO(the_horn_speaks_in_its_own_intonation_unless_the_player_lips_it) {
  GIVEN("the trumpet's C4 (1+3) and D5 (the open 5th partial), with lipping at none and at full");
  const Fingered c_raw = finger_named("Trumpet", 60, 0.0), d_raw = finger_named("Trumpet", 74, 0.0);
  const Fingered c_lipped = finger_named("Trumpet", 60, 1.0), d_lipped = finger_named("Trumpet", 74, 1.0);
  THEN("unlipped, 1+3 is about 30 cents sharp and the 5th partial 14 flat; lipped, both are tempered");
  CHECK_NEAR(measure::cents(c_raw.hz, 261.626), 30.0, 5.0, spec::fmt("C4 on 1+3: %+.1f cents", measure::cents(c_raw.hz, 261.626)));
  CHECK_NEAR(measure::cents(d_raw.hz, 587.330), -13.7, 1.0, spec::fmt("D5 on the 5th partial: %+.1f cents", measure::cents(d_raw.hz, 587.330)));
  CHECK_NEAR(measure::cents(c_lipped.hz, 261.626), 0.0, 0.01, "C4 lipped");
  CHECK_NEAR(measure::cents(d_lipped.hz, 587.330), 0.0, 0.01, "D5 lipped");
}

SCENARIO(the_trombone_slide_and_its_trigger) {
  GIVEN("the tenor trombone");
  const Fingered bb2 = finger_named("Trombone", 46), e2 = finger_named("Trombone", 40), f3 = finger_named("Trombone", 53), c2 = finger_named("Trombone", 36);
  THEN("Bb2 is first position, E2 seventh, F3 first on the 3rd partial, and C2, in the gap below E2, needs the F trigger");
  CHECK(bb2.partial == 2 && bb2.drop == 0, spec::fmt("Bb2: partial %.0f, position %.0f", bb2.partial, bb2.drop + 1));
  CHECK(e2.partial == 2 && e2.drop == 6, spec::fmt("E2: partial %.0f, position %.0f", e2.partial, e2.drop + 1));
  CHECK(f3.partial == 3 && f3.drop == 0, spec::fmt("F3: partial %.0f, position %.0f", f3.partial, f3.drop + 1));
  CHECK(c2.partial == 2 && c2.drop > 6, spec::fmt("C2: partial %.0f, %.0f semitones down (trigger)", c2.partial, c2.drop));
  CHECK_NEAR(measure::cents(finger_named("Trombone", 40, 0.0).natural_hz, 82.407), 0.0, 1.0, "the slide's positions are in tune");
}

SCENARIO(the_double_horn_switches_to_its_b_flat_side) {
  GIVEN("the double horn, below and above the thumb valve's switch point");
  const Fingered low = finger_named("French horn", 55), high = finger_named("French horn", 70);
  THEN("the F side plays the lower note on a high partial of F; the Bb side the upper one on a partial of Bb");
  CHECK_NEAR(low.natural_hz / low.partial * std::exp2(low.drop / 12.0), 43.65, 1.5, spec::fmt("G3 on F: partial %.0f", low.partial));
  CHECK_NEAR(high.natural_hz / high.partial * std::exp2(high.drop / 12.0), 58.27, 2.0, spec::fmt("Bb4 on Bb: partial %.0f", high.partial));
}

SCENARIO(a_natural_horn_plays_only_its_partials) {
  GIVEN("the bugle in G and the alphorn in F#, played on every key, unlipped");
  THEN("every key sounds a partial of the open horn; the alphorn's 11th falls between B and C, a quarter-tone over its scale's fourth, the famous 'alphorn fa'");
  bool all = true;
  for (int key = 55; key <= 79; ++key) {
    const Fingered f = finger_named("Bugle", key, 0.0);
    const double n = f.hz / 98.0;
    if (std::abs(n - std::round(n)) > 1e-6) all = false;
  }
  CHECK(all, "bugle: whole partials only");
  const Fingered fa = finger_named("Alphorn", 84, 0.0);
  const double c6 = 440.0 * std::exp2((84 - 69) / 12.0);
  CHECK(fa.partial == 11, spec::fmt("key 84 plays the %.0fth partial", fa.partial));
  CHECK_NEAR(measure::cents(fa.hz, c6), -49.0, 3.0, spec::fmt("the 11th sits %+.1f cents from C6", measure::cents(fa.hz, c6)));
}

SCENARIO(the_chanter_has_nine_notes) {
  GIVEN("the Highland chanter, played on every key from G4 to A5");
  std::vector<int> heard;
  for (int key = 67; key <= 81; ++key) {
    const int k = finger_named("Highland bagpipe", key).key;
    if (std::find(heard.begin(), heard.end(), k) == heard.end()) heard.push_back(k);
  }
  THEN("only its nine notes sound: low G, low A, B, C#, D, E, F#, high G, high A");
  CHECK(heard.size() == 9, spec::fmt("%.0f distinct notes", static_cast<double>(heard.size())));
  CHECK(finger_named("Highland bagpipe", 72).key == 73 || finger_named("Highland bagpipe", 72).key == 71, "C natural is not on the chanter");
}

SCENARIO(a_note_beyond_the_compass_sounds_inside_it) {
  GIVEN("the flute (C4 to A6) asked for C2 and C8, the tuba (D1 to F4) for C6");
  Rig flute("Flute"), tuba("Tuba");
  THEN("each sounds an octave (or more) inside its compass");
  CHECK(flute.s.fingering_for(36).key == 60, spec::fmt("C2 on the flute plays key %.0f", flute.s.fingering_for(36).key));
  CHECK(flute.s.fingering_for(108).key == 84, spec::fmt("C8 on the flute plays key %.0f", flute.s.fingering_for(108).key));
  CHECK(tuba.s.fingering_for(84).key == 60, spec::fmt("C6 on the tuba plays key %.0f", tuba.s.fingering_for(84).key));
}

SCENARIO(a_solo_player_slurs_without_tonguing_again) {
  GIVEN("a solo alto sax holding C4, then G4 pressed with C4 still held, then G4 let go");
  Rig rig("Alto sax");
  rig.st.tongue = 1.0; rig.apply();
  rig.s.note_on(60, 0.8, 1); rig.run(0.5);
  rig.s.note_on(67, 0.8, 2); rig.run(0.5);
  const auto m1 = rig.mono();
  const size_t change = static_cast<size_t>(0.5 * kRate);
  rig.s.note_off(67); rig.run(0.5);
  const auto m2 = rig.mono();
  THEN("one player plays throughout: the slur keeps sounding (no gap) and lands on G4, and letting G4 go returns to C4");
  CHECK(rig.s.sounding() == 1, spec::fmt("%.0f player(s) sounding", rig.s.sounding()));
  const auto contour = measure::contour_db(std::vector<float>(m1.begin() + static_cast<long>(change - 4800), m1.begin() + static_cast<long>(change + 9600)), kRate, 0.002, 0.004);
  double lowest = 0.0; for (double c : contour) lowest = std::min(lowest, c);
  CHECK(lowest > -40.0, spec::fmt("quietest moment of the slur %.1f dBFS", lowest));
  CHECK_NEAR(measure::cents(measure::pitch_hz(Span(m1, m1.size() - 9600, 9600), kRate, 150, 600), 392.0), 0.0, 30.0, "on G4 after the slur");
  CHECK_NEAR(measure::cents(measure::pitch_hz(Span(m2, m2.size() - 9600, 9600), kRate, 120, 600), 261.63), 0.0, 30.0, "back on C4");
}

SCENARIO(a_section_plays_chords_and_doubles_in_unison) {
  GIVEN("the trumpet in Section mode with C4 and G4 held, then in Solo mode with three players on C4");
  Rig chord("Trumpet");
  chord.st.solo = false; chord.apply();
  chord.s.note_on(60, 0.8, 1); chord.s.note_on(67, 0.8, 2); chord.run(0.8);
  const auto m = chord.mono();
  const Span held(m, m.size() - 19200, 19200);
  Rig unison("Trumpet");
  unison.st.players = 3; unison.st.spread_cents = 10.0; unison.apply();
  unison.s.note_on(60, 0.8, 1); unison.run(0.8);
  THEN("both notes of the chord sound; three players sound the unison");
  CHECK(measure::db(measure::tone_amplitude(held, kRate, 261.63)) > -50.0 && measure::db(measure::tone_amplitude(held, kRate, 392.0)) > -50.0,
        spec::fmt("C4 %.1f dB, G4 %.1f dB", measure::db(measure::tone_amplitude(held, kRate, 261.63)), measure::db(measure::tone_amplitude(held, kRate, 392.0))));
  CHECK(chord.s.sounding() == 2, spec::fmt("%.0f players for the chord", chord.s.sounding()));
  CHECK(unison.s.sounding() == 3, spec::fmt("%.0f players in unison", unison.s.sounding()));
}

SCENARIO(the_breath_can_come_from_a_controller) {
  GIVEN("a held clarinet note whose breath comes from a controller, at none, then at full");
  Rig rig("Clarinet");
  rig.s.set_controller(0.0);
  rig.s.note_on(60, 1.0, 1); rig.run(0.4);
  const auto quiet = rig.mono();
  rig.s.set_controller(1.0); rig.run(0.6);
  const auto loud = rig.mono();
  THEN("with no breath the held key is silent; with full breath it sounds");
  CHECK(measure::db(measure::rms(Span(quiet, quiet.size() - 9600, 9600))) < -60.0, "no breath, no note");
  CHECK(measure::db(measure::rms(Span(loud, loud.size() - 9600, 9600))) > -30.0, "breath brings it in");
}

SCENARIO(the_pipes_drone_while_the_bag_is_squeezed) {
  GIVEN("the Highland pipes playing E5 with the drones on, then off");
  const auto play = [](bool drones) {
    Rig rig("Highland bagpipe");
    rig.st.drones = drones; rig.apply();
    rig.s.note_on(76, 0.8, 1); rig.run(1.2);
    const auto m = rig.mono();
    return Span(m, m.size() - 24000, 24000).size() ? measure::db(measure::tone_amplitude(Span(m, m.size() - 24000, 24000), kRate, 110.0)) : -200.0;
  };
  const double on = play(true), off = play(false);
  THEN("the bass drone's A2 sounds under the chanter only when the drones are on");
  CHECK(on > off + 20.0, spec::fmt("A2 at %.1f dB with the drones, %.1f dB without", on, off));
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
