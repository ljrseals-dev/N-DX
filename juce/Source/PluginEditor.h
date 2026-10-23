#pragma once
#include "NDXUI.h"
#include "PluginProcessor.h"

namespace ndxjuce {

// Resolution-independent editor: the UI is laid out in a fixed 900x680 logical space and scaled.
class NDXEditor final : public juce::AudioProcessorEditor {
public:
    explicit NDXEditor(NDXProcessor&);
    ~NDXEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int kW = 900, kH = 680;

private:
    class Content;
    NDXProcessor& proc;
    std::unique_ptr<Content> content;
    juce::ComponentBoundsConstrainer constrainer;
    juce::TooltipWindow tooltip{this, 600};

    void setUiScale(double s);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NDXEditor)
};

}  // namespace ndxjuce
