// Renders single notes of an instrument straight from the engine (no room), for listening and
// for comparing against recordings. A review tool, not a test.
//
//   honkr-note-render <out.wav> instrument=0..26 key=60 velocity=0.7 hold=1.5 tail=1.0
//                       [breath=1 lipping=1 a4=440 temperament=0 vibrato=0 rate=48000]
//
// Prints the instrument, the note's fingering and the pitch it sounds.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "../src/engine.hpp"
#include "../src/wav.hpp"

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: honkr-note-render <out.wav> key=value ...\n"); return 2; }
  std::map<std::string, std::string> a{{"instrument", "0"}, {"key", "60"}, {"velocity", "0.7"}, {"hold", "1.5"}, {"tail", "1.0"}, {"breath", "1"},
                                       {"lipping", "1"}, {"a4", "440"}, {"temperament", "0"}, {"vibrato", "0"}, {"rate", "48000"}};
  for (int i = 2; i < argc; ++i) {
    const char* eq = std::strchr(argv[i], '=');
    if (!eq) { std::fprintf(stderr, "bad argument %s\n", argv[i]); return 2; }
    const std::string key(argv[i], static_cast<size_t>(eq - argv[i]));
    if (!a.count(key)) { std::fprintf(stderr, "unknown key %s\n", key.c_str()); return 2; }
    a[key] = eq + 1;
  }
  using namespace honkr;
  const double rate = std::atof(a["rate"].c_str());
  Engine engine;
  engine.prepare(rate);
  Settings s = Settings::defaults();
  s.v[params::kInstrument] = std::atof(a["instrument"].c_str());
  s.v[params::kBreath] = std::atof(a["breath"].c_str());
  s.v[params::kLipping] = std::atof(a["lipping"].c_str());
  s.v[params::kPitchA4] = std::atof(a["a4"].c_str());
  s.v[params::kTemperament] = std::atof(a["temperament"].c_str());
  s.v[params::kVibrato] = std::atof(a["vibrato"].c_str());
  s.v[params::kMasterDb] = 0.0;
  const int key = std::atoi(a["key"].c_str());
  const double hold = std::atof(a["hold"].c_str()), tail = std::atof(a["tail"].c_str());
  std::vector<float> left(static_cast<size_t>((hold + tail) * rate)), right(left.size());
  engine.note_on(s, 0, key, -1, std::atof(a["velocity"].c_str()));
  const size_t off = static_cast<size_t>(hold * rate);
  for (size_t at = 0; at < left.size(); at += 256) {
    const uint32_t n = static_cast<uint32_t>(std::min<size_t>(256, left.size() - at));
    if (at <= off && off < at + n) engine.note_off(s, 0, key, -1);
    engine.render(s, left.data() + at, right.data() + at, n);
  }
  if (!wav::write_file(argv[1], {left, right}, rate)) { std::fprintf(stderr, "cannot write %s\n", argv[1]); return 1; }
  const int inst = s.instrument();
  const auto f = engine.section().fingering_for(key);
  std::printf("%s key %d: sounds key %d, %.3f Hz, partial %d, valves/slide %d\n", params::kInstrumentNames[inst], key, f.key, f.hz, f.partial, f.valve_mask);
  return 0;
}
