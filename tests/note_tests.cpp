// Executable scenarios for features/clap_plugin.feature (notes and automation): sound comes out
// when and only when notes say so, through both the CLAP and MIDI dialects, with sample-accurate
// event timing and automation.
#include "../src/params.hpp"
#include "audio_measure.hpp"
#include "clap_harness.hpp"
#include "spec.hpp"

#include <vector>

using namespace harness;
using measure::Span;
namespace P = honkr::params;

namespace {
const char* g_path = nullptr;
constexpr double kRate = 48000.0;

// Renders `blocks` blocks collecting the mono output.
std::vector<float> collect(Runner& runner, int blocks, Events* events = nullptr) {
  std::vector<float> out;
  for (int b = 0; b < blocks; ++b) {
    runner.run(b == 0 ? events : nullptr);
    for (uint32_t i = 0; i < runner.frames(); ++i) out.push_back(0.5f * (runner.left()[i] + runner.right()[i]));
  }
  return out;
}
}  // namespace

SCENARIO(a_clap_note_on_makes_sound_and_note_off_stops_it) {
  GIVEN("the alto sax with a 50 ms release, given a CLAP note on");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  set_param(plugin, P::kRelease, 50.0);
  Runner runner(plugin, kRate, 256);
  Events on; on.push(note_event(CLAP_EVENT_NOTE_ON, 0, 60, 7, 0.9));
  const auto held = collect(runner, 40, &on);
  THEN("audio appears once the reed speaks, and keeps sounding while the key is held");
  CHECK(measure::rms(Span(held, 2400)) > 0.01, "sound while held");
  CHECK(measure::rms(Span(held, held.size() - 2400)) > 0.01, "still sounding at the end of the hold (a wind note does not decay)");
  WHEN("the note is released by id");
  Events off; off.push(note_event(CLAP_EVENT_NOTE_OFF, 0, 60, 7, 0.0));
  const auto tail = collect(runner, 200, &off);
  THEN("the breath stops and the plug-in falls silent");
  CHECK(measure::peak(Span(tail, tail.size() - 6000)) < 1e-3, "silent after the release");
  CHECK(runner.run().peak < 1e-4f, "stays silent");
  runner.destroy();
}

SCENARIO(midi_notes_controllers_and_bend_are_honoured) {
  GIVEN("MIDI note on on the alto sax without vibrato, then pitch bend up");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  set_param(plugin, P::kInstrument, P::kAltoSax);
  set_param(plugin, P::kVibrato, 0.0);
  set_param(plugin, P::kRelease, 30.0);
  Runner runner(plugin, kRate, 256);
  Events on; on.push(midi_event(0x90, 60, 100));
  const auto plain = collect(runner, 100, &on);
  Events bend; bend.push(midi_event(0xe0, 0x7f, 0x7f));
  const auto bent = collect(runner, 100, &bend);
  THEN("the pitch is C4, then two semitones up with the wheel fully up");
  const double c4 = 261.63;
  CHECK_NEAR(measure::cents(measure::peak_hz_near(Span(plain, 12000, 8000), kRate, c4, 120.0), c4), 0.0, 25.0, "C4");
  CHECK_NEAR(measure::cents(measure::peak_hz_near(Span(bent, 12000, 8000), kRate, c4 * std::exp2(200.0 / 1200.0), 120.0), c4), 200.0, 30.0, "bent up two semitones");
  WHEN("the wheel returns and the note is released (MIDI note off)");
  Events centre; centre.push(midi_event(0xe0, 0x00, 0x40)); centre.push(midi_event(0x80, 60, 0));
  const auto off = collect(runner, 100, &centre);
  THEN("the note ends");
  CHECK(measure::peak(Span(off, off.size() - 6000)) < 1e-3, "MIDI note off ends the note");
  WHEN("the breath comes from a breath controller (CC 2): blown, then let go while the key is held, then blown again");
  set_param(plugin, P::kBreathSource, P::kFromBreathCc);
  Events blown; blown.push(midi_event(0xb0, 2, 110)); blown.push(midi_event(0x90, 60, 100));
  const auto swelled = collect(runner, 80, &blown);
  Events stop; stop.push(midi_event(0xb0, 2, 0));
  const auto stopped = collect(runner, 120, &stop);
  Events again; again.push(midi_event(0xb0, 2, 110));
  const auto resumed = collect(runner, 80, &again);
  THEN("the note sounds with the breath, falls silent without it though the key is still down, and speaks again when blown");
  const double blown_db = measure::db(measure::rms(Span(swelled, swelled.size() - 6000)));
  const double stopped_db = measure::db(measure::rms(Span(stopped, stopped.size() - 6000)));
  const double resumed_db = measure::db(measure::rms(Span(resumed, resumed.size() - 6000)));
  std::printf("    breath CC 2: blown %.1f dBFS, let go %.1f dBFS, blown again %.1f dBFS\n", blown_db, stopped_db, resumed_db);
  CHECK(blown_db > -40.0, spec::fmt("blown through CC 2: %.1f dBFS", blown_db));
  CHECK(stopped_db < blown_db - 40.0, spec::fmt("no breath, no sound: %.1f dBFS against %.1f", stopped_db, blown_db));
  CHECK(resumed_db > blown_db - 6.0, spec::fmt("blown again: %.1f dBFS", resumed_db));
  Events lift; lift.push(midi_event(0x80, 60, 0));
  collect(runner, 100, &lift);
  set_param(plugin, P::kBreathSource, P::kFromVelocity);
  WHEN("all notes off (CC 123) arrives during a chord in Section play mode");
  set_param(plugin, P::kPlayMode, 1.0);
  Events chord; chord.push(midi_event(0x90, 60, 100)); chord.push(midi_event(0x90, 64, 100)); chord.push(midi_event(0x90, 67, 100));
  const auto sounding = collect(runner, 60, &chord);
  CHECK(measure::rms(Span(sounding, sounding.size() - 4800)) > 0.01, "the chord sounds");
  Events panic; panic.push(midi_event(0xb0, 123, 0));
  const auto silenced = collect(runner, 200, &panic);
  THEN("nothing is left sounding");
  CHECK(measure::peak(Span(silenced, silenced.size() - 6000)) < 1e-3, "all notes off");
  runner.destroy();
}

SCENARIO(events_are_applied_at_their_sample_offset) {
  GIVEN("a note on scheduled 200 samples into a 256-sample block, on the trumpet");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  set_param(plugin, P::kInstrument, P::kTrumpet);
  Runner runner(plugin, kRate, 256);
  Events events;
  auto on = note_event(CLAP_EVENT_NOTE_ON, 0, 72, -1, 1.0);
  on.header.time = 200;
  events.push(on);
  std::vector<float> out;
  for (int b = 0; b < 8; ++b) {
    runner.run(b == 0 ? &events : nullptr);
    out.insert(out.end(), runner.left().begin(), runner.left().end());
  }
  THEN("the first 200 samples are exactly silent and the breath starts right at the event");
  bool silent_before = true;
  for (uint32_t i = 0; i < 200; ++i) silent_before = silent_before && out[i] == 0.0f;
  size_t first = out.size();
  for (size_t i = 0; i < out.size(); ++i) if (std::abs(out[i]) > 1e-6f) { first = i; break; }
  std::printf("    first sound at sample %zu\n", first);
  CHECK(silent_before, "silent before the event");
  CHECK(first >= 200 && first < 200 + 48, spec::fmt("first sound at sample %.0f (the event is at 200)", static_cast<double>(first)));
  runner.destroy();
}

SCENARIO(parameter_automation_changes_the_sound_and_flush_works_offline) {
  GIVEN("the master level set by a parameter event inside process, then by flush, on a held clarinet note");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  set_param(plugin, P::kInstrument, P::kClarinet);
  set_param(plugin, P::kVibrato, 0.0);
  Runner runner(plugin, kRate, 256);
  Events on; on.push(note_event(CLAP_EVENT_NOTE_ON, 0, 60, -1, 1.0)); on.push(param_event(P::kMasterDb, 0.0));
  const auto loud = collect(runner, 120, &on);
  Events quieter; quieter.push(param_event(P::kMasterDb, -24.0));
  const auto quiet = collect(runner, 120, &quieter);
  THEN("the level drops by about 24 dB");
  const double drop = measure::db(measure::rms(Span(loud, 20000, 8000))) - measure::db(measure::rms(Span(quiet, 20000, 8000)));
  CHECK_NEAR(drop, 24.0, 2.0, "automation applied");
  CHECK(param(plugin, P::kMasterDb) == -24.0, "value readable through the params extension");
  WHEN("a parameter arrives through flush");
  set_param(plugin, P::kMasterDb, -6.0);
  THEN("the plug-in reports it");
  CHECK(param(plugin, P::kMasterDb) == -6.0, "flush applied");
  WHEN("an out-of-range value arrives");
  set_param(plugin, P::kMasterDb, 500.0);
  THEN("it is clamped to the declared range");
  CHECK(param(plugin, P::kMasterDb) == P::spec(P::kMasterDb).max, "clamped");
  runner.destroy();
}

SCENARIO(choosing_a_preset_rewrites_the_other_parameters) {
  GIVEN("the preset parameter set to 'Klezmer clarinet'");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  double index = -1.0;
  CHECK(params_of(plugin)->text_to_value(plugin, P::kPreset, "Klezmer clarinet", &index), "preset found by name");
  set_param(plugin, P::kPreset, index);
  THEN("the instrument becomes the clarinet and the preset's room and vibrato are set");
  CHECK(param(plugin, P::kInstrument) == static_cast<double>(P::kClarinet), "instrument switched");
  CHECK(param(plugin, P::kRoomSize) > 0.0, "preset room applied");
  CHECK(param(plugin, P::kVibrato) == 25.0, "preset vibrato applied");
  char text[64]{};
  params_of(plugin)->value_to_text(plugin, P::kPreset, index, text, sizeof(text));
  CHECK(std::string(text) == "Klezmer clarinet", "preset name formats back");
  WHEN("a parameter is edited afterwards and the same preset index is sent again");
  set_param(plugin, P::kMicDistance, 0.9);
  set_param(plugin, P::kPreset, index);
  THEN("the edit survives (re-sending the current preset is not a reload)");
  CHECK(param(plugin, P::kMicDistance) == 0.9, "edit kept");
  plugin->destroy(plugin);
}

SCENARIO(every_instrument_sounds_through_the_plugin) {
  GIVEN("each instrument selected by parameter, with one note (C4, folded into the compass where it lies outside)");
  Library lib(g_path);
  bool all = true;
  for (int instrument = 0; instrument < P::kInstrumentCount; ++instrument) {
    const clap_plugin_t* plugin = lib.create();
    set_param(plugin, P::kInstrument, instrument);
    Runner runner(plugin, kRate, 256);
    Events on; on.push(note_event(CLAP_EVENT_NOTE_ON, 0, 60, -1, 0.8));
    const auto out = collect(runner, 100, &on);
    bool finite = true;
    for (float v : out) finite = finite && std::isfinite(v);
    // Low brass takes a while to speak: judge the second half of the note.
    const double rms = measure::rms(Span(out, out.size() / 2));
    if (!finite || rms < 1e-3) { all = false; std::printf("    instrument %d (%s): rms %.5f finite %d\n", instrument, P::kInstrumentNames[instrument], rms, finite); }
    runner.destroy();
  }
  THEN("all twenty-seven are audible and finite");
  CHECK(all, "every instrument sounds");
}

SCENARIO(tail_reports_longer_in_a_church) {
  GIVEN("the tail extension on the instrument alone, then in a church with the microphones 5 m away");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  const auto* tail = static_cast<const clap_plugin_tail_t*>(plugin->get_extension(plugin, CLAP_EXT_TAIL));
  Runner runner(plugin, kRate, 256);
  runner.run();
  const uint32_t dry = tail->get(plugin);
  set_param(plugin, P::kRoomSize, 6); set_param(plugin, P::kMicDistance, 5.0);
  runner.run();
  const uint32_t wet = tail->get(plugin);
  THEN("the wet tail is longer and both are sane");
  CHECK(dry > static_cast<uint32_t>(0.07 * kRate) && dry < 48000 * 10, spec::fmt("dry tail %.2f s covers the 70 ms release plus a margin", dry / kRate));
  CHECK(wet > dry * 2, spec::fmt("wet tail %.1f s exceeds dry %.1f s", wet / kRate, dry / kRate));
  runner.destroy();
}

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: note_tests <HONKR.clap> [filter]\n"); return 2; }
  g_path = argv[1];
  return spec::run(argc, argv, 2);
}
