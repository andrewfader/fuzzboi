// FUZZBOI's CLAP plug-in: the thin layer between a host and the engine. Parameters live in
// atomics the host and editor write and the audio thread snapshots once per block; notes,
// MIDI and automation arrive as CLAP events with sample offsets; state is a versioned blob
// with the parameter values and any loaded samples. The native editor, when built, is included
// from ui/ at the bottom of the anonymous namespace so it can see the Plugin.
#include <clap/clap.h>

#if defined(FUZZBOI_EDITOR) && (defined(__linux__) || defined(_WIN32) || defined(__APPLE__))
#define FUZZBOI_HAS_EDITOR 1
#endif

#ifdef FUZZBOI_HAS_EDITOR
#include "ui/canvas.hpp"
#if defined(__linux__)
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include "ui/canvas_x11.hpp"
#elif defined(_WIN32)
#include "ui/canvas_win32.hpp"
#include <commdlg.h>
#elif defined(__APPLE__)
#include "ui/canvas_cocoa.hpp"
#endif
#include "ui/artwork.hpp"
#include "ui/visualizer.hpp"
#include "ui/editor.hpp"
#include "ui/layout.hpp"
#include "ui/pages.hpp"
#endif

#include "engine.hpp"
#include "params.hpp"
#include "presets.hpp"
#include "wav.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

#if defined(FUZZBOI_HAS_EDITOR) && defined(__APPLE__)
// The Objective-C runtime keeps one flat class namespace per process; version the view's name so
// two generations of the plug-in scanned by one host do not collide.
#define FUZZBOI_EDITOR_VIEW FuzzboiEditorView_0_1_0
@interface FUZZBOI_EDITOR_VIEW : NSView
@property(assign, nonatomic) void* plugin;
@property(assign, nonatomic) NSTimer* refresh;
@end
#endif

namespace {

using namespace fuzzboi;
using namespace fuzzboi::params;

constexpr uint32_t kStateMagic = 0x49425a46u;  // "FZBI"
constexpr uint32_t kStateVersion = 1;
constexpr size_t kMaxSampleFrames = 4u * 1024u * 1024u;

// What the editor wants the host to know about: a gesture begin/end or a value, queued from the
// main thread and emitted on the audio thread (or in flush) as CLAP output events.
struct GuiEvent { enum Kind : uint8_t { Begin, Value, End } kind; clap_id id; double value; };

struct Plugin {
  clap_plugin_t api{};
  const clap_host_t* host{};
  const clap_host_tail_t* host_tail{};
  bool initialized = false;
  double sample_rate = 48000.0;

  std::array<std::atomic<double>, kParamCount> params{};
  // The host's modulation offsets, kept apart from the values so neither erases the other.
  std::array<std::atomic<double>, kParamCount> modulation{};
  Engine engine;
  Settings settings = Settings::defaults();
  std::atomic<bool> params_rescan_pending{false};

  // Editor -> host events (single producer on the main thread, single consumer on the audio thread).
  std::array<GuiEvent, 1024> gui_queue{};
  std::atomic<uint32_t> gui_write{0}, gui_read{0};
  // Something the editor should redraw for: incremented whenever a parameter changes.
  std::atomic<uint64_t> ui_revision{1};
  // The most recent output, every sample, for the editor's scope and visualizer.
  static constexpr uint32_t kScopeSize = 4096;
  std::array<std::atomic<float>, kScopeSize> scope{};
  std::atomic<uint32_t> scope_write{0};
  std::atomic<uint64_t> scope_revision{0};
  std::atomic<float> peak_l{0.0f}, peak_r{0.0f};
  std::atomic<bool> clipped{false};
  std::atomic<int> active_voices{0};

#ifdef FUZZBOI_HAS_EDITOR
  uint32_t gui_width = fuzzboi::ui::width, gui_height = fuzzboi::ui::height;
  uint32_t gui_request_width = 0, gui_request_height = 0;
  uint64_t gui_seen_revision = 0, gui_seen_scope_revision = 0;
  int gui_scope_ticks = 0;
  std::unique_ptr<fuzzboi::ui::Editor> editor;
  std::unique_ptr<fuzzboi::ui::EditorHost> editor_host;
#if defined(__linux__)
  const clap_host_timer_support_t* host_timers{};
  const clap_host_posix_fd_support_t* host_fds{};
  Display* display{};
  Window window{};
  GC gc{};
  Pixmap gui_back{};
  XftDraw* gui_back_xft{};
  int gui_back_width = 0, gui_back_height = 0;
  std::array<XftFont*, 3> gui_fonts{};
  std::array<int, 3> gui_font_pixels{};
  clap_id gui_timer = CLAP_INVALID_ID;
  int gui_fd = -1;
  int gui_picker = -1, gui_picker_slot = -1;
  pid_t gui_picker_pid = -1;
  std::string gui_picker_output;
#elif defined(_WIN32)
  HWND hwnd{};
#elif defined(__APPLE__)
  FUZZBOI_EDITOR_VIEW* view{};
#endif
#endif
};

Plugin* self(const clap_plugin_t* plugin) { return static_cast<Plugin*>(plugin->plugin_data); }

double param_value(const Plugin* p, clap_id id) { return p->params[id].load(std::memory_order_relaxed); }

void set_param(Plugin* p, clap_id id, double value) {
  p->params[id].store(clamp_to_spec(id, value), std::memory_order_relaxed);
  p->ui_revision.fetch_add(1, std::memory_order_release);
}

// A preset is applied by writing every parameter; the host is told to rescan on its main
// thread, since that is the only place rescan may be called.
void apply_preset(Plugin* p, int index) {
  std::array<double, kParamCount> values{};
  presets::apply(static_cast<size_t>(std::clamp<int>(index, 0, static_cast<int>(presets::count()) - 1)), values.data());
  for (clap_id i = 0; i < kParamCount; ++i) p->params[i].store(values[i], std::memory_order_relaxed);
  p->ui_revision.fetch_add(1, std::memory_order_release);
  p->params_rescan_pending.store(true, std::memory_order_release);
  if (p->initialized && p->host && p->host->request_callback) p->host->request_callback(p->host);
}

// --- samples --------------------------------------------------------------------------------------

// "toy_piano_C4.wav" or "jug-a3.wav": a trailing note name sets the root pitch. Defaults to C4.
double root_hz_from_name(const std::string& stem) {
  static const char* const names[12] = {"c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b"};
  std::string lower;
  for (char c : stem) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  const size_t sep = lower.find_last_of("_- ");
  const std::string tail = sep == std::string::npos ? lower : lower.substr(sep + 1);
  for (int pc = 11; pc >= 0; --pc) {
    const std::string n = names[pc];
    if (tail.size() > n.size() && tail.compare(0, n.size(), n) == 0) {
      const char* digits = tail.c_str() + n.size();
      char* end = nullptr;
      const long octave = std::strtol(digits, &end, 10);
      if (end != digits && *end == '\0' && octave >= -1 && octave <= 9)
        return dsp::midi_to_hz(static_cast<double>((octave + 1) * 12 + pc));
    }
  }
  return dsp::midi_to_hz(60);
}

std::shared_ptr<const dsp::Sample> load_sample_file(const std::string& path) {
  wav::Audio audio;
  if (!wav::read_file(path.c_str(), audio) || audio.mono.empty()) return nullptr;
  auto sample = std::make_shared<dsp::Sample>();
  if (audio.mono.size() > kMaxSampleFrames) audio.mono.resize(kMaxSampleFrames);
  sample->frames = std::move(audio.mono);
  sample->rate = audio.rate;
  sample->name = std::filesystem::path(path).stem().string();
  sample->root_hz = root_hz_from_name(sample->name);
  sample->source = path;
  return sample;
}

bool load_and_install_sample(Plugin* p, int slot, const std::string& path) {
  auto sample = load_sample_file(path);
  if (!sample) return false;
  p->engine.set_sample(slot, sample);
  p->ui_revision.fetch_add(1, std::memory_order_release);
  return true;
}

// FUZZBOI_SAMPLES names a directory (WAVs are loaded into slots in name order) or a
// path-separated list of files (one per slot).
void load_samples_from_environment(Plugin* p) {
  const char* value = std::getenv("FUZZBOI_SAMPLES");
  if (!value || !*value) return;
  std::error_code ec;
  if (std::filesystem::is_directory(value, ec)) {
    std::vector<std::string> files;
    for (const auto& entry : std::filesystem::directory_iterator(value, ec)) {
      if (!entry.is_regular_file()) continue;
      std::string ext = entry.path().extension().string();
      for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (ext == ".wav") files.push_back(entry.path().string());
    }
    std::sort(files.begin(), files.end());
    for (size_t i = 0; i < files.size() && i < static_cast<size_t>(kSampleSlots); ++i) load_and_install_sample(p, static_cast<int>(i), files[i]);
    return;
  }
#ifdef _WIN32
  constexpr char separator = ';';
#else
  constexpr char separator = ':';
#endif
  std::string_view list(value);
  for (int slot = 0, at = 0; slot < kSampleSlots && static_cast<size_t>(at) <= list.size(); ++slot) {
    const size_t end = list.find(separator, static_cast<size_t>(at));
    const std::string path(list.substr(static_cast<size_t>(at), end == std::string_view::npos ? list.size() - static_cast<size_t>(at) : end - static_cast<size_t>(at)));
    if (!path.empty()) load_and_install_sample(p, slot, path);
    if (end == std::string_view::npos) break;
    at = static_cast<int>(end) + 1;
  }
}

// --- events ---------------------------------------------------------------------------------------

[[maybe_unused]] void gui_push(Plugin* p, GuiEvent::Kind kind, clap_id id, double value) {
  const uint32_t write = p->gui_write.load(std::memory_order_relaxed);
  const uint32_t next = (write + 1) % static_cast<uint32_t>(p->gui_queue.size());
  if (next == p->gui_read.load(std::memory_order_acquire)) return;  // full: drop rather than block
  p->gui_queue[write] = {kind, id, value};
  p->gui_write.store(next, std::memory_order_release);
}

void emit_gui_events(Plugin* p, const clap_output_events_t* out) {
  uint32_t read = p->gui_read.load(std::memory_order_relaxed);
  const uint32_t write = p->gui_write.load(std::memory_order_acquire);
  while (read != write) {
    const GuiEvent& e = p->gui_queue[read];
    if (out && out->try_push) {
      if (e.kind == GuiEvent::Value) {
        clap_event_param_value_t ev{};
        ev.header = {sizeof(ev), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
        ev.param_id = e.id; ev.note_id = -1; ev.port_index = -1; ev.channel = -1; ev.key = -1; ev.value = e.value;
        out->try_push(out, &ev.header);
      } else {
        clap_event_param_gesture_t ev{};
        ev.header = {sizeof(ev), 0, CLAP_CORE_EVENT_SPACE_ID,
                     static_cast<uint16_t>(e.kind == GuiEvent::Begin ? CLAP_EVENT_PARAM_GESTURE_BEGIN : CLAP_EVENT_PARAM_GESTURE_END), 0};
        ev.param_id = e.id;
        out->try_push(out, &ev.header);
      }
    }
    read = (read + 1) % static_cast<uint32_t>(p->gui_queue.size());
  }
  p->gui_read.store(read, std::memory_order_release);
}

void snapshot_settings(Plugin* p) {
  for (size_t i = 0; i < kParamCount; ++i) {
    const double base = p->params[i].load(std::memory_order_relaxed) + p->modulation[i].load(std::memory_order_relaxed);
    p->settings.v[i] = clamp_to_spec(static_cast<uint32_t>(i), base);
  }
}

void handle_event(Plugin* p, const clap_event_header_t* ev) {
  if (!ev || ev->space_id != CLAP_CORE_EVENT_SPACE_ID) return;
  switch (ev->type) {
    case CLAP_EVENT_NOTE_ON: {
      const auto* n = reinterpret_cast<const clap_event_note_t*>(ev);
      if (n->key < 0 || n->key > 127) return;
      p->engine.note_on(p->settings, std::max<int16_t>(0, n->channel), n->key, n->note_id, std::clamp(n->velocity, 0.0, 1.0));
      return;
    }
    case CLAP_EVENT_NOTE_OFF: {
      const auto* n = reinterpret_cast<const clap_event_note_t*>(ev);
      p->engine.note_off(p->settings, n->channel, n->key, n->note_id);
      return;
    }
    case CLAP_EVENT_NOTE_CHOKE: p->engine.all_sound_off(); return;
    case CLAP_EVENT_PARAM_VALUE: {
      const auto* v = reinterpret_cast<const clap_event_param_value_t*>(ev);
      if (v->param_id >= kParamCount) return;
      if (v->param_id == kPreset) {
        const int index = static_cast<int>(std::lround(v->value));
        if (index != static_cast<int>(std::lround(param_value(p, kPreset)))) apply_preset(p, index);
        else set_param(p, kPreset, v->value);
        snapshot_settings(p);
        return;
      }
      set_param(p, v->param_id, v->value);
      snapshot_settings(p);
      return;
    }
    case CLAP_EVENT_PARAM_MOD: {
      const auto* m = reinterpret_cast<const clap_event_param_mod_t*>(ev);
      if (m->param_id >= kParamCount || kSpecs[m->param_id].stepped) return;
      p->modulation[m->param_id].store(std::isfinite(m->amount) ? m->amount : 0.0, std::memory_order_relaxed);
      snapshot_settings(p);
      return;
    }
    case CLAP_EVENT_TRANSPORT: {
      const auto* t = reinterpret_cast<const clap_event_transport_t*>(ev);
      if (t->flags & CLAP_TRANSPORT_HAS_TEMPO) p->engine.set_tempo(t->tempo);
      return;
    }
    case CLAP_EVENT_MIDI: {
      const auto* m = reinterpret_cast<const clap_event_midi_t*>(ev);
      const uint8_t status = m->data[0] & 0xf0u, channel = m->data[0] & 0x0fu;
      switch (status) {
        case 0x90:
          if (m->data[2] > 0) { p->engine.note_on(p->settings, channel, m->data[1], -1, m->data[2] / 127.0); return; }
          [[fallthrough]];
        case 0x80: p->engine.note_off(p->settings, channel, m->data[1], -1); return;
        case 0xb0:
          if (m->data[1] == 64) p->engine.sustain(channel, m->data[2] >= 64);
          else if (m->data[1] == 1) p->engine.mod_wheel(channel, m->data[2] / 127.0);
          else if (m->data[1] == 120) p->engine.all_sound_off();
          else if (m->data[1] == 123) p->engine.all_notes_off();
          return;
        case 0xe0: {
          const int value = (m->data[2] << 7) | m->data[1];
          p->engine.pitch_bend(channel, (value - 8192) / 8192.0);
          return;
        }
        default: return;
      }
    }
    default: return;
  }
}

// --- the plug-in -------------------------------------------------------------------------------------

#ifdef FUZZBOI_HAS_EDITOR
void gui_destroy(const clap_plugin_t* plugin);
bool gui_is_open(const Plugin* p);
#endif

bool plugin_init(const clap_plugin_t* plugin) {
  auto* p = self(plugin);
  p->initialized = true;
  if (p->host && p->host->get_extension) {
    p->host_tail = static_cast<const clap_host_tail_t*>(p->host->get_extension(p->host, CLAP_EXT_TAIL));
#if defined(FUZZBOI_HAS_EDITOR) && defined(__linux__)
    p->host_timers = static_cast<const clap_host_timer_support_t*>(p->host->get_extension(p->host, CLAP_EXT_TIMER_SUPPORT));
    p->host_fds = static_cast<const clap_host_posix_fd_support_t*>(p->host->get_extension(p->host, CLAP_EXT_POSIX_FD_SUPPORT));
#endif
  }
  load_samples_from_environment(p);
  return true;
}

void plugin_destroy(const clap_plugin_t* plugin) {
  auto* p = self(plugin);
  p->initialized = false;
#ifdef FUZZBOI_HAS_EDITOR
  if (gui_is_open(p)) gui_destroy(plugin);
#endif
  delete p;
}

bool plugin_activate(const clap_plugin_t* plugin, double rate, uint32_t, uint32_t) {
  auto* p = self(plugin);
  if (!std::isfinite(rate) || rate < 8000.0 || rate > 768000.0) return false;
  p->sample_rate = rate;
  p->engine.prepare(rate);
  snapshot_settings(p);
  return true;
}
void plugin_deactivate(const clap_plugin_t*) {}
bool plugin_start(const clap_plugin_t*) { return true; }
void plugin_stop(const clap_plugin_t*) {}
void plugin_reset(const clap_plugin_t* plugin) {
  auto* p = self(plugin);
  for (auto& m : p->modulation) m.store(0.0, std::memory_order_relaxed);
  p->engine.reset();
  snapshot_settings(p);
}

clap_process_status plugin_process(const clap_plugin_t* plugin, const clap_process_t* process) {
  auto* p = self(plugin);
  if (process->audio_outputs_count == 0 || !process->audio_outputs[0].data32) return CLAP_PROCESS_ERROR;
  auto& out = process->audio_outputs[0];
  if (out.channel_count < 2 || !out.data32[0] || !out.data32[1]) return CLAP_PROCESS_ERROR;
  if (process->transport && (process->transport->flags & CLAP_TRANSPORT_HAS_TEMPO)) p->engine.set_tempo(process->transport->tempo);
  snapshot_settings(p);
  const clap_input_events_t* in = process->in_events;
  const uint32_t count = in ? in->size(in) : 0;
  uint32_t next_event = 0, frame = 0;
  const uint32_t frames = process->frames_count;
  float* left = out.data32[0];
  float* right = out.data32[1];
  // Split the block at every event so notes and automation land on their sample.
  while (frame < frames) {
    uint32_t until = frames;
    while (next_event < count) {
      const clap_event_header_t* ev = in->get(in, next_event);
      if (!ev) { ++next_event; continue; }
      if (ev->time > frame) { until = std::min(until, ev->time); break; }
      handle_event(p, ev);
      ++next_event;
    }
    if (until > frame) {
      p->engine.render(p->settings, left + frame, right + frame, until - frame);
      frame = until;
    }
  }
  while (next_event < count) { handle_event(p, in->get(in, next_event)); ++next_event; }
  // Scope and meters for the editor.
  uint32_t write = p->scope_write.load(std::memory_order_relaxed);
  for (uint32_t i = frames > Plugin::kScopeSize ? frames - Plugin::kScopeSize : 0; i < frames; ++i) {
    p->scope[write].store(0.5f * (left[i] + right[i]), std::memory_order_relaxed);
    write = (write + 1) & (Plugin::kScopeSize - 1);
  }
  p->scope_write.store(write, std::memory_order_release);
  p->scope_revision.fetch_add(1, std::memory_order_release);
  p->peak_l.store(p->engine.peak_left(), std::memory_order_relaxed);
  p->peak_r.store(p->engine.peak_right(), std::memory_order_relaxed);
  p->clipped.store(p->engine.clipped(), std::memory_order_relaxed);
  p->engine.reset_meters();
  p->active_voices.store(p->engine.active_voices(), std::memory_order_relaxed);
  emit_gui_events(p, process->out_events);
  // Always CONTINUE: the tail extension tells the host how long to keep calling, and the
  // editor's queued gestures still need a process or flush to reach the host.
  return CLAP_PROCESS_CONTINUE;
}

uint32_t audio_count(const clap_plugin_t*, bool input) { return input ? 0U : 1U; }
bool audio_get(const clap_plugin_t*, uint32_t index, bool input, clap_audio_port_info_t* info) {
  if (input || index != 0 || !info) return false;
  *info = {};
  info->id = 0;
  std::snprintf(info->name, sizeof(info->name), "Stereo output");
  info->flags = CLAP_AUDIO_PORT_IS_MAIN;
  info->channel_count = 2;
  info->port_type = CLAP_PORT_STEREO;
  info->in_place_pair = CLAP_INVALID_ID;
  return true;
}
const clap_plugin_audio_ports_t kAudioPorts{audio_count, audio_get};

uint32_t note_count(const clap_plugin_t*, bool input) { return input ? 1U : 0U; }
bool note_get(const clap_plugin_t*, uint32_t index, bool input, clap_note_port_info_t* info) {
  if (!input || index != 0 || !info) return false;
  *info = {};
  info->id = 0;
  info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
  info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
  std::snprintf(info->name, sizeof(info->name), "Notes");
  return true;
}
const clap_plugin_note_ports_t kNotePorts{note_count, note_get};

uint32_t params_count(const clap_plugin_t*) { return kParamCount; }
bool params_info(const clap_plugin_t*, uint32_t index, clap_param_info_t* info) {
  if (index >= kParamCount || !info) return false;
  const auto& s = kSpecs[index];
  *info = {};
  info->id = index;
  info->flags = CLAP_PARAM_IS_AUTOMATABLE | (s.stepped ? CLAP_PARAM_IS_STEPPED : CLAP_PARAM_IS_MODULATABLE);
  if (s.unit == Unit::Options || index == kPreset) info->flags |= CLAP_PARAM_IS_ENUM;
  std::snprintf(info->name, sizeof(info->name), "%s", s.name);
  std::snprintf(info->module, sizeof(info->module), "%s", s.module);
  info->min_value = s.min; info->max_value = s.max; info->default_value = s.def;
  return true;
}
bool params_value(const clap_plugin_t* plugin, clap_id id, double* value) {
  if (id >= kParamCount || !value) return false;
  *value = param_value(self(plugin), id);
  return true;
}
bool value_to_text(const clap_plugin_t*, clap_id id, double value, char* text, uint32_t capacity) {
  return format_value(id, value, text, capacity);
}
bool text_to_value(const clap_plugin_t*, clap_id id, const char* text, double* value) {
  return parse_value(id, text, value);
}
void params_flush(const clap_plugin_t* plugin, const clap_input_events_t* in, const clap_output_events_t* out) {
  auto* p = self(plugin);
  if (in) for (uint32_t i = 0; i < in->size(in); ++i) handle_event(p, in->get(in, i));
  emit_gui_events(p, out);
}
const clap_plugin_params_t kParams{params_count, params_info, params_value, value_to_text, text_to_value, params_flush};

// --- state -----------------------------------------------------------------------------------------
//
// Header, then every parameter value, then the sample slots: a count and, per loaded slot, the
// slot index, name, rate, root pitch and 16-bit frames. Editor size rides along so a project
// reopens at the size it was left at.
struct StateHeader { uint32_t magic, version, param_count, gui_width, gui_height, sample_count; };

template <typename T> bool write_all(const clap_ostream_t* stream, const T* data, uint64_t count) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(data);
  const uint64_t size = count * sizeof(T);
  for (uint64_t done = 0; done < size;) {
    const int64_t n = stream->write(stream, bytes + done, size - done);
    if (n <= 0) return false;
    done += static_cast<uint64_t>(n);
  }
  return true;
}
template <typename T> bool read_all(const clap_istream_t* stream, T* data, uint64_t count) {
  auto* bytes = reinterpret_cast<uint8_t*>(data);
  const uint64_t size = count * sizeof(T);
  for (uint64_t done = 0; done < size;) {
    const int64_t n = stream->read(stream, bytes + done, size - done);
    if (n <= 0) return false;
    done += static_cast<uint64_t>(n);
  }
  return true;
}

bool state_save(const clap_plugin_t* plugin, const clap_ostream_t* stream) {
  if (!stream || !stream->write) return false;
  auto* p = self(plugin);
  std::vector<std::pair<int, std::shared_ptr<const dsp::Sample>>> samples;
  for (int slot = 0; slot < kSampleSlots; ++slot) if (auto s = p->engine.sample(slot)) samples.emplace_back(slot, s);
  StateHeader header{kStateMagic, kStateVersion, kParamCount, 0, 0, static_cast<uint32_t>(samples.size())};
#ifdef FUZZBOI_HAS_EDITOR
  header.gui_width = p->gui_width; header.gui_height = p->gui_height;
#endif
  if (!write_all(stream, &header, 1)) return false;
  std::array<double, kParamCount> values{};
  for (size_t i = 0; i < kParamCount; ++i) values[i] = param_value(p, static_cast<clap_id>(i));
  if (!write_all(stream, values.data(), kParamCount)) return false;
  for (const auto& [slot, sample] : samples) {
    const uint32_t slot_index = static_cast<uint32_t>(slot);
    const uint32_t name_size = static_cast<uint32_t>(std::min<size_t>(sample->name.size(), 255));
    const uint32_t frames = static_cast<uint32_t>(std::min(sample->frames.size(), kMaxSampleFrames));
    const double rate = sample->rate, root = sample->root_hz;
    if (!write_all(stream, &slot_index, 1) || !write_all(stream, &name_size, 1) || !write_all(stream, sample->name.data(), name_size) ||
        !write_all(stream, &rate, 1) || !write_all(stream, &root, 1) || !write_all(stream, &frames, 1)) return false;
    std::vector<int16_t> pcm(frames);
    for (uint32_t i = 0; i < frames; ++i) pcm[i] = static_cast<int16_t>(std::lround(std::clamp(sample->frames[i], -1.0f, 1.0f) * 32767.0f));
    if (!write_all(stream, pcm.data(), frames)) return false;
  }
  return true;
}

bool state_load(const clap_plugin_t* plugin, const clap_istream_t* stream) {
  if (!stream || !stream->read) return false;
  auto* p = self(plugin);
  StateHeader header{};
  if (!read_all(stream, &header, 1)) return false;
  if (header.magic != kStateMagic || header.version == 0 || header.version > kStateVersion) return false;
  if (header.param_count == 0 || header.param_count > 4096 || header.sample_count > static_cast<uint32_t>(kSampleSlots)) return false;
  // A newer file may carry more parameters than this build knows; older files fewer. Read what
  // is there, keep the defaults for the rest.
  std::vector<double> values(header.param_count);
  if (!read_all(stream, values.data(), header.param_count)) return false;
  for (clap_id i = 0; i < kParamCount; ++i) {
    const double v = i < header.param_count ? values[i] : kSpecs[i].def;
    p->params[i].store(clamp_to_spec(i, v), std::memory_order_relaxed);
  }
  for (int slot = 0; slot < kSampleSlots; ++slot) p->engine.set_sample(slot, nullptr);
  for (uint32_t n = 0; n < header.sample_count; ++n) {
    uint32_t slot = 0, name_size = 0, frames = 0;
    double rate = 0.0, root = 0.0;
    if (!read_all(stream, &slot, 1) || !read_all(stream, &name_size, 1) || name_size > 255) return false;
    std::string name(name_size, '\0');
    if (!read_all(stream, name.data(), name_size) || !read_all(stream, &rate, 1) || !read_all(stream, &root, 1) || !read_all(stream, &frames, 1)) return false;
    if (slot >= static_cast<uint32_t>(kSampleSlots) || frames > kMaxSampleFrames || !std::isfinite(rate) || rate < 1000.0 || rate > 384000.0 ||
        !std::isfinite(root) || root <= 0.0) return false;
    std::vector<int16_t> pcm(frames);
    if (!read_all(stream, pcm.data(), frames)) return false;
    auto sample = std::make_shared<dsp::Sample>();
    sample->name = name; sample->rate = rate; sample->root_hz = root; sample->source = "project state";
    sample->frames.resize(frames);
    for (uint32_t i = 0; i < frames; ++i) sample->frames[i] = pcm[i] / 32768.0f;
    p->engine.set_sample(static_cast<int>(slot), sample);
  }
#ifdef FUZZBOI_HAS_EDITOR
  if (header.gui_width >= static_cast<uint32_t>(fuzzboi::ui::minimum_width) && header.gui_height >= static_cast<uint32_t>(fuzzboi::ui::minimum_height) &&
      header.gui_width <= 8192 && header.gui_height <= 8192) {
    p->gui_width = header.gui_width; p->gui_height = header.gui_height;
  }
#endif
  for (auto& m : p->modulation) m.store(0.0, std::memory_order_relaxed);
  snapshot_settings(p);
  p->ui_revision.fetch_add(1, std::memory_order_release);
  p->params_rescan_pending.store(true, std::memory_order_release);
  if (p->initialized && p->host && p->host->request_callback) p->host->request_callback(p->host);
  return true;
}
const clap_plugin_state_t kState{state_save, state_load};

uint32_t latency_get(const clap_plugin_t*) { return 0; }
const clap_plugin_latency_t kLatency{latency_get};

uint32_t tail_get(const clap_plugin_t* plugin) {
  auto* p = self(plugin);
  return p->engine.tail_samples(p->settings);
}
const clap_plugin_tail_t kTail{tail_get};

bool voice_info_get(const clap_plugin_t*, clap_voice_info_t* info) {
  if (!info) return false;
  info->voice_count = kVoices;
  info->voice_capacity = kVoices;
  info->flags = CLAP_VOICE_INFO_SUPPORTS_OVERLAPPING_NOTES;
  return true;
}
const clap_plugin_voice_info_t kVoiceInfo{voice_info_get};

#ifdef FUZZBOI_HAS_EDITOR
#include "ui/frontend.hpp"
#include "ui/backends.hpp"
#endif

const void* get_extension(const clap_plugin_t*, const char* id) {
  if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &kAudioPorts;
  if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS)) return &kNotePorts;
  if (!std::strcmp(id, CLAP_EXT_PARAMS)) return &kParams;
  if (!std::strcmp(id, CLAP_EXT_STATE)) return &kState;
  if (!std::strcmp(id, CLAP_EXT_VOICE_INFO)) return &kVoiceInfo;
  if (!std::strcmp(id, CLAP_EXT_LATENCY)) return &kLatency;
  if (!std::strcmp(id, CLAP_EXT_TAIL)) return &kTail;
#ifdef FUZZBOI_HAS_EDITOR
  if (!std::strcmp(id, CLAP_EXT_GUI)) return &kGui;
#if defined(__linux__)
  if (!std::strcmp(id, CLAP_EXT_TIMER_SUPPORT)) return &kTimerSupport;
  if (!std::strcmp(id, CLAP_EXT_POSIX_FD_SUPPORT)) return &kPosixFdSupport;
#endif
#endif
  return nullptr;
}

void on_main_thread(const clap_plugin_t* plugin) {
  auto* p = self(plugin);
  if (!p->initialized || !p->params_rescan_pending.exchange(false, std::memory_order_acq_rel) || !p->host || !p->host->get_extension) return;
  if (const auto* hp = static_cast<const clap_host_params_t*>(p->host->get_extension(p->host, CLAP_EXT_PARAMS)))
    if (hp->rescan) hp->rescan(p->host, CLAP_PARAM_RESCAN_VALUES | CLAP_PARAM_RESCAN_TEXT);
}

const char* kFeatures[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER, CLAP_PLUGIN_FEATURE_SAMPLER,
                           CLAP_PLUGIN_FEATURE_STEREO, nullptr};
const clap_plugin_descriptor_t kDescriptor{
    CLAP_VERSION_INIT, "org.fuzzboi.fuzzboi", "FUZZBOI", "FUZZBOI contributors",
    "", "", "", FUZZBOI_VERSION, "Psychedelic sound machine: combo organ, tape keys, toys, FM, fuzz and tape", kFeatures};

const clap_plugin_t* create_plugin(const clap_host_t* host) {
  auto* p = new (std::nothrow) Plugin;
  if (!p) return nullptr;
  p->host = host;
  g_preset_names = presets::names();
  for (clap_id i = 0; i < kParamCount; ++i) { p->params[i].store(kSpecs[i].def); p->modulation[i].store(0.0); }
  p->api = {&kDescriptor, p, plugin_init, plugin_destroy, plugin_activate, plugin_deactivate,
            plugin_start, plugin_stop, plugin_reset, plugin_process, get_extension, on_main_thread};
  return &p->api;
}

uint32_t factory_count(const clap_plugin_factory_t*) { return 1; }
const clap_plugin_descriptor_t* factory_descriptor(const clap_plugin_factory_t*, uint32_t index) { return index == 0 ? &kDescriptor : nullptr; }
const clap_plugin_t* factory_create(const clap_plugin_factory_t*, const clap_host_t* host, const char* id) {
  if (!host || !id || !clap_version_is_compatible(host->clap_version) || std::strcmp(id, kDescriptor.id)) return nullptr;
  return create_plugin(host);
}
const clap_plugin_factory_t kFactory{factory_count, factory_descriptor, factory_create};

bool entry_init(const char*) {
#if defined(FUZZBOI_HAS_EDITOR) && defined(__linux__)
  XInitThreads();
#endif
  g_preset_names = presets::names();
  return true;
}
// The names live in this module: a host (or a test linking the engine itself, where the loader
// shares the inline variable with the plug-in, as macOS does) must not keep pointing at them
// once the module is unloaded.
void entry_deinit() { g_preset_names = nullptr; }
const void* entry_factory(const char* id) { return id && !std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? &kFactory : nullptr; }

}  // namespace

#if defined(FUZZBOI_HAS_EDITOR) && defined(__APPLE__)
@implementation FUZZBOI_EDITOR_VIEW
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent*)event { (void)event; return YES; }
- (void)drawRect:(NSRect)dirty { (void)dirty; editor_cocoa_draw(self.plugin); }
- (void)onRefresh:(NSTimer*)timer { (void)timer; editor_cocoa_refresh(self.plugin); }
- (void)handleEvent:(NSEvent*)event action:(fuzzboi::ui::PointerAction)action button:(int)button {
  const NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  auto* p = static_cast<Plugin*>(self.plugin);
  const int x = fuzzboi::ui::unscale_x(static_cast<int>(point.x), static_cast<int>(p->gui_width));
  const int y = fuzzboi::ui::unscale_y(static_cast<int>(point.y), static_cast<int>(p->gui_height));
  const NSEventModifierFlags flags = [event modifierFlags];
  const unsigned modifiers = ((flags & NSEventModifierFlagShift) ? fuzzboi::ui::kShift : 0U) |
                             ((flags & NSEventModifierFlagCommand) ? fuzzboi::ui::kControl : 0U) |
                             ((flags & NSEventModifierFlagOption) ? fuzzboi::ui::kAlt : 0U);
  editor_cocoa_input(self.plugin, action, button, x, y, modifiers);
}
- (void)updateTrackingAreas {
  for (NSTrackingArea* area in [[self.trackingAreas copy] autorelease]) [self removeTrackingArea:area];
  NSTrackingArea* area = [[NSTrackingArea alloc]
      initWithRect:NSZeroRect
           options:NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways | NSTrackingInVisibleRect
             owner:self
          userInfo:nil];
  [self addTrackingArea:area];
  [area release];
  [super updateTrackingAreas];
}
- (void)mouseDown:(NSEvent*)event { [self handleEvent:event action:fuzzboi::ui::PointerAction::Down button:1]; }
- (void)mouseDragged:(NSEvent*)event { [self handleEvent:event action:fuzzboi::ui::PointerAction::Move button:1]; }
- (void)mouseUp:(NSEvent*)event { [self handleEvent:event action:fuzzboi::ui::PointerAction::Up button:1]; }
- (void)rightMouseDown:(NSEvent*)event { [self handleEvent:event action:fuzzboi::ui::PointerAction::Down button:3]; }
- (void)rightMouseUp:(NSEvent*)event { [self handleEvent:event action:fuzzboi::ui::PointerAction::Up button:3]; }
- (void)rightMouseDragged:(NSEvent*)event { [self handleEvent:event action:fuzzboi::ui::PointerAction::Move button:3]; }
- (void)otherMouseDown:(NSEvent*)event { [self handleEvent:event action:fuzzboi::ui::PointerAction::Down button:2]; }
- (void)otherMouseUp:(NSEvent*)event { [self handleEvent:event action:fuzzboi::ui::PointerAction::Up button:2]; }
- (void)scrollWheel:(NSEvent*)event {
  if ([event deltaY] == 0) return;
  [self handleEvent:event action:fuzzboi::ui::PointerAction::Down button:([event deltaY] > 0 ? 4 : 5)];
}
- (void)mouseMoved:(NSEvent*)event { [self handleEvent:event action:fuzzboi::ui::PointerAction::Move button:0]; }
- (void)mouseExited:(NSEvent*)event { (void)event; editor_cocoa_input(self.plugin, fuzzboi::ui::PointerAction::Leave, 0, 0, 0, 0); }
@end
#endif

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry{CLAP_VERSION_INIT, entry_init, entry_deinit, entry_factory};
