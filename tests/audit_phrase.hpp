// The phrase every preset is judged on (docs/QUALITY.md): a held middle C (with the E and G above
// it, which a solo player slurs to and a section plays as a chord), then three short melody
// notes, then the preset's own tail. Every instrument plays it in its own compass: a note beyond
// it sounds an octave inside. Shared by the
// honkr-preset-audit tool and engine_tests so the bar is measured the same way in both.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "../src/engine.hpp"

namespace honkr::audit {

constexpr double kRate = 48000.0;
constexpr uint32_t kBlock = 256;
constexpr double kFirstNoteSeconds = 0.30;
constexpr double kPhraseEndSeconds = 3.1;
// The level every preset is matched on: the loudest 400 ms of the phrase (BS.1770 momentary
// loudness of the mono sum) lands here, within the slack below, with the limiter asked for no
// more than kMaxLimitDb of reduction over the whole phrase.
constexpr double kTargetLufs = -18.0;
constexpr double kLoudnessSlackDb = 2.0;
constexpr double kMaxLimitDb = Limiter::kClipDb;

struct Note { double at_s; int key; double velocity; double length_s; };
inline const std::vector<Note>& phrase() {
  static const std::vector<Note> notes{
      {0.30, 60, 0.75, 1.6}, {0.30, 64, 0.7, 1.6}, {0.30, 67, 0.7, 1.6},
      {2.10, 72, 0.8, 0.22}, {2.35, 74, 0.7, 0.22}, {2.60, 76, 0.9, 0.45},
  };
  return notes;
}

struct Rendered { std::vector<float> left, right, mono; size_t first_note = 0, phrase_end = 0; };

// Renders the phrase through `engine` with `s`, then the tail up to `max_tail_s`.
inline Rendered render(const Settings& s, Engine& engine, double max_tail_s = 8.0) {
  const double total_s = kPhraseEndSeconds + std::min(max_tail_s, engine.tail_samples(s) / kRate) + 0.2;
  const size_t frames = static_cast<size_t>(total_s * kRate);
  Rendered out;
  out.left.assign(frames, 0.0f); out.right.assign(frames, 0.0f);
  out.first_note = static_cast<size_t>(kFirstNoteSeconds * kRate);
  out.phrase_end = static_cast<size_t>(kPhraseEndSeconds * kRate);
  struct Pending { size_t at; int key; bool on; double velocity; };
  std::vector<Pending> events;
  for (const Note& n : phrase()) {
    events.push_back({static_cast<size_t>(n.at_s * kRate), n.key, true, n.velocity});
    events.push_back({static_cast<size_t>((n.at_s + n.length_s) * kRate), n.key, false, 0.0});
  }
  std::sort(events.begin(), events.end(), [](const Pending& a, const Pending& b) { return a.at < b.at; });
  size_t next = 0;
  for (size_t done = 0; done < frames; done += kBlock) {
    const uint32_t n = static_cast<uint32_t>(std::min<size_t>(kBlock, frames - done));
    while (next < events.size() && events[next].at < done + n) {
      if (events[next].on) engine.note_on(s, 0, events[next].key, -1, events[next].velocity);
      else engine.note_off(s, 0, events[next].key, -1);
      ++next;
    }
    engine.render(s, out.left.data() + done, out.right.data() + done, n);
  }
  out.mono.resize(frames);
  for (size_t i = 0; i < frames; ++i) out.mono[i] = 0.5f * (out.left[i] + out.right[i]);
  return out;
}

}  // namespace honkr::audit
