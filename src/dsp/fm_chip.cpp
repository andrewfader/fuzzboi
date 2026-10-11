#include "fm_chip.hpp"

#include <ymfm_opm.h>

#include <algorithm>
#include <cmath>

namespace fuzzboi::dsp {

namespace {

// One operator of a patch. Rates are chip units; `mul` is the frequency multiple, `dt2` the
// coarse detune that makes bells and marimbas inharmonic, `level` the base total level (0 is
// loudest), `carrier` whether the user's envelope applies here.
struct OperatorPatch { int mul, dt2, level, attack, decay, sustain_rate, sustain, release; bool carrier; };
struct Patch { int algorithm, feedback; std::array<OperatorPatch, 4> op; };  // op order M1, M2, C1, C2

// Algorithm 4 is two independent pairs (M1->M2, C1->C2): the workhorse for keys and mallets.
// Algorithm 7 is four parallel carriers (additive). Algorithm 0 is the full serial chain.
constexpr std::array<Patch, 8> kPatches{{
    // Electric piano: a tine pair (fast, bright, ratio 14) over a body pair (ratio 1).
    {4, 0, {{{14, 0, 42, 31, 18, 6, 2, 8, false}, {1, 0, 8, 31, 12, 4, 3, 7, true},
             {1, 0, 30, 31, 14, 3, 2, 7, false}, {1, 0, 2, 31, 10, 2, 2, 7, true}}}},
    // Bell: ratios 1:3.5 and 1:1 through DT2, long ring.
    {4, 0, {{{7, 1, 36, 31, 10, 2, 2, 6, false}, {2, 0, 4, 31, 8, 1, 1, 5, true},
             {3, 2, 38, 31, 9, 2, 2, 6, false}, {1, 0, 4, 31, 8, 1, 1, 5, true}}}},
    // Synth bass: serial chain, fast decay, feedback growl.
    {0, 5, {{{1, 0, 28, 31, 16, 8, 4, 8, false}, {1, 0, 40, 31, 14, 8, 4, 8, false},
             {1, 0, 32, 31, 14, 6, 3, 8, false}, {1, 0, 0, 31, 12, 4, 2, 8, true}}}},
    // Brass: two unison pairs, slow attack on the modulators for the blare.
    {4, 3, {{{1, 0, 30, 20, 8, 2, 1, 8, false}, {1, 0, 4, 22, 8, 1, 1, 8, true},
             {1, 0, 34, 18, 8, 2, 1, 8, false}, {2, 0, 10, 22, 8, 1, 1, 8, true}}}},
    // Clav: a snappy ratio-3 pair plus a ratio-1 pair, fast decay to a low sustain.
    {4, 2, {{{3, 0, 26, 31, 18, 10, 4, 9, false}, {1, 0, 2, 31, 16, 8, 6, 9, true},
             {1, 0, 38, 31, 18, 10, 4, 9, false}, {2, 0, 6, 31, 16, 8, 6, 9, true}}}},
    // FM organ: four parallel carriers at 1, 2, 3, 4 (additive drawbars).
    {7, 0, {{{1, 0, 6, 31, 0, 0, 0, 10, true}, {2, 0, 10, 31, 0, 0, 0, 10, true},
             {3, 0, 20, 31, 0, 0, 0, 10, true}, {4, 0, 16, 31, 0, 0, 0, 10, true}}}},
    // Marimba: ratio 4 and ratio 10 modulators, quick decay, a wooden thunk.
    {4, 0, {{{4, 0, 34, 31, 22, 10, 6, 10, false}, {1, 0, 2, 31, 14, 6, 8, 10, true},
             {10, 0, 48, 31, 26, 12, 8, 12, false}, {1, 0, 6, 31, 18, 8, 8, 10, true}}}},
    // Glass pad: ratios 1:2 and 1:3 with slow attacks and plenty of LFO.
    {5, 1, {{{2, 0, 36, 14, 6, 1, 1, 6, false}, {1, 0, 6, 12, 6, 0, 0, 6, true},
             {3, 1, 44, 12, 6, 1, 1, 6, true}, {1, 0, 8, 12, 6, 0, 0, 6, true}}}},
}};

// Brightness offsets every modulator's total level: 0.6 is the patch as written, 1 is 12 dB
// more modulation (16 steps of 0.75 dB), 0 is 24 dB less.
int modulator_offset(double brightness) {
  return static_cast<int>(std::lround((0.6 - std::clamp(brightness, 0.0, 1.0)) * 40.0));
}

constexpr uint32_t kClock = 3579545;
constexpr double kFullScale = 10850.0;  // one channel at full swing, as ymfm scales it
constexpr uint8_t kSlotOffsets[4] = {0x00, 0x08, 0x10, 0x18};  // M1, M2, C1, C2
// OPM key-code zero denotes C-sharp; MIDI pitch classes start at C.
constexpr uint8_t kNoteCodes[12] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14};

}  // namespace

struct FmChip::Impl : ymfm::ymfm_interface {
  std::unique_ptr<ymfm::ym2151> chip;
  double host_rate = 48000.0, chip_rate = 55930.0, accumulator = 0.0;
  float last = 0.0f, previous = 0.0f;
  std::array<bool, kChannels> keyed{};
  std::array<int, kChannels> last_kc{}, last_kf{};
  FmControls controls{};

  void write(uint16_t reg, uint8_t value) {
    chip->write(0, static_cast<uint8_t>(reg));
    chip->write(1, value);
  }

  void program(int channel, const FmControls& c) {
    const Patch& p = kPatches[static_cast<size_t>(std::clamp(c.patch, 0, 7))];
    const int offset = modulator_offset(c.brightness);
    for (int op = 0; op < 4; ++op) {
      const OperatorPatch& o = p.op[static_cast<size_t>(op)];
      const uint8_t slot = static_cast<uint8_t>(kSlotOffsets[op] + channel);
      const int level = o.carrier ? o.level : std::clamp(o.level + offset, 0, 127);
      const int attack = o.carrier ? c.attack : std::min(31, o.attack + (c.attack - 31) / 2);
      const int decay = o.carrier ? c.decay : std::clamp(o.decay + (c.decay - 12) / 2, 0, 31);
      const int sustain = o.carrier ? c.sustain : o.sustain;
      const int release = o.carrier ? c.release : std::clamp(o.release + (c.release - 7) / 2, 0, 15);
      write(static_cast<uint16_t>(0x40 + slot), static_cast<uint8_t>((std::clamp(c.detune, 0, 7) << 4) | o.mul));
      write(static_cast<uint16_t>(0x60 + slot), static_cast<uint8_t>(level));
      write(static_cast<uint16_t>(0x80 + slot), static_cast<uint8_t>((1 << 6) | std::clamp(attack, 0, 31)));
      write(static_cast<uint16_t>(0xa0 + slot), static_cast<uint8_t>((c.lfo_depth ? 0x80 : 0) | std::clamp(decay, 0, 31)));
      write(static_cast<uint16_t>(0xc0 + slot), static_cast<uint8_t>((o.dt2 << 6) | std::clamp(o.sustain_rate, 0, 31)));
      write(static_cast<uint16_t>(0xe0 + slot), static_cast<uint8_t>((std::clamp(sustain, 0, 15) << 4) | std::clamp(release, 0, 15)));
    }
    const int fb = std::clamp(c.feedback + p.feedback, 0, 7);
    write(static_cast<uint16_t>(0x20 + channel), static_cast<uint8_t>(0xc0 | (fb << 3) | p.algorithm));
    // Vibrato (PM) and tremolo (AM) sensitivity per channel; the depth itself is global.
    const int pms = c.lfo_depth ? 4 : 0, ams = c.lfo_depth ? 1 : 0;
    write(static_cast<uint16_t>(0x38 + channel), static_cast<uint8_t>((pms << 4) | ams));
  }

  void program_global(const FmControls& c) {
    write(0x1b, 0x02);  // triangle LFO
    write(0x18, static_cast<uint8_t>(std::clamp(c.lfo_rate, 0, 255)));
    write(0x19, static_cast<uint8_t>(std::clamp(c.lfo_depth / 2, 0, 127)));
    write(0x19, static_cast<uint8_t>(0x80 | std::clamp(c.lfo_depth, 0, 127)));
  }

  void pitch(int channel, double hz) {
    // KC holds octave and note; KF the fraction in 64ths of a semitone. The chip's octave 4 /
    // note A is 440 Hz at the nominal clock.
    // Indexing from one semitone below puts C at code 14 of the octave below, as the chip lays it out.
    const double midi = std::clamp(68.0 + 12.0 * std::log2(std::max(1.0, hz) / 440.0), 0.0, 127.999);
    const int whole = static_cast<int>(std::floor(midi));
    const int kc = (std::clamp(whole / 12 - 1, 0, 7) << 4) | kNoteCodes[whole % 12];
    const int kf = std::clamp(static_cast<int>((midi - whole) * 64.0), 0, 63) << 2;
    if (kc != last_kc[static_cast<size_t>(channel)]) { write(static_cast<uint16_t>(0x28 + channel), static_cast<uint8_t>(kc)); last_kc[static_cast<size_t>(channel)] = kc; }
    if (kf != last_kf[static_cast<size_t>(channel)]) { write(static_cast<uint16_t>(0x30 + channel), static_cast<uint8_t>(kf)); last_kf[static_cast<size_t>(channel)] = kf; }
  }
};

FmChip::FmChip() : impl_(std::make_unique<Impl>()) {}
FmChip::~FmChip() = default;

void FmChip::prepare(double host_rate) {
  impl_->host_rate = host_rate;
  if (!impl_->chip) impl_->chip = std::make_unique<ymfm::ym2151>(*impl_);
  impl_->chip_rate = impl_->chip->sample_rate(kClock);
  reset();
}

void FmChip::reset() {
  if (!impl_->chip) return;
  impl_->chip->reset();
  impl_->keyed.fill(false);
  impl_->last_kc.fill(-1);
  impl_->last_kf.fill(-1);
  impl_->accumulator = 0.0;
  impl_->last = impl_->previous = 0.0f;
  impl_->program_global(impl_->controls);
  for (int ch = 0; ch < kChannels; ++ch) impl_->program(ch, impl_->controls);
}

void FmChip::note_on(int channel, double hz, const FmControls& controls) {
  if (!impl_->chip || channel < 0 || channel >= kChannels) return;
  impl_->controls = controls;
  impl_->write(0x08, static_cast<uint8_t>(channel));  // key off first: a retrigger restarts the envelopes
  impl_->program(channel, controls);
  impl_->program_global(controls);
  impl_->pitch(channel, hz);
  impl_->write(0x08, static_cast<uint8_t>(0x78 | channel));
  impl_->keyed[static_cast<size_t>(channel)] = true;
}

void FmChip::note_off(int channel) {
  if (!impl_->chip || channel < 0 || channel >= kChannels) return;
  impl_->write(0x08, static_cast<uint8_t>(channel));
  impl_->keyed[static_cast<size_t>(channel)] = false;
}

void FmChip::set_pitch(int channel, double hz) {
  if (!impl_->chip || channel < 0 || channel >= kChannels) return;
  impl_->pitch(channel, hz);
}

void FmChip::update(const FmControls& controls) {
  if (!impl_->chip) return;
  impl_->controls = controls;
  impl_->program_global(controls);
  for (int ch = 0; ch < kChannels; ++ch) impl_->program(ch, controls);
}

bool FmChip::any_keyed() const {
  for (bool k : impl_->keyed) if (k) return true;
  return false;
}

float FmChip::render() {
  if (!impl_->chip) return 0.0f;
  // The chip runs at its own rate; step it as many times as the host sample spans and
  // interpolate between the last two chip samples.
  impl_->accumulator += impl_->chip_rate / impl_->host_rate;
  while (impl_->accumulator >= 1.0) {
    impl_->accumulator -= 1.0;
    ymfm::ym2151::output_data out{};
    impl_->chip->generate(&out);
    impl_->previous = impl_->last;
    impl_->last = static_cast<float>((out.data[0] + out.data[1]) / (2.0 * kFullScale));
  }
  const float t = static_cast<float>(impl_->accumulator);
  return impl_->previous + (impl_->last - impl_->previous) * t;
}

}  // namespace fuzzboi::dsp
