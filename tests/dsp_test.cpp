// Standalone N-DX DSP tests - no VST3 SDK required.
#include "ndx_dsp.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <vector>

using namespace ndx;

static int fails = 0;
#define CHECK(cond, msg)                                         \
    do {                                                         \
        if (!(cond)) { std::printf("FAIL: %s\n", msg); ++fails; } \
        else std::printf("ok:   %s\n", msg);                     \
    } while (0)

static const double SR = 48000.0;

static void setDefaults(NDXEngine& e) {
    for (unsigned i = 0; i < kNumParams; ++i) e.setParam(i, kSpecs[i].def);
}

struct Out { std::vector<float> L, R; double meter = 0; };

static Out run(NDXEngine& e, const std::vector<float>& inL, const std::vector<float>& inR) {
    Out o;
    const size_t N = inL.size();
    o.L.assign(N, 0.f); o.R.assign(N, 0.f);
    std::vector<float> a = inL, b = inR;
    for (size_t pos = 0; pos < N; pos += 480) {
        const int n = (int)std::min<size_t>(480, N - pos);
        float* in[2] = {a.data() + pos, b.data() + pos};
        float* out[2] = {o.L.data() + pos, o.R.data() + pos};
        e.process(in, nullptr, out, n);
        o.meter = e.meterGR();
    }
    return o;
}

static std::vector<float> sine(size_t n, double f, double amp) {
    std::vector<float> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = (float)(amp * std::sin(2.0 * kPi * f * (double)i / SR));
    return v;
}
static double rmsDb(const std::vector<float>& v, size_t from, size_t to) {
    double s = 0;
    for (size_t i = from; i < to; ++i) s += (double)v[i] * v[i];
    return 10.0 * std::log10(s / (double)(to - from) + 1e-20);
}

int main() {
    // 1. latency
    {
        NDXEngine e; e.prepare(SR);
        CHECK(e.latencySamples() == (int)std::lround(0.002 * SR) + kOsLat, "latency = 2 ms lookahead + constant oversampler delay");
    }

    // 2. robustness sweep: every mode x intent, loud noisy program, must stay finite and bounded
    {
        std::mt19937 rng(1);
        std::uniform_real_distribution<float> U(-1.f, 1.f);
        const size_t N = 48000;
        std::vector<float> L(N), R(N);
        for (size_t i = 0; i < N; ++i) {
            const double env = (i / 6000) % 2 ? 2.0 : 0.05;
            L[i] = (float)(env * U(rng)); R[i] = (float)(env * U(rng) * 0.7 + 0.3 * L[i]);
        }
        bool ok = true;
        for (int mode = 0; mode < 9 && ok; ++mode)
            for (int intent = 0; intent < 12 && ok; ++intent) {
                NDXEngine e; setDefaults(e); e.prepare(SR);
                e.setParam(kMode, mode); e.setParam(kIntent, intent);
                e.setParam(kCharacter, 20.0 * (intent % 6)); e.setParam(kSaturation, 40);
                e.setParam(kStereoMode, intent % 2); e.setParam(kAutoThreshold, intent % 3 == 0);
                Out o = run(e, L, R);
                for (size_t i = 0; i < N; ++i)
                    if (!std::isfinite(o.L[i]) || !std::isfinite(o.R[i]) || std::fabs(o.L[i]) > 50.f || std::fabs(o.R[i]) > 50.f) { ok = false; break; }
            }
        CHECK(ok, "all modes x intents finite and bounded");
    }

    // 3. bypass is a pure latency-aligned copy
    {
        NDXEngine e; setDefaults(e); e.prepare(SR);
        e.setParam(kBypass, 1);
        std::vector<float> z(24000, 0.f), imp(2000, 0.f);
        run(e, z, z);
        imp[100] = 1.f;
        Out o = run(e, imp, imp);
        size_t pk = 0;
        for (size_t i = 0; i < o.L.size(); ++i) if (std::fabs(o.L[i]) > std::fabs(o.L[pk])) pk = i;
        CHECK(pk == 100 + (size_t)e.latencySamples(), "bypass impulse arrives exactly at reported latency");
        CHECK(std::fabs(o.L[pk] - 1.f) < 0.01f, "bypass impulse amplitude preserved");
    }

    // 4. classic compression amount (thr -20, ratio 4, hard knee, 0.5 amp sine -> ~10.5 dB GR)
    {
        NDXEngine e; setDefaults(e); e.prepare(SR);
        e.setParam(kMode, kModeClassic); e.setParam(kThreshold, -20); e.setParam(kRatio, 4);
        e.setParam(kKnee, 0); e.setParam(kAttack, 1); e.setParam(kRelease, 100); e.setParam(kDetector, 2);
        auto s = sine(48000, 1000.0, 0.5);
        Out o = run(e, s, s);
        const double red = rmsDb(s, 36000, 48000) - rmsDb(o.L, 36000, 48000);
        std::printf("      measured reduction %.2f dB, meter %.2f dB\n", red, o.meter);
        CHECK(red > 7.0 && red < 13.0, "classic mode reduces loud sine by roughly the expected amount");
        CHECK(o.meter > 6.0 && o.meter < 14.0, "gain-reduction meter reports compression");
    }

    // 5. below threshold = unity (band split sums flat)
    {
        NDXEngine e; setDefaults(e); e.prepare(SR);
        e.setParam(kMode, kModeHybrid); e.setParam(kSpectral, 100);
        for (double f : {80.0, 1000.0, 9000.0}) {
            auto s = sine(48000, f, 0.01);
            Out o = run(e, s, s);
            const double d = rmsDb(o.L, 36000, 48000) - rmsDb(s, 36000, 48000);
            char msg[96];
            std::snprintf(msg, sizeof msg, "quiet %.0f Hz sine passes at unity (%.2f dB)", f, d);
            CHECK(std::fabs(d) < 0.5, msg);
        }
    }

    // 6. weights file loader
    {
        const char* path = "ndx_test_weights.bin";
        {
            std::ofstream f(path, std::ios::binary);
            f.write("NDXW", 4);
            int32_t hdr[4] = {1, 12, 16, 9};
            f.write(reinterpret_cast<char*>(hdr), 16);
            std::mt19937 rng(7);
            std::uniform_real_distribution<float> U(-0.3f, 0.3f);
            const int count = 16 * 12 + 16 * 16 + 16 + 9 * 16 + 9;
            for (int i = 0; i < count; ++i) { float v = U(rng); f.write(reinterpret_cast<char*>(&v), 4); }
        }
        NDXEngine e; setDefaults(e); e.prepare(SR);
        CHECK(e.loadWeights(path), "valid weights file loads");
        e.setParam(kMode, kModeHybrid);
        auto s = sine(48000, 220.0, 0.9);
        Out o = run(e, s, s);
        bool ok = true;
        for (float v : o.L) if (!std::isfinite(v) || std::fabs(v) > 10.f) ok = false;
        CHECK(ok, "processing with learned residual stays bounded");
        NDXEngine e2;
        CHECK(!e2.loadWeights("does_not_exist.bin"), "missing weights file rejected");
        std::remove(path);
    }


    // 7. oversampler kernel: identity nonlinearity -> unity impulse at exactly kOsLat samples, all factors
    for (int F : {2, 4, 8}) {
        const std::vector<double> h = designOsKernel(F);
        OsState st;
        double best = 0; int bestIdx = -1;
        for (int n = 0; n < 120; ++n) {
            double o = osProcess(st, h, F, n == 10 ? 1.0 : 0.0, [](double v) { return v; });
            if (std::fabs(o) > std::fabs(best)) { best = o; bestIdx = n; }
        }
        char msg[96];
        std::snprintf(msg, sizeof msg, "oversampler %dx: impulse at +%d samples, peak %.3f (band-limited)", F, bestIdx - 10, best);
        CHECK(bestIdx - 10 == kOsLat && best > 0.9 && best < 1.05, msg);
    }

    // 8. saturation + oversampling stays time-aligned with the dry/non-saturated path
    {
        auto s = sine(24000, 1000.0, 0.01);
        NDXEngine a; setDefaults(a); a.prepare(SR); a.setParam(kMode, kModeClassic); a.setParam(kThreshold, 0);
        NDXEngine b; setDefaults(b); b.prepare(SR); b.setParam(kMode, kModeClassic); b.setParam(kThreshold, 0);
        b.setParam(kSaturation, 100); b.setParam(kOversample, 2);
        Out oa = run(a, s, s), ob = run(b, s, s);
        double md = 0;
        for (size_t i = 12000; i < 24000; ++i) md = std::max(md, (double)std::fabs(oa.L[i] - ob.L[i]));
        std::printf("      max diff sat(4x) vs clean: %.6f\n", md);
        CHECK(md < 0.0008, "saturator with 4x oversampling is time-aligned (no latency change)");
    }

    // 9. every oversampling factor + every detector + sidechain filter stays finite
    {
        std::mt19937 rng(5);
        std::uniform_real_distribution<float> U(-1.f, 1.f);
        std::vector<float> L(24000), R(24000);
        for (size_t i = 0; i < L.size(); ++i) { L[i] = 1.5f * U(rng); R[i] = 1.5f * U(rng); }
        bool ok = true;
        for (int os = 0; os < 4; ++os) for (int det = 0; det < 3; ++det) for (int w = 0; w < 5; ++w) {
            NDXEngine e; setDefaults(e); e.prepare(SR);
            e.setParam(kOversample, os); e.setParam(kDetector, det); e.setParam(kSpecWeight, w);
            e.setParam(kSaturation, 80); e.setParam(kScFilter, 1); e.setParam(kInputTrim, 6);
            Out o = run(e, L, R);
            for (size_t i = 0; i < L.size(); ++i) if (!std::isfinite(o.L[i]) || std::fabs(o.L[i]) > 50.f) ok = false;
        }
        CHECK(ok, "oversampling x detector x spectral weighting x sidechain filter finite");
    }

    std::printf(fails ? "\n%d TEST(S) FAILED\n" : "\nALL TESTS PASSED\n", fails);
    return fails ? 1 : 0;
}
