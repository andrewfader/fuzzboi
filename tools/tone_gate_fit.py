"""Dials quality-gate presets in against the records, as a player dials an amplifier in against a
record: coordinate descent on the knobs the rig really has (the instrument, fuzz type and cabinet
type are left as the preset chose them), minimising tests/tone_gate.hpp's loss. Prints the
settings it settles on; writing them into src/presets.hpp is left to a person. A tuning tool.

    python3 -I tools/tone_gate_fit.py build-tune/fuzzboi-tone-gate-try "Preset (phys)" [knob-set [previous.log]]

A previous run's log continues from the settings it settled on.
"""
import ast
import re
import subprocess
import sys

KNOBS = {
    # name: (low, high, first step, discrete values or None)
    'Pick': (0.05, 1.0, 0.2, None),
    'Pluck point': (30, 200, 30, None),
    'Pickup': (0, 3, None, [0, 1, 2, 3]),
    'Fuzz': (0.0, 1.0, 0.2, None),
    'Fuzz tone': (0.0, 1.0, 0.2, None),
    'Cabinet': (0.0, 1.0, 0.25, None),
    'Mic distance': (0.05, 3.0, 0.5, None),
    'Mic pattern': (0, 2, None, [0, 1, 2]),
    'Room': (0, 6, None, [0, 1, 2, 3, 4, 5, 6]),
    'Absorption': (0.05, 0.8, 0.15, None),
    'Double track': (0.0, 1.0, 0.3, None),
    'Bandwidth': (3000, 16000, 3000, None),
    'Tape drive': (0.0, 0.8, 0.2, None),
    'Chorus mix': (0.0, 0.6, 0.15, None),
    'Reverb mix': (0.0, 0.5, 0.15, None),
    'Noise floor': (-100, -40, 10, None),
}
SETS = {
    'amp': ['Noise floor', 'Pick', 'Pluck point', 'Pickup', 'Fuzz', 'Fuzz tone', 'Cabinet', 'Mic distance', 'Mic pattern', 'Room', 'Absorption', 'Double track'],
    'clean': ['Noise floor', 'Pick', 'Pluck point', 'Pickup', 'Cabinet', 'Mic distance', 'Mic pattern', 'Room', 'Absorption', 'Double track', 'Chorus mix'],
    'acoustic': ['Noise floor', 'Pick', 'Pluck point', 'Mic distance', 'Mic pattern', 'Room', 'Absorption', 'Double track'],
    'tape': ['Noise floor', 'Pick', 'Pluck point', 'Pickup', 'Fuzz', 'Fuzz tone', 'Cabinet', 'Mic distance', 'Mic pattern', 'Room', 'Absorption', 'Double track', 'Bandwidth', 'Tape drive'],
}


def evaluate(tool, preset, settings):
    # -30 dB keeps the output limiter out of it, as the presets' own trims to -16 LUFS do.
    args = [tool, preset, 'Master=-30'] + [f'{k}={v:g}' for k, v in settings.items()]
    out = subprocess.run(args, capture_output=True, text=True).stdout
    m = re.search(r'LOSS ([0-9.]+)', out)
    return (float(m.group(1)) if m else 1e9), out


def main():
    tool, preset = sys.argv[1], sys.argv[2]
    knobs = SETS[sys.argv[3] if len(sys.argv) > 3 else 'amp']
    settings = {}
    if len(sys.argv) > 4:
        for line in open(sys.argv[4]):
            if line.startswith('SETTINGS '):
                settings = ast.literal_eval(line[len('SETTINGS '):])
    best, report = evaluate(tool, preset, settings)
    steps = {k: KNOBS[k][2] for k in knobs}
    print(f'start {best:.3f}', flush=True)
    for round_ in range(3):
        for k in knobs:
            lo, hi, _, options = KNOBS[k]
            if options is not None:
                candidates = options
            else:
                current = settings.get(k)
                if current is None:
                    # Read the preset's value from the first evaluation is not possible; probe the middle.
                    candidates = [lo + (hi - lo) * f for f in (0.15, 0.4, 0.65, 0.9)] if round_ == 0 else []
                else:
                    candidates = [min(hi, max(lo, current + d)) for d in (-steps[k], steps[k])]
            for value in candidates:
                trial = dict(settings, **{k: value})
                loss, out = evaluate(tool, preset, trial)
                if loss < best - 1e-4:
                    best, report, settings = loss, out, trial
            if options is None and k in steps:
                steps[k] *= 0.5
        print(f'round {round_ + 1}: {best:.3f} {settings}', flush=True)
    print(report)
    print('SETTINGS', settings)


if __name__ == '__main__':
    main()
