#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace ndxjuce {

namespace {
juce::String formatValue(const ndx::ParamSpec& s, float v) {
    juce::String t(v, s.precision);
    const juce::String u(s.units);
    if (u.isEmpty()) return t;
    return (u == ":1" || u == "%") ? t + u : t + " " + u;
}
}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout NDXProcessor::createLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    for (unsigned i = 0; i < ndx::kNumParams; ++i) {
        const ndx::ParamSpec& s = ndx::kSpecs[i];
        if (i == ndx::kGainReduction) continue;  // meter only - read straight from the engine
        const ParameterID pid{paramId(i), 1};
        if (s.items == ndx::kOffOnItems) {
            layout.add(std::make_unique<AudioParameterBool>(pid, s.name, s.def > 0.5));
        } else if (s.items) {
            StringArray names;
            for (int k = 0; k < s.numItems; ++k) names.add(s.items[k]);
            layout.add(std::make_unique<AudioParameterChoice>(pid, s.name, names, (int)s.def));
        } else {
            const ndx::ParamSpec spec = s;
            NormalisableRange<float> range(
                (float)spec.mn, (float)spec.mx,
                [spec](float, float, float n) { return (float)ndx::plainFromNorm(spec, n); },
                [spec](float, float, float v) { return (float)ndx::normFromPlain(spec, v); });
            layout.add(std::make_unique<AudioParameterFloat>(
                pid, s.name, range, (float)s.def,
                AudioParameterFloatAttributes()
                    .withLabel(s.units)
                    .withStringFromValueFunction([spec](float v, int) { return formatValue(spec, v); })));
        }
    }
    return layout;
}

NDXProcessor::NDXProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "NDX_STATE", createLayout()) {
    for (unsigned i = 0; i < ndx::kNumParams; ++i)
        raw[i] = (i == ndx::kGainReduction) ? nullptr : apvts.getRawParameterValue(paramId(i));
    apvts.state.setProperty("preset", "Init (Hybrid)", nullptr);
}

juce::AudioProcessorParameter* NDXProcessor::getBypassParameter() const {
    return apvts.getParameter(paramId(ndx::kBypass));
}

bool NDXProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.inputBuses.size() > 1) {
        const auto sc = layouts.getChannelSet(true, 1);
        if (!sc.isDisabled() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void NDXProcessor::prepareToPlay(double sampleRate, int) {
    engine.prepare(sampleRate);  // may read ndx_weights.bin (not the audio thread)
    setLatencySamples(engine.latencySamples());
}

void NDXProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    for (unsigned i = 0; i < ndx::kNumParams; ++i)
        if (raw[i]) engine.setParam(i, (double)raw[i]->load());

    auto main = getBusBuffer(buffer, true, 0);
    if (main.getNumChannels() < 2) return;

    const float* scPtrs[2] = {nullptr, nullptr};
    const float* const* sc = nullptr;
    if (getBusCount(true) > 1 && getBus(true, 1) != nullptr && getBus(true, 1)->isEnabled()) {
        auto side = getBusBuffer(buffer, true, 1);
        if (side.getNumChannels() >= 2) {
            scPtrs[0] = side.getReadPointer(0);
            scPtrs[1] = side.getReadPointer(1);
            sc = scPtrs;
        }
    }

    // mono feed for the Compression Field analyser (input, before processing)
    {
        const int n = main.getNumSamples();
        int s1, z1, s2, z2;
        fifo.prepareToWrite(n, s1, z1, s2, z2);
        const float* l = main.getReadPointer(0);
        const float* r = main.getReadPointer(1);
        for (int i = 0; i < z1; ++i) fifoBuffer[(size_t)(s1 + i)] = 0.5f * (l[i] + r[i]);
        for (int i = 0; i < z2; ++i) fifoBuffer[(size_t)(s2 + i)] = 0.5f * (l[z1 + i] + r[z1 + i]);
        fifo.finishedWrite(z1 + z2);
    }

    float* const* chans = main.getArrayOfWritePointers();
    engine.process(chans, sc, chans, main.getNumSamples());
}

int NDXProcessor::popAnalyserSamples(float* dest, int maxSamples) {
    int s1, z1, s2, z2;
    const int n = std::min(maxSamples, fifo.getNumReady());
    fifo.prepareToRead(n, s1, z1, s2, z2);
    for (int i = 0; i < z1; ++i) dest[i] = fifoBuffer[(size_t)(s1 + i)];
    for (int i = 0; i < z2; ++i) dest[z1 + i] = fifoBuffer[(size_t)(s2 + i)];
    fifo.finishedRead(z1 + z2);
    return z1 + z2;
}

juce::AudioProcessorEditor* NDXProcessor::createEditor() { return new NDXEditor(*this); }

// ------------------------------------------------------------------ state
void NDXProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void NDXProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// ------------------------------------------------------------------ presets
juce::String NDXProcessor::getCurrentPresetName() const {
    return apvts.state.getProperty("preset", "Init (Hybrid)").toString();
}

void NDXProcessor::setPlain(unsigned id, double plain) {
    if (auto* p = apvts.getParameter(paramId(id))) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1((float)plain));
        p->endChangeGesture();
    }
}

void NDXProcessor::resetToDefaults() {
    for (unsigned i = 0; i < ndx::kNumParams; ++i)
        if (i != ndx::kGainReduction) setPlain(i, ndx::kSpecs[i].def);
}

void NDXProcessor::applyFactoryPreset(int index) {
    if (index < 0 || index >= ndx::kNumFactoryPresets) return;
    const ndx::PresetDef& def = ndx::kFactoryPresets[index];
    resetToDefaults();
    for (int k = 0; k < def.count; ++k) setPlain(def.values[k].id, def.values[k].value);
    apvts.state.setProperty("preset", def.name, nullptr);
}

juce::File NDXProcessor::getUserPresetDir() const {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("NDX").getChildFile("Presets");
}

juce::StringArray NDXProcessor::getUserPresetNames() const {
    juce::StringArray names;
    for (const auto& f : getUserPresetDir().findChildFiles(juce::File::findFiles, false, "*.ndxpreset"))
        names.add(f.getFileNameWithoutExtension());
    names.sort(true);
    return names;
}

bool NDXProcessor::saveUserPreset(const juce::String& rawName) {
    const juce::String name = juce::File::createLegalFileName(rawName.trim());
    if (name.isEmpty()) return false;
    const auto dir = getUserPresetDir();
    if (!dir.createDirectory()) return false;
    auto state = apvts.copyState();
    state.setProperty("preset", name, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (!xml || !xml->writeTo(dir.getChildFile(name + ".ndxpreset"))) return false;
    apvts.state.setProperty("preset", name, nullptr);
    return true;
}

bool NDXProcessor::loadUserPreset(const juce::String& name) {
    const auto file = getUserPresetDir().getChildFile(name + ".ndxpreset");
    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (!xml || !xml->hasTagName(apvts.state.getType())) return false;
    const auto loaded = juce::ValueTree::fromXml(*xml);
    for (unsigned i = 0; i < ndx::kNumParams; ++i) {
        if (i == ndx::kGainReduction) continue;
        const juce::String id = paramId(i);
        auto child = loaded.getChildWithProperty("id", id);
        if (child.isValid()) setPlain(i, (double)child.getProperty("value"));
    }
    apvts.state.setProperty("preset", name, nullptr);
    return true;
}

bool NDXProcessor::deleteUserPreset(const juce::String& name) {
    return getUserPresetDir().getChildFile(name + ".ndxpreset").deleteFile();
}

bool NDXProcessor::loadNeuralWeights(const juce::File& file) {
    return engine.loadWeights(file.getFullPathName().toStdString());
}

}  // namespace ndxjuce

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ndxjuce::NDXProcessor(); }
