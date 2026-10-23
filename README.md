# N-DX — Neural Dynamics Engine (VST3)

**Compression that listens.** A hybrid compressor: a deterministic, automatable DSP core steered by a *bounded* neural/intent controller.

> VST is a trademark of Steinberg Media Technologies GmbH, registered in Europe and other countries.
> This project uses the Steinberg VST 3 SDK. Check the licence of the SDK version you clone before distributing binaries.

## Two editions, one engine

| Edition | Folder | GUI | Build |
|---|---|---|---|
| **N-DX (full GUI)** - the one that looks like the artwork | `juce/` | Custom UI: Compression Field, meters, tabs, preset bar, detector / sidechain / spectral / M-S / oversampling / saturation panels | `10_build_juce.bat` (VST3 + Standalone; AU on macOS) |
| **N-DX Generic UI** - reference build straight on the Steinberg VST3 SDK | `src/` | Host's generic parameter editor | `00_RUN_ALL.bat` |

Both wrap the *same* DSP engine (`src/ndx_dsp.h`), parameter table and presets, so they sound identical and share automation names. JUCE itself bundles the Steinberg VST3 SDK for its VST3 wrapper.
JUCE has its own licence terms (GPLv3 or commercial) - check them before you distribute the JUCE edition.

## Build in three steps (Windows 10/11, x64)

Requirements: **Visual Studio 2022 (or Build Tools) with "Desktop development with C++"**, **CMake 3.20+**, **Git**. Python 3 is optional (presets / weights tools).

```
00_RUN_ALL.bat            :: check -> fetch SDK -> DSP tests -> configure -> build -> validate
00_RUN_ALL.bat install    :: ...and install plug-in + presets for the current user
00_RUN_ALL.bat release    :: ...and zip a release into dist\
00_RUN_ALL.bat juce       :: ...and also build the full-GUI JUCE edition
```

Output: `build\VST3\Release\NDX.vst3`

## The .bat system

| Script | What it does |
|---|---|
| `config.bat` | Shared settings (names, paths, SDK repo/ref, auto-detected Visual Studio generator). Override any value via environment variables, e.g. `set NDX_GENERATOR=Visual Studio 17 2022`, `set SDK_REF=<tag>`, `set NDX_CONFIG=Debug`. |
| `01_check_env.bat` | Verifies git, cmake, Visual Studio C++ tools, python. |
| `02_get_sdk.bat` | Clones `steinbergmedia/vst3sdk` (with submodules) into `external\vst3sdk`; optional pin via `SDK_REF`. |
| `03_configure.bat` | CMake configure into `build\` (fetches the SDK first if missing). |
| `04_build.bat` | Builds the `NDX` target, confirms the `.vst3` exists. |
| `05_validate.bat` | Builds and runs Steinberg's `validator` on the plug-in. |
| `06_install.bat [user\|system]` | Copies to `%LOCALAPPDATA%\Programs\Common\VST3` (no admin) or `%CommonProgramFiles%\VST3` (admin). |
| `07_package.bat` | Zips plug-in + presets + README + press kit into `dist\`. |
| `08_test_dsp.bat` | Builds/runs the standalone DSP unit tests — **no SDK needed**. |
| `09_make_presets.bat [install]` | Regenerates the 19 `.vstpreset` files and `src/ndx_presets.h` and optionally installs them to `Documents\VST3 Presets\Lewis Seals\N-DX`. |
| `10_build_juce.bat [install]` | Builds the full-GUI JUCE edition (downloads JUCE on first run). Output: `build_juce\NDXJuce_artefacts\Release\VST3\N-DX.vst3` and `Standalone\N-DX.exe`. |
| `99_clean.bat [all]` | Removes build output (and with `all`, the downloaded SDK). |

## Project layout

```
src/ndx_params.h      parameter table (single source of truth)
src/ndx_dsp.h         DSP engine - header only, SDK-free, unit tested
src/ndx_processor.*   VST3 audio processor (IAudioProcessor)
src/ndx_controller.*  VST3 edit controller (parameters, host generic UI)
src/ndx_entry.cpp     plug-in factory
src/ndx_presets.h     GENERATED factory presets (tools/make_presets.py)
juce/                 JUCE edition: processor, editor, UI kit (Source/NDXUI.h)
resource/ndx.rc       Windows version info
presets/NDX/          19 factory presets (.vstpreset)
tools/                make_presets.py, make_weights.py
tests/                standalone DSP tests
press/                press kit
```

## What the plug-in does (v1.0)

**Signal path:** input → (optional external sidechain) → lookahead delay (fixed 2 ms reported latency; the *Lookahead* knob sets how far ahead the detector sees) → detector (peak/RMS/blend, sidechain HPF, M/S option, stereo link) → gain computer (soft knee) → two-stage ballistics with hold → 3-band LR4 spectral gain (aligned dry path) → ADAA saturation → makeup / parallel mix / output → optional safety ceiling.

**New in this revision (needed by the artwork):** input trim, sidechain HPF/LPF filter (12 dB/oct, detector path only), True-Envelope detector (inter-sample peak), spectral weighting modes (Adaptive / Static / Bass / Vocal / Air Focus), and 2x/4x/8x oversampling of the saturator using linear-phase FIR filters - latency is a constant 32 samples for every setting, so it never changes while you play.

**Modes:** Classic · Adaptive · Spectral · Neural · Hybrid · Character · Master · Restore · Experimental.

- **Scene engine** — soft transient / sustain / silence weights drive program-dependent release ("musical recovery") and an anti-pumping floor.
- **Intent engine** — Punch, Glue, Vocal Forward, Bass Control, Open Master, Dense, Wide, Transient, Smooth, Loud, Invisible. Each biases attack, release, threshold, ratio, knee, spectral weighting, saturation, mix and link.
- **Neural controller** — a small recurrent network (12 features → 16 hidden → 9 modulations). It only outputs values in −1…1; they are scaled by *Neural Amount*, slew-limited and clamped, and the audio path stays deterministic. **Without a weights file the network contributes exactly zero**, so out of the box the controller is the intent/scene prior alone. Drop a trained `ndx_weights.bin` into `%APPDATA%\NDX\` (or point `NDX_WEIGHTS` at it) to add a learned residual. See `tools\make_weights.py` for the format.
- **Character** — continuous morph Clean ↔ Modern ↔ Punch ↔ FET ↔ Opto ↔ VCA ↔ Vari-Mu ↔ Saturated (detector law, knee, ratio, ballistics, drive and bias).
- **Restore** — transient restorer for over-compressed material (boosts peaks that stick out of the slow envelope). It is a musically plausible reconstruction, not true de-compression.
- Gain-reduction meter is exposed as a read-only host parameter.

## The JUCE GUI

Built to the artwork. Everything on screen is bound to a real parameter or live engine data:

- **Compression Field** - live spectrogram of the input (FFT, log frequency, time scrolling upward, same colour ramp as the artwork) with the per-band gain-reduction curve on top. Drag the two handles to move the Low / High crossovers; double-click a handle to reset it.
- **Input / Output strips** - L/R peak meters with peak hold and numeric readout, Trim and Makeup knobs.
- **Tabs** - Dynamics, Spectral, Neural (Neural Amount, Intent, Auto Threshold, Auto Makeup, Restore, **Analyze**), Character, Master. Double-click a knob to reset it, double-click a value to type one in.
- **Preset bar** - 19 factory presets, previous/next, user presets (Save writes `%APPDATA%\NDX\Presets\*.ndxpreset`), plus a presets menu (reset, delete, reveal folder).
- **Panels** - Detector (Peak / RMS / True Env with live envelope scope), Sidechain (filter toggle, HPF/LPF with live response graph), Spectral Dynamics (live band graph + weighting menu), M/S (switch + stereo link), Oversampling (Off/2x/4x/8x), Saturation (drive knob + transfer scope).
- **Header** - Mode selector, Bypass, user menu, settings menu (interface size 75-150 %, load neural weights file, about).
- **Analyze** - listens to the incoming audio for 3 seconds and sets Threshold, Ratio and Attack from the measured level spread. It is a heuristic suggestion, not a trained model.
- The window is resizable with a fixed aspect ratio and renders from a fixed 900 x 680 layout, so it looks the same at any size or DPI.

## Honest status

- Verified here: the DSP engine (now including the oversampler, sidechain filters and True-Envelope detector) compiles warning-free with `g++ -Wall -Wextra` and passes the unit tests (compression amount, flat band-sum, bypass latency, finite/bounded output across every mode × intent, weights loader).
- **Not verified here:** the JUCE edition (`juce/`) has **never been compiled or run** - no network in the authoring sandbox, so JUCE was not available. It is written against the JUCE 8.0.8 API; expect to fix a few compile errors or layout details on the first build and tell me what they are. The VST3 wrapper (`ndx_processor/controller/entry`, `CMakeLists.txt`) was written against the SDK API from knowledge but *not compiled* (no network in the authoring sandbox). Expect to possibly fix a small SDK-version detail on first build; `05_validate.bat` is your check. For SDK older than 3.7 configure with `-DNDX_LEGACY_INITMODULE=ON`.
- Not in v1.0: AAX (needs Avid's SDK and licence), a tested AU build, a PyTorch training pipeline, a true per-frequency compression field (the field shows the input spectrum plus three-band gain reduction, not a dynamic-EQ surface), learned-from-user-actions personal model. The artwork's "state-space memory" is represented by the recurrent neural controller, learned-from-user-actions personal model.

## Troubleshooting

- *"VST3 SDK not found"* → run `02_get_sdk.bat`.
- *Generator error* → `set NDX_GENERATOR=Visual Studio 17 2022` (or 16 2019) and rerun `03_configure.bat`.
- *Duplicate symbol InitModule* → your SDK provides it; leave `NDX_LEGACY_INITMODULE` off.
- *Undefined InitModule* → SDK older than 3.7; turn the option on.
- *Copy failed on install* → use `06_install.bat user`, or an Administrator prompt for the system folder.
- If you fork: generate new class IDs in `src/ndx_ids.h` and mirror them in `tools/make_presets.py`.
