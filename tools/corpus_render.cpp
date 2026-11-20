// Renders a preset playing its corpus phrase (tests/corpus_phrases.hpp) to a 44.1 kHz stereo WAV
// named as the song corpus names a separated stem, `<id>_<stem>.wav`, so the corpus's own
// extractor (song-corpus/scripts/features.py, after conversion to FLAC) measures it exactly as it
// measured the records. With no preset named, renders every case the corpus gate holds
// (tests/corpus_gate.hpp). Settings after the preset override it, by parameter name. With
// iowa-tuba, splices the University of Iowa tuba's mezzo-forte and fortissimo notes (the
// reference recordings' cache, references/README.md) into the tuba line on the same grid, and
// writes each take dry and through the home tape (corpus::kHomeTape): the reference
// tools/corpus_reference.py measures. A review and fitting tool, not a test.
//
//   honkr-corpus-render OUT_DIR
//   honkr-corpus-render OUT_DIR "PRESET" counterpoint|bass|tuba STEM ["Param name=value" ...]
//   honkr-corpus-render iowa-tuba CACHE_DIR OUT_DIR
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "../src/presets.hpp"
#include "../src/wav.hpp"
#include "../tests/corpus_gate.hpp"
#include "references/tuba_line.hpp"

namespace {

using namespace honkr;

std::string slug(const char* name) {
  std::string out;
  for (const char* c = name; *c; ++c) out.push_back(std::isalnum(static_cast<unsigned char>(*c)) ? static_cast<char>(std::tolower(static_cast<unsigned char>(*c))) : '-');
  return out;
}

bool write(const std::string& dir, const char* preset, const char* stem, const corpus::Audio& a) {
  const std::string path = dir + "/" + slug(preset) + "_" + stem + ".wav";
  const corpus::Features f = corpus::measure(a.left, a.right, corpus::kRate);
  std::printf("%-34s %-6s centroid %6.0f Hz  roll-off %6.0f Hz  corr %6.4f  wobble %5.2f ct  -> %s\n", preset, stem, f.centroid_hz, f.rolloff85_hz, f.corr, f.wobble_cents, path.c_str());
  return wav::write_file(path.c_str(), {a.left, a.right}, corpus::kRate);
}

bool iowa_tuba(const std::string& cache, const std::string& dir) {
  for (const char* dynamic : {"mf", "ff"}) {
    std::string error;
    const std::vector<float> line = references::tuba_line(cache, dynamic, &error);
    if (line.empty()) { std::fprintf(stderr, "%s\n", error.c_str()); return false; }
    const size_t frames = line.size();
    const std::string tag = std::string("iowa-tuba-") + dynamic;
    if (!write(dir, (tag + "-dry").c_str(), "bass", {line, line})) return false;
    Settings s = Settings::defaults();
    corpus::record_as(corpus::kHomeTape, s);
    dsp::air::Room room;
    dsp::tape::Tape tape;
    room.prepare(corpus::kRate);
    tape.prepare(corpus::kRate);
    room.configure(Engine::room_settings(s));
    tape.configure(s[params::kWowFlutter], s[params::kTapeBandwidth]);
    corpus::Audio taped{line, line};
    for (size_t i = 0; i < frames; ++i) {
      room.process(taped.left[i], taped.right[i]);
      tape.process(taped.left[i], taped.right[i]);
    }
    if (!write(dir, (tag + "-taped").c_str(), "bass", taped)) return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 4 && std::strcmp(argv[1], "iowa-tuba") == 0) {
    std::filesystem::create_directories(argv[3]);
    return iowa_tuba(argv[2], argv[3]) ? 0 : 1;
  }
  if (argc != 2 && argc < 5) {
    std::fprintf(stderr, "usage: honkr-corpus-render OUT_DIR [\"PRESET\" counterpoint|bass|tuba STEM [\"Param=value\" ...]] | iowa-tuba CACHE_DIR OUT_DIR\n");
    return 2;
  }
  const std::string dir = argv[1];
  std::filesystem::create_directories(dir);
  if (argc == 2) {
    for (const corpus::Case& c : corpus::cases()) {
      Settings s = Settings::defaults();
      presets::apply(static_cast<size_t>(corpus::preset_index(c.preset)), s.v.data());
      if (!write(dir, c.preset, c.stem, corpus::play(s, c.phrase))) return 1;
    }
    return 0;
  }
  const long index = corpus::preset_index(argv[2]);
  if (index < 0) { std::fprintf(stderr, "no preset named %s\n", argv[2]); return 2; }
  Settings s = Settings::defaults();
  presets::apply(static_cast<size_t>(index), s.v.data());
  for (int i = 5; i < argc; ++i) {
    const char* eq = std::strrchr(argv[i], '=');
    bool found = false;
    for (uint32_t id = 0; eq && id < params::kParamCount; ++id)
      if (std::strncmp(params::spec(id).name, argv[i], static_cast<size_t>(eq - argv[i])) == 0 && std::strlen(params::spec(id).name) == static_cast<size_t>(eq - argv[i])) {
        s.v[id] = params::clamp_to_spec(id, std::atof(eq + 1));
        found = true;
      }
    if (!found) { std::fprintf(stderr, "bad setting %s\n", argv[i]); return 2; }
  }
  const corpus::Phrase phrase = std::strcmp(argv[3], "bass") == 0 ? corpus::Phrase::BassLine : std::strcmp(argv[3], "tuba") == 0 ? corpus::Phrase::TubaLine : corpus::Phrase::Counterpoint;
  return write(dir, argv[2], argv[4], corpus::play(s, phrase)) ? 0 : 1;
}
