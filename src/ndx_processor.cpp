#include "ndx_processor.h"

#include <algorithm>
#include <cstring>

#include "base/source/fstreamer.h"
#include "ndx_ids.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

NDXProcessor::NDXProcessor() {
    setControllerClass(kNDXControllerUID);
    for (unsigned i = 0; i < ndx::kNumParams; ++i) {
        norm[i] = ndx::normFromPlain(ndx::kSpecs[i], ndx::kSpecs[i].def);
        engine.setParam(i, ndx::kSpecs[i].def);
    }
}

tresult PLUGIN_API NDXProcessor::initialize(FUnknown* context) {
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk) return result;
    addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
    addAudioInput(STR16("Sidechain"), SpeakerArr::kStereo, BusTypes::kAux, 0);  // inactive by default
    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
    return kResultOk;
}

tresult PLUGIN_API NDXProcessor::setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                                    SpeakerArrangement* outputs, int32 numOuts) {
    if (numIns != 2 || numOuts != 1) return kResultFalse;
    if (inputs[0] != SpeakerArr::kStereo || inputs[1] != SpeakerArr::kStereo) return kResultFalse;
    if (outputs[0] != SpeakerArr::kStereo) return kResultFalse;
    return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
}

tresult PLUGIN_API NDXProcessor::setupProcessing(ProcessSetup& setup) {
    sampleRate = setup.sampleRate;
    engine.prepare(sampleRate);  // may read ndx_weights.bin (not called from the audio thread)
    return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API NDXProcessor::setActive(TBool state) {
    if (state) engine.reset();
    return AudioEffect::setActive(state);
}

tresult PLUGIN_API NDXProcessor::canProcessSampleSize(int32 symbolicSampleSize) {
    return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
}

uint32 PLUGIN_API NDXProcessor::getLatencySamples() {
    return static_cast<uint32>(engine.latencySamples());
}

void NDXProcessor::applyParam(ParamID id, double n) {
    if (id >= ndx::kNumParams || id == ndx::kGainReduction) return;
    norm[id] = n;
    engine.setParam(id, ndx::plainFromNorm(ndx::kSpecs[id], n));
}

tresult PLUGIN_API NDXProcessor::process(ProcessData& data) {
    // ---- parameter changes (last point of each queue; engine smooths internally)
    if (data.inputParameterChanges) {
        const int32 count = data.inputParameterChanges->getParameterCount();
        for (int32 i = 0; i < count; ++i) {
            IParamValueQueue* q = data.inputParameterChanges->getParameterData(i);
            if (!q) continue;
            const int32 pts = q->getPointCount();
            if (pts <= 0) continue;
            ParamValue v = 0;
            int32 offset = 0;
            if (q->getPoint(pts - 1, offset, v) == kResultTrue) applyParam(q->getParameterId(), v);
        }
    }

    if (data.numInputs < 1 || data.numOutputs < 1 || data.numSamples <= 0) return kResultOk;
    AudioBusBuffers& in = data.inputs[0];
    AudioBusBuffers& out = data.outputs[0];
    if (!in.channelBuffers32 || !out.channelBuffers32) return kResultOk;

    if (in.numChannels < 2 || out.numChannels < 2) {  // not stereo: pass through
        const int32 n = std::min(in.numChannels, out.numChannels);
        for (int32 c = 0; c < n; ++c)
            if (out.channelBuffers32[c] != in.channelBuffers32[c])
                std::memcpy(out.channelBuffers32[c], in.channelBuffers32[c], sizeof(float) * (size_t)data.numSamples);
        out.silenceFlags = 0;
        return kResultOk;
    }

    const float* scBuf[2] = {nullptr, nullptr};
    const float* const* sc = nullptr;
    if (data.numInputs > 1 && data.inputs[1].numChannels >= 2 && data.inputs[1].channelBuffers32) {
        scBuf[0] = data.inputs[1].channelBuffers32[0];
        scBuf[1] = data.inputs[1].channelBuffers32[1];
        if (scBuf[0] && scBuf[1]) sc = scBuf;
    }

    engine.process(in.channelBuffers32, sc, out.channelBuffers32, data.numSamples);
    out.silenceFlags = 0;

    // ---- gain-reduction meter -> host (read-only parameter)
    if (data.outputParameterChanges) {
        int32 index = 0;
        IParamValueQueue* q = data.outputParameterChanges->addParameterData(ndx::kGainReduction, index);
        if (q) {
            int32 pt = 0;
            q->addPoint(0, std::min(1.0, engine.meterGR() / 30.0), pt);
        }
    }
    return kResultOk;
}

tresult PLUGIN_API NDXProcessor::setState(IBStream* state) {
    if (!state) return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    int32 ver = 0;
    if (!s.readInt32(ver) || ver != 2) return kResultFalse;
    for (unsigned i = 0; i < ndx::kNumParams; ++i) {
        double v = 0;
        if (!s.readDouble(v)) return kResultFalse;
        applyParam(i, v);
    }
    return kResultOk;
}

tresult PLUGIN_API NDXProcessor::getState(IBStream* state) {
    if (!state) return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    s.writeInt32(2);
    for (unsigned i = 0; i < ndx::kNumParams; ++i) s.writeDouble(norm[i]);
    return kResultOk;
}
