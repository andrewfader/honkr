// What a preset plays when it is measured against the song corpus (tests/corpus_gate_tests.cpp,
// tools/corpus_render.cpp): sixteen seconds at the corpus's 44.1 kHz, the way the records use
// the instrument. A horn section plays three lines in counterpoint in F, moving at different
// times; a bass horn plays a root-and-fifth bass line in its low register; the tuba plays that
// line a fifth up, folded into C2..B2, the octave the University of Iowa tuba was recorded in
// (references/), so a recording of the real instrument can play exactly the same notes.
#pragma once

#include <algorithm>
#include <vector>

#include "../src/engine.hpp"

namespace honkr::corpus {

constexpr double kRate = 44100.0;
constexpr uint32_t kBlock = 256;
constexpr double kSeconds = 16.0;

enum class Phrase { Counterpoint, BassLine, TubaLine };

struct Note { double beat, beats; int key; double velocity; };

inline std::vector<Note> notes(Phrase phrase) {
  if (phrase == Phrase::TubaLine) {
    std::vector<Note> line = notes(Phrase::BassLine);
    for (Note& n : line) n.key = n.key + 7 > 47 ? n.key - 5 : n.key + 7;
    return line;
  }
  if (phrase == Phrase::BassLine)
    return {{0, 1.8, 29, 0.7}, {2, 0.9, 36, 0.65}, {3, 0.9, 34, 0.65}, {4, 1.8, 31, 0.7}, {6, 1.8, 38, 0.65},
            {8, 1.8, 34, 0.7}, {10, 0.9, 41, 0.65}, {11, 0.9, 36, 0.65}, {12, 1.8, 29, 0.7}, {14, 0.9, 36, 0.65},
            {15, 0.9, 33, 0.65}, {16, 1.8, 34, 0.7}, {18, 1.8, 36, 0.7}, {20, 3.6, 29, 0.7}};
  return {
      // the top line
      {0, 1.9, 69, 0.7}, {2, 0.95, 70, 0.65}, {3, 0.95, 72, 0.7}, {4, 2.9, 74, 0.75}, {7, 0.95, 72, 0.65},
      {8, 1.9, 70, 0.7}, {10, 1.9, 69, 0.65}, {12, 2.9, 67, 0.7}, {15, 0.95, 69, 0.65}, {16, 3.9, 65, 0.7},
      {20, 3.9, 69, 0.7},
      // the inner line, entering a beat later
      {1, 2.9, 60, 0.6}, {4, 1.9, 65, 0.6}, {6, 1.9, 67, 0.6}, {8, 0.95, 65, 0.6}, {9, 0.95, 64, 0.6},
      {10, 1.9, 62, 0.6}, {12, 1.9, 64, 0.6}, {14, 1.9, 60, 0.6}, {16, 1.9, 62, 0.6}, {18, 1.9, 60, 0.6},
      {20, 3.9, 60, 0.6},
      // the low line
      {0, 3.9, 41, 0.65}, {4, 1.9, 46, 0.65}, {6, 1.9, 43, 0.65}, {8, 3.9, 46, 0.65}, {12, 1.9, 48, 0.65},
      {14, 1.9, 45, 0.65}, {16, 1.9, 46, 0.65}, {18, 1.9, 48, 0.65}, {20, 3.9, 41, 0.65},
  };
}

struct Audio { std::vector<float> left, right; };

// Plays the phrase at 96 beats a minute from 0.25 s, for kSeconds; at the phrase's own
// velocities, or at one velocity throughout.
inline Audio play(const Settings& s, Phrase phrase, double seconds = kSeconds, double velocity = -1.0) {
  constexpr double kBeat = 60.0 / 96.0, kStart = 0.25;
  Engine engine;
  engine.prepare(kRate);
  struct Event { size_t at; int key; bool on; double velocity; };
  std::vector<Event> events;
  for (const Note& n : notes(phrase)) {
    events.push_back({static_cast<size_t>((kStart + n.beat * kBeat) * kRate), n.key, true, velocity >= 0.0 ? velocity : n.velocity});
    events.push_back({static_cast<size_t>((kStart + (n.beat + n.beats) * kBeat) * kRate), n.key, false, 0.0});
  }
  std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) { return a.at < b.at || (a.at == b.at && !a.on && b.on); });
  const size_t frames = static_cast<size_t>(seconds * kRate);
  Audio out;
  out.left.assign(frames, 0.0f); out.right.assign(frames, 0.0f);
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
  return out;
}

}  // namespace honkr::corpus
