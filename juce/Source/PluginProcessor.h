#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "ndx_dsp.h"
#include "ndx_presets.h"

namespace ndxjuce {

inline juce::String paramId(unsigned i) { return juce::String(ndx::kSpecs[i].name).removeCharacters(" "); }

class NDXProcessor final : public juce::AudioProcessor {
public:
    NDXProcessor();
    ~NDXProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    // The engine implements its own latency-compensated, click-free bypass (reads the Bypass parameter),
    // so the host's bypass path must run through processBlock as well.
    void processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer& m) override { processBlock(b, m); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "N-DX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    juce::AudioProcessorParameter* getBypassParameter() const override;

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // ---- presets (message thread only)
    juce::String getCurrentPresetName() const;
    void applyFactoryPreset(int index);
    void resetToDefaults();
    juce::File getUserPresetDir() const;
    juce::StringArray getUserPresetNames() const;
    bool saveUserPreset(const juce::String& name);
    bool loadUserPreset(const juce::String& name);
    bool deleteUserPreset(const juce::String& name);
    bool loadNeuralWeights(const juce::File& file);
    bool neuralWeightsLoaded() const { return engine.hasWeights(); }

    // ---- meters / analyser feed for the editor (thread-safe reads)
    const ndx::NDXEngine& getEngine() const { return engine; }
    int popAnalyserSamples(float* dest, int maxSamples);
    int getLatency() const { return engine.latencySamples(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setPlain(unsigned id, double plain);

    ndx::NDXEngine engine;
    std::atomic<float>* raw[ndx::kNumParams] = {};

    static constexpr int kFifoSize = 16384;
    juce::AbstractFifo fifo{kFifoSize};
    std::vector<float> fifoBuffer = std::vector<float>(kFifoSize, 0.f);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NDXProcessor)
};

}  // namespace ndxjuce
