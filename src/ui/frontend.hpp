// The plug-in's side of the editor: an EditorHost that reads and writes Plugin parameters, the
// help text and relevance rules, and the per-page artwork (the liquid-light scope, the tape
// slots, the fuzz curve, the echo heads, the tape reels). Included inside the plug-in's
// anonymous namespace so it can see Plugin. Layout, widgets and input live in editor.hpp.
#pragma once

#include <array>
#include <cmath>
#include <vector>

#include "canvas.hpp"
#include "editor.hpp"
#include "layout.hpp"
#include "pages.hpp"

// Whether a control does anything for the current source and settings. Irrelevant controls
// stay where they are but are dimmed, so the layout never jumps when the source changes.
bool gui_param_relevant(const Plugin* p, clap_id id) {
  const auto value = [p](clap_id param) { return p->params[param].load(std::memory_order_relaxed); };
  const int source = static_cast<int>(value(kSource));
  if (id >= kDraw16 && id <= kOrganRepeatRate) {
    if (id == kOrganRepeatRate) return source == kSourceOrgan && value(kOrganRepeat) >= 0.5;
    if (id == kOrganPercDecay) return source == kSourceOrgan && value(kOrganPercussion) > 0.0;
    return source == kSourceOrgan;
  }
  if (id >= kOsc1Wave && id <= kFilterDrive) {
    if (id == kPulseWidth || id == kPwmDepth) return source == kSourceSynth && (static_cast<int>(value(kOsc1Wave)) == 1 || static_cast<int>(value(kOsc2Wave)) == 1);
    return source == kSourceSynth;
  }
  if (id >= kFmPatch && id <= kFmDetune) return source == kSourceFm;
  if (id >= kTapeBank && id <= kVarispeed) return source == kSourceTape;
  if (id == kSampleRoot) return source == kSourceCollage || (source == kSourceTape && static_cast<int>(value(kTapeBank)) == 3);
  if (id >= kSampleLoop && id <= kOptigan) return source == kSourceCollage;
  if (id >= kToyHammer && id <= kToyInharmonic) return source == kSourceToyPiano || source == kSourceMusicBox;
  if (id >= kJugResonance && id <= kJugGrowl) return source == kSourceJug;
  if (id >= kThereminGlide && id <= kThereminUnison) return source == kSourceTheremin;
  if (id == kSawBow) return source == kSourceSaw;
  if (id == kPiccoloBreath || id == kPiccoloChaos) return source == kSourcePiccolo;
  if (id >= kJangleOctave && id <= kJangleDecay) return source == kSourceJangle;
  switch (id) {
    case kGlideMs: return static_cast<int>(value(kVoiceMode)) != 0 || source == kSourceTheremin || source == kSourceSaw;
    case kUnisonDetune: case kStereoSpread: return value(kUnison) >= 2.0 && static_cast<int>(value(kVoiceMode)) == 0;
    case kVibratoDelay: return value(kVibratoDepth) > 0.0;
    case kFuzzTone: case kFuzzLevel: case kFuzzMode: return value(kFuzz) > 0.0;
    case kPhaserRate: case kPhaserDepth: case kPhaserFeedback: case kPhaserStages: return value(kPhaserMix) > 0.0;
    case kRotarySpeed: return value(kRotaryMix) > 0.0;
    case kTremoloRate: case kTremoloShape: return value(kTremoloDepth) > 0.0;
    case kEchoTime: return value(kEchoMix) > 0.0 && value(kEchoSync) < 0.5;
    case kEchoDivision: return value(kEchoMix) > 0.0 && value(kEchoSync) >= 0.5;
    case kEchoFeedback: case kEchoHeads: case kEchoWow: case kEchoSaturate: case kEchoAge: case kEchoSync: return value(kEchoMix) > 0.0;
    case kSpringTension: case kSpringDrip: return value(kSpringMix) > 0.0;
    case kVerbSize: case kVerbDamp: case kVerbPredelay: case kVerbShimmer: return value(kVerbMix) > 0.0 || value(kWall) > 0.0;
    case kWallChamber: case kWallSquash: return value(kWall) > 0.0;
    case kBackwardsChunk: return value(kBackwardsMix) > 0.0;
    case kFeedbackHarmonic: case kFeedbackSwell: return value(kFeedbackAmount) > 0.0;
    case kChordVoicing: case kChordStrum: return value(kChordType) > 0.0;
    case kChorusMode: case kChorusRate: case kChorusDepth: return value(kChorusMix) > 0.0;
    case kPumpDivision: return value(kPump) > 0.0;
    case kCabType: return value(kCabinet) > 0.0;
    default: return true;
  }
}

const char* gui_help(clap_id id) {
  switch (id) {
    case kSource: return "Chooses what makes the sound: organ, synth, FM chip, tape keys, a toy, or the collage sampler.";
    case kMasterDb: return "Overall output level after every effect.";
    case kVoiceMode: return "Poly plays chords; Mono retriggers one voice; Legato slides between held notes without retriggering.";
    case kGlideMs: return "Time to slide from the previous pitch to the new one.";
    case kTranspose: return "Shifts every note by whole semitones.";
    case kFineCents: return "Shifts every note by cents; -50 is a true quarter tone for 24-TET lines.";
    case kBendRange: return "How many semitones a full pitch wheel bends.";
    case kAttackMs: return "How quickly a note reaches full level.";
    case kDecayMs: return "How quickly it falls to the sustain level.";
    case kSustain: return "The level a held note settles at.";
    case kReleaseMs: return "How long a note fades after the key is released (time to fall 60 dB).";
    case kVelocitySens: return "How much key velocity changes the level: 0 ignores it, 100% is full range.";
    case kVibratoRate: return "Speed of the pitch vibrato.";
    case kVibratoDepth: return "Depth of the vibrato in cents; the mod wheel adds more on top.";
    case kVibratoDelay: return "Holds the vibrato back for this long after each note starts.";
    case kUnison: return "Plays each note with this many detuned voices stacked.";
    case kUnisonDetune: return "How far apart the unison voices are tuned.";
    case kStereoSpread: return "Pans the unison voices across the stereo field.";
    case kDraw16: case kDraw8: case kDraw4: case kDraw223: case kDraw2: case kDraw1: return "Level of this footage: 16' is an octave below, 2 2/3' a twelfth above, 1' three octaves up.";
    case kOrganReed: return "Blends each footage from the pure flute tab toward the buzzy reed tab.";
    case kOrganClick: return "The burst of the key contacts closing on every note.";
    case kOrganPercussion: return "A decaying strike on the 4' footage at the start of each note.";
    case kOrganPercDecay: return "How long the percussion takes to die away.";
    case kOrganVibrato: return "Depth of the organ's own vibrato scanner.";
    case kOrganRepeat: return "Lowrey marimba repeat: gates the note on and off at the repeat rate, the Baba O'Riley trick.";
    case kOrganRepeatRate: return "How many times a second the repeat re-strikes the note.";
    case kOsc1Wave: case kOsc2Wave: return "Waveform of this oscillator: saw, pulse, triangle or sine.";
    case kOsc2Semi: return "Interval of oscillator 2 from the note, in semitones.";
    case kOsc2Detune: return "Fine detune of oscillator 2 for thickness.";
    case kOscMix: return "Balance between oscillator 1 and 2.";
    case kPulseWidth: return "Width of the pulse wave; 50% is square.";
    case kPwmDepth: return "How much a slow LFO wobbles the pulse width.";
    case kSub: return "A square wave one octave below, for weight.";
    case kNoise: return "White noise mixed in before the filter.";
    case kCutoff: return "Cutoff of the four-pole ladder filter.";
    case kResonance: return "Emphasis at the cutoff; the ladder sings on its own near the top.";
    case kFilterEnv: return "How far the filter envelope sweeps the cutoff, in either direction.";
    case kFilterAttack: case kFilterDecay: case kFilterSustain: case kFilterRelease: return "Shapes the filter envelope.";
    case kKeyTrack: return "How much the cutoff follows the played note.";
    case kFilterDrive: return "Pushes the signal harder into the filter's saturation.";
    case kFmPatch: return "Which four-operator program the YM2151 chip plays.";
    case kFmBrightness: return "How hard the modulators drive the carriers: higher is brighter.";
    case kFmAttack: case kFmDecay: case kFmSustain: case kFmRelease: return "The chip's carrier envelope, in its own rate units.";
    case kFmFeedback: return "Self-modulation of the first operator, from smooth to buzzy.";
    case kFmLfoRate: return "Speed of the chip's hardware LFO.";
    case kFmLfoDepth: return "How much the chip LFO wobbles pitch and level.";
    case kFmDetune: return "Operator detune register: 3 is none, lower sharpens, higher flattens.";
    case kTapeBank: return "Which tape plays under the keys: flute, strings, choir, or the WAV in slot 1.";
    case kTapeLength: return "Seconds of tape under each key before it runs out; 8 like the original, 9 is endless.";
    case kTapeWow: return "Slow speed wander of the tape transport.";
    case kTapeFlutter: return "Fast speed jitter of the tape transport.";
    case kTapeHiss: return "Tape noise floor.";
    case kTapeAge: return "Worn heads and old tape: rolls the top end off.";
    case kVarispeed: return "Runs the tape faster or slower, in semitones.";
    case kSampleRoot: return "The key that plays slot 1; following keys play the following slots.";
    case kSampleLoop: return "Whether a sample repeats or plays once.";
    case kCollageSlice: return "How long each slice plays before the collage rolls the dice again.";
    case kCollageChance: return "Probability that a new slice jumps to a random place in the sample.";
    case kCollageReverse: return "Probability that a slice plays backwards.";
    case kCollageScatter: return "Starts each note at a random point, up to this far into the sample.";
    case kOptigan: return "Optical-disc character: narrow bandwidth, grain, lamp hum and disc wow.";
    case kToyHammer: return "Hard or soft hammer: more energy in the upper partials and the clank.";
    case kToyDamp: return "How quickly the tines stop ringing.";
    case kToyInharmonic: return "From harmonic partials toward the true clamped-bar ratios of a toy piano.";
    case kJugResonance: return "The jug's Helmholtz resonance, in Hz.";
    case kJugFlutter: return "How fast the tongued pulses come.";
    case kJugGrowl: return "Microphone overdrive and sub-harmonic buzz.";
    case kThereminGlide: return "How long the hand takes to reach the new pitch.";
    case kThereminShape: return "From a pure sine to the asymmetric output stage of a real theremin.";
    case kThereminUnison: return "Stacks this many detuned theremins.";
    case kPiccoloBreath: return "Breath noise through the plastic.";
    case kPiccoloChaos: return "How badly out of tune each note lands and wanders.";
    case kSawBow: return "Bow scrape at the start of each note.";
    case kJangleOctave: return "Level of the octave string in each course below the B.";
    case kJangleBright: return "Pick brightness.";
    case kJangleDecay: return "How long the strings ring.";
    case kFuzz: return "Amount of fuzz; 0 is bypass.";
    case kFuzzTone: return "The pedal's tone control: dark to bright.";
    case kFuzzLevel: return "Output level of the fuzz.";
    case kFuzzMode: return "Muff (Big Muff), Face (Fuzz Face), Bender (Tone Bender), Plexi (a cranked Marshall head, the brown sound) or Octave (Octavia: the octave above on top).";
    case kPhaserMix: return "Amount of phaser.";
    case kPhaserRate: return "Sweep speed.";
    case kPhaserDepth: return "How far the notches sweep.";
    case kPhaserFeedback: return "Resonance of the notches (the Small Stone's colour switch).";
    case kPhaserStages: return "Number of allpass stages: more stages, more notches.";
    case kRotaryMix: return "Amount of rotary speaker.";
    case kRotarySpeed: return "Horn speed; the bass rotor follows a little slower, as on a Leslie.";
    case kTremoloDepth: return "Depth of the level tremolo.";
    case kTremoloRate: return "Tremolo speed.";
    case kTremoloShape: return "Smooth sine, choppy square, or the dipping bias wobble of an old amp.";
    case kEchoMix: return "Amount of tape echo.";
    case kEchoTime: return "Delay of the first head.";
    case kEchoFeedback: return "How much of each repeat is fed back; above 100% the loop runs away.";
    case kEchoHeads: return "Which playback heads are heard: heads 2 and 3 sit at 1.5x and 2x the time.";
    case kEchoWow: return "Wow and flutter of the echo's tape.";
    case kEchoSaturate: return "Tape and preamp saturation in the repeats.";
    case kEchoAge: return "Worn tape: each repeat loses more top end.";
    case kEchoSync: return "Locks the echo time to the host tempo.";
    case kEchoDivision: return "The note value of the synced echo.";
    case kSpringMix: return "Amount of spring reverb.";
    case kSpringTension: return "Spring length: shorter springs boing faster.";
    case kSpringDrip: return "How dispersive the spring is: more drip, more chirp.";
    case kVerbMix: return "Amount of the chamber reverb.";
    case kVerbSize: return "Size of the chamber.";
    case kVerbDamp: return "How quickly the chamber's highs die.";
    case kVerbPredelay: return "Gap before the reverb starts.";
    case kVerbShimmer: return "Feeds the chamber's tail back an octave up, so it climbs into a glassy halo: the shimmer reverb.";
    case kWall: return "Wall of sound: doubling, chamber send and squash, all at once.";
    case kWallChamber: return "How much of the wall goes to the echo chamber.";
    case kWallSquash: return "How hard the compressor squashes the wall.";
    case kMachineDrive: return "Saturation of the master tape.";
    case kMachineWow: return "Slow speed wander of the master machine.";
    case kMachineFlutter: return "Fast speed jitter of the master machine.";
    case kMachineHiss: return "Tape hiss on the master.";
    case kMachineBandwidth: return "Top-end limit of the master tape path.";
    case kGenerations: return "How many cassette-to-cassette bounces the mix has been through.";
    case kLofiBits: return "Bit depth of the lo-fi reduction.";
    case kLofiRate: return "Sample rate of the lo-fi reduction, with the aliasing left in.";
    case kBackwardsMix: return "Amount of backwards playback.";
    case kBackwardsChunk: return "Length of each reversed chunk.";
    case kFeedbackAmount: return "Amp feedback: a harmonic of the held note howls up over the swell time.";
    case kFeedbackHarmonic: return "Which harmonic of the note the feedback locks onto.";
    case kFeedbackSwell: return "How long the howl takes to build.";
    case kPreset: return "Loads a complete starting recipe; later edits remain fully automatable.";
    case kChordType: return "One-finger chords: the played key becomes the root of this chord, as on an Orchid or an Omnichord.";
    case kChordVoicing: return "How the chord is spread: close, drop-2, open with a bass note, or wide with the top an octave up.";
    case kChordStrum: return "Time between the chord's notes, low to high, like a strummed guitar; 0 plays them together.";
    case kChorusMix: return "The Juno bucket-brigade chorus: how much of the shimmering copy is added to the dry sound.";
    case kChorusMode: return "Juno I is slow and shallow, II faster and deeper, I+II both at once, Ensemble the three-phase string-machine chorus, Flanger the short jet-plane comb.";
    case kChorusRate: return "Speed of the chorus sweep.";
    case kChorusDepth: return "Width of the chorus sweep: more is a deeper pitch shimmer.";
    case kPump: return "Sidechain-style ducking on the host's beat: the whole mix, echoes included, breathes in time.";
    case kPumpDivision: return "Which note value the pump ducks on.";
    case kJanglePick: return "Where the pick strikes: near the bridge is thin and bright, toward the middle is hollow with the even harmonics notched.";
    case kJangleBody: return "0 is an electric pickup's resonance (the Rickenbacker chime), 100% a wooden acoustic body.";
    case kCabinet: return "A guitar speaker in a box: low thump, presence, and a top end that stops a few kHz up. Also good on organs and synths.";
    case kCabType: return "Tweed is a warm combo, British a scooped 4x12, Top boost the brightest chime.";
    default: return "Adjusts this part of the current sound; changes are immediately audible and automatable.";
  }
}

void gui_request_flush(Plugin* p) {
  if (p->host && p->host->get_extension)
    if (const auto* hp = static_cast<const clap_host_params_t*>(p->host->get_extension(p->host, CLAP_EXT_PARAMS)))
      if (hp->request_flush) hp->request_flush(p->host);
}

bool gui_choose_sample(Plugin* p, int slot);

void gui_mark_state(Plugin* p) {
  if (p->host && p->host->get_extension)
    if (const auto* hs = static_cast<const clap_host_state_t*>(p->host->get_extension(p->host, CLAP_EXT_STATE)))
      if (hs->mark_dirty) hs->mark_dirty(p->host);
}

class PluginEditorHost final : public fuzzboi::ui::EditorHost {
 public:
  explicit PluginEditorHost(Plugin* p) : p_(p) {}

  int param_count() const override { return static_cast<int>(kParamCount); }
  fuzzboi::ui::ParamInfo info(int id) const override {
    const auto& s = kSpecs[static_cast<size_t>(id)];
    return {s.name, s.min, s.max, s.def, s.stepped};
  }
  double value(int id) const override { return p_->params[static_cast<size_t>(id)].load(std::memory_order_relaxed); }
  void begin_edit(int id) override { gui_push(p_, GuiEvent::Begin, static_cast<clap_id>(id), 0); gui_request_flush(p_); }
  void edit(int id, double v) override {
    if (static_cast<clap_id>(id) == kPreset) {
      apply_preset(p_, static_cast<int>(std::lround(v)));
    } else {
      set_param(p_, static_cast<clap_id>(id), v);
    }
    snapshot_settings(p_);
    gui_push(p_, GuiEvent::Value, static_cast<clap_id>(id), value(id));
    gui_request_flush(p_);
  }
  void end_edit(int id) override { gui_push(p_, GuiEvent::End, static_cast<clap_id>(id), 0); gui_request_flush(p_); gui_mark_state(p_); }
  void format(int id, double v, char* out, size_t capacity) const override {
    format_value(static_cast<uint32_t>(id), v, out, static_cast<uint32_t>(capacity));
  }
  const char* help(int id) const override { return gui_help(static_cast<clap_id>(id)); }
  const char* short_label(int id) const override {
    if (const char* name = fuzzboi::ui::fuzzboi_short_name(id)) return name;
    return label(id);
  }
  bool relevant(int id) const override { return gui_param_relevant(p_, static_cast<clap_id>(id)); }

  int size_percent() const override {
    return static_cast<int>(std::lround(100.0 * fuzzboi::ui::uniform_scale(static_cast<int>(p_->gui_width), static_cast<int>(p_->gui_height))));
  }
  void request_size(int percent) override {
    p_->gui_request_width = static_cast<uint32_t>(fuzzboi::ui::width * percent / 100);
    p_->gui_request_height = static_cast<uint32_t>(fuzzboi::ui::height * percent / 100);
  }

  const char* strip_help(int page) const override {
    if (page == 4) return "Sample slots: left-click loads a WAV (a trailing note name such as _A3 sets its pitch)  •  right-click clears  •  an empty slot plays built-in found sound";
    if (page == 0) return "The output scope, lit by the signal: held still on each cycle and amplified to fill the frame (gain shown).";
    if (page == 9) return "MilkDrop-style visualizer driven by the output  \xe2\x80\xa2  click the picture for the next preset, right-click for the previous  \xe2\x80\xa2  AUTO moves on every 20 s";
    return nullptr;
  }

  void draw_meters(fuzzboi::ui::Painter& g, const fuzzboi::ui::Rect& r) override {
    using namespace fuzzboi::ui::theme;
    const float peak_l = p_->peak_l.load(std::memory_order_relaxed), peak_r = p_->peak_r.load(std::memory_order_relaxed);
    const bool clipped = p_->clipped.load(std::memory_order_relaxed);
    g.text(r.x, r.y + 24, "OUT", muted, 50, fuzzboi::ui::TextSize::Small);
    const int bar_x = r.x + 52, bar_w = 230;
    g.rect(bar_x, r.y + 6, bar_w, 9, border);
    g.rect(bar_x, r.y + 20, bar_w, 9, border);
    const uint32_t level = clipped ? red : green;
    g.rect(bar_x, r.y + 6, static_cast<int>(bar_w * std::clamp(peak_l, 0.0f, 1.0f)), 9, level);
    g.rect(bar_x, r.y + 20, static_cast<int>(bar_w * std::clamp(peak_r, 0.0f, 1.0f)), 9, level);
    char voices[32]{};
    std::snprintf(voices, sizeof(voices), clipped ? "CLIP" : "%d VOICES", p_->active_voices.load(std::memory_order_relaxed));
    g.text(bar_x + bar_w + 18, r.y + 24, voices, clipped ? red : muted, r.right() - bar_x - bar_w - 18, fuzzboi::ui::TextSize::Small);
  }

  void draw_strip(int page, fuzzboi::ui::Painter& g, const fuzzboi::ui::Rect& r) override {
    fuzzboi::ui::LiveState live;
    const uint32_t write = p_->scope_write.load(std::memory_order_acquire);
    float energy = 0.0f;
    for (size_t i = 0; i < live.scope.size(); ++i) {
      live.scope[i] = p_->scope[(write + static_cast<uint32_t>(i)) & (Plugin::kScopeSize - 1)].load(std::memory_order_relaxed);
      energy += live.scope[i] * live.scope[i];
    }
    live.energy = std::sqrt(energy / static_cast<float>(live.scope.size()));
    live.sample_rate = p_->sample_rate;
    if (page == kVisualizerPage) {
      // The visualizer moves by the time since it last drew, so it runs at the same speed
      // whatever the redraw rate.
      const double now = fuzzboi::ui::now_ms();
      const double dt = visual_last_ms_ < 0.0 ? 1.0 / 30.0 : (now - visual_last_ms_) / 1000.0;
      visual_last_ms_ = now;
      visualizer_.step(live.scope.data(), live.scope.size(), live.sample_rate, dt);
      visualizer_.draw(g, r);
      return;
    }
    live.phase = static_cast<double>(p_->scope_revision.load(std::memory_order_relaxed) % 100000) * 0.07;
    for (int i = 0; i < kSampleSlots; ++i) {
      const auto sample = p_->engine.sample(i);
      live.slot_loaded[static_cast<size_t>(i)] = sample != nullptr;
      if (sample && live.first_slot < 0) {
        live.first_slot = i; live.first_slot_name = sample->name; live.first_slot_root_hz = sample->root_hz;
        live.first_slot_seconds = static_cast<double>(sample->frames.size()) / sample->rate;
      }
    }
    fuzzboi::ui::Artwork(*this, live).draw(page, g, r);
  }

  bool strip_pointer(int page, int button, int x, int y, const fuzzboi::ui::Rect& r) override {
    if (page == kVisualizerPage) return visualizer_.pointer(button, x, y, r);
    if (page != 4) return false;
    for (int i = 0; i < kSampleSlots; ++i) {
      if (!fuzzboi::ui::strip::slot_tile(r, i).contains(x, y)) continue;
      if (button == 1) { if (gui_choose_sample(p_, i)) gui_mark_state(p_); return true; }
      if (button == 3) { p_->engine.set_sample(i, nullptr); p_->ui_revision.fetch_add(1, std::memory_order_release); gui_mark_state(p_); return true; }
    }
    return false;
  }

  fuzzboi::ui::Visualizer& visualizer() { return visualizer_; }

 private:
  static constexpr int kVisualizerPage = 9;
  Plugin* p_;
  fuzzboi::ui::Visualizer visualizer_;
  double visual_last_ms_ = -1.0;
};
static_assert(Plugin::kScopeSize == fuzzboi::ui::kScopeSamples, "the scope ring and the artwork agree on its length");

fuzzboi::ui::Editor& gui_editor(Plugin* p) {
  if (!p->editor) {
    p->editor_host = std::make_unique<PluginEditorHost>(p);
    p->editor = std::make_unique<fuzzboi::ui::Editor>(*p->editor_host, fuzzboi::ui::fuzzboi_pages(), fuzzboi::ui::fuzzboi_header_control());
  }
  return *p->editor;
}

void gui_draw(Plugin* p, fuzzboi::ui::Canvas& canvas) { gui_editor(p).draw(canvas); }

using GuiPointer = fuzzboi::ui::PointerAction;

bool gui_set_size(const clap_plugin_t* plugin, uint32_t w, uint32_t h);

// Asks the host to resize its editor frame; only once it agrees does the editor window follow.
void gui_apply_requested_size(Plugin* p) {
  const uint32_t w = p->gui_request_width, h = p->gui_request_height;
  p->gui_request_width = p->gui_request_height = 0;
  if (!w || !h || (w == p->gui_width && h == p->gui_height) || !p->host || !p->host->get_extension) return;
  const auto* host_gui = static_cast<const clap_host_gui_t*>(p->host->get_extension(p->host, CLAP_EXT_GUI));
  if (!host_gui || !host_gui->request_resize || !host_gui->request_resize(p->host, w, h)) return;
  gui_set_size(&p->api, w, h);
  gui_mark_state(p);
}

void gui_input(Plugin* p, GuiPointer action, int button, int x, int y, unsigned modifiers = 0) {
  gui_editor(p).pointer(action, button, x, y, modifiers, fuzzboi::ui::now_ms());
  gui_apply_requested_size(p);
}

// True when the editor needs a repaint for its own animations.
bool gui_tick(Plugin* p) { return gui_editor(p).tick(fuzzboi::ui::now_ms()); }

[[maybe_unused]] int gui_size_percent_for_test(Plugin* p) { gui_editor(p); return p->editor_host->size_percent(); }
