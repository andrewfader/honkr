// Executable scenarios for features/editor.feature (plug-in side): the editor host that sits
// between the editor and the plug-in, driven without a window. It compiles the plug-in source
// directly so it can reach the host, the gesture queue, the relevance rules, the help text and
// the page artwork, and it feeds them a real, processing plug-in instance. Linux only, because
// the editor backend is selected at compile time; no display is ever opened.
#include "../src/honkr.cpp"
#include "clap_harness.hpp"
#include "spec.hpp"

#include <string>
#include <vector>

namespace {

// A host that counts what the editor asks of it.
int g_flushes = 0, g_dirty = 0, g_callbacks = 0;
void host_request_flush(const clap_host_t*) { ++g_flushes; }
void host_rescan(const clap_host_t*, clap_param_rescan_flags) {}
void host_clear(const clap_host_t*, clap_id, clap_param_clear_flags) {}
const clap_host_params_t g_host_params{host_rescan, host_clear, host_request_flush};
void host_mark_dirty(const clap_host_t*) { ++g_dirty; }
const clap_host_state_t g_host_state{host_mark_dirty};
const void* host_extension(const clap_host_t*, const char* id) {
  if (!std::strcmp(id, CLAP_EXT_PARAMS)) return &g_host_params;
  if (!std::strcmp(id, CLAP_EXT_STATE)) return &g_host_state;
  return nullptr;
}
void host_request_restart(const clap_host_t*) {}
void host_request_process(const clap_host_t*) {}
void host_request_callback(const clap_host_t*) { ++g_callbacks; }
const clap_host_t g_host{CLAP_VERSION_INIT, nullptr, "frontend test host", "HONKR", "", "1",
                         host_extension, host_request_restart, host_request_process, host_request_callback};

// Collects the events the plug-in pushes to the host.
struct Collected { std::vector<uint16_t> types; std::vector<std::pair<clap_id, double>> values; };
bool collect_push(const clap_output_events_t* list, const clap_event_header_t* ev) {
  auto* c = static_cast<Collected*>(list->ctx);
  c->types.push_back(ev->type);
  if (ev->type == CLAP_EVENT_PARAM_VALUE) {
    const auto* v = reinterpret_cast<const clap_event_param_value_t*>(ev);
    c->values.emplace_back(v->param_id, v->value);
  }
  return true;
}

class RecordingCanvas final : public honkr::ui::Canvas {
 public:
  int shapes = 0, texts = 0, outside = 0;
  std::vector<std::string> strings;
  void clear(uint32_t) override {}
  void fill_rect(int x, int y, int w, int h, uint32_t) override { ++shapes; check(x, y); check(x + w, y + h); }
  void draw_polyline(const honkr::ui::Point* p, int n, uint32_t, int) override { ++shapes; for (int i = 0; i < n; ++i) check(p[i].x, p[i].y); }
  void fill_circle(int x, int y, int d, uint32_t) override { ++shapes; check(x, y); check(x + d, y + d); }
 protected:
  void draw_glyphs(int x, int y, const std::string& s, uint32_t, honkr::ui::TextSize) override { ++texts; strings.push_back(s); check(x, y); }
  int measure_text(const std::string& s, honkr::ui::TextSize size) override { return static_cast<int>(s.size()) * honkr::ui::text_pixels(size) * 55 / 100; }
 private:
  void check(int x, int y) { if (x < -2 || x > honkr::ui::width + 16 || y < -16 || y > honkr::ui::height + 16) ++outside; }
};

struct Rig {
  const clap_plugin_t* plugin;
  Plugin* p;
  std::vector<float> left = std::vector<float>(256), right = std::vector<float>(256);
  Rig() {
    g_flushes = g_dirty = g_callbacks = 0;
    plugin = create_plugin(&g_host);
    p = self(plugin);
    plugin->init(plugin);
    plugin->activate(plugin, 48000.0, 1, 256);
    plugin->start_processing(plugin);
  }
  ~Rig() { plugin->stop_processing(plugin); plugin->deactivate(plugin); plugin->destroy(plugin); }
  void process(int blocks, int key = -1) {
    harness::Events events;
    if (key >= 0) events.push(harness::note_event(CLAP_EVENT_NOTE_ON, 0, static_cast<int16_t>(key), -1, 1.0));
    for (int b = 0; b < blocks; ++b) {
      clap_input_events_t in = events.list();
      float* channels[2] = {left.data(), right.data()};
      clap_audio_buffer_t out{}; out.data32 = channels; out.channel_count = 2;
      clap_process_t pr{}; pr.frames_count = 256; pr.in_events = &in; pr.audio_outputs = &out; pr.audio_outputs_count = 1;
      plugin->process(plugin, &pr);
      events.clear();
    }
  }
  PluginEditorHost& host() { gui_editor(p); return *static_cast<PluginEditorHost*>(p->editor_host.get()); }
};

}  // namespace

SCENARIO(the_host_describes_every_parameter_with_help_and_labels) {
  GIVEN("the plug-in's editor host");
  Rig rig;
  PluginEditorHost& host = rig.host();
  THEN("every parameter has matching info, a label, a short label, help, and formats its default");
  bool ok = host.param_count() == static_cast<int>(kParamCount);
  for (int id = 0; id < static_cast<int>(kParamCount); ++id) {
    const auto info = host.info(id);
    const auto& s = kSpecs[static_cast<size_t>(id)];
    ok = ok && info.min == s.min && info.max == s.max && info.def == s.def && info.stepped == s.stepped;
    ok = ok && host.label(id) && *host.label(id) && host.short_label(id) && *host.short_label(id);
    ok = ok && host.help(id) && std::strlen(host.help(id)) > 10;
    char text[64]{};
    host.format(id, s.def, text, sizeof(text));
    ok = ok && text[0] != '\0';
    ok = ok && host.value(id) == s.def;
  }
  CHECK(ok, "every parameter described");
  CHECK(std::string(host.help(kLipping)).find("31") != std::string::npos, "help text names the physics (the 7th partial 31 cents flat)");
}

SCENARIO(relevance_follows_the_selected_instrument_and_switches) {
  GIVEN("the alto sax selected");
  Rig rig;
  PluginEditorHost& host = rig.host();
  CHECK(param_value(rig.p, kInstrument) == static_cast<double>(kAltoSax), "the alto sax is the default");
  THEN("the reed and the keys are live, the lips, mute, hand, drones and throat are dimmed, and dependent controls follow their switches");
  CHECK(host.relevant(kReed) && host.relevant(kKeyNoise) && host.relevant(kTongue) && host.relevant(kBreath), "alto sax controls relevant");
  CHECK(!host.relevant(kLipTension) && !host.relevant(kLipping) && !host.relevant(kMute) && !host.relevant(kHand) && !host.relevant(kDrones) && !host.relevant(kThroat) && !host.relevant(kEmbouchure),
        "other instruments' controls dimmed");
  CHECK(!host.relevant(kTemperamentRoot), "the key waits for a temperament");
  set_param(rig.p, kTemperament, 2.0);
  CHECK(host.relevant(kTemperamentRoot), "the key is live in meantone");
  CHECK(!host.relevant(kRoomAbsorption), "absorption dims with no room");
  set_param(rig.p, kRoomSize, 3.0);
  CHECK(host.relevant(kRoomAbsorption), "absorption lights with a room");
  CHECK(host.relevant(kVibratoRate) && host.relevant(kVibratoDelay), "the vibrato's rate and delay are live with the default vibrato");
  set_param(rig.p, kVibrato, 0.0);
  CHECK(!host.relevant(kVibratoRate) && !host.relevant(kVibratoDelay), "and dim without vibrato");
  CHECK(!host.relevant(kSpread) && !host.relevant(kTiming), "spread and timing wait for a second player");
  set_param(rig.p, kPlayers, 3.0);
  CHECK(host.relevant(kSpread) && host.relevant(kTiming), "live with three players");
  WHEN("the trumpet is selected");
  set_param(rig.p, kInstrument, kTrumpet);
  THEN("the lips, lipping, mute and hand light up and the reed dims");
  CHECK(host.relevant(kLipTension) && host.relevant(kLipping) && host.relevant(kMute) && host.relevant(kHand) && host.relevant(kEmbouchure) && host.relevant(kKeyNoise), "brass controls live");
  CHECK(!host.relevant(kReed) && !host.relevant(kDrones), "the reed and drones dimmed");
  WHEN("the Highland pipes are selected");
  set_param(rig.p, kInstrument, kBagpipe);
  THEN("the drones are live and the tongue dimmed");
  CHECK(host.relevant(kDrones) && host.relevant(kReed) && !host.relevant(kTongue) && !host.relevant(kMute), "pipes");
  WHEN("the flute is selected");
  set_param(rig.p, kInstrument, kFlute);
  THEN("the embouchure is live and the reed and lips are dimmed");
  CHECK(host.relevant(kEmbouchure) && !host.relevant(kReed) && !host.relevant(kLipTension) && !host.relevant(kMute), "flute");
  bool every_instrument_has_live_controls = true;
  for (int instrument = 0; instrument < kInstrumentCount; ++instrument) {
    set_param(rig.p, kInstrument, instrument);
    int live = 0;
    for (int id = 0; id < static_cast<int>(kParamCount); ++id) live += host.relevant(id);
    every_instrument_has_live_controls = every_instrument_has_live_controls && live > 30;
  }
  THEN("no instrument dims the whole editor");
  CHECK(every_instrument_has_live_controls, "no instrument dims the whole editor");
}

SCENARIO(editor_gestures_reach_the_host_as_clap_events) {
  GIVEN("a drag of the master level through the editor host");
  Rig rig;
  PluginEditorHost& host = rig.host();
  host.begin_edit(kMasterDb);
  host.edit(kMasterDb, -12.0);
  host.edit(kMasterDb, -9.0);
  host.end_edit(kMasterDb);
  THEN("the plug-in holds the value, asked for a flush, and marked the state dirty");
  CHECK(param_value(rig.p, kMasterDb) == -9.0, "value applied");
  CHECK(g_flushes >= 3 && g_dirty >= 1, "host asked to flush and mark dirty");
  WHEN("the host flushes");
  Collected collected;
  clap_output_events_t out{&collected, collect_push};
  kParams.flush(rig.plugin, nullptr, &out);
  THEN("it receives begin, two values and end, in order");
  CHECK(collected.types.size() == 4 && collected.types[0] == CLAP_EVENT_PARAM_GESTURE_BEGIN && collected.types[3] == CLAP_EVENT_PARAM_GESTURE_END, "gesture bracketed");
  CHECK(collected.values.size() == 2 && collected.values[1] == std::make_pair(static_cast<clap_id>(kMasterDb), -9.0), "values in order");
  WHEN("the preset 'Concert flute' is chosen through the editor");
  double flute = -1.0;
  CHECK(parse_value(kPreset, "Concert flute", &flute), "preset found by name");
  host.begin_edit(kPreset); host.edit(kPreset, flute); host.end_edit(kPreset);
  THEN("the whole preset is applied and the host is asked to rescan on its main thread");
  CHECK(param_value(rig.p, kInstrument) == static_cast<double>(kFlute) && param_value(rig.p, kRoomSize) == 5.0, "preset applied");
  CHECK(g_callbacks >= 1 && rig.p->params_rescan_pending.load(), "rescan requested");
  rig.plugin->on_main_thread(rig.plugin);
  CHECK(!rig.p->params_rescan_pending.load(), "rescan delivered");
  WHEN("the queue overflows");
  for (int i = 0; i < 2000; ++i) host.edit(kMasterDb, -6.0);
  Collected overflow;
  clap_output_events_t out2{&overflow, collect_push};
  kParams.flush(rig.plugin, nullptr, &out2);
  THEN("events are dropped rather than the editor blocking, and the value is still right");
  CHECK(overflow.types.size() < 1100 && param_value(rig.p, kMasterDb) == -6.0, "bounded queue");
}

SCENARIO(page_artwork_draws_from_the_running_plugin) {
  GIVEN("a plug-in that has rendered a note");
  Rig rig;
  rig.process(20, 60);
  PluginEditorHost& host = rig.host();
  THEN("every page's strip and the meters draw inside the editor, the keyboard strip names the alto sax and lights the sounding key, and the fingering names the note");
  bool inside = true, drew = true;
  std::vector<std::string> all;
  for (int page = 0; page < honkr::ui::tab_count; ++page) {
    RecordingCanvas canvas;
    honkr::ui::Painter painter(canvas);
    const honkr::ui::Rect strip{honkr::ui::margin, honkr::ui::strip_y, honkr::ui::width - 2 * honkr::ui::margin, 168};
    host.draw_strip(page, painter, strip);
    host.draw_meters(painter, honkr::ui::meter_rect);
    inside = inside && canvas.outside == 0;
    drew = drew && canvas.shapes > 0 && canvas.texts > 0;
    all.insert(all.end(), canvas.strings.begin(), canvas.strings.end());
  }
  CHECK(inside, "inside the editor");
  CHECK(drew, "every artwork page draws");
  bool named = false, voices = false, fingered = false;
  for (const std::string& s : all) {
    named = named || s.find("Alto sax") != std::string::npos;
    voices = voices || s.find("1 SOUNDING") != std::string::npos;
    fingered = fingered || s == "C4";
  }
  CHECK(named, "the keyboard strip names the instrument");
  CHECK(voices, "meters show how many sound");
  CHECK(fingered, "the fingering names the sounding note");
  CHECK(rig.p->sounding[60].load() && rig.p->live_key.load() == 60 && rig.p->live_pressure.load() > 0.0f, "the sounding key and the breath are marked for the picture");
  bool helped = true;
  for (int page = 0; page < honkr::ui::tab_count; ++page) helped = helped && host.strip_help(page) != nullptr;
  CHECK(helped && host.strip_help(honkr::ui::tab_count) == nullptr, "strip help for every page's strip");
  CHECK(!host.strip_pointer(0, 1, 10, 10, {0, 0, 10, 10}), "the strips take no clicks");
}

SCENARIO(the_plugins_scope_follows_its_output) {
  GIVEN("a plug-in that has rendered 40 blocks of a held note");
  Rig rig;
  rig.process(40, 60);
  THEN("the scope ring holds the last 4096 samples at full rate, exactly what the plug-in output");
  const uint32_t write = rig.p->scope_write.load();
  bool same = true;
  for (uint32_t i = 0; i < 256; ++i)
    same = same && rig.p->scope[(write + Plugin::kScopeSize - 256 + i) & (Plugin::kScopeSize - 1)].load() == 0.5f * (rig.left[i] + rig.right[i]);
  CHECK(same, "the newest block, sample for sample");
}

SCENARIO(the_whole_editor_draws_and_ticks_without_a_window) {
  GIVEN("the plug-in's editor, created on demand");
  Rig rig;
  honkr::ui::Editor& editor = gui_editor(rig.p);
  CHECK(&editor == &gui_editor(rig.p), "created once");
  THEN("it draws every page from live plug-in state and takes pointer input");
  bool inside = true;
  for (int page = 0; page < editor.page_count(); ++page) {
    editor.set_page(page);
    for (int i = 0; i < 30; ++i) gui_tick(rig.p);
    RecordingCanvas canvas;
    gui_draw(rig.p, canvas);
    inside = inside && canvas.outside == 0 && canvas.texts > 10;
  }
  CHECK(inside, "every page drawn");
  editor.set_page(0);
  for (int i = 0; i < 30; ++i) gui_tick(rig.p);
  const auto layout = editor.layout();
  int master_param = -1;
  honkr::ui::Rect master{};
  for (const auto& c : layout.controls) {
    const auto& spec = editor.page_spec(0).sections[static_cast<size_t>(c.section)].controls[static_cast<size_t>(c.control)];
    if (spec.param == kMasterDb) { master_param = spec.param; master = c.rect; }
  }
  CHECK(master_param == kMasterDb, "master knob on page 0");
  const double before = param_value(rig.p, kMasterDb);
  gui_input(rig.p, GuiPointer::Down, 1, master.x + master.w / 2, master.y + 60);
  gui_input(rig.p, GuiPointer::Move, 1, master.x + master.w / 2, master.y + 10);
  gui_input(rig.p, GuiPointer::Up, 1, master.x + master.w / 2, master.y + 10);
  CHECK(param_value(rig.p, kMasterDb) > before, "a drag through gui_input raised the level");
  WHEN("the editor asks for a size the host cannot grant");
  rig.host().request_size(75);
  gui_input(rig.p, GuiPointer::Move, 0, 1, 1);
  THEN("the window size stays as it was");
  CHECK(rig.p->gui_width == static_cast<uint32_t>(honkr::ui::width) && rig.p->gui_request_width == 0, "size request declined cleanly");
  CHECK(gui_size_percent_for_test(rig.p) == 100, "reported at 100%");
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
