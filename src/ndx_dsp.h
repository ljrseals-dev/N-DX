// N-DX Neural Dynamics Engine - DSP core (header only, no SDK dependency).
//
// Audio path is deterministic. The "neural controller" only produces BOUNDED
// modulation values (-1..1) that are scaled, slew-limited and clamped before
// they touch the DSP (see updateControl()).
#pragma once
#include "ndx_params.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#define NDX_HAS_SSE 1
#endif

namespace ndx {

constexpr double kPi = 3.14159265358979323846;

inline double clampd(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline double lerpd(double a, double b, double t) { return a + (b - a) * t; }
inline double dbToLin(double db) { return std::pow(10.0, db * 0.05); }
inline double linToDb(double x) { return 20.0 * std::log10(x < 1e-9 ? 1e-9 : x); }
inline double smooth01(double a, double b, double x) {
    double t = clampd((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
inline double coefFromMs(double ms, double sr) {
    return 1.0 - std::exp(-1.0 / (std::max(ms, 0.001) * 0.001 * sr));
}

struct NoDenorm {
#ifdef NDX_HAS_SSE
    unsigned old;
    NoDenorm() : old(_mm_getcsr()) { _mm_setcsr(old | 0x8040u); }
    ~NoDenorm() { _mm_setcsr(old); }
#endif
};

// ---------------------------------------------------------------- filters
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline double process(double x) {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
    void setButter(double f, double sr, bool hp) {
        f = clampd(f, 10.0, sr * 0.45);
        const double w0 = 2.0 * kPi * f / sr, c = std::cos(w0);
        const double al = std::sin(w0) / (2.0 * 0.70710678118654752);
        const double a0 = 1.0 + al;
        if (!hp) { b0 = (1 - c) * 0.5 / a0; b1 = (1 - c) / a0; b2 = b0; }
        else     { b0 = (1 + c) * 0.5 / a0; b1 = -(1 + c) / a0; b2 = b0; }
        a1 = -2.0 * c / a0;
        a2 = (1.0 - al) / a0;
    }
};

struct LR4 {  // Linkwitz-Riley 4th order = two cascaded Butterworth biquads
    Biquad a, b;
    void setLP(double f, double sr) { a.setButter(f, sr, false); b.setButter(f, sr, false); }
    void setHP(double f, double sr) { a.setButter(f, sr, true);  b.setButter(f, sr, true); }
    inline double process(double x) { return b.process(a.process(x)); }
    void reset() { a.reset(); b.reset(); }
};

struct Split3 {  // 3-band LR4 split. compensate=true keeps the band sum magnitude-flat.
    LR4 lpL, hpL, lpH, hpH, apLo, apHi;
    void set(double fl, double fh, double sr) {
        lpL.setLP(fl, sr); hpL.setHP(fl, sr);
        lpH.setLP(fh, sr); hpH.setHP(fh, sr);
        apLo.setLP(fh, sr); apHi.setHP(fh, sr);
    }
    inline void process(double x, double* o, bool compensate) {
        const double lo = lpL.process(x);
        const double rest = hpL.process(x);
        o[1] = lpH.process(rest);
        o[2] = hpH.process(rest);
        o[0] = compensate ? apLo.process(lo) + apHi.process(lo) : lo;
    }
    void reset() { lpL.reset(); hpL.reset(); lpH.reset(); hpH.reset(); apLo.reset(); apHi.reset(); }
};

struct Delay {
    std::vector<float> buf;
    int mask = 0, w = 0;
    void init(int minSize) {
        int s = 1;
        while (s < minSize) s <<= 1;
        buf.assign((size_t)s, 0.0f);
        mask = s - 1;
        w = 0;
    }
    inline void push(float x) { buf[(size_t)w] = x; w = (w + 1) & mask; }
    inline float tap(int d) const { return buf[(size_t)((w - 1 - d) & mask)]; }  // tap(0) = newest
};

// Two-stage (fast/slow release) gain-reduction smoother, in dB domain.
struct Ball {
    double fast = 0, slow = 0;
    int holdF = 0, holdS = 0;
    inline double process(double t, double atk, double relF, double relS, int holdN, double w) {
        if (t > fast) { fast += atk * (t - fast); holdF = holdN; }
        else if (holdF > 0) --holdF;
        else fast += relF * (t - fast);
        if (t > slow) { slow += atk * (t - slow); holdS = holdN; }
        else if (holdS > 0) --holdS;
        else slow += relS * (t - slow);
        return fast + w * (slow - fast);
    }
    void reset() { fast = slow = 0; holdF = holdS = 0; }
};

// First-order antiderivative-antialiased tanh saturator (no oversampling needed).
struct Sat {
    double xp = 0, Fp = 0;
    static double F(double x) {  // log(cosh(x)), overflow-safe
        const double a = std::fabs(x);
        return a + std::log1p(std::exp(-2.0 * a)) - 0.69314718055994531;
    }
    inline double process(double x, double g, double bias) {
        const double u = g * x + bias;
        const double d = u - xp;
        double y;
        const double Fu = F(u);
        if (std::fabs(d) < 1e-5) y = std::tanh(0.5 * (u + xp));
        else y = (Fu - Fp) / d;
        xp = u; Fp = Fu;
        return (y - std::tanh(bias)) / g;
    }
    void reset() { xp = 0; Fp = 0; }
};


// ---------------------------------------------------------------- oversampling (for the saturator)
// Linear-phase windowed-sinc up/down pair. Total latency is a CONSTANT kOsLat base samples for
// every factor (and for "Off"), so the plug-in latency never changes when the user switches.
constexpr int kOsLat = 32;

inline double besselI0(double x) {
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 60; ++k) {
        const double h = x / (2.0 * k);
        term *= h * h;
        sum += term;
        if (term < 1e-14 * sum) break;
    }
    return sum;
}

inline std::vector<double> designOsKernel(int F) {  // unity-DC lowpass, N = kOsLat*F + 1 taps
    const int N = kOsLat * F + 1;
    std::vector<double> h((size_t)N);
    const double fc = 0.478 / F, beta = 7.86;  // ~80 dB stopband
    double sum = 0.0;
    for (int k = 0; k < N; ++k) {
        const double m = k - (N - 1) * 0.5;
        const double sinc = std::fabs(m) < 1e-12 ? 2.0 * fc : std::sin(2.0 * kPi * fc * m) / (kPi * m);
        const double r = 2.0 * k / (N - 1) - 1.0;
        h[(size_t)k] = sinc * besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / besselI0(beta);
        sum += h[(size_t)k];
    }
    for (double& v : h) v /= sum;
    return h;
}

struct OsState {
    double xh[64] = {0};
    double sh[512] = {0};
    int xw = 0, sw = 0;
    void reset() { for (double& v : xh) v = 0; for (double& v : sh) v = 0; xw = sw = 0; }
};

// Process one base-rate sample through nonlinear fn() at F-times the rate. No allocation.
template <class Fn>
inline double osProcess(OsState& s, const std::vector<double>& h, int F, double x, Fn fn) {
    const int N = (int)h.size();
    s.xh[s.xw & 63] = x;
    double out = 0.0;
    for (int p = 0; p < F; ++p) {
        double acc = 0.0;
        for (int j = 0, k = p; k < N; ++j, k += F) acc += h[(size_t)k] * s.xh[(s.xw - j) & 63];
        s.sh[s.sw & 511] = fn(acc * F);
        if (p == 0) {  // decimate at the base-rate phase
            double o = 0.0;
            for (int k = 0; k < N; ++k) o += h[(size_t)k] * s.sh[(s.sw - k) & 511];
            out = o;
        }
        ++s.sw;
    }
    ++s.xw;
    return out;
}

// ---------------------------------------------------------------- neural controller
// Small Elman RNN producing 9 bounded modulation values from 12 features.
// Weights are optional (ndx_weights.bin). Without them the residual is exactly zero
// and the controller is the deterministic Intent/scene prior only.
constexpr int kNumMods = 9;  // attack, release, threshold, ratio, knee, spectral, saturation, mix, link

struct NeuralController {
    static constexpr int NIN = 12, H = 16, NOUT = kNumMods;
    std::vector<float> W1, Wh, b1, W2, b2;
    bool loaded = false;
    double h[H] = {0};

    bool loadFile(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        char magic[4];
        int32_t ver = 0, nin = 0, nh = 0, nout = 0;
        f.read(magic, 4);
        f.read(reinterpret_cast<char*>(&ver), 4);
        f.read(reinterpret_cast<char*>(&nin), 4);
        f.read(reinterpret_cast<char*>(&nh), 4);
        f.read(reinterpret_cast<char*>(&nout), 4);
        if (!f || magic[0] != 'N' || magic[1] != 'D' || magic[2] != 'X' || magic[3] != 'W') return false;
        if (ver != 1 || nin != NIN || nh != H || nout != NOUT) return false;
        std::vector<float> a((size_t)(H * NIN)), b((size_t)(H * H)), c((size_t)H), d((size_t)(NOUT * H)), e((size_t)NOUT);
        auto rd = [&f](std::vector<float>& v) { f.read(reinterpret_cast<char*>(v.data()), (std::streamsize)(v.size() * 4)); };
        rd(a); rd(b); rd(c); rd(d); rd(e);
        if (!f) return false;
        for (float v : a) if (!std::isfinite(v)) return false;
        for (float v : b) if (!std::isfinite(v)) return false;
        for (float v : d) if (!std::isfinite(v)) return false;
        W1 = a; Wh = b; b1 = c; W2 = d; b2 = e;
        loaded = true;
        return true;
    }
    void reset() { for (double& v : h) v = 0; }
    void step(const double* x, double* out) {
        if (!loaded) { for (int k = 0; k < NOUT; ++k) out[k] = 0; return; }
        double nh[H];
        for (int i = 0; i < H; ++i) {
            double s = b1[(size_t)i];
            for (int j = 0; j < NIN; ++j) s += W1[(size_t)(i * NIN + j)] * x[j];
            for (int j = 0; j < H; ++j) s += Wh[(size_t)(i * H + j)] * h[j];
            nh[i] = std::tanh(s);
        }
        for (int i = 0; i < H; ++i) h[i] = nh[i];
        for (int k = 0; k < NOUT; ++k) {
            double s = b2[(size_t)k];
            for (int i = 0; i < H; ++i) s += W2[(size_t)(k * H + i)] * h[i];
            out[k] = std::tanh(s);
        }
    }
};

inline std::string defaultWeightsPath() {
    if (const char* e = std::getenv("NDX_WEIGHTS")) return e;
#ifdef _WIN32
    if (const char* a = std::getenv("APPDATA")) return std::string(a) + "\\NDX\\ndx_weights.bin";
#else
    if (const char* hm = std::getenv("HOME")) return std::string(hm) + "/.config/ndx/ndx_weights.bin";
#endif
    return "";
}

// ---------------------------------------------------------------- intent + character tables
// order: attack, release, threshold, ratio, knee, spectral, saturation, mix, link
inline const double kIntentTable[12][kNumMods] = {
    { 0.0,  0.0,  0.0,  0.0,  0.0,  0.0,  0.0, 0.0,  0.0},  // Manual
    { 0.7, -0.3, -0.2,  0.1, -0.3,  0.0,  0.1, 0.0,  0.0},  // Punch
    { 0.3,  0.3, -0.3, -0.4,  0.5,  0.0,  0.2, 0.0,  0.0},  // Glue
    {-0.2, -0.1, -0.4,  0.2,  0.3,  0.5,  0.1, 0.0,  0.0},  // Vocal Forward
    {-0.4,  0.2, -0.5,  0.5,  0.0,  0.7,  0.1, 0.0,  0.0},  // Bass Control
    { 0.5,  0.4,  0.2, -0.5,  0.6,  0.2,  0.0, 0.0,  0.0},  // Open Master
    {-0.5, -0.3, -0.7,  0.6,  0.0,  0.3,  0.5, 0.0,  0.0},  // Dense
    { 0.2,  0.2, -0.2, -0.1,  0.3,  0.0,  0.0, 0.0, -0.8},  // Wide
    { 0.9, -0.5, -0.2,  0.2, -0.5,  0.4,  0.0, 0.0,  0.0},  // Transient
    {-0.2,  0.6, -0.1, -0.4,  0.8,  0.0,  0.0, 0.0,  0.0},  // Smooth
    {-0.3, -0.2, -0.8,  0.7, -0.2,  0.3,  0.6, 0.0,  0.0},  // Loud
    {-0.3,  0.3, -0.1, -0.6,  0.8,  0.0, -1.0, 0.0,  0.0},  // Invisible
};

struct CharNode { double rmsMix, kneeAdd, ratioScale, attScale, relScale, twoStage, satDrive, satBias; };
// Clean <-> Modern <-> Punch <-> FET <-> Opto <-> VCA <-> Vari-Mu <-> Saturated
inline const CharNode kChar[8] = {
    {0.5,  0.0, 1.0, 1.00, 1.0, 0.0, 0.00, 0.00},  // Clean
    {0.4,  2.0, 1.0, 0.80, 0.8, 0.2, 0.10, 0.00},  // Modern
    {0.2, -2.0, 1.1, 1.80, 0.7, 0.0, 0.20, 0.00},  // Punch
    {0.0, -4.0, 1.5, 0.15, 0.6, 0.1, 0.50, 0.10},  // FET
    {0.8,  8.0, 0.6, 1.50, 1.8, 1.0, 0.20, 0.05},  // Opto
    {0.5,  0.0, 1.1, 0.70, 0.9, 0.0, 0.15, 0.00},  // VCA
    {0.9, 10.0, 0.5, 2.50, 1.6, 0.5, 0.45, 0.15},  // Vari-Mu
    {0.5,  0.0, 1.0, 1.00, 1.0, 0.0, 1.00, 0.20},  // Saturated
};
inline CharNode charAt(double pos01) {
    const double p = clampd(pos01, 0.0, 1.0) * 7.0;
    int i = (int)p;
    if (i >= 7) return kChar[7];
    const double f = p - i;
    const CharNode& a = kChar[i];
    const CharNode& b = kChar[i + 1];
    return {lerpd(a.rmsMix, b.rmsMix, f), lerpd(a.kneeAdd, b.kneeAdd, f), lerpd(a.ratioScale, b.ratioScale, f),
            lerpd(a.attScale, b.attScale, f), lerpd(a.relScale, b.relScale, f), lerpd(a.twoStage, b.twoStage, f),
            lerpd(a.satDrive, b.satDrive, f), lerpd(a.satBias, b.satBias, f)};
}

struct Flags { bool scene, spec, neural, chr, restore, master; double expMul; };
inline Flags flagsFor(int mode) {
    switch (mode) {
        case kModeClassic:   return {false, false, false, false, false, false, 1.0};
        case kModeAdaptive:  return {true,  false, false, false, false, false, 1.0};
        case kModeSpectral:  return {true,  true,  false, false, false, false, 1.0};
        case kModeNeural:    return {true,  false, true,  false, false, false, 1.0};
        case kModeHybrid:    return {true,  true,  true,  false, false, false, 1.0};
        case kModeCharacter: return {true,  false, false, true,  false, false, 1.0};
        case kModeMaster:    return {true,  true,  true,  false, false, true,  1.0};
        case kModeRestore:   return {false, false, false, false, true,  false, 1.0};
        default:             return {true,  true,  true,  true,  false, false, 1.5};
    }
}

// Static compression curve with soft knee. Returns gain reduction in dB (>= 0).
inline double gainReduction(double lvl, double thr, double ratio, double knee) {
    const double r = ratio >= 19.99 ? 1000.0 : ratio;
    const double slope = 1.0 / r - 1.0;  // negative
    const double over = lvl - thr;
    if (knee < 1e-3) return over > 0 ? -slope * over : 0.0;
    if (2.0 * over < -knee) return 0.0;
    if (2.0 * std::fabs(over) <= knee) {
        const double t = over + knee * 0.5;
        return -slope * t * t / (2.0 * knee);
    }
    return -slope * over;
}

inline double softCeil(double y, double c) {
    const double t = 0.8 * c, a = std::fabs(y);
    if (a <= t) return y;
    const double s = t + (c - t) * std::tanh((a - t) / (c - t));
    return y < 0 ? -s : s;
}

// ---------------------------------------------------------------- engine
class NDXEngine {
public:
    static constexpr int kCtrl = 32;  // control-rate block (samples)

    NDXEngine() { for (unsigned i = 0; i < kNumParams; ++i) P[i] = kSpecs[i].def; }

    void setParam(unsigned id, double plain) { if (id < kNumParams) P[id] = plain; }

    // Safe to call from a non-audio thread while processing: loads into the idle slot, then flips.
    bool loadWeights(const std::string& path) {
        const int idle = 1 - ncActive.load(std::memory_order_acquire);
        if (!ncSlot[idle].loadFile(path)) return false;
        ncSlot[idle].reset();
        ncActive.store(idle, std::memory_order_release);
        return true;
    }
    bool hasWeights() const { return ncSlot[ncActive.load(std::memory_order_acquire)].loaded; }

    int latencySamples() const { return L + kOsLat; }
    double meterGR() const { return meter; }
    void reset() { resetState(); }

    // ---- UI-thread-readable meters (updated once per process() call)
    float inputPeak(int ch) const { return inPk[ch & 1].load(std::memory_order_relaxed); }
    float outputPeak(int ch) const { return outPk[ch & 1].load(std::memory_order_relaxed); }
    float bandGainReductionDb(int b) const { return bandGr[b % 3].load(std::memory_order_relaxed); }
    float gainReductionDb() const { return grOut.load(std::memory_order_relaxed); }

    void prepare(double sampleRate) {
        sr = sampleRate > 8000.0 ? sampleRate : 48000.0;
        L = std::max(8, (int)std::lround(0.002 * sr));
        for (int f = 0; f < 3; ++f) osKernel[f] = designOsKernel(2 << f);
        rmsC = coefFromMs(10, sr);
        slowC = coefFromMs(150, sr);
        fastRel = std::exp(-1.0 / (0.010 * sr));
        smC = coefFromMs(15, sr);
        rAtkC = coefFromMs(0.3, sr);
        rRelC = coefFromMs(40, sr);
        corrC = coefFromMs(300, sr);
        resetState();
        if (!hasWeights()) {
            const std::string p = defaultWeightsPath();
            if (!p.empty()) loadWeights(p);
        }
    }

    // in/out: 2 channels (may alias). sc: nullptr or 2 channels (external sidechain).
    void process(float* const* in, const float* const* sc, float* const* out, int n) {
        NoDenorm nd;
        (void)nd;
        double mmax = 0.0;
        blkIn[0] = blkIn[1] = blkOut[0] = blkOut[1] = 0.0;
        int pos = 0;
        while (pos < n) {
            const int m = std::min(kCtrl, n - pos);
            updateControl(m);
            for (int i = 0; i < m; ++i) {
                processSample(pos + i, in, sc, out);
                if (grCur > mmax) mmax = grCur;
            }
            pos += m;
        }
        meter = mmax;
        for (int c = 0; c < 2; ++c) {
            inPk[c].store((float)blkIn[c], std::memory_order_relaxed);
            outPk[c].store((float)blkOut[c], std::memory_order_relaxed);
        }
        for (int b = 0; b < 3; ++b) bandGr[b].store((float)bandGrNow[b], std::memory_order_relaxed);
        grOut.store((float)mmax, std::memory_order_relaxed);
    }

private:
    struct Eff {
        double thr = -18, ratio = 4, knee = 6, spec = 0, satAmt = 0, satBias = 0, link = 1, rmsMix = 0.5;
        double atkC = 0.1, relFC = 0.001, relSC = 0.001, w = 0;
        int holdN = 0, la = 0, osF = 1;
        double makeupT = 1, outT = 1, mixT = 1, byT = 0, boostT = 0, trimT = 1;
        double bw[3] = {1, 1, 1};
        bool restore = false, ms = false, ext = false, scOn = false, hpOn = false, lpOn = false;
        bool trueEnv = false, safety = false;
        double ceil = 1.0;
    } E;

    double P[kNumParams];
    double sr = 48000.0;
    int L = 96;
    Delay dl[2], dsc[2], rawDl[2], dryDl[2], wetDl[2];
    Split3 aSplit[2], dSplit[2];
    Biquad hpB[2], lpB[2];
    double lastXL = -1, lastXH = -1, lastHp = -1, lastLp = -1;
    double dh[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}};
    double msEnv[2] = {0, 0}, fastEnv[2] = {0, 0}, slowMs[2] = {0, 0}, bandMs[2][3] = {{0, 0, 0}, {0, 0, 0}};
    Ball bal[2], balB[2][3];
    Sat sat[2];
    OsState osS[2];
    std::vector<double> osKernel[3];
    int osFactor = 1;
    bool satWas = false;
    NeuralController ncSlot[2];
    std::atomic<int> ncActive{0};
    double modsS[kNumMods] = {0};
    double rmsC = 0.002, slowC = 0.0001, fastRel = 0.999, smC = 0.001, rAtkC = 0.1, rRelC = 0.001, corrC = 0.0001;
    double sceneT = 0, sceneSil = 0, sceneSus = 1, transDensity = 0, avgGR = 0, grCur = 0, autoLevel = -30;
    double lowRatio = 0.4;
    double sLL = 1e-9, sRR = 1e-9, sLR = 0;
    double makeupS = 1, outS = 1, mixS = 1, byS = 0, boostS = 1, boostDbS = 0, trimS = 1;
    double meter = 0;
    double blkIn[2] = {0, 0}, blkOut[2] = {0, 0}, bandGrNow[3] = {0, 0, 0};
    std::atomic<float> inPk[2] = {{0.f}, {0.f}}, outPk[2] = {{0.f}, {0.f}}, bandGr[3] = {{0.f}, {0.f}, {0.f}};
    std::atomic<float> grOut{0.f};
    bool first = true;

    void resetState() {
        for (int c = 0; c < 2; ++c) {
            dl[c].init(L + kOsLat + 16); dsc[c].init(L + kOsLat + 16); rawDl[c].init(L + kOsLat + 16);
            dryDl[c].init(kOsLat + 16); wetDl[c].init(kOsLat + 16);
            aSplit[c].reset(); dSplit[c].reset(); hpB[c].reset(); lpB[c].reset();
            msEnv[c] = fastEnv[c] = slowMs[c] = 0;
            for (int k = 0; k < 4; ++k) dh[c][k] = 0;
            for (int b = 0; b < 3; ++b) { bandMs[c][b] = 0; balB[c][b].reset(); }
            bal[c].reset(); sat[c].reset(); osS[c].reset();
        }
        for (double& v : modsS) v = 0;
        ncSlot[0].reset(); ncSlot[1].reset();
        sceneT = sceneSil = transDensity = avgGR = grCur = 0;
        sceneSus = 1; autoLevel = -30; lowRatio = 0.4;
        sLL = sRR = 1e-9; sLR = 0;
        boostS = 1; boostDbS = 0;
        lastXL = lastXH = lastHp = lastLp = -1;
        satWas = false;
        first = true;
    }

    void updateControl(int m) {
        const int mode = (int)P[kMode];
        const Flags fl = flagsFor(mode);
        NeuralController& nc = ncSlot[ncActive.load(std::memory_order_acquire)];

        // ---- features -------------------------------------------------
        double fastDb = -120, slowDb = -120, rmsDb = -120;
        for (int c = 0; c < 2; ++c) {
            fastDb = std::max(fastDb, linToDb(fastEnv[c]));
            slowDb = std::max(slowDb, linToDb(std::sqrt(slowMs[c]) * 1.41421356));
            rmsDb = std::max(rmsDb, linToDb(std::sqrt(msEnv[c]) * 1.41421356));
        }
        const double dTr = fastDb - slowDb;
        const double silNow = 1.0 - smooth01(-75.0, -60.0, slowDb);
        const double trNow = smooth01(3.0, 9.0, dTr) * (1.0 - silNow);
        const double a10 = 1.0 - std::exp(-m / (sr * 0.01));
        sceneT += a10 * (trNow - sceneT);
        sceneSil += a10 * (silNow - sceneSil);
        sceneSus = clampd(1.0 - sceneT - sceneSil, 0.0, 1.0);
        transDensity += (1.0 - std::exp(-m / (sr * 1.0))) * (sceneT - transDensity);
        const double crest = slowDb < -80 ? 0.0 : clampd(fastDb - rmsDb, 0.0, 30.0);
        double pw[3] = {0, 0, 0};
        for (int c = 0; c < 2; ++c) for (int b = 0; b < 3; ++b) pw[b] += bandMs[c][b];
        const double tot = pw[0] + pw[1] + pw[2] + 1e-18;
        lowRatio = pw[0] / tot;
        const double corr = clampd(sLR / std::sqrt(sLL * sRR + 1e-18), -1.0, 1.0);
        avgGR += (1.0 - std::exp(-m / (sr * 2.0))) * (grCur - avgGR);
        if (slowDb > -60.0) autoLevel += (1.0 - std::exp(-m / (sr * 4.0))) * (slowDb - autoLevel);
        const double guard = clampd(avgGR / 24.0, 0.0, 1.0);

        // ---- neural / intent controller (bounded) -----------------------
        double raw[kNumMods] = {0};
        if (fl.neural) {
            const double x[NeuralController::NIN] = {
                clampd((slowDb + 60.0) / 60.0, 0.0, 1.2), crest / 20.0, transDensity,
                pw[0] / tot, pw[1] / tot, pw[2] / tot, corr, grCur / 24.0, avgGR / 24.0,
                sceneT, sceneSus, sceneSil};
            double resid[kNumMods] = {0};
            nc.step(x, resid);
            const int intent = (int)clampd(P[kIntent], 0, 11);
            for (int k = 0; k < kNumMods; ++k) raw[k] = kIntentTable[intent][k];
            raw[0] += 0.3 * transDensity;
            raw[1] += 0.4 * sceneSus - 0.3 * sceneT + 0.3 * guard;
            raw[2] += 0.3 * sceneSil;
            raw[3] -= 0.3 * guard;
            raw[5] += 0.4 * (pw[0] / tot - 0.4);
            const double amt = P[kNeural] / 100.0 * fl.expMul;
            for (int k = 0; k < kNumMods; ++k) raw[k] = clampd((raw[k] + 0.5 * resid[k]) * amt, -1.5, 1.5);
        }
        const double slew = 1.0 - std::exp(-m / (sr * 0.05));
        for (int k = 0; k < kNumMods; ++k) modsS[k] += slew * (raw[k] - modsS[k]);

        // ---- effective parameters (all clamped = safety layer) ----------
        CharNode cn = {-1.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0};
        if (fl.chr) cn = charAt(P[kCharacter] / 100.0);

        double thrBase = P[kThreshold];
        if (P[kAutoThreshold] > 0.5) thrBase = autoLevel + P[kThreshold];
        E.thr = clampd(thrBase + 6.0 * modsS[2], -70.0, 6.0);
        E.ratio = clampd(P[kRatio] * std::pow(2.0, 0.75 * modsS[3]) * cn.ratioScale, 1.0, 20.0);
        E.knee = clampd(P[kKnee] + 6.0 * modsS[4] + cn.kneeAdd, 0.0, 30.0);
        if (fl.master) { E.ratio = std::min(E.ratio, 3.0); E.knee = std::max(E.knee, 6.0); }

        const double attMs = clampd(P[kAttack] * std::pow(2.0, 2.0 * modsS[0]) * cn.attScale, 0.05, 200.0);
        double relMs = clampd(P[kRelease] * std::pow(2.0, 1.5 * modsS[1]) * cn.relScale, 5.0, 4000.0);
        if (fl.scene) relMs = std::max(relMs, 8.0 + 40.0 * guard);
        E.atkC = coefFromMs(attMs, sr);
        double wT = 0.0;
        if (fl.scene) {
            E.relFC = coefFromMs(relMs * 0.35, sr);
            E.relSC = coefFromMs(relMs * 2.5, sr);
            wT = clampd(0.25 + 0.6 * sceneSus - 0.5 * sceneT + 0.5 * cn.twoStage + 0.3 * guard, 0.0, 1.0);
        } else {
            E.relFC = E.relSC = coefFromMs(relMs, sr);
        }
        E.w += slew * (wT - E.w);
        E.holdN = (int)std::lround(P[kHold] * 0.001 * sr);
        E.la = (int)clampd(std::floor(P[kLookahead] * 0.001 * sr + 0.5), 0.0, (double)L);

        E.spec = fl.spec ? clampd(P[kSpectral] / 100.0 + 0.5 * modsS[5], 0.0, 1.0) : 0.0;
        double s = P[kSaturation] / 100.0 + 0.5 * modsS[6] + cn.satDrive;
        if (fl.master) s = std::min(s, 0.2);
        E.satAmt = clampd(s, 0.0, 1.0);
        E.satBias = cn.satBias;
        E.mixT = clampd(P[kMix] / 100.0 + 0.3 * modsS[7], 0.0, 1.0);
        E.link = clampd(P[kLink] / 100.0 + 0.5 * modsS[8], 0.0, 1.0);

        const int det = (int)P[kDetector];  // 0 Peak, 1 RMS, 2 True Env
        E.rmsMix = cn.rmsMix >= 0.0 ? cn.rmsMix : (det == 0 ? 0.0 : (det == 1 ? 1.0 : 0.5));
        E.trueEnv = det == 2;

        // spectral band weights
        const double lowW = 1.0 - P[kLowProtect] / 100.0;
        switch ((int)P[kSpecWeight]) {
            case 1:  E.bw[0] = lowW;       E.bw[1] = 1.0; E.bw[2] = 1.0; break;                  // Static
            case 2:  E.bw[0] = 1.0 * lowW; E.bw[1] = 0.6; E.bw[2] = 0.4; break;                  // Bass Focus
            case 3:  E.bw[0] = 0.5 * lowW; E.bw[1] = 1.0; E.bw[2] = 0.7; break;                  // Vocal Focus
            case 4:  E.bw[0] = 0.4 * lowW; E.bw[1] = 0.6; E.bw[2] = 1.0; break;                  // Air Focus
            default: E.bw[0] = 1.0 - (P[kLowProtect] / 100.0) * clampd(0.3 + 1.4 * lowRatio, 0.0, 1.0);
                     E.bw[1] = 1.0; E.bw[2] = 1.0; break;                                        // Adaptive
        }

        double mk = P[kMakeup];
        if (P[kAutoMakeup] > 0.5) mk += -E.thr * (1.0 - 1.0 / E.ratio) * 0.5;
        E.makeupT = fl.restore ? 1.0 : dbToLin(clampd(mk, -24.0, 36.0));
        E.outT = dbToLin(P[kOutput]);
        E.trimT = dbToLin(P[kInputTrim]);
        E.byT = P[kBypass] > 0.5 ? 1.0 : 0.0;
        E.restore = fl.restore;
        E.ms = P[kStereoMode] > 0.5;
        E.ext = P[kSidechain] > 0.5;
        E.safety = P[kSafety] > 0.5;
        E.ceil = dbToLin(P[kCeiling]);
        E.boostT = (fl.restore && slowDb > -70.0) ? P[kRestore] / 100.0 * 12.0 * smooth01(2.0, 12.0, dTr) : 0.0;

        // sidechain filters (2nd-order Butterworth, detector path only)
        E.scOn = P[kScFilter] > 0.5;
        E.hpOn = E.scOn && P[kScHpf] > 21.0;
        E.lpOn = E.scOn && P[kScLpf] < 19500.0;
        if (std::fabs(P[kScHpf] - lastHp) > 0.01) {
            for (int c = 0; c < 2; ++c) hpB[c].setButter(P[kScHpf], sr, true);
            lastHp = P[kScHpf];
        }
        if (std::fabs(P[kScLpf] - lastLp) > 0.5) {
            for (int c = 0; c < 2; ++c) lpB[c].setButter(P[kScLpf], sr, false);
            lastLp = P[kScLpf];
        }
        if (!E.hpOn) for (int c = 0; c < 2; ++c) hpB[c].reset();
        if (!E.lpOn) for (int c = 0; c < 2; ++c) lpB[c].reset();

        // oversampling factor (kernels were designed in prepare(): no allocation here)
        const int f = 1 << (int)clampd(P[kOversample], 0, 3);
        if (f != osFactor) {
            osFactor = f;
            for (int c = 0; c < 2; ++c) { osS[c].reset(); sat[c].reset(); }
        }
        E.osF = osFactor;

        const double fl_ = P[kXoverLow], fh_ = std::max(P[kXoverHigh], fl_ * 2.0);
        if (std::fabs(fl_ - lastXL) > 0.5 || std::fabs(fh_ - lastXH) > 0.5) {
            for (int c = 0; c < 2; ++c) { aSplit[c].set(fl_, fh_, sr); dSplit[c].set(fl_, fh_, sr); }
            lastXL = fl_; lastXH = fh_;
        }

        if (first) {
            makeupS = E.makeupT; outS = E.outT; mixS = E.mixT; byS = E.byT; trimS = E.trimT;
            first = false;
        }
    }

    inline void processSample(int i, float* const* in, const float* const* sc, float* const* out) {
        const bool ext = E.ext && sc != nullptr;
        trimS += smC * (E.trimT - trimS);
        double x[2];
        for (int c = 0; c < 2; ++c) {
            double v = in[c][i];
            if (!std::isfinite(v)) v = 0.0;
            rawDl[c].push((float)v);            // untouched copy for bypass
            x[c] = v * trimS;
            dl[c].push((float)x[c]);
            if (sc) dsc[c].push(sc[c][i]);
            blkIn[c] = std::max(blkIn[c], std::fabs(x[c]));
        }
        sLL += corrC * (x[0] * x[0] - sLL);
        sRR += corrC * (x[1] * x[1] - sRR);
        sLR += corrC * (x[0] * x[1] - sLR);

        double a[2], d[2];
        for (int c = 0; c < 2; ++c) {
            a[c] = dl[c].tap(L);
            d[c] = ext ? (double)dsc[c].tap(L - E.la) : (double)dl[c].tap(L - E.la);
        }
        if (E.ms) {
            const double am = (a[0] + a[1]) * 0.5, as = (a[0] - a[1]) * 0.5;
            const double dm = (d[0] + d[1]) * 0.5, ds = (d[0] - d[1]) * 0.5;
            a[0] = am; a[1] = as; d[0] = dm; d[1] = ds;
        }

        // ---- detection ---------------------------------------------------
        double lvl[2], bl[2][3];
        for (int c = 0; c < 2; ++c) {
            double v = d[c];
            if (E.hpOn) v = hpB[c].process(v);
            if (E.lpOn) v = lpB[c].process(v);
            double pk = std::fabs(v);
            double* h = dh[c];
            h[0] = h[1]; h[1] = h[2]; h[2] = h[3]; h[3] = v;
            if (E.trueEnv) {  // inter-sample peak estimate (Catmull-Rom midpoint)
                const double mid = (-h[0] + 9.0 * h[1] + 9.0 * h[2] - h[3]) * 0.0625;
                pk = std::max(pk, std::fabs(mid));
            }
            msEnv[c] += rmsC * (v * v - msEnv[c]);
            const double rms = std::sqrt(msEnv[c]) * 1.41421356;
            lvl[c] = linToDb(pk + (rms - pk) * E.rmsMix);
            fastEnv[c] = std::max(pk, fastEnv[c] * fastRel);
            slowMs[c] += slowC * (v * v - slowMs[c]);
            double bd[3];
            dSplit[c].process(v, bd, false);
            for (int b = 0; b < 3; ++b) {
                bandMs[c][b] += rmsC * (bd[b] * bd[b] - bandMs[c][b]);
                bl[c][b] = linToDb(std::sqrt(bandMs[c][b]) * 1.41421356);
            }
        }
        const double mx = std::max(lvl[0], lvl[1]);
        double bmx[3];
        for (int b = 0; b < 3; ++b) bmx[b] = std::max(bl[0][b], bl[1][b]);

        // ---- smoothed ramps ---------------------------------------------
        makeupS += smC * (E.makeupT - makeupS);
        outS += smC * (E.outT - outS);
        mixS += smC * (E.mixT - mixS);
        byS += smC * (E.byT - byS);
        if (E.restore) {
            boostDbS += (E.boostT > boostDbS ? rAtkC : rRelC) * (E.boostT - boostDbS);
            boostS = std::exp(boostDbS * 0.11512925464970229);
        }

        const bool satActive = E.satAmt > 0.001;
        const double drv = 1.0 + 7.0 * E.satAmt;
        const double blend = clampd(E.satAmt * 4.0, 0.0, 1.0);
        if (!satActive && satWas) {
            for (int c = 0; c < 2; ++c) { sat[c].reset(); osS[c].reset(); }
        }
        satWas = satActive;

        double y[2], grSum = 0.0;
        for (int c = 0; c < 2; ++c) {
            double g[3];
            double grBand[3] = {0, 0, 0};
            if (E.restore) {
                g[0] = g[1] = g[2] = boostS;
            } else {
                const double l2 = lvl[c] + (mx - lvl[c]) * E.link;
                const double grM = bal[c].process(gainReduction(l2, E.thr, E.ratio, E.knee),
                                                  E.atkC, E.relFC, E.relSC, E.holdN, E.w);
                grSum += grM;
                for (int b = 0; b < 3; ++b) {
                    const double lb = bl[c][b] + (bmx[b] - bl[c][b]) * E.link;
                    const double tb = gainReduction(lb, E.thr - 5.0, E.ratio, E.knee) * E.bw[b];
                    const double gb = balB[c][b].process(tb, E.atkC, E.relFC, E.relSC, E.holdN, E.w);
                    const double gr = grM + (gb - grM) * E.spec;
                    grBand[b] = gr;
                    g[b] = std::exp(-gr * 0.11512925464970229);
                }
            }
            for (int b = 0; b < 3; ++b) bandGrNow[b] = (c == 0 ? 0.0 : bandGrNow[b]) + grBand[b] * 0.5;

            double ab[3];
            aSplit[c].process(a[c], ab, true);
            const double dryNow = ab[0] + ab[1] + ab[2];
            const double wetNow = ab[0] * g[0] + ab[1] * g[1] + ab[2] * g[2];

            // everything after the split is delayed by kOsLat so latency is constant
            dryDl[c].push((float)dryNow);
            wetDl[c].push((float)wetNow);
            const double dry = dryDl[c].tap(kOsLat);
            double wet = wetDl[c].tap(kOsLat);
            if (satActive) {
                double so;
                if (E.osF == 1) so = sat[c].process(wet, drv, E.satBias);
                else so = osProcess(osS[c], osKernel[(E.osF == 2) ? 0 : (E.osF == 4 ? 1 : 2)], E.osF, wetNow,
                                    [&](double v) { return sat[c].process(v, drv, E.satBias); });
                wet += (so - wet) * blend;
            }
            wet *= makeupS;
            y[c] = (dry * (1.0 - mixS) + wet * mixS) * outS;
        }
        grCur = E.restore ? 0.0 : grSum * 0.5;

        if (E.ms) {
            const double l = y[0] + y[1], r = y[0] - y[1];
            y[0] = l; y[1] = r;
        }
        for (int c = 0; c < 2; ++c) {
            const double rawD = rawDl[c].tap(L + kOsLat);
            double o = y[c] + (rawD - y[c]) * byS;
            if (E.safety) o = softCeil(o, E.ceil);
            if (!std::isfinite(o)) o = 0.0;
            blkOut[c] = std::max(blkOut[c], std::fabs(o));
            out[c][i] = (float)o;
        }
    }
};

}  // namespace ndx
