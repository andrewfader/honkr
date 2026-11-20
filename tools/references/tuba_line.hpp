// The University of Iowa tuba (references/README.md) playing the corpus's tuba line
// (tests/corpus_phrases.hpp): its recorded notes, C2 to B2, each from its onset for as long as
// the line holds it (or the recording does), faded over 20 ms, placed where corpus::play starts
// it. Read from the reference recordings' cache; empty, with the reason, when it cannot be.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "../../tests/corpus_phrases.hpp"
#include "../../tests/timbre.hpp"
#include "aiff.hpp"

namespace references {

inline std::vector<float> tuba_line(const std::string& cache, const std::string& dynamic, std::string* error) {
  using namespace honkr;
  aiff::Audio audio;
  const std::string path = cache + "/Tuba." + dynamic + ".C2B2.aiff";
  if (!aiff::read_file(path.c_str(), audio, error)) { *error = path + ": " + *error; return {}; }
  const auto notes = timbre::segment(audio.mono, audio.rate);
  if (notes.size() != 12 || audio.rate != corpus::kRate) { *error = path + ": " + std::to_string(notes.size()) + " notes at " + std::to_string(audio.rate) + " Hz"; return {}; }
  constexpr double kBeat = 60.0 / 96.0, kStart = 0.25;
  const size_t frames = static_cast<size_t>(corpus::kSeconds * corpus::kRate), fade = static_cast<size_t>(0.02 * corpus::kRate);
  std::vector<float> line(frames, 0.0f);
  for (const corpus::Note& n : corpus::notes(corpus::Phrase::TubaLine)) {
    const timbre::Segment& seg = notes[static_cast<size_t>(n.key - 36)];
    const size_t at = static_cast<size_t>((kStart + n.beat * kBeat) * corpus::kRate);
    const size_t length = std::min({static_cast<size_t>(n.beats * kBeat * corpus::kRate), seg.length, frames - at});
    for (size_t i = 0; i < length; ++i) {
      const double tail = i + fade >= length ? 0.5 + 0.5 * std::cos(dsp::kPi * static_cast<double>(i + fade - length) / static_cast<double>(fade)) : 1.0;
      line[at + i] += static_cast<float>(tail * audio.mono[seg.onset + i]);
    }
  }
  return line;
}

}  // namespace references
