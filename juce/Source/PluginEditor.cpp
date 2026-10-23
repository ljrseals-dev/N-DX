#include "PluginEditor.h"

using namespace ndxui;

namespace ndxjuce {

namespace {

float toDb(float lin) { return 20.0f * std::log10(juce::jmax(lin, 1.0e-6f)); }

// ------------------------------------------------------------------ vector icon buttons
class IconButton final : public juce::Button {
public:
    enum class Kind { Gear, User };
    IconButton(Kind k, const juce::String& tip) : juce::Button(tip), kind(k) { setTooltip(tip); setTitle(tip); }
    void paintButton(juce::Graphics& g, bool over, bool down) override {
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(down ? col::cyan.withAlpha(0.25f) : (over ? col::cyan.withAlpha(0.12f) : col::panel));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour(over ? col::cyan : col::edge);
        g.drawRoundedRectangle(r, 7.0f, 1.0f);
        g.setColour(over ? col::text : col::dim);
        const auto c = r.getCentre();
        if (kind == Kind::Gear) {
            juce::Path p;
            p.addStar(c, 8, 5.2f, 8.2f, 0.0f);
            p.addEllipse(c.x - 2.8f, c.y - 2.8f, 5.6f, 5.6f);
            p.setUsingNonZeroWinding(false);
            g.fillPath(p);
        } else {
            g.fillEllipse(juce::Rectangle<float>(7.5f, 7.5f).withCentre({c.x, c.y - 3.5f}));
            juce::Path body;
            body.addCentredArc(c.x, c.y + 8.5f, 7.5f, 6.5f, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
            body.addCentredArc(c.x, c.y + 8.5f, 7.5f, 6.5f, 0.0f, juce::MathConstants<float>::halfPi, juce::MathConstants<float>::pi * 1.5f, false);
            g.fillPath(body);
        }
    }

private:
    Kind kind;
};

// ------------------------------------------------------------------ preset bar
class PresetBar final : public juce::Component {
public:
    std::function<void()> onMenu, onPrev, onNext, onSave;
    void setName(const juce::String& n) { if (n != name) { name = n; repaint(); } }

    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff121927), 0, 0, juce::Colour(0xff090d14), 0, r.getBottom(), false));
        g.fillRoundedRectangle(r, 8.0f);
        g.setColour(col::edge);
        g.drawRoundedRectangle(r, 8.0f, 1.0f);
        g.setColour(col::text);
        g.setFont(font(13.0f));
        g.drawText(name, nameArea().toFloat(), juce::Justification::centred, true);
        g.setColour(col::dim);
        juce::Path down;
        const auto na = nameArea().toFloat();
        down.addTriangle(na.getRight() - 16, na.getCentreY() - 2, na.getRight() - 8, na.getCentreY() - 2, na.getRight() - 12, na.getCentreY() + 3);
        g.fillPath(down);
        chevron(g, prevArea(), true);
        chevron(g, nextArea(), false);
        auto s = saveArea().toFloat().reduced(1.0f, 4.0f);
        g.setColour(col::blue.withAlpha(0.22f));
        g.fillRoundedRectangle(s, 5.0f);
        g.setColour(col::cyan.withAlpha(0.7f));
        g.drawRoundedRectangle(s, 5.0f, 1.0f);
        g.setColour(col::text);
        g.setFont(spaced(10.5f, true, 0.1f));
        g.drawText("SAVE", s, juce::Justification::centred);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        const auto p = e.getPosition();
        if (nameArea().contains(p) && onMenu) onMenu();
        else if (prevArea().contains(p) && onPrev) onPrev();
        else if (nextArea().contains(p) && onNext) onNext();
        else if (saveArea().contains(p) && onSave) onSave();
    }

private:
    juce::Rectangle<int> nameArea() const { return getLocalBounds().withTrimmedRight(116); }
    juce::Rectangle<int> prevArea() const { return {getWidth() - 114, 0, 28, getHeight()}; }
    juce::Rectangle<int> nextArea() const { return {getWidth() - 86, 0, 28, getHeight()}; }
    juce::Rectangle<int> saveArea() const { return {getWidth() - 56, 0, 56, getHeight()}; }
    static void chevron(juce::Graphics& g, juce::Rectangle<int> a, bool left) {
        const auto c = a.toFloat().getCentre();
        juce::Path p;
        const float d = left ? -1.0f : 1.0f;
        p.startNewSubPath(c.x - 2.5f * d, c.y - 5.0f);
        p.lineTo(c.x + 2.5f * d, c.y);
        p.lineTo(c.x - 2.5f * d, c.y + 5.0f);
        g.setColour(col::dim);
        g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    juce::String name;
};

// ------------------------------------------------------------------ tab slot wrappers
class LabeledChoice final : public juce::Component {
public:
    LabeledChoice(juce::AudioProcessorValueTreeState& apvts, unsigned id, const juce::String& title) {
        const auto& s = ndx::kSpecs[id];
        juce::StringArray items;
        for (int i = 0; i < s.numItems; ++i) items.add(s.items[i]);
        box.addItemList(items, 1);
        box.setTitle(title);
        addAndMakeVisible(box);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramId(id), box);
        label.setText(title.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(spaced(9.5f, true, 0.1f));
        label.setColour(juce::Label::textColourId, col::dim);
        label.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(label);
    }
    void resized() override {
        auto r = getLocalBounds();
        r.removeFromBottom(16);
        label.setBounds(r.removeFromBottom(14));
        box.setBounds(r.withSizeKeepingCentre(juce::jmin(r.getWidth() - 6, 104), 26));
    }
private:
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

class LabeledToggle final : public juce::Component {
public:
    LabeledToggle(juce::AudioProcessorValueTreeState& apvts, unsigned id, const juce::String& title)
        : toggle(apvts, id, "", ParamToggle::Style::Switch) {
        addAndMakeVisible(toggle);
        label.setText(title.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(spaced(9.5f, true, 0.1f));
        label.setColour(juce::Label::textColourId, col::dim);
        label.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(label);
    }
    void resized() override {
        auto r = getLocalBounds();
        r.removeFromBottom(16);
        label.setBounds(r.removeFromBottom(14));
        toggle.setBounds(r.withSizeKeepingCentre(38, 22));
    }
private:
    ParamToggle toggle;
    juce::Label label;
};

class ActionButton final : public juce::Component {
public:
    ActionButton(const juce::String& title, const juce::String& buttonText, std::function<void()> fn) {
        button.setButtonText(buttonText);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff10305f));
        button.setColour(juce::TextButton::textColourOffId, col::cyan);
        button.onClick = std::move(fn);
        button.setTooltip("Listens to the incoming audio for 3 seconds, then suggests Threshold, Ratio and Attack");
        addAndMakeVisible(button);
        label.setText(title.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(spaced(9.5f, true, 0.1f));
        label.setColour(juce::Label::textColourId, col::dim);
        label.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(label);
    }
    void resized() override {
        auto r = getLocalBounds();
        r.removeFromBottom(16);
        label.setBounds(r.removeFromBottom(14));
        button.setBounds(r.withSizeKeepingCentre(juce::jmin(r.getWidth() - 10, 92), 28));
    }
    juce::TextButton button;
private:
    juce::Label label;
};

// ------------------------------------------------------------------ input / output strip
class IoStrip final : public juce::Component {
public:
    IoStrip(juce::AudioProcessorValueTreeState& apvts, const juce::String& title, unsigned knobParam,
            const juce::String& knobTitle, bool scaleRight)
        : titleText(title), meter(scaleRight), knob(apvts, knobParam, knobTitle) {
        readout.setJustificationType(juce::Justification::centred);
        readout.setFont(font(15.0f, true));
        readout.setColour(juce::Label::textColourId, col::text);
        readout.setText("-inf", juce::dontSendNotification);
        readout.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(readout);
        addAndMakeVisible(meter);
        addAndMakeVisible(knob);
    }
    void update(float l, float r, float dt) {
        meter.update(toDb(l), toDb(r), dt);
        const float p = meter.peakDb();
        readout.setText(p < -80.0f ? "-inf" : juce::String(p, 1) + " dB", juce::dontSendNotification);
    }
    void paint(juce::Graphics& g) override {
        drawPanel(g, getLocalBounds().toFloat(), 10.0f);
        drawPanelTitle(g, getLocalBounds().toFloat(), titleText);
    }
    void resized() override {
        auto r = getLocalBounds().reduced(8, 6);
        r.removeFromTop(16);
        readout.setBounds(r.removeFromTop(24));
        knob.setBounds(r.removeFromBottom(92));
        meter.setBounds(r.reduced(2, 4));
    }
private:
    juce::String titleText;
    juce::Label readout;
    LevelMeter meter;
    KnobWidget knob;
};

constexpr int kFftOrder = 11, kFftSize = 1 << kFftOrder;

}  // namespace

// ====================================================================== Content
class NDXEditor::Content final : public juce::Component, private juce::Timer {
public:
    Content(NDXProcessor& p, std::function<void(double)> scaleCallback)
        : proc(p), apvts(p.apvts), onScale(std::move(scaleCallback)), fft(kFftOrder),
          window((size_t)kFftSize, juce::dsp::WindowingFunction<float>::hann, false), tabs(juce::StringArray{"Dynamics", "Spectral", "Neural", "Character", "Master"}) {
        setLookAndFeel(&lf);
        setOpaque(true);

        inStrip = std::make_unique<IoStrip>(apvts, "Input", ndx::kInputTrim, "Trim", true);
        outStrip = std::make_unique<IoStrip>(apvts, "Output", ndx::kMakeup, "Makeup", false);
        field = std::make_unique<FieldDisplay>(apvts);
        addAndMakeVisible(*inStrip);
        addAndMakeVisible(*outStrip);
        addAndMakeVisible(*field);
        addAndMakeVisible(presetBar);
        addAndMakeVisible(tabs);

        // header controls
        for (int i = 0; i < ndx::kSpecs[ndx::kMode].numItems; ++i) modeBox.addItem(ndx::kSpecs[ndx::kMode].items[i], i + 1);
        modeBox.setTitle("Mode");
        modeBox.setTooltip("Operating mode");
        addAndMakeVisible(modeBox);
        modeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramId(ndx::kMode), modeBox);
        bypass = std::make_unique<ParamToggle>(apvts, ndx::kBypass, "Bypass", ParamToggle::Style::Pill);
        addAndMakeVisible(*bypass);
        userBtn = std::make_unique<IconButton>(IconButton::Kind::User, "Presets menu");
        gearBtn = std::make_unique<IconButton>(IconButton::Kind::Gear, "Settings");
        addAndMakeVisible(*userBtn);
        addAndMakeVisible(*gearBtn);

        presetBar.setName(proc.getCurrentPresetName());
        presetBar.onMenu = [this] { showPresetMenu(); };
        presetBar.onPrev = [this] { stepPreset(-1); };
        presetBar.onNext = [this] { stepPreset(1); };
        presetBar.onSave = [this] { promptSave(); };
        userBtn->onClick = [this] { showUserMenu(); };
        gearBtn->onClick = [this] { showSettingsMenu(); };

        buildTabs();
        buildBottomPanels();
        tabs.onChange = [this](int) { layoutSlots(); };

        row.fill(0.0f);
        smoothRow.fill(0.0f);
        ring.fill(0.0f);
        scopeHist.assign(120, 0.0f);

        statusText = "READY";
        resized();
        startTimerHz(30);
    }

    ~Content() override {
        stopTimer();
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(col::bg);
        g.setGradientFill(juce::ColourGradient(col::blue.withAlpha(0.12f), 120.0f, -20.0f, juce::Colours::transparentBlack, 120.0f, 260.0f, true));
        g.fillRect(getLocalBounds());
        g.setGradientFill(juce::ColourGradient(col::violet.withAlpha(0.13f), 450.0f, 720.0f, juce::Colours::transparentBlack, 450.0f, 450.0f, true));
        g.fillRect(getLocalBounds());

        // logo
        juce::ColourGradient logo(col::blue, 16.0f, 0.0f, col::pink, 150.0f, 0.0f, false);
        logo.addColour(0.5, col::violet);
        g.setGradientFill(logo);
        g.setFont(font(36.0f, true).withExtraKerningFactor(0.05f));
        g.drawText("N-DX", juce::Rectangle<int>(16, 6, 190, 42), juce::Justification::centredLeft, false);
        g.setColour(col::dim);
        g.setFont(spaced(8.5f, true, 0.32f));
        g.drawText("NEURAL DYNAMICS ENGINE", juce::Rectangle<int>(18, 46, 230, 12), juce::Justification::centredLeft, false);

        drawPanel(g, knobPanel.toFloat(), 10.0f);
        const char* names[] = {"Detector", "Sidechain", "Spectral Dynamics", "M/S", "Oversampling", "Saturation"};
        for (int i = 0; i < 6; ++i) {
            drawPanel(g, bottomPanels[i].toFloat(), 10.0f);
            drawPanelTitle(g, bottomPanels[i].toFloat(), names[i]);
        }
        // footer
        g.setColour(col::faint);
        g.setFont(font(10.0f));
        g.drawText(statusText, juce::Rectangle<int>(18, 662, 520, 14), juce::Justification::centredLeft, false);
        g.drawText(latencyText, juce::Rectangle<int>(540, 662, 344, 14), juce::Justification::centredRight, false);
    }

    void resized() override {
        presetBar.setBounds(250, 14, 380, 32);
        modeBox.setBounds(640, 16, 108, 28);
        bypass->setBounds(756, 16, 68, 28);
        userBtn->setBounds(832, 16, 28, 28);
        gearBtn->setBounds(864, 16, 28, 28);

        inStrip->setBounds(16, 70, 96, 302);
        field->setBounds(122, 70, 656, 302);
        outStrip->setBounds(788, 70, 96, 302);
        tabs.setBounds(122, 380, 656, 26);
        knobPanel = {16, 412, 868, 112};

        const int widths[6] = {124, 164, 176, 112, 112, 156};
        const float gap = (868.0f - 844.0f) / 5.0f;
        float x = 16.0f;
        for (int i = 0; i < 6; ++i) {
            bottomPanels[i] = {(int)std::lround(x), 534, widths[i], 124};
            x += (float)widths[i] + gap;
        }
        layoutBottom();
        layoutSlots();
    }

private:
    // ------------------------------------------------------------ construction helpers
    void addSlot(std::unique_ptr<juce::Component> c, int tab) {
        addChildComponent(*c);
        slots.push_back({std::move(c), tab});
    }

    void buildTabs() {
        auto knob = [this](unsigned id, const char* title, int tab) { addSlot(std::make_unique<KnobWidget>(apvts, id, title), tab); };
        knob(ndx::kThreshold, "Threshold", 0); knob(ndx::kRatio, "Ratio", 0); knob(ndx::kAttack, "Attack", 0);
        knob(ndx::kRelease, "Release", 0); knob(ndx::kKnee, "Knee", 0); knob(ndx::kLookahead, "Lookahead", 0);

        knob(ndx::kSpectral, "Spectral", 1); knob(ndx::kXoverLow, "Low Xover", 1); knob(ndx::kXoverHigh, "High Xover", 1);
        knob(ndx::kLowProtect, "Low Protect", 1);
        addSlot(std::make_unique<LabeledChoice>(apvts, ndx::kSpecWeight, "Weighting"), 1);

        knob(ndx::kNeural, "Neural Amount", 2);
        addSlot(std::make_unique<LabeledChoice>(apvts, ndx::kIntent, "Intent"), 2);
        addSlot(std::make_unique<LabeledToggle>(apvts, ndx::kAutoThreshold, "Auto Threshold"), 2);
        addSlot(std::make_unique<LabeledToggle>(apvts, ndx::kAutoMakeup, "Auto Makeup"), 2);
        knob(ndx::kRestore, "Restore", 2);
        auto analyze = std::make_unique<ActionButton>("Analyze", "ANALYZE", [this] { startAnalyze(); });
        analyzeButton = analyze.get();
        addSlot(std::move(analyze), 2);

        knob(ndx::kCharacter, "Character", 3); knob(ndx::kSaturation, "Saturation", 3);

        knob(ndx::kMix, "Mix", 4); knob(ndx::kHold, "Hold", 4); knob(ndx::kOutput, "Output", 4); knob(ndx::kCeiling, "Ceiling", 4);
        addSlot(std::make_unique<LabeledToggle>(apvts, ndx::kSafety, "Safety Ceiling"), 4);
        addSlot(std::make_unique<LabeledChoice>(apvts, ndx::kSidechain, "Sidechain"), 4);
    }

    void buildBottomPanels() {
        // detector
        detRadio = std::make_unique<ParamRadio>(apvts, ndx::kDetector);
        detScope = std::make_unique<LambdaView>();
        detScope->painter = [this](juce::Graphics& g, juce::Rectangle<float> r) {
            juce::Path p;
            const int n = (int)scopeHist.size();
            for (int i = 0; i < n; ++i) {
                const float v = juce::jlimit(0.0f, 1.0f, (toDb(scopeHist[(size_t)i]) + 54.0f) / 54.0f);
                const float x = r.getX() + r.getWidth() * (float)i / (float)(n - 1);
                const float y = r.getCentreY() - v * r.getHeight() * 0.5f;
                if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
            }
            for (int i = n - 1; i >= 0; --i) {
                const float v = juce::jlimit(0.0f, 1.0f, (toDb(scopeHist[(size_t)i]) + 54.0f) / 54.0f);
                p.lineTo(r.getX() + r.getWidth() * (float)i / (float)(n - 1), r.getCentreY() + v * r.getHeight() * 0.5f);
            }
            p.closeSubPath();
            g.setColour(col::cyan.withAlpha(0.22f));
            g.fillPath(p);
            g.setColour(col::cyan);
            g.strokePath(p, juce::PathStrokeType(1.0f));
        };
        // sidechain
        scToggle = std::make_unique<ParamToggle>(apvts, ndx::kScFilter, "Filter", ParamToggle::Style::Dot);
        hpfBar = std::make_unique<ParamBar>(apvts, ndx::kScHpf, "HPF");
        lpfBar = std::make_unique<ParamBar>(apvts, ndx::kScLpf, "LPF");
        scGraph = std::make_unique<LambdaView>();
        scGraph->painter = [this](juce::Graphics& g, juce::Rectangle<float> r) {
            g.setColour(juce::Colour(0xff080b12));
            g.fillRoundedRectangle(r, 4.0f);
            const bool on = apvts.getRawParameterValue(paramId(ndx::kScFilter))->load() > 0.5f;
            const float hp = apvts.getRawParameterValue(paramId(ndx::kScHpf))->load();
            const float lp = apvts.getRawParameterValue(paramId(ndx::kScLpf))->load();
            juce::Path p;
            for (int i = 0; i <= 120; ++i) {
                const float f = 20.0f * std::pow(1000.0f, (float)i / 120.0f);
                float mag = 1.0f;
                if (on) {
                    mag /= std::sqrt(1.0f + std::pow(hp / f, 4.0f));
                    mag /= std::sqrt(1.0f + std::pow(f / lp, 4.0f));
                }
                const float db = juce::jlimit(-30.0f, 3.0f, 20.0f * std::log10(juce::jmax(mag, 1.0e-4f)));
                const float x = r.getX() + r.getWidth() * (float)i / 120.0f;
                const float y = r.getBottom() - 3.0f - (db + 30.0f) / 33.0f * (r.getHeight() - 8.0f);
                if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
            }
            juce::Path fill = p;
            fill.lineTo(r.getRight(), r.getBottom());
            fill.lineTo(r.getX(), r.getBottom());
            fill.closeSubPath();
            g.setGradientFill(juce::ColourGradient((on ? col::blue : col::faint).withAlpha(0.45f), 0, r.getY(), juce::Colours::transparentBlack, 0, r.getBottom(), false));
            g.fillPath(fill);
            g.setColour(on ? col::cyan : col::faint);
            g.strokePath(p, juce::PathStrokeType(1.5f));
        };
        // spectral
        specGraph = std::make_unique<LambdaView>();
        specGraph->painter = [this](juce::Graphics& g, juce::Rectangle<float> r) {
            g.setColour(juce::Colour(0xff080b12));
            g.fillRoundedRectangle(r, 4.0f);
            const float xl = apvts.getRawParameterValue(paramId(ndx::kXoverLow))->load();
            const float xh = apvts.getRawParameterValue(paramId(ndx::kXoverHigh))->load();
            auto fx = [&](float f) { return r.getX() + r.getWidth() * std::log10(f / 20.0f) / 3.0f; };
            g.setColour(col::edge);
            g.drawVerticalLine((int)fx(xl), r.getY() + 2, r.getBottom() - 2);
            g.drawVerticalLine((int)fx(xh), r.getY() + 2, r.getBottom() - 2);
            const float cx[3] = {fx(std::sqrt(20.0f * xl)), fx(std::sqrt(xl * xh)), fx(std::sqrt(xh * 20000.0f))};
            auto yOf = [&](float gr) { return r.getBottom() - 4.0f - juce::jlimit(0.0f, 1.0f, gr / 18.0f) * (r.getHeight() - 10.0f); };
            const float yy[3] = {yOf(bandGr[0]), yOf(bandGr[1]), yOf(bandGr[2])};
            juce::Path p;
            p.startNewSubPath(r.getX(), yy[0]);
            p.lineTo(cx[0], yy[0]);
            p.cubicTo((cx[0] + cx[1]) * 0.5f, yy[0], (cx[0] + cx[1]) * 0.5f, yy[1], cx[1], yy[1]);
            p.cubicTo((cx[1] + cx[2]) * 0.5f, yy[1], (cx[1] + cx[2]) * 0.5f, yy[2], cx[2], yy[2]);
            p.lineTo(r.getRight(), yy[2]);
            juce::Path fill = p;
            fill.lineTo(r.getRight(), r.getBottom());
            fill.lineTo(r.getX(), r.getBottom());
            fill.closeSubPath();
            g.setGradientFill(juce::ColourGradient(col::violet.withAlpha(0.55f), 0, r.getY(), col::blue.withAlpha(0.10f), 0, r.getBottom(), false));
            g.fillPath(fill);
            g.setColour(col::cyan);
            g.strokePath(p, juce::PathStrokeType(1.5f));
        };
        for (int i = 0; i < ndx::kSpecs[ndx::kSpecWeight].numItems; ++i) specCombo.addItem(ndx::kSpecs[ndx::kSpecWeight].items[i], i + 1);
        specCombo.setTitle("Spectral weighting");
        specCombo.setTooltip("How the three bands share the gain reduction");
        specAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramId(ndx::kSpecWeight), specCombo);
        // M/S
        msToggle = std::make_unique<ParamToggle>(apvts, ndx::kStereoMode, "Mid/Side", ParamToggle::Style::Switch);
        linkKnob = std::make_unique<KnobWidget>(apvts, ndx::kLink, "Stereo Link");
        // oversampling
        osRadio = std::make_unique<ParamRadio>(apvts, ndx::kOversample);
        // saturation
        satKnob = std::make_unique<KnobWidget>(apvts, ndx::kSaturation, "Drive");
        satScope = std::make_unique<LambdaView>();
        satScope->painter = [this](juce::Graphics& g, juce::Rectangle<float> r) {
            g.setColour(juce::Colour(0xff080b12));
            g.fillRoundedRectangle(r, 4.0f);
            const float amt = apvts.getRawParameterValue(paramId(ndx::kSaturation))->load() / 100.0f;
            const double drv = 1.0 + 7.0 * amt, blend = juce::jlimit(0.0f, 1.0f, amt * 4.0f);
            juce::Path in, outp;
            for (int i = 0; i <= 80; ++i) {
                const double x = 0.9 * std::sin(2.0 * ndx::kPi * i / 80.0);
                const double s = std::tanh(drv * x) / drv;
                const double y = amt > 0.001f ? x + (s - x) * blend : x;
                const float px = r.getX() + r.getWidth() * (float)i / 80.0f;
                const float cy = r.getCentreY(), hh = r.getHeight() * 0.42f;
                if (i == 0) { in.startNewSubPath(px, cy - (float)x * hh); outp.startNewSubPath(px, cy - (float)y * hh); }
                else { in.lineTo(px, cy - (float)x * hh); outp.lineTo(px, cy - (float)y * hh); }
            }
            g.setColour(col::faint);
            g.strokePath(in, juce::PathStrokeType(1.0f));
            g.setColour(col::cyan);
            g.strokePath(outp, juce::PathStrokeType(1.6f));
        };

        for (juce::Component* c : std::initializer_list<juce::Component*>{
                 detRadio.get(), detScope.get(), scToggle.get(), hpfBar.get(), lpfBar.get(), scGraph.get(), specGraph.get(), &specCombo,
                 msToggle.get(), linkKnob.get(), osRadio.get(), satKnob.get(), satScope.get()})
            addAndMakeVisible(*c);
    }

    void layoutBottom() {
        auto at = [](juce::Rectangle<int> p, int x, int y, int w, int h) { return juce::Rectangle<int>(p.getX() + x, p.getY() + y, w, h); };
        detRadio->setBounds(at(bottomPanels[0], 12, 26, 100, 56));
        detScope->setBounds(at(bottomPanels[0], 8, 86, 108, 30));
        scToggle->setBounds(at(bottomPanels[1], 10, 22, 80, 18));
        scGraph->setBounds(at(bottomPanels[1], 8, 44, 148, 40));
        hpfBar->setBounds(at(bottomPanels[1], 8, 92, 72, 22));
        lpfBar->setBounds(at(bottomPanels[1], 84, 92, 72, 22));
        specGraph->setBounds(at(bottomPanels[2], 8, 24, 160, 58));
        specCombo.setBounds(at(bottomPanels[2], 8, 90, 160, 26));
        msToggle->setBounds(at(bottomPanels[3], 10, 24, 92, 22));
        linkKnob->setBounds(at(bottomPanels[3], 14, 50, 84, 72));
        osRadio->setBounds(at(bottomPanels[4], 14, 28, 88, 88));
        satKnob->setBounds(at(bottomPanels[5], 6, 22, 82, 98));
        satScope->setBounds(at(bottomPanels[5], 92, 30, 56, 80));
    }

    void layoutSlots() {
        const int tab = tabs.getSelected();
        int count = 0;
        for (auto& s : slots) if (s.tab == tab) ++count;
        const int slotW = 108, total = count * slotW;
        int x = knobPanel.getCentreX() - total / 2;
        for (auto& s : slots) {
            const bool vis = s.tab == tab;
            s.comp->setVisible(vis);
            if (vis) { s.comp->setBounds(x, knobPanel.getY() + 8, slotW, knobPanel.getHeight() - 14); x += slotW; }
        }
    }

    // ------------------------------------------------------------ presets and menus
    juce::StringArray allPresetNames() const {
        juce::StringArray n;
        for (int i = 0; i < ndx::kNumFactoryPresets; ++i) n.add(ndx::kFactoryPresets[i].name);
        n.addArray(proc.getUserPresetNames());
        return n;
    }
    void loadPresetIndex(int i) {
        if (i < ndx::kNumFactoryPresets) proc.applyFactoryPreset(i);
        else proc.loadUserPreset(proc.getUserPresetNames()[i - ndx::kNumFactoryPresets]);
        presetBar.setName(proc.getCurrentPresetName());
    }
    void stepPreset(int dir) {
        const auto names = allPresetNames();
        if (names.isEmpty()) return;
        int cur = names.indexOf(proc.getCurrentPresetName());
        if (cur < 0) cur = dir > 0 ? -1 : 0;
        loadPresetIndex((cur + dir + names.size()) % names.size());
    }
    void showPresetMenu() {
        juce::PopupMenu m;
        m.addSectionHeader("Factory presets");
        for (int i = 0; i < ndx::kNumFactoryPresets; ++i)
            m.addItem(1 + i, ndx::kFactoryPresets[i].name, true, proc.getCurrentPresetName() == ndx::kFactoryPresets[i].name);
        const auto user = proc.getUserPresetNames();
        if (!user.isEmpty()) {
            m.addSeparator();
            m.addSectionHeader("User presets");
            for (int i = 0; i < user.size(); ++i) m.addItem(1000 + i, user[i], true, proc.getCurrentPresetName() == user[i]);
        }
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetBar), [safe = juce::Component::SafePointer<Content>(this)](int r) {
            if (safe == nullptr || r == 0) return;
            safe->loadPresetIndex(r >= 1000 ? ndx::kNumFactoryPresets + (r - 1000) : r - 1);
        });
    }
    void promptSave() {
        auto* w = new juce::AlertWindow("Save preset", "Name your preset", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor("name", proc.getCurrentPresetName());
        w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Content>(this), w](int r) {
            if (r != 1 || safe == nullptr) return;
            const auto name = w->getTextEditorContents("name");
            safe->setStatus(safe->proc.saveUserPreset(name) ? "PRESET SAVED: " + name.toUpperCase() : juce::String("COULD NOT SAVE PRESET"));
            safe->presetBar.setName(safe->proc.getCurrentPresetName());
        }), true);
    }
    void showUserMenu() {
        juce::PopupMenu m;
        m.addItem(1, "Save preset...");
        m.addItem(2, "Reset all parameters to defaults");
        const auto user = proc.getUserPresetNames();
        juce::PopupMenu del;
        for (int i = 0; i < user.size(); ++i) del.addItem(100 + i, user[i]);
        m.addSubMenu("Delete user preset", del, !user.isEmpty());
        m.addSeparator();
        m.addItem(3, "Reveal user preset folder");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(userBtn.get()), [safe = juce::Component::SafePointer<Content>(this), user](int r) {
            if (safe == nullptr || r == 0) return;
            if (r == 1) safe->promptSave();
            else if (r == 2) { safe->proc.resetToDefaults(); safe->proc.apvts.state.setProperty("preset", "Init (Hybrid)", nullptr); safe->setStatus("PARAMETERS RESET"); }
            else if (r == 3) { safe->proc.getUserPresetDir().createDirectory(); safe->proc.getUserPresetDir().revealToUser(); }
            else if (r >= 100) { safe->proc.deleteUserPreset(user[r - 100]); safe->setStatus("DELETED PRESET: " + user[r - 100].toUpperCase()); }
        });
    }
    void showSettingsMenu() {
        juce::PopupMenu m, scale;
        const double cur = (double)proc.apvts.state.getProperty("uiScale", 1.0);
        const double vals[] = {0.75, 1.0, 1.25, 1.5};
        for (int i = 0; i < 4; ++i) scale.addItem(10 + i, juce::String((int)std::lround(vals[i] * 100)) + "%", true, std::abs(cur - vals[i]) < 0.01);
        m.addSubMenu("Interface size", scale);
        m.addItem(1, "Load neural weights file...");
        m.addItem(2, proc.neuralWeightsLoaded() ? "Neural weights: trained file loaded" : "Neural weights: none (intent + scene prior)", false);
        m.addSeparator();
        m.addItem(3, "About N-DX");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(gearBtn.get()), [safe = juce::Component::SafePointer<Content>(this), vals](int r) {
            if (safe == nullptr || r == 0) return;
            if (r >= 10 && r < 14) safe->onScale(vals[r - 10]);
            else if (r == 1) safe->chooseWeights();
            else if (r == 3)
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "N-DX Neural Dynamics Engine",
                    "Version 1.0.0\nCompression that listens.\n\nA bounded neural/intent controller steers a deterministic DSP core.\n"
                    "VST is a trademark of Steinberg Media Technologies GmbH.");
        });
    }
    void chooseWeights() {
        chooser = std::make_unique<juce::FileChooser>("Select an ndx_weights.bin file", juce::File(), "*.bin");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [safe = juce::Component::SafePointer<Content>(this)](const juce::FileChooser& fc) {
                                 if (safe == nullptr) return;
                                 const auto f = fc.getResult();
                                 if (!f.existsAsFile()) return;
                                 safe->setStatus(safe->proc.loadNeuralWeights(f) ? "NEURAL WEIGHTS LOADED: " + f.getFileName().toUpperCase()
                                                                                  : juce::String("NOT A VALID N-DX WEIGHTS FILE"));
                             });
    }

    // ------------------------------------------------------------ analyze
    void startAnalyze() {
        analyzeLevels.clear();
        analyzeTicks = 90;  // 3 seconds at 30 Hz
        setStatus("ANALYZING INPUT - PLAY AUDIO...");
    }
    void finishAnalyze() {
        std::vector<float> v;
        for (float d : analyzeLevels) if (d > -70.0f) v.push_back(d);
        if (v.size() < 20) { setStatus("ANALYZE: NO SIGNAL DETECTED"); return; }
        std::sort(v.begin(), v.end());
        const float p50 = v[v.size() / 2], p95 = v[(size_t)((float)v.size() * 0.95f)];
        const double thr = juce::jlimit(-60.0f, -2.0f, p50 - 1.0f);
        const double ratio = juce::jlimit(2.0f, 8.0f, 2.0f + (p95 - p50) / 5.0f);
        const double atk = (p95 - p50) > 12.0f ? 20.0 : 8.0;
        auto setP = [this](unsigned id, double val) {
            if (auto* p = apvts.getParameter(paramId(id))) {
                p->beginChangeGesture();
                p->setValueNotifyingHost(p->convertTo0to1((float)val));
                p->endChangeGesture();
            }
        };
        setP(ndx::kThreshold, thr); setP(ndx::kRatio, ratio); setP(ndx::kAttack, atk);
        setStatus("ANALYZED: THRESHOLD " + juce::String(thr, 1) + " dB  /  RATIO " + juce::String(ratio, 1) + ":1  /  ATTACK " + juce::String(atk, 0) + " ms");
    }

    void setStatus(const juce::String& s) { statusText = s; statusHold = 120; repaint(juce::Rectangle<int>(0, 656, getWidth(), 24)); }

    // ------------------------------------------------------------ spectrum for the Compression Field
    void computeSpectrum() {
        for (int i = 0; i < kFftSize; ++i) fftData[(size_t)i] = ring[(size_t)((ringPos + i) & (kFftSize - 1))];
        std::fill(fftData.begin() + kFftSize, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable(fftData.data(), (size_t)kFftSize);
        fft.performFrequencyOnlyForwardTransform(fftData.data());
        const float binHz = (float)juce::jmax(8000.0, proc.getSampleRate()) / (float)kFftSize;
        const int bins = FieldDisplay::kBins;
        for (int b = 0; b < bins; ++b) {
            const float f0 = 20.0f * std::pow(1000.0f, (float)b / (float)bins), f1 = 20.0f * std::pow(1000.0f, (float)(b + 1) / (float)bins);
            const float k0 = f0 / binHz, k1 = f1 / binHz;
            float m = 0.0f;
            if (k1 - k0 < 1.0f) {
                const float kc = 0.5f * (k0 + k1);
                const int i = juce::jlimit(0, kFftSize / 2 - 2, (int)kc);
                const float fr = kc - (float)i;
                m = fftData[(size_t)i] * (1.0f - fr) + fftData[(size_t)i + 1] * fr;
            } else {
                for (int k = (int)std::ceil(k0); k <= (int)std::floor(k1) && k < kFftSize / 2; ++k) m = juce::jmax(m, fftData[(size_t)k]);
            }
            const float db = 20.0f * std::log10(m / ((float)kFftSize * 0.25f) + 1.0e-9f);
            const float v = juce::jlimit(0.0f, 1.0f, (db + 72.0f) / 60.0f);
            smoothRow[(size_t)b] = 0.6f * v + 0.4f * smoothRow[(size_t)b];
            row[(size_t)b] = smoothRow[(size_t)b];
        }
        field->pushRow(row.data());
    }

    void timerCallback() override {
        const float dt = 1.0f / 30.0f;
        const auto& eng = proc.getEngine();
        inStrip->update(eng.inputPeak(0), eng.inputPeak(1), dt);
        outStrip->update(eng.outputPeak(0), eng.outputPeak(1), dt);

        float tmp[4096];
        const int n = proc.popAnalyserSamples(tmp, 4096);
        for (int i = 0; i < n; ++i) { ring[(size_t)(ringPos & (kFftSize - 1))] = tmp[i]; ++ringPos; }
        if (n > 0) computeSpectrum();

        for (int b = 0; b < 3; ++b) bandGr[b] = eng.bandGainReductionDb(b);
        field->setBandGr(bandGr[0], bandGr[1], bandGr[2], eng.gainReductionDb());

        const float pk = juce::jmax(eng.inputPeak(0), eng.inputPeak(1));
        scopeHist.erase(scopeHist.begin());
        scopeHist.push_back(pk);
        detScope->repaint();
        specGraph->repaint();
        scGraph->repaint();
        satScope->repaint();

        if (analyzeTicks > 0) {
            analyzeLevels.push_back(toDb(pk));
            if (--analyzeTicks == 0) finishAnalyze();
        }
        if (statusHold > 0) --statusHold;
        if (presetBar.isVisible()) presetBar.setName(proc.getCurrentPresetName());

        const int latency = proc.getLatency();
        const double sr = juce::jmax(8000.0, proc.getSampleRate());
        juce::String lt = "LATENCY " + juce::String(latency) + " SAMPLES (" + juce::String(1000.0 * latency / sr, 1) + " MS)   /   "
                          + (proc.neuralWeightsLoaded() ? "NEURAL WEIGHTS LOADED" : "INTENT + SCENE CONTROLLER");
        juce::String st = statusHold > 0 ? statusText : juce::String("N-DX 1.0.0   /   ") + proc.getCurrentPresetName().toUpperCase();
        if (lt != latencyText || st != statusText) {
            latencyText = lt;
            if (statusHold == 0) statusText = st;
            repaint(juce::Rectangle<int>(0, 656, getWidth(), 24));
        }
    }

    struct Slot { std::unique_ptr<juce::Component> comp; int tab; };

    NDXLookAndFeel lf;
    NDXProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    std::function<void(double)> onScale;

    // analyser
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, kFftSize> ring{};
    std::array<float, 2 * kFftSize> fftData{};
    std::array<float, FieldDisplay::kBins> row{}, smoothRow{};
    int ringPos = 0;
    std::vector<float> scopeHist;
    float bandGr[3] = {0, 0, 0};
    std::vector<float> analyzeLevels;
    int analyzeTicks = 0, statusHold = 0;
    juce::String statusText, latencyText;

    TabStrip tabs;
    PresetBar presetBar;
    juce::ComboBox modeBox, specCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt, specAtt;
    std::unique_ptr<ParamToggle> bypass, scToggle, msToggle;
    std::unique_ptr<IconButton> userBtn, gearBtn;
    std::unique_ptr<IoStrip> inStrip, outStrip;
    std::unique_ptr<FieldDisplay> field;
    std::vector<Slot> slots;
    ActionButton* analyzeButton = nullptr;
    std::unique_ptr<ParamRadio> detRadio, osRadio;
    std::unique_ptr<LambdaView> detScope, scGraph, specGraph, satScope;
    std::unique_ptr<ParamBar> hpfBar, lpfBar;
    std::unique_ptr<KnobWidget> linkKnob, satKnob;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> knobPanel, bottomPanels[6];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Content)
};

// ====================================================================== editor shell
NDXEditor::NDXEditor(NDXProcessor& p) : AudioProcessorEditor(&p), proc(p) {
    content = std::make_unique<Content>(p, [this](double s) { setUiScale(s); });
    addAndMakeVisible(*content);
    constrainer.setFixedAspectRatio((double)kW / (double)kH);
    constrainer.setSizeLimits(kW * 3 / 4, kH * 3 / 4, kW * 2, kH * 2);
    setConstrainer(&constrainer);
    setResizable(true, true);
    const double s = juce::jlimit(0.75, 1.5, (double)p.apvts.state.getProperty("uiScale", 1.0));
    setSize((int)std::lround(kW * s), (int)std::lround(kH * s));
}

NDXEditor::~NDXEditor() = default;

void NDXEditor::setUiScale(double s) {
    proc.apvts.state.setProperty("uiScale", s, nullptr);
    setSize((int)std::lround(kW * s), (int)std::lround(kH * s));
}

void NDXEditor::paint(juce::Graphics& g) { g.fillAll(col::bg); }

void NDXEditor::resized() {
    content->setTransform(juce::AffineTransform::scale((float)getWidth() / (float)kW));
    content->setBounds(0, 0, kW, kH);
}

}  // namespace ndxjuce
