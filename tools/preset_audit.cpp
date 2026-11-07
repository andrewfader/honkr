// Renders every factory preset with the same short phrase and measures it against the quality
// bar in docs/QUALITY.md, so a preset round can be judged from audio rather than from the
// settings. Writes one stereo WAV per preset for listening and spectrograms, and prints one
// line per preset: peak, momentary loudness, limiter reduction, held level, DC, idle noise,
// brightness, stereo width, tail, and the master trim still wanted to reach the target.
//
//   honkr-preset-audit OUTPUT_DIR [PRESET_FILTER]
//   honkr-preset-audit --levels
//
// `--levels` prints the per-instrument table behind the trims in engine.hpp (levels::): one note
// in the middle of each instrument's compass at a firm breath, with the trim each would still
// need.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "../src/engine.hpp"
#include "../src/presets.hpp"
#include "../src/wav.hpp"
#include "../tests/audio_measure.hpp"
#include "../tests/audit_phrase.hpp"

namespace {

using namespace honkr;
using measure::Span;
using audit::kRate;
using audit::kBlock;

bool finite(const std::vector<float>& x) { for (float v : x) if (!std::isfinite(v)) return false; return true; }

std::string slug(const char* name) {
  std::string out;
  for (const char* c = name; *c; ++c) out.push_back(std::isalnum(static_cast<unsigned char>(*c)) ? static_cast<char>(std::tolower(static_cast<unsigned char>(*c))) : '_');
  return out;
}

int print_levels() {
  for (int inst = 0; inst < params::kInstrumentCount; ++inst) {
    Engine engine;
    engine.prepare(kRate);
    Settings s = Settings::defaults();
    s.v[params::kInstrument] = inst; s.v[params::kMasterDb] = 0;
    std::vector<float> l(57600), r(57600);
    engine.note_on(s, 0, levels::reference_key(inst), -1, 0.8);
    for (size_t b = 0; b < l.size() / kBlock; ++b) engine.render(s, l.data() + b * kBlock, r.data() + b * kBlock, kBlock);
    const double peak = measure::db(measure::peak(Span(l))), rms = measure::db(measure::rms(Span(l, 14400, 38400)));
    std::printf("%-22s peak %6.1f dBFS  held rms %6.1f dBFS  trim still wanted %+5.1f dB\n", params::kInstrumentNames[inst], peak, rms, std::min(-16.0 - rms, -3.0 - peak));
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && std::strcmp(argv[1], "--levels") == 0) return print_levels();
  if (argc < 2) { std::fprintf(stderr, "usage: honkr-preset-audit OUTPUT_DIR [PRESET_FILTER]\n"); return 2; }
  const std::string out_dir = argv[1];
  const std::string filter = argc >= 3 ? argv[2] : "";
  std::filesystem::create_directories(out_dir);
  std::printf("%-3s %-28s %6s %6s %6s %6s %6s %6s %6s %6s %7s %5s %6s %5s %s\n", "#", "preset", "peak", "loud", "limit", "held", "melody", "hold", "dc", "idle", "bright", "width", "tail", "want", "flags");
  int failures = 0;
  for (size_t p = 0; p < presets::count(); ++p) {
    const char* name = presets::all()[p].name;
    if (!filter.empty() && std::string(name).find(filter) == std::string::npos) continue;
    Engine engine;
    engine.prepare(kRate);
    Settings s = Settings::defaults();
    presets::apply(p, s.v.data());
    const audit::Rendered r = audit::render(s, engine);
    const double peak = measure::db(std::max(measure::peak(Span(r.left)), measure::peak(Span(r.right))));
    const double loud = measure::momentary_loudness_max(r.mono, kRate);
    const double limit = engine.limited_db();
    const Span chord(r.mono, r.first_note + 14400, static_cast<size_t>(1.1 * kRate));
    const double held = measure::db(measure::rms(chord));
    const double dc = measure::db(std::abs(measure::mean(chord)));
    const double idle = measure::db(measure::rms(Span(r.mono, 2400, r.first_note - 2400 - 512)));
    const double melody = measure::db(measure::peak(Span(r.mono, static_cast<size_t>(2.1 * kRate), static_cast<size_t>(1.0 * kRate))));
    const double early = measure::db(measure::rms(Span(r.mono, r.first_note + 2400, 4800)));
    const double later = measure::db(measure::rms(Span(r.mono, r.first_note + 48000, 4800)));
    const double hold = later - early;
    const double bright = measure::spectral_centroid(chord, kRate, 12);
    const double width = 1.0 - measure::correlation(Span(r.left, r.first_note + 9600, 48000), Span(r.right, r.first_note + 9600, 48000));
    const double tail = measure::db(measure::peak(Span(r.mono, r.mono.size() - 4800)));
    const double want = audit::kTargetLufs - loud;
    std::string flags;
    if (!finite(r.left) || !finite(r.right)) flags += " NONFINITE";
    if (peak > -1.5) flags += " HOT";
    if (loud > audit::kTargetLufs + audit::kLoudnessSlackDb) flags += " LOUD";
    if (loud < audit::kTargetLufs - audit::kLoudnessSlackDb) flags += " QUIET";
    if (melody < peak - 16.0) flags += " MELODYLOST";
    if (dc > -40.0) flags += " DC";
    if (idle > -55.0) flags += " NOISY";
    // The render stops 8 s after the phrase: a tail only fails if it was heard out in full.
    if (tail > -50.0 && engine.tail_samples(s) / kRate <= 8.0) flags += " TAIL";
    if (engine.clipped()) flags += " CLIP";
    if (!flags.empty()) ++failures;
    std::printf("%-3zu %-28s %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %7.0f %5.2f %6.1f %+5.1f%s\n", p, name, peak, loud, limit, held, melody, hold, dc, idle, bright, width, tail, want, flags.c_str());
    char file[64];
    std::snprintf(file, sizeof(file), "%02zu_%s.wav", p, slug(name).c_str());
    wav::write_file((out_dir + "/" + file).c_str(), {r.left, r.right}, kRate);
  }
  std::printf("%d preset(s) flagged\n", failures);
  return 0;
}
