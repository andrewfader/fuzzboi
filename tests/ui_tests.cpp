// Executable scenarios for features/editor.feature: the real page table and editor, driven by a
// fake host that enforces CLAP's gesture rules and a canvas that records what is drawn. No
// window is needed, so this runs everywhere the plug-in builds.
#include "../src/params.hpp"
#include "../src/presets.hpp"
#include "../src/ui/editor.hpp"
#include "../src/ui/layout.hpp"
#include "../src/ui/artwork.hpp"
#include "../src/ui/pages.hpp"
#include "../src/ui/visualizer.hpp"
#include "spec.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <set>
#include <string>
#include <vector>

using namespace fuzzboi::ui;
namespace P = fuzzboi::params;
constexpr int kParams = static_cast<int>(P::kParamCount);

namespace {

// Checks every edit against CLAP's rule that a change from the editor sits inside a gesture for
// that parameter, and that gestures never nest or leak.
class FakeHost final : public EditorHost {
 public:
  FakeHost() { for (int i = 0; i < kParams; ++i) values[static_cast<size_t>(i)] = P::kSpecs[static_cast<size_t>(i)].def; }
  std::array<double, P::kParamCount> values{};
  std::set<int> open;
  int begins = 0, edits = 0, ends = 0, violations = 0;
  std::vector<int> strip_clicks, requested;
  int percent = 0;

  int param_count() const override { return kParams; }
  ParamInfo info(int id) const override {
    const auto& s = P::kSpecs[static_cast<size_t>(id)];
    return {s.name, s.min, s.max, s.def, s.stepped};
  }
  double value(int id) const override { return values[static_cast<size_t>(id)]; }
  void begin_edit(int id) override { if (open.count(id)) ++violations; open.insert(id); ++begins; }
  void edit(int id, double v) override {
    if (!open.count(id)) ++violations;
    const auto& s = P::kSpecs[static_cast<size_t>(id)];
    if (v < s.min - 1e-9 || v > s.max + 1e-9) ++violations;
    if (s.stepped && v != std::round(v)) ++violations;
    values[static_cast<size_t>(id)] = v;
    ++edits;
  }
  void end_edit(int id) override { if (!open.count(id)) ++violations; open.erase(id); ++ends; }
  void format(int id, double v, char* out, size_t n) const override { P::format_value(static_cast<uint32_t>(id), v, out, static_cast<uint32_t>(n)); }
  const char* short_label(int id) const override { const char* n = fuzzboi_short_name(id); return n ? n : label(id); }
  bool strip_pointer(int page, int button, int, int, const Rect&) override { strip_clicks.push_back(page * 10 + button); return true; }
  int size_percent() const override { return percent; }
  void request_size(int p) override { requested.push_back(p); percent = p; }
  void reset_counts() { begins = edits = ends = 0; }
  bool idle() const { return open.empty(); }
};

// Records drawing and checks it stays inside the design space.
class RecordingCanvas final : public Canvas {
 public:
  int rects = 0, lines = 0, circles = 0, texts = 0, outside = 0;
  std::vector<std::string> strings;
  void clear(uint32_t) override {}
  void fill_rect(int x, int y, int w, int h, uint32_t) override { ++rects; check(x, y); check(x + w, y + h); }
  void draw_polyline(const Point* points, int count, uint32_t, int) override { ++lines; for (int i = 0; i < count; ++i) check(points[i].x, points[i].y); }
  void fill_circle(int x, int y, int d, uint32_t) override { ++circles; check(x, y); check(x + d, y + d); }
 protected:
  void draw_glyphs(int x, int y, const std::string& text, uint32_t, TextSize size) override { ++texts; strings.push_back(text); check(x, y); check(x + measure_text(text, size), y); }
  int measure_text(const std::string& text, TextSize size) override { return static_cast<int>(text.size()) * text_pixels(size) * 55 / 100; }
 private:
  void check(int x, int y) { if (x < -2 || x > width + 16 || y < -16 || y > height + 16) ++outside; }
};

struct Rig {
  FakeHost host;
  Editor editor{host, fuzzboi_pages(), fuzzboi_header_control()};
  double now = 1000.0;
  void settle() { for (int i = 0; i < 60; ++i) editor.tick(now += 16.0); }
  void move(int x, int y, unsigned mods = 0) { editor.pointer(PointerAction::Move, 0, x, y, mods, now); }
  void down(int button, int x, int y, unsigned mods = 0) { editor.pointer(PointerAction::Down, button, x, y, mods, now); }
  void up(int button, int x, int y, unsigned mods = 0) { editor.pointer(PointerAction::Up, button, x, y, mods, now); }
  void click(int button, int x, int y, unsigned mods = 0) { down(button, x, y, mods); up(button, x, y, mods); now += 500.0; }
  void wheel(int direction, int x, int y, unsigned mods = 0) { down(direction > 0 ? 4 : 5, x, y, mods); }
  PlacedControl find(int param) const {
    for (const PlacedControl& placed : editor.layout().controls)
      if (editor.page_spec(editor.page()).sections[static_cast<size_t>(placed.section)].controls[static_cast<size_t>(placed.control)].param == param) return placed;
    return {};
  }
  bool go_to(int param) {
    for (int page = 0; page < editor.page_count(); ++page)
      for (const SectionSpec& section : editor.page_spec(page).sections)
        for (const ControlSpec& control : section.controls)
          if (control.param == param) { editor.set_page(page); settle(); return true; }
    return false;
  }
};

}  // namespace

SCENARIO(coordinates_scale_to_any_window_and_back) {
  GIVEN("several window sizes from half to double the design");
  bool round_trips = true, legible = true;
  for (const auto size : std::array<std::array<int, 2>, 5>{{{800, 525}, {1280, 720}, {1600, 1050}, {1920, 1200}, {2200, 900}}}) {
    for (const int x : {0, margin, 700, width}) round_trips = round_trips && std::abs(unscale_x(scale_x(x, size[0]), size[0]) - x) <= 1;
    for (const int y : {0, tab_y, strip_y, height}) round_trips = round_trips && std::abs(unscale_y(scale_y(y, size[1]), size[1]) - y) <= 1;
    for (const TextSize t : {TextSize::Small, TextSize::Normal, TextSize::Large}) legible = legible && font_pixels(t, size[0], size[1]) >= 9;
  }
  THEN("design coordinates survive the trip and type never drops below legibility");
  CHECK(round_trips, "scale and unscale round trip");
  CHECK(legible, "fonts stay legible");
  CHECK(mix(0x000000, 0xffffff, 0.0) == 0x000000 && mix(0x000000, 0xffffff, 1.0) == 0xffffff, "colour mix endpoints");
}

SCENARIO(every_parameter_is_placed_exactly_once) {
  GIVEN("the page table");
  std::vector<int> seen(static_cast<size_t>(kParams), 0);
  const auto pages = fuzzboi_pages();
  CHECK(static_cast<int>(pages.size()) == tab_count, "one page per tab");
  for (const PageSpec& page : pages)
    for (const SectionSpec& section : page.sections)
      for (const ControlSpec& control : section.controls) ++seen[static_cast<size_t>(control.param)];
  ++seen[static_cast<size_t>(fuzzboi_header_control().param)];
  for (const int id : fuzzboi_strip_params()) ++seen[static_cast<size_t>(id)];
  THEN("each parameter is reachable from exactly one place");
  bool ok = true;
  for (int id = 0; id < kParams; ++id)
    if (seen[static_cast<size_t>(id)] != 1) { ok = false; std::printf("    parameter %d ('%s') is placed %d times\n", id, P::kSpecs[static_cast<size_t>(id)].name, seen[static_cast<size_t>(id)]); }
  CHECK(ok, "placement");
}

SCENARIO(widgets_suit_their_parameters) {
  GIVEN("every control on every page");
  bool ok = true;
  for (const PageSpec& page : fuzzboi_pages())
    for (const SectionSpec& section : page.sections)
      for (const ControlSpec& c : section.controls) {
        const auto& s = P::kSpecs[static_cast<size_t>(c.param)];
        switch (c.widget) {
          case Widget::Toggle: ok = ok && s.stepped && s.max - s.min == 1.0; break;
          case Widget::Segments: ok = ok && s.stepped && s.max - s.min >= 1.0 && s.max - s.min <= 4.0; break;
          case Widget::Menu: ok = ok && s.stepped && s.max - s.min >= 2.0; break;
          default: break;
        }
      }
  THEN("toggles are two-valued, segments short lists, menus long lists");
  CHECK(ok, "widget choice");
}

SCENARIO(every_page_fits_without_overlap) {
  GIVEN("each page with every card open");
  const auto pages = fuzzboi_pages();
  bool fits = true, no_overlap = true, all_shown = true;
  for (size_t p = 0; p < pages.size(); ++p) {
    const PageLayout lay = layout_page(pages[p], std::vector<double>(pages[p].sections.size(), 1.0));
    size_t controls = 0;
    for (const SectionSpec& s : pages[p].sections) controls += s.controls.size();
    all_shown = all_shown && lay.controls.size() == controls;
    for (size_t i = 0; i < lay.sections.size(); ++i) {
      const Rect& card = lay.sections[i].card;
      fits = fits && card.x >= margin && card.right() <= width - margin && card.y >= lay.content_top && card.bottom() <= content_bottom;
      if (card.bottom() > content_bottom) std::printf("    page %zu card %zu ends at %d, below the content area (%d)\n", p, i, card.bottom(), content_bottom);
      for (size_t j = i + 1; j < lay.sections.size(); ++j) no_overlap = no_overlap && !card.overlaps(lay.sections[j].card);
    }
    for (size_t i = 0; i < lay.controls.size(); ++i)
      for (size_t j = i + 1; j < lay.controls.size(); ++j) no_overlap = no_overlap && !lay.controls[i].rect.overlaps(lay.controls[j].rect);
  }
  THEN("everything sits between the tabs and the footer, nothing overlaps, every control is visible");
  CHECK(fits, "cards fit");
  CHECK(no_overlap, "no overlap");
  CHECK(all_shown, "all controls shown");
  bool tabs_ok = true;
  for (int i = 0; i < tab_count; ++i) tabs_ok = tabs_ok && tab_at(tab_rect(i).x + 5, tab_y + 5) == i && tab_rect(i).right() <= width - margin;
  CHECK(tabs_ok, "tabs hit-test");
  CHECK(!preset_rect.overlaps(meter_rect) && !preset_rect.overlaps(size_rect) && preset_rect.bottom() < tab_y, "header chrome does not collide");
}

SCENARIO(tabs_switch_pages_with_an_animation_that_settles) {
  GIVEN("a click on the fourth tab");
  Rig rig;
  rig.click(1, tab_rect(3).x + 20, tab_y + 20);
  THEN("the page changes, animates, and comes to rest without any parameter edit");
  CHECK(rig.editor.page() == 3, "page 3");
  CHECK(rig.editor.tick(rig.now += 16.0), "animating");
  rig.settle();
  CHECK(!rig.editor.tick(rig.now += 16.0), "settled");
  CHECK(rig.host.begins == 0, "navigation is not an edit");
}

SCENARIO(sections_collapse_and_the_column_reflows) {
  GIVEN("the voice page's Envelope card, clicked closed");
  Rig rig;
  const PageLayout before = rig.editor.layout();
  const Rect header = before.sections[1].header;
  rig.click(1, header.x + 60, header.y + 10);
  rig.settle();
  THEN("the card shrinks to its header, the card below moves up, and the state survives a page change");
  const PageLayout after = rig.editor.layout();
  CHECK(!rig.editor.section_open(0, 1), "closed");
  CHECK(after.sections[1].card.h == card_header, "header only");
  CHECK(after.sections[2].card.y < before.sections[2].card.y, "reflowed");
  rig.editor.set_page(2); rig.settle(); rig.editor.set_page(0); rig.settle();
  CHECK(!rig.editor.section_open(0, 1), "remembered");
}

SCENARIO(knobs_drag_fine_tune_reset_and_wheel) {
  GIVEN("the attack knob on the voice page");
  Rig rig;
  const PlacedControl attack = rig.find(P::kAttackMs);
  CHECK(attack.rect.w > 0, "attack is on the first page");
  const int cx = attack.rect.x + attack.rect.w / 2, cy = attack.rect.y + 60;
  const auto& spec = P::kSpecs[P::kAttackMs];
  rig.host.values[P::kAttackMs] = spec.min;
  WHEN("it is dragged up by the full drag range");
  rig.down(1, cx, cy);
  for (int i = 1; i <= 10; ++i) rig.move(cx, cy - static_cast<int>(Editor::drag_range) * i / 10);
  THEN("it reaches its maximum inside one gesture");
  CHECK(std::abs(rig.host.values[P::kAttackMs] - spec.max) < 1e-9, "max");
  rig.up(1, cx, cy);
  CHECK(rig.host.idle() && rig.host.begins == 1 && rig.host.ends == 1, "one gesture");
  WHEN("Shift is held");
  rig.now += 1000.0;
  rig.host.values[P::kAttackMs] = spec.min;
  rig.down(1, cx, cy); rig.move(cx, cy - 100, kShift); rig.up(1, cx, cy - 100, kShift);
  THEN("the same movement is a tenth as large");
  CHECK_NEAR(rig.host.values[P::kAttackMs], (spec.max - spec.min) * 100.0 / Editor::drag_range * Editor::fine_factor, 1e-6, "fine drag");
  WHEN("it is right-clicked, Ctrl-clicked and double-clicked");
  rig.host.values[P::kAttackMs] = 900; rig.click(3, cx, cy);
  const bool right = rig.host.values[P::kAttackMs] == spec.def;
  rig.host.values[P::kAttackMs] = 900; rig.click(1, cx, cy, kControl);
  const bool ctrl = rig.host.values[P::kAttackMs] == spec.def;
  rig.host.values[P::kAttackMs] = 900; rig.down(1, cx, cy); rig.up(1, cx, cy); rig.now += 120.0; rig.down(1, cx, cy); rig.up(1, cx, cy);
  const bool dbl = rig.host.values[P::kAttackMs] == spec.def;
  THEN("each resets to the default");
  CHECK(right && ctrl && dbl && rig.host.idle(), "resets");
  WHEN("the wheel turns over it");
  rig.now += 1000.0;
  rig.host.values[P::kAttackMs] = 100;
  rig.wheel(1, cx, cy);
  THEN("the value nudges by the wheel fraction");
  CHECK_NEAR(rig.host.values[P::kAttackMs], 100 + (spec.max - spec.min) * Editor::wheel_fraction, 1e-9, "wheel");
  GIVEN("a stepped knob (unison)");
  const PlacedControl unison = rig.find(P::kUnison);
  rig.host.values[P::kUnison] = 1;
  rig.down(1, unison.rect.x + unison.rect.w / 2, unison.rect.y + 60);
  for (int i = 1; i <= 40; ++i) rig.move(unison.rect.x + unison.rect.w / 2, unison.rect.y + 60 - i * 3);
  rig.up(1, unison.rect.x + unison.rect.w / 2, unison.rect.y - 60);
  THEN("small movements accumulate and the value stays an integer");
  CHECK(rig.host.values[P::kUnison] > 1 && rig.host.values[P::kUnison] == std::round(rig.host.values[P::kUnison]), "stepped drag");
  CHECK(rig.host.violations == 0, "no gesture violations");
}

SCENARIO(toggles_segments_and_menus_choose_options) {
  GIVEN("the voice mode segments and the source menu");
  Rig rig;
  const PlacedControl mode = rig.find(P::kVoiceMode);
  for (int option = 0; option < 3; ++option) {
    const int x = mode.rect.x + 8 + (mode.rect.w - 16) * (2 * option + 1) / 6;
    rig.click(1, x, mode.rect.y + 60);
    CHECK(rig.host.values[P::kVoiceMode] == option, spec::fmt("segment %.0f selects its option", option));
  }
  const PlacedControl source = rig.find(P::kSource);
  rig.click(1, source.rect.x + 40, source.rect.y + 60);
  THEN("the menu opens without an edit and lists every source");
  CHECK(rig.editor.menu_open() && rig.editor.menu_param() == P::kSource, "menu open");
  rig.settle();
  RecordingCanvas canvas;
  rig.editor.draw(canvas);
  bool listed = false;
  for (const std::string& s : canvas.strings) listed = listed || s == P::kSourceNames[P::kSourceCount - 1];
  CHECK(listed && canvas.outside == 0, "every source listed inside the editor");
  WHEN("the second item is clicked");
  rig.click(1, source.rect.x + 8 + 30, source.rect.bottom() + 4 + 8 + 1 * menu_item_height + 10);
  THEN("the source changes inside one gesture and the menu closes");
  CHECK(!rig.editor.menu_open() && rig.host.values[P::kSource] == 1 && rig.host.idle(), "item chosen");
  WHEN("the header preset menu is opened from another page and the wheel turns");
  rig.editor.set_page(5); rig.settle();
  rig.click(1, preset_rect.x + 50, preset_rect.y + 20);
  CHECK(rig.editor.menu_param() == P::kPreset, "preset menu");
  rig.settle();
  RecordingCanvas presets;
  rig.editor.draw(presets);
  CHECK(presets.outside == 0, "the preset catalogue stays inside the window");
  rig.click(1, 2, 2);
  const double before = rig.host.values[P::kPreset];
  rig.wheel(1, preset_rect.x + 50, preset_rect.y + 20);
  CHECK(rig.host.values[P::kPreset] == before + 1 && rig.host.idle(), "wheel steps the preset");
  GIVEN("the echo sync toggle on the echo page");
  CHECK(rig.go_to(P::kEchoSync), "found");
  const PlacedControl sync = rig.find(P::kEchoSync);
  const double was = rig.host.values[P::kEchoSync];
  rig.click(1, sync.rect.x + sync.rect.w / 2, sync.rect.y + 60);
  THEN("it flips");
  CHECK(rig.host.values[P::kEchoSync] == 1.0 - was && rig.host.idle(), "toggle");
  CHECK(rig.host.violations == 0, "no gesture violations");
}

SCENARIO(lost_releases_and_pointer_leave_never_leak_gestures) {
  GIVEN("a drag whose release is lost");
  Rig rig;
  const PlacedControl attack = rig.find(P::kAttackMs), release = rig.find(P::kReleaseMs);
  rig.down(1, attack.rect.x + 30, attack.rect.y + 60);
  rig.editor.pointer(PointerAction::Leave, 0, 0, 0, 0, rig.now);
  CHECK(rig.editor.dragging() == P::kAttackMs, "leaving the window keeps the drag");
  rig.now += 1000.0;
  rig.down(1, release.rect.x + 30, release.rect.y + 60);
  THEN("the next press closes the old gesture before opening a new one");
  CHECK(!rig.host.open.count(P::kAttackMs) && rig.host.open.count(P::kReleaseMs), "gesture handover");
  rig.up(1, release.rect.x + 30, release.rect.y + 60);
  rig.up(1, 10, 10);
  CHECK(rig.host.idle() && rig.host.violations == 0, "clean");
}

SCENARIO(strip_clicks_reach_the_host_and_the_size_button_works) {
  GIVEN("the tape page's artwork strip");
  Rig rig;
  rig.editor.set_page(4); rig.settle();
  const PageLayout lay = rig.editor.layout();
  CHECK(lay.strip.h > 0, "the tape page has a strip");
  rig.click(3, lay.strip.x + 40, lay.strip.y + 40);
  THEN("the click reaches the host with its page and button");
  CHECK(rig.host.strip_clicks.size() == 1 && rig.host.strip_clicks[0] == 43, "strip click");
  rig.editor.set_page(5); rig.settle();
  CHECK(rig.editor.layout().strip.h == 0, "the toys page has no strip");
  GIVEN("a host that can resize");
  rig.host.percent = 100;
  const int x = size_rect.x + 20, y = size_rect.y + 20;
  bool steps = true;
  for (const int expected : {90, 75, 60, 50, 125, 100}) { rig.click(1, x, y); steps = steps && rig.host.requested.back() == expected; }
  THEN("clicks step the size down and wrap, right-click restores 100%, none of it is an edit");
  CHECK(steps, "size steps");
  rig.click(3, x, y);
  CHECK(rig.host.requested.back() == 100 && rig.host.edits == 0, "right-click restores");
}

SCENARIO(every_page_draws_inside_the_editor_with_every_label) {
  GIVEN("each page, settled, collapsed, hovered, and at extreme values");
  Rig rig;
  bool labels = true, inside = true;
  for (int page = 0; page < rig.editor.page_count(); ++page) {
    rig.editor.set_page(page);
    for (int frame = 0; frame < 3; ++frame) { RecordingCanvas c; rig.editor.draw(c); rig.editor.tick(rig.now += 16.0); inside = inside && c.outside == 0; }
    rig.settle();
    RecordingCanvas canvas;
    rig.editor.draw(canvas);
    inside = inside && canvas.outside == 0 && canvas.texts > 10 && canvas.rects > 10;
    const PageLayout lay = rig.editor.layout();
    for (const PlacedControl& placed : lay.controls) {
      const ControlSpec& spec = rig.editor.page_spec(page).sections[static_cast<size_t>(placed.section)].controls[static_cast<size_t>(placed.control)];
      const char* label = rig.host.short_label(spec.param);
      bool shown = false;
      for (const std::string& s : canvas.strings)
        shown = shown || s == label || (s.size() > 3 && s.compare(s.size() - 3, 3, "...") == 0 && std::string(label).rfind(s.substr(0, s.size() - 3), 0) == 0);
      if (!shown) { labels = false; std::printf("    label '%s' not drawn on page %d\n", label, page); }
      rig.move(placed.rect.x + placed.rect.w / 2, placed.rect.y + 60);
      RecordingCanvas hovered; rig.editor.draw(hovered); inside = inside && hovered.outside == 0;
    }
    for (size_t s = 0; s < lay.sections.size(); ++s) rig.editor.set_section_open(page, static_cast<int>(s), false);
    rig.settle();
    RecordingCanvas collapsed; rig.editor.draw(collapsed);
    inside = inside && collapsed.outside == 0 && rig.editor.layout().controls.empty();
    for (size_t s = 0; s < lay.sections.size(); ++s) rig.editor.set_section_open(page, static_cast<int>(s), true);
  }
  for (int id = 0; id < kParams; ++id) rig.host.values[static_cast<size_t>(id)] = P::kSpecs[static_cast<size_t>(id)].max;
  for (int page = 0; page < rig.editor.page_count(); ++page) { rig.editor.set_page(page); rig.settle(); RecordingCanvas c; rig.editor.draw(c); inside = inside && c.outside == 0; }
  THEN("every label is drawn and nothing is drawn outside the design space");
  CHECK(labels, "labels");
  CHECK(inside, "inside");
}

SCENARIO(short_names_fit_a_cell_and_ellipsis_respects_utf8) {
  GIVEN("every short name and a multibyte string");
  bool fit = true;
  for (int id = 0; id < kParams; ++id) if (const char* name = fuzzboi_short_name(id)) fit = fit && std::strlen(name) <= 14;
  THEN("names are at most 14 characters and the ellipsis cuts between characters");
  CHECK(fit, "short names");
  RecordingCanvas canvas;
  const int limit = canvas.text_width("é...", TextSize::Small);
  CHECK(canvas.fit("éééé", limit, TextSize::Small) == "é...", "utf8 ellipsis");
  CHECK(canvas.fit("Long label", 1, TextSize::Small).empty(), "nothing fits in one unit");
}

SCENARIO(presets_have_unique_names_that_fit_the_header) {
  GIVEN("the preset table");
  std::set<std::string> names;
  bool unique = true, fits = true, valid = true;
  for (const auto& preset : fuzzboi::presets::all()) {
    unique = unique && names.insert(preset.name).second;
    fits = fits && std::strlen(preset.name) <= 32;
    for (const auto& s : preset.settings) valid = valid && s.value >= P::kSpecs[s.id].min && s.value <= P::kSpecs[s.id].max && s.id != P::kPreset;
  }
  THEN("names are unique and short, and every setting is inside its parameter's range");
  CHECK(unique, "unique");
  CHECK(fits, "fit");
  CHECK(valid, "in range");
}

// --- the output scope and the visualizer ---------------------------------------------------

constexpr double kRate = 48000.0;
constexpr double kTau = 6.283185307179586;
double db(double gain) { return 20.0 * std::log10(gain); }

// The scope ring as the plug-in fills it: the last kScopeSamples samples of `signal`, ending at
// time `end` seconds.
template <typename F>
std::array<float, kScopeSamples> ring_at(double end, F signal) {
  std::array<float, kScopeSamples> ring{};
  for (size_t i = 0; i < ring.size(); ++i) ring[i] = static_cast<float>(signal(end - static_cast<double>(ring.size() - i) / kRate));
  return ring;
}

// Steps the visualizer through `seconds` of `signal` at 30 frames per second.
template <typename F>
double run(Visualizer& v, double start, double seconds, F signal) {
  double t = start;
  for (int frame = 0; frame < static_cast<int>(seconds * 30.0); ++frame) {
    t += 1.0 / 30.0;
    const auto ring = ring_at(t, signal);
    v.step(ring.data(), ring.size(), kRate, 1.0 / 30.0);
  }
  return t;
}

SCENARIO(scope_holds_still_and_amplifies_quiet_output) {
  GIVEN("a 220 Hz sine at -24 dBFS, captured at two unrelated moments");
  const double amp = std::pow(10.0, -24.0 / 20.0);
  const auto sine = [amp](double t) { return amp * std::sin(kTau * 220.0 * t); };
  const ScopeTrace a = scope_trace(ring_at(1.0, sine), kRate, 384), b = scope_trace(ring_at(1.0137, sine), kRate, 384);
  THEN("it is amplified by the 23.1 dB that brings its peak to 0.9 of the frame, and says so");
  CHECK_NEAR(db(a.gain), db(0.9 / amp), 0.1, "gain in dB");
  float peak = 0.0f;
  for (float x : a.points) peak = std::max(peak, std::abs(x));
  CHECK_NEAR(static_cast<double>(peak), 0.9, 0.02, "trace peak");
  THEN("the trace starts on a rising zero crossing, so both captures draw the same picture");
  CHECK(std::abs(a.points[0]) < 0.05f && a.points[3] > a.points[0], "starts rising from zero");
  double worst = 0.0;
  for (size_t i = 0; i < a.points.size(); ++i) worst = std::max(worst, static_cast<double>(std::abs(a.points[i] - b.points[i])));
  CHECK(worst < 0.05, spec::fmt("captures agree within 5%% of the frame: worst %.3f", worst));
  THEN("the window is 20 ms: 4.4 cycles of 220 Hz");
  int rising = 0;
  for (size_t i = 1; i < a.points.size(); ++i) rising += a.points[i - 1] < 0.0f && a.points[i] >= 0.0f;
  CHECK(rising == 4, spec::fmt("four rising crossings after the trigger: got %.0f", static_cast<double>(rising)));
  GIVEN("silence, hiss at -90 dBFS, a quiet sine at -40 dBFS and a full-scale sine");
  const ScopeTrace silent = scope_trace(ring_at(1.0, [](double) { return 0.0; }), kRate, 384);
  const ScopeTrace hiss = scope_trace(ring_at(1.0, [](double t) { return 3e-5 * std::sin(kTau * 7919.0 * t); }), kRate, 384);
  const ScopeTrace quiet = scope_trace(ring_at(1.0, [](double t) { return 0.01 * std::sin(kTau * 220.0 * t); }), kRate, 384);
  const ScopeTrace loud = scope_trace(ring_at(1.0, [](double t) { return 0.95 * std::sin(kTau * 220.0 * t); }), kRate, 384);
  THEN("silence and hiss are not blown up, the gain stops at +24 dB, and loud output is never turned down");
  CHECK(silent.gain == 1.0 && hiss.gain == 1.0, "no gain on silence or hiss");
  CHECK_NEAR(db(quiet.gain), 24.08, 0.01, "gain limit");
  CHECK(loud.gain == 1.0, "unity on loud output");
}

SCENARIO(visualizer_lights_up_with_sound_and_its_trails_fade_in_silence) {
  GIVEN("the visualizer on each preset, fed two seconds of silence");
  bool dark = true, lit = true, trails = true, fades = true;
  for (int preset = 0; preset < kVisualPresetCount; ++preset) {
    Visualizer v;
    v.set_preset(preset);
    double t = run(v, 0.0, 2.0, [](double) { return 0.0; });
    dark = dark && v.brightness() < 1e-3;
    WHEN("a 220 Hz sine at -24 dBFS plays for two seconds");
    t = run(v, t, 2.0, [](double s) { return 0.063 * std::sin(kTau * 220.0 * s); });
    const double playing = v.brightness();
    lit = lit && playing > 0.03;
    WHEN("it stops");
    t = run(v, t, 0.2, [](double) { return 0.0; });
    const double tail = v.brightness();
    run(v, t, 4.0, [](double) { return 0.0; });
    trails = trails && tail > 0.3 * playing;
    fades = fades && v.brightness() < 0.02 * playing;
    if (playing <= 0.03 || tail <= 0.3 * playing || v.brightness() >= 0.02 * playing)
      std::printf("    %s: playing %.4f, after 0.2 s %.4f, after 4.2 s %.5f\n", v.preset_name(), playing, tail, v.brightness());
  }
  THEN("silence stays dark, sound lights the picture, and the light trails on then fades away");
  CHECK(dark, "dark in silence");
  CHECK(lit, "lit by sound");
  CHECK(trails, "trails persist 0.2 s later at 30% or more");
  CHECK(fades, "gone below 2% four seconds later");
}

SCENARIO(visualizer_hears_bass_and_treble_hits) {
  GIVEN("the visualizer settled on a steady 440 Hz tone at -20 dBFS");
  Visualizer v;
  const auto tone = [](double t) { return 0.1 * std::sin(kTau * 440.0 * t); };
  double t = run(v, 0.0, 3.0, tone);
  THEN("bass and treble read as ordinary, about 1");
  CHECK(v.bass() < 1.3 && v.treble() < 1.3, spec::fmt("steady: bass %.2f, treble %.2f", v.bass(), v.treble()));
  WHEN("a 60 Hz kick at -12 dBFS lands");
  t = run(v, t, 0.1, [&](double s) { return tone(s) + 0.25 * std::sin(kTau * 60.0 * s); });
  THEN("bass jumps well above its average and treble does not");
  CHECK(v.bass() > 1.8 && v.treble() < 1.3, spec::fmt("kick: bass %.2f, treble %.2f", v.bass(), v.treble()));
  WHEN("the kick has passed and a 6 kHz hit lands instead");
  t = run(v, t, 4.0, tone);
  run(v, t, 0.1, [&](double s) { return tone(s) + 0.1 * std::sin(kTau * 6000.0 * s); });
  THEN("treble jumps and bass does not");
  CHECK(v.treble() > 1.8 && v.bass() < 1.3, spec::fmt("hat: bass %.2f, treble %.2f", v.bass(), v.treble()));
}

SCENARIO(visualizer_presets_cycle_like_loamps) {
  GIVEN("the visualizer in its strip");
  const Rect strip{margin, strip_y, width - 2 * margin, content_bottom - strip_y};
  Visualizer v;
  const auto press = [&](int button, const Rect& r) { return v.pointer(button, r.x + r.w / 2, r.y + r.h / 2, strip); };
  WHEN("next, previous and previous again are clicked");
  press(1, Visualizer::button_rect(strip, Visualizer::kNext));
  const int after_next = v.preset();
  press(1, Visualizer::button_rect(strip, Visualizer::kPrevious));
  press(1, Visualizer::button_rect(strip, Visualizer::kPrevious));
  THEN("the preset steps forward, back, and wraps to the last");
  CHECK(after_next == 1 && v.preset() == kVisualPresetCount - 1, "next, previous, wrap");
  WHEN("the picture is left- and right-clicked, and something outside it is clicked");
  press(1, Visualizer::picture_rect(strip));
  const int after_left = v.preset();
  press(3, Visualizer::picture_rect(strip));
  THEN("left-click moves on, right-click goes back, outside clicks are not taken");
  CHECK(after_left == 0 && v.preset() == kVisualPresetCount - 1, "picture clicks");
  CHECK(!v.pointer(1, strip.x - 50, strip.y - 50, strip), "outside ignored");
  WHEN("random is clicked twenty times");
  bool moved = true;
  std::set<int> seen;
  for (int i = 0; i < 20; ++i) { const int before = v.preset(); press(1, Visualizer::button_rect(strip, Visualizer::kRandom)); moved = moved && v.preset() != before; seen.insert(v.preset()); }
  THEN("each pick is a different preset and the picks range over the list");
  CHECK(moved && seen.size() >= 5, "random moves");
  WHEN("auto is switched on and 21 seconds pass");
  press(1, Visualizer::button_rect(strip, Visualizer::kAuto));
  const int before_auto = v.preset();
  run(v, 0.0, 21.0, [](double t) { return 0.05 * std::sin(kTau * 330.0 * t); });
  THEN("it has moved on by itself");
  CHECK(v.auto_cycle() && v.preset() != before_auto, "auto cycles");
  GIVEN("every preset fed the same three seconds of music");
  std::vector<std::vector<float>> pictures;
  bool finite = true, inside = true;
  for (int preset = 0; preset < kVisualPresetCount; ++preset) {
    Visualizer each;
    each.set_preset(preset);
    run(each, 0.0, 3.0, [](double t) { return 0.05 * std::sin(kTau * 220.0 * t) + 0.12 * std::exp(-std::fmod(t, 0.5) * 18.0) * std::sin(kTau * 55.0 * t); });
    for (float c : each.cells()) finite = finite && std::isfinite(c) && c >= 0.0f;
    pictures.push_back(each.cells());
    RecordingCanvas canvas;
    Painter painter(canvas);
    each.draw(painter, strip);
    inside = inside && canvas.outside == 0 && canvas.rects > 10 && canvas.lines > 0;
  }
  THEN("each draws a finite picture inside the editor, unlike every other preset's");
  CHECK(finite, "finite, non-negative light");
  CHECK(inside, "drawn inside");
  bool distinct = true;
  for (size_t i = 0; i < pictures.size(); ++i)
    for (size_t j = i + 1; j < pictures.size(); ++j) {
      double diff = 0.0;
      for (size_t k = 0; k < pictures[i].size(); ++k) diff += std::abs(pictures[i][k] - pictures[j][k]);
      diff /= static_cast<double>(pictures[i].size());
      if (diff < 0.02) { distinct = false; std::printf("    presets %zu and %zu differ by only %.4f\n", i, j, diff); }
    }
  CHECK(distinct, "presets look different");
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
