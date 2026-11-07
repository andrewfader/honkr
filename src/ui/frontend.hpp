// The plug-in's side of the editor: an EditorHost that reads and writes Plugin parameters, the
// help text and relevance rules, and the per-page artwork. Included inside the plug-in's
// anonymous namespace so it can see Plugin. Layout, widgets and input live in editor.hpp.
#pragma once

#include <array>
#include <cmath>
#include <vector>

#include "canvas.hpp"
#include "editor.hpp"
#include "layout.hpp"
#include "pages.hpp"

// Whether a control does anything for the current instrument and settings. Irrelevant controls
// stay where they are but are dimmed, so the layout never jumps when the instrument changes.
bool gui_param_relevant(const Plugin* p, clap_id id) {
  const auto value = [p](clap_id param) { return p->params[param].load(std::memory_order_relaxed); };
  const int instrument = static_cast<int>(value(kInstrument));
  if (!honkr::ui::param_relevant_for_instrument(static_cast<int>(id), instrument)) return false;
  switch (id) {
    case kTemperamentRoot: return value(kTemperament) > 0.0;
    case kVibratoRate: case kVibratoDelay: return value(kVibrato) > 0.0;
    case kSpread: case kTiming: return value(kPlayers) > 1.0;
    case kHand: case kMute: return true;
    case kRoomAbsorption: return value(kRoomSize) > 0.0;
    case kMicPattern: case kMicSetup: return value(kRoomSize) > 0.0 || value(kMicDistance) > 0.0;
    default: return true;
  }
}

const char* gui_help(clap_id id) {
  switch (id) {
    case kInstrument: return "Which instrument: clarinet and bass clarinet; soprano, alto, tenor and baritone saxophones; oboe, bassoon and the Highland pipes; flute, piccolo, alto flute, recorder, tin whistle, pan flute, shakuhachi and ocarina; trumpet, flugelhorn, French horn, trombone, euphonium and tuba; bugle, alphorn and shofar, which have only their harmonic series; the didgeridoo.";
    case kMasterDb: return "Overall output level, before the limiter.";
    case kTranspose: return "Shifts every key by whole semitones; a note beyond the instrument's compass sounds an octave inside it.";
    case kPitchA4: return "The pitch of A4: 415 Hz for baroque instruments, 430 for classical ones, 440 or 442 today; a Highland pipe band's A is near 470.";
    case kTemperament: return "How the octave is divided: equal, Pythagorean (pure fifths), quarter-comma meantone (pure thirds, a wolf), Werckmeister III, Kirnberger III or Vallotti.";
    case kTemperamentRoot: return "The key the temperament is laid out from; A always sits at the reference pitch.";
    case kFineTune: return "Moves the whole instrument up or down by cents.";
    case kPlayMode: return "Solo: one player, one note at a time; a key pressed while another is held is slurred, without a new tongue. Section: a player for every key held, for chords.";
    case kGlide: return "How long a slur takes to reach the next note: a trombone's slide, a clarinettist's smear. The keys, valves or slide take their own few milliseconds whatever this is.";
    case kBendRange: return "How far the pitch-bend wheel bends, in semitones.";
    case kBreathSource: return "Where the breath comes from: the key's velocity, or a controller held for the note's whole length (a breath controller on CC 2, an expression pedal on CC 11, the mod wheel, or key pressure). A controller takes over once it has moved.";
    case kBreath: return "How hard the player blows against the instrument's own pressure: under its threshold the reed, lips or jet will not speak; far over, reeds choke, flutes overblow and brass blare.";
    case kDynamics: return "How much the key's velocity reaches the breath: at none every note is mezzo-forte.";
    case kAttack: return "How long the breath takes to build behind the tongue: a few milliseconds for a hard attack, hundreds for a swell.";
    case kRelease: return "How long the breath takes to stop when the key is let go.";
    case kTongue: return "How hard the tongue releases the note: the burst of pressure and the consonant at the start.";
    case kBreathNoise: return "The turbulence of the breath against the instrument's own: the air in a breathy flute or a subtone sax.";
    case kGrowl: return "Growling or flutter-tonguing: the throat or tongue rattling the air at about 25 to 35 Hz.";
    case kReed: return "The reed's strength. On a clarinet a soft reed closes readily and buzzes brightly, a hard one is darker. On the saxophones and double reeds a soft reed speaks easily but dully and, softest of all, will not speak; a hard one is edgier and brighter.";
    case kEmbouchure: return "Flutes: where the jet strikes the edge and how far it travels; toward the top the jet is short enough to overblow the pipe to its upper modes. Brass: how firm the lips are: loose lips open wide and sound brassier, a firm embouchure narrows the aperture to a darker, more focused tone.";
    case kLipTension: return "Where the player sets the lips against the note, in cents: above brightens and pulls sharp, far enough and the horn cracks to the next partial; below darkens and flattens.";
    case kLipping: return "How far the player bends the horn's own intonation to the tempered pitch: at none you hear the harmonic series (the 5th partial 14 cents flat, the 7th 31) and the valves' compromise (1+3 about 30 cents sharp).";
    case kHand: return "The player's hand in a horn's bell, or a plunger over a trumpet's or trombone's: darker as it closes. Automate it for a wah.";
    case kMute: return "A mute in the bell: straight (bright, nasal), cup (soft, covered), Harmon (the thin, buzzing jazz mute), bucket (velvety).";
    case kKeyNoise: return "The pads and valves: how loudly they click as the fingers move.";
    case kThroat: return "Didgeridoo: the vowel the player's throat shapes over the drone, from oo through ah to ee.";
    case kDrones: return "Highland pipes: the bass drone and two tenors, sounding while the bag is squeezed.";
    case kVibrato: return "How deep the vibrato is, in cents: the jaw on a reed, the lips on brass, the breath on a flute (where it is mostly loudness).";
    case kVibratoRate: return "How fast the vibrato goes.";
    case kVibratoDelay: return "How long into a note the vibrato waits before it begins (then fades in over a quarter second).";
    case kPlayers: return "How many players double each note: a section in unison, each a few cents and milliseconds apart.";
    case kSpread: return "How far apart the section's players are tuned, in cents.";
    case kTiming: return "How ragged the section's entries are: each player after the first comes in at a random moment up to this much later.";
    case kStereoWidth: return "How wide the section sits across the stereo picture.";
    case kPan: return "Where the player (or the middle of the section) sits, left to right.";
    case kRoomSize: return "The room the instrument plays in, from a vocal booth to a church, or none.";
    case kRoomAbsorption: return "How much the walls absorb: a stone church at a few percent, a curtained studio near the top.";
    case kMicDistance: return "How far the microphones stand from the instrument: close is dry and bright; far away the room takes over and the air dulls the top.";
    case kHumidity: return "The air's humidity: dry air absorbs the treble more over the way to the microphones.";
    case kTemperature: return "The air's temperature: the speed of sound and the air's absorption.";
    case kMicPattern: return "The microphones' pattern: omni hears the room all round, cardioid less of it, figure-8 the front and back.";
    case kMicSetup: return "A stereo pair, or one microphone in mono.";
    case kPreset: return "Factory presets: an instrument in a room, ready to play.";
    default: return nullptr;
  }
}

void gui_request_flush(Plugin* p) {
  if (p->host && p->host->get_extension)
    if (const auto* hp = static_cast<const clap_host_params_t*>(p->host->get_extension(p->host, CLAP_EXT_PARAMS)))
      if (hp->request_flush) hp->request_flush(p->host);
}

void gui_mark_state(Plugin* p) {
  if (p->host && p->host->get_extension)
    if (const auto* hs = static_cast<const clap_host_state_t*>(p->host->get_extension(p->host, CLAP_EXT_STATE)))
      if (hs->mark_dirty) hs->mark_dirty(p->host);
}

class PluginEditorHost final : public honkr::ui::EditorHost {
 public:
  explicit PluginEditorHost(Plugin* p) : p_(p) {}

  int param_count() const override { return static_cast<int>(kParamCount); }
  honkr::ui::ParamInfo info(int id) const override {
    const auto& s = kSpecs[static_cast<size_t>(id)];
    return {s.name, s.min, s.max, s.def, s.stepped};
  }
  double value(int id) const override { return p_->params[static_cast<size_t>(id)].load(std::memory_order_relaxed); }
  void begin_edit(int id) override { gui_push(p_, GuiEvent::Begin, static_cast<clap_id>(id), 0); gui_request_flush(p_); }
  void edit(int id, double v) override {
    if (static_cast<clap_id>(id) == kPreset) apply_preset(p_, static_cast<int>(std::lround(v)));
    else set_param(p_, static_cast<clap_id>(id), v);
    snapshot_settings(p_);
    gui_push(p_, GuiEvent::Value, static_cast<clap_id>(id), value(id));
    gui_request_flush(p_);
  }
  void end_edit(int id) override { gui_push(p_, GuiEvent::End, static_cast<clap_id>(id), 0); gui_request_flush(p_); gui_mark_state(p_); }
  void format(int id, double v, char* out, size_t capacity) const override {
    format_value(static_cast<uint32_t>(id), v, out, static_cast<uint32_t>(capacity));
  }
  const char* help(int id) const override { return gui_help(static_cast<clap_id>(id)); }
  const char* short_label(int id) const override {
    if (const char* name = honkr::ui::honkr_short_name(id)) return name;
    return label(id);
  }
  bool relevant(int id) const override { return gui_param_relevant(p_, static_cast<clap_id>(id)); }

  int size_percent() const override {
    return static_cast<int>(std::lround(100.0 * honkr::ui::uniform_scale(static_cast<int>(p_->gui_width), static_cast<int>(p_->gui_height))));
  }
  void request_size(int percent) override {
    p_->gui_request_width = static_cast<uint32_t>(honkr::ui::width * percent / 100);
    p_->gui_request_height = static_cast<uint32_t>(honkr::ui::height * percent / 100);
  }

  const char* strip_help(int page) const override {
    if (page == 0) return "The instrument's compass on a keyboard, keys lit while they sound, with the note and how it is fingered; the output scope held still on each cycle and amplified to fill the frame (gain shown).";
    if (page == 1) return "The breath: the pressure the player blows, against the instrument's pianissimo and fortissimo, with the vibrato's shape over a note.";
    if (page == 2) return "The mouthpiece: the reed against the lay, the lips, or the jet over the edge, opening and closing as the instrument plays.";
    if (page == 3) return "The section from above: each player where they sit.";
    if (page == 4) return "The room from above: the instrument, the microphones at their distance.";
    return nullptr;
  }

  void draw_meters(honkr::ui::Painter& g, const honkr::ui::Rect& r) override {
    using namespace honkr::ui::theme;
    const float peak_l = p_->peak_l.load(std::memory_order_relaxed), peak_r = p_->peak_r.load(std::memory_order_relaxed);
    const bool clipped = p_->clipped.load(std::memory_order_relaxed);
    g.text(r.x, r.y + 24, "OUT", muted, 50, honkr::ui::TextSize::Small);
    const int bar_x = r.x + 52, bar_w = 230;
    g.rect(bar_x, r.y + 6, bar_w, 9, border);
    g.rect(bar_x, r.y + 20, bar_w, 9, border);
    const uint32_t level = clipped ? red : green;
    g.rect(bar_x, r.y + 6, static_cast<int>(bar_w * std::clamp(peak_l, 0.0f, 1.0f)), 9, level);
    g.rect(bar_x, r.y + 20, static_cast<int>(bar_w * std::clamp(peak_r, 0.0f, 1.0f)), 9, level);
    char voices[32]{};
    std::snprintf(voices, sizeof(voices), clipped ? "CLIP" : "%d SOUNDING", p_->active_voices.load(std::memory_order_relaxed));
    g.text(bar_x + bar_w + 18, r.y + 24, voices, clipped ? red : muted, r.right() - bar_x - bar_w - 18, honkr::ui::TextSize::Small);
  }

  void draw_strip(int page, honkr::ui::Painter& g, const honkr::ui::Rect& r) override {
    honkr::ui::LiveState live;
    const uint32_t write = p_->scope_write.load(std::memory_order_acquire);
    float energy = 0.0f;
    for (size_t i = 0; i < live.scope.size(); ++i) {
      live.scope[i] = p_->scope[(write + static_cast<uint32_t>(i)) & (Plugin::kScopeSize - 1)].load(std::memory_order_relaxed);
      energy += live.scope[i] * live.scope[i];
    }
    live.energy = std::sqrt(energy / static_cast<float>(live.scope.size()));
    live.sample_rate = p_->sample_rate;
    for (int k = 0; k < 128; ++k) live.sounding[static_cast<size_t>(k)] = p_->sounding[static_cast<size_t>(k)].load(std::memory_order_relaxed);
    live.voices = p_->active_voices.load(std::memory_order_relaxed);
    live.key = p_->live_key.load(std::memory_order_relaxed);
    live.partial = p_->live_partial.load(std::memory_order_relaxed);
    live.valve_mask = p_->live_mask.load(std::memory_order_relaxed);
    live.pressure = p_->live_pressure.load(std::memory_order_relaxed);
    live.opening = p_->live_opening.load(std::memory_order_relaxed);
    honkr::ui::Artwork(*this, live).draw(page, g, r);
  }

  bool strip_pointer(int page, int button, int x, int y, const honkr::ui::Rect& r) override {
    (void)page; (void)button; (void)x; (void)y; (void)r;
    return false;
  }

 private:
  Plugin* p_;
};
static_assert(Plugin::kScopeSize == honkr::ui::kScopeSamples, "the scope ring and the artwork agree on its length");

honkr::ui::Editor& gui_editor(Plugin* p) {
  if (!p->editor) {
    p->editor_host = std::make_unique<PluginEditorHost>(p);
    p->editor = std::make_unique<honkr::ui::Editor>(*p->editor_host, honkr::ui::honkr_pages(), honkr::ui::honkr_header_control());
  }
  return *p->editor;
}

void gui_draw(Plugin* p, honkr::ui::Canvas& canvas) { gui_editor(p).draw(canvas); }

using GuiPointer = honkr::ui::PointerAction;

bool gui_set_size(const clap_plugin_t* plugin, uint32_t w, uint32_t h);

// Asks the host to resize its editor frame; only once it agrees does the editor window follow.
void gui_apply_requested_size(Plugin* p) {
  const uint32_t w = p->gui_request_width, h = p->gui_request_height;
  p->gui_request_width = p->gui_request_height = 0;
  if (!w || !h || (w == p->gui_width && h == p->gui_height) || !p->host || !p->host->get_extension) return;
  const auto* host_gui = static_cast<const clap_host_gui_t*>(p->host->get_extension(p->host, CLAP_EXT_GUI));
  if (!host_gui || !host_gui->request_resize || !host_gui->request_resize(p->host, w, h)) return;
  gui_set_size(&p->api, w, h);
  gui_mark_state(p);
}

void gui_input(Plugin* p, GuiPointer action, int button, int x, int y, unsigned modifiers = 0) {
  gui_editor(p).pointer(action, button, x, y, modifiers, honkr::ui::now_ms());
  gui_apply_requested_size(p);
}

// True when the editor needs a repaint for its own animations.
bool gui_tick(Plugin* p) { return gui_editor(p).tick(honkr::ui::now_ms()); }

[[maybe_unused]] int gui_size_percent_for_test(Plugin* p) { gui_editor(p); return p->editor_host->size_percent(); }
