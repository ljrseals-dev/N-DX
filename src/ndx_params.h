// N-DX parameter table - single source of truth shared by DSP engine, processor and controller.
// No SDK dependency (so the DSP can be unit tested standalone).
#pragma once
#include <algorithm>
#include <cmath>

namespace ndx {

enum ParamId : unsigned int {
    kBypass = 0, kMode, kIntent, kInputTrim,
    kThreshold, kRatio, kKnee, kAttack, kRelease, kHold, kLookahead,
    kMakeup, kAutoMakeup, kAutoThreshold, kMix,
    kStereoMode, kLink, kSidechain, kScFilter, kScHpf, kScLpf, kDetector,
    kSpectral, kSpecWeight, kXoverLow, kXoverHigh, kLowProtect,
    kCharacter, kSaturation, kOversample, kNeural, kRestore,
    kSafety, kCeiling, kOutput,
    kGainReduction,          // read-only meter (processor -> host)
    kNumParams
};

enum ModeId { kModeClassic = 0, kModeAdaptive, kModeSpectral, kModeNeural, kModeHybrid,
              kModeCharacter, kModeMaster, kModeRestore, kModeExperimental };

inline const char* const kOffOnItems[]  = {"Off", "On"};
inline const char* const kStereoItems[] = {"Stereo", "Mid/Side"};
inline const char* const kScItems[]     = {"Internal", "External"};
inline const char* const kDetItems[]    = {"Peak", "RMS", "True Env"};
inline const char* const kWeightItems[] = {"Adaptive", "Static", "Bass Focus", "Vocal Focus", "Air Focus"};
inline const char* const kOsItems[]     = {"Off", "2x", "4x", "8x"};
inline const char* const kModeItems[]   = {"Classic", "Adaptive", "Spectral", "Neural", "Hybrid",
                                           "Character", "Master", "Restore", "Experimental"};
inline const char* const kIntentItems[] = {"Manual", "Punch", "Glue", "Vocal Forward", "Bass Control",
                                           "Open Master", "Dense", "Wide", "Transient", "Smooth",
                                           "Loud", "Invisible"};

struct ParamSpec {
    unsigned id;
    const char* name;
    const char* units;
    double mn, mx, def;
    double skew;            // plain = mn + (mx-mn) * norm^skew
    int precision;
    const char* const* items;
    int numItems;
};

inline const ParamSpec kSpecs[kNumParams] = {
    {kBypass,        "Bypass",             "",    0,    1,    0,    1, 0, kOffOnItems,  2},
    {kMode,          "Mode",               "",    0,    8,    4,    1, 0, kModeItems,   9},
    {kIntent,        "Intent",             "",    0,   11,    0,    1, 0, kIntentItems, 12},
    {kInputTrim,     "Input Trim",         "dB", -24,  24,    0,    1, 1, nullptr, 0},
    {kThreshold,     "Threshold",          "dB", -60,   0,  -18,    1, 1, nullptr, 0},
    {kRatio,         "Ratio",              ":1",  1,   20,    4,    2, 1, nullptr, 0},
    {kKnee,          "Knee",               "dB",  0,   24,    6,    1, 1, nullptr, 0},
    {kAttack,        "Attack",             "ms",  0.05, 100, 10,    3, 2, nullptr, 0},
    {kRelease,       "Release",            "ms",  5,  2000, 120,    3, 0, nullptr, 0},
    {kHold,          "Hold",               "ms",  0,   500,   0,    2, 0, nullptr, 0},
    {kLookahead,     "Lookahead",          "ms",  0,     2,   0,    1, 2, nullptr, 0},
    {kMakeup,        "Makeup",             "dB", -12,  24,    0,    1, 1, nullptr, 0},
    {kAutoMakeup,    "Auto Makeup",        "",    0,    1,    0,    1, 0, kOffOnItems,  2},
    {kAutoThreshold, "Auto Threshold",     "",    0,    1,    0,    1, 0, kOffOnItems,  2},
    {kMix,           "Mix",                "%",   0,  100,  100,    1, 0, nullptr, 0},
    {kStereoMode,    "Stereo Mode",        "",    0,    1,    0,    1, 0, kStereoItems, 2},
    {kLink,          "Stereo Link",        "%",   0,  100,  100,    1, 0, nullptr, 0},
    {kSidechain,     "Sidechain",          "",    0,    1,    0,    1, 0, kScItems,     2},
    {kScFilter,      "Sidechain Filter",   "",    0,    1,    0,    1, 0, kOffOnItems,  2},
    {kScHpf,         "Sidechain HPF",      "Hz", 20,  500,   80,    3, 0, nullptr, 0},
    {kScLpf,         "Sidechain LPF",      "Hz", 1000, 20000, 12000, 1, 0, nullptr, 0},
    {kDetector,      "Detector",           "",    0,    2,    0,    1, 0, kDetItems,    3},
    {kSpectral,      "Spectral",           "%",   0,  100,   50,    1, 0, nullptr, 0},
    {kSpecWeight,    "Spectral Weighting", "",    0,    4,    0,    1, 0, kWeightItems, 5},
    {kXoverLow,      "Xover Low",          "Hz", 60,  400,  150,    2, 0, nullptr, 0},
    {kXoverHigh,     "Xover High",         "Hz", 1500, 8000, 3500,  2, 0, nullptr, 0},
    {kLowProtect,    "Low Protect",        "%",   0,  100,   30,    1, 0, nullptr, 0},
    {kCharacter,     "Character",          "%",   0,  100,    0,    1, 0, nullptr, 0},
    {kSaturation,    "Saturation",         "%",   0,  100,    0,    1, 0, nullptr, 0},
    {kOversample,    "Oversampling",       "",    0,    3,    0,    1, 0, kOsItems,     4},
    {kNeural,        "Neural Amount",      "%",   0,  100,   60,    1, 0, nullptr, 0},
    {kRestore,       "Restore Amount",     "%",   0,  100,   50,    1, 0, nullptr, 0},
    {kSafety,        "Safety Ceiling",     "",    0,    1,    0,    1, 0, kOffOnItems,  2},
    {kCeiling,       "Ceiling",            "dB", -12,   0, -0.3,    1, 1, nullptr, 0},
    {kOutput,        "Output",             "dB", -24,  12,    0,    1, 1, nullptr, 0},
    {kGainReduction, "Gain Reduction",     "dB",  0,   30,    0,    1, 1, nullptr, 0},
};

inline double plainFromNorm(const ParamSpec& s, double n) {
    n = n < 0 ? 0 : (n > 1 ? 1 : n);
    double v = s.mn + (s.mx - s.mn) * std::pow(n, s.skew);
    if (s.items) v = std::floor(v + 0.5);
    return v;
}

inline double normFromPlain(const ParamSpec& s, double v) {
    double t = (v - s.mn) / (s.mx - s.mn);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return std::pow(t, 1.0 / s.skew);
}

}  // namespace ndx
