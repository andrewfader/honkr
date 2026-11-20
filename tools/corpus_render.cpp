// Renders a preset playing its corpus phrase (tests/corpus_phrases.hpp) to a 44.1 kHz stereo WAV
// named as the song corpus names a separated stem, `<id>_<stem>.wav`, so the corpus's own
// extractor (song-corpus/scripts/features.py, after conversion to FLAC) measures it exactly as it
// measured the records. With no preset named, renders every case the corpus gate holds
// (tests/corpus_gate.hpp). Settings after the preset override it, by parameter name. A review
// and fitting tool, not a test.
//
//   honkr-corpus-render OUT_DIR
//   honkr-corpus-render OUT_DIR "PRESET" counterpoint|bass STEM ["Param name=value" ...]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "../src/presets.hpp"
#include "../src/wav.hpp"
#include "../tests/corpus_gate.hpp"

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

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2 && argc < 5) {
    std::fprintf(stderr, "usage: honkr-corpus-render OUT_DIR [\"PRESET\" counterpoint|bass STEM [\"Param=value\" ...]]\n");
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
  const corpus::Phrase phrase = std::strcmp(argv[3], "bass") == 0 ? corpus::Phrase::BassLine : corpus::Phrase::Counterpoint;
  return write(dir, argv[2], argv[4], corpus::play(s, phrase)) ? 0 : 1;
}
