# The physical guitar

The **Physical guitar** source is a guitar simulated from its physics rather than a sample or a
Karplus-Strong imitation: six real strings on a real neck over a real body, shared by every
voice the way a guitarist has one guitar. The model is `src/dsp/guitar.hpp`; the engine drives it
from `src/engine.hpp` (like the FM chip, the instrument is the engine's and the voices fret,
pluck, bend and damp its strings).

## The instruments

| Model | Pattern | Scale, frets | Strings | Output |
|---|---|---|---|---|
| Steel dreadnought | Martin D-28 | 25.4", 20 | D'Addario EJ16 phosphor bronze 12-53 | modal body (Rossing and Caldersmith's D-28 modes, then a generated dense set), radiation |
| Nylon classical | Torres-style classical | 650 mm, 19 | D'Addario EJ45 normal tension | modal body (Woodhouse's measured classical), radiation |
| Single-coil solid | Fender Stratocaster | 25.5", 21 | D'Addario EXL110 nickel 10-46 | three single coils 152/99/41 mm from the saddle, loaded resonance near 3 kHz |
| Humbucker solid | Gibson Les Paul | 24.75", 22 | EXL110 | two humbuckers 147/45 mm, coils 18.5 mm apart, resonance near 2.2 kHz; the middle selector position is both |
| Bass VI | Fender Bass VI | 30", 21 | XLB095 + NW072-NW024 (24-95) | three single coils 179/116/48 mm, resonance near 2.9 kHz; every tuning an octave below a guitar's, low action (1.9/1.6 mm) and a low-cut nut |

Eight tunings (standard, drop D, half step down, DADGAD, open G, D, E, C), a capo, and the player:
pluck point in mm from the saddle, finger to plectrum, palm mute, string age, hand position.

## What is simulated

- **Strings from the maker's data.** Mass per length is D'Addario's published unit weight; the
  tension at pitch follows from it (within 1% of their chart, `guitar_tests`). Bending stiffness
  is the core's (Zollner's core ratios for wound strings, Woodhouse's measured values for nylon
  basses; nylon's modulus rises with stress per Woodhouse and Lynch-Aird).
- **A real neck.** Frets at equal-tempered positions. Pressing a note down stretches the string
  over the fret (action, nut clearance, fret height, the fingertip in the gap), raising its
  tension; the stiff string also vibrates as if shorter where it is held. The saddle is moved back
  until the 12th fret is an exact octave, as a luthier sets it, giving setbacks of about 2 mm on
  the treble and 4-6 mm on the bass, with the plain B (acoustic) and plain G (electric) further
  back than their neighbours, as on real compensated saddles. The first fret reads a few cents
  sharp, as on a guitar with an uncompensated nut.
- **Two-rail digital waveguides, two planes per string.** The vertical plane drives the top and
  dies fast; the horizontal barely moves the bridge and rings on. They sit a little apart in pitch
  (each partial its own split), and the bridge rotates a little energy between them on every
  reflection, so a note beats and its decay bends.
- **Losses from physics.** Air drag (Stokes), the wire's internal friction and bending losses
  (Woodhouse's form), and the bridge's conductance (rising with the body's modal density on an
  acoustic, the neck's on a solid body), fitted per partial up to 4 kHz by a shelf-plus-low-pass
  reflection filter.
- **Stiffness dispersion.** Sixteen identical allpasses fitted to put the partials at
  n f1 sqrt((1 + B n^2) / (1 + B)); the B measured from the audio matches the physics within 40%
  (`guitar_tests`) and real recordings within a factor of 1.5 on most notes (the critic).
- **The pluck.** The string held in a triangle at the pluck point, rounded by the contact width
  and slip-off time of a fingertip or a pick, released at rest; the half travelling away from the
  bridge is run through the bridge filters, so the loop starts exactly as such a string would. A
  pick scrapes (a little low-passed noise). Every pluck varies a little in place, force and angle.
- **Tension modulation.** The string's elongation, summed from its slope, raises its tension:
  hard plucks start 5 to 20 cents sharp and settle.
- **Sympathetic strings.** The bridge's motion drives the other strings; open strings tuned to a
  played note's partials ring on. The coupling is scaled so octave tunings stay stable.
- **Pickups and body.** Magnetic pickups sense string velocity over their aperture at their real
  positions (a humbucker's two coils, two pickups in parallel), into the coil's loaded resonance.
  An acoustic radiates the bridge force through its body modes, the acceleration of the air it
  moves, and the cancellation below the air resonance.
- **A guitarist.** Notes are fingered where a player would put them (free strings, inside the
  hand's span, open strings in the open position); a second note on a string chokes the first; a
  legato note on the same string is a hammer-on, pull-off or (with glide) slide; a released note
  is damped by the fretting hand over the Release time; with nothing held the hands mute
  sympathetic strings unless the sustain pedal lets them ring; unison voices take different
  strings.

## Presets

Six presets are written for it ("Open-D folk steel" to "Palm-muted chug"), and each of the 27
presets built on the Jangle 12-string has a twin ending in "(phys)" that keeps its effects,
envelope and voice settings and translates the jangle's controls: body 50% or more becomes the
steel dreadnought, fuzzed presets the humbucker, the rest the single-coil; the pick position
becomes the same fraction of the string as a pluck point in mm; brightness becomes pick hardness
(bright or fuzzed presets on the bridge pickup); short decays become palm muting. The guitar has
six strings, so a 12-string's octave courses are played through the chord machine's Octaves
setting: each key sounds its note and the octave on two real strings, as a course does.

## The critic: against real guitars

`tools/guitar_critic.py` renders the same note on the model (`build/fuzzboi-guitar-render`, the
model alone, no effects) for every recording in `assets/samples/guitar/manifest.json` and grades
pitch, inharmonicity, partial envelope, per-partial T60, two-stage decay, beating, attack shape
and high-frequency content, spectral centroid over time and the noise between partials, each
against a tolerance a careful listener would accept, then averages the per-note spectral error by
frequency to separate the instrument's fixed colouring from note-to-note differences.

    python3 -I tools/guitar_critic.py --render build/fuzzboi-guitar-render --out /tmp/critic

The reference set (47 notes, harvested by `tools/harvest_archive.py` and by hand, licences in the
manifest; the audio is not in git) is 22 nylon notes (Yamaha classical with string and fret,
Kyster's open strings, Philharmonia), 17 electric (a Fender bridge pickup straight into a DI with
string and fret, and a birch electric through an unknown clean amp) and 8 steel notes from an
**archtop**: no freely licensed flat-top steel-string recording was found, so the dreadnought is
judged against a different body.

An independent adversarial review (a separate agent told to tear it apart, given the report, the
spectrograms, the renders and the model's source, running its own measurements) judged two
rounds. The first found nine flaws; the changes it prompted fixed the periodic "phaser" notch
sweep from the polarization split (notch timing now scattered, no diagonal in the
spectrograms), the nylon basses dying several times too fast (E2 at 1 s / 2 s: -5.3 / -9.8 dB
against the recording's -5.6 / -12.1), the flat decay across partials on the low steel strings,
and a release click; it judged none of the three instruments mistakable for the real thing yet.
State at the last run (2026-10-12):

| Instrument | Measures within tolerance | Strongest evidence it is right | What still gives it away |
|---|---|---|---|
| Electric (vs a Fender bridge pickup DI) | 67% | partial T60s within about 1.5x on E2, A2 and E4 (E4: 10.4 s fundamental against 10.1 s); stretch B within 10%; colouring flat within about 4 dB from 80 Hz to 2.5 kHz | about 10 dB of extra energy above 3 kHz on the pick attack; wound low strings too bright (partials 3 to 6) and too sustained (E2 fundamental 74 s against 54 s) |
| Steel (vs an archtop) | 66% | per-partial decay on the low strings (E2: 11.1 to 3.5 s against 9.8 to 2.6 s) | the middle strings die about twice too fast (D#3 fundamental 6.5 s against 13.7 s); a pick attack about 10 dB too bright on E2 |
| Nylon (vs three classical guitars) | 45% | bass sustain (above); stretch B | the onset: real nylon notes swell over 30 to 40 ms with almost nothing above 3 kHz, the model's has 10 to 17 dB more; the fretted high treble (A4) is too pure; a slow beat null on D3 |

Pitch passes on every note. The envelope measure fails on most notes and should not be read as
a model failure: it depends on where the real player plucked, which is unknown. The reviewer
also found the critic's own weak spots, some fixed (a high-pass before analysis, loop points,
the attack measure, pitch against the labelled note) and some not: the T60 noise floor is taken
between partials, which on low notes still sits in the partial's leakage; the B fit returns 0 for
the A2 nylon recordings; the "both under 0.5% above 3 kHz" pass can hide 10 dB; and the 80 Hz
band of the frequency table holds only fundamentals, so its value is an artefact.

## Known gaps

- Fret buzz, string-to-fret collision, longitudinal (phantom) partials and finger squeak on
  slides are not modelled.
- The body is a modal filter on the bridge force, not a coupled model: the body's resonances
  shape each note's decay only through the bridge conductance at its partials.
- Slanted pickups (a Strat's bridge pickup) are taken at the treble side's distance.
- Body modes above the measured ones are generated (seeded, at the measured average spacing),
  not measured; the dreadnought has no flat-top recording to check them against.
- The sympathetic coupling is explicit (a sample late, nothing debits the driving string), so it
  is scaled down to stay stable in octave tunings rather than set from the bridge's admittance.
