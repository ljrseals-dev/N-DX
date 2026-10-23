#!/usr/bin/env python3
"""Create / convert an N-DX neural-controller weights file (ndx_weights.bin).

File format (little-endian):
    'NDXW' | int32 version=1 | int32 nIn=12 | int32 nHidden=16 | int32 nOut=9
    float32 W1[16*12] | Wh[16*16] | b1[16] | W2[9*16] | b2[9]
Network (Elman RNN, evaluated every 32 samples):
    h = tanh(W1 x + Wh h + b1);   out = tanh(W2 h + b2)
Inputs x (12): level, crest, transient density, low/mid/high band ratios, stereo correlation,
               current GR, average GR, scene transient, scene sustain, scene silence.
Outputs (9, each -1..1): attack, release, threshold, ratio, knee, spectral, saturation, mix, link.
The plug-in scales these by Neural Amount, slew-limits and clamps them - the network can never
push a parameter outside its safe range.

Install location (first match wins): $NDX_WEIGHTS, %APPDATA%\\NDX\\ndx_weights.bin, ~/.config/ndx/ndx_weights.bin

Usage:
    python make_weights.py --zero  out.bin                  # explicit "no residual" file
    python make_weights.py --random --seed 3 --scale 0.2 out.bin
    python make_weights.py --npz trained.npz out.bin        # keys: W1 Wh b1 W2 b2 (needs numpy)
"""
import argparse
import random
import struct

NIN, NH, NOUT = 12, 16, 9
SHAPES = {"W1": (NH, NIN), "Wh": (NH, NH), "b1": (NH,), "W2": (NOUT, NH), "b2": (NOUT,)}


def count(shape):
    n = 1
    for d in shape:
        n *= d
    return n


def write(path, arrays):
    with open(path, "wb") as f:
        f.write(b"NDXW" + struct.pack("<iiii", 1, NIN, NH, NOUT))
        for key in ("W1", "Wh", "b1", "W2", "b2"):
            vals = arrays[key]
            assert len(vals) == count(SHAPES[key]), key
            f.write(struct.pack("<%df" % len(vals), *vals))
    print("wrote", path)


def main():
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--zero", action="store_true")
    g.add_argument("--random", action="store_true")
    g.add_argument("--npz")
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--scale", type=float, default=0.2)
    ap.add_argument("out")
    a = ap.parse_args()

    if a.zero:
        arrays = {k: [0.0] * count(s) for k, s in SHAPES.items()}
    elif a.random:
        rnd = random.Random(a.seed)
        arrays = {k: [rnd.uniform(-a.scale, a.scale) for _ in range(count(s))] for k, s in SHAPES.items()}
    else:
        import numpy as np
        z = np.load(a.npz)
        arrays = {}
        for k, s in SHAPES.items():
            arr = np.asarray(z[k], dtype="float32")
            assert arr.shape == s, "%s: expected %s got %s" % (k, s, arr.shape)
            arrays[k] = arr.reshape(-1).tolist()
    write(a.out, arrays)


if __name__ == "__main__":
    main()
