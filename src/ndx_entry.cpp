#include "ndx_controller.h"
#include "ndx_ids.h"
#include "ndx_processor.h"
#include "ndx_version.h"
#include "public.sdk/source/main/pluginfactory.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

// SDK 3.7+ provides InitModule/DeinitModule itself. For older SDKs configure with
// -DNDX_LEGACY_INITMODULE=ON (see CMakeLists.txt).
#ifdef NDX_LEGACY_INITMODULE
bool InitModule() { return true; }
bool DeinitModule() { return true; }
#endif

BEGIN_FACTORY_DEF(NDX_COMPANY, NDX_URL, NDX_EMAIL)

    DEF_CLASS2(INLINE_UID_FROM_FUID(kNDXProcessorUID),
               PClassInfo::kManyInstances,
               kVstAudioEffectClass,
               NDX_NAME,
               Vst::kDistributable,
               Vst::PlugType::kFxDynamics,
               NDX_VERSION_STR,
               kVstVersionString,
               NDXProcessor::createInstance)

    DEF_CLASS2(INLINE_UID_FROM_FUID(kNDXControllerUID),
               PClassInfo::kManyInstances,
               kVstComponentControllerClass,
               NDX_NAME " Controller",
               0,
               "",
               NDX_VERSION_STR,
               kVstVersionString,
               NDXController::createInstance)

END_FACTORY
