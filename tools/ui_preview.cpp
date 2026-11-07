// Renders the editor offscreen through the real X11 canvas and writes one PPM per page, for
// reviewing the layout without a host. No window is ever mapped.
//
//   honkr-ui-preview OUTPUT_DIR [WIDTH HEIGHT] [PRESET]
//
// The host here is a stand-in: parameters start at a preset's values and the page artwork is
// drawn from those values (the scope shows a quiet test signal and one note in the middle of the instrument's compass sounds, fingered as the instrument fingers it), but layout, widgets,
// text and interaction are the plug-in's own.
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/Xutil.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../src/params.hpp"
#include "../src/presets.hpp"
#include "../src/ui/artwork.hpp"
#include "../src/ui/canvas_x11.hpp"
#include "../src/ui/editor.hpp"
#include "../src/ui/pages.hpp"

namespace {

using namespace honkr::ui;
namespace P = honkr::params;

class PreviewHost final : public EditorHost {
 public:
  PreviewHost() { for (size_t i = 0; i < P::kParamCount; ++i) values[i] = P::kSpecs[i].def; }
  std::array<double, P::kParamCount> values{};
  int param_count() const override { return P::kParamCount; }
  ParamInfo info(int id) const override { const auto& s = P::kSpecs[static_cast<size_t>(id)]; return {s.name, s.min, s.max, s.def, s.stepped}; }
  double value(int id) const override { return values[static_cast<size_t>(id)]; }
  void begin_edit(int) override {}
  void edit(int id, double v) override { values[static_cast<size_t>(id)] = P::clamp_to_spec(static_cast<uint32_t>(id), v); }
  void end_edit(int) override {}
  void format(int id, double v, char* out, size_t n) const override { P::format_value(static_cast<uint32_t>(id), v, out, static_cast<uint32_t>(n)); }
  const char* short_label(int id) const override { const char* n = honkr_short_name(id); return n ? n : label(id); }
  bool relevant(int id) const override { return param_relevant_for_instrument(id, static_cast<int>(values[P::kInstrument])); }
  int size_percent() const override { return 100; }
  void draw_meters(Painter& g, const Rect& r) override {
    using namespace theme;
    g.text(r.x, r.y + 24, "OUT", muted, 50, TextSize::Small);
    g.rect(r.x + 52, r.y + 6, 230, 9, border); g.rect(r.x + 52, r.y + 20, 230, 9, border);
    g.rect(r.x + 52, r.y + 6, 120, 9, green); g.rect(r.x + 52, r.y + 20, 100, 9, green);
    g.text(r.x + 300, r.y + 24, "4 SOUNDING", muted, 150, TextSize::Small);
  }
  void draw_strip(int page, Painter& g, const Rect& r) override {
    LiveState live;
    // A quiet (about -20 dBFS) two-partial note, so the scope's gain is visible in the review,
    // and a C major chord sounding.
    for (size_t i = 0; i < live.scope.size(); ++i) live.scope[i] = static_cast<float>(0.07 * std::sin(static_cast<double>(i) * 0.03) + 0.03 * std::sin(static_cast<double>(i) * 0.09));
    live.energy = 0.3f;
    // The middle of the instrument's compass, fingered as the instrument fingers it.
    const int inst = static_cast<int>(values[P::kInstrument]);
    const auto& in = honkr::dsp::winds::instrument(inst);
    const auto f = honkr::dsp::winds::finger(in, (in.low + in.high) / 2, 440.0, 1.0, honkr::dsp::winds::HzOf{});
    live.sounding[static_cast<size_t>(f.key)] = true;
    live.voices = 1;
    live.key = f.key; live.partial = f.partial; live.valve_mask = f.valve_mask; live.pressure = 0.5 * (in.pressure_soft + in.pressure_loud); live.opening = 0.6;
    Artwork(*this, live).draw(page, g, r);
  }
};

void write_ppm(Display* display, Pixmap pixmap, int w, int h, const std::string& path) {
  XImage* image = XGetImage(display, pixmap, 0, 0, static_cast<unsigned>(w), static_cast<unsigned>(h), AllPlanes, ZPixmap);
  if (!image) { std::fprintf(stderr, "XGetImage failed\n"); std::exit(1); }
  FILE* out = std::fopen(path.c_str(), "wb");
  if (!out) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); std::exit(1); }
  std::fprintf(out, "P6\n%d %d\n255\n", w, h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const unsigned long pixel = XGetPixel(image, x, y);
      const unsigned char rgb[]{static_cast<unsigned char>(pixel >> 16), static_cast<unsigned char>(pixel >> 8), static_cast<unsigned char>(pixel)};
      std::fwrite(rgb, 1, 3, out);
    }
  std::fclose(out);
  XDestroyImage(image);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: honkr-ui-preview OUTPUT_DIR [WIDTH HEIGHT] [PRESET]\n"); return 2; }
  const std::string out_dir = argv[1];
  const int w = argc >= 4 ? std::atoi(argv[2]) : width, h = argc >= 4 ? std::atoi(argv[3]) : height;
  const int preset = argc >= 5 ? std::atoi(argv[4]) : 0;
  Display* display = XOpenDisplay(nullptr);
  if (!display) { std::fprintf(stderr, "no X display\n"); return 1; }
  initialize_x11_text(display);
  const int screen = DefaultScreen(display);
  Pixmap pixmap = XCreatePixmap(display, DefaultRootWindow(display), static_cast<unsigned>(w), static_cast<unsigned>(h), static_cast<unsigned>(DefaultDepth(display, screen)));
  GC gc = XCreateGC(display, pixmap, 0, nullptr);
  XftDraw* xft = XftDrawCreate(display, pixmap, DefaultVisual(display, screen), DefaultColormap(display, screen));
  std::array<XftFont*, 3> fonts{};
  for (size_t i = 0; i < fonts.size(); ++i) {
    char pattern[128];
    std::snprintf(pattern, sizeof(pattern), "DejaVu Sans:weight=%s:pixelsize=%d", i == 2 ? "bold" : "medium", font_pixels(static_cast<TextSize>(i), w, h));
    fonts[i] = XftFontOpenName(display, screen, pattern);
  }
  PreviewHost host;
  honkr::presets::apply(static_cast<size_t>(preset), host.values.data());
  P::g_preset_names = honkr::presets::names();
  Editor editor(host, honkr_pages(), honkr_header_control());
  double now = 0.0;
  for (int page = 0; page < editor.page_count(); ++page) {
    editor.set_page(page);
    for (int tick = 0; tick < 40; ++tick) editor.tick(now += 16.0);
    X11Canvas canvas(display, pixmap, gc, xft, fonts, w, h);
    editor.draw(canvas);
    write_ppm(display, pixmap, w, h, out_dir + "/page" + std::to_string(page + 1) + ".ppm");
  }
  // One frame with the source menu open and one with a knob hovered, for the footer help.
  editor.set_page(0);
  for (int tick = 0; tick < 40; ++tick) editor.tick(now += 16.0);
  const PageLayout lay = editor.layout();
  for (const PlacedControl& c : lay.controls)
    if (editor.page_spec(0).sections[static_cast<size_t>(c.section)].controls[static_cast<size_t>(c.control)].param == P::kInstrument) {
      editor.pointer(PointerAction::Down, 1, c.rect.x + 40, c.rect.y + 60, 0, now);
      editor.pointer(PointerAction::Up, 1, c.rect.x + 40, c.rect.y + 60, 0, now);
    }
  for (int tick = 0; tick < 40; ++tick) editor.tick(now += 16.0);
  {
    X11Canvas canvas(display, pixmap, gc, xft, fonts, w, h);
    editor.draw(canvas);
    write_ppm(display, pixmap, w, h, out_dir + "/menu.ppm");
  }
  // And the header's preset catalogue, the longest menu, which packs its rows to stay inside.
  editor.pointer(PointerAction::Down, 1, 2, 2, 0, now);
  editor.pointer(PointerAction::Up, 1, 2, 2, 0, now);
  for (int tick = 0; tick < 10; ++tick) editor.tick(now += 16.0);
  {
    const Rect header = editor.header_control_rect();
    editor.pointer(PointerAction::Down, 1, header.x + 50, header.y + 20, 0, now);
    editor.pointer(PointerAction::Up, 1, header.x + 50, header.y + 20, 0, now);
  }
  for (int tick = 0; tick < 40; ++tick) editor.tick(now += 16.0);
  {
    X11Canvas canvas(display, pixmap, gc, xft, fonts, w, h);
    editor.draw(canvas);
    write_ppm(display, pixmap, w, h, out_dir + "/presets.ppm");
  }
  for (auto* font : fonts) if (font) XftFontClose(display, font);
  XftDrawDestroy(xft);
  XFreeGC(display, gc);
  XFreePixmap(display, pixmap);
  XCloseDisplay(display);
  std::printf("wrote %d pages to %s\n", editor.page_count(), out_dir.c_str());
  return 0;
}
