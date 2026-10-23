// N-DX user interface kit: theme, look-and-feel and reusable widgets.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cstring>
#include <functional>

#include "PluginProcessor.h"

namespace ndxui {

namespace col {
inline const juce::Colour bg{0xff060910}, panel{0xff0d121b}, panelHi{0xff151c2a}, edge{0xff263045};
inline const juce::Colour text{0xffe8f0ff}, dim{0xff8795ae}, faint{0xff4a566c};
inline const juce::Colour cyan{0xff3fd9f2}, blue{0xff2d86ff}, violet{0xff9b6bff}, pink{0xffff5db1};
inline const juce::Colour orange{0xffffa94d}, green{0xff4df0b0}, amber{0xffffc04d};
}  // namespace col

inline juce::Font font(float h, bool bold = false) {
    return juce::Font(juce::FontOptions(h, bold ? juce::Font::bold : juce::Font::plain));
}
inline juce::Font spaced(float h, bool bold, float kerning) { return font(h, bold).withExtraKerningFactor(kerning); }

inline void drawPanel(juce::Graphics& g, juce::Rectangle<float> r, float radius = 10.0f) {
    g.setGradientFill(juce::ColourGradient(col::panelHi, r.getX(), r.getY(), col::panel, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, radius);
    g.setColour(col::edge);
    g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.0f);
}

inline void drawPanelTitle(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t) {
    g.setColour(col::dim);
    g.setFont(spaced(9.5f, true, 0.14f));
    g.drawText(t.toUpperCase(), r.withHeight(18.0f).withTrimmedLeft(10.0f), juce::Justification::centredLeft, false);
}

inline juce::Colour heat(float t) {
    struct Stop { float p; juce::uint32 c; };
    static const Stop st[] = {{0.00f, 0xff050a1e}, {0.22f, 0xff0a2a8c}, {0.42f, 0xff1f7fe6}, {0.58f, 0xff18d4dc},
                              {0.74f, 0xffffb03a}, {0.88f, 0xffff4d78}, {1.00f, 0xffffe6f2}};
    t = juce::jlimit(0.0f, 1.0f, t);
    for (int i = 1; i < 7; ++i)
        if (t <= st[i].p) {
            const float f = (t - st[i - 1].p) / (st[i].p - st[i - 1].p);
            return juce::Colour(st[i - 1].c).interpolatedWith(juce::Colour(st[i].c), f);
        }
    return juce::Colour(st[6].c);
}

// ------------------------------------------------------------------ look and feel
class NDXLookAndFeel : public juce::LookAndFeel_V4 {
public:
    NDXLookAndFeel() {
        setColour(juce::ComboBox::textColourId, col::text);
        setColour(juce::ComboBox::backgroundColourId, col::panel);
        setColour(juce::ComboBox::outlineColourId, col::edge);
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff0f1522));
        setColour(juce::PopupMenu::textColourId, col::text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff1b4f9e));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::Label::textColourId, col::text);
        setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0a0e16));
        setColour(juce::TextEditor::textColourId, col::text);
        setColour(juce::TextEditor::outlineColourId, col::edge);
        setColour(juce::TextEditor::focusedOutlineColourId, col::cyan);
        setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff0f1522));
        setColour(juce::TooltipWindow::textColourId, col::text);
        setColour(juce::TooltipWindow::outlineColourId, col::edge);
        setColour(juce::AlertWindow::backgroundColourId, juce::Colour(0xff0f1522));
        setColour(juce::AlertWindow::textColourId, col::text);
        setColour(juce::AlertWindow::outlineColourId, col::edge);
        setColour(juce::TextButton::buttonColourId, col::panelHi);
        setColour(juce::TextButton::textColourOffId, col::text);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1,
                          juce::Slider& s) override {
        auto b = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h).reduced(2.0f);
        const float d = juce::jmin(b.getWidth(), b.getHeight()), r = d * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        const bool on = s.isEnabled();

        g.setColour(col::cyan.withAlpha(on ? 0.07f : 0.0f));
        g.fillEllipse(juce::Rectangle<float>(d + 8, d + 8).withCentre(c));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2b3548), c.x, c.y - r, juce::Colour(0xff0b0f17), c.x, c.y + r, false));
        g.fillEllipse(juce::Rectangle<float>(d, d).withCentre(c));
        g.setColour(col::edge);
        g.drawEllipse(juce::Rectangle<float>(d, d).withCentre(c), 1.0f);

        const float tr = r - 4.5f;
        juce::Path track;
        track.addCentredArc(c.x, c.y, tr, tr, 0.0f, a0, a1, true);
        g.setColour(juce::Colour(0xff1b2434));
        g.strokePath(track, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc(c.x, c.y, tr, tr, 0.0f, a0, ang, true);
        g.setGradientFill(juce::ColourGradient(col::blue, c.x - r, c.y, col::cyan, c.x + r, c.y, false));
        g.strokePath(val, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float ri = r * 0.64f;
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1e2736), c.x, c.y - ri, juce::Colour(0xff090d14), c.x, c.y + ri, false));
        g.fillEllipse(juce::Rectangle<float>(ri * 2, ri * 2).withCentre(c));
        g.setColour(col::edge.withAlpha(0.8f));
        g.drawEllipse(juce::Rectangle<float>(ri * 2, ri * 2).withCentre(c), 1.0f);

        juce::Path p;
        p.addRoundedRectangle(-1.6f, -ri + 3.0f, 3.2f, ri * 0.5f, 1.5f);
        g.setColour(on ? col::text : col::faint);
        g.fillPath(p, juce::AffineTransform::rotation(ang).translated(c.x, c.y));
    }

    void drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override {
        auto r = juce::Rectangle<float>(0, 0, (float)w, (float)h).reduced(0.5f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff121927), 0, 0, juce::Colour(0xff0a0e16), 0, (float)h, false));
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(box.hasKeyboardFocus(true) ? col::cyan : col::edge);
        g.drawRoundedRectangle(r, 6.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float)w - 13.0f, ay = (float)h * 0.5f;
        arrow.addTriangle(ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour(col::dim);
        g.fillPath(arrow);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return font(12.0f); }
    juce::Font getPopupMenuFont() override { return font(13.0f); }
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override {
        label.setBounds(8, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont(getComboBoxFont(box));
    }
};

// ------------------------------------------------------------------ small helpers
class LambdaView : public juce::Component {
public:
    std::function<void(juce::Graphics&, juce::Rectangle<float>)> painter;
    LambdaView() { setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics& g) override { if (painter) painter(g, getLocalBounds().toFloat()); }
};

inline double plainOf(juce::RangedAudioParameter* p) { return p ? (double)p->convertFrom0to1(p->getValue()) : 0.0; }

// ------------------------------------------------------------------ knob with title + editable value
class KnobWidget : public juce::Component {
public:
    KnobWidget(juce::AudioProcessorValueTreeState& apvts, unsigned id, const juce::String& title) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setTitle(title);
        slider.setTooltip(title + " - drag, mouse-wheel, or double-click to reset");
        addAndMakeVisible(slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ndxjuce::paramId(id), slider);
        slider.setDoubleClickReturnValue(true, ndx::kSpecs[id].def);
        slider.onValueChange = [this] { refresh(); };

        titleLabel.setText(title.toUpperCase(), juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setFont(spaced(9.5f, true, 0.1f));
        titleLabel.setColour(juce::Label::textColourId, col::dim);
        titleLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(titleLabel);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(font(12.0f, true));
        valueLabel.setColour(juce::Label::textColourId, col::text);
        valueLabel.setEditable(false, true, true);
        valueLabel.setTooltip("Double-click to type a value");
        valueLabel.onTextChange = [this] {
            slider.setValue(slider.getValueFromText(valueLabel.getText()), juce::sendNotificationSync);
            refresh();
        };
        addAndMakeVisible(valueLabel);
        refresh();
    }

    void resized() override {
        auto r = getLocalBounds();
        valueLabel.setBounds(r.removeFromBottom(16));
        titleLabel.setBounds(r.removeFromBottom(14));
        slider.setBounds(r);
    }

    juce::Slider slider;

private:
    void refresh() { valueLabel.setText(slider.getTextFromValue(slider.getValue()), juce::dontSendNotification); }
    juce::Label titleLabel, valueLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ------------------------------------------------------------------ on/off, bound to any 2-state parameter
class ParamToggle : public juce::Button {
public:
    enum class Style { Switch, Dot, Pill };

    ParamToggle(juce::AudioProcessorValueTreeState& apvts, unsigned id, const juce::String& label, Style st)
        : juce::Button(label), style(st),
          att(*apvts.getParameter(ndxjuce::paramId(id)), [this](float v) {
              setToggleState(v > 0.5f, juce::dontSendNotification);
              repaint();
          }) {
        setClickingTogglesState(true);
        setTitle(label);
        onClick = [this] { att.setValueAsCompleteGesture(getToggleState() ? 1.0f : 0.0f); };
        att.sendInitialUpdate();
    }

    void paintButton(juce::Graphics& g, bool over, bool) override {
        auto r = getLocalBounds().toFloat();
        const bool on = getToggleState();
        if (style == Style::Switch) {
            auto track = r.removeFromLeft(38.0f).withSizeKeepingCentre(36.0f, 18.0f);
            g.setColour(on ? col::blue.withAlpha(0.85f) : juce::Colour(0xff1b2434));
            g.fillRoundedRectangle(track, 9.0f);
            g.setColour(on ? col::cyan : col::edge);
            g.drawRoundedRectangle(track, 9.0f, 1.0f);
            const float kx = on ? track.getRight() - 9.0f : track.getX() + 9.0f;
            g.setColour(juce::Colours::white);
            g.fillEllipse(juce::Rectangle<float>(13, 13).withCentre({kx, track.getCentreY()}));
            if (getButtonText().isNotEmpty()) {
                g.setColour(on ? col::text : col::dim);
                g.setFont(font(11.5f));
                g.drawText(getButtonText(), r.withTrimmedLeft(4), juce::Justification::centredLeft);
            }
        } else if (style == Style::Dot) {
            auto dot = r.removeFromLeft(18.0f).withSizeKeepingCentre(12.0f, 12.0f);
            g.setColour(col::edge);
            g.drawEllipse(dot, 1.3f);
            if (on) {
                g.setColour(col::cyan.withAlpha(0.25f));
                g.fillEllipse(dot.expanded(3));
                g.setColour(col::cyan);
                g.fillEllipse(dot.reduced(2.5f));
            }
            g.setColour(on ? col::text : col::dim);
            g.setFont(font(11.5f));
            g.drawText(getButtonText(), r.withTrimmedLeft(2), juce::Justification::centredLeft);
        } else {
            auto p = r.reduced(0.5f);
            g.setColour(on ? col::amber.withAlpha(0.22f) : (over ? col::panelHi.brighter(0.1f) : col::panel));
            g.fillRoundedRectangle(p, 7.0f);
            g.setColour(on ? col::amber : col::edge);
            g.drawRoundedRectangle(p, 7.0f, 1.0f);
            g.setColour(on ? col::amber : col::dim);
            g.setFont(spaced(10.5f, true, 0.12f));
            g.drawText(getButtonText().toUpperCase(), p, juce::Justification::centred);
        }
    }

private:
    Style style;
    juce::ParameterAttachment att;
};

// ------------------------------------------------------------------ radio group for a choice parameter
class ParamRadio : public juce::Component {
public:
    ParamRadio(juce::AudioProcessorValueTreeState& apvts, unsigned id)
        : spec(ndx::kSpecs[id]),
          att(*apvts.getParameter(ndxjuce::paramId(id)), [this](float v) {
              current = (int)std::lround(v);
              repaint();
          }) {
        setTitle(spec.name);
        att.sendInitialUpdate();
    }
    void paint(juce::Graphics& g) override {
        const float rowH = (float)getHeight() / (float)spec.numItems;
        for (int i = 0; i < spec.numItems; ++i) {
            auto row = juce::Rectangle<float>(0, i * rowH, (float)getWidth(), rowH);
            auto dot = row.removeFromLeft(18.0f).withSizeKeepingCentre(12.0f, 12.0f);
            const bool sel = i == current;
            g.setColour(sel ? col::cyan : col::edge);
            g.drawEllipse(dot, 1.3f);
            if (sel) {
                g.setColour(col::cyan.withAlpha(0.25f));
                g.fillEllipse(dot.expanded(3));
                g.setColour(col::cyan);
                g.fillEllipse(dot.reduced(2.5f));
            }
            g.setColour(sel ? col::text : col::dim);
            g.setFont(font(11.5f));
            g.drawText(spec.items[i], row.withTrimmedLeft(2), juce::Justification::centredLeft);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        const int idx = juce::jlimit(0, spec.numItems - 1, (int)(e.position.y / ((float)getHeight() / (float)spec.numItems)));
        att.setValueAsCompleteGesture((float)idx);
    }

private:
    const ndx::ParamSpec& spec;
    int current = 0;
    juce::ParameterAttachment att;
};

// ------------------------------------------------------------------ drag bar showing "HPF 80 Hz"
class ParamBar : public juce::Component, public juce::SettableTooltipClient {
public:
    ParamBar(juce::AudioProcessorValueTreeState& apvts, unsigned id, const juce::String& prefix)
        : param(apvts.getParameter(ndxjuce::paramId(id))), prefixText(prefix), defaultValue(ndx::kSpecs[id].def),
          att(*param, [this](float) { repaint(); }) {
        setTitle(prefix);
        setTooltip(prefix + " - drag, wheel, double-click to reset");
        att.sendInitialUpdate();
    }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colour(0xff0a0e16));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(col::blue.withAlpha(0.35f));
        g.fillRoundedRectangle(r.withWidth(r.getWidth() * param->getValue()), 5.0f);
        g.setColour(col::edge);
        g.drawRoundedRectangle(r, 5.0f, 1.0f);
        g.setColour(col::text);
        g.setFont(font(11.0f));
        g.drawText(prefixText + " " + param->getText(param->getValue(), 12), r, juce::Justification::centred);
    }
    void mouseDown(const juce::MouseEvent& e) override { att.beginGesture(); setFrom(e); }
    void mouseDrag(const juce::MouseEvent& e) override { setFrom(e); }
    void mouseUp(const juce::MouseEvent&) override { att.endGesture(); }
    void mouseDoubleClick(const juce::MouseEvent&) override { att.setValueAsCompleteGesture((float)defaultValue); }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override {
        att.setValueAsCompleteGesture(param->convertFrom0to1(juce::jlimit(0.0f, 1.0f, param->getValue() + w.deltaY * 0.05f)));
    }

private:
    void setFrom(const juce::MouseEvent& e) {
        const float n = juce::jlimit(0.0f, 1.0f, e.position.x / (float)getWidth());
        att.setValueAsPartOfGesture(param->convertFrom0to1(n));
    }
    juce::RangedAudioParameter* param;
    juce::String prefixText;
    double defaultValue;
    juce::ParameterAttachment att;
};

// ------------------------------------------------------------------ tab strip
class TabStrip : public juce::Component {
public:
    std::function<void(int)> onChange;
    explicit TabStrip(juce::StringArray n) : names(std::move(n)) {}
    int getSelected() const { return selected; }
    void paint(juce::Graphics& g) override {
        const float w = (float)getWidth() / (float)names.size();
        for (int i = 0; i < names.size(); ++i) {
            auto r = juce::Rectangle<float>(i * w, 0, w, (float)getHeight()).reduced(2.0f, 1.0f);
            if (i == selected) {
                g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2a7bff), r.getX(), r.getY(), juce::Colour(0xff0d3a8f), r.getX(), r.getBottom(), false));
                g.fillRoundedRectangle(r, 5.0f);
                g.setColour(col::cyan);
                g.drawRoundedRectangle(r, 5.0f, 1.0f);
            } else {
                g.setColour(juce::Colour(0xff0a0e16));
                g.fillRoundedRectangle(r, 5.0f);
                g.setColour(col::edge);
                g.drawRoundedRectangle(r, 5.0f, 1.0f);
            }
            g.setColour(i == selected ? juce::Colours::white : col::dim);
            g.setFont(spaced(10.5f, true, 0.12f));
            g.drawText(names[i].toUpperCase(), r, juce::Justification::centred);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        const int i = juce::jlimit(0, names.size() - 1, (int)(e.position.x / ((float)getWidth() / (float)names.size())));
        if (i != selected) { selected = i; repaint(); if (onChange) onChange(i); }
    }

private:
    juce::StringArray names;
    int selected = 0;
};

// ------------------------------------------------------------------ level meter (L/R) with scale and peak hold
class LevelMeter : public juce::Component {
public:
    explicit LevelMeter(bool scaleRight) : scaleOnRight(scaleRight) {}

    void update(float lDb, float rDb, float dt) {
        const float in[2] = {lDb, rDb};
        for (int c = 0; c < 2; ++c) {
            disp[c] = juce::jmax(in[c], disp[c] - 40.0f * dt);
            if (in[c] >= peak[c]) { peak[c] = in[c]; hold[c] = 1.5f; }
            else if ((hold[c] -= dt) < 0.0f) peak[c] = juce::jmax(in[c], peak[c] - 20.0f * dt);
        }
        repaint();
    }
    float peakDb() const { return juce::jmax(peak[0], peak[1]); }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat();
        auto scale = scaleOnRight ? area.removeFromRight(24.0f) : area.removeFromLeft(24.0f);
        const float gap = 5.0f, bw = (area.getWidth() - gap) / 2.0f;
        for (int c = 0; c < 2; ++c) {
            juce::Rectangle<float> bar(area.getX() + c * (bw + gap), area.getY(), bw, area.getHeight());
            g.setColour(juce::Colour(0xff080b12));
            g.fillRoundedRectangle(bar, 3.0f);
            const auto fill = bar.withTop(bar.getBottom() - frac(disp[c]) * bar.getHeight());
            juce::ColourGradient grad(col::orange, bar.getX(), bar.getY(), col::blue, bar.getX(), bar.getBottom(), false);
            grad.addColour(0.22, col::amber);
            grad.addColour(0.45, col::green);
            grad.addColour(0.75, col::cyan);
            g.saveState();
            g.reduceClipRegion(fill.toNearestInt());
            g.setGradientFill(grad);
            g.fillRoundedRectangle(bar, 3.0f);
            g.restoreState();
            g.setColour(juce::Colour(0xff080b12));
            for (float db = -36.0f; db < 0.0f; db += 1.5f) g.drawHorizontalLine((int)(bar.getBottom() - frac(db) * bar.getHeight()), bar.getX(), bar.getRight());
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            const float py = bar.getBottom() - frac(peak[c]) * bar.getHeight();
            g.fillRect(bar.getX(), py - 0.75f, bar.getWidth(), 1.5f);
        }
        g.setFont(font(8.5f));
        for (int db = 0; db >= -36; db -= 6) {
            const float y = area.getBottom() - frac((float)db) * area.getHeight();
            g.setColour(col::dim);
            g.drawText(juce::String(db), scale.withY(y - 6.0f).withHeight(12.0f), scaleOnRight ? juce::Justification::centredRight : juce::Justification::centredLeft, false);
        }
    }

private:
    static float frac(float db) { return juce::jlimit(0.0f, 1.0f, (db + 36.0f) / 36.0f); }
    bool scaleOnRight;
    float disp[2] = {-90.0f, -90.0f}, peak[2] = {-90.0f, -90.0f}, hold[2] = {0.0f, 0.0f};
};

// ------------------------------------------------------------------ Compression Field (live spectrogram + band gain-reduction curve)
class FieldDisplay : public juce::Component, public juce::SettableTooltipClient {
public:
    static constexpr int kBins = 192, kRows = 110;
    static constexpr float kMinFreq = 20.0f, kMaxFreq = 20000.0f;

    explicit FieldDisplay(juce::AudioProcessorValueTreeState& apvts)
        : xoLow(apvts.getParameter(ndxjuce::paramId(ndx::kXoverLow))),
          xoHigh(apvts.getParameter(ndxjuce::paramId(ndx::kXoverHigh))),
          lowAtt(*xoLow, [this](float) { repaint(); }),
          highAtt(*xoHigh, [this](float) { repaint(); }),
          image(juce::Image::ARGB, kBins, kRows, true),
          hist((size_t)(kBins * kRows), 0.0f) {
        setTitle("Compression Field");
        setTooltip("Live spectrum history with per-band gain reduction. Drag the two handles to move the crossovers.");
        lowAtt.sendInitialUpdate();
        highAtt.sendInitialUpdate();
    }

    void resized() override { plot = getPlot(); }

    void pushRow(const float* level01) {
        std::memmove(hist.data(), hist.data() + kBins, sizeof(float) * (size_t)(kBins * (kRows - 1)));
        std::memcpy(hist.data() + kBins * (kRows - 1), level01, sizeof(float) * (size_t)kBins);
        juce::Image::BitmapData data(image, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < kRows; ++y) {
            const float fade = 0.28f + 0.72f * (float)y / (float)(kRows - 1);
            for (int x = 0; x < kBins; ++x) {
                const float v = hist[(size_t)(y * kBins + x)];
                data.setPixelColour(x, y, col::bg.interpolatedWith(heat(v), fade));
            }
        }
        repaint();
    }

    void setBandGr(float lo, float mid, float hi, float total) {
        const float k = 0.35f;
        grLo += k * (lo - grLo); grMid += k * (mid - grMid); grHi += k * (hi - grHi); grTotal += k * (total - grTotal);
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto full = getLocalBounds().toFloat();
        drawPanel(g, full, 10.0f);
        g.setColour(col::dim);
        g.setFont(spaced(9.5f, true, 0.14f));
        g.drawText("COMPRESSION FIELD", full.withHeight(22.0f).withTrimmedLeft(12.0f), juce::Justification::centredLeft, false);
        g.setColour(grTotal > 0.2f ? col::cyan : col::faint);
        g.drawText("GR " + juce::String(-grTotal, 1) + " dB", full.withHeight(22.0f).withTrimmedRight(60.0f), juce::Justification::centredRight, false);

        plot = getPlot();
        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle(plot, 4.0f);
        g.reduceClipRegion(clip);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(image, plot, juce::RectanglePlacement::stretchToFit);
        g.restoreState();

        // grid
        g.setColour(col::blue.withAlpha(0.16f));
        for (float f : {20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f})
            g.drawVerticalLine((int)xOf(f), plot.getY(), plot.getBottom());
        for (int i = 1; i < 6; ++i) g.drawHorizontalLine((int)(plot.getY() + plot.getHeight() * (float)i / 6.0f), plot.getX(), plot.getRight());
        g.setColour(col::edge);
        g.drawRoundedRectangle(plot, 4.0f, 1.0f);

        // curve
        juce::Path curve;
        const float lowF = (float)plainOf(xoLow), highF = (float)plainOf(xoHigh);
        for (int i = 0; i <= 160; ++i) {
            const float f = kMinFreq * std::pow(kMaxFreq / kMinFreq, (float)i / 160.0f);
            const juce::Point<float> p(xOf(f), yOfGr(grAt(f, lowF, highF)));
            if (i == 0) curve.startNewSubPath(p); else curve.lineTo(p);
        }
        g.setColour(col::cyan.withAlpha(0.14f));
        g.strokePath(curve, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xffeaf8ff));
        g.strokePath(curve, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // handles
        for (int h = 0; h < 2; ++h) {
            const float f = h == 0 ? lowF : highF;
            const juce::Point<float> p(xOf(f), yOfGr(grAt(f, lowF, highF)));
            const bool hot = hovered == h || dragging == h;
            g.setColour(col::cyan.withAlpha(hot ? 0.35f : 0.18f));
            g.fillEllipse(juce::Rectangle<float>(20, 20).withCentre(p));
            g.setColour(juce::Colour(0xff0a1426));
            g.fillEllipse(juce::Rectangle<float>(11, 11).withCentre(p));
            g.setColour(hot ? juce::Colours::white : col::cyan);
            g.drawEllipse(juce::Rectangle<float>(11, 11).withCentre(p), 2.0f);
            if (hot) {
                g.setFont(font(10.5f, true));
                g.setColour(col::text);
                g.drawText(juce::String((int)std::lround(f)) + " Hz", juce::Rectangle<float>(70, 14).withCentre({p.x, p.y - 20.0f}), juce::Justification::centred);
            }
        }

        // frequency labels
        g.setColour(col::dim);
        g.setFont(font(9.0f));
        const std::pair<float, const char*> labels[] = {{20, "20"}, {50, "50"}, {100, "100"}, {200, "200"}, {500, "500"}, {1000, "1k"},
                                                        {2000, "2"}, {5000, "5k"}, {10000, "10k"}, {20000, "20k"}};
        for (auto& l : labels)
            g.drawText(l.second, juce::Rectangle<float>(34, 12).withCentre({xOf(l.first), plot.getBottom() + 9.0f}), juce::Justification::centred, false);
        g.setFont(spaced(8.5f, true, 0.2f));
        g.drawText("FREQUENCY  /  TIME (NEWEST AT BOTTOM)", full.withTop(plot.getBottom() + 14.0f).withTrimmedLeft(12.0f), juce::Justification::centredLeft, false);

        // colour legend (spectrum level)
        auto lg = juce::Rectangle<float>(plot.getRight() + 12.0f, plot.getY() + 14.0f, 10.0f, plot.getHeight() - 28.0f);
        g.setGradientFill(juce::ColourGradient(heat(1.0f), lg.getX(), lg.getY(), heat(0.0f), lg.getX(), lg.getBottom(), false));
        g.fillRoundedRectangle(lg, 3.0f);
        g.setColour(col::dim);
        g.setFont(font(8.5f));
        g.drawText("dB", juce::Rectangle<float>(30, 12).withCentre({lg.getCentreX() + 6.0f, lg.getY() - 8.0f}), juce::Justification::centred, false);
        for (int db = -12; db >= -72; db -= 12) {
            const float y = lg.getY() + lg.getHeight() * ((float)(-12 - db) / 60.0f);
            g.drawText(juce::String(db), juce::Rectangle<float>(26, 12).withCentre({lg.getRight() + 15.0f, y}), juce::Justification::centredLeft, false);
        }
    }

    void mouseMove(const juce::MouseEvent& e) override {
        const int h = hit(e.position);
        if (h != hovered) { hovered = h; setMouseCursor(h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor); repaint(); }
    }
    void mouseExit(const juce::MouseEvent&) override { hovered = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override {
        dragging = hit(e.position);
        if (dragging == 0) lowAtt.beginGesture();
        if (dragging == 1) highAtt.beginGesture();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (dragging < 0) return;
        const float f = fOf(e.position.x);
        if (dragging == 0) lowAtt.setValueAsPartOfGesture(juce::jlimit(60.0f, 400.0f, f));
        else highAtt.setValueAsPartOfGesture(juce::jlimit(1500.0f, 8000.0f, f));
    }
    void mouseUp(const juce::MouseEvent&) override {
        if (dragging == 0) lowAtt.endGesture();
        if (dragging == 1) highAtt.endGesture();
        dragging = -1;
        repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override {
        const int h = hit(e.position);
        if (h == 0) lowAtt.setValueAsCompleteGesture((float)ndx::kSpecs[ndx::kXoverLow].def);
        if (h == 1) highAtt.setValueAsCompleteGesture((float)ndx::kSpecs[ndx::kXoverHigh].def);
    }

private:
    juce::Rectangle<float> getPlot() const { return getLocalBounds().toFloat().withTrimmedTop(24.0f).withTrimmedBottom(32.0f).withTrimmedLeft(12.0f).withTrimmedRight(50.0f); }
    float xOf(float f) const { return plot.getX() + plot.getWidth() * std::log10(f / kMinFreq) / 3.0f; }
    float fOf(float x) const { return kMinFreq * std::pow(1000.0f, juce::jlimit(0.0f, 1.0f, (x - plot.getX()) / plot.getWidth())); }
    float yOfGr(float gr) const { return plot.getBottom() - plot.getHeight() * (0.16f + 0.68f * juce::jlimit(0.0f, 1.0f, gr / 18.0f)); }

    float grAt(float f, float xl, float xh) const {  // smooth interpolation between band centres (log-frequency)
        const float c0 = std::sqrt(kMinFreq * xl), c1 = std::sqrt(xl * xh), c2 = std::sqrt(xh * kMaxFreq);
        auto sm = [](float a, float b, float x) { const float t = juce::jlimit(0.0f, 1.0f, (x - a) / (b - a)); return t * t * (3.0f - 2.0f * t); };
        const float lf = std::log(f);
        if (f <= c0) return grLo;
        if (f >= c2) return grHi;
        if (f <= c1) return grLo + (grMid - grLo) * sm(std::log(c0), std::log(c1), lf);
        return grMid + (grHi - grMid) * sm(std::log(c1), std::log(c2), lf);
    }
    int hit(juce::Point<float> p) const {
        const float lowF = (float)plainOf(xoLow), highF = (float)plainOf(xoHigh);
        for (int h = 0; h < 2; ++h) {
            const float f = h == 0 ? lowF : highF;
            if (juce::Point<float>(xOf(f), yOfGr(grAt(f, lowF, highF))).getDistanceFrom(p) < 13.0f) return h;
        }
        return -1;
    }

    juce::RangedAudioParameter *xoLow, *xoHigh;
    juce::ParameterAttachment lowAtt, highAtt;
    juce::Image image;
    std::vector<float> hist;
    juce::Rectangle<float> plot;
    float grLo = 0, grMid = 0, grHi = 0, grTotal = 0;
    int hovered = -1, dragging = -1;
};

}  // namespace ndxui
