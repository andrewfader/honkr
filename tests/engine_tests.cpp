// Executable scenarios for features/engine.feature: notes, breath controllers, the wheels, the
// instrument switch, every instrument at a comparable level, every preset against the quality
// bar, the limiter and the tail, through the engine's own interface (no CLAP yet).
#include "../src/engine.hpp"
#include "../src/presets.hpp"
#include "audio_measure.hpp"
#include "audit_phrase.hpp"
#include "spec.hpp"

#include <string>
#include <vector>

using namespace honkr;
using measure::Span;

namespace {
constexpr double kRate = 48000.0;
constexpr uint32_t kBlock = 256;

struct Rig {
  Engine engine;
  Settings s = Settings::defaults();
  std::vector<float> left, right;
  Rig() { engine.prepare(kRate); }
  void render(uint32_t frames) {
    const size_t at = left.size();
    left.resize(at + frames); right.resize(at + frames);
    for (uint32_t done = 0; done < frames; done += kBlock) {
      const uint32_t n = std::min(kBlock, frames - done);
      engine.render(s, left.data() + at + done, right.data() + at + done, n);
    }
  }
  std::vector<float> mono() const { std::vector<float> m(left.size()); for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (left[i] + right[i]); return m; }
  void on(int key, double vel = 0.7) { engine.note_on(s, 0, key, -1, vel); }
  void off(int key) { engine.note_off(s, 0, key, -1); }
};
bool finite(const std::vector<float>& x) { for (float v : x) if (!std::isfinite(v)) return false; return true; }
}  // namespace

SCENARIO(a_note_sounds_while_held_and_stops_on_release) {
  GIVEN("the default alto sax, one note held for half a second with a 70 ms release");
  Rig rig;
  rig.on(60);
  rig.render(24000);
  rig.off(60);
  rig.render(48000);
  THEN("the note sounds while held, stops on release, and the engine reports silence within a second");
  const auto m = rig.mono();
  CHECK(finite(m), "finite");
  CHECK(measure::rms(Span(m, 9600, 14400)) > 0.01, "audible while held");
  CHECK(measure::peak(Span(m, 24000 + 24000)) < 1e-3, "quiet after the release");
  CHECK(rig.engine.silent(), "silent");
}

SCENARIO(a_breath_controller_takes_over_once_it_moves) {
  GIVEN("the breath from CC 2, a note played before the controller has moved, then the controller at none and at full");
  Rig rig;
  rig.s.v[params::kBreathSource] = params::kFromBreathCc;
  rig.on(60, 0.8); rig.render(24000);
  const double before = measure::db(measure::rms(Span(rig.mono(), rig.left.size() - 9600)));
  rig.engine.breath(0, 0.0); rig.render(24000);
  const double none = measure::db(measure::rms(Span(rig.mono(), rig.left.size() - 9600)));
  rig.engine.breath(0, 1.0); rig.render(24000);
  const double full = measure::db(measure::rms(Span(rig.mono(), rig.left.size() - 9600)));
  THEN("the velocity blows the note until the controller moves; then no breath is silence and full breath is loud");
  CHECK(before > -40.0, spec::fmt("before the controller: %.1f dBFS", before));
  CHECK(none < -60.0, spec::fmt("controller at none: %.1f dBFS", none));
  CHECK(full > before - 6.0, spec::fmt("controller at full: %.1f dBFS", full));
}

SCENARIO(dynamics_scales_the_breath) {
  GIVEN("a soft and a hard note on the trumpet with Dynamics at full and at none");
  const auto level = [](double dynamics, double velocity) {
    Rig rig; rig.s.v[params::kInstrument] = params::kTrumpet; rig.s.v[params::kDynamics] = dynamics;
    rig.on(65, velocity); rig.render(36000);
    return measure::db(measure::rms(Span(rig.mono(), 14400, 19200)));
  };
  THEN("at full the two differ by more than 10 dB; at none they are the same mezzo-forte");
  CHECK(level(1.0, 1.0) - level(1.0, 0.1) > 10.0, spec::fmt("full range: %.1f dB", level(1.0, 1.0) - level(1.0, 0.1)));
  CHECK(std::abs(level(0.0, 1.0) - level(0.0, 0.1)) < 1.0, "everything mezzo-forte");
}

SCENARIO(switching_instruments_silences_the_old_one) {
  GIVEN("a held trumpet note, then the instrument switched to the flute");
  Rig rig;
  rig.s.v[params::kInstrument] = params::kTrumpet;
  rig.on(65); rig.render(9600);
  rig.s.v[params::kInstrument] = params::kFlute;
  rig.render(4800);
  THEN("the trumpet stops at once and the flute plays the next note");
  CHECK(measure::peak(Span(rig.mono(), rig.left.size() - 2400)) < 1e-3, "old instrument silenced");
  rig.on(72); rig.render(14400);
  CHECK(measure::rms(Span(rig.mono(), rig.left.size() - 4800)) > 1e-3, "new instrument sounds");
}

SCENARIO(the_wheel_adds_vibrato_and_the_bend_bends) {
  GIVEN("a held clarinet note without vibrato, then the mod wheel full up, then the bend wheel full up");
  Rig rig;
  rig.s.v[params::kInstrument] = params::kClarinet; rig.s.v[params::kVibrato] = 0.0; rig.s.v[params::kVibratoDelay] = 0.0;
  rig.on(60); rig.render(24000);
  rig.engine.mod_wheel(0, 1.0); rig.render(48000);
  const auto m = rig.mono();
  const auto track = measure::pitch_track(std::vector<float>(m.end() - 38400, m.end()), kRate, 0.005, 0.03, 150.0, 400.0);
  double lo = 1e9, hi = 0.0; for (double f : track) if (f > 0.0) { lo = std::min(lo, f); hi = std::max(hi, f); }
  rig.engine.mod_wheel(0, 0.0); rig.engine.pitch_bend(0, 1.0); rig.render(24000);
  const double bent = measure::pitch_hz(Span(rig.mono(), rig.left.size() - 9600), kRate, 150.0, 400.0);
  THEN("the wheel swings the pitch by tens of cents; the bend raises it by the bend range, two semitones");
  CHECK(1200.0 * std::log2(hi / lo) > 30.0, spec::fmt("vibrato swing %.0f cents", 1200.0 * std::log2(hi / lo)));
  CHECK_NEAR(measure::cents(bent, 261.63), 200.0, 25.0, spec::fmt("bent to %.1f Hz", bent));
}

SCENARIO(a_harder_breath_is_brighter) {
  GIVEN("the clarinet, the alto sax and the bassoon each playing one note at a pianissimo and at a fortissimo touch");
  bool all_ok = true;
  for (const char* name : {"Clarinet", "Alto sax", "Bassoon"}) {
    int inst = 0;
    for (int i = 0; i < params::kInstrumentCount; ++i) if (std::string(params::kInstrumentNames[i]) == name) inst = i;
    const auto centroid = [&](double vel) {
      Rig rig;
      rig.s.v[params::kInstrument] = inst; rig.s.v[params::kVibrato] = 0;
      rig.on(levels::reference_key(inst), vel); rig.render(57600);
      return measure::spectral_centroid(Span(rig.mono(), 19200, 38400), kRate, 12);
    };
    const double soft = centroid(0.2), loud = centroid(0.95);
    std::printf("    %s: centroid %.0f Hz soft, %.0f Hz loud\n", name, soft, loud);
    if (!(loud > 1.15 * soft)) all_ok = false;
  }
  THEN("each fortissimo's spectrum reaches well higher than its pianissimo");
  CHECK(all_ok, "brighter when blown harder");
}

SCENARIO(every_instrument_plays_one_note_at_a_comparable_level) {
  GIVEN("one note in the middle of every instrument's compass at a firm breath, master at 0 dB, 1.2 s of it");
  bool all_ok = true;
  for (int inst = 0; inst < params::kInstrumentCount; ++inst) {
    Rig rig;
    rig.s.v[params::kInstrument] = inst; rig.s.v[params::kMasterDb] = 0;
    rig.on(levels::reference_key(inst), 0.8); rig.render(57600);
    const auto m = rig.mono();
    // The level the trims aim at: -16 dBFS RMS once the note has spoken, with the peak under
    // the ceiling.
    const double held = measure::db(measure::rms(Span(m, 14400, 38400)));
    const double peak = measure::db(measure::peak(Span(m)));
    const bool ok = finite(m) && held >= -22.0 && held <= -10.0 && peak <= -2.0 + 0.1;
    if (!ok) { all_ok = false; std::printf("    %s: held %.1f dBFS, peak %.1f dBFS\n", params::kInstrumentNames[inst], held, peak); }
  }
  THEN("every one holds between -22 and -10 dBFS RMS and peaks under the ceiling");
  CHECK(all_ok, "instruments sit at one level");
}

// A brass section voices its low notes on trombones: a trumpet's bottom (E3) barely speaks, so
// "Brass section, chords" gives every key below its split to trombones.
SCENARIO(a_brass_section_voices_its_low_notes_on_trombones) {
  GIVEN("the 'Brass section, chords' preset playing E3 G#3 B3 E4 at a firm touch, and the same with the low instrument Off");
  size_t index = presets::count();
  for (size_t p = 0; p < presets::count(); ++p) if (std::string(presets::all()[p].name) == "Brass section, chords") index = p;
  CHECK(index < presets::count(), "the preset exists");
  if (index >= presets::count()) return;
  const int chord[] = {52, 56, 59, 64};
  // keys: the chord or one key of it. low: 1 the preset as it is, 0 the low instrument Off, -1
  // the trombones on their own (the section's instrument the trombone, no low instrument).
  const auto play = [&](std::vector<int> keys, int low, Rig& rig) {
    presets::apply(index, rig.s.v.data());
    if (low <= 0) rig.s.v[params::kLowInstrument] = 0;
    if (low < 0) rig.s.v[params::kInstrument] = params::kTrombone;
    for (int k : keys) rig.on(k, 0.75);
    rig.render(57600);
  };
  // The held level, 0.3 s to 1.0 s in, in dBFS RMS.
  const auto held_db = [](const std::vector<float>& m) { return measure::db(measure::rms(Span(m, 14400, 33600))); };
  Rig mixed;
  play({chord[0], chord[1], chord[2], chord[3]}, 1, mixed);
  const int split = static_cast<int>(mixed.s.v[params::kSplitKey]);
  THEN("every key below the split is played by trombones and the others by trumpets, two players each");
  int trombones = 0, trumpet_players = 0;
  bool right = true;
  for (int i = 0; i < dsp::winds::Section::kMaxPlayers; ++i) {
    const auto v = mixed.engine.section().view(i);
    if (!v.awake) continue;
    const int want = v.key < split ? params::kTrombone : params::kTrumpet;
    right = right && v.instrument == want;
    (v.instrument == params::kTrombone ? trombones : trumpet_players) += 1;
  }
  CHECK(right && trombones == 6 && trumpet_players == 2, spec::fmt("%.0f trombone and %.0f trumpet players, the split at key %.0f", trombones, trumpet_players, split));
  CHECK(finite(mixed.mono()) && measure::peak(Span(mixed.mono())) <= Limiter::kCeiling + 1e-4, "the chord is finite and under the ceiling");
  THEN("each low note alone holds within 3 dB of the top note's level, where on the trumpets E3 is at least 8 dB under");
  Rig top;
  play({64}, 1, top);
  const double top_db = held_db(top.mono());
  bool comparable = true, trombone_sound = true;
  double trumpet_e3 = 0.0;
  for (int k : {52, 56, 59}) {
    Rig with, without, alone;
    play({k}, 1, with); play({k}, 0, without); play({k}, -1, alone);
    const auto w = with.mono(), o = without.mono(), t = alone.mono();
    std::printf("    key %d against E4: %+.1f dB with trombones, %+.1f dB on trumpets\n", k, held_db(w) - top_db, held_db(o) - top_db);
    comparable = comparable && std::abs(held_db(w) - top_db) <= 3.0;
    if (k == 52) trumpet_e3 = held_db(o) - top_db;
    // The section's low note is the trombones' own note, level and colour.
    const double cw = measure::spectral_centroid(Span(w, 14400, 16384), kRate, 13), ct = measure::spectral_centroid(Span(t, 14400, 16384), kRate, 13);
    trombone_sound = trombone_sound && std::abs(held_db(w) - held_db(t)) <= 0.5 && std::abs(cw / ct - 1.0) <= 0.02;
  }
  CHECK(comparable, "the low notes sound with the top");
  CHECK(trumpet_e3 < -8.0, spec::fmt("the trumpets' E3 alone is %.1f dB under E4 (the reason for the trombones)", trumpet_e3));
  THEN("each low note is what the trombones play on their own: the same level within 0.5 dB and spectral centroid within 2%");
  CHECK(trombone_sound, "the low notes are trombones");
}

SCENARIO(every_preset_meets_the_quality_bar) {
  GIVEN("each factory preset played the audit phrase: a held note (a chord for a section), then three melody notes");
  bool all_ok = true;
  double loudest = -120.0, quietest = 0.0;
  for (size_t p = 0; p < presets::count(); ++p) {
    Engine engine;
    engine.prepare(kRate);
    Settings s = Settings::defaults();
    presets::apply(p, s.v.data());
    const audit::Rendered r = audit::render(s, engine, 2.0);
    const double peak = std::max(measure::peak(Span(r.left)), measure::peak(Span(r.right)));
    const double loud = measure::momentary_loudness_max(r.mono, kRate);
    const Span chord(r.mono, r.first_note + 14400, 52800);
    const double dc = std::abs(measure::mean(chord));
    const bool ok = finite(r.mono) && std::abs(loud - audit::kTargetLufs) <= audit::kLoudnessSlackDb && peak <= Limiter::kCeiling + 1e-4 && !engine.clipped() && measure::rms(chord) > 1e-3 && dc < 0.01;
    loudest = std::max(loudest, loud); quietest = std::min(quietest, loud);
    if (!ok) { all_ok = false; std::printf("    preset %zu '%s': loud %.1f LUFS limit %.1f dB peak %.1f dBFS dc %.1f dB rms %.4f finite %d clipped %d\n", p, presets::all()[p].name, loud, engine.limited_db(), measure::db(peak), measure::db(dc), measure::rms(chord), finite(r.mono), engine.clipped()); }
  }
  THEN("every one is finite, within 2 dB of the target loudness, under the ceiling with the clip light off, audible and free of DC");
  CHECK(all_ok, "all presets meet the bar");
  CHECK(presets::count() == params::kPresetCount, spec::fmt("preset table has %.0f entries, the parameter promises %.0f", static_cast<double>(presets::count()), params::kPresetCount));
}

SCENARIO(the_output_limiter_holds_the_ceiling_and_leaves_quiet_signals_alone) {
  GIVEN("a fortissimo brass chord with the master at +12 dB, then at -24 dB");
  const auto chord = [](double master) {
    Rig rig; rig.s.v[params::kMasterDb] = master; rig.s.v[params::kInstrument] = params::kTrombone; rig.s.v[params::kPlayMode] = 1;
    for (int key : {48, 55, 60, 64, 67}) rig.on(key, 1.0);
    rig.render(48000);
    return std::tuple{rig.engine.clipped(), measure::db(std::max(measure::peak(Span(rig.left)), measure::peak(Span(rig.right)))), rig.engine.limited_db()};
  };
  const auto [hot_clipped, hot_peak, hot_limited] = chord(12.0);
  const auto [quiet_clipped, quiet_peak, quiet_limited] = chord(-24.0);
  THEN("the hot one peaks at the -2 dBFS ceiling with the clip light on; the quiet one, peaking under the ceiling, is not reduced at all");
  CHECK_NEAR(hot_peak, -2.0, 0.1, "ceiling");
  CHECK(hot_clipped && hot_limited > 9.0, ::spec::fmt("clip light, %.1f dB of reduction", hot_limited));
  CHECK(!quiet_clipped && quiet_limited == 0.0 && quiet_peak < -2.5, ::spec::fmt("quiet signal passes: peak %.1f dBFS, %.2f dB of reduction", quiet_peak, quiet_limited));
}

SCENARIO(the_advertised_tail_covers_the_decay) {
  GIVEN("the French horn preset in a hall with a long release, a note released");
  Rig rig;
  for (size_t p = 0; p < presets::count(); ++p) if (std::string(presets::all()[p].name) == "French horn") presets::apply(p, rig.s.v.data());
  rig.s.v[params::kRelease] = 1500.0;
  rig.on(60); rig.render(24000); rig.off(60);
  const uint32_t tail = rig.engine.tail_samples(rig.s);
  rig.render(tail);
  THEN("after the advertised tail the output is quiet");
  CHECK(tail > 96000, spec::fmt("tail of %.1f s advertised", tail / kRate));
  CHECK(measure::peak(Span(rig.mono(), rig.left.size() - 4800)) < 0.02, "quiet after the tail");
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
