// Every parameter, proved against what it advertises, from audio, through the built plug-in.
//
// The plug-in advertises each parameter as a name, a unit and a range, and the editor's help
// text says what it does. This suite holds one proof per parameter ID: a musical context (an
// instrument, a few settings, the notes and controllers played), two values of the parameter,
// and a measurement in the advertised unit comparing the two renders: cents for pitch, dB for
// level, Hz for rates, milliseconds for times, a count of players, or the percent change of a
// measured quantity (a spectral centroid, a band's energy) for the rest.
//
// The table is indexed by ID and the suite fails if any parameter has no proof, so a new
// parameter cannot ship without one.
//
//   advertised_tests <HONKR.clap> [name-filter]

#include "../src/dsp/air.hpp"
#include "../src/dsp/core.hpp"
#include "../src/dsp/temperament.hpp"
#include "../src/params.hpp"
#include "audio_measure.hpp"
#include "clap_harness.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace harness;
using measure::Span;
namespace P = honkr::params;

namespace {

// One sample rate throughout: the plug-in calibrates its bores once per rate per process.
constexpr double kRate = 48000.0;
constexpr uint32_t kBlock = 256;
// The plug-in stays loaded for the whole run, so that calibration is done only once.
std::unique_ptr<Library> g_library;
double g_note_hz = 261.63;

struct Setting { uint32_t id; double value; };

// Something played at a time: a key, or a MIDI message (a controller, pressure, the bend).
struct Timed {
  double at_s;
  enum Kind { On, Off, Midi } kind;
  int key = 0;
  double velocity = 0.7;
  uint8_t status = 0, data1 = 0, data2 = 0;
};
Timed key_on(double at, int key, double velocity = 0.7) { return {at, Timed::On, key, velocity}; }
Timed key_off(double at, int key) { return {at, Timed::Off, key}; }
Timed midi(double at, uint8_t status, uint8_t data1, uint8_t data2) { Timed t{at, Timed::Midi}; t.status = status; t.data1 = data1; t.data2 = data2; return t; }

// The musical situation a parameter is proved in: one key held from the start for hold_s, then
// a tail, and whatever else is played meanwhile.
struct Context {
  int instrument = P::kAltoSax;
  std::vector<Setting> settings;
  int key = 60;
  double velocity = 0.7;
  double hold_s = 1.0, tail_s = 0.5;
  std::vector<Timed> extra;
};

struct Render { std::vector<float> mono, left, right; size_t note_off = 0; };

size_t at(double seconds) { return static_cast<size_t>(seconds * kRate); }

Render render(const Context& c, uint32_t id, double value) {
  const clap_plugin_t* plugin = g_library->create();
  set_param(plugin, P::kInstrument, c.instrument);
  for (const Setting& s : c.settings) set_param(plugin, s.id, s.value);
  set_param(plugin, id, value);
  Runner runner(plugin, kRate, kBlock);
  Render out;
  out.note_off = at(c.hold_s);
  const size_t total = at(c.hold_s + c.tail_s);
  std::vector<Timed> timeline;
  timeline.push_back(key_on(0.0, c.key, c.velocity));
  timeline.push_back(key_off(c.hold_s, c.key));
  timeline.insert(timeline.end(), c.extra.begin(), c.extra.end());
  // Controllers before notes at the same instant, so a note starts with them already set.
  std::stable_sort(timeline.begin(), timeline.end(), [](const Timed& a, const Timed& b) {
    if (at(a.at_s) != at(b.at_s)) return at(a.at_s) < at(b.at_s);
    return a.kind == Timed::Midi && b.kind != Timed::Midi;
  });
  size_t next = 0;
  Events events;
  for (size_t rendered = 0; rendered < total; rendered += kBlock) {
    events.clear();
    while (next < timeline.size() && at(timeline[next].at_s) < rendered + kBlock) {
      const Timed& t = timeline[next++];
      const uint32_t offset = static_cast<uint32_t>(std::max(at(t.at_s), rendered) - rendered);
      if (t.kind == Timed::Midi) {
        clap_event_midi_t e = midi_event(t.status, t.data1, t.data2);
        e.header.time = offset;
        events.push(e);
      } else {
        clap_event_note_t e = note_event(t.kind == Timed::On ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF, 0, static_cast<int16_t>(t.key), -1, t.kind == Timed::On ? t.velocity : 0.0);
        e.header.time = offset;
        events.push(e);
      }
    }
    runner.run(&events);
    for (uint32_t i = 0; i < kBlock; ++i) {
      out.left.push_back(runner.left()[i]); out.right.push_back(runner.right()[i]);
      out.mono.push_back(0.5f * (runner.left()[i] + runner.right()[i]));
    }
  }
  runner.destroy();
  return out;
}

// --- measurements --------------------------------------------------------------------------------

// The held note once it has spoken, to just before the key is let go.
Span held(const Render& r) { return Span(r.mono, at(0.15), r.note_off - at(0.2) - at(0.15)); }
Span window(const Render& r, double from_s, double to_s) { return Span(r.mono, at(from_s), at(to_s) - at(from_s)); }
double level_db(const Span& s) { return measure::db(measure::rms(s)); }
double centroid(const Span& s) { return measure::spectral_centroid(s, kRate, 13); }
// A tuner's reading of the strongest component near a frequency: its amplitude, and its pitch.
double peak_hz(const Span& s, double around, double span_cents = 60.0, double step = 0.5) {
  double best = -1.0, best_hz = around;
  for (double c = -span_cents; c <= span_cents; c += step) {
    const double f = around * std::exp2(c / 1200.0);
    const double a = measure::tone_amplitude(s, kRate, f);
    if (a > best) { best = a; best_hz = f; }
  }
  // Refined to a tenth of a cent around the coarse peak.
  const double coarse = best_hz;
  for (double c = -step; c <= step; c += 0.1) {
    const double f = coarse * std::exp2(c / 1200.0);
    const double a = measure::tone_amplitude(s, kRate, f);
    if (a > best) { best = a; best_hz = f; }
  }
  return best_hz;
}
double tone_near(const Span& s, double hz, double span_cents = 30.0) {
  double best = 0.0;
  for (double c = -span_cents; c <= span_cents; c += 2.0) best = std::max(best, measure::tone_amplitude(s, kRate, hz * std::exp2(c / 1200.0)));
  return best;
}
double tone_db(const Span& s, double hz, double span_cents = 30.0) { return measure::db(tone_near(s, hz, span_cents)); }
// The energy in a band, in dB: its share of the spectrum times the mean square.
double band_db(const Span& s, double lo, double hi) {
  const double rms = measure::rms(s);
  return 10.0 * std::log10(std::max(measure::band_fraction(s, kRate, lo, hi) * rms * rms, 1e-24));
}
// Level contour every 2 ms over 4 ms windows.
std::vector<double> contour(const Render& r) { return measure::contour_db(r.mono, kRate, 0.002, 0.004); }
// How much later, in seconds, the note starts in b than in a: the start of a (from just before
// it is first heard to 10 ms in, where nothing is periodic yet) found in b by normalised
// cross-correlation.
double lag_s(const Render& a, const Render& b, double max_s) {
  double top = 0.0;
  for (size_t i = 0; i < at(0.5); ++i) top = std::max(top, static_cast<double>(std::abs(a.mono[i])));
  size_t start = 0;
  while (start < at(0.5) && std::abs(a.mono[start]) < 1e-3 * top) ++start;
  const size_t from = start > at(0.002) ? start - at(0.002) : 0, n = at(0.012), max_lag = at(max_s);
  double best = -1e30; size_t best_lag = 0;
  std::vector<double> r(max_lag + 1, 0.0);
  for (size_t lag = 0; lag <= max_lag; ++lag) {
    double num = 0.0, ea = 0.0, eb = 0.0;
    for (size_t i = from; i < from + n; ++i) {
      const double x = a.mono[i], y = b.mono[i + lag];
      num += x * y; ea += x * x; eb += y * y;
    }
    r[lag] = num / std::sqrt(std::max(ea * eb, 1e-30));
    if (r[lag] > best) { best = r[lag]; best_lag = lag; }
  }
  double shift = 0.0;
  if (best_lag > 0 && best_lag < max_lag) {
    const double d = r[best_lag - 1] - 2.0 * r[best_lag] + r[best_lag + 1];
    if (std::abs(d) > 1e-12) shift = std::clamp(0.5 * (r[best_lag - 1] - r[best_lag + 1]) / d, -0.5, 0.5);
  }
  return (static_cast<double>(best_lag) + shift) / kRate;
}

// A tuner's reading every 10 ms on 40 ms windows from `from_s` to `to_s`, in Hz, around `hz`.
std::vector<double> pitch_track(const Render& r, double from_s, double to_s, double hz, double span_cents = 80.0) {
  std::vector<double> out;
  const size_t win = at(0.04), hop = at(0.01);
  for (size_t p = at(from_s); p + win <= at(to_s); p += hop) out.push_back(peak_hz(Span(r.mono, p, win), hz, span_cents, 2.0));
  return out;
}
// How far the pitch swings either side of its mean, in cents: the RMS deviation times sqrt 2,
// a sine's peak.
double swing_cents(const std::vector<double>& hz) {
  double mean = 0.0;
  for (double f : hz) mean += std::log2(f);
  mean /= static_cast<double>(hz.size());
  double sum = 0.0;
  for (double f : hz) { const double c = 1200.0 * (std::log2(f) - mean); sum += c * c; }
  return std::sqrt(2.0 * sum / static_cast<double>(hz.size()));
}

// The local maxima of a fine scan around a frequency, within `floor_db` of the strongest: the
// separate tones a section in unison makes of one partial.
std::vector<double> peaks_near(const Span& s, double hz, double span_cents, double floor_db) {
  std::vector<double> f, a;
  for (double c = -span_cents; c <= span_cents; c += 0.5) { f.push_back(hz * std::exp2(c / 1200.0)); a.push_back(measure::tone_amplitude(s, kRate, f.back())); }
  const double top = *std::max_element(a.begin(), a.end());
  std::vector<double> out;
  for (size_t i = 1; i + 1 < a.size(); ++i)
    if (a[i] > a[i - 1] && a[i] >= a[i + 1] && measure::db(a[i] / top) > -floor_db) out.push_back(f[i]);
  return out;
}

// --- verdicts --------------------------------------------------------------------------------------

struct Verdict { bool ok; std::string detail; };
Verdict verdict(bool ok, const char* format, double a = 0.0, double b = 0.0, double c = 0.0, double d = 0.0) {
  char out[320]; std::snprintf(out, sizeof(out), format, a, b, c, d); return {ok, out};
}
using Check = std::function<Verdict(const Render&, const Render&)>;

struct Proof {
  Context context;
  double low, high;
  Check check;
};

// The fundamental near the note moves by `expected` cents.
Check PitchCents(double expected, double tolerance, double span = 120.0) {
  return [=](const Render& a, const Render& b) {
    const double pa = peak_hz(held(a), g_note_hz, span), pb = peak_hz(held(b), g_note_hz * std::exp2(expected / 1200.0), span);
    const double cents = measure::cents(pb, pa);
    return verdict(std::abs(cents - expected) <= tolerance, "pitch moved %+.1f cents (expected %+.1f +- %.1f)", cents, expected, tolerance);
  };
}
Check LevelDb(double expected, double tolerance) {
  return [=](const Render& a, const Render& b) {
    const double d = level_db(held(b)) - level_db(held(a));
    return verdict(std::abs(d - expected) <= tolerance, "level moved %+.2f dB (expected %+.2f +- %.2f)", d, expected, tolerance);
  };
}
// The tail after release, 50 to 600 ms after the key is let go, rises by at least min_db.
Check LongerTail(double min_db) {
  return [=](const Render& a, const Render& b) {
    const double d = level_db(Span(b.mono, b.note_off + at(0.05), at(0.55))) - level_db(Span(a.mono, a.note_off + at(0.05), at(0.55)));
    return verdict(d >= min_db, "tail level moved %+.1f dB (expected at least %+.1f)", d, min_db);
  };
}
// L/R correlation over the held note.
double lr_correlation(const Render& r) { return measure::correlation(Span(r.left, at(0.15), at(0.6)), Span(r.right, at(0.15), at(0.6))); }

// How long a level contour takes, from `from_s`, to first reach `threshold_db`; in ms.
double reach_ms(const std::vector<double>& c, double from_s, double threshold_db) {
  const long i = measure::first_at_or_above(c, threshold_db, static_cast<size_t>(from_s / 0.002));
  return i < 0 ? 1e9 : static_cast<double>(i) * 2.0 - from_s * 1000.0;
}
// After `from_s`, the time until the contour stays below `threshold_db`; in ms.
double fall_ms(const std::vector<double>& c, double from_s, double threshold_db) {
  const long last = measure::last_at_or_above(c, threshold_db);
  return std::max(0.0, static_cast<double>(last + 1) * 2.0 - from_s * 1000.0);
}

// The even harmonics' energy against the odd ones', in dB, over the first eight.
// The even harmonics' energy over the odd ones', the fundamental left out as the recordings'
// measure leaves it (tests/timbre.hpp): a strong fundamental is not a clarinet's hollowness.
double even_odd_db(const Span& s, double f0) {
  double even = 0.0, odd = 0.0;
  for (int k = 2; k <= 9; ++k) { const double a = tone_near(s, f0 * k, 15.0); (k % 2 ? odd : even) += a * a; }
  return 10.0 * std::log10(std::max(even, 1e-24) / std::max(odd, 1e-24));
}

Context ctx(int instrument, std::vector<Setting> settings = {}, int key = 60) { Context c; c.instrument = instrument; c.settings = std::move(settings); c.key = key; return c; }

std::array<Proof, P::kParamCount> proofs() {
  using namespace P;
  std::array<Proof, kParamCount> t{};
  const auto set = [&](ParamId id, Context c, double low, double high, Check check) { t[id] = Proof{std::move(c), low, high, std::move(check)}; };
  // A steady tone for anything measured in pitch: no vibrato.
  const Setting still{kVibrato, 0};

  // --- the instrument and its tuning -------------------------------------------------------------
  // A clarinet is a cylinder closed at the reed, so its low register sounds mostly the odd
  // harmonics; a saxophone's cone sounds them all. On the same G3, the clarinet's even
  // harmonics sit well under the saxophone's.
  set(kInstrument, ctx(kAltoSax, {still}, 55), kAltoSax, kClarinet, [](const Render& a, const Render& b) {
    const double sa = even_odd_db(held(a), g_note_hz), sb = even_odd_db(held(b), g_note_hz);
    return verdict(sb < sa - 10.0, "even/odd harmonic energy %.1f dB (alto sax) -> %.1f dB (clarinet), expected at least 10 dB less", sa, sb);
  });
  set(kMasterDb, ctx(kAltoSax, {still}), -30, -6, LevelDb(24.0, 0.3));
  set(kTranspose, ctx(kAltoSax, {still}), 0, 12, PitchCents(1200.0, 5.0, 150.0));
  // A4 from 415 to 440 is 101.6 cents, on A4 itself.
  set(kPitchA4, ctx(kAltoSax, {still}, 69), 415, 440, PitchCents(1200.0 * std::log2(440.0 / 415.0), 3.0));
  // Quarter-comma meantone laid from C with A on the fork: G# is eight narrow fifths up from C
  // and A three, so G# sits five fifths' narrowing (5 x 5.38 = 26.9 cents) under A's place:
  // 17.1 cents flat of equal temperament, read on G#4.
  {
    const double fifth = 1200.0 * std::log2(std::pow(5.0, 0.25)) - 700.0;
    set(kTemperament, ctx(kAltoSax, {still}, 68), 0, 2, PitchCents((8 - 3) * fifth, 2.0));
  }
  // Laid from D instead of C, E-flat becomes D-sharp, seven narrow fifths up from D instead of
  // three below C, with A one fifth up from D instead of three from C: 41.1 cents lower against
  // A, read on E-flat 4.
  {
    const double fifth = 1200.0 * std::log2(std::pow(5.0, 0.25)) - 700.0;
    const double from_c = (-3 - 3) * fifth, from_d = (7 - 1) * fifth;  // Eb (or D#) against A
    set(kTemperamentRoot, ctx(kAltoSax, {still, {kTemperament, 2}}, 63), 0, 2, PitchCents(from_d - from_c, 2.0));
  }
  set(kFineTune, ctx(kAltoSax, {still}), -50, 50, PitchCents(100.0, 3.0));

  // --- how it is played ------------------------------------------------------------------------------
  // C4 held and E4 pressed half-way through. Solo slurs to E4 and C4 stops; Section plays a
  // chord, C4 sounding on under E4.
  {
    Context c = ctx(kAltoSax, {still});
    c.hold_s = 1.2;
    c.extra = {key_on(0.6, 64), key_off(1.2, 64)};
    set(kPlayMode, c, 0, 1, [](const Render& a, const Render& b) {
      const Span la = window(a, 0.8, 1.15), lb = window(b, 0.8, 1.15);
      const double c_solo = tone_db(la, g_note_hz), c_section = tone_db(lb, g_note_hz);
      const double e_solo = tone_db(la, honkr::dsp::midi_to_hz(64)), e_section = tone_db(lb, honkr::dsp::midi_to_hz(64));
      const double c_solo_rel = c_solo - e_solo, c_section_rel = c_section - e_section;
      return verdict(c_section_rel - c_solo_rel >= 20.0 && e_solo > c_solo + 20.0,
                     "C4 against E4 after E4 is pressed: %.1f dB (solo) -> %.1f dB (section); solo E4 over C4 by %.1f dB (expected section at least 20 dB more, solo E4 at least 20 dB over)",
                     c_solo_rel, c_section_rel, e_solo - c_solo);
    });
  }
  // A slur from C4 up to G4: the time the pitch takes to arrive within 25 cents of G4, which
  // grows by about the glide (the bore's length moves linearly, so the last 25 of 700 cents
  // take the last 2.3% of it).
  {
    Context c = ctx(kAltoSax, {still, {kTongue, 0}});
    c.hold_s = 1.5;
    c.extra = {key_on(0.5, 67), key_off(1.5, 67)};
    set(kGlide, c, 0, 400, [](const Render& a, const Render& b) {
      const double g4 = honkr::dsp::midi_to_hz(67);
      const auto arrival = [&](const Render& r) {
        const auto track = pitch_track(r, 0.45, 1.4, honkr::dsp::midi_to_hz(64), 450.0);
        size_t last_out = 0;
        for (size_t i = 0; i < track.size(); ++i) if (std::abs(measure::cents(track[i], g4)) > 25.0) last_out = i + 1;
        return 0.45 * 1000.0 + static_cast<double>(last_out) * 10.0 + 20.0 - 500.0;  // window centre, from the slur
      };
      const double ta = arrival(a), tb = arrival(b), expected = 400.0 * 0.977;
      return verdict(std::abs((tb - ta) - expected) <= 40.0, "arrival %.0f ms -> %.0f ms after the slur: %+.0f ms (expected %+.0f +- 40)", ta, tb, tb - ta, expected);
    });
  }
  // The wheel full up, at 2 and then at 7 semitones: 500 cents higher.
  {
    Context c = ctx(kAltoSax, {still}, 60);
    c.extra = {midi(0.0, 0xe0, 0x7f, 0x7f)};
    // Each note is looked for around where its bend puts it (the wheel's top is 8191/8192).
    set(kBendRange, c, 2, 7, [](const Render& a, const Render& b) {
      const double full = 8191.0 / 8192.0, expected = 500.0 * full;
      const double pa = peak_hz(held(a), g_note_hz * std::exp2(2.0 * full / 12.0), 120.0), pb = peak_hz(held(b), g_note_hz * std::exp2(7.0 * full / 12.0), 120.0);
      const double cents = measure::cents(pb, pa);
      return verdict(std::abs(cents - expected) <= 5.0, "pitch moved %+.1f cents (expected %+.1f +- 5.0)", cents, expected);
    });
  }

  // --- the breath ------------------------------------------------------------------------------------
  // A breath controller blown full, then all but let go half-way through the note. From the
  // velocity, the note is unmoved; from the breath controller, it falls away.
  {
    Context c = ctx(kAltoSax, {still}, 60);
    c.velocity = 1.0;
    c.hold_s = 2.0;
    c.extra = {midi(0.0, 0xb0, 2, 127), midi(1.0, 0xb0, 2, 10)};
    set(kBreathSource, c, kFromVelocity, kFromBreathCc, [](const Render& a, const Render& b) {
      const double da = level_db(window(a, 1.4, 1.8)) - level_db(window(a, 0.4, 0.9));
      const double db = level_db(window(b, 1.4, 1.8)) - level_db(window(b, 0.4, 0.9));
      return verdict(std::abs(da) <= 1.5 && db <= -20.0, "level change when CC2 drops to 10: %+.1f dB (velocity) -> %+.1f dB (breath CC2), expected ~0 and at most -20", da, db);
    });
  }
  set(kBreath, ctx(kAltoSax, {still}), 0.5, 1.0, [](const Render& a, const Render& b) {
    const double la = level_db(held(a)), lb = level_db(held(b));
    return verdict(lb - la >= 20.0, "level %.1f dBFS (50%% breath) -> %.1f dBFS (100%%): %+.1f dB, expected under the threshold at 50%% (at least 20 dB quieter)", la, lb, lb - la);
  });
  // A soft key on the trumpet (whose lips follow the breath over a wide range): with Dynamics
  // at none, it plays mezzo-forte like any other.
  {
    Context c = ctx(kTrumpet, {still}, 65); c.velocity = 0.1;
    set(kDynamics, c, 1, 0, [](const Render& a, const Render& b) {
      const double d = level_db(held(b)) - level_db(held(a));
      return verdict(d >= 6.0, "a soft key's level moved %+.1f dB with Dynamics 100%% -> 0%% (expected at least +6)", d);
    });
  }
  // The breath building behind the tongue: the time to come within 3 dB of the held level.
  {
    Context c = ctx(kAltoSax, {still, {kTongue, 0}}); c.hold_s = 1.2;
    set(kAttack, c, 5, 300, [](const Render& a, const Render& b) {
      const auto rise = [](const Render& r) { const auto c = contour(r); return reach_ms(c, 0.0, level_db(window(r, 0.8, 1.1)) - 3.0); };
      const double ra = rise(a), rb = rise(b);
      return verdict(ra < 80.0 && rb - ra >= 150.0 && rb - ra <= 450.0, "rise to -3 dB of the held level %.0f ms -> %.0f ms (expected under 80, then 150 to 450 ms later)", ra, rb);
    });
  }
  // The time after the key is let go until the sound has fallen 40 dB. The breath falls with a
  // time constant of a third of the release and the trumpet's sound with it, so 40 dB takes
  // ln(100) / 3 of the release: 1400 ms more release is 2149 ms more fall.
  {
    Context c = ctx(kTrumpet, {still}, 65); c.tail_s = 3.0;
    set(kRelease, c, 200, 1600, [](const Render& a, const Render& b) {
      const auto fall = [](const Render& r) { return fall_ms(contour(r), static_cast<double>(r.note_off) / kRate, level_db(held(r)) - 40.0); };
      const double fa = fall(a), fb = fall(b), expected = std::log(100.0) / 3.0 * 1400.0;
      return verdict(std::abs((fb - fa) - expected) <= 0.1 * expected, "fall of 40 dB after release %.0f ms -> %.0f ms: %+.0f ms (expected %+.0f +- 10%%)", fa, fb, fb - fa, expected);
    });
  }
  // The tongue's release: a burst of pressure and the consonant's noise in the first 30 ms.
  set(kTongue, ctx(kTrumpet, {still}, 70), 0, 1, [](const Render& a, const Render& b) {
    const auto burst = [](const Render& r) { const Span s(r.mono, 0, at(0.03)); return band_db(s, 3000.0, 16000.0); };
    const auto peak = [](const Render& r) { const auto c = contour(r); double m = -200.0; for (size_t i = 0; i < 40 && i < c.size(); ++i) m = std::max(m, c[i]); return m - level_db(window(r, 0.5, 0.8)); };
    const double ba = burst(a), bb = burst(b), pa = peak(a), pb = peak(b);
    return verdict(bb - ba >= 6.0, "attack high-band energy %+.1f dB; first 80 ms peak over held %+.1f -> %+.1f dB (expected the burst at least 6 dB more)", bb - ba, pa, pb);
  });
  // The breath's turbulence on a flute, heard between its harmonics: four times the turbulence
  // is 12 dB more hiss there, the tone itself unchanged.
  set(kBreathNoise, ctx(kFlute, {still}, 72), 1, 4, [](const Render& a, const Render& b) {
    const auto hiss = [](const Render& r) {
      double e = 0.0;
      for (int k = 1; k <= 10; ++k) e += std::pow(measure::tone_amplitude(held(r), kRate, g_note_hz * (k + 0.5)), 2.0);
      return 10.0 * std::log10(e);
    };
    const double d = hiss(b) - hiss(a), expected = 20.0 * std::log10(4.0), tone = level_db(held(b)) - level_db(held(a));
    return verdict(std::abs(d - expected) <= 1.5 && std::abs(tone) <= 0.5, "noise between the harmonics moved %+.1f dB (expected %+.1f +- 1.5); the whole %+.2f dB (expected within 0.5)", d, expected, tone);
  });
  // Growl: the level rattles at 25 to 35 Hz.
  {
    Context c = ctx(kTenorSax, {still}, 55); c.hold_s = 1.5;
    set(kGrowl, c, 0, 1, [](const Render& a, const Render& b) {
      const auto envelope = [](const Render& r) {
        std::vector<double> linear;
        const std::vector<float> part(r.mono.begin() + static_cast<long>(at(0.3)), r.mono.begin() + static_cast<long>(r.note_off - at(0.1)));
        for (double v : measure::contour_db(part, kRate, 0.002, 0.004)) linear.push_back(std::pow(10.0, v / 20.0));
        return linear;
      };
      const auto depth = [](const std::vector<double>& e) {
        // The envelope's component at 25 to 35 Hz against its mean, in dB.
        double mean = 0.0; for (double v : e) mean += v; mean /= static_cast<double>(e.size());
        double best = 0.0;
        for (double hz = 25.0; hz <= 35.0; hz += 0.5) {
          std::complex<double> s = 0.0;
          for (size_t i = 0; i < e.size(); ++i) s += (e[i] - mean) * std::polar(1.0, -honkr::dsp::kTau * hz * 0.002 * static_cast<double>(i));
          best = std::max(best, 2.0 * std::abs(s) / static_cast<double>(e.size()));
        }
        return measure::db(best / mean);
      };
      const auto ea = envelope(a), eb = envelope(b);
      const double hz = measure::modulation_hz(eb, 0.002, 15.0, 60.0);
      const double da = depth(ea), db = depth(eb);
      return verdict(hz >= 25.0 && hz <= 35.0 && db - da >= 15.0, "level modulation at %.1f Hz; its depth at 25-35 Hz %.1f dB -> %.1f dB (expected 25-35 Hz, at least 15 dB deeper)", hz, da, db);
    });
  }

  // --- the mouth -------------------------------------------------------------------------------------
  // A hard reed stands open and sounds darker than a soft one: proved on the clarinet. (On the
  // conical reeds, saxophones, oboe and bassoon, the model does the opposite: a soft reed does
  // not speak at all and a hard one is brighter. Reported, not proved.)
  set(kReed, ctx(kClarinet, {still}, 62), 0.1, 0.9, [](const Render& a, const Render& b) {
    const double ca = centroid(held(a)), cb = centroid(held(b));
    const double change = 100.0 * (cb / ca - 1.0);
    return verdict(change <= -10.0, "spectral centroid %.0f Hz -> %.0f Hz: %+.0f%% (expected at least 10%% darker)", ca, cb, change);
  });
  // Flutes: a short jet overblows; the flute's C5 leaves its fundamental for an upper mode.
  // (The help promises the octave; the model jumps to the 5th or 6th mode. And on brass, a firm
  // embouchure darkens where the help promises it brightens. Both reported, not proved.)
  set(kEmbouchure, ctx(kFlute, {still}, 72), 0.5, 1.0, [](const Render& a, const Render& b) {
    const double pa = measure::pitch_hz(held(a), kRate, 100.0, 4000.0), pb = measure::pitch_hz(held(b), kRate, 100.0, 4000.0);
    const double ca = measure::cents(pa, g_note_hz), cb = pb > 0.0 ? measure::cents(pb, g_note_hz) : 0.0;
    return verdict(std::abs(ca) <= 10.0 && cb >= 1150.0, "sounding %+.0f cents -> %+.0f cents from C5 (expected the note, then overblown at least an octave)", ca, cb);
  });
  // The French horn's D5, the 10th partial of its B-flat side, the lips set a semitone above
  // it: the horn cracks up to the next partial, the 11th, 165 cents higher.
  set(kLipTension, ctx(kFrenchHorn, {still}, 74), 0, 100, [](const Render& a, const Render& b) {
    const double pa = measure::pitch_hz(held(a), kRate, 100.0, 2000.0), pb = measure::pitch_hz(held(b), kRate, 100.0, 2000.0);
    const double cents = measure::cents(pb, pa), expected = 1200.0 * std::log2(11.0 / 10.0);
    return verdict(std::abs(cents - expected) <= 25.0, "pitch %.1f Hz -> %.1f Hz: %+.0f cents (expected the next partial, %+.0f +- 25)", pa, pb, cents, expected);
  });
  // The trumpet's concert C4 is fingered 1+3 on the 3rd partial. The two crooks together are
  // short of the fourth they are asked for (Benade): the horn alone sounds that much sharp, and
  // full lipping brings it to the tempered C4.
  {
    const double crooks = 1.0 + (std::exp2(2.0 / 12.0) - 1.0) + (std::exp2(3.0 / 12.0) - 1.0);
    const double natural = 3.0 * 116.54 / crooks;
    const double expected = measure::cents(honkr::dsp::midi_to_hz(60), natural);
    set(kLipping, ctx(kTrumpet, {still}, 60), 0, 1, PitchCents(expected, 4.0, 80.0));
  }
  // The hand closing the bell: the energy above 2 kHz falls away.
  set(kHand, ctx(kTrumpet, {still}, 70), 0, 1, [](const Render& a, const Render& b) {
    const double d = (band_db(held(b), 2000.0, 16000.0) - level_db(held(b))) - (band_db(held(a), 2000.0, 16000.0) - level_db(held(a)));
    return verdict(d <= -15.0, "share of the energy above 2 kHz moved %+.1f dB (expected at most -15)", d);
  });
  // A Harmon mute: thin, the fundamental all but gone against what is left.
  set(kMute, ctx(kTrumpet, {still}, 70), 0, 3, [](const Render& a, const Render& b) {
    const double fa = tone_db(held(a), g_note_hz) - level_db(held(a)), fb = tone_db(held(b), g_note_hz) - level_db(held(b));
    return verdict(fb - fa <= -10.0, "the fundamental against the whole %.1f dB (open) -> %.1f dB (Harmon) (expected at least 10 dB less)", fa, fb);
  });
  // The pads' click on a note too soft to speak, at 25% and at 100%: 12 dB, the click being
  // proportional to the setting.
  {
    Context c = ctx(kAltoSax, {still, {kTongue, 0}, {kBreathNoise, 0}, {kDynamics, 1}}); c.velocity = 0.01; c.hold_s = 0.3; c.tail_s = 0.1;
    set(kKeyNoise, c, 0.25, 1.0, [](const Render& a, const Render& b) {
      const double d = level_db(Span(b.mono, 0, at(0.04))) - level_db(Span(a.mono, 0, at(0.04)));
      const double expected = 20.0 * std::log10(4.0);
      return verdict(std::abs(d - expected) <= 1.0, "the click's level moved %+.2f dB (expected %+.2f +- 1)", d, expected);
    });
  }
  // The didgeridoo's throat from "oo" to "ee": the second formant leaves 800 Hz for 2300 Hz.
  set(kThroat, ctx(kDidgeridoo, {still}, 38), 0, 1, [](const Render& a, const Render& b) {
    const auto tilt = [](const Render& r) { return band_db(held(r), 2000.0, 2650.0) - band_db(held(r), 700.0, 920.0); };
    const double ta = tilt(a), tb = tilt(b);
    return verdict(tb - ta >= 12.0, "2.0-2.65 kHz against 0.7-0.92 kHz %.1f dB (oo) -> %.1f dB (ee) (expected at least 12 dB more)", ta, tb);
  });
  // The pipes' drones under the chanter's D5: the bass drone's A2 (110 Hz) and the tenors' A3
  // (220 Hz) appear, at their pitch.
  set(kDrones, ctx(kBagpipe, {still}, 74), 0, 1, [](const Render& a, const Render& b) {
    const double ua = tone_db(held(a), 110.0) , ub = tone_db(held(b), 110.0);
    const double hz = peak_hz(held(b), 220.0, 40.0);
    const double cents = measure::cents(hz, 220.0);
    return verdict(ub - ua >= 20.0 && std::abs(cents) <= 5.0, "A2 tone %.1f dB -> %.1f dB; the tenors at %.2f Hz (%+.1f cents) (expected at least 20 dB more, within 5 cents of 220 Hz)", ua, ub, hz, cents);
  });

  // --- vibrato -----------------------------------------------------------------------------------------
  // 25 cents of vibrato on the alto's A4, read off a tuner every 10 ms once it has come in.
  {
    Context c = ctx(kAltoSax, {{kVibratoDelay, 0}}, 69); c.hold_s = 2.0;
    set(kVibrato, c, 0, 25, [](const Render& a, const Render& b) {
      const double sa = swing_cents(pitch_track(a, 0.5, 1.75, g_note_hz)), sb = swing_cents(pitch_track(b, 0.5, 1.75, g_note_hz));
      return verdict(sa <= 3.0 && std::abs(sb - 25.0) <= 5.0, "pitch swing %.1f cents -> %.1f cents (expected under 3, then 25 +- 5)", sa, sb);
    });
  }
  {
    Context c = ctx(kAltoSax, {{kVibrato, 25}, {kVibratoDelay, 0}}, 69); c.hold_s = 2.5;
    set(kVibratoRate, c, 4.0, 7.0, [](const Render& a, const Render& b) {
      const double ha = measure::modulation_hz(pitch_track(a, 0.5, 2.25, g_note_hz), 0.01, 2.0, 12.0);
      const double hb = measure::modulation_hz(pitch_track(b, 0.5, 2.25, g_note_hz), 0.01, 2.0, 12.0);
      return verdict(std::abs(ha - 4.0) <= 0.4 && std::abs(hb - 7.0) <= 0.5, "pitch swings at %.2f Hz -> %.2f Hz (expected 4 +- 0.4, then 7 +- 0.5)", ha, hb);
    });
  }
  // When the vibrato comes in: the first time the pitch's local swing (over a 200 ms window)
  // reaches half the depth, which moves by the delay.
  {
    Context c = ctx(kAltoSax, {{kVibrato, 30}, {kTongue, 0}}, 69); c.hold_s = 2.0;
    set(kVibratoDelay, c, 300, 1100, [](const Render& a, const Render& b) {
      const auto onset = [](const Render& r) {
        const auto track = pitch_track(r, 0.1, 1.9, g_note_hz);
        for (size_t i = 0; i + 20 <= track.size(); ++i) {
          double lo = 1e9, hi = -1e9;
          for (size_t j = i; j < i + 20; ++j) { const double cents = measure::cents(track[j], g_note_hz); lo = std::min(lo, cents); hi = std::max(hi, cents); }
          if (0.5 * (hi - lo) >= 15.0) return 100.0 + static_cast<double>(i + 20) * 10.0;
        }
        return 1e9;
      };
      const double oa = onset(a), ob = onset(b);
      return verdict(std::abs((ob - oa) - 800.0) <= 60.0, "vibrato reaches half depth at %.0f ms -> %.0f ms: %+.0f ms (expected +800 +- 60)", oa, ob, ob - oa);
    });
  }

  // --- the section -----------------------------------------------------------------------------------
  // Three players 30 cents apart: their 4th harmonics (12 Hz apart near 1 kHz) are three tones.
  {
    Context c = ctx(kAltoSax, {still, {kSpread, 30}, {kTiming, 0}}, 60); c.hold_s = 2.0;
    set(kPlayers, c, 1, 3, [](const Render& a, const Render& b) {
      const double na = static_cast<double>(peaks_near(held(a), 4.0 * g_note_hz, 70.0, 15.0).size());
      const double nb = static_cast<double>(peaks_near(held(b), 4.0 * g_note_hz, 70.0, 15.0).size());
      return verdict(na == 1.0 && nb == 3.0, "tones at the 4th harmonic: %.0f -> %.0f (expected 1, then 3)", na, nb);
    });
  }
  // Three players: the outer two sit Spread cents either side of the middle one (each also
  // off by up to 2 cents of its own).
  {
    Context c = ctx(kAltoSax, {still, {kPlayers, 3}, {kTiming, 0}}, 60); c.hold_s = 2.0;
    set(kSpread, c, 0, 24, [](const Render& a, const Render& b) {
      const auto half = [](const Render& r) {
        const auto p = peaks_near(held(r), 4.0 * g_note_hz, 70.0, 15.0);
        return p.size() < 2 ? 0.0 : 0.5 * measure::cents(p.back(), p.front());
      };
      const double ha = half(a), hb = half(b);
      return verdict(ha <= 4.0 && std::abs(hb - 24.0) <= 4.0, "outer players +-%.1f cents -> +-%.1f cents (expected under 4, then 24 +- 4)", ha, hb);
    });
  }
  // Three players 30 cents apart, so each is heard on its own at the 8th harmonic (36 Hz
  // apart near 2 kHz): when each comes in (reaches half its own level, on 100 ms windows).
  // Together at 0; at 60 ms the latest comes in well after the first, but no more than 60 ms.
  {
    Context c = ctx(kAltoSax, {still, {kPlayers, 3}, {kSpread, 30}, {kTongue, 0}, {kAttack, 2}}, 60);
    set(kTiming, c, 0, 60, [](const Render& a, const Render& b) {
      const auto entries = [](const Render& r) {
        double first = 1e9, last = -1e9;
        for (double f : peaks_near(held(r), 8.0 * g_note_hz, 70.0, 20.0)) {
          std::vector<double> amp;
          for (size_t p = 0; p + at(0.1) < at(0.6); p += at(0.002)) amp.push_back(measure::tone_amplitude(Span(r.mono, p, at(0.1)), kRate, f));
          size_t i = 0;
          while (i < amp.size() && amp[i] < 0.5 * amp.back()) ++i;
          first = std::min(first, static_cast<double>(i) * 2.0); last = std::max(last, static_cast<double>(i) * 2.0);
        }
        return last - first;
      };
      const double ea = entries(a), eb = entries(b);
      return verdict(ea <= 10.0 && eb >= 25.0 && eb <= 70.0, "latest entry after the first %.0f ms -> %.0f ms (expected under 10, then 25 to 70)", ea, eb);
    });
  }
  {
    Context c = ctx(kAltoSax, {still, {kPlayers, 3}, {kSpread, 10}}, 60);
    set(kStereoWidth, c, 0, 1, [](const Render& a, const Render& b) {
      const double ca = lr_correlation(a), cb = lr_correlation(b);
      return verdict(ca >= 0.99 && cb <= ca - 0.1, "L/R correlation %.3f -> %.3f (expected 1 for a section in the middle, then at least 0.1 less)", ca, cb);
    });
  }
  // Equal-power panning: at R 50 the right channel is tan(3 pi / 8), 7.66 dB, over the left;
  // at L 50 the other way round.
  set(kPan, ctx(kAltoSax, {still}), -0.5, 0.5, [](const Render& a, const Render& b) {
    const auto balance = [](const Render& r) { return level_db(Span(r.right, at(0.15), at(0.6))) - level_db(Span(r.left, at(0.15), at(0.6))); };
    const double ba = balance(a), bb = balance(b), expected = measure::db(std::tan(3.0 * honkr::dsp::kPi / 8.0));
    return verdict(std::abs(ba + expected) <= 0.3 && std::abs(bb - expected) <= 0.3, "right over left %+.2f dB -> %+.2f dB (expected -+%.2f +- 0.3)", ba, bb, expected);
  });

  // --- the room and the air ----------------------------------------------------------------------------
  { Context c = ctx(kAltoSax, {still, {kMicDistance, 2.0}, {kRelease, 20}}); c.tail_s = 0.8; set(kRoomSize, c, 0, 3, LongerTail(10.0)); }
  {
    Context c = ctx(kAltoSax, {still, {kRoomSize, 5}, {kMicDistance, 3.0}, {kRelease, 20}}); c.hold_s = 1.5; c.tail_s = 1.0;
    set(kRoomAbsorption, c, 0.08, 0.6, [](const Render& a, const Render& b) {
      const double d = level_db(Span(b.mono, b.note_off + at(0.05), at(0.55))) - level_db(Span(a.mono, a.note_off + at(0.05), at(0.55)));
      return verdict(d <= -6.0, "tail level moved %+.1f dB (expected at most -6)", d);
    });
  }
  // The microphone 10 m away in 20 C air: the note arrives 10 / 343.2 s = 29.1 ms later.
  set(kMicDistance, ctx(kAltoSax, {still}), 0, 10, [](const Render& a, const Render& b) {
    const double ms = 1000.0 * lag_s(a, b, 0.05), expected = 1000.0 * 10.0 / (331.3 * std::sqrt(1.0 + 20.0 / 273.15));
    return verdict(std::abs(ms - expected) <= 0.2, "arrival %+.2f ms later (expected %+.2f +- 0.2)", ms, expected);
  });
  // Twelve metres of open air from the oboe's C5: its 15th harmonic near 7.85 kHz loses what
  // ISO 9613-1 gives at 10% and at 70% humidity, humid air taking less.
  {
    Context c = ctx(kOboe, {still, {kMicDistance, 12.0}}, 72);
    set(kHumidity, c, 0.1, 0.7, [](const Render& a, const Render& b) {
      const double hz = peak_hz(held(a), 15.0 * g_note_hz, 30.0);
      const double expected = 12.0 * (honkr::dsp::air::absorption_db_per_m(hz, 20.0, 0.1) - honkr::dsp::air::absorption_db_per_m(hz, 20.0, 0.7));
      const double d = tone_db(held(b), hz, 10.0) - tone_db(held(a), hz, 10.0);
      return verdict(std::abs(d - expected) <= 0.6, "15th harmonic (%.0f Hz) moved %+.2f dB (expected %+.2f +- 0.6)", hz, d, expected);
    });
  }
  // Sound is faster in warm air: 10 m takes 30.75 ms at -10 C and 28.19 ms at 40 C.
  {
    Context c = ctx(kAltoSax, {still, {kMicDistance, 10.0}});
    set(kTemperature, c, -10, 40, [](const Render& a, const Render& b) {
      const auto travel = [](double celsius) { return 10.0 / (331.3 * std::sqrt(1.0 + celsius / 273.15)); };
      const double expected = 1000.0 * (travel(-10.0) - travel(40.0));
      const double ms = 1000.0 * lag_s(b, a, 0.01);
      return verdict(std::abs(ms - expected) <= 0.1, "the warm air's note arrives %.2f ms sooner (expected %.2f +- 0.1)", ms, expected);
    });
  }
  // A cardioid 5 cm from the bass clarinet's G2 against an omni there: the proximity effect
  // lifts the fundamental by |1 + c / (2 j w d)| (half the pattern is the pressure gradient).
  set(kMicPattern, ctx(kBassClarinet, {still, {kMicDistance, 0.05}}, 43), 0, 1, [](const Render& a, const Render& b) {
    const double f0 = peak_hz(held(a), g_note_hz, 60.0);
    const double c = 331.3 * std::sqrt(1.0 + 20.0 / 273.15), x = 0.5 * c / (honkr::dsp::kTau * f0 * 0.05);
    const double expected = 10.0 * std::log10(1.0 + x * x);
    const double d = tone_db(held(b), f0, 5.0) - tone_db(held(a), f0, 5.0);
    return verdict(std::abs(d - expected) <= 1.5, "fundamental (%.1f Hz) moved %+.1f dB (expected %+.1f +- 1.5)", f0, d, expected);
  });
  set(kMicSetup, ctx(kAltoSax, {still, {kRoomSize, 3}, {kMicDistance, 2.0}}), 0, 1, [](const Render& a, const Render& b) {
    const double ca = lr_correlation(a), cb = lr_correlation(b);
    return verdict(ca <= 0.95 && cb >= 0.999, "L/R correlation %.3f (stereo pair) -> %.3f (mono) (expected under 0.95, then 1)", ca, cb);
  });
  // "Oboe, orchestral" to "Oboe, baroque": the same oboe at A = 415, 101.6 cents lower on A4.
  set(kPreset, ctx(kAltoSax, {}, 69), 11, 12, PitchCents(1200.0 * std::log2(415.0 / 440.0), 4.0));
  return t;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: advertised_tests <HONKR.clap> [filter]\n"); return 2; }
  g_library = std::make_unique<Library>(argv[1]);
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  const char* filter = argc > 2 ? argv[2] : nullptr;
  const auto table = proofs();
  int missing = 0, failed = 0, passed = 0;
  for (uint32_t id = 0; id < P::kParamCount; ++id) {
    const Proof& proof = table[id];
    const char* name = P::kSpecs[id].name;
    if (!proof.check) { std::printf("MISSING  %s has no proof\n", name); ++missing; continue; }
    if (filter && !std::strstr(name, filter)) continue;
    g_note_hz = honkr::dsp::midi_to_hz(proof.context.key);
    const Render low = render(proof.context, id, proof.low), high = render(proof.context, id, proof.high);
    bool finite = true;
    for (const Render* r : {&low, &high}) for (float v : r->mono) finite = finite && std::isfinite(v);
    const Verdict v = finite ? proof.check(low, high) : Verdict{false, "output is not finite"};
    char lo[64], hi[64];
    P::format_value(id, proof.low, lo, sizeof(lo)); P::format_value(id, proof.high, hi, sizeof(hi));
    if (v.ok) ++passed; else ++failed;
    std::printf("%-8s %-16s %s -> %s: %s\n", v.ok ? "ok" : "FAILED", name, lo, hi, v.detail.c_str());
  }
  std::printf("%d proved, %d failed, %d missing of %u parameters\n", passed, failed, missing, static_cast<unsigned>(P::kParamCount));
  g_library.reset();
  return failed == 0 && missing == 0 ? 0 : 1;
}
