#!/usr/bin/env python3
"""Generate N-DX .vstpreset files (VST3 preset container, no SDK needed).

The 'Comp' chunk holds exactly what NDXProcessor::getState() writes:
    int32 version(=2), then kNumParams little-endian doubles of NORMALIZED values.

SPEC below must stay in sync with src/ndx_params.h (name, min, max, default, skew, numItems).
Class ID must match kNDXProcessorUID in src/ndx_ids.h.

Usage:  python tools/make_presets.py [output_dir]
"""
import os
import struct
import sys

CLASS_ID = "%08X%08X%08X%08X" % (0x4E445850, 0x7A1C4B3E, 0x9F0D2C58, 0x1B6E8A37)

# name, min, max, default, skew, numItems(0 = continuous)
SPEC = [
    ("Bypass", 0, 1, 0, 1, 2), ("Mode", 0, 8, 4, 1, 9), ("Intent", 0, 11, 0, 1, 12),
    ("InputTrim", -24, 24, 0, 1, 0),
    ("Threshold", -60, 0, -18, 1, 0), ("Ratio", 1, 20, 4, 2, 0), ("Knee", 0, 24, 6, 1, 0),
    ("Attack", 0.05, 100, 10, 3, 0), ("Release", 5, 2000, 120, 3, 0), ("Hold", 0, 500, 0, 2, 0),
    ("Lookahead", 0, 2, 0, 1, 0), ("Makeup", -12, 24, 0, 1, 0), ("AutoMakeup", 0, 1, 0, 1, 2),
    ("AutoThreshold", 0, 1, 0, 1, 2), ("Mix", 0, 100, 100, 1, 0), ("StereoMode", 0, 1, 0, 1, 2),
    ("Link", 0, 100, 100, 1, 0), ("Sidechain", 0, 1, 0, 1, 2), ("ScFilter", 0, 1, 0, 1, 2),
    ("ScHpf", 20, 500, 80, 3, 0), ("ScLpf", 1000, 20000, 12000, 1, 0), ("Detector", 0, 2, 0, 1, 3),
    ("Spectral", 0, 100, 50, 1, 0), ("SpecWeight", 0, 4, 0, 1, 5), ("XoverLow", 60, 400, 150, 2, 0),
    ("XoverHigh", 1500, 8000, 3500, 2, 0), ("LowProtect", 0, 100, 30, 1, 0), ("Character", 0, 100, 0, 1, 0),
    ("Saturation", 0, 100, 0, 1, 0), ("Oversample", 0, 3, 0, 1, 4), ("Neural", 0, 100, 60, 1, 0),
    ("Restore", 0, 100, 50, 1, 0), ("Safety", 0, 1, 0, 1, 2), ("Ceiling", -12, 0, -0.3, 1, 0),
    ("Output", -24, 12, 0, 1, 0), ("GainReduction", 0, 30, 0, 1, 0),
]
NAMES = [s[0] for s in SPEC]

PRESETS = {
    "00 Init (Hybrid)": {},
    "01 Modern Punch": dict(Mode=4, Intent=1, Threshold=-24, Ratio=3.5, Knee=8, Attack=12, Release=120, Lookahead=2, Spectral=55, Saturation=15, ScFilter=1, ScHpf=80, ScLpf=12000, Neural=70),
    "02 Classic Precision": dict(Mode=0, Threshold=-18, Ratio=4, Knee=6, Attack=10, Release=120),
    "03 Punch Drums": dict(Mode=4, Intent=1, Threshold=-20, Ratio=4, Knee=4, Attack=15, Release=90, Spectral=40, Neural=70, Detector=2),
    "04 Glue Bus": dict(Mode=4, Intent=2, Threshold=-16, Ratio=2, Knee=12, Attack=25, Release=200, AutoMakeup=1, Detector=1),
    "05 Vocal Forward": dict(Mode=4, Intent=3, Threshold=-22, Ratio=3.5, Attack=6, Release=110, Spectral=55, SpecWeight=3, Saturation=10, ScFilter=1, ScHpf=120),
    "06 Bass Control": dict(Mode=4, Intent=4, Threshold=-24, Ratio=5, Attack=20, Release=140, Spectral=80, SpecWeight=2, LowProtect=10, XoverLow=120),
    "07 Open Master": dict(Mode=6, Intent=5, Threshold=-14, Ratio=1.8, Knee=14, Attack=30, Release=250, AutoMakeup=1, Safety=1, Ceiling=-0.3, Oversample=1),
    "08 Dense Parallel": dict(Mode=4, Intent=6, Threshold=-30, Ratio=8, Attack=3, Release=60, Mix=40, Saturation=25, Oversample=2),
    "09 Wide Mid-Side": dict(Mode=4, Intent=7, StereoMode=1, Link=60, Threshold=-20, Ratio=3),
    "10 Transient Snap": dict(Mode=4, Intent=8, Threshold=-18, Ratio=3, Attack=30, Release=60),
    "11 Smooth Leveler": dict(Mode=4, Intent=9, Threshold=-26, Ratio=2.5, Knee=18, Attack=12, Release=400, Detector=1),
    "12 Loud": dict(Mode=4, Intent=10, Threshold=-28, Ratio=6, Attack=4, Release=80, Saturation=30, AutoMakeup=1, Safety=1, Oversample=2),
    "13 Invisible": dict(Mode=4, Intent=11, Threshold=-20, Ratio=2, Knee=18, Attack=20, Release=300),
    "14 Air Lift Compressor": dict(Mode=2, Spectral=70, SpecWeight=4, Threshold=-26, Ratio=2.5, Attack=8, Release=140),
    "15 FET Character": dict(Mode=5, Character=43, Threshold=-20, Ratio=4, Attack=10, Release=120, Oversample=1),
    "16 Opto Character": dict(Mode=5, Character=57, Threshold=-22, Ratio=3, Attack=10, Release=150),
    "17 Vari-Mu Character": dict(Mode=5, Character=86, Threshold=-20, Ratio=2, Attack=10, Release=200, AutoMakeup=1, Oversample=1),
    "18 Restore Over-Compressed": dict(Mode=7, Restore=70, Mix=100),
}


def to_norm(spec, plain):
    _, mn, mx, _, skew, _ = spec
    t = (plain - mn) / (mx - mn)
    t = min(1.0, max(0.0, t))
    return t ** (1.0 / skew)


def comp_chunk(overrides):
    vals = []
    for spec in SPEC:
        name, default = spec[0], spec[3]
        vals.append(to_norm(spec, overrides.get(name, default)))
    return struct.pack("<i", 2) + b"".join(struct.pack("<d", v) for v in vals)


def write_vstpreset(path, overrides):
    comp = comp_chunk(overrides)
    data_offset = 48  # 'VST3'(4) + version(4) + classID(32) + listOffset(8)
    list_offset = data_offset + len(comp)
    out = b"VST3" + struct.pack("<i", 1) + CLASS_ID.encode("ascii") + struct.pack("<q", list_offset)
    out += comp
    out += b"List" + struct.pack("<i", 1) + b"Comp" + struct.pack("<qq", data_offset, len(comp))
    with open(path, "wb") as f:
        f.write(out)


def write_header(path):
    """Emit src/ndx_presets.h (plain parameter values by id) for the JUCE build."""
    lines = ["// GENERATED by tools/make_presets.py - do not edit.", "#pragma once",
             '#include "ndx_params.h"', "namespace ndx {",
             "struct PresetValue { unsigned id; double value; };",
             "struct PresetDef { const char* name; const PresetValue* values; int count; };",
             "static_assert(kNumParams == %d, \"parameter table changed - rerun tools/make_presets.py\");" % len(SPEC)]
    for i, (name, ov) in enumerate(PRESETS.items()):
        vals = ", ".join("{%d, %r}" % (NAMES.index(k), float(v)) for k, v in ov.items())
        lines.append("inline const PresetValue kPresetValues%d[] = {%s};" % (i, vals if vals else "{0, 0.0}"))
    lines.append("inline const PresetDef kFactoryPresets[] = {")
    for i, (name, ov) in enumerate(PRESETS.items()):
        label = name.split(" ", 1)[1]
        lines.append('    {"%s", kPresetValues%d, %d},' % (label, i, len(ov)))
    lines.append("};")
    lines.append("inline constexpr int kNumFactoryPresets = %d;" % len(PRESETS))
    lines.append("}  // namespace ndx")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "presets", "NDX")
    os.makedirs(out_dir, exist_ok=True)
    assert len(SPEC) == 36 and len(NAMES) == 36
    for name, ov in PRESETS.items():
        unknown = set(ov) - set(NAMES)
        assert not unknown, (name, unknown)
        write_vstpreset(os.path.join(out_dir, name + ".vstpreset"), ov)
    write_header(os.path.join(here, "..", "src", "ndx_presets.h"))
    print("wrote %d presets to %s" % (len(PRESETS), os.path.abspath(out_dir)))


if __name__ == "__main__":
    main()
