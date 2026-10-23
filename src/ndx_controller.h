#pragma once
#include "public.sdk/source/vst/vsteditcontroller.h"

class NDXController : public Steinberg::Vst::EditController {
public:
    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IEditController*>(new NDXController());
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    // v1.0 uses the host's generic editor. A custom VSTGUI "Compression Field" view goes here later.
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString /*name*/) SMTG_OVERRIDE { return nullptr; }
};
