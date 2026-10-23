#pragma once
#include "pluginterfaces/base/funknown.h"

// Unique class IDs. If you fork this plugin, generate your own (e.g. with the SDK's
// "VST3 Project Generator" or any GUID tool) - and update tools/make_presets.py to match.
static const Steinberg::FUID kNDXProcessorUID(0x4E445850, 0x7A1C4B3E, 0x9F0D2C58, 0x1B6E8A37);
static const Steinberg::FUID kNDXControllerUID(0x4E445843, 0x3D92481F, 0xA7C5E614, 0x0F8B25D9);
