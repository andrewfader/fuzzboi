// The instrument: sixteen voices, each able to be any of the sound sources, a sample bank,
// one FM chip shared by the FM voices, the chord machine that turns one key into a voiced
// (and optionally strummed) chord, and the effects chain from the pedalboard to the master
// tape. The engine knows nothing about CLAP: it takes a snapshot of parameter values per block
// and note events with sample offsets, and it never allocates while rendering.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

#include "dsp/analog_synth.hpp"
#include "dsp/core.hpp"
#include "dsp/envelopes.hpp"
#include "dsp/fm_chip.hpp"
#include "dsp/fx/echoes.hpp"
#include "dsp/fx/master.hpp"
#include "dsp/fx/pedals.hpp"
#include "dsp/guitar.hpp"
#include "dsp/organ.hpp"
#include "dsp/sampler.hpp"
#include "dsp/strings.hpp"
#include "dsp/tape.hpp"
#include "dsp/tape_keys.hpp"
#include "dsp/toys.hpp"
#include "params.hpp"

namespace fuzzboi {

// A block's worth of parameter values, copied out of the plug-in's atomics once per process
// call so every voice and effect in the block sees the same settings.
struct Settings {
  std::array<double, params::kParamCount> v{};
  double operator[](params::ParamId id) const { return v[static_cast<size_t>(id)]; }
  int source() const { return static_cast<int>(std::lround(v[params::kSource])); }
  bool on(params::ParamId id) const { return v[static_cast<size_t>(id)] >= 0.5; }
  static Settings defaults() {
    Settings s;
    for (size_t i = 0; i < params::kParamCount; ++i) s.v[i] = params::kSpecs[i].def;
    return s;
  }
};

constexpr int kVoices = 16;
constexpr int kSampleSlots = 16;

// Level trims that put every source on the same footing: one C4 at full velocity with the
// default settings lands at about -13 dBFS RMS while held, or -5 dBFS peak for the struck and
// plucked sources, whichever comes first. Measured with the levels probe described in
// docs/QUALITY.md and held there by engine_tests, so a preset's Master knob is a musical choice
// rather than a correction for a quiet model. FM patches and tape banks have their own rows
// because each is a different instrument behind one source.
namespace levels {
constexpr std::array<double, params::kSourceCount> kSourceDb{
    0.5,   // combo organ
    1.0,   // analog synth
    0.0,   // FM electric (see kFmPatchDb)
    0.0,   // tape keys (see kTapeBankDb)
    -1.5,  // toy piano
    -4.0,  // music box
    4.5,   // electric jug
    -4.0,  // theremin
    -2.5,  // singing saw
    -1.5,  // plastic piccolo
    -4.5,  // Stylophone
    -1.0,  // jangle 12-string
    3.0,   // collage sampler: the found sounds hold the harvested bank's -23 dBFS RMS, so a single
           // note sits low; the full +7.6 dB --levels asks for would put a sample normalised to
           // -3 dBFS peak into the limiter, so this lifts it only as far as the per-note window
    0.0,   // physical guitar (see kGuitarModelDb)
};
constexpr std::array<double, params::kFmPatchCount> kFmPatchDb{3.5, 5.0, 2.0, 8.5, 6.5, 1.0, 6.5, 1.0};
constexpr std::array<double, params::kTapeBankCount> kTapeBankDb{-1.0, 5.5, 5.0, 3.0};  // user tape: as the collage sampler
// The guitar's output is a radiated pressure or a pickup voltage in its own units; these bring
// each instrument to the same footing as the other sources.
constexpr std::array<double, params::kGuitarModelCount> kGuitarModelDb{0.0, 10.8, 0.0, 0.0};
inline double gain(const std::array<double, params::kSourceCount>& table, int index) {
  return dsp::db_to_gain(table[static_cast<size_t>(std::clamp(index, 0, params::kSourceCount - 1))]);
}
}  // namespace levels

// --- one voice ---------------------------------------------------------------------------------

class Voice {
 public:
  void prepare(double rate) {
    rate_ = rate;
    amp_.prepare(rate);
    glide_.set_time(1.0, rate);
    vibrato_.prepare(rate);
    organ_.prepare(rate); synth_.prepare(rate); tape_key_.prepare(rate); tape_tone_.prepare(rate);
    tape_path_.prepare(rate); tape_speed_.prepare(rate); modal_.prepare(rate); jug_.prepare(rate);
    theremin_.prepare(rate); saw_.prepare(rate); piccolo_.prepare(rate); stylophone_.prepare(rate);
    jangle_.prepare(rate); sampler_.prepare(rate);
    note_ = -1;
    active_ = false;
  }

  bool active() const { return active_; }
  bool held() const { return held_; }
  bool releasing() const { return active_ && !held_; }
  int note() const { return note_; }
  // The key that was played: with the chord machine on, several voices sounding different
  // notes share one trigger, and that is what a note-off matches.
  int trigger() const { return trigger_; }
  int16_t channel() const { return channel_; }
  int32_t note_id() const { return note_id_; }
  uint64_t age() const { return age_; }
  int source() const { return source_; }
  int fm_channel() const { return fm_channel_; }
  // The guitar string this voice is fretting (-1: none, or it was taken by a later note).
  int guitar_string() const { return guitar_string_; }
  void drop_string() { guitar_string_ = -1; }
  // The pitch the voice wants as a multiple of the note its string is fretted at: glide, bend,
  // vibrato, fine tune and unison detune, which the guitar applies to the string.
  double pitch_ratio() const { return pitch_ratio_; }
  // Released while the sustain pedal holds it.
  bool sustained() const { return sustained_; }
  double pan() const { return pan_; }

  struct Start {
    int16_t channel; int key; int32_t note_id; double velocity; uint64_t age;
    double detune_cents; double pan; int fm_channel; bool legato;
    std::shared_ptr<const dsp::Sample> sample; uint32_t seed;
    int trigger = -1;     // the played key (defaults to `key`)
    double gain = 1.0;    // unison voices share the level of one
    int guitar_string = -1, guitar_note = -1;  // where the guitar fretted the note
  };

  void start(const Settings& s, const Start& in) {
    const bool legato = in.legato && active_;
    channel_ = in.channel; note_ = in.key; note_id_ = in.note_id; velocity_ = in.velocity; age_ = in.age;
    detune_cents_ = in.detune_cents; pan_ = in.pan; fm_channel_ = in.fm_channel;
    trigger_ = in.trigger >= 0 ? in.trigger : in.key; gain_ = in.gain;
    guitar_string_ = in.guitar_string; guitar_note_ = in.guitar_note; pitch_ratio_ = 1.0;
    held_ = true; active_ = true; sustained_ = false; samples_ = 0;
    source_ = s.source();
    const double hz = dsp::midi_to_hz(in.key);
    if (!legato) glide_.snap(hz);
    glide_.target(hz);
    amp_.set(s[params::kAttackMs], s[params::kDecayMs], s[params::kSustain], s[params::kReleaseMs]);
    if (!legato) {
      amp_.note_on();
      vibrato_.reset();
      sample_ = in.sample;
      switch (source_) {
        case params::kSourceOrgan: organ_.note_on(organ_settings(s)); break;
        case params::kSourceSynth: synth_.note_on(velocity_); break;
        case params::kSourceTape: tape_key_.start(); tape_tone_.note_on();
          if (static_cast<int>(s[params::kTapeBank]) == 3 && sample_) sampler_.note_on(sample_, sampler_settings(s), in.seed);
          break;
        case params::kSourceToyPiano: case params::kSourceMusicBox: modal_.note_on(modal_settings(s), hz, velocity_); break;
        case params::kSourceJug: jug_.note_on(); break;
        case params::kSourceTheremin: theremin_.note_on(hz, false); break;
        case params::kSourceSaw: saw_.note_on(hz, false); break;
        case params::kSourcePiccolo: piccolo_.note_on(piccolo_settings(s)); break;
        case params::kSourceJangle: jangle_.note_on(jangle_settings(s), hz, in.key, velocity_); break;
        case params::kSourceCollage: sampler_.note_on(sample_, sampler_settings(s), in.seed); break;
        default: break;
      }
    } else {
      // Legato: the pitch moves, the envelopes and tape keep going.
      if (source_ == params::kSourceTheremin) theremin_.note_on(hz, true);
      if (source_ == params::kSourceSaw) saw_.note_on(hz, true);
    }
  }

  void release(bool sustain_pedal_down) {
    held_ = false;
    if (sustain_pedal_down) { sustained_ = true; return; }
    amp_.note_off();
    synth_.note_off();
  }
  // The pedal came up: a voice that was held by it now releases.
  void pedal_up() { if (sustained_ && !held_) { sustained_ = false; amp_.note_off(); synth_.note_off(); } }
  void kill() { active_ = false; held_ = false; sustained_ = false; amp_.kill(); note_ = -1; trigger_ = -1; fm_channel_ = -1; guitar_string_ = -1; }
  void retune(double hz) { glide_.target(hz); }

  // Renders one mono sample. `bend` is in semitones, `wheel` 0..1 adds vibrato.
  float render(const Settings& s, double bend, double wheel, double tempo_bpm) {
    (void)tempo_bpm;
    if (!active_) return 0.0f;
    glide_.set_time(std::max(0.0, s[params::kGlideMs]) * 0.5, rate_);
    const double env = amp_.next();
    if (!amp_.active()) { kill(); return 0.0f; }
    ++samples_;
    // Pitch: glide target, transpose, fine tune, bend, unison detune, delayed vibrato (plus wheel).
    const double vib_delay_samples = s[params::kVibratoDelay] * 0.001 * rate_;
    const double vib_ramp = vib_delay_samples > 0.0 ? std::clamp((static_cast<double>(samples_) - vib_delay_samples) / std::max(1.0, rate_ * 0.3), 0.0, 1.0) : 1.0;
    const double vib_cents = (s[params::kVibratoDepth] * vib_ramp + wheel * 60.0) * vibrato_.next(s[params::kVibratoRate]);
    const double hz = glide_.next() * std::exp2((s[params::kTranspose] + bend) / 12.0) *
                      dsp::cents_ratio(s[params::kFineCents] + detune_cents_ + vib_cents);
    const double note = dsp::hz_to_midi(hz);
    float out = 0.0f;
    switch (source_) {
      case params::kSourceOrgan: out = organ_.next(organ_settings(s), hz); break;
      case params::kSourceSynth: out = synth_.next(synth_settings(s), hz, note); break;
      case params::kSourceFm: out = 0.0f; break;  // rendered by the chip, mixed by the engine
      case params::kSourceGuitar:  // the strings are the engine's; the voice steers its string's pitch
        pitch_ratio_ = hz / dsp::midi_to_hz(note_ + s[params::kTranspose]);
        out = 0.0f;
        break;
      case params::kSourceTape: out = render_tape(s, hz); break;
      case params::kSourceToyPiano: case params::kSourceMusicBox: out = modal_.next(modal_settings(s)); break;
      case params::kSourceJug: out = jug_.next(jug_settings(s), hz); break;
      case params::kSourceTheremin: out = theremin_.next(theremin_settings(s), hz); break;
      case params::kSourceSaw: out = saw_.next(saw_settings(s), hz); break;
      case params::kSourcePiccolo: out = piccolo_.next(piccolo_settings(s), hz); break;
      case params::kSourceStylophone: out = stylophone_.next(hz, 0.0); break;
      case params::kSourceJangle: out = jangle_.next(jangle_settings(s), hz); break;
      case params::kSourceCollage: {
        out = sampler_.next(sampler_settings(s), sample_ ? sample_->root_hz : hz, true);
        break;
      }
      default: break;
    }
    const double velocity_gain = 1.0 - s[params::kVelocitySens] * (1.0 - velocity_);
    return out * static_cast<float>(env * velocity_gain * gain_ * levels::gain(levels::kSourceDb, source_));
  }

  // Settings adapters: the parameter table to each model's own struct.
  static dsp::OrganSettings organ_settings(const Settings& s) {
    dsp::OrganSettings o;
    o.drawbars = {s[params::kDraw16], s[params::kDraw8], s[params::kDraw4], s[params::kDraw223], s[params::kDraw2], s[params::kDraw1]};
    o.reed = s[params::kOrganReed]; o.click = s[params::kOrganClick]; o.percussion = s[params::kOrganPercussion];
    o.percussion_ms = s[params::kOrganPercDecay]; o.vibrato = s[params::kOrganVibrato];
    o.repeat = s.on(params::kOrganRepeat); o.repeat_hz = s[params::kOrganRepeatRate];
    return o;
  }
  static dsp::SynthSettings synth_settings(const Settings& s) {
    dsp::SynthSettings o;
    o.osc1 = static_cast<dsp::Wave>(static_cast<int>(s[params::kOsc1Wave]));
    o.osc2 = static_cast<dsp::Wave>(static_cast<int>(s[params::kOsc2Wave]));
    o.osc2_semitones = static_cast<int>(s[params::kOsc2Semi]); o.osc2_cents = s[params::kOsc2Detune];
    o.mix = s[params::kOscMix]; o.pulse_width = s[params::kPulseWidth]; o.pwm_depth = s[params::kPwmDepth];
    o.sub = s[params::kSub]; o.noise = s[params::kNoise]; o.cutoff_hz = s[params::kCutoff]; o.resonance = s[params::kResonance];
    o.filter_env = s[params::kFilterEnv]; o.filter_attack_ms = s[params::kFilterAttack]; o.filter_decay_ms = s[params::kFilterDecay];
    o.filter_sustain = s[params::kFilterSustain]; o.filter_release_ms = s[params::kFilterRelease];
    o.key_track = s[params::kKeyTrack]; o.drive = s[params::kFilterDrive];
    return o;
  }
  static dsp::ModalSettings modal_settings(const Settings& s) {
    return {s[params::kToyHammer], s[params::kToyDamp], s[params::kToyInharmonic], s.source() == params::kSourceMusicBox};
  }
  static dsp::JugSettings jug_settings(const Settings& s) { return {s[params::kJugResonance], s[params::kJugFlutter], s[params::kJugGrowl]}; }
  static dsp::ThereminSettings theremin_settings(const Settings& s) {
    return {s[params::kThereminGlide], s[params::kThereminShape], static_cast<int>(s[params::kThereminUnison])};
  }
  static dsp::SawSettings saw_settings(const Settings& s) { return {s[params::kSawBow]}; }
  static dsp::PiccoloSettings piccolo_settings(const Settings& s) { return {s[params::kPiccoloBreath], s[params::kPiccoloChaos]}; }
  static dsp::JangleSettings jangle_settings(const Settings& s) {
    return {s[params::kJangleOctave], s[params::kJangleBright], s[params::kJangleDecay], s[params::kJanglePick], s[params::kJangleBody]};
  }
  static dsp::SamplerSettings sampler_settings(const Settings& s) {
    dsp::SamplerSettings o;
    o.varispeed_semitones = s[params::kVarispeed]; o.loop = s.on(params::kSampleLoop);
    o.slice_ms = s[params::kCollageSlice]; o.slice_chance = s[params::kCollageChance];
    o.reverse_chance = s[params::kCollageReverse]; o.scatter = s[params::kCollageScatter]; o.optigan = s[params::kOptigan];
    return o;
  }

 private:
  float render_tape(const Settings& s, double hz) {
    const double speed = tape_speed_.next(s[params::kTapeWow], s[params::kTapeFlutter]) * std::exp2(s[params::kVarispeed] / 12.0);
    const double gain = tape_key_.advance(speed, s[params::kTapeLength]);
    if (gain <= 0.0) return 0.0f;
    const int bank = static_cast<int>(s[params::kTapeBank]);
    float tone;
    if (bank == 3 && sample_) {
      dsp::SamplerSettings ss = sampler_settings(s);
      ss.varispeed_semitones = 0.0;
      ss.loop = true;
      tone = sampler_.next(ss, hz * (speed / std::exp2(s[params::kVarispeed] / 12.0)), false);
    } else {
      tone = tape_tone_.next(static_cast<dsp::TapeBank>(std::clamp(bank, 0, 2)), hz * speed);
    }
    tape_path_.set(s[params::kTapeAge], 18000.0);
    const double trim = dsp::db_to_gain(levels::kTapeBankDb[static_cast<size_t>(std::clamp(bank, 0, params::kTapeBankCount - 1))]);
    return tape_path_.process(tone, 0.15, s[params::kTapeHiss]) * static_cast<float>(gain * trim);
  }

  double rate_ = 48000.0;
  int16_t channel_ = 0;
  int note_ = -1, trigger_ = -1;
  int32_t note_id_ = -1;
  double velocity_ = 1.0, detune_cents_ = 0.0, pan_ = 0.0, gain_ = 1.0;
  uint64_t age_ = 0, samples_ = 0;
  int source_ = 0, fm_channel_ = -1, guitar_string_ = -1, guitar_note_ = -1;
  double pitch_ratio_ = 1.0;
  bool active_ = false, held_ = false, sustained_ = false;
  dsp::Adsr amp_;
  dsp::Smoother glide_;
  dsp::Lfo vibrato_;
  dsp::OrganVoice organ_;
  dsp::SynthVoice synth_;
  dsp::TapeKey tape_key_;
  dsp::TapeToneGenerator tape_tone_;
  dsp::TapePath tape_path_;
  dsp::WowFlutter tape_speed_;
  dsp::ModalVoice modal_;
  dsp::JugVoice jug_;
  dsp::ThereminVoice theremin_;
  dsp::SingingSawVoice saw_;
  dsp::PiccoloVoice piccolo_;
  dsp::StylophoneVoice stylophone_;
  dsp::JangleVoice jangle_;
  dsp::SamplerVoice sampler_;
  std::shared_ptr<const dsp::Sample> sample_;
};

// --- the engine ----------------------------------------------------------------------------------

// Samples cross from the main thread to the audio thread through these. libc++ (Apple) has no
// std::atomic<std::shared_ptr>, so it gets the equivalent free functions instead.
#if defined(__cpp_lib_atomic_shared_ptr)
template <class T>
using AtomicSharedPtr = std::atomic<std::shared_ptr<T>>;
#else
template <class T>
class AtomicSharedPtr {
 public:
  void store(std::shared_ptr<T> p) {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
    std::atomic_store(&ptr_, std::move(p));
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  }
  std::shared_ptr<T> load() const {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
    return std::atomic_load(&ptr_);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  }
 private:
  std::shared_ptr<T> ptr_;
};
#endif

class Engine {
 public:
  void prepare(double rate) {
    rate_ = rate;
    for (auto& v : voices_) v.prepare(rate);
    fm_.prepare(rate);
    for (auto& f : fuzz_) f.prepare(rate);
    for (auto& c : cabinet_) c.prepare(rate);
    for (auto& p : phaser_) p.prepare(rate);
    chorus_.prepare(rate);
    for (auto& t : tremolo_) t.prepare(rate);
    rotary_.prepare(rate);
    for (auto& e : echo_) e.prepare(rate);
    for (auto& s : spring_) s.prepare(rate);
    chamber_.prepare(rate);
    shimmer_.prepare(rate);
    shimmer_back_ = 0.0f;
    for (auto& w : wall_) w.prepare(rate);
    for (auto& b : backwards_) b.prepare(rate);
    for (auto& l : lofi_) l.prepare(rate);
    for (auto& m : machine_) m.prepare(rate);
    feedback_.prepare(rate);
    guitar_.prepare(rate);
    pump_.prepare(rate);
    limiter_.prepare(rate);
    master_.set_time(10.0, rate);
    master_.snap(dsp::db_to_gain(params::kSpecs[params::kMasterDb].def));
    found_ = found_sounds(rate);
    reset();
  }
  void reset() {
    for (auto& v : voices_) v.kill();
    fm_.reset();
    fm_in_use_.fill(false);
    for (auto& e : echo_) e.clear();
    for (auto& s : spring_) s.clear();
    chamber_.clear();
    shimmer_.clear();
    shimmer_back_ = 0.0f;
    feedback_.clear();
    guitar_.reset();
    pump_.reset();
    limiter_.reset();
    bend_.fill(0.0); wheel_.fill(0.0); sustain_.fill(false);
    held_count_ = 0;
    pending_count_ = 0;
    peak_l_ = peak_r_ = 0.0f;
    clipped_ = false;
  }

  // --- samples (main thread writes, audio thread reads) ---
  void set_sample(int slot, std::shared_ptr<const dsp::Sample> sample) {
    if (slot < 0 || slot >= kSampleSlots) return;
    slots_[static_cast<size_t>(slot)].store(std::move(sample));
  }
  std::shared_ptr<const dsp::Sample> sample(int slot) const {
    if (slot < 0 || slot >= kSampleSlots) return nullptr;
    return slots_[static_cast<size_t>(slot)].load();
  }
  bool slot_loaded(int slot) const { return sample(slot) != nullptr; }

  // --- events ---
  void note_on(const Settings& s, int16_t channel, int key, int32_t note_id, double velocity) {
    ++held_count_;
    held_keys_[static_cast<size_t>(std::clamp(key, 0, 127))] = true;
    play_key(s, channel, key, note_id, velocity);
  }

  // The chord machine: the notes a played key becomes, lowest first. With the chord off that
  // is the key itself. Returns how many were written (at most kChordNotes).
  static constexpr int kChordNotes = 8;
  static int chord_notes(const Settings& s, int key, int* out) {
    static constexpr int kIntervals[params::kChordTypeCount][5] = {
        {0, -1, -1, -1, -1}, {0, 4, 7, -1, -1}, {0, 3, 7, -1, -1}, {0, 2, 7, -1, -1}, {0, 5, 7, -1, -1},
        {0, 4, 7, 10, -1}, {0, 4, 7, 11, -1}, {0, 3, 7, 10, -1}, {0, 4, 7, 14, -1}, {0, 3, 7, 10, 14},
        {0, 7, 12, -1, -1}, {0, 12, -1, -1, -1}};
    const int type = std::clamp(static_cast<int>(std::lround(s[params::kChordType])), 0, params::kChordTypeCount - 1);
    int notes[kChordNotes];
    int n = 0;
    for (int i = 0; i < 5 && kIntervals[type][i] >= 0; ++i) notes[n++] = key + kIntervals[type][i];
    if (n > 1) {
      switch (static_cast<int>(std::lround(s[params::kChordVoicing]))) {
        case 1: notes[n - 2] -= 12; break;                       // drop 2: the second-highest note down an octave
        case 2: notes[1] += 12; notes[n++] = key - 12; break;    // open: a bass note below, the second note on top
        case 3: notes[n - 1] += 12; notes[n++] = key - 12; break; // wide: bass below, the top an octave higher
        default: break;
      }
    }
    std::sort(notes, notes + n);
    int written = 0;
    for (int i = 0; i < n; ++i) if (notes[i] >= 0 && notes[i] <= 127) out[written++] = notes[i];
    return std::max(written, 1);
  }

  // Starts every note of the chord a key stands for (just the key with the chord off): at once in
  // mono and legato, strummed low to high through the pending queue in poly.
  void play_key(const Settings& s, int16_t channel, int key, int32_t note_id, double velocity) {
    const int mode = static_cast<int>(s[params::kVoiceMode]);
    const int unison = std::clamp(static_cast<int>(s[params::kUnison]), 1, 4);
    int notes[kChordNotes];
    const int n = chord_notes(s, key, notes);
    if (mode != 0) {
      // Mono and legato: one voice per chord note, retriggered (mono) or glided into (legato)
      // while held; everything else is silenced.
      const bool legato = mode == 2 && voices_[0].active() && voices_[0].held();
      for (int i = n; i < kVoices; ++i) {
        Voice& extra = voices_[static_cast<size_t>(i)];
        if (!extra.active()) continue;
        if (extra.guitar_string() >= 0) guitar_.release(extra.guitar_string(), 0.05);
        extra.kill();
      }
      for (int i = 0; i < n; ++i) {
        Voice& v = voices_[static_cast<size_t>(i)];
        start_voice(s, v, channel, notes[i], note_id, velocity, 0.0, 0.0, legato && v.active(), key, 1.0);
      }
      return;
    }
    const double strum = std::max(0.0, s[params::kChordStrum]) * 0.001 * rate_;
    for (int i = 0; i < n; ++i) {
      const uint64_t due = clock_ + static_cast<uint64_t>(strum * i);
      if (due <= clock_) start_chord_note(s, channel, notes[i], key, note_id, velocity, unison);
      else if (pending_count_ < static_cast<int>(pending_.size())) pending_[static_cast<size_t>(pending_count_++)] = {due, channel, notes[i], key, note_id, velocity};
    }
  }

  void start_chord_note(const Settings& s, int16_t channel, int note, int trigger, int32_t note_id, double velocity, int unison) {
    const double gain = 1.0 / std::sqrt(static_cast<double>(unison));
    for (int u = 0; u < unison; ++u) {
      Voice& v = allocate();
      const double spread = unison > 1 ? (static_cast<double>(u) / (unison - 1) - 0.5) * 2.0 : 0.0;
      start_voice(s, v, channel, note, note_id, velocity, spread * s[params::kUnisonDetune], spread * s[params::kStereoSpread], false, trigger, gain);
    }
  }
  void note_off(const Settings& s, int16_t channel, int key, int32_t note_id) {
    held_keys_[static_cast<size_t>(std::clamp(key, 0, 127))] = false;
    if (held_count_ > 0) --held_count_;
    const int mode = static_cast<int>(s[params::kVoiceMode]);
    // Strum notes that have not sounded yet are dropped rather than played after the key is up.
    for (int i = 0; i < pending_count_;) {
      const Pending& p = pending_[static_cast<size_t>(i)];
      if ((p.trigger == key || key < 0) && (p.channel == channel || channel < 0)) pending_[static_cast<size_t>(i)] = pending_[static_cast<size_t>(--pending_count_)];
      else ++i;
    }
    for (auto& v : voices_) {
      if (!v.active() || !v.held()) continue;
      if (v.channel() != channel && channel >= 0) continue;
      if (v.trigger() != key && key >= 0) continue;
      if (note_id >= 0 && v.note_id() >= 0 && v.note_id() != note_id) continue;
      if (mode != 0) {
        // Another key still down: fall back to it instead of releasing.
        for (int k = 127; k >= 0; --k)
          if (held_keys_[static_cast<size_t>(k)]) {
            play_key(s, v.channel(), k, -1, 1.0);
            return;
          }
      }
      const bool pedal = sustain_[static_cast<size_t>(std::clamp<int>(v.channel(), 0, 15))];
      v.release(pedal);
      if (v.source() == params::kSourceFm && v.fm_channel() >= 0 && !pedal) fm_.note_off(v.fm_channel());
      if (v.guitar_string() >= 0) {
        // The fretting hand lets go: damped over the release time, or left ringing on the pedal.
        if (pedal) guitar_.let_ring(v.guitar_string());
        else { guitar_.release(v.guitar_string(), s[params::kReleaseMs] * 0.001); v.drop_string(); }
      }
    }
  }
  void all_notes_off() {
    for (auto& v : voices_) {
      if (v.active() && v.held()) { v.release(false); if (v.fm_channel() >= 0) fm_.note_off(v.fm_channel()); }
      if (v.guitar_string() >= 0) { guitar_.release(v.guitar_string(), 0.1); v.drop_string(); }
    }
    held_keys_.fill(false);
    held_count_ = 0;
    pending_count_ = 0;
  }
  void all_sound_off() { for (auto& v : voices_) v.kill(); fm_in_use_.fill(false); fm_.reset(); guitar_.reset(); held_keys_.fill(false); held_count_ = 0; pending_count_ = 0; }
  int pending_notes() const { return pending_count_; }
  void sustain(int16_t channel, bool down) {
    const size_t ch = static_cast<size_t>(std::clamp<int>(channel, 0, 15));
    sustain_[ch] = down;
    if (!down)
      for (auto& v : voices_)
        if (v.channel() == channel) {
          const bool was_held_by_pedal = v.sustained() && !v.held();
          v.pedal_up();
          if (v.active() && !v.held() && v.fm_channel() >= 0) fm_.note_off(v.fm_channel());
          if (was_held_by_pedal && v.guitar_string() >= 0) { guitar_.release(v.guitar_string(), release_s_); v.drop_string(); }
        }
  }
  void pitch_bend(int16_t channel, double semitones_scaled) { bend_[static_cast<size_t>(std::clamp<int>(channel, 0, 15))] = semitones_scaled; }
  void mod_wheel(int16_t channel, double amount) { wheel_[static_cast<size_t>(std::clamp<int>(channel, 0, 15))] = amount; }
  void set_tempo(double bpm) { if (std::isfinite(bpm) && bpm > 0.0) tempo_ = bpm; }
  double tempo() const { return tempo_; }

  int active_voices() const { int n = 0; for (const auto& v : voices_) n += v.active(); return n; }
  bool silent() const { return active_voices() == 0 && !fm_.any_keyed() && !guitar_.awake(); }
  // The physical guitar, for inspection (tests, the critic).
  const dsp::guitar::Guitar& guitar() const { return guitar_; }
  float peak_left() const { return peak_l_; }
  float peak_right() const { return peak_r_; }
  bool clipped() const { return clipped_; }
  // Deepest limiter reduction in dB since the meters were last reset (0: the ceiling was never hit).
  double limited_db() const { return limiter_.held_db(); }
  void reset_meters() { peak_l_ = peak_r_ = 0.0f; clipped_ = false; limiter_.reset_meter(); }
  double lowest_held_hz() const {
    for (int k = 0; k < 128; ++k) if (held_keys_[static_cast<size_t>(k)]) return dsp::midi_to_hz(k);
    return 0.0;
  }

  // --- rendering ---
  void render(const Settings& s, float* left, float* right, uint32_t frames) {
    // FM controls are per block: re-programming the chip's registers per sample is pointless.
    const dsp::FmControls fm = fm_controls(s);
    if (fm_dirty(fm)) { fm_.update(fm); last_fm_ = fm; }
    master_.target(dsp::db_to_gain(s[params::kMasterDb]));
    const double bend_range = s[params::kBendRange];
    const double echo_seconds = s.on(params::kEchoSync) ? division_seconds(static_cast<int>(s[params::kEchoDivision])) : s[params::kEchoTime] * 0.001;
    const double feedback_hz = lowest_held_hz();
    const float fm_trim = static_cast<float>(0.7 * dsp::db_to_gain(levels::kFmPatchDb[static_cast<size_t>(std::clamp(fm.patch, 0, params::kFmPatchCount - 1))]));
    const double pump_period = division_seconds(static_cast<int>(s[params::kPumpDivision]));
    const int unison = std::clamp(static_cast<int>(s[params::kUnison]), 1, 4);
    const auto chorus_mode = static_cast<dsp::ChorusMode>(std::clamp(static_cast<int>(s[params::kChorusMode]), 0, 4));
    const auto cab_type = static_cast<dsp::CabType>(std::clamp(static_cast<int>(s[params::kCabType]), 0, 2));
    guitar_.configure(guitar_setup(s));
    release_s_ = s[params::kReleaseMs] * 0.001;
    const float guitar_trim = static_cast<float>(
        dsp::db_to_gain(levels::kGuitarModelDb[static_cast<size_t>(std::clamp(static_cast<int>(s[params::kGuitarModel]), 0, params::kGuitarModelCount - 1))]) *
        levels::gain(levels::kSourceDb, params::kSourceGuitar));
    for (uint32_t i = 0; i < frames; ++i) {
      // Strummed chord notes whose moment has come.
      for (int p = 0; p < pending_count_;) {
        const Pending& due = pending_[static_cast<size_t>(p)];
        if (due.due <= clock_) {
          const Pending fire = due;
          pending_[static_cast<size_t>(p)] = pending_[static_cast<size_t>(--pending_count_)];
          start_chord_note(s, fire.channel, fire.note, fire.trigger, fire.note_id, fire.velocity, unison);
        } else ++p;
      }
      ++clock_;
      float l = 0.0f, r = 0.0f;
      for (auto& v : voices_) {
        if (!v.active()) continue;
        const size_t ch = static_cast<size_t>(std::clamp<int>(v.channel(), 0, 15));
        const float mono = v.render(s, bend_[ch] * bend_range, wheel_[ch], tempo_);
        if (v.guitar_string() >= 0) guitar_.set_ratio(v.guitar_string(), v.pitch_ratio());
        // Constant-power pan from the unison spread.
        const double angle = (v.pan() * 0.5 + 0.5) * dsp::kPi * 0.5;
        l += mono * static_cast<float>(std::cos(angle));
        r += mono * static_cast<float>(std::sin(angle));
      }
      // The FM chip renders all its channels at once; voices only drive its registers.
      const float fm_out = fm_.render() * fm_trim;
      l += fm_out; r += fm_out;
      // So does the guitar: six strings and one body, whichever voices are fretting them.
      const float guitar_out = guitar_.render() * guitar_trim;
      l += guitar_out; r += guitar_out;
      // Pedals and tape: mono effects run once per side so unison width survives.
      const float mono = (l + r) * 0.5f;
      const float howl = feedback_.process(mono, s[params::kFeedbackAmount], static_cast<int>(s[params::kFeedbackHarmonic]), s[params::kFeedbackSwell], feedback_hz) - mono;
      l += howl; r += howl;
      const auto fuzz_mode = static_cast<dsp::FuzzMode>(std::clamp(static_cast<int>(s[params::kFuzzMode]), 0, 4));
      l = fuzz_[0].process(l, s[params::kFuzz], s[params::kFuzzTone], s[params::kFuzzLevel], fuzz_mode);
      r = fuzz_[1].process(r, s[params::kFuzz], s[params::kFuzzTone], s[params::kFuzzLevel], fuzz_mode);
      l = cabinet_[0].process(l, s[params::kCabinet], cab_type);
      r = cabinet_[1].process(r, s[params::kCabinet], cab_type);
      const int stages = static_cast<int>(s[params::kPhaserStages]);
      l = phaser_[0].process(l, s[params::kPhaserMix], s[params::kPhaserRate], s[params::kPhaserDepth], s[params::kPhaserFeedback], stages);
      r = phaser_[1].process(r, s[params::kPhaserMix], s[params::kPhaserRate], s[params::kPhaserDepth], s[params::kPhaserFeedback], stages);
      const auto chorused = chorus_.process(l, r, s[params::kChorusMix], s[params::kChorusRate], s[params::kChorusDepth], chorus_mode);
      l = chorused.left; r = chorused.right;
      const auto trem_shape = static_cast<dsp::TremoloShape>(static_cast<int>(s[params::kTremoloShape]));
      l = tremolo_[0].process(l, s[params::kTremoloDepth], s[params::kTremoloRate], trem_shape);
      r = tremolo_[1].process(r, s[params::kTremoloDepth], s[params::kTremoloRate], trem_shape);
      if (s[params::kRotaryMix] > 0.0) {
        const auto spun = rotary_.process((l + r) * 0.5f, s[params::kRotaryMix], s[params::kRotarySpeed]);
        l = spun.left; r = spun.right;
      }
      const int heads = static_cast<int>(s[params::kEchoHeads]);
      l = echo_[0].process(l, s[params::kEchoMix], echo_seconds, s[params::kEchoFeedback], heads, s[params::kEchoWow], s[params::kEchoSaturate], s[params::kEchoAge]);
      r = echo_[1].process(r, s[params::kEchoMix], echo_seconds, s[params::kEchoFeedback], heads, s[params::kEchoWow], s[params::kEchoSaturate], s[params::kEchoAge]);
      l = spring_[0].process(l, s[params::kSpringMix], s[params::kSpringTension], s[params::kSpringDrip]);
      r = spring_[1].process(r, s[params::kSpringMix], s[params::kSpringTension], s[params::kSpringDrip]);
      float send_l = 0.0f, send_r = 0.0f;
      l = wall_[0].process(l, s[params::kWall], s[params::kWallChamber], s[params::kWallSquash], &send_l);
      r = wall_[1].process(r, s[params::kWall], s[params::kWallChamber], s[params::kWallSquash], &send_r);
      // The chamber takes the reverb send plus the wall's own send; with both at zero it idles.
      const double verb_mix = s[params::kVerbMix];
      // The shimmer's octave-up copy of the last output goes back in with the send.
      const float chamber_in = (l + r) * 0.5f * static_cast<float>(verb_mix) + (send_l + send_r) * 0.5f + shimmer_back_;
      if (verb_mix > 0.0 || s[params::kWall] > 0.0) {
        const auto wet = chamber_.process(chamber_in, 1.0, s[params::kVerbSize], s[params::kVerbDamp], s[params::kVerbPredelay]);
        const auto shimmer = shimmer_.process((wet.left + wet.right) * 0.5f, s[params::kVerbShimmer], s[params::kVerbSize]);
        shimmer_back_ = shimmer.feed;
        l = l * static_cast<float>(1.0 - verb_mix * 0.5) + wet.left + shimmer.halo;
        r = r * static_cast<float>(1.0 - verb_mix * 0.5) + wet.right + shimmer.halo;
      }
      // The pump ducks the whole mix, echoes and reverb included, on the host's beat.
      const float pump_gain = pump_.gain(s[params::kPump], pump_period);
      l *= pump_gain; r *= pump_gain;
      l = backwards_[0].process(l, s[params::kBackwardsMix], s[params::kBackwardsChunk]);
      r = backwards_[1].process(r, s[params::kBackwardsMix], s[params::kBackwardsChunk]);
      const int bits = static_cast<int>(s[params::kLofiBits]);
      l = lofi_[0].process(l, bits, s[params::kLofiRate]);
      r = lofi_[1].process(r, bits, s[params::kLofiRate]);
      const int generations = static_cast<int>(s[params::kGenerations]);
      l = machine_[0].process(l, s[params::kMachineDrive], s[params::kMachineWow], s[params::kMachineFlutter], s[params::kMachineHiss], s[params::kMachineBandwidth], generations);
      r = machine_[1].process(r, s[params::kMachineDrive], s[params::kMachineWow], s[params::kMachineFlutter], s[params::kMachineHiss], s[params::kMachineBandwidth], generations);
      const float gain = static_cast<float>(master_.next());
      l *= gain; r *= gain;
      if (!std::isfinite(l)) l = 0.0f;
      if (!std::isfinite(r)) r = 0.0f;
      // The limiter holds the ceiling; the clip light means it is holding hard enough to hear.
      const float limit = limiter_.gain(l, r);
      l *= limit; r *= limit;
      if (limiter_.held_db() > dsp::Limiter::kClipDb) clipped_ = true;
      peak_l_ = std::max(peak_l_, std::abs(l)); peak_r_ = std::max(peak_r_, std::abs(r));
      left[i] = l;
      right[i] = r;
    }
    // Free FM channels whose voices ended this block.
    for (int ch = 0; ch < dsp::FmChip::kChannels; ++ch) {
      if (!fm_in_use_[static_cast<size_t>(ch)]) continue;
      bool owned = false;
      for (const auto& v : voices_) if (v.active() && v.fm_channel() == ch) owned = true;
      if (!owned) fm_in_use_[static_cast<size_t>(ch)] = false;
    }
  }

  // How long after the last note the output can still be sounding, in samples.
  uint32_t tail_samples(const Settings& s) const {
    double seconds = s[params::kReleaseMs] * 0.001 + kChordNotes * s[params::kChordStrum] * 0.001;
    if (s.source() == params::kSourceJangle) seconds = std::max(seconds, s[params::kJangleDecay]);
    if (s.source() == params::kSourceToyPiano || s.source() == params::kSourceMusicBox) seconds = std::max(seconds, 3.0);
    if (s.source() == params::kSourceFm) seconds = std::max(seconds, 4.0);
    // Released strings are damped over the release time; the open strings ringing in sympathy
    // with them fall away with them.
    if (s.source() == params::kSourceGuitar) seconds = std::max(seconds, s[params::kReleaseMs] * 0.001 + 1.0);
    if (s[params::kEchoMix] > 0.0) {
      const double fb = std::min(0.98, s[params::kEchoFeedback]);
      const double repeats = fb > 1e-6 ? 1.0 + std::ceil(std::log(1e-4) / std::log(fb)) : 1.0;
      seconds += 2.0 * s[params::kEchoTime] * 0.001 * repeats;
      if (s[params::kEchoFeedback] >= 1.0) seconds += 30.0;  // runaway: the host should keep calling
    }
    if (s[params::kSpringMix] > 0.0) seconds += 3.0;
    if (s[params::kVerbMix] > 0.0 || s[params::kWall] > 0.0) seconds += 2.0 + 8.0 * s[params::kVerbSize] + 6.0 * s[params::kVerbShimmer];
    if (s[params::kBackwardsMix] > 0.0) seconds += 2.0 * s[params::kBackwardsChunk] * 0.001;
    if (s[params::kFeedbackAmount] > 0.0) seconds += s[params::kFeedbackSwell] * 0.001 + 1.0;
    seconds += 0.1;
    return static_cast<uint32_t>(std::min(seconds * rate_, 2.0e9));
  }

  double division_seconds(int division) const {
    static constexpr double kBeats[7] = {0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0, 2.0, 4.0};
    return 60.0 / tempo_ * kBeats[std::clamp(division, 0, 6)];
  }

  static dsp::guitar::Setup guitar_setup(const Settings& s) {
    dsp::guitar::Setup g;
    g.model = static_cast<int>(std::lround(s[params::kGuitarModel]));
    g.tuning = static_cast<int>(std::lround(s[params::kGuitarTuning]));
    g.capo = static_cast<int>(std::lround(s[params::kGuitarCapo]));
    g.pluck_m = s[params::kGuitarPluck] * 0.001;
    g.hardness = s[params::kGuitarPick];
    g.pickup = static_cast<int>(std::lround(s[params::kGuitarPickup]));
    g.mute = s[params::kGuitarMute];
    g.age = s[params::kGuitarAge];
    g.position = static_cast<int>(std::lround(s[params::kGuitarPosition]));
    return g;
  }

  static dsp::FmControls fm_controls(const Settings& s) {
    dsp::FmControls c;
    c.patch = static_cast<int>(s[params::kFmPatch]); c.brightness = s[params::kFmBrightness];
    c.attack = static_cast<int>(s[params::kFmAttack]); c.decay = static_cast<int>(s[params::kFmDecay]);
    c.sustain = static_cast<int>(s[params::kFmSustain]); c.release = static_cast<int>(s[params::kFmRelease]);
    c.feedback = static_cast<int>(s[params::kFmFeedback]); c.lfo_rate = static_cast<int>(s[params::kFmLfoRate]);
    c.lfo_depth = static_cast<int>(s[params::kFmLfoDepth]); c.detune = static_cast<int>(s[params::kFmDetune]);
    return c;
  }

 private:
  bool fm_dirty(const dsp::FmControls& c) const {
    const dsp::FmControls& l = last_fm_;
    return c.patch != l.patch || c.brightness != l.brightness || c.attack != l.attack || c.decay != l.decay ||
           c.sustain != l.sustain || c.release != l.release || c.feedback != l.feedback || c.lfo_rate != l.lfo_rate ||
           c.lfo_depth != l.lfo_depth || c.detune != l.detune;
  }

  Voice& allocate() {
    for (auto& v : voices_) if (!v.active()) return v;
    // Steal the oldest releasing voice, else the oldest voice.
    Voice* best = nullptr;
    for (auto& v : voices_) if (v.releasing() && (!best || v.age() < best->age())) best = &v;
    if (!best) for (auto& v : voices_) if (!best || v.age() < best->age()) best = &v;
    if (best->fm_channel() >= 0) { fm_.note_off(best->fm_channel()); fm_in_use_[static_cast<size_t>(best->fm_channel())] = false; }
    if (best->guitar_string() >= 0) guitar_.release(best->guitar_string(), 0.05);
    best->kill();
    return *best;
  }

  int allocate_fm_channel() {
    for (int ch = 0; ch < dsp::FmChip::kChannels; ++ch) if (!fm_in_use_[static_cast<size_t>(ch)]) { fm_in_use_[static_cast<size_t>(ch)] = true; return ch; }
    // All busy: take the channel of the oldest FM voice.
    Voice* oldest = nullptr;
    for (auto& v : voices_) if (v.active() && v.fm_channel() >= 0 && (!oldest || v.age() < oldest->age())) oldest = &v;
    if (!oldest) return 0;
    const int ch = oldest->fm_channel();
    oldest->kill();
    return ch;
  }

  void start_voice(const Settings& s, Voice& v, int16_t channel, int key, int32_t note_id, double velocity,
                   double detune_cents, double pan, bool legato, int trigger = -1, double gain = 1.0) {
    Voice::Start start{channel, key, note_id, velocity, ++age_counter_, detune_cents, pan, -1, legato, nullptr,
                       static_cast<uint32_t>(age_counter_ * 2654435761u)};
    start.trigger = trigger;
    start.gain = gain;
    const int source = s.source();
    if (source == params::kSourceCollage) {
      const int root = static_cast<int>(s[params::kSampleRoot]);
      const int slot = ((key - root) % kSampleSlots + kSampleSlots) % kSampleSlots;
      start.sample = slots_[static_cast<size_t>(slot)].load();
      if (!start.sample) start.sample = found_[static_cast<size_t>(slot)];
    } else if (source == params::kSourceTape && static_cast<int>(s[params::kTapeBank]) == 3) {
      start.sample = slots_[0].load();
      if (!start.sample) start.sample = found_[0];
    }
    if (source == params::kSourceFm) {
      if (legato && v.active() && v.fm_channel() >= 0) {
        start.fm_channel = v.fm_channel();
        fm_.set_pitch(start.fm_channel, dsp::midi_to_hz(key) * std::exp2(s[params::kTranspose] / 12.0) * dsp::cents_ratio(s[params::kFineCents] + detune_cents));
      } else {
        if (v.active() && v.fm_channel() >= 0) { fm_.note_off(v.fm_channel()); fm_in_use_[static_cast<size_t>(v.fm_channel())] = false; }
        start.fm_channel = allocate_fm_channel();
        fm_.note_on(start.fm_channel, dsp::midi_to_hz(key) * std::exp2(s[params::kTranspose] / 12.0) * dsp::cents_ratio(s[params::kFineCents] + detune_cents), fm_controls(s));
      }
    } else if (v.active() && v.fm_channel() >= 0) {
      fm_.note_off(v.fm_channel());
      fm_in_use_[static_cast<size_t>(v.fm_channel())] = false;
    }
    if (source == params::kSourceGuitar) start_guitar(s, v, start, legato);
    else if (v.active() && v.guitar_string() >= 0) guitar_.release(v.guitar_string(), 0.05);
    v.start(s, start);
  }

  // Frets and plucks the guitar for a voice. A legato note on the string the voice already holds
  // is a hammer-on or pull-off (a slide with glide), if the string reaches it; anything else is a
  // fresh pluck where the fingering puts it, which chokes whatever that string was sounding.
  void start_guitar(const Settings& s, Voice& v, Voice::Start& start, bool legato) {
    guitar_.configure(guitar_setup(s));
    const int note = start.key + static_cast<int>(std::lround(s[params::kTranspose]));
    const double strength = 1.0 - s[params::kVelocitySens] * (1.0 - start.velocity);
    if (legato && v.active() && v.guitar_string() >= 0 && guitar_.held(v.guitar_string())) {
      const int string = v.guitar_string();
      const int fret = note - guitar_.open_note(string);
      if (fret >= guitar_.setup().capo && fret <= guitar_.guitar().frets) {
        guitar_.legato(string, fret, strength, s[params::kGlideMs] > 0.0);
        start.guitar_string = string;
        start.guitar_note = note;
        return;
      }
    }
    // A mono retrigger moves the hand off the old note.
    if (v.active() && v.guitar_string() >= 0) { guitar_.release(v.guitar_string(), 0.05); v.drop_string(); }
    const auto f = guitar_.finger(note);
    if (f.string < 0) return;
    for (auto& other : voices_) if (&other != &v && other.guitar_string() == f.string) other.drop_string();
    guitar_.pluck(f.string, f.fret, strength, start.seed);
    start.guitar_string = f.string;
    start.guitar_note = f.note;
  }

  // The built-in found sounds are deterministic, so one set per sample rate serves every
  // instance in the process; prepare() runs on the main thread, where a lock is fine.
  static std::array<std::shared_ptr<const dsp::Sample>, kSampleSlots> found_sounds(double rate) {
    static std::mutex mutex;
    static std::map<double, std::array<std::shared_ptr<const dsp::Sample>, kSampleSlots>> cache;
    std::lock_guard<std::mutex> lock(mutex);
    auto it = cache.find(rate);
    if (it != cache.end()) return it->second;
    std::array<std::shared_ptr<const dsp::Sample>, kSampleSlots> set;
    for (int i = 0; i < kSampleSlots; ++i) set[static_cast<size_t>(i)] = dsp::generate_found_sound(0x1000u + static_cast<uint32_t>(i) * 7919u, rate, 3.0);
    cache[rate] = set;
    return set;
  }

  double rate_ = 48000.0, tempo_ = 120.0;
  std::array<Voice, kVoices> voices_{};
  dsp::FmChip fm_;
  dsp::FmControls last_fm_{};
  std::array<bool, dsp::FmChip::kChannels> fm_in_use_{};
  std::array<AtomicSharedPtr<const dsp::Sample>, kSampleSlots> slots_{};
  std::array<std::shared_ptr<const dsp::Sample>, kSampleSlots> found_{};
  std::array<dsp::Fuzz, 2> fuzz_{};
  std::array<dsp::Cabinet, 2> cabinet_{};
  std::array<dsp::Phaser, 2> phaser_{};
  dsp::Chorus chorus_;
  std::array<dsp::Tremolo, 2> tremolo_{};
  dsp::Rotary rotary_;
  std::array<dsp::TapeEcho, 2> echo_{};
  std::array<dsp::SpringReverb, 2> spring_{};
  dsp::Chamber chamber_;
  dsp::Shimmer shimmer_;
  float shimmer_back_ = 0.0f;
  std::array<dsp::WallOfSound, 2> wall_{};
  std::array<dsp::Backwards, 2> backwards_{};
  std::array<dsp::LoFi, 2> lofi_{};
  std::array<dsp::TapeMachine, 2> machine_{};
  dsp::FeedbackVoice feedback_;
  dsp::guitar::Guitar guitar_;
  double release_s_ = 0.12;
  dsp::Pump pump_;
  dsp::Limiter limiter_;
  dsp::Smoother master_;
  std::array<double, 16> bend_{}, wheel_{};
  std::array<bool, 16> sustain_{};
  std::array<bool, 128> held_keys_{};
  int held_count_ = 0;
  uint64_t age_counter_ = 0, clock_ = 0;
  // Chord notes waiting for their strum moment (fixed storage: no allocation while rendering).
  struct Pending { uint64_t due; int16_t channel; int note; int trigger; int32_t note_id; double velocity; };
  std::array<Pending, 64> pending_{};
  int pending_count_ = 0;
  float peak_l_ = 0.0f, peak_r_ = 0.0f;
  bool clipped_ = false;
};

}  // namespace fuzzboi
