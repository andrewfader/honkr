// HONKR against recordings of the real instruments (references/README.md).
//
//   honkr-reference extract [CACHE_DIR]      features of every note of the downloaded recordings,
//                                             written to references/features.tsv
//   honkr-reference match [--update-baseline] [--notes]
//                                             plays every reference note through HONKR, prints the
//                                             per-instrument scores against the baseline; with
//                                             --update-baseline writes references/baseline.tsv
//
// Run from the repository root. `extract` needs tools/references/fetch.py to have run.
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "../tests/reference_match.hpp"
#include "references/aiff.hpp"

namespace {

// The keys a recording's name says it holds: "Oboe.mf.C5B5.aiff" is C5 to B5, "TenorTrombone.mf.C5.aiff"
// just C5. A note read outside them (an octave slip of the pitch reader) is left out.
std::pair<int, int> named_range(const std::string& name) {
  static const char* kNotes = "C D EF G A B";
  std::vector<int> keys;
  const std::string stem = name.substr(0, name.rfind('.'));
  const std::string last = stem.substr(stem.rfind('.') + 1);
  for (size_t i = 0; i < last.size();) {
    const char* at = std::strchr(kNotes, last[i]);
    if (!at || last[i] == ' ') return {0, 127};
    int pc = static_cast<int>(at - kNotes);
    ++i;
    if (i < last.size() && last[i] == 'b') { --pc; ++i; }
    if (i >= last.size() || !std::isdigit(static_cast<unsigned char>(last[i]))) return {0, 127};
    keys.push_back(12 * (last[i] - '0' + 1) + pc);
    ++i;
  }
  if (keys.empty()) return {0, 127};
  return {keys.front(), keys.back()};
}

int extract(const std::string& cache) {
  std::ifstream manifest("references/manifest.tsv");
  if (!manifest) { std::fprintf(stderr, "run from the repository root\n"); return 2; }
  std::ofstream out("references/features.tsv");
  out << "# Features of every note of the reference recordings (tests/timbre.hpp), from `honkr-reference extract`.\n"
         "# instrument\tdynamic\tkey\tf0\tcentroid\todd_even_db\tnoise_db\tattack_ms\tlevel_db\th1..h10 dB\n";
  std::string line;
  int files = 0, notes = 0;
  while (std::getline(manifest, line)) {
    if (line.empty() || line[0] == '#') continue;
    const size_t a = line.find('\t'), b = line.find('\t', a + 1);
    const std::string instrument = line.substr(0, a), dynamic = line.substr(a + 1, b - a - 1);
    std::string name = line.substr(line.rfind('/') + 1);
    name.erase(std::remove(name.begin(), name.end(), ' '), name.end());
    aiff::Audio audio;
    std::string error;
    if (!aiff::read_file((cache + "/" + name).c_str(), audio, &error)) { std::fprintf(stderr, "%s: %s\n", name.c_str(), error.c_str()); return 1; }
    ++files;
    const auto [first_key, last_key] = named_range(name);
    for (const auto& seg : timbre::segment(audio.mono, audio.rate)) {
      // The note's pitch, read broadly, names its key; then it is analysed around that key.
      const measure::Span middle(audio.mono, seg.onset + seg.length / 3, std::min<size_t>(seg.length / 3, static_cast<size_t>(audio.rate * 0.5)));
      const double hz = measure::pitch_hz(middle, audio.rate, 25.0, 2500.0);
      if (hz <= 0.0) continue;
      const int key = static_cast<int>(std::lround(69.0 + 12.0 * std::log2(hz / 440.0)));
      const timbre::Features f = timbre::analyse(audio.mono, audio.rate, seg.onset, seg.length, 440.0 * std::exp2((key - 69) / 12.0));
      if (f.f0 <= 0.0 || std::abs(timbre::cents(f.f0, 440.0 * std::exp2((key - 69) / 12.0))) > 60.0) continue;
      if (key < first_key - 1 || key > last_key + 1) continue;
      out << timbre::to_line(instrument, dynamic, key, f) << "\n";
      ++notes;
    }
  }
  std::printf("%d files, %d notes -> references/features.tsv\n", files, notes);
  return 0;
}

int match(bool update, bool per_note) {
  const auto refs = reference::load_features("references/features.tsv");
  if (refs.empty()) { std::fprintf(stderr, "no references/features.tsv (run from the repository root)\n"); return 2; }
  std::vector<reference::Played> detail;
  const auto scores = reference::score(refs, 48000.0, &detail);
  const auto baseline = reference::load_baseline("references/baseline.tsv");
  if (per_note)
    for (const auto& p : detail)
      std::printf("%-14s %s %3d  centroid %5.2f / %5.2f  odd-even %+6.1f / %+6.1f  noise %6.1f / %6.1f  level %6.1f / %6.1f\n", p.reference.instrument.c_str(), p.reference.dynamic.c_str(), p.reference.key,
                  p.model.centroid, p.reference.f.centroid, p.model.odd_even_db, p.reference.f.odd_even_db, p.model.noise_db, p.reference.f.noise_db, p.model.level_db, p.reference.f.level_db);
  std::printf("%-14s %5s %10s %10s %11s %8s %10s %14s\n", "instrument", "notes", "profile_db", "brightness", "odd_even_db", "noise_db", "dynamic_db", "dynamic_bright");
  for (const auto& [name, s] : scores) {
    std::printf("%-14s %5d %10.2f %10.3f %11.2f %8.2f %10.2f %14.3f", name.c_str(), s.notes, s.profile_db, s.brightness, s.odd_even_db, s.noise_db, s.dynamic_db, s.dynamic_bright);
    if (auto it = baseline.find(name); it != baseline.end()) {
      const auto now = reference::values(s), then = reference::values(it->second);
      int better = 0, worse = 0;
      for (size_t i = 0; i < now.size(); ++i) { if (now[i] < then[i] - reference::kSlack[i]) ++better; if (now[i] > then[i] + reference::kSlack[i]) ++worse; }
      std::printf("   %s", worse ? "WORSE" : better ? "better" : "same");
    }
    std::printf("\n");
  }
  if (update) { reference::write_baseline("references/baseline.tsv", scores); std::printf("wrote references/baseline.tsv\n"); }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && !std::strcmp(argv[1], "extract")) return extract(argc >= 3 ? argv[2] : std::string(std::getenv("HOME") ? std::getenv("HOME") : ".") + "/.cache/honkr-references");
  if (argc >= 2 && !std::strcmp(argv[1], "match")) {
    bool update = false, notes = false;
    for (int i = 2; i < argc; ++i) { if (!std::strcmp(argv[i], "--update-baseline")) update = true; if (!std::strcmp(argv[i], "--notes")) notes = true; }
    return match(update, notes);
  }
  std::fprintf(stderr, "usage: honkr-reference extract [CACHE_DIR] | match [--update-baseline] [--notes]\n");
  return 2;
}
