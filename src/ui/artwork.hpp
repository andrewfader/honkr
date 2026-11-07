// The per-page artwork above the cards: the keyboard with its sounding keys, the fingering and
// the output scope; the breath and vibrato; the mouthpiece and the valves or slide; the section;
// the room. Drawn from
// parameter values plus a LiveState snapshot, so the plug-in and the offscreen preview tool
// render exactly the same pictures.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../dsp/core.hpp"
#include "../dsp/winds.hpp"
#include "../params.hpp"
#include "editor.hpp"
#include "layout.hpp"

namespace honkr::ui {

// How much of the output the plug-in keeps for the scope: every sample, so the scope shows the
// real waveform rather than an aliased decimation of it.
constexpr size_t kScopeSamples = 4096;

// What the artwork needs from the running instrument.
struct LiveState {
  std::array<float, kScopeSamples> scope{};  // recent output samples, oldest first
  double sample_rate = 48000.0;
  float energy = 0.0f;
  std::array<bool, 128> sounding{};
  int voices = 0;
  // The first sounding player: the note it plays, how it is fingered, the pressure in its mouth
  // and the valve's (or jet's) opening.
  int key = -1, partial = 1, valve_mask = 0;
  double pressure = 0.0, opening = 0.0;
};

// The scope's picture of the output: 20 ms starting at the latest rising zero crossing, so a
// steady note holds still, amplified so its peak fills most of the frame (up to +24 dB).
struct ScopeTrace {
  std::vector<float> points;
  double gain = 1.0;
};

inline ScopeTrace scope_trace(const std::array<float, kScopeSamples>& s, double sample_rate, int count) {
  ScopeTrace out;
  float peak = 0.0f;
  for (float x : s) peak = std::max(peak, std::abs(x));
  out.gain = peak > 1e-4f ? std::clamp(0.9 / peak, 1.0, 16.0) : 1.0;
  const size_t n = s.size();
  const size_t window = std::clamp<size_t>(static_cast<size_t>(sample_rate * 0.02), 64, n / 2);
  size_t start = n - window;
  for (size_t i = n - window; i > n - window - n / 2 && i > 0; --i)
    if (s[i - 1] < 0.0f && s[i] >= 0.0f) { start = i; break; }
  out.points.resize(static_cast<size_t>(std::max(2, count)));
  for (size_t k = 0; k < out.points.size(); ++k) {
    const double pos = static_cast<double>(start) + static_cast<double>(k) * static_cast<double>(window - 1) / static_cast<double>(out.points.size() - 1);
    const size_t i = std::min(static_cast<size_t>(pos), n - 2);
    const double frac = pos - static_cast<double>(i);
    out.points[k] = static_cast<float>(std::clamp((s[i] + (s[i + 1] - s[i]) * frac) * out.gain, -1.0, 1.0));
  }
  return out;
}

// The compass of an instrument: lowest and highest key, at concert pitch.
inline std::pair<int, int> compass(int instrument) {
  const auto& in = dsp::winds::instrument(instrument);
  return {in.low, in.high};
}

class Artwork {
 public:
  Artwork(const EditorHost& host, const LiveState& live) : host_(host), live_(live) {}

  void draw(int page, Painter& g, const Rect& r) const {
    switch (page) {
      case 0: draw_keyboard(g, r); break;
      case 1: draw_breath(g, r); break;
      case 2: draw_mouthpiece(g, r); break;
      case 3: draw_section(g, r); break;
      case 4: draw_room(g, r); break;
      default: break;
    }
  }

 private:
  double value(params::ParamId id) const { return host_.value(static_cast<int>(id)); }
  int instrument() const { return std::clamp(static_cast<int>(std::lround(value(params::kInstrument))), 0, params::kInstrumentCount - 1); }
  static const char* key_name(int key) {
    static const char* const kKeys[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"};
    return kKeys[((key % 12) + 12) % 12];
  }

  // What the fingers are doing for the sounding note: the valves, the slide, the partial.
  std::string fingering_text() const {
    if (live_.key < 0) return "";
    const int inst = instrument();
    char line[160]{};
    const int key = live_.key;
    if (params::has_valves(inst)) {
      std::string valves;
      for (int v = 0; v < 4; ++v) if (live_.valve_mask & (1 << v)) { if (!valves.empty()) valves += "+"; valves += static_cast<char>('1' + v); }
      std::snprintf(line, sizeof(line), "%s%d  •  partial %d  •  valves %s", key_name(key), key / 12 - 1, live_.partial, valves.empty() ? "open" : valves.c_str());
    } else if (inst == params::kTrombone) {
      const int drop = live_.valve_mask;
      if (drop > 6) std::snprintf(line, sizeof(line), "%s%d  •  partial %d  •  F trigger, position %d", key_name(key), key / 12 - 1, live_.partial, drop - 4);
      else std::snprintf(line, sizeof(line), "%s%d  •  partial %d  •  position %d", key_name(key), key / 12 - 1, live_.partial, drop + 1);
    } else if (params::is_natural_horn(inst)) {
      std::snprintf(line, sizeof(line), "%s%d  •  partial %d of the open horn", key_name(key), key / 12 - 1, live_.partial);
    } else {
      std::snprintf(line, sizeof(line), "%s%d", key_name(key), key / 12 - 1);
    }
    return line;
  }

  // The keyboard across the strip, the instrument's compass in ivory and ebony, keys lit while
  // they sound; the fingering of the note; the output scope below.
  void draw_keyboard(Painter& g, const Rect& r) const {
    using namespace theme;
    char name[96]{};
    params::format_value(params::kInstrument, value(params::kInstrument), name, sizeof(name));
    g.text(r.x + 20, r.y + 24, name, amber, 360, TextSize::Normal);
    const auto [low, high] = compass(instrument());
    char range[64]{};
    std::snprintf(range, sizeof(range), "%s%d to %s%d  •  %d notes", key_name(low), low / 12 - 1, key_name(high), high / 12 - 1, high - low + 1);
    g.text(r.x + 380, r.y + 24, range, muted, 360, TextSize::Small);
    const std::string fingering = fingering_text();
    if (!fingering.empty()) g.text(r.x + 760, r.y + 24, fingering.c_str(), soft, 520, TextSize::Small);
    const int first = 21, last = 108;
    int whites = 0;
    for (int k = first; k <= last; ++k) whites += !is_black(k);
    const int kx = r.x + 20, kw = r.w - 40, key_w = kw / whites, ky = r.y + 36, key_h = 72;
    int wi = 0;
    for (int k = first; k <= last; ++k) {
      if (is_black(k)) continue;
      const bool in = k >= low && k <= high, on = live_.sounding[static_cast<size_t>(k)];
      g.rect(kx + wi * key_w + 1, ky, key_w - 2, key_h, on ? amber : (in ? text : faint));
      ++wi;
    }
    wi = 0;
    for (int k = first; k <= last; ++k) {
      if (!is_black(k)) { ++wi; continue; }
      const bool in = k >= low && k <= high, on = live_.sounding[static_cast<size_t>(k)];
      g.rect(kx + wi * key_w - key_w / 3, ky, 2 * key_w / 3, key_h * 6 / 10, on ? orange : (in ? bg : border));
    }
    const int top = ky + key_h + 10, mid = top + (r.bottom() - top) / 2, amp = (r.bottom() - top) / 2 - 6;
    if (amp > 6) {
      const ScopeTrace trace = scope_trace(live_.scope, live_.sample_rate, 384);
      std::vector<Point> scope;
      const int last_point = static_cast<int>(trace.points.size()) - 1;
      for (int i = 0; i <= last_point; ++i)
        scope.push_back({kx + i * kw / last_point, mid - static_cast<int>(trace.points[static_cast<size_t>(i)] * static_cast<float>(amp))});
      g.polyline(scope, soft, 2);
      char gain[48]{};
      std::snprintf(gain, sizeof(gain), "scope \xc3\x97%.1f  (+%.0f dB)", trace.gain, 20.0 * std::log10(trace.gain));
      g.text_right(r.right() - 20, r.y + 24, gain, muted, 220, TextSize::Small);
    }
  }
  static bool is_black(int key) { const int pc = key % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

  // The breath gauge: the instrument's pianissimo and fortissimo pressures, the player's breath
  // scaled against them and the pressure blowing now; beside it a note's vibrato, its delay and
  // its fade-in.
  void draw_breath(Painter& g, const Rect& r) const {
    using namespace theme;
    const auto& in = dsp::winds::instrument(instrument());
    g.text(r.x + 20, r.y + 24, "BREATH  •  the mouth's pressure against the instrument's", muted, 560, TextSize::Small);
    const double breath = value(params::kBreath);
    const double top = 1.6 * 1.5;
    const int bx = r.x + 20, bw = 640, by = r.y + 56, bh = 26;
    g.round_rect(Rect{bx, by, bw, bh}, 6, panel_hi);
    const auto at = [&](double p) { return bx + static_cast<int>(std::clamp(p / top, 0.0, 1.0) * bw); };
    const int soft_x = at(in.pressure_soft * breath), loud_x = at(in.pressure_loud * breath);
    g.round_rect(Rect{soft_x, by, std::max(4, loud_x - soft_x), bh}, 6, green);
    const int now_x = at(live_.pressure);
    g.rect(now_x - 2, by - 8, 4, bh + 16, live_.key >= 0 ? amber : faint);
    char line[160]{};
    std::snprintf(line, sizeof(line), "pp %.2f  to  ff %.2f  •  now %.2f", in.pressure_soft * breath, in.pressure_loud * breath, live_.pressure);
    g.text(bx, by + bh + 34, line, soft, bw, TextSize::Small);
    const char* source = params::kBreathSourceNames[std::clamp(static_cast<int>(std::lround(value(params::kBreathSource))), 0, params::kBreathSourceCount - 1)];
    std::snprintf(line, sizeof(line), "breath from %s", source);
    g.text(bx, by + bh + 62, line, muted, bw, TextSize::Small);
    // The vibrato over the first two seconds of a note.
    const int vx = r.x + 720, vw = r.w - 760, vy = r.y + r.h / 2 + 8, va = r.h / 2 - 34;
    g.text(vx, r.y + 24, "VIBRATO  •  two seconds of a note", muted, 420, TextSize::Small);
    g.line(vx, vy, vx + vw, vy, border);
    const double depth = value(params::kVibrato), rate = value(params::kVibratoRate), delay = value(params::kVibratoDelay);
    std::vector<Point> curve;
    for (int i = 0; i <= 300; ++i) {
      const double t = 2.0 * i / 300.0;
      const double onset = std::clamp((t * 1000.0 - delay) / 250.0, 0.0, 1.0);
      const double v = depth / 60.0 * onset * std::sin(dsp::kTau * rate * t);
      curve.push_back({vx + i * vw / 300, vy - static_cast<int>(v * va)});
    }
    g.polyline(curve, depth > 0.0 ? orange : faint, 2);
    std::snprintf(line, sizeof(line), "\xc2\xb1%.0f cents at %.1f Hz, after %.0f ms", depth, rate, delay);
    g.text(vx, r.bottom() - 10, line, soft, vw, TextSize::Small);
  }

  // The mouthpiece in section: a reed over the lay, the lips, or the jet over the edge, opening
  // as the instrument plays; and the fingers: valves pressed, the slide's position.
  void draw_mouthpiece(Painter& g, const Rect& r) const {
    using namespace theme;
    const int inst = instrument();
    const auto& in = dsp::winds::instrument(inst);
    const double opening = std::clamp(live_.opening, -1.0, 1.5);
    const int cx = r.x + 60, cy = r.y + r.h / 2 + 10;
    if (in.acoustics.mouth == dsp::horn::Mouth::Reed) {
      g.text(r.x + 20, r.y + 24, "THE REED  •  against the lay, shut by the mouth's pressure", muted, 600, TextSize::Small);
      g.round_rect(Rect{cx, cy - 6, 420, 40}, 6, panel_hi);           // the mouthpiece's lay
      const int gap = static_cast<int>(std::clamp(opening, 0.0, 1.0) * 30.0);
      g.line(cx + 40, cy - 6 - gap, cx + 420, cy - 8, live_.key >= 0 ? amber : soft, 6);  // the reed
      char line[96]{};
      std::snprintf(line, sizeof(line), "opening %.0f%%", std::clamp(opening, 0.0, 1.0) * 100.0);
      g.text(cx + 440, cy, line, soft, 200, TextSize::Small);
    } else if (in.acoustics.mouth == dsp::horn::Mouth::Lips) {
      g.text(r.x + 20, r.y + 24, "THE LIPS  •  buzzing in the mouthpiece's cup", muted, 640, TextSize::Small);
      const int gap = static_cast<int>(std::clamp(opening, 0.0, 1.5) * 16.0);
      g.round_rect(Rect{cx, cy - 34 - gap, 220, 30}, 14, live_.key >= 0 ? orange : soft);
      g.round_rect(Rect{cx, cy + 4 + gap, 220, 30}, 14, live_.key >= 0 ? orange : soft);
      g.round_rect(Rect{cx + 240, cy - 50, 60, 100}, 10, panel_hi);   // the cup
    } else {
      g.text(r.x + 20, r.y + 24, in.acoustics.mouth == dsp::horn::Mouth::Vessel ? "THE JET  •  over the vessel's mouth" : "THE JET  •  across the embouchure hole, onto the edge", muted, 640, TextSize::Small);
      g.rect(cx + 200, cy - 40, 200, 12, panel_hi);                   // the edge
      const int deflect = static_cast<int>(std::clamp(opening, -1.0, 1.0) * 22.0);
      g.line(cx, cy - 34, cx + 190, cy - 34 + deflect, live_.key >= 0 ? cyan : soft, 5);
    }
    // The fingers.
    const int fx = r.x + 720;
    if (params::has_valves(inst)) {
      const int valves = in.valves;
      g.text(fx, r.y + 24, "VALVES", muted, 200, TextSize::Small);
      for (int v = 0; v < valves; ++v) {
        const bool down = live_.key >= 0 && (live_.valve_mask & (1 << v));
        // The valve numbers sit under the title, the caps under them, pressed caps further down.
        g.round_rect(Rect{fx + v * 90, r.y + 62 + (down ? 20 : 0), 60, r.h - 104}, 10, down ? amber : panel_hi);
        char label[4] = {static_cast<char>('1' + v), 0, 0, 0};
        g.text_centered(fx + v * 90 + 30, r.y + 52, label, soft, 60, TextSize::Small);
      }
    } else if (inst == params::kTrombone) {
      g.text(fx, r.y + 24, "SLIDE  •  first to seventh position", muted, 400, TextSize::Small);
      const int sw = r.w - (fx - r.x) - 60;
      g.rect(fx, cy, sw, 6, panel_hi);
      for (int pos = 0; pos < 7; ++pos) g.rect(fx + pos * sw / 7, cy - 10, 2, 26, border);
      const int drop = live_.key >= 0 ? live_.valve_mask : 0;
      const int pos = drop > 6 ? drop - 5 : drop;
      g.round_rect(Rect{fx + pos * sw / 7 - 10, cy - 24, 40, 54}, 8, amber);
      if (drop > 6) g.text(fx, r.bottom() - 12, "F trigger down", orange, 300, TextSize::Small);
    }
    // The note and its fingering on the title row, right-aligned, clear of the valves and slide.
    const std::string fingering = fingering_text();
    if (!fingering.empty()) g.text_right(r.right() - 20, r.y + 24, fingering.c_str(), soft, 420, TextSize::Small);
  }

  // The section from above: the players where the width and pan seat them.
  void draw_section(Painter& g, const Rect& r) const {
    using namespace theme;
    g.text(r.x + 20, r.y + 24, "THE SECTION FROM ABOVE  •  left to right as heard", muted, 520, TextSize::Small);
    const int players = std::clamp(static_cast<int>(std::lround(value(params::kPlayers))), 1, 4);
    const double width = value(params::kStereoWidth), pan = value(params::kPan);
    const int lx = r.x + 60, lw = r.w - 120, ly = r.y + r.h / 2 + 10;
    g.line(lx, ly, lx + lw, ly, border);
    g.text(lx - 30, ly + 6, "L", muted, 20, TextSize::Small);
    g.text(lx + lw + 12, ly + 6, "R", muted, 20, TextSize::Small);
    for (int i = 0; i < players; ++i) {
      const double spread = players > 1 ? (i - 0.5 * (players - 1)) / std::max(1.0, 0.5 * (players - 1)) : 0.0;
      const double at = std::clamp(pan + width * (players > 1 ? 0.8 * spread : 0.0), -1.0, 1.0);
      const int x = lx + static_cast<int>((at * 0.5 + 0.5) * lw);
      g.circle(x - 18, ly - 18, 36, live_.key >= 0 ? amber : soft);
    }
    char line[96]{};
    std::snprintf(line, sizeof(line), "%d player%s  •  \xc2\xb1%.0f cents  •  up to %.0f ms apart", players, players == 1 ? "" : "s", value(params::kSpread), value(params::kTiming));
    g.text(r.x + 560, r.y + 24, line, soft, r.w - 600, TextSize::Small);
  }

  // The room from above: its box, the instrument, the microphones at their distance.
  void draw_room(Painter& g, const Rect& r) const {
    using namespace theme;
    const int size = std::clamp(static_cast<int>(value(params::kRoomSize)), 0, params::kRoomSizeCount - 1);
    const double volume = params::kRoomVolumes[static_cast<size_t>(size)];
    g.text(r.x + 20, r.y + 24, "THE ROOM FROM ABOVE", muted, 300, TextSize::Small);
    const double a = volume > 0.0 ? std::cbrt(volume / (1.0 * 1.4 * 1.9)) : 0.0;
    const double length = 1.9 * a, width = 1.4 * a;
    const double scale = volume > 0.0 ? std::min((r.w - 60) / length, (r.h - 60) / width) : 0.0;
    const int bx = r.x + 30, by = r.y + 40;
    const int bw = volume > 0.0 ? static_cast<int>(length * scale) : r.w - 60, bh = volume > 0.0 ? static_cast<int>(width * scale) : r.h - 60;
    g.round_rect(Rect{bx, by, bw, bh}, 6, volume > 0.0 ? panel_hi : panel);
    // The instrument a quarter of the way along; the microphones down the room.
    const int ix = bx + bw / 4, iy = by + bh * 43 / 100;
    g.circle(ix - 8, iy - 8, 16, amber);
    const double distance = value(params::kMicDistance);
    const double reach = volume > 0.0 ? std::min(distance, 0.9 * (length - length / 4) - 0.3) : distance;
    const int mx = ix + static_cast<int>((volume > 0.0 ? reach * scale : std::min(reach / 12.0, 1.0) * (bw * 0.7)));
    const bool mono = value(params::kMicSetup) >= 0.5;
    if (mono) g.circle(mx - 6, iy - 6, 12, cyan);
    else { g.circle(mx - 6, iy - 16, 12, cyan); g.circle(mx - 6, iy + 4, 12, cyan); }
    g.line(ix, iy, mx, iy, border);
    char line[160]{};
    std::snprintf(line, sizeof(line), "%s  •  %.0f m\xc2\xb3  •  microphones %.2f m away  •  %.0f%% humidity, %.0f C", params::kRoomSizeNames[size], volume, distance,
                  value(params::kHumidity) * 100.0, value(params::kTemperature));
    g.text(r.x + 330, r.y + 24, line, soft, r.w - 360, TextSize::Small);
  }

  const EditorHost& host_;
  const LiveState& live_;
};

}  // namespace honkr::ui
