#include "ndx_controller.h"

#include "base/source/fstreamer.h"
#include "ndx_params.h"
#include "pluginterfaces/base/fstrdefs.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"
#include "public.sdk/source/vst/vstparameters.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

// Continuous parameter with a power-law skew and unit display, driven by ndx::kSpecs.
class SkewParameter : public Parameter {
public:
    SkewParameter(const ndx::ParamSpec& s, int32 flags)
    : Parameter(UString128(s.name).getText(), s.id, UString128(s.units).getText(),
                ndx::normFromPlain(s, s.def), 0, flags),
      spec(s) {
        setPrecision(s.precision);
    }

    ParamValue toPlain(ParamValue valueNormalized) const SMTG_OVERRIDE {
        return ndx::plainFromNorm(spec, valueNormalized);
    }
    ParamValue toNormalized(ParamValue plainValue) const SMTG_OVERRIDE {
        return ndx::normFromPlain(spec, plainValue);
    }
    void toString(ParamValue valueNormalized, String128 string) const SMTG_OVERRIDE {
        UString wrapper(string, str16BufferSize(String128));
        wrapper.printFloat(toPlain(valueNormalized), precision);
    }
    bool fromString(const TChar* string, ParamValue& valueNormalized) const SMTG_OVERRIDE {
        UString wrapper(const_cast<TChar*>(string), strlen16(string));
        double v = 0.0;
        if (wrapper.scanFloat(v)) {
            valueNormalized = toNormalized(v);
            return true;
        }
        return false;
    }

private:
    const ndx::ParamSpec& spec;
};

}  // namespace

tresult PLUGIN_API NDXController::initialize(FUnknown* context) {
    tresult result = EditController::initialize(context);
    if (result != kResultOk) return result;

    for (unsigned i = 0; i < ndx::kNumParams; ++i) {
        const ndx::ParamSpec& s = ndx::kSpecs[i];
        if (s.items) {
            int32 flags = ParameterInfo::kCanAutomate | ParameterInfo::kIsList;
            if (i == ndx::kBypass) flags |= ParameterInfo::kIsBypass;
            auto* p = new StringListParameter(UString128(s.name).getText(), s.id, nullptr, flags);
            for (int k = 0; k < s.numItems; ++k) p->appendString(UString128(s.items[k]).getText());
            const double dn = ndx::normFromPlain(s, s.def);
            p->getInfo().defaultNormalizedValue = dn;
            p->setNormalized(dn);
            parameters.addParameter(p);
        } else {
            const int32 flags = (i == ndx::kGainReduction) ? static_cast<int32>(ParameterInfo::kIsReadOnly)
                                                           : static_cast<int32>(ParameterInfo::kCanAutomate);
            parameters.addParameter(new SkewParameter(s, flags));
        }
    }
    return kResultOk;
}

tresult PLUGIN_API NDXController::setComponentState(IBStream* state) {
    if (!state) return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    int32 ver = 0;
    if (!s.readInt32(ver) || ver != 2) return kResultFalse;
    for (unsigned i = 0; i < ndx::kNumParams; ++i) {
        double v = 0.0;
        if (!s.readDouble(v)) return kResultFalse;
        if (i != ndx::kGainReduction) setParamNormalized(i, v);
    }
    return kResultOk;
}
