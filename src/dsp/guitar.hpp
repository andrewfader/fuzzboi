// The guitar: six real strings on a real neck over a real body, simulated from their physics
// rather than imitated. Everything that sets the sound is a measured property of an actual
// instrument: the scale length and the fret positions on it, each string's gauge, mass per length
// (the maker's published unit weight), core stiffness and losses, the action and nut height (so
// how sharp a fretted note goes, and how far back the saddle has to sit to compensate), the point
// the string is plucked at and the width and softness of what plucks it, the body's resonances,
// and where the pickups sit and how their coils resonate.
//
// Each string is a digital waveguide (Smith; Karjalainen, Välimäki and Tolonen) carrying
// displacement waves on two rails, nut to bridge and back, in each of two transverse planes:
// vertical, which drives the top and so loses energy fast, and horizontal, which barely moves the
// bridge and rings on; they sit about two cents apart (Woodhouse 2004) and give a plucked string
// its beating, two-stage decay. At the bridge the wave is inverted, loses energy through a filter
// fitted to the string's physical loss rates (air drag, the wire's internal losses, the bridge's
// conductance) and is dispersed by an allpass chain fitted to the core's bending stiffness, so the
// upper partials sit sharp by f_n = n f0 sqrt(1 + B n^2) as on the real string. The pluck is the
// physical initial condition: the string held in a triangle peaked at the pluck point, rounded by
// the width of the finger or pick and by how quickly it slips off, then let go. A hard pluck
// stretches the string enough to raise its tension, so it starts a few cents sharp and settles
// (Kirchhoff-Carrier tension modulation, from the elongation of the simulated string). Strings
// pick up energy from the moving bridge and ring in sympathy. An acoustic radiates the bridge force
// through its body's modes; an electric's pickups sense the string's velocity over their poles.
//
// The six strings are one instrument shared by every voice, the way a guitarist has one guitar:
// a fingering chooses the string and fret for each note, a second note on a string chokes the
// first, a legato note on the same string is a hammer-on, pull-off or slide, and a released note
// is damped by the fretting hand.
//
// Sources: D'Addario String Tension Specifications (unit weights, tensions); Woodhouse, "Plucked
// guitar transients", Acta Acustica 90 (2004) (classical string losses, stiffness, polarization
// split, body Q); Zollner, Physics of the Electric Guitar (decay of a Stratocaster's strings,
// wound-string core ratios, fretting strain, pickup positions and resonances); Rossing and
// Caldersmith, The Science of String Instruments ch. 3 (body modes); Martin service specs (action).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <complex>
#include <vector>

#include "air.hpp"
#include "core.hpp"
#include "filters.hpp"

namespace fuzzboi::dsp::guitar {

constexpr int kStrings = 6;  // the courses the fretting hand plays
// A twelve-string's courses are pairs: each course's second string sits after the six main ones.
constexpr int kMaxStrings = 2 * kStrings;
constexpr double kInch = 0.0254;
constexpr double kPoundPerInch = 0.45359237 / 0.0254;  // lb/in to kg/m
constexpr double kAirDensity = 1.2;       // kg/m^3 at 20 C
constexpr double kAirViscosity = 1.8e-5;  // Pa s

// --- strings -------------------------------------------------------------------------------------

enum class Core { Steel, Nylon };

struct StringSpec {
  const char* code;       // the maker's part number
  double gauge_in;        // outer diameter
  double unit_weight;     // lb/in, the maker's published figure
  double core_ratio;      // core diameter over gauge (1 for a plain string)
  Core core;
  // Internal losses in Woodhouse's form: a frequency-independent loss factor (friction in the
  // wire and between the wraps) and one for bending, which takes over in the high partials.
  double eta_f, eta_b;
  // Air drag as a fixed decay rate (s^-1) when it was measured with the string; 0 to compute it
  // from the diameter by Stokes' law.
  double air_rate;
  double ei_measured;     // bending stiffness (N m^2) when it was measured; 0 to compute it
  bool wound;
};

inline constexpr double kSteelYoung = 2.0e11;
// Nylon's modulus rises with the stress it is under (Woodhouse and Lynch-Aird 2019): 4.5 GPa
// plus 39 times the stress.
inline double nylon_young(double stress) { return 4.5e9 + 39.0 * stress; }

// --- instruments ---------------------------------------------------------------------------------

enum class Kind { Acoustic, Electric };

// One body resonance as seen from the bridge: frequency, quality factor and peak level.
struct Mode { double hz, q, gain; };

struct Pickup {
  double from_bridge_m;   // centre of the (first) coil from the saddle
  double coil_spacing_m;  // a humbucker's second coil, further from the bridge (0: single coil)
  double aperture_m;      // length of string the magnetic window sees (0: no pickup here)
  double resonance_hz, q; // coil inductance against its own and the cable's capacitance, loaded
};

constexpr int kLowModes = 10;
constexpr int kPickups = 3;

struct GuitarSpec {
  const char* name;
  Kind kind;
  double scale_m;
  int frets;
  std::array<StringSpec, kStrings> strings;  // the low E string first
  // Action: the string's height over the 12th fret, bass and treble side; its clearance over the
  // first fret, bass and treble; the fret wire's height over the board.
  double action_bass_m, action_treble_m, nut_bass_m, nut_treble_m, fret_height_m;
  // Real part of the bridge's (or a solid body's neck's) admittance in the vertical plane, s/kg,
  // away from the body's resonances; the horizontal plane sees this times `horizontal_coupling`.
  double bridge_conductance, horizontal_coupling;
  // The plane normal to the top rings this many cents above the parallel one.
  double polarization_split_cents;
  std::array<Mode, kLowModes> modes;  // the measured low body modes; denser ones are generated above
  double dense_from_hz, dense_to_hz, dense_spacing_hz, dense_q, dense_level;
  std::array<Pickup, kPickups> pickups;  // neck, middle, bridge (electric)
  // Scale on the bridge coupling between strings. The coupling is explicit (each string hears the
  // others' push on the bridge a sample late, and nothing debits the string that pushed), so at
  // full strength strings tuned in octaves (DADGAD, open D) feed each other and ring on; at these
  // scales every tuning dies away (guitar_tests the_whole_instrument_is_stable_in_every_tuning).
  double sympathetic;
  // Rotation (radians) between the two planes at each reflection off the bridge.
  double plane_exchange;
  // Semitones the instrument's open strings sit from the tuning table's guitar notes: -12 for a
  // six-string bass, which takes every guitar tuning an octave down.
  int transpose = 0;
  // A twelve-string: each course is a pair, its second string (`course`, low E course first) an
  // octave or a unison above the main one. Players tune the pairs by ear, so each second string
  // sits a few cents off true (`course_cents`), which is the slow chorus a twelve-string has. The
  // pick meets the two strings of a course one after the other, `course_gap_s` apart at a
  // moderate stroke: on most twelve-strings the octave string lies on the bass side and is struck
  // first on a downstroke; Rickenbacker reversed the pairs so the main string speaks first.
  bool twelve = false;
  std::array<StringSpec, kStrings> course{};
  std::array<int, kStrings> course_interval{};
  std::array<double, kStrings> course_cents{};
  bool octave_first = true;
  double course_gap_s = 0.0;
};

// Open-string notes, low string first, as MIDI numbers.
struct Tuning { const char* name; std::array<int, kStrings> notes; };

constexpr int kTuningCount = 8;
inline constexpr std::array<Tuning, kTuningCount> kTunings{{
    {"Standard", {40, 45, 50, 55, 59, 64}},
    {"Drop D", {38, 45, 50, 55, 59, 64}},
    {"Half step down", {39, 44, 49, 54, 58, 63}},
    {"DADGAD", {38, 45, 50, 55, 57, 62}},
    {"Open G", {38, 43, 50, 55, 59, 62}},
    {"Open D", {38, 45, 50, 54, 57, 62}},
    {"Open E", {40, 47, 52, 56, 59, 64}},
    {"Open C", {36, 43, 48, 55, 60, 64}},
}};

constexpr int kModelCount = 14;
enum Model : int {
  kDreadnought, kClassical, kSingleCoil, kHumbucker, kBassVI,
  kElectric12, kAcoustic12, kArchtop, kAuditorium, kPlateSingleCoil, kOffset, kFibreglass,
  kSemiHollow, kThinlineP90
};

// The string sets, low E first: part number, gauge, unit weight (D'Addario's chart), core ratio
// (Zollner's ranges for wound strings), core, eta_f, eta_b, air, measured EI, wound.
//
// Phosphor-bronze lights 12-53 (EJ16). The wound strings' friction between the wraps is an
// estimate, a little above nickel's (bronze is softer).
inline constexpr std::array<StringSpec, kStrings> kPhosphorBronzeLights{{
    {"PB053", 0.053, 0.00056962, 0.38, Core::Steel, 3.0e-4, 3e-4, 0.0, 0.0, true},
    {"PB042", 0.042, 0.00036722, 0.42, Core::Steel, 3.0e-4, 3e-4, 0.0, 0.0, true},
    {"PB032", 0.032, 0.00021018, 0.50, Core::Steel, 3.0e-4, 3e-4, 0.0, 0.0, true},
    {"PB024", 0.024, 0.00011682, 0.52, Core::Steel, 3.0e-4, 3e-4, 0.0, 0.0, true},
    {"PL016", 0.016, 0.00005671, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL012", 0.012, 0.00003190, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
}};
// Nickel-wound regular lights 10-46 (EXL110). The plain strings' eta_f is Zollner's fit to steel
// wire; the wound strings lose about three times that between the wraps, which with the neck's
// conductance is what recorded Stratocaster notes show once air drag is taken off (E2's 2nd to
// 8th partials ring 10 to 23 s, the fundamental near a minute; tools/guitar_critic.py).
inline constexpr std::array<StringSpec, kStrings> kNickelRegularLights{{
    {"NW046", 0.046, 0.00038216, 0.38, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"NW036", 0.036, 0.00023964, 0.42, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"NW026", 0.026, 0.00012671, 0.50, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"PL017", 0.017, 0.00006402, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL013", 0.013, 0.00003744, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL010", 0.010, 0.00002215, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
}};
// Normal-tension nylon (EJ45): clear nylon trebles, silver-plated copper on nylon floss basses.
// Internal losses (the two trebles' friction lowered from his 4e-4 to 2.5e-4, which recorded
// trebles' upper partials ask for once Stokes drag is added) and the basses' bending stiffness
// are Woodhouse's fits to a classical set; air
// drag is Stokes' (his fitted air term, taken as a decay rate, held the bass fundamentals to about
// 11 s, where recorded classical basses ring 10 to 38 s; tools/guitar_critic.py).
inline constexpr std::array<StringSpec, kStrings> kNylonNormal{{
    {"J4506", 0.043, 0.00030632, 0.55, Core::Nylon, 2.0e-5, 2.0e-2, 0.0, 57e-6, true},
    {"J4505", 0.035, 0.00018416, 0.55, Core::Nylon, 7.0e-5, 2.5e-2, 0.0, 40e-6, true},
    {"J4504", 0.029, 0.00010754, 0.55, Core::Nylon, 5.0e-5, 2.0e-2, 0.0, 51e-6, true},
    {"J4503", 0.0403, 0.00004679, 1.00, Core::Nylon, 1.4e-4, 2.0e-2, 0.0, 0.0, false},
    {"J4502", 0.0322, 0.00002827, 1.00, Core::Nylon, 2.5e-4, 2.0e-2, 0.0, 0.0, false},
    {"J4501", 0.0280, 0.00002092, 1.00, Core::Nylon, 2.5e-4, 2.4e-2, 0.0, 0.0, false},
}};

// A six-string bass set, 24-95, as Fender strung the Bass VI: the nickel-wound XL NW024 to NW072
// with an XLB095 bass string for the low E (unit weights from D'Addario's chart; core ratios follow
// the 10-46 set's, the heavier strings at the low end of Zollner's range). At the 30-inch scale an
// octave below a guitar every string pulls 26 to 32 lb.
inline constexpr std::array<StringSpec, kStrings> kNickelBassVI{{
    {"XLB095", 0.095, 0.00169349, 0.34, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"NW072", 0.072, 0.00094124, 0.36, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"NW056", 0.056, 0.00057598, 0.38, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"NW044", 0.044, 0.00035182, 0.38, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"NW034", 0.034, 0.00021590, 0.42, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"NW024", 0.024, 0.00010857, 0.50, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
}};

// Unit weights the chart does not list are its neighbour's scaled by the gauge squared (mass per
// length goes as the cross-section): marked "scaled" below. Within a winding type this matches
// the chart's own listed gauges to a few percent.
inline constexpr double scaled(double chart_weight, double chart_gauge, double gauge) {
  return chart_weight * (gauge / chart_gauge) * (gauge / chart_gauge);
}

// Nickel-wound 12-string regular lights 10-46 (EXL150), the set a Rickenbacker 360/12 leaves
// the factory close to. Main strings low E course first: NW046, NW036, NW026, NW021 (scaled),
// PL013, PL010; second strings: NW026 an octave over the low E, PL018, PL012, PL008 over A, D
// and G, and PL013, PL010 in unison with the B and E.
inline constexpr std::array<StringSpec, kStrings> kNickel12Main{{
    {"NW046", 0.046, 0.00038216, 0.38, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"NW036", 0.036, 0.00023964, 0.42, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"NW026", 0.026, 0.00012671, 0.50, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"NW021", 0.021, scaled(0.00012671, 0.026, 0.021), 0.52, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"PL013", 0.013, 0.00003744, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL010", 0.010, 0.00002215, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
}};
inline constexpr std::array<StringSpec, kStrings> kNickel12Course{{
    {"NW026", 0.026, 0.00012671, 0.50, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"PL018", 0.018, 0.00007199, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL012", 0.012, 0.00003190, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL008", 0.008, 0.00001418, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL013", 0.013, 0.00003744, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL010", 0.010, 0.00002215, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
}};
// Phosphor-bronze 12-string lights 10-47 (EJ38): mains PB047, PB039, PB030, PB023 (all scaled
// from the EJ16 chart's neighbours), PL014, PL010; seconds PB027 (scaled), PL018, PL012, PL008,
// PL014, PL010.
inline constexpr std::array<StringSpec, kStrings> kBronze12Main{{
    {"PB047", 0.047, scaled(0.00036722, 0.042, 0.047), 0.40, Core::Steel, 3.0e-4, 3e-4, 0.0, 0.0, true},
    {"PB039", 0.039, scaled(0.00036722, 0.042, 0.039), 0.44, Core::Steel, 3.0e-4, 3e-4, 0.0, 0.0, true},
    {"PB030", 0.030, scaled(0.00021018, 0.032, 0.030), 0.50, Core::Steel, 3.0e-4, 3e-4, 0.0, 0.0, true},
    {"PB023", 0.023, scaled(0.00011682, 0.024, 0.023), 0.52, Core::Steel, 3.0e-4, 3e-4, 0.0, 0.0, true},
    {"PL014", 0.014, 0.00004401, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL010", 0.010, 0.00002215, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
}};
inline constexpr std::array<StringSpec, kStrings> kBronze12Course{{
    {"PB027", 0.027, scaled(0.00011682, 0.024, 0.027), 0.50, Core::Steel, 3.0e-4, 3e-4, 0.0, 0.0, true},
    {"PL018", 0.018, 0.00007199, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL012", 0.012, 0.00003190, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL008", 0.008, 0.00001418, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL014", 0.014, 0.00004401, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL010", 0.010, 0.00002215, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
}};
// Chromes flatwound jazz lights 11-50 (ECG24): a flat ribbon over a round core. The chart lists
// no flatwound weights, so the wound strings are roundwound nickel scaled to gauge plus 8% (a
// ribbon packs more metal than round wire of the same outer diameter); the ribbon's turns rub
// along their whole width, so friction between the wraps is about three times a roundwound's,
// which is what makes flatwounds thud and die early in the upper partials.
inline constexpr std::array<StringSpec, kStrings> kFlatwoundJazz{{
    {"ECG050", 0.050, 1.08 * scaled(0.00038216, 0.046, 0.050), 0.45, Core::Steel, 6.0e-4, 6e-4, 0.0, 0.0, true},
    {"ECG040", 0.040, 1.08 * scaled(0.00038216, 0.046, 0.040), 0.47, Core::Steel, 6.0e-4, 6e-4, 0.0, 0.0, true},
    {"ECG030", 0.030, 1.08 * scaled(0.00023964, 0.036, 0.030), 0.50, Core::Steel, 6.0e-4, 6e-4, 0.0, 0.0, true},
    {"ECG022", 0.022, 1.08 * scaled(0.00012671, 0.026, 0.022), 0.55, Core::Steel, 6.0e-4, 6e-4, 0.0, 0.0, true},
    {"PL015", 0.015, 0.00004984, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL011", 0.011, 0.00002680, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
}};
// Nickel-wound medium 11-49 (EXL115), what heavier-handed players put on a short-scale offset.
inline constexpr std::array<StringSpec, kStrings> kNickelMedium{{
    {"NW049", 0.049, scaled(0.00038216, 0.046, 0.049), 0.38, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"NW039", 0.039, scaled(0.00023964, 0.036, 0.039), 0.42, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"NW028", 0.028, scaled(0.00012671, 0.026, 0.028), 0.50, Core::Steel, 2.0e-4, 3e-4, 0.0, 0.0, true},
    {"PL018", 0.018, 0.00007199, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL014", 0.014, 0.00004401, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
    {"PL011", 0.011, 0.00002680, 1.00, Core::Steel, 7.0e-5, 3e-4, 0.0, 0.0, false},
}};

inline const GuitarSpec& spec(int model) {
  static const std::array<GuitarSpec, kModelCount> specs = [] {
    std::array<GuitarSpec, kModelCount> g{};
    // A steel-string dreadnought (Martin D-28 pattern): 25.4" scale, 20 frets, Martin's action
    // (2.2 mm bass, 1.5 mm treble at the 12th). The low modes are Rossing and Caldersmith's
    // assembled D-28: the coupled air-top-back trio at 102, 193 and 204 Hz, the back's (0,1),
    // the top's (0,1), the seesaw pair at 376 and 436 Hz, and the higher plate modes.
    g[kDreadnought] = {
        "Steel dreadnought", Kind::Acoustic, 25.4 * kInch, 20, kPhosphorBronzeLights,
        2.2e-3, 1.5e-3, 0.45e-3, 0.30e-3, 1.2e-3,
        0.0060, 0.20, 1.0,
        {{{102.0, 20.0, 1.00}, {193.0, 30.0, 0.80}, {204.0, 35.0, 0.60}, {257.0, 30.0, 0.30}, {326.0, 30.0, 0.35},
          {376.0, 35.0, 0.50}, {436.0, 35.0, 0.45}, {509.0, 35.0, 0.30}, {643.0, 40.0, 0.30}, {733.0, 40.0, 0.25}}},
        780.0, 9000.0, 45.0, 32.0, 0.22,
        {},
        0.4, 0.003};
    // A classical guitar: 650 mm scale, 19 frets, nylon. The low modes are Woodhouse's measured
    // classical guitar (91, 205 and 225 Hz, Q 25 to 47), then the (1,0)-coupled resonance near
    // 270 Hz, the A1 air and (0,1) top modes near 390 Hz and the (2,0) near 550 Hz; above them
    // modes come every 38 Hz or so on average, as he found.
    g[kClassical] = {
        "Nylon classical", Kind::Acoustic, 0.650, 19, kNylonNormal,
        3.8e-3, 3.0e-3, 0.50e-3, 0.40e-3, 1.0e-3,
        0.0030, 0.20, 2.0,
        {{{91.0, 25.0, 1.00}, {205.0, 47.0, 0.85}, {225.0, 47.0, 0.60}, {270.0, 35.0, 0.45}, {391.0, 35.0, 0.35},
          {430.0, 35.0, 0.30}, {550.0, 40.0, 0.40}, {616.0, 40.0, 0.25}, {700.0, 40.0, 0.20}, {800.0, 40.0, 0.20}}},
        840.0, 9000.0, 40.0, 35.0, 0.18,
        {},
        0.4, 0.003};
    // A single-coil solid body (Fender Stratocaster pattern): 25.5" scale, 21 frets, nickel
    // 10-46. Pickups 152, 99 and 41 mm from the saddle; a 2.2 H coil loaded by its 250k pots, a
    // few metres of cable and the input resonates near 3 kHz and rolls off above it (fitted to
    // DI recordings of a Fender bridge pickup).
    g[kSingleCoil] = {
        "Single-coil solid", Kind::Electric, 25.5 * kInch, 21, kNickelRegularLights,
        2.0e-3, 1.6e-3, 0.45e-3, 0.30e-3, 1.1e-3,
        0.00056, 0.35, 0.3,
        {},
        0.0, 0.0, 1.0, 1.0, 0.0,
        {{{0.152, 0.0, 0.010, 2900.0, 1.6}, {0.099, 0.0, 0.010, 2900.0, 1.6}, {0.041, 0.0, 0.010, 3000.0, 1.6}}},
        0.15, 0.001};
    // A humbucker solid body (Gibson Les Paul pattern): 24.75" scale, 22 frets, the same strings,
    // two humbuckers 147 and 45 mm from the saddle whose pole rows sit 18.5 mm apart, about 4.5 H
    // each, resonating near 2.2 kHz under the same load. The middle position of the selector is
    // both pickups.
    g[kHumbucker] = {
        "Humbucker solid", Kind::Electric, 24.75 * kInch, 22, kNickelRegularLights,
        2.0e-3, 1.6e-3, 0.45e-3, 0.30e-3, 1.1e-3,
        0.00045, 0.35, 0.3,
        {},
        0.0, 0.0, 1.0, 1.0, 0.0,
        {{{0.147, 0.0185, 0.010, 2200.0, 1.5}, {0.0, 0.0, 0.0, 2200.0, 1.5}, {0.045, 0.0185, 0.010, 2300.0, 1.5}}},
        0.15, 0.001};
    // A six-string bass (Fender Bass VI pattern): 30" scale, 21 frets, six strings an octave below
    // a guitar's. Three single coils at the Stratocaster's positions scaled to the longer string
    // (179, 116 and 48 mm from the saddle), wound like a guitar's, so they resonate where the
    // Stratocaster's do. The action is a low bass setup (1.9 mm bass, 1.6 mm treble at the 12th)
    // over a low-cut nut: the slack low strings stretch sharp when fretted, and a high action
    // pushes the first frets on the low E past what a player tunes around.
    g[kBassVI] = {
        "Bass VI", Kind::Electric, 30.0 * kInch, 21, kNickelBassVI,
        1.9e-3, 1.6e-3, 0.25e-3, 0.25e-3, 1.2e-3,
        0.00056, 0.35, 0.3,
        {},
        0.0, 0.0, 1.0, 1.0, 0.0,
        {{{0.179, 0.0, 0.012, 2900.0, 1.6}, {0.116, 0.0, 0.012, 2900.0, 1.6}, {0.048, 0.0, 0.012, 3000.0, 1.6}}},
        0.15, 0.001, -12};
    // An electric twelve-string (Rickenbacker 360/12 pattern): 24.75" scale, 24 low frets, a
    // semi-hollow body under a trapeze tailpiece, EXL150 strings, the pairs reversed so the main
    // string speaks first. The "toaster" single coils are wound to about half a Stratocaster's
    // inductance, so under the same load they resonate near 5 kHz: the chime.
    g[kElectric12] = {
        "Electric 12-string", Kind::Electric, 24.75 * kInch, 24, kNickel12Main,
        1.6e-3, 1.4e-3, 0.40e-3, 0.28e-3, 0.9e-3,
        0.00080, 0.35, 0.3,
        {},
        0.0, 0.0, 1.0, 1.0, 0.0,
        {{{0.150, 0.0, 0.009, 5000.0, 1.2}, {0.0, 0.0, 0.0, 5000.0, 1.2}, {0.046, 0.0, 0.009, 5200.0, 1.2}}},
        0.10, 0.001, 0,
        true, kNickel12Course, {12, 12, 12, 12, 0, 0}, {3.0, -2.0, 4.0, 3.0, -1.5, 2.0}, false, 0.0025};
    // An acoustic twelve-string (Martin D12-28 pattern): the dreadnought's body braced heavier
    // for twelve strings' pull (about 1.8 times a six-string's), so its modes sit about 5% above
    // the D-28's and its bridge takes energy a little less readily; EJ38 strings, a high 12-string
    // action over a nut cut low (as setups for twelves are, or the first frets play sharp), the
    // octave strings struck first.
    g[kAcoustic12] = {
        "Acoustic 12-string", Kind::Acoustic, 25.4 * kInch, 20, kBronze12Main,
        2.4e-3, 1.8e-3, 0.40e-3, 0.30e-3, 1.2e-3,
        0.0050, 0.20, 1.0,
        {{{107.0, 20.0, 1.00}, {203.0, 30.0, 0.80}, {214.0, 35.0, 0.60}, {270.0, 30.0, 0.30}, {342.0, 30.0, 0.35},
          {395.0, 35.0, 0.50}, {458.0, 35.0, 0.45}, {534.0, 35.0, 0.30}, {675.0, 40.0, 0.30}, {770.0, 40.0, 0.25}}},
        800.0, 9000.0, 45.0, 32.0, 0.22,
        {},
        0.25, 0.003, 0,
        true, kBronze12Course, {12, 12, 12, 12, 0, 0}, {4.0, -3.0, 3.0, 5.0, -2.0, 2.0}, true, 0.0030};
    // A jazz archtop (Gibson ES-175 pattern): 24.75" scale, 20 frets, a laminated hollow body with
    // a floating bridge (which drains the strings faster than a solid body), Chromes flatwounds over
    // a nut cut low for their stiff wound strings, and two humbuckers 162 and 50 mm from the saddle.
    g[kArchtop] = {
        "Jazz archtop", Kind::Electric, 24.75 * kInch, 20, kFlatwoundJazz,
        2.0e-3, 1.7e-3, 0.36e-3, 0.28e-3, 1.0e-3,
        0.0012, 0.30, 0.5,
        {},
        0.0, 0.0, 1.0, 1.0, 0.0,
        {{{0.162, 0.0185, 0.010, 2100.0, 1.4}, {0.0, 0.0, 0.0, 2100.0, 1.4}, {0.050, 0.0185, 0.010, 2300.0, 1.4}}},
        0.15, 0.002};
    // A grand auditorium cutaway (Taylor 914ce pattern): 25.5" scale, EJ16 lights, a low Taylor
    // action. Its lower bout is about a tenth narrower than a dreadnought's, so the D-28's modes
    // are scaled up by the inverse of the size (an estimate: no measured set was found).
    g[kAuditorium] = {
        "Grand auditorium", Kind::Acoustic, 25.5 * kInch, 20, kPhosphorBronzeLights,
        2.0e-3, 1.6e-3, 0.42e-3, 0.28e-3, 1.1e-3,
        0.0055, 0.20, 1.0,
        {{{110.0, 24.0, 1.00}, {205.0, 30.0, 0.80}, {218.0, 35.0, 0.60}, {270.0, 30.0, 0.30}, {345.0, 30.0, 0.35},
          {395.0, 35.0, 0.50}, {455.0, 35.0, 0.45}, {530.0, 35.0, 0.30}, {670.0, 40.0, 0.30}, {760.0, 40.0, 0.25}}},
        820.0, 9000.0, 48.0, 32.0, 0.22,
        {},
        0.4, 0.003};
    // A plate-bridge single coil (Fender Telecaster pattern): 25.5" scale, 21 frets, strings
    // through the body, the bridge pickup slanted in a steel plate 40 mm from the saddle (the
    // plate's eddy currents and the coil's higher wind give a sharper peak near 3.8 kHz), a chromed
    // neck pickup 155 mm up whose cover lowers and softens its peak.
    g[kPlateSingleCoil] = {
        "Plate single-coil", Kind::Electric, 25.5 * kInch, 21, kNickelRegularLights,
        2.0e-3, 1.6e-3, 0.45e-3, 0.30e-3, 0.9e-3,
        0.00055, 0.35, 0.3,
        {},
        0.0, 0.0, 1.0, 1.0, 0.0,
        {{{0.155, 0.0, 0.010, 2700.0, 1.1}, {0.0, 0.0, 0.0, 2700.0, 1.1}, {0.040, 0.0, 0.010, 3800.0, 2.0}}},
        0.15, 0.001};
    // A short-scale offset (Fender Mustang pattern): 24" scale, 22 frets, EXL115 mediums, a
    // floating vibrato bridge that saps sustain, two low-wound single coils 140 and 55 mm from the
    // saddle resonating near 4.2 kHz.
    g[kOffset] = {
        "Offset short scale", Kind::Electric, 24.0 * kInch, 22, kNickelMedium,
        1.8e-3, 1.5e-3, 0.45e-3, 0.30e-3, 1.0e-3,
        0.0010, 0.35, 0.3,
        {},
        0.0, 0.0, 1.0, 1.0, 0.0,
        {{{0.140, 0.0, 0.010, 4200.0, 1.4}, {0.0, 0.0, 0.0, 4200.0, 1.4}, {0.055, 0.0, 0.010, 4400.0, 1.4}}},
        0.15, 0.002};
    // A fibreglass hollow body (Valco "Res-O-Glas" pattern, sold as the Airline): 24.75" scale,
    // 20 frets, two moulded fibreglass shells, a high action, two small single coils 150 and 45 mm
    // from the saddle resonating near 5 kHz: bright, thin and raw.
    g[kFibreglass] = {
        "Fibreglass hollow", Kind::Electric, 24.75 * kInch, 20, kNickelRegularLights,
        2.4e-3, 2.0e-3, 0.50e-3, 0.35e-3, 0.9e-3,
        0.0010, 0.35, 0.3,
        {},
        0.0, 0.0, 1.0, 1.0, 0.0,
        {{{0.150, 0.0, 0.012, 5000.0, 1.5}, {0.0, 0.0, 0.0, 5000.0, 1.5}, {0.045, 0.0, 0.012, 5200.0, 1.6}}},
        0.15, 0.002};
    // A semi-hollow (Gibson ES-335 pattern): 24.75" scale, 22 frets, EXL110, a thinline body with a
    // maple block under the bridge, so it rings almost as long as a solid body, and two humbuckers
    // where a Les Paul has them.
    g[kSemiHollow] = {
        "Semi-hollow", Kind::Electric, 24.75 * kInch, 22, kNickelRegularLights,
        2.0e-3, 1.6e-3, 0.45e-3, 0.30e-3, 1.1e-3,
        0.00052, 0.35, 0.3,
        {},
        0.0, 0.0, 1.0, 1.0, 0.0,
        {{{0.147, 0.0185, 0.010, 2250.0, 1.5}, {0.0, 0.0, 0.0, 2250.0, 1.5}, {0.045, 0.0185, 0.010, 2350.0, 1.5}}},
        0.15, 0.001};
    // A fully hollow thinline with P-90s (Epiphone Casino pattern): 24.75" scale, 20 frets, EXL110,
    // a floating bridge on a trapeze tailpiece that drains the strings faster, and two P-90s 155 and
    // 48 mm from the saddle: single coils with a wide window (two rows of adjustable poles over bar
    // magnets) and about three times a Stratocaster coil's turns, so they resonate lower and
    // broader, near 2.8 kHz.
    g[kThinlineP90] = {
        "Thinline P-90", Kind::Electric, 24.75 * kInch, 20, kNickelRegularLights,
        2.0e-3, 1.7e-3, 0.45e-3, 0.30e-3, 1.0e-3,
        0.0014, 0.30, 0.4,
        {},
        0.0, 0.0, 1.0, 1.0, 0.0,
        {{{0.155, 0.0, 0.018, 2700.0, 1.3}, {0.0, 0.0, 0.0, 2700.0, 1.3}, {0.048, 0.0, 0.018, 2900.0, 1.3}}},
        0.15, 0.002};
    return g;
  }();
  return specs[static_cast<size_t>(std::clamp(model, 0, kModelCount - 1))];
}

// --- the neck --------------------------------------------------------------------------------------

// Distance of fret n from the nut on an equal-tempered neck.
inline double fret_position(double scale, int fret) { return scale * (1.0 - std::exp2(-fret / 12.0)); }

// The physics of one string on one guitar in one tuning.
struct StringPhysics {
  double open_hz = 82.41;     // what the open string is tuned to
  double mu = 0.0;            // kg/m
  double tension = 0.0;       // N at pitch
  double ea = 0.0, ei = 0.0;  // core stiffnesses: stretching, bending
  double impedance = 0.0;     // sqrt(T mu), kg/s
  double diameter = 0.0;      // m
  double compensation = 0.0;  // saddle setback beyond the scale length, m
  double action = 0.0, nut = 0.0;
  StringSpec spec{};
};

// How much longer the string's path is with a note fretted than free: what fretting stretches it
// by. Heights are over the plane of the fret crowns. The free string runs straight from the nut
// (its clearance over the frets) to the saddle, set so it clears the 12th fret by the action.
// Fretted, it runs from the nut down into the gap to the fingertip (pressing a quarter of a fret
// height below the crowns, 40% of the way back toward the previous fret), over the fret's crown,
// and on to the saddle. Friction at the fret is ignored, so one tension acts along the whole length.
inline double fretting_stretch(const StringPhysics& p, const GuitarSpec& g, double length, int fret) {
  if (fret <= 0) return 0.0;
  const double nut_h = p.nut;
  const double x12 = fret_position(g.scale_m, 12);
  const double saddle_h = nut_h + (p.action - nut_h) * length / x12;
  const double x = fret_position(g.scale_m, fret);
  const double x_finger = x - 0.4 * (x - fret_position(g.scale_m, fret - 1));
  const double depth = 0.25 * g.fret_height_m;
  const double path = std::hypot(x_finger, nut_h + depth) + std::hypot(x - x_finger, depth) + std::hypot(length - x, saddle_h);
  const double straight = std::hypot(length, saddle_h - nut_h);
  return std::max(0.0, path - straight);
}

// The pitch fret n plays: the string pressed down (stretched, so tenser) speaking from the fret to
// the compensated saddle. A stiff string bends over a characteristic length sqrt(E I / T) where it
// is held, so it vibrates as if shorter: the term that makes short (fretted) lengths go sharp, and
// with the fretting stretch the reason saddles are set back. A fret crown and a saddle hold the
// string somewhere between pinned (no shortening) and clamped (the full length at each end); half
// the clamped figure at each end brings the setbacks to the 2 mm treble and 4 to 5 mm bass that
// compensated steel-string saddles have.
inline double fretted_hz(const StringPhysics& p, const GuitarSpec& g, int fret, double compensation) {
  const double length = g.scale_m + compensation;
  const double speaking = length - fret_position(g.scale_m, fret);
  const double tension = p.tension + p.ea * fretting_stretch(p, g, length, fret) / length;
  const double bend = std::sqrt(p.ei / tension);
  return std::sqrt(tension / p.mu) / (2.0 * (speaking - bend));
}

// `string` counts the main strings 0 to 5 and a twelve-string's second strings 6 to 11; a second
// string sits in its course's slot on the bridge and nut, so it shares that position's action.
inline StringPhysics string_physics(const GuitarSpec& g, int string, double open_hz) {
  StringPhysics p;
  const int index = std::clamp(string, 0, kMaxStrings - 1);
  const size_t s = static_cast<size_t>(index % kStrings);
  p.spec = index >= kStrings ? g.course[s] : g.strings[s];
  p.open_hz = open_hz;
  p.mu = p.spec.unit_weight * kPoundPerInch;
  p.diameter = p.spec.gauge_in * kInch;
  const double t = static_cast<double>(s) / (kStrings - 1);
  p.action = lerp(g.action_bass_m, g.action_treble_m, t);
  p.nut = lerp(g.nut_bass_m, g.nut_treble_m, t);
  const double dc = p.diameter * p.spec.core_ratio;
  const double area = kPi * dc * dc / 4.0;
  const auto stiffen = [&](double tension) {
    const double young = p.spec.core == Core::Nylon ? nylon_young(tension / area) : kSteelYoung;
    p.ea = young * area;
    p.ei = p.spec.ei_measured > 0.0 ? p.spec.ei_measured : young * kPi * dc * dc * dc * dc / 64.0;
  };
  // Tuned open, the tension is whatever brings the open string to pitch over the compensated
  // length. The compensation is set as a luthier sets it: the saddle moves back until the 12th
  // fret plays an exact octave. A few rounds of the two converge.
  // The tension that tunes the open string over a given length (the stiffness term makes it a
  // fixed point; it converges in a few steps).
  const auto tune_open = [&](double length) {
    double tension = p.mu * std::pow(2.0 * length * open_hz, 2.0);
    for (int i = 0; i < 8; ++i) {
      stiffen(tension);
      const double effective = length - std::sqrt(p.ei / tension);
      tension = p.mu * std::pow(2.0 * effective * open_hz, 2.0);
    }
    stiffen(tension);
    return tension;
  };
  p.compensation = 0.0;
  for (int round = 0; round < 6; ++round) {
    double lo = -0.002, hi = 0.020;  // a bass bridge's saddles travel further back than a guitar's
    for (int i = 0; i < 48; ++i) {
      const double mid = 0.5 * (lo + hi);
      StringPhysics trial = p;
      trial.tension = tune_open(g.scale_m + mid);
      if (fretted_hz(trial, g, 12, mid) > 2.0 * open_hz) lo = mid; else hi = mid;
    }
    p.compensation = 0.5 * (lo + hi);
  }
  p.tension = tune_open(g.scale_m + p.compensation);
  p.impedance = std::sqrt(p.tension * p.mu);
  return p;
}

// Inharmonicity coefficient of a speaking length: B = pi^2 E I / (T L^2).
inline double inharmonicity(const StringPhysics& p, double speaking) {
  return kPi * kPi * p.ei / (p.tension * speaking * speaking);
}

// --- losses --------------------------------------------------------------------------------------

// The real part of the bridge's admittance (s/kg) at a frequency: the floor plus the low body
// modes' peaks, where the top takes energy from the string fastest. Above the low modes an
// acoustic top's modes crowd together and overlap, and it soaks up energy over the whole band:
// the floor rises with frequency (levelling off at twice by 1.2 kHz),
// which is what makes a flat-top's upper partials die within a few seconds while its fundamental
// rings on.
inline double bridge_conductance(const GuitarSpec& g, double hz) {
  double sum = g.bridge_conductance;
  if (g.kind == Kind::Acoustic) sum *= 1.0 + std::min(hz, 1200.0) / 1200.0;
  if (g.kind == Kind::Acoustic)
    for (const Mode& m : g.modes) {
      if (m.hz <= 0.0) continue;
      const double x = m.q * (hz / m.hz - m.hz / hz);
      sum += g.bridge_conductance * 4.0 * m.gain / (1.0 + x * x);
    }
  return sum;
}

// Air drag on a vibrating cylinder (Stokes, in Valette's form), as a decay rate in s^-1.
inline double air_rate(const StringPhysics& p, double hz) {
  if (p.spec.air_rate > 0.0) return p.spec.air_rate;
  const double r = 2.0 * kPi * kAirViscosity + 2.0 * kPi * p.diameter * std::sqrt(kPi * kAirViscosity * kAirDensity * hz);
  return r / (2.0 * p.mu);
}

// The amplitude decay rate (s^-1) of the partial at `hz` of a string speaking `length` metres with
// fundamental `f0`: air drag, the wire's internal losses (Woodhouse's form, with bending loss
// weighted by the share of the restoring force stiffness provides at that wavenumber), the bridge
// (2 Z G of the wave's amplitude on every round trip, f0 of them a second), plus the player's palm
// mute and the strings' age. `bridge_scale` picks the plane.
inline double decay_rate(const StringPhysics& p, const GuitarSpec& g, double f0, double hz, double length,
                         double bridge_scale, double mute, double age) {
  const double w = kTau * hz;
  const double k = kPi * (hz / f0) / length;
  const double stiff = p.ei * k * k;
  // Old strings: grime packed into the windings and a dulled surface add loss that grows with
  // frequency. Fully dead, a wound string's 2 kHz partial is gone in about a tenth of a second
  // and a plain one's in about a second, while the fundamental hardly changes.
  const double grime = 1.0 + age * (p.spec.wound ? 25.0 : 8.0) * std::min(hz / 1000.0, 4.0);
  const double eta = (p.tension * p.spec.eta_f * grime + stiff * p.spec.eta_b) / (p.tension + stiff);
  const double internal = 0.5 * w * eta;
  const double bridge = 2.0 * p.impedance * bridge_conductance(g, hz) * bridge_scale * f0;
  // A palm resting by the saddle: flesh is a broadband, mostly resistive load, hardest on the
  // upper partials.
  const double palm = mute * (5.0 + 0.010 * hz);
  return air_rate(p, hz) + internal + bridge + palm;
}

// --- signal pieces -------------------------------------------------------------------------------

// One rail of a waveguide: a power-of-two circular buffer read at integer, linear or four-point
// Lagrange fractional delays, writable in place (for the pluck).
class Rail {
 public:
  void prepare(size_t capacity) {
    size_t n = 16;
    while (n < capacity + 8) n <<= 1;
    buffer_.assign(n, 0.0f);
    mask_ = n - 1;
    write_ = 0;
  }
  void clear() { std::fill(buffer_.begin(), buffer_.end(), 0.0f); }
  size_t capacity() const { return buffer_.size() - 8; }
  void push(float x) { write_ = (write_ + 1) & mask_; buffer_[write_] = x; }
  // The sample pushed `k` pushes ago (0: the latest).
  float at(size_t k) const { return buffer_[(write_ - k) & mask_]; }
  float& slot(size_t k) { return buffer_[(write_ - k) & mask_]; }
  float linear(double d) const {
    d = std::clamp(d, 0.0, static_cast<double>(capacity()));
    const size_t i = static_cast<size_t>(d);
    const float f = static_cast<float>(d - static_cast<double>(i));
    return at(i) + (at(i + 1) - at(i)) * f;
  }
  // Third-order Lagrange: flat through the band a string spends its energy in, and smooth under
  // a delay that moves every sample (bends, vibrato, tension glide).
  float lagrange(double d) const {
    d = std::clamp(d, 1.0, static_cast<double>(capacity() - 2));
    const size_t i = static_cast<size_t>(d);
    const double t = d - static_cast<double>(i) + 1.0;  // position among taps i-1 .. i+2
    const double h0 = -(t - 1.0) * (t - 2.0) * (t - 3.0) / 6.0;
    const double h1 = t * (t - 2.0) * (t - 3.0) / 2.0;
    const double h2 = -t * (t - 1.0) * (t - 3.0) / 2.0;
    const double h3 = t * (t - 1.0) * (t - 2.0) / 6.0;
    return static_cast<float>(h0 * at(i - 1) + h1 * at(i) + h2 * at(i + 1) + h3 * at(i + 2));
  }
  void add(double d, float x) {
    d = std::clamp(d, 0.0, static_cast<double>(capacity()));
    const size_t i = static_cast<size_t>(d);
    const float f = static_cast<float>(d - static_cast<double>(i));
    slot(i) += x * (1.0f - f);
    slot(i + 1) += x * f;
  }
 private:
  std::vector<float> buffer_;
  size_t mask_ = 0, write_ = 0;
};

// The bridge's reflection filter for one plane: an inverted one-pole loss filter fitted to the
// string's decay at two partials, then a chain of identical first-order allpasses fitted to its
// stiffness dispersion.
constexpr int kSplitSections = 2;
struct ReflectionDesign {
  // Loss: a shelf (pole and zero: a step up in loss above a corner frequency) times a gentle
  // one-pole low-pass (loss that keeps rising), times a gain:
  //   H = gain (1 - pole) / (1 - pole z^-1) * (1 + zero z^-1) / (1 + zero) * (1 - lp) / (1 - lp z^-1)
  // A string loses energy roughly in proportion to frequency with steps at the body's modes;
  // a single one-pole (loss with the square of frequency) left the mid partials ringing for
  // several times their real length.
  double gain = 0.999, pole = 0.0, zero = 0.0, lp = 0.0;
  double allpass = 0.0;             // dispersion coefficient (negative: high partials come round early)
  int sections = 0;
  // Second-order allpasses (a1, a2) that give one plane a slightly different, irregular phase:
  // the bridge's admittance is not the same in the two directions and varies with frequency, so
  // each partial's polarization split is its own.
  std::array<std::array<double, 2>, kSplitSections> split{};
};

// Phase delays (samples) at w (rad/sample).
inline double loss_delay(const ReflectionDesign& r, double w) {
  // Pole: + atan(p sin w / (1 - p cos w)); zero (1 + q z^-1): - atan(-q sin w / (1 + q cos w)).
  const double pole = r.pole == 0.0 ? 0.0 : std::atan2(r.pole * std::sin(w), 1.0 - r.pole * std::cos(w));
  const double zero = r.zero == 0.0 ? 0.0 : std::atan2(r.zero * std::sin(w), 1.0 + r.zero * std::cos(w));
  const double lp = r.lp == 0.0 ? 0.0 : std::atan2(r.lp * std::sin(w), 1.0 - r.lp * std::cos(w));
  return (pole + zero + lp) / w;
}
inline double allpass_delay(double c, double w) {
  // H = (c + z^-1) / (1 + c z^-1): phase -w + 2 atan(c sin w / (1 + c cos w)).
  return 1.0 - 2.0 * std::atan2(c * std::sin(w), 1.0 + c * std::cos(w)) / w;
}
// Phase delay of a second-order allpass (a2 + a1 z^-1 + z^-2) / (1 + a1 z^-1 + a2 z^-2).
inline double allpass2_delay(double a1, double a2, double w) {
  if (a1 == 0.0 && a2 == 0.0) return 2.0;
  const std::complex<double> z1 = std::polar(1.0, -w), z2 = z1 * z1;
  const std::complex<double> h = (a2 + a1 * z1 + z2) / (1.0 + a1 * z1 + a2 * z2);
  double phase = std::arg(h);
  // Unwrap toward the continuous phase, which runs from 0 to -2 pi.
  while (phase > 0.0) phase -= kTau;
  if (w > 1e-9 && phase < -kTau + 1e-12) phase += kTau;
  return -phase / w;
}
inline double reflection_delay(const ReflectionDesign& r, double w) {
  double d = loss_delay(r, w) + (r.sections > 0 ? r.sections * allpass_delay(r.allpass, w) : 0.0);
  for (const auto& s : r.split) if (s[0] != 0.0 || s[1] != 0.0) d += allpass2_delay(s[0], s[1], w);
  return d;
}

// Fits the loss filter so one pass (one round trip of the string) loses exactly the decay rate
// rates[0] at the fundamental, and the decay rates of the partials above it (rates[n - 1] for
// partial n, up to `count`) as closely as a one-pole can, in least squares on the dB lost per
// pass. Fitting at only two partials left the ones between them ringing several times too long.
inline void fit_loss_curve(ReflectionDesign& r, double f0, const double* rates, int count, double rate) {
  constexpr int kMax = 64;
  count = std::min(count, kMax);
  std::array<double, kMax + 1> cw{};
  for (int n = 1; n <= count; ++n) cw[static_cast<size_t>(n)] = std::cos(std::min(kTau * f0 * n / rate, kPi * 0.95));
  // The shelf is set by its corner pole and its plateau (the gain far above the corner relative
  // to below it); the zero follows from those two.
  const auto zero_for = [](double a, double plateau) { return ((1.0 - a) - plateau * (1.0 + a)) / ((1.0 - a) + plateau * (1.0 + a)); };
  // Squared magnitude of the whole loss filter (without its gain) at cos w.
  const auto mag2 = [](double a, double q, double b, double c) {
    return (1.0 + 2.0 * q * c + q * q) / (1.0 - 2.0 * a * c + a * a) / (1.0 - 2.0 * b * c + b * b);
  };
  const auto cost = [&](double a, double plateau, double b) {
    const double q = zero_for(a, plateau);
    const double ref = mag2(a, q, b, cw[1]);
    // No frequency below the fundamental may come round with more than it went in: a shelf
    // whose corner sits under the fundamental would need gain at DC, and capping that afterwards
    // would cost the fundamental its sustain.
    const double dc_gain = -0.5 * std::log(mag2(a, q, b, 1.0) / ref) + rates[0] / f0;  // nepers of loss at DC
    if (dc_gain < 0.0) return 1e6 * (1.0 - dc_gain);
    double sum = 0.0;
    for (int n = 2; n <= count; ++n) {
      const double lost = -0.5 * std::log(mag2(a, q, b, cw[static_cast<size_t>(n)]) / ref);  // nepers per pass beyond the fundamental's
      const double want = (std::max(rates[n - 1], rates[0]) - rates[0]) / f0;
      const double d = lost - want;
      sum += d * d / n;
    }
    return sum;
  };
  // Search in the corner, the plateau's depth (log of 1 - plateau) and the low-pass pole.
  const auto plateau_of = [](double depth) { return 1.0 - std::exp(depth); };
  double ba = 0.9, bd = std::log(0.01), bb = 0.0, best = 1e300;
  for (double a : {0.0, 0.5, 0.8, 0.9, 0.95, 0.98, 0.99, 0.995})
    for (double d : {-9.0, -7.0, -5.5, -4.5, -3.5, -2.5})
      for (double b : {0.0, 0.2, 0.4, 0.6}) {
        const double c = cost(a, plateau_of(d), b);
        if (c < best) { best = c; ba = a; bd = d; bb = b; }
      }
  for (double step = 0.1; step > 1e-4; step *= 0.5)
    for (int round = 0; round < 6; ++round) {
      bool moved = false;
      const std::array<std::array<double, 3>, 6> dirs{{{step, 0, 0}, {-step, 0, 0}, {0, step * 10, 0}, {0, -step * 10, 0}, {0, 0, step}, {0, 0, -step}}};
      for (const auto& dir : dirs) {
        const double a = std::clamp(ba + dir[0], 0.0, 0.999), d = std::clamp(bd + dir[1], -12.0, -0.5), b = std::clamp(bb + dir[2], 0.0, 0.95);
        const double c = cost(a, plateau_of(d), b);
        if (c < best) { best = c; ba = a; bd = d; bb = b; moved = true; }
      }
      if (!moved) break;
    }
  r.pole = ba;
  r.zero = zero_for(ba, plateau_of(bd));
  r.lp = bb;
  // The gain that loses exactly the fundamental's rate; then make sure no frequency gains.
  const auto mag = [&](double c) { return (1.0 - r.pole) * (1.0 - r.lp) / (1.0 + r.zero) * std::sqrt(mag2(r.pole, r.zero, r.lp, c)); };
  double gain = std::exp(-rates[0] / f0) / mag(cw[1]);
  double peak = 0.0;
  for (int k = 0; k <= 128; ++k) peak = std::max(peak, gain * mag(std::cos(kPi * k / 128.0)));
  if (peak > 0.99999) gain *= 0.99999 / peak;
  r.gain = gain;
}

// The two-point fit (kept for the tests of the filter itself).
inline void fit_loss(ReflectionDesign& r, double f0, double s1, double fk, double sk, double rate) {
  const double g1 = std::exp(-s1 / f0), gk = std::exp(-std::max(sk, s1) / f0);
  const double w1 = kTau * f0 / rate, wk = kTau * std::min(fk, rate * 0.45) / rate;
  const auto mag2 = [](double a, double w) { return 1.0 - 2.0 * a * std::cos(w) + a * a; };
  const double want = gk / g1;  // |H(wk)| / |H(w1)|
  double lo = 0.0, hi = 0.999;
  for (int i = 0; i < 60; ++i) {
    const double a = 0.5 * (lo + hi);
    if (std::sqrt(mag2(a, w1) / mag2(a, wk)) > want) lo = a; else hi = a;
  }
  r.pole = 0.5 * (lo + hi);
  r.gain = std::min(g1 * std::sqrt(mag2(r.pole, w1)) / (1.0 - r.pole), 0.99999);
}

// Fits the dispersion chain: a run of identical first-order allpasses whose coefficient puts the
// loop's partials where the stiff string has them, n f1 sqrt((1 + B n^2) / (1 + B)), with the
// fundamental tuned exactly by the delay. Identical sections cannot follow the stiff string's
// curve exactly across forty partials, so the fit is least squares in cents weighted by 1/n^2
// (the low partials carry the pitch and most of the energy) over the partials below 4 kHz:
// within a cent or two up to the 8th partial on the bass strings, under a cent on the trebles.
// Sixteen sections always fit best, so only the coefficient is searched. Returns the weighted
// residual.
constexpr int kMaxDispersion = 16;
inline double fit_dispersion(ReflectionDesign& r, double f1, double b, double rate) {
  r.sections = 0; r.allpass = 0.0;
  const double period = rate / f1;
  const double top = std::min(4000.0, rate * 0.4);
  int count = 0;
  while (count < 48 && (count + 1) * f1 * std::sqrt(1.0 + b * (count + 1) * (count + 1)) < top) ++count;
  // Below a cent of stretch at the top fitted partial the allpasses are not worth having.
  if (count < 2 || 1200.0 * std::log2(std::sqrt((1.0 + b * count * count) / (1.0 + b))) < 1.0) return 0.0;
  const auto error = [&](double c) {
    ReflectionDesign t = r; t.sections = kMaxDispersion; t.allpass = c;
    const double base = period - reflection_delay(t, kTau * f1 / rate);  // the delay that tunes partial 1
    if (base < period * 0.3) return 1e12;
    double sum = 0.0;
    for (int n = 2; n <= count; ++n) {
      const double fn = n * f1 * std::sqrt((1.0 + b * n * n) / (1.0 + b));
      const double w = kTau * fn / rate;
      // Loop phase at the target frequency against the 2 pi n a resonance needs, in cents.
      const double cents = 1200.0 * std::log2(kTau * n / (w * (base + reflection_delay(t, w))));
      sum += cents * cents / (n * n);
    }
    return sum;
  };
  double lo = -0.95, hi = 0.0;
  for (int i = 0; i < 32; ++i) {  // golden section on c
    const double a = hi - 0.618 * (hi - lo), c = lo + 0.618 * (hi - lo);
    if (error(a) < error(c)) hi = c; else lo = a;
  }
  r.sections = kMaxDispersion;
  r.allpass = 0.5 * (lo + hi);
  return error(r.allpass);
}

// A second-order resonator with unity peak gain, for the body's modes.
class Resonator {
 public:
  void set(double hz, double q, double gain, double rate) {
    const double w = kTau * std::clamp(hz, 10.0, rate * 0.45) / rate;
    const double r = std::exp(-w / (2.0 * std::max(q, 0.5)));
    a1_ = static_cast<float>(2.0 * r * std::cos(w));
    a2_ = static_cast<float>(-r * r);
    b_ = static_cast<float>(gain * (1.0 - r * r) * 0.5);
  }
  float process(float x) {
    const float y = b_ * (x - x2_) + a1_ * y1_ + a2_ * y2_;
    x2_ = x1_; x1_ = x; y2_ = y1_; y1_ = y;
    return y;
  }
  void reset() { x1_ = x2_ = y1_ = y2_ = 0.0f; }
 private:
  float a1_ = 0.0f, a2_ = 0.0f, b_ = 0.0f, x1_ = 0.0f, x2_ = 0.0f, y1_ = 0.0f, y2_ = 0.0f;
};

// --- one plane of one string -----------------------------------------------------------------------

// Two rails: `upper` carries waves from the nut (index 0) toward the bridge, `lower` from the
// bridge (index 0) back to the nut. The reflection filter sits at the bridge; the nut (or the
// fret under the finger) reflects with the fretting finger's loss.
class Plane {
 public:
  void prepare(size_t capacity) {
    upper_.prepare(capacity); lower_.prepare(capacity);
    scratch_.assign(capacity + 8, 0.0f);
    clear();
  }
  void clear() {
    upper_.clear(); lower_.clear();
    loss_state_ = nut_state_ = loss_in_ = lp_state_ = 0.0f;
    allpass_state_.fill(0.0f);
    for (auto& z : split_state_) z.fill(0.0f);
  }
  void design(const ReflectionDesign& d) {
    loss_b_ = static_cast<float>(d.gain * (1.0 - d.pole) / (1.0 + d.zero) * (1.0 - d.lp));
    loss_a_ = static_cast<float>(d.pole);
    loss_q_ = static_cast<float>(d.zero);
    lp_a_ = static_cast<float>(d.lp);
    allpass_ = static_cast<float>(d.allpass);
    sections_ = d.sections;
    for (int k = 0; k < kSplitSections; ++k) {
      split_a1_[static_cast<size_t>(k)] = static_cast<float>(d.split[static_cast<size_t>(k)][0]);
      split_a2_[static_cast<size_t>(k)] = static_cast<float>(d.split[static_cast<size_t>(k)][1]);
    }
  }
  // The second-order split allpasses (direct form II), on one sample.
  float split(float x, std::array<std::array<float, 2>, kSplitSections>& state) const {
    for (int k = 0; k < kSplitSections; ++k) {
      const float a1 = split_a1_[static_cast<size_t>(k)], a2 = split_a2_[static_cast<size_t>(k)];
      if (a1 == 0.0f && a2 == 0.0f) continue;
      auto& z = state[static_cast<size_t>(k)];
      const float w = x - a1 * z[0] - a2 * z[1];
      x = a2 * w + a1 * z[0] + z[1];
      z[1] = z[0]; z[0] = w;
    }
    return x;
  }
  // One sample, in two halves so the bridge can mix the two planes in between. `bridge`: reads
  // the wave arriving at the bridge (`upper_delay` back) and reflects it through the filters;
  // returns the arriving wave and leaves the reflection in `reflected`.
  float bridge(double upper_delay, float& reflected) {
    const float arriving = upper_.lagrange(upper_delay);
    loss_state_ = loss_b_ * (arriving + loss_q_ * loss_in_) + loss_a_ * loss_state_;
    loss_in_ = arriving;
    lp_state_ = loss_state_ + lp_a_ * lp_state_;
    float y = -lp_state_;
    for (int m = 0; m < sections_; ++m) {
      float& z = allpass_state_[static_cast<size_t>(m)];
      const float out = allpass_ * y + z;
      z = y - allpass_ * out;
      y = out;
    }
    reflected = split(y, split_state_);
    return arriving;
  }
  // `finish`: sends the reflected wave (plus the bridge's own displacement, a moving end) back
  // toward the nut, and reflects the wave arriving at the nut (`lower_len` back) with the
  // fretting finger's loss.
  void finish(float reflected, size_t lower_len, float nut_gain, float nut_pole) {
    lower_.push(reflected);
    nut_state_ = (1.0f - nut_pole) * lower_.at(lower_len) + nut_pole * nut_state_;
    upper_.push(-nut_gain * nut_state_);
  }
  float tick(double upper_delay, size_t lower_len, float bridge_in, float nut_gain, float nut_pole) {
    float reflected = 0.0f;
    const float arriving = bridge(upper_delay, reflected);
    finish(reflected + bridge_in, lower_len, nut_gain, nut_pole);
    return arriving;
  }
  // Adds what the bridge would have sent back toward the nut had `arrivals` (oldest first) come
  // in at it over the last `n` samples: runs the reflection filters over them from rest and adds
  // the result to the rail back and to the filters' own state. Linear, so it lays a pluck on top
  // of whatever the string is already doing, and the part of the wave the filters stand in for is
  // inside them, where it belongs.
  void preroll(const float* arrivals, size_t n) {
    n = std::min(n, scratch_.size());
    float loss = 0.0f, in1 = 0.0f, lpv = 0.0f;
    std::array<float, kMaxDispersion> state{};
    std::array<std::array<float, 2>, kSplitSections> split_pre{};
    for (size_t i = 0; i < n; ++i) {
      loss = loss_b_ * (arrivals[i] + loss_q_ * in1) + loss_a_ * loss;
      in1 = arrivals[i];
      lpv = loss + lp_a_ * lpv;
      float y = -lpv;
      for (int m = 0; m < sections_; ++m) {
        float& z = state[static_cast<size_t>(m)];
        const float out = allpass_ * y + z;
        z = y - allpass_ * out;
        y = out;
      }
      scratch_[i] = split(y, split_pre);
    }
    for (size_t i = 0; i < n; ++i) lower_.slot(n - 1 - i) += scratch_[i];
    loss_state_ += loss;
    loss_in_ += in1;
    lp_state_ += lpv;
    for (int m = 0; m < sections_; ++m) allpass_state_[static_cast<size_t>(m)] += state[static_cast<size_t>(m)];
    for (int k = 0; k < kSplitSections; ++k)
      for (int j = 0; j < 2; ++j) split_state_[static_cast<size_t>(k)][static_cast<size_t>(j)] += split_pre[static_cast<size_t>(k)][static_cast<size_t>(j)];
  }
  Rail& upper() { return upper_; }
  Rail& lower() { return lower_; }
  const Rail& upper() const { return upper_; }
  const Rail& lower() const { return lower_; }
  void scale(float k, size_t span) {
    for (size_t i = 0; i <= span + 4; ++i) { upper_.slot(i) *= k; lower_.slot(i) *= k; }
    loss_state_ *= k; nut_state_ *= k; loss_in_ *= k; lp_state_ *= k;
    for (auto& z : allpass_state_) z *= k;
    for (auto& z : split_state_) { z[0] *= k; z[1] *= k; }
  }
 private:
  Rail upper_, lower_;
  float loss_b_ = 0.99f, loss_a_ = 0.0f, loss_q_ = 0.0f, loss_in_ = 0.0f, lp_a_ = 0.0f, lp_state_ = 0.0f, allpass_ = 0.0f;
  int sections_ = 0;
  float loss_state_ = 0.0f, nut_state_ = 0.0f;
  std::array<float, kMaxDispersion> allpass_state_{};
  std::vector<float> scratch_;
  std::array<float, kSplitSections> split_a1_{}, split_a2_{};
  std::array<std::array<float, 2>, kSplitSections> split_state_{};
};

// --- the player's settings ------------------------------------------------------------------------

enum PickupSelect : int { kNeckPickup, kMiddlePickup, kBridgePickup, kNeckAndBridge };
constexpr int kPickupChoices = 4;

struct Setup {
  int model = kDreadnought;
  int tuning = 0;
  int capo = 0;
  double pluck_m = 0.09;    // pluck point from the saddle
  double hardness = 0.6;    // 0 the flesh of a finger, 1 a hard plectrum
  int pickup = kNeckPickup;
  double mute = 0.0;        // palm mute
  double age = 0.0;         // 0 new strings, 1 dead ones
  int position = 0;         // the fretting hand's position (fret)
  double slip_scale = 1.0;  // scales the release time (calibration against recordings only)
  // The room the guitar is in. It was tuned at 20 C; the strings and the neck expand differently
  // as the room warms or cools, and the top takes up or gives off water with the humidity.
  double humidity = 0.45;   // relative, 0..1
  double celsius = 20.0;
};

// Linear thermal expansion, per kelvin: carbon steel; nylon 6 (a string's polymer, near room
// temperature); maple and mahogany along the grain, which is what sets the neck's length (Wood
// Handbook ch. 4: 3 to 4.5e-6 parallel to the grain).
constexpr double kSteelExpansion = 11.7e-6, kNylonExpansion = 8.0e-5, kNeckExpansion = 3.8e-6;

// A string tuned at 20 C in a room at `celsius`: held between nut and saddle on a neck that grows
// less than the string would, so it slackens as it warms (and tightens as it cools) by
// E A (alpha_string - alpha_neck) dT. A steel string goes about a cent flat per degree, a nylon
// one several times that.
inline void warm(StringPhysics& p, double celsius) {
  const double expansion = p.spec.core == Core::Nylon ? kNylonExpansion : kSteelExpansion;
  p.tension = std::max(0.2 * p.tension, p.tension - p.ea * (expansion - kNeckExpansion) * (celsius - 20.0));
  p.impedance = std::sqrt(p.tension * p.mu);
}

// --- the guitar ------------------------------------------------------------------------------------

class Guitar {
 public:
  struct Fingering { int string = -1; int fret = 0; int note = -1; };

  void prepare(double rate) {
    rate_ = rate;
    // Room for a string bent or detuned down to 15 Hz.
    const size_t capacity = static_cast<size_t>(rate / 15.0) + 16;
    for (auto& s : strings_) { s.v.prepare(capacity); s.h.prepare(capacity); }
    shape_.assign(3 * capacity + 64, 0.0f);
    preroll_.assign(capacity + 8, 0.0f);
    smooth_.assign(3 * capacity + 64, 0.0f);
    env_fall_ = static_cast<float>(std::exp(-1.0 / (0.05 * rate)));
    tension_glide_ = 1.0 - std::exp(-1.0 / (0.002 * rate));
    static_fade_ = static_cast<float>(std::exp(-1.0 / (0.03 * rate)));
    leak_ = static_cast<float>(1.0 - kTau * 5.0 / rate);
    dc_.set(10.0, rate);
    configured_ = false;
    configure(setup_);
    reset();
  }

  void reset() {
    for (int i = 0; i < kMaxStrings; ++i) {
      String& s = strings_[static_cast<size_t>(i)];
      s.v.clear(); s.h.clear();
      s.held = false; s.damping = false; s.free = false; s.env = 0.0f; s.excite_left = 0;
      s.ratio = 1.0; s.tension_ratio = s.tension_target = 1.0; s.elongation = 0.0; s.push = 0.0f;
      s.prev_v = s.prev_h = 0.0f; s.pickup_prev.fill(0.0f);
      s.fret = setup_.capo;
      if (i < count()) tune(i, setup_.capo, true);
    }
    pending_.fill({});
    for (auto& m : body_) m.reset();
    acceleration_.reset(); radiation_.reset();
    pickup_filter_.reset();
    dc_.reset();
    static_force_ = 0.0f;
    awake_ = false;
  }

  // Takes the player's settings for this block. A new guitar, tuning or capo re-strings the
  // instrument: ringing strings sound on as they were until they are played again; idle ones
  // take the new pitch at once.
  void configure(const Setup& in) {
    Setup s = in;
    s.model = std::clamp(s.model, 0, kModelCount - 1);
    s.tuning = std::clamp(s.tuning, 0, kTuningCount - 1);
    s.capo = std::clamp(s.capo, 0, 12);
    s.pickup = std::clamp(s.pickup, 0, kPickupChoices - 1);
    s.humidity = std::clamp(s.humidity, 0.0, 1.0);
    s.celsius = std::clamp(s.celsius, -20.0, 50.0);
    const bool restring = !configured_ || s.model != setup_.model || s.tuning != setup_.tuning || s.capo != setup_.capo ||
                          s.celsius != setup_.celsius;
    const bool rebody = !configured_ || s.model != setup_.model || s.pickup != setup_.pickup ||
                        s.humidity != setup_.humidity || s.celsius != setup_.celsius;
    const bool reloss = s.mute != setup_.mute || s.age != setup_.age;
    setup_ = s;
    const GuitarSpec& g = guitar();
    if (restring)
      for (auto& string : dispersion_) for (auto& d : string) d.valid = false;
    if (restring) {
      for (int i = 0; i < count(); ++i) {
        physics_[static_cast<size_t>(i)] = string_physics(g, i, open_hz(i));
        warm(physics_[static_cast<size_t>(i)], setup_.celsius);
        String& st = strings_[static_cast<size_t>(i)];
        if (!st.held && st.env < kQuiet) { st.fret = setup_.capo; tune(i, setup_.capo, true); }
      }
      // A six-string has no second strings: whatever a twelve-string left ringing on them stops.
      for (int i = count(); i < kMaxStrings; ++i) {
        String& st = strings_[static_cast<size_t>(i)];
        st.v.clear(); st.h.clear();
        st.held = st.damping = st.free = false;
        st.env = 0.0f; st.push = 0.0f; st.excite_left = 0;
      }
      pending_.fill({});
    }
    if (rebody) {
      build_output(g);
      exchange_cos_ = static_cast<float>(std::cos(g.plane_exchange));
      exchange_sin_ = static_cast<float>(std::sin(g.plane_exchange));
    }
    if (rebody || (reloss && !restring))
      for (int i = 0; i < count(); ++i) {
        design_reflection(i);
        strings_[static_cast<size_t>(i)].tuned_ratio = -1.0;
      }
    configured_ = true;
  }

  const Setup& setup() const { return setup_; }
  const GuitarSpec& guitar() const { return spec(setup_.model); }
  // How many strings the instrument has: six, or twelve in six courses.
  int count() const { return guitar().twelve ? kMaxStrings : kStrings; }
  // Strings 0 to 5 are the courses' main strings; on a twelve-string, 6 to 11 are their second
  // strings.
  const StringPhysics& physics(int string) const { return physics_[static_cast<size_t>(std::clamp(string, 0, kMaxStrings - 1))]; }
  int open_note(int string) const {
    return kTunings[static_cast<size_t>(setup_.tuning)].notes[static_cast<size_t>(std::clamp(string, 0, kStrings - 1))] + guitar().transpose;
  }
  // What a string is tuned to: a second string is its course's note plus the course interval,
  // left a few cents off by the player's ear.
  double open_hz(int string) const {
    const GuitarSpec& g = guitar();
    if (string < kStrings) return midi_to_hz(open_note(string));
    const size_t c = static_cast<size_t>(std::clamp(string - kStrings, 0, kStrings - 1));
    return midi_to_hz(open_note(static_cast<int>(c)) + g.course_interval[c]) * cents_ratio(g.course_cents[c]);
  }

  // Where a guitarist would play `note`: on a string where it lies between the capo and the top
  // fret, preferring a string that is free (playing on a held one chokes it) and a fret inside
  // the hand's span (position to position + 4), open strings when the hand is in the open
  // position, and the lower fret when it is a toss-up. Notes outside the instrument's range move
  // by octaves into it, as a player would take them.
  Fingering finger(int note) const {
    const GuitarSpec& g = guitar();
    const int capo = setup_.capo;
    const int low = open_note(0) + capo, high = open_note(kStrings - 1) + g.frets;
    while (note < low) note += 12;
    while (note > high) note -= 12;
    const int hand = std::max(setup_.position, capo);
    Fingering best;
    double best_cost = 1e9;
    for (int s = 0; s < kStrings; ++s) {
      const int fret = note - open_note(s);
      if (fret < capo || fret > g.frets) continue;
      const String& st = strings_[static_cast<size_t>(s)];
      double cost;
      if (fret == capo) cost = hand <= capo + 3 ? 0.0 : 3.0;
      else if (fret < hand) cost = 1.0 * (hand - fret);
      else if (fret <= hand + 4) cost = 0.1 * (fret - hand);
      else cost = 1.0 * (fret - hand - 4);
      if (st.held) cost += 100.0;
      else if (st.env > 1e-5f) cost += 1.5;  // let a ringing string ring
      cost += 0.01 * fret;
      if (cost < best_cost) { best_cost = cost; best = {s, fret, note}; }
    }
    return best;
  }

  // Plucks the course `string` stopped at `fret`. `velocity` 0..1 sets how far the string is
  // pulled (and, through that, how fast it slips off); `seed` varies the pluck, since no two are
  // alike. On a twelve-string the pick goes through both strings of the course: the one it meets
  // first now, the other a couple of milliseconds later (sooner on a hard, fast stroke). The
  // thinner octave string is pushed as far but carries less, and the pick has spent a little on
  // the first string, so the second string sounds a little softer.
  void pluck(int string, int fret, double velocity, uint32_t seed) {
    if (string < 0 || string >= kStrings) return;
    const GuitarSpec& g = guitar();
    if (!g.twelve) { pluck_string(string, fret, velocity, seed); return; }
    const int second = string + kStrings;
    const int first_hit = g.octave_first ? second : string, second_hit = g.octave_first ? string : second;
    pluck_string(first_hit, fret, velocity, seed);
    Pending& p = pending_[static_cast<size_t>(string)];
    const double gap = g.course_gap_s * (1.3 - 0.6 * std::clamp(velocity, 0.0, 1.0));
    p.string = second_hit;
    p.fret = fret;
    p.velocity = 0.85 * velocity;
    p.seed = seed ^ 0x9e3779b9u;
    p.samples = std::max(1, static_cast<int>(gap * rate_));
    // Both strings of the course are under the fingers from the moment the stroke starts.
    String& later = strings_[static_cast<size_t>(second_hit)];
    later.held = true; later.damping = false; later.free = false;
  }

  // Plucks one string (0 to 5 a course's main string, 6 to 11 a twelve-string's second string).
  void pluck_string(int string, int fret, double velocity, uint32_t seed) {
    if (string < 0 || string >= count()) return;
    String& st = strings_[static_cast<size_t>(string)];
    rng_.seed(seed ^ 0x6a09e667u);
    st.held = true;
    st.damping = false;
    st.free = false;
    st.fret = std::clamp(fret, 0, guitar().frets);
    tune(string, st.fret, true);
    const double length = st.speaking, half = st.period * 0.5;
    // The pick meets the moving string and mostly stops it before pulling it aside.
    st.v.scale(0.15f, static_cast<size_t>(st.period) + 4);
    st.h.scale(0.15f, static_cast<size_t>(st.period) + 4);
    // Human variation: a few millimetres in where, a decibel in how hard, degrees in which way.
    const double where = std::clamp(setup_.pluck_m + 0.003 * rng_.bipolar(), 0.008, length - 0.008);
    const double vel = std::clamp(velocity * (1.0 + 0.06 * rng_.bipolar()), 0.02, 1.2);
    const double hardness = std::clamp(setup_.hardness, 0.0, 1.0);
    // Displacement at the pluck point, from a feather touch to a hard dig (metres).
    const double amplitude = (0.25 + 2.75 * vel) * 1e-3;
    // Contact width (finger flesh about 15 mm, a pick's edge 1 mm) and slip-off time (a fingertip
    // rolls off in about a third of a millisecond, a pick snaps off in a few hundredths; harder
    // plucks snap faster) round the triangle's corner over a stretch of string, never more than a
    // sixth of it: a finger does not smear a treble string end to end.
    const double contact = lerp(0.015, 0.001, hardness);
    const double slip_s = lerp(0.35e-3, 0.03e-3, hardness) * (1.3 - 0.6 * std::min(vel, 1.0)) * setup_.slip_scale;
    const double c = 2.0 * length * st.hz;  // wave speed
    const double width = std::min(contact + c * slip_s, length / 6.0);
    // Mostly parallel to the top for a pick stroke, more into it for a finger.
    const double angle = (lerp(38.0, 22.0, hardness) + 8.0 * rng_.bipolar()) * kPi / 180.0;
    const double point = where / length * half;  // samples from the bridge
    build_shape(half, point, width / length * half);
    add_shape(st, st.v, half, amplitude * std::sin(angle));
    add_shape(st, st.h, half, amplitude * std::cos(angle));
    // The pick scraping off: a few milliseconds of noise at the contact point, louder on a wound
    // string, hardly any from a finger.
    st.excite_left = static_cast<int>(rate_ * lerp(0.004, 0.0025, hardness));
    st.excite_level = static_cast<float>(amplitude * 0.004 * hardness * hardness * (st.wound ? 1.6 : 0.6));
    st.excite_point = point;
    st.excite_tone.set(lerp(800.0, 2500.0, hardness), rate_);
    st.env = static_cast<float>(amplitude * 0.5);
    st.onset = 0.0f;
    st.onset_step = static_cast<float>(1.0 / std::max(1.0, rate_ * lerp(2.0e-3, 0.5e-3, hardness)));
    st.elongation = 0.0;
    st.elongation_clock = 0;
    measure_elongation(string);
    st.tension_ratio = st.tension_target;
    // Velocities are differences from the previous sample, so that sample is set to where the
    // string is now: otherwise the first sample after a pluck reads the whole displacement as a
    // jump and the pickup clicks.
    st.prev_v = st.v.upper().lagrange(st.upper_delay);
    st.prev_h = st.h.upper().lagrange(st.upper_delay_h);
    for (int c = 0; c < st.tap_count; ++c) st.pickup_prev[static_cast<size_t>(c)] = tap_displacement(st, st.taps[static_cast<size_t>(c)]);
    // The bridge force this pluck starts with (see render): what the arriving wave's velocity
    // will be in the first sample.
    {
      const float z2 = static_cast<float>(2.0 * physics_[static_cast<size_t>(string)].impedance);
      const auto first = [&](const Plane& p, double upper) { return (p.upper().linear(upper - 1.0) - p.upper().linear(upper)) * static_cast<float>(rate_); };
      static_force_ += z2 * (first(st.v, st.upper_delay) + static_cast<float>(guitar().horizontal_coupling) * first(st.h, st.upper_delay_h));
    }
    awake_ = true;
  }

  // A legato note on a ringing string: the fingertip lands on (hammer-on) or leaves for
  // (pull-off) another fret without a new pluck, or slides there (`slide`; the voice then glides
  // the pitch by the ratio it supplies). On a twelve-string one fingertip stops both strings of
  // the course.
  void legato(int string, int fret, double velocity, bool slide) {
    if (string < 0 || string >= kStrings) return;
    legato_string(string, fret, velocity, slide);
    if (!guitar().twelve) return;
    legato_string(string + kStrings, fret, velocity, slide);
    Pending& p = pending_[static_cast<size_t>(string)];
    if (p.samples > 0) p.fret = fret;
  }

  // The key is up: the fretting hand lifts and damps the course so it falls 60 dB in
  // `release_s`, or sooner if the string would die sooner on its own. A second string the pick
  // has not reached yet is not struck at all.
  void release(int string, double release_s) {
    if (string < 0 || string >= kStrings) return;
    pending_[static_cast<size_t>(string)].samples = 0;
    damp_string(string, release_s);
    if (guitar().twelve) damp_string(string + kStrings, release_s);
    // The last note let go and nothing left to ring: the hands come to rest on the strings, and
    // the ones that were only ringing in sympathy are muted too, a little more gently.
    for (int i = 0; i < count(); ++i) if (strings_[static_cast<size_t>(i)].held || strings_[static_cast<size_t>(i)].free) return;
    const double damp_s = std::max(0.005, release_s);
    for (int i = 0; i < count(); ++i) {
      String& other = strings_[static_cast<size_t>(i)];
      if (!other.damping && other.env > kQuiet) { other.damping = true; other.damp_s = 2.0 * damp_s; apply_damping(other); }
    }
  }
  // Lets go without damping (the sustain pedal is down: let it ring).
  void let_ring(int string) {
    if (string < 0 || string >= kStrings) return;
    for (int i : {string, string + kStrings}) {
      if (i >= count()) break;
      strings_[static_cast<size_t>(i)].held = false;
      strings_[static_cast<size_t>(i)].free = true;
    }
  }
  bool held(int string) const { return string >= 0 && string < kStrings && strings_[static_cast<size_t>(string)].held; }
  int fret(int string) const { return strings_[static_cast<size_t>(std::clamp(string, 0, kStrings - 1))].fret; }

  // The pitch the voice wants as a multiple of the fretted pitch: bends, vibrato, fine tuning and
  // slides all arrive this way, and the loop follows (both strings of a course, which bend
  // together under one finger).
  void set_ratio(int string, double ratio) {
    if (string < 0 || string >= kStrings) return;
    strings_[static_cast<size_t>(string)].ratio = std::clamp(ratio, 0.25, 4.0);
    if (guitar().twelve) strings_[static_cast<size_t>(string + kStrings)].ratio = std::clamp(ratio, 0.25, 4.0);
  }

  bool awake() const { return awake_; }
  // How strongly the selected pickups pick up the mains field around them, against a single
  // coil's: a single coil 1; a humbucker's two reverse-wound, reverse-magnet coils cancel it to
  // about -24 dB; two pickups together average; an acoustic has no coil, 0.
  double hum_sensitivity() const {
    const GuitarSpec& g = guitar();
    if (g.kind != Kind::Electric) return 0.0;
    const auto one = [](const Pickup& pk) { return pk.aperture_m <= 0.0 ? 0.0 : pk.coil_spacing_m > 0.0 ? 0.063 : 1.0; };
    if (both_pickups(g)) return 0.5 * (one(g.pickups[kNeckPickup]) + one(g.pickups[kBridgePickup]));
    return one(g.pickups[static_cast<size_t>(setup_.pickup)]);
  }
  // How long the longest-ringing open string takes to fall 60 dB, for the host's tail.
  double longest_decay_s() const {
    double t = 0.5;
    for (int i = 0; i < count(); ++i) {
      const StringPhysics& p = physics_[static_cast<size_t>(i)];
      const double r = decay_rate(p, guitar(), p.open_hz, p.open_hz, guitar().scale_m, guitar().horizontal_coupling, setup_.mute, setup_.age);
      t = std::max(t, 6.91 / std::max(r, 1e-3));
    }
    return std::min(t, 60.0);
  }
  // What a tuner would read from a string at a fret once the pluck's tension glide has settled.
  double pitch_at(int string, int fret) const {
    const StringPhysics& p = physics(string);
    return fretted_hz(p, guitar(), fret, p.compensation);
  }
  double speaking_length(int string, int fret) const {
    const StringPhysics& p = physics(string);
    return guitar().scale_m + p.compensation - fret_position(guitar().scale_m, fret);
  }
  double inharmonicity_at(int string, int fret) const { return inharmonicity(physics(string), speaking_length(string, fret)); }
  // The displacement along a string (metres, both planes summed), sampled from the bridge to the
  // nut at one point per sample of travel: what the tests read the plucked shape from.
  void string_shape(int string, std::vector<float>& out) const {
    const String& st = strings_[static_cast<size_t>(std::clamp(string, 0, kMaxStrings - 1))];
    out.clear();
    for (int i = 0; i <= static_cast<int>(st.period * 0.5); ++i) out.push_back(displacement(st.v, st, i) + displacement(st.h, st, i));
  }
  // The pitch rise tension modulation is causing right now, as a ratio.
  double tension_ratio(int string) const { return strings_[static_cast<size_t>(std::clamp(string, 0, kMaxStrings - 1))].tension_ratio; }

  // One output sample: the radiated sound of an acoustic or the pickup voltage of an electric,
  // at a digital level.
  float render() {
    if (!awake_) return 0.0f;
    const GuitarSpec& g = guitar();
    const bool electric = g.kind == Kind::Electric;
    // The pick reaching a course's second string.
    for (auto& p : pending_)
      if (p.samples > 0 && --p.samples == 0) pluck_string(p.string, p.fret, p.velocity, p.seed);
    // Any string sounding keeps them all running: the quiet ones are listening at the bridge.
    const int strings = count();
    bool any = false;
    for (int i = 0; i < strings; ++i) any = any || strings_[static_cast<size_t>(i)].held || strings_[static_cast<size_t>(i)].env > kQuiet;
    if (!any) {
      for (int i = 0; i < strings; ++i) quiet(i);
      awake_ = false;
      return 0.0f;
    }
    // The bridge moves with the total force on it, so each string's end is pushed by the others
    // (its own push is already in its loss filter). The bridge's displacement comes from its
    // velocity, conductance times force, through a leaky integrator; one sample late, which keeps
    // the junction explicit and is negligible at these couplings.
    float total = 0.0f;
    for (int i = 0; i < strings; ++i) total += strings_[static_cast<size_t>(i)].push;
    float force_sum = 0.0f, pickup = 0.0f;
    for (int i = 0; i < strings; ++i) {
      String& st = strings_[static_cast<size_t>(i)];
      const StringPhysics& p = physics_[static_cast<size_t>(i)];
      // The tension glides sample by sample toward the last measurement: a stepped loop length
      // would jump the pickup's view of the string and click.
      st.tension_ratio += (st.tension_target - st.tension_ratio) * tension_glide_;
      const double ratio = st.ratio * st.tension_ratio;
      if (std::abs(ratio - st.tuned_ratio) > 2e-6) retune(st, ratio);
      if (st.excite_left > 0) {
        --st.excite_left;
        const float n = st.excite_tone.low(rng_.bipolar()) * st.excite_level;
        st.h.upper().add(std::max(0.0, st.upper_delay_h - st.excite_point), n);
        st.h.lower().add(std::max(0.0, st.excite_point - st.filter_delay_h), n);
        st.v.upper().add(std::max(0.0, st.upper_delay - st.excite_point), n * 0.5f);
      }
      const float in = static_cast<float>(g.sympathetic) * (total - st.push);
      // The bridge turns a little of each plane's wave into the other on every reflection, an
      // exact rotation so it moves energy without making any: what a pick puts across the top
      // drifts into the plane that drives it, and the note's decay bends from one rate to the other.
      float rv = 0.0f, rh = 0.0f;
      const float av = st.v.bridge(st.upper_delay, rv);
      const float ah = st.h.bridge(st.upper_delay_h, rh);
      st.v.finish(exchange_cos_ * rv + exchange_sin_ * rh + in, st.lower_len, st.nut_gain, st.nut_pole);
      st.h.finish(exchange_cos_ * rh - exchange_sin_ * rv + in * static_cast<float>(g.horizontal_coupling), st.lower_len, st.nut_gain, st.nut_pole);
      // Force on the bridge: twice the impedance times the velocity of the arriving wave.
      const float vel_v = (av - st.prev_v) * static_cast<float>(rate_);
      const float vel_h = (ah - st.prev_h) * static_cast<float>(rate_);
      st.prev_v = av; st.prev_h = ah;
      const float z2 = static_cast<float>(2.0 * p.impedance);
      // A raised-cosine fade over the release.
      const float fade = st.onset >= 1.0f ? 1.0f : 0.5f - 0.5f * std::cos(static_cast<float>(kPi) * st.onset);
      st.onset = std::min(1.0f, st.onset + st.onset_step);
      force_sum += fade * z2 * (vel_v + static_cast<float>(g.horizontal_coupling) * vel_h);
      st.push = leak_ * st.push + static_cast<float>(g.bridge_conductance / rate_) * z2 * vel_v;
      if (electric) pickup += fade * sense(st);
      const float level = std::max(std::abs(av), std::abs(ah));
      st.env = std::max(level, st.env * env_fall_);
      if (++st.elongation_clock >= 32) { st.elongation_clock = 0; measure_elongation(i); }
      // A string that has been damped out: the finger leaves and it is an open string again, at
      // rest.
      if (!st.held && st.damping && st.env < kQuiet * 10.0f) quiet(i);
    }
    float out;
    if (electric) {
      out = pickup_filter_.low(pickup);
    } else {
      // A plucked string pulls on the bridge before it is let go, but the pull builds over the
      // tenth of a second the finger or pick takes to push it aside, too slowly to sound. The
      // model starts at the moment of release, already pulling: that standing pull is taken
      // off and handed back over the same tenth of a second, so the release does not click.
      force_sum -= static_force_;
      static_force_ *= static_fade_;
      // A little of the bridge force straight through stands in for the top's modes above the
      // generated set and the strings' own radiation.
      float sum = force_sum * static_cast<float>(direct_);
      for (int m = 0; m < body_count_; ++m) sum += body_[static_cast<size_t>(m)].process(force_sum);
      // Radiation: the sound pressure follows the acceleration of the air the body moves, so it
      // rises 6 dB an octave until the body stops being small against the wavelength (near
      // 350 Hz); below the air resonance the top and the soundhole push the air in opposite
      // directions and cancel, a further 12 dB an octave.
      out = radiation_.process(acceleration_.high(sum));
    }
    return dc_.process(out * static_cast<float>(output_scale_));
  }

 private:
  static constexpr float kQuiet = 2e-9f;  // metres: a couple of nanometres is silence

  // A course's second string, waiting for the pick to reach it.
  struct Pending { int samples = 0, string = 0, fret = 0; double velocity = 0.0; uint32_t seed = 0; };

  void legato_string(int string, int fret, double velocity, bool slide) {
    String& st = strings_[static_cast<size_t>(string)];
    const int old = st.fret;
    st.held = true; st.damping = false; st.free = false;
    st.fret = std::clamp(fret, setup_.capo, guitar().frets);
    tune(string, st.fret, false);
    if (!slide) {
      // The fingertip striking the fret, or flicking the string sideways as it leaves.
      const double kick = (st.fret > old ? 0.3 : 0.5) * (0.25 + 2.75 * velocity) * 1e-3;
      const double by_fret = std::min(4.0, st.period * 0.05);
      st.v.upper().add(by_fret, static_cast<float>(kick * 0.5));
      st.h.upper().add(by_fret, static_cast<float>(kick * 0.85));
    }
    st.env = std::max(st.env, 1e-4f);
    awake_ = true;
  }

  void damp_string(int string, double release_s) {
    String& st = strings_[static_cast<size_t>(string)];
    st.held = false;
    st.free = false;
    st.damping = true;
    st.damp_s = std::max(0.005, release_s);
    apply_damping(st);
  }

  struct String {
    Plane v, h;
    bool held = false, damping = false, wound = false, free = false;
    // The release: a string is not let go in an instant (about 2 ms off a fingertip, half a
    // millisecond off a pick), so what it does to the bridge and the pickup comes in over that
    // time rather than as a click.
    float onset = 1.0f, onset_step = 1.0f;
    int fret = 0;
    double hz = 82.41, base_hz = 82.41, speaking = 0.65, period = 582.0;
    double ratio = 1.0, tension_ratio = 1.0, tension_target = 1.0, tuned_ratio = -1.0, elongation = 0.0;
    int elongation_clock = 0;
    double upper_delay = 290.0, upper_delay_h = 290.0, damp_s = 0.1;
    // The bridge filters' delay at the pitch, per plane. The loop is the rail into the bridge
    // (half a period, so a wave anywhere on the string reaches the bridge when it physically
    // would), the filters, and the rail back to the nut (half a period less the filters).
    double filter_delay = 0.0, filter_delay_h = 0.0;
    size_t lower_len = 290;
    float nut_gain = 1.0f, nut_pole = 0.0f;
    float env = 0.0f, prev_v = 0.0f, prev_h = 0.0f, push = 0.0f;
    ReflectionDesign rv, rh;
    int excite_left = 0;
    float excite_level = 0.0f;
    double excite_point = 0.0;
    OnePole excite_tone;
    std::array<double, 4> taps{};  // pickup coil centres, samples from the bridge
    int tap_count = 0;
    std::array<float, 4> pickup_prev{};
  };

  // Sets a string to a fret: speaking length, pitch, and bridge filters fitted to the physics at
  // that length. `split` re-divides the loop between the rails (a new pluck); otherwise only the
  // rail into the bridge changes length and the waves on the string travel on (hammer-on, slide).
  void tune(int index, int fret, bool split) {
    String& st = strings_[static_cast<size_t>(index)];
    const StringPhysics& p = physics_[static_cast<size_t>(index)];
    st.wound = p.spec.wound;
    st.speaking = speaking_length(index, fret);
    st.base_hz = fretted_hz(p, guitar(), fret, p.compensation);
    st.hz = st.base_hz;
    st.period = rate_ / st.hz;
    design_reflection(index);
    // The fretting finger: a stopped string ends on the fret, but the flesh behind it takes a
    // little; an open string ends on the rigid nut.
    st.nut_gain = fret > setup_.capo ? 0.9994f : 1.0f;
    st.nut_pole = fret > setup_.capo ? 0.02f : 0.0f;
    if (st.damping) apply_damping(st);
    if (split) st.lower_len = split_for(st);
    st.tuned_ratio = -1.0;
    retune(st, st.ratio * st.tension_ratio);
  }

  void design_reflection(int index) {
    String& st = strings_[static_cast<size_t>(index)];
    const GuitarSpec& g = guitar();
    const StringPhysics& p = physics_[static_cast<size_t>(index)];
    const double f0 = st.base_hz;
    // Fit the loss at the fundamental and at the partial nearest 2.5 kHz, where the fading of a
    // pluck's brightness is heard.
    const int count = std::clamp(static_cast<int>(std::min(4000.0, rate_ * 0.4) / f0), 2, kMaxFitPartials);
    for (int plane = 0; plane < 2; ++plane) {
      ReflectionDesign& r = plane == 0 ? st.rv : st.rh;
      const double scale = plane == 0 ? 1.0 : g.horizontal_coupling;
      std::array<double, kMaxFitPartials> rates{};
      for (int n = 1; n <= count; ++n) {
        // The bridge barely gives sideways at low frequencies, but higher up it rocks, and the
        // parallel plane's loss approaches the normal one's by about 1.5 kHz.
        const double plane_scale = plane == 0 ? 1.0 : scale + (1.0 - scale) * std::min(1.0, n * f0 / 1500.0);
        rates[static_cast<size_t>(n - 1)] = decay_rate(p, g, f0, n * f0, st.speaking, plane_scale, setup_.mute, setup_.age);
      }
      fit_loss_curve(r, f0, rates.data(), count, rate_);
    }
    // The horizontal plane's own phase: two broad, weak allpass resonances at frequencies set by
    // the string (the same every time it is played), sized so the partials' splits scatter by a
    // cent or two around the mean split instead of growing in step with partial number.
    {
      Rng r(0x51e7u + static_cast<uint32_t>(index) * 977u + static_cast<uint32_t>(setup_.model) * 131u);
      const double radius = std::clamp(0.0004 * rate_ / f0, 0.02, 0.35);
      for (int k = 0; k < kSplitSections; ++k) {
        const double hz = 400.0 * std::pow(8.0, r.unipolar());
        const double theta = kTau * std::min(hz, rate_ * 0.4) / rate_;
        st.rh.split[static_cast<size_t>(k)] = {-2.0 * radius * std::cos(theta), radius * radius};
      }
    }
    // The stiffness is the same in both planes and depends only on the string and fret: fitted
    // once per re-stringing and kept (the fit takes tens of microseconds, too long for every
    // pluck of a strum).
    Dispersion& d = dispersion_[static_cast<size_t>(index)][static_cast<size_t>(std::clamp(st.fret, 0, kMaxFrets))];
    if (!d.valid) {
      ReflectionDesign fit = st.rv;
      fit_dispersion(fit, f0, inharmonicity(p, st.speaking), rate_);
      d = {fit.allpass, fit.sections, true};
    }
    st.rv.allpass = st.rh.allpass = d.allpass;
    st.rv.sections = st.rh.sections = d.sections;
    st.v.design(st.rv);
    st.h.design(st.rh);
  }

  // The rail into the bridge takes whatever the loop needs beyond the rail back to the nut and
  // the filters, so the fundamental is exactly the asked-for pitch.
  void retune(String& st, double ratio) {
    const GuitarSpec& g = guitar();
    st.hz = std::clamp(st.base_hz * ratio, 15.0, rate_ * 0.45);
    const double w = kTau * st.hz / rate_;
    st.period = rate_ / st.hz;
    // One sample of the loop is the rail into the bridge being read before it is written.
    st.filter_delay = reflection_delay(st.rv, w);
    st.filter_delay_h = reflection_delay(st.rh, w);
    // The parallel plane rings a couple of cents below the normal one; the two straddle the
    // pitch, so a tuner reading either half of the decay reads true.
    const double half_split = cents_ratio(0.5 * g.polarization_split_cents);
    double upper = st.period / half_split - 1.0 - static_cast<double>(st.lower_len) - st.filter_delay;
    if (upper < 2.0) {
      // Slid too far up for the current split: divide the loop afresh.
      st.lower_len = split_for(st);
      upper = st.period / half_split - 1.0 - static_cast<double>(st.lower_len) - st.filter_delay;
    }
    st.upper_delay = std::max(1.0, upper);
    st.upper_delay_h = std::max(1.0, st.period * half_split - 1.0 - static_cast<double>(st.lower_len) - st.filter_delay_h);
    st.tuned_ratio = ratio;
    const double half = st.period * 0.5;
    // Pickup coil centres, in samples of physical position from the bridge.
    const double per_m = half / st.speaking;
    st.tap_count = 0;
    const auto add_pickup = [&](const Pickup& pk) {
      if (pk.aperture_m <= 0.0) return;
      st.taps[static_cast<size_t>(st.tap_count++)] = pk.from_bridge_m * per_m;
      if (pk.coil_spacing_m > 0.0) st.taps[static_cast<size_t>(st.tap_count++)] = (pk.from_bridge_m + pk.coil_spacing_m) * per_m;
    };
    if (g.kind == Kind::Electric) {
      if (both_pickups(g)) { add_pickup(g.pickups[kNeckPickup]); add_pickup(g.pickups[kBridgePickup]); }
      else add_pickup(g.pickups[static_cast<size_t>(setup_.pickup)]);
    }
  }

  // The rail back to the nut: half a period less the bridge filters' delay, so the rail into the
  // bridge is half a period long.
  size_t split_for(const String& st) const {
    const double d = reflection_delay(st.rv, kTau * st.hz / rate_);
    return static_cast<size_t>(std::max(2.0, std::floor(st.period * 0.5 - d - 1.0)));
  }

  bool both_pickups(const GuitarSpec& g) const {
    return setup_.pickup == kNeckAndBridge || (setup_.pickup == kMiddlePickup && g.pickups[kMiddlePickup].aperture_m <= 0.0);
  }

  // Damping by the fretting hand: the loop loses 60 dB in damp_s, the finger's flesh taking the
  // top end first.
  void apply_damping(String& st) {
    const double g = std::pow(1e-3, 1.0 / std::max(1.0, st.damp_s * st.hz));
    st.nut_gain = std::min(st.nut_gain, static_cast<float>(g));
    st.nut_pole = std::max(st.nut_pole, 0.35f);
  }

  // A string that has rung out: cleared to exact zeros (no denormals) and, if a finger was on it,
  // an open string again, free to ring in sympathy.
  void quiet(int index) {
    String& st = strings_[static_cast<size_t>(index)];
    st.v.clear(); st.h.clear();
    st.env = 0.0f; st.prev_v = st.prev_h = 0.0f; st.push = 0.0f;
    st.pickup_prev.fill(0.0f);
    st.damping = false;
    st.free = false;
    st.tension_ratio = st.tension_target = 1.0;
    st.elongation = 0.0;
    st.fret = setup_.capo;
    tune(index, setup_.capo, true);
  }

  // Displacement of a plane at `x` samples of physical position from the bridge: the wave
  // heading for the bridge that will arrive in x samples, minus the one that arrived x samples ago
  // and came back off it inverted. Reading the returning wave from the rail's history rather
  // than from the rail back to the nut ignores one pass of the gentle bridge filters, but it is
  // there right up to the saddle, where the bridge pickup sits and the rail back is still inside
  // the filters.
  float displacement(const Plane& p, const String& st, double x) const {
    const double upper = &p == &st.v ? st.upper_delay : st.upper_delay_h;
    x = std::clamp(x, 0.0, st.period * 0.5);
    return p.upper().linear(std::max(0.0, upper - x)) - p.upper().linear(upper + x);
  }

  // The displacement a coil sees: both planes (the vertical more, since it moves the string toward
  // and away from the magnet), averaged over the magnet's window.
  float tap_displacement(const String& st, double at) const {
    const double window = 0.5 * 0.010 * (st.period * 0.5) / st.speaking;
    float y = 0.0f;
    for (int k = -1; k <= 1; ++k) y += displacement(st.v, st, at + k * window) + 0.45f * displacement(st.h, st, at + k * window);
    return y * (1.0f / 3.0f);
  }

  // The magnetic pickup: the string's velocity over each coil (both planes, the vertical more,
  // since it moves the string toward and away from the magnet), averaged over the magnet's
  // window; coils in series for a humbucker, pickups in parallel when two are selected.
  float sense(String& st) {
    if (st.tap_count == 0) return 0.0f;
    float total = 0.0f;
    for (int c = 0; c < st.tap_count; ++c) {
      const float y = tap_displacement(st, st.taps[static_cast<size_t>(c)]);
      total += (y - st.pickup_prev[static_cast<size_t>(c)]) * static_cast<float>(rate_);
      st.pickup_prev[static_cast<size_t>(c)] = y;
    }
    return total / static_cast<float>(st.tap_count);
  }

  // The string's stretch from the waves on it (both planes): the tension rises by E A times the
  // strain, the pitch by the square root of that. The squared slope is summed along each rail on
  // its own: the cross term between the two travelling waves averages to nothing over a period,
  // and joining them point by point would count the stretch of string the bridge filters stand in
  // for as a step.
  void measure_elongation(int index) {
    String& st = strings_[static_cast<size_t>(index)];
    const StringPhysics& p = physics_[static_cast<size_t>(index)];
    const double half = st.period * 0.5;
    if (half < 4.0) return;
    const auto slope2 = [](const Rail& r, size_t n) {
      double sum = 0.0;
      for (size_t i = 1; i <= n; ++i) { const double d = r.at(i) - r.at(i - 1); sum += d * d; }
      return sum;
    };
    const size_t upper = static_cast<size_t>(st.upper_delay), upper_h = static_cast<size_t>(st.upper_delay_h);
    const double sum = slope2(st.v.upper(), upper) + slope2(st.v.lower(), st.lower_len) +
                       slope2(st.h.upper(), upper_h) + slope2(st.h.lower(), st.lower_len);
    // The integral of the squared slope over the length, over twice the length.
    const double dx = st.speaking / half;
    const double strain = sum / dx / (2.0 * st.speaking);
    // The tension follows the string's energy, not each cycle of it.
    st.elongation += (strain - st.elongation) * (st.elongation == 0.0 ? 1.0 : 0.3);
    st.tension_target = std::sqrt(1.0 + p.ea * st.elongation / p.tension);
  }

  // The plucked shape: a triangle of unit height peaked `point` samples from the bridge on a
  // string `half` samples long, smoothed twice by a box `width` samples wide. Both ends are fixed,
  // so the smoothing runs over the shape's odd extension past them (as the string's modes see it);
  // a box simply cut off at the end would leave a kink by the bridge and brighten the pluck.
  void build_shape(double half, double point, double width) {
    const int n = std::min(static_cast<int>(half) + 2, static_cast<int>(shape_.size()) / 3 - 2);
    point = std::clamp(point, 1.0, half - 1.0);
    const int w = std::clamp(static_cast<int>(width), 1, std::max(1, n / 3));
    const auto triangle = [&](double x) { return x <= point ? x / point : (half - x) / (half - point); };
    // ext[i + n] holds the odd extension at position i, for i in [-n, 2n].
    const auto odd = [&](int i) -> float {
      double x = static_cast<double>(i);
      double sign = 1.0;
      while (x < 0.0 || x > half) {
        if (x < 0.0) { x = -x; sign = -sign; }
        if (x > half) { x = 2.0 * half - x; sign = -sign; }
      }
      return static_cast<float>(sign * triangle(x));
    };
    for (int i = -n; i <= 2 * n; ++i) smooth_[static_cast<size_t>(i + n)] = odd(i);
    for (int pass = 0; pass < 2; ++pass) {
      double acc = 0.0;
      // A centred box: the average over [i - w/2, i - w/2 + w).
      for (int i = -n; i <= 2 * n; ++i) {
        acc += smooth_[static_cast<size_t>(i + n)];
        if (i - w >= -n) acc -= smooth_[static_cast<size_t>(i - w + n)];
        const int centre = i - w / 2;
        if (centre >= -n) shape_[static_cast<size_t>(centre + n)] = static_cast<float>(acc / w);
      }
      for (int i = -n; i <= 2 * n - w; ++i) smooth_[static_cast<size_t>(i + n)] = shape_[static_cast<size_t>(i + n)];
    }
    for (int i = 0; i <= n; ++i) shape_[static_cast<size_t>(i)] = i <= static_cast<int>(half) ? smooth_[static_cast<size_t>(i + n)] : 0.0f;
    shape_[0] = 0.0f;
    shape_len_ = half;
  }
  float shape_at(double x) const {
    if (x <= 0.0 || x >= shape_len_) return 0.0f;
    const size_t i = static_cast<size_t>(x);
    const float f = static_cast<float>(x - static_cast<double>(i));
    return shape_[i] + (shape_[i + 1] - shape_[i]) * f;
  }
  // Lays the shape onto a plane's rails at rest (zero velocity): half the displacement in each
  // travelling wave.
  // Each piece of the wave goes where it will reach the bridge at the moment it physically would:
  // the half heading for the bridge from x away arrives in x samples; the half heading for the
  // nut comes back after the round trip, less the time the filters account for.
  void add_shape(const String& st, Plane& p, double half, double amplitude) {
    const bool vertical = &p == &st.v;
    const double upper = vertical ? st.upper_delay : st.upper_delay_h;
    const float a = static_cast<float>(amplitude * 0.5);
    const size_t up = static_cast<size_t>(upper);
    for (size_t j = 0; j <= up; ++j) {
      const double x = upper - static_cast<double>(j);
      if (x <= half) p.upper().slot(j) += a * shape_at(x);
    }
    // The history past the bridge, read only by the pickups: the half heading for the nut as if
    // it had already come off the bridge, inverted.
    for (size_t x = 1; x <= static_cast<size_t>(half) + 1; ++x) p.upper().slot(up + x) -= a * shape_at(static_cast<double>(x) + (upper - static_cast<double>(up)));
    // The half heading for the nut came off the bridge: by the image method it is the inverted
    // wave that arrived x samples ago, so the bridge filters are run over that history.
    const size_t n = std::min(static_cast<size_t>(half) + 1, preroll_.size());
    for (size_t i = 0; i < n; ++i) preroll_[i] = -a * shape_at(static_cast<double>(n - i));
    p.preroll(preroll_.data(), n);
  }

  // The output stage: an acoustic's body (the measured low modes, then a dense generated set
  // above, as a soundboard's modes crowd together with frequency, each a resonator driven by the
  // bridge force) or an electric's pickup resonance.
  void build_output(const GuitarSpec& g) {
    body_count_ = 0;
    if (g.kind == Kind::Acoustic) {
      // The top at the moisture the room's air brings it to: its modes shift and broaden together,
      // and a mode's peak response goes with its Q (the modal mass is the same), so a damper top
      // answers each mode less.
      const air::WoodBody wood = air::wood_body(setup_.humidity, setup_.celsius);
      for (const Mode& m : g.modes)
        if (m.hz > 0.0 && body_count_ < kBodyModes)
          body_[static_cast<size_t>(body_count_++)].set(m.hz * wood.frequency_ratio, m.q * wood.q_ratio, m.gain * wood.q_ratio, rate_);
      Rng r(0xb0d1e5u + static_cast<uint32_t>(setup_.model));
      for (double f = g.dense_from_hz; f < g.dense_to_hz && body_count_ < kBodyModes; f += g.dense_spacing_hz * (0.6 + 0.8 * r.unipolar())) {
        // Levels scatter (no two modes alike) under a top that rolls off gently above 3 kHz.
        const double roll = 1.0 / std::sqrt(1.0 + std::pow(f / 3000.0, 2.0));
        // Two draws in two statements: within one expression the compiler picks their order, and
        // MSVC picked the other one, so Windows built every acoustic a different body.
        const double scatter = 0.3 + 0.7 * r.unipolar();
        const double sign = r.unipolar() < 0.5 ? -1.0 : 1.0;
        const double level = g.dense_level * roll * scatter * sign;
        body_[static_cast<size_t>(body_count_++)].set(f * wood.frequency_ratio, g.dense_q * (0.7 + 0.6 * r.unipolar()) * wood.q_ratio, level * wood.q_ratio, rate_);
      }
      direct_ = 0.01;
      output_scale_ = 1.0;
      acceleration_.set(350.0, rate_);
      radiation_.set(Biquad::Type::HighPass, g.modes[0].hz, 0.7, 0.0, rate_);
    } else {
      // Two pickups in parallel halve the inductance: the resonance rises by the square root of 2.
      const bool both = both_pickups(g);
      const Pickup& pk = g.pickups[static_cast<size_t>(both ? kNeckPickup : setup_.pickup)];
      pickup_filter_.prepare(rate_);
      pickup_filter_.set(pk.resonance_hz * (both ? std::sqrt(2.0) : 1.0), pk.q);
      direct_ = 0.0;
      output_scale_ = 0.07;
    }
    for (auto& m : body_) m.reset();
  }

  static constexpr int kBodyModes = 240;
  static constexpr int kMaxFitPartials = 64;
  static constexpr int kMaxFrets = 24;
  struct Dispersion { double allpass = 0.0; int sections = 0; bool valid = false; };
  std::array<std::array<Dispersion, kMaxFrets + 1>, kMaxStrings> dispersion_{};
  double rate_ = 48000.0;
  Setup setup_{};
  bool configured_ = false, awake_ = false;
  std::array<String, kMaxStrings> strings_{};
  std::array<StringPhysics, kMaxStrings> physics_{};
  std::array<Pending, kStrings> pending_{};
  std::array<Resonator, kBodyModes> body_{};
  int body_count_ = 0;
  StateVariable pickup_filter_;
  OnePole acceleration_;
  Biquad radiation_;
  DcBlocker dc_;
  double direct_ = 0.0, output_scale_ = 0.02, shape_len_ = 0.0;
  float env_fall_ = 0.9995f, leak_ = 0.999f;
  double tension_glide_ = 0.01;
  float static_force_ = 0.0f, static_fade_ = 0.999f;
  float exchange_cos_ = 1.0f, exchange_sin_ = 0.0f;
  std::vector<float> shape_, smooth_, preroll_;
  Rng rng_{0x67452301u};
};

}  // namespace fuzzboi::dsp::guitar
