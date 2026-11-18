/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#include "PerformanceView.h"
#include "PerformanceColours.h"

#include "SurgeSynthEditor.h"
#include "SurgeSynthProcessor.h"
#include "SurgeGUIEditor.h"
#include "SurgeSynthesizer.h"
#include "SurgeStorage.h"
#include "FilterConfiguration.h"

#include "fmt/core.h"

#include <algorithm>

namespace Surge
{
namespace Mobile
{

/*
 * How a PerfControl reads and writes whatever it is bound to (a Parameter or a macro).
 */
struct PerfBinding
{
    std::function<bool()> available;
    std::function<std::string()> title;
    std::function<float()> get01;
    std::function<std::string()> display;
    std::function<float()> default01;
    std::function<void()> begin;
    std::function<void(float)> set01;
    std::function<void()> end;
};

namespace
{
constexpr int gap = 6;
constexpr int topBarHeight = 52;
// Below this width the patch browser becomes a slide-over panel
constexpr int narrowWidth = 1000;

struct PerformanceLookAndFeel : public juce::LookAndFeel_V4
{
    PerformanceLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, Colours::background);
        setColour(juce::TextButton::buttonColourId, Colours::panelRaised);
        setColour(juce::TextButton::buttonOnColourId, Colours::accent);
        setColour(juce::TextButton::textColourOffId, Colours::text);
        setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        setColour(juce::ComboBox::outlineColourId, Colours::outline);
        setColour(juce::Slider::rotarySliderFillColourId, Colours::accent);
        setColour(juce::Slider::rotarySliderOutlineColourId, Colours::panelRaised);
        setColour(juce::Slider::trackColourId, Colours::accent);
        setColour(juce::Slider::backgroundColourId, Colours::panelRaised);
        setColour(juce::Slider::thumbColourId, Colours::text);
        setColour(juce::Label::textColourId, Colours::text);
    }

    juce::Font getTextButtonFont(juce::TextButton &, int buttonHeight) override
    {
        return perfFont(juce::jmin(17.f, buttonHeight * 0.45f), juce::Font::bold);
    }

    // Menus must use the embedded font too (see perfFont)
    juce::Font getPopupMenuFont() override { return perfFont(17.f); }

    void drawPopupMenuSectionHeader(juce::Graphics &g, const juce::Rectangle<int> &area,
                                    const juce::String &name) override
    {
        g.setFont(perfFont(15.f, juce::Font::bold));
        g.setColour(Colours::textDim);
        g.drawFittedText(name, area.reduced(12, 0), juce::Justification::bottomLeft, 1);
    }

    void drawButtonBackground(juce::Graphics &g, juce::Button &b, const juce::Colour &bg,
                              bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(1.f);
        auto c = b.getToggleState() ? findColour(juce::TextButton::buttonOnColourId) : bg;
        if (down)
            c = c.brighter(0.25f);
        else if (highlighted)
            c = c.brighter(0.08f);
        g.setColour(c);
        g.fillRoundedRectangle(r, 6.f);
        g.setColour(Colours::outline);
        g.drawRoundedRectangle(r, 6.f, 1.f);
    }

    void drawRotarySlider(juce::Graphics &g, int x, int y, int w, int h, float pos,
                          float startAngle, float endAngle, juce::Slider &s) override
    {
        auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(6.f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.f;
        auto centre = bounds.getCentre();
        auto lineW = juce::jmax(4.f, radius * 0.18f);
        auto arcRadius = radius - lineW * 0.5f;
        auto angle = startAngle + pos * (endAngle - startAngle);
        bool enabled = s.isEnabled();

        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.f, startAngle, endAngle,
                            true);
        g.setColour(Colours::panelRaised);
        g.strokePath(track, {lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded});

        if (enabled)
        {
            juce::Path value;
            value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.f, startAngle, angle,
                                true);
            g.setColour(Colours::accent);
            g.strokePath(value,
                         {lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded});
        }

        auto inner = radius - lineW * 1.8f;
        g.setColour(Colours::panel);
        g.fillEllipse(juce::Rectangle<float>(inner * 2, inner * 2).withCentre(centre));

        juce::Point<float> tip(centre.x + inner * 0.8f * std::sin(angle),
                               centre.y - inner * 0.8f * std::cos(angle));
        g.setColour(enabled ? Colours::text : Colours::textDim);
        g.drawLine({centre.getPointOnCircumference(inner * 0.25f, angle), tip}, 3.f);
    }
};

std::string paramDisplay(Parameter *p)
{
    if (!p)
        return {};
    char txt[256]{};
    p->get_display(txt);
    return txt;
}
} // namespace

/*
 * A large knob or fader with a title above and its value below.
 */
class PerfControl : public juce::Component
{
  public:
    PerfControl(bool isFader, PerfBinding b) : binding(std::move(b)), fader(isFader)
    {
        if (fader)
        {
            slider.setSliderStyle(juce::Slider::LinearBarVertical);
            slider.setSliderSnapsToMousePosition(false);
        }
        else
        {
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setMouseDragSensitivity(220);
        }
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.setRange(0.0, 1.0, 0.0);
        slider.setVelocityBasedMode(false);
        slider.setPopupDisplayEnabled(false, false, nullptr);

        slider.onDragStart = [this]() {
            inGesture = true;
            if (binding.begin)
                binding.begin();
        };
        slider.onDragEnd = [this]() {
            if (binding.end)
                binding.end();
            inGesture = false;
        };
        slider.onValueChange = [this]() {
            auto v = (float)slider.getValue();
            if (inGesture)
            {
                binding.set01(v);
            }
            else
            {
                // e.g. double-tap to reset
                if (binding.begin)
                    binding.begin();
                binding.set01(v);
                if (binding.end)
                    binding.end();
            }
            updateText();
        };

        addAndMakeVisible(slider);
    }

    void sync()
    {
        bool avail = !binding.available || binding.available();
        if (avail != slider.isEnabled())
        {
            slider.setEnabled(avail);
            repaint();
        }

        if (!inGesture)
        {
            auto v = binding.get01();
            if (std::abs(v - slider.getValue()) > 1e-6)
                slider.setValue(v, juce::dontSendNotification);
            if (binding.default01)
                slider.setDoubleClickReturnValue(true, binding.default01());
        }

        updateText();
    }

    void updateText()
    {
        auto t = binding.title ? binding.title() : std::string();
        auto d = binding.display ? binding.display() : std::string();
        if (t != titleText || d != valueText)
        {
            titleText = t;
            valueText = d;
            slider.setTitle(t);
            repaint();
        }
    }

    void paint(juce::Graphics &g) override
    {
        auto r = getLocalBounds();
        bool en = slider.isEnabled();
        g.setColour(en ? Colours::text : Colours::textDim);
        g.setFont(perfFont(13.f, juce::Font::bold));
        g.drawText(titleText, r.removeFromTop(titleH), juce::Justification::centred, true);
        g.setColour(en ? Colours::textDim : Colours::textDim.withAlpha(0.5f));
        g.setFont(perfFont(12.f));
        g.drawText(en ? valueText : "Off", r.removeFromBottom(valueH),
                   juce::Justification::centred, true);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop(titleH);
        r.removeFromBottom(valueH);
        if (fader)
        {
            auto w = juce::jmin(r.getWidth(), 56);
            slider.setBounds(r.withSizeKeepingCentre(w, r.getHeight()));
        }
        else
        {
            auto s = juce::jmin(r.getWidth(), r.getHeight());
            slider.setBounds(r.withSizeKeepingCentre(s, s));
        }
    }

  private:
    static constexpr int titleH = 18, valueH = 16;

    PerfBinding binding;
    bool fader;
    bool inGesture{false};
    juce::Slider slider;
    std::string titleText, valueText;
};

/*
 * A row/grid of mutually exclusive buttons.
 */
class SegmentedButtons : public juce::Component
{
  public:
    SegmentedButtons(std::vector<std::string> labels, int columns) : cols(columns)
    {
        for (size_t i = 0; i < labels.size(); ++i)
        {
            auto b = std::make_unique<juce::TextButton>(labels[i]);
            b->setTriggeredOnMouseDown(true);
            b->onClick = [this, i]() {
                if (onSelect)
                    onSelect((int)i);
            };
            addAndMakeVisible(*b);
            buttons.push_back(std::move(b));
        }
    }

    std::function<void(int)> onSelect;

    void setSelected(int idx)
    {
        for (size_t i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState((int)i == idx, juce::dontSendNotification);
    }

    void resized() override
    {
        if (buttons.empty())
            return;
        int rows = ((int)buttons.size() + cols - 1) / cols;
        auto r = getLocalBounds();
        int bw = (r.getWidth() - (cols - 1) * 4) / cols;
        int bh = (r.getHeight() - (rows - 1) * 4) / rows;
        for (size_t i = 0; i < buttons.size(); ++i)
        {
            int c = (int)i % cols, row = (int)i / cols;
            buttons[i]->setBounds(r.getX() + c * (bw + 4), r.getY() + row * (bh + 4), bw, bh);
        }
    }

  private:
    int cols;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
};

/*
 * A tappable FX slot showing its slot name and effect type; lit when the slot is active.
 */
class FxToggle : public juce::Button
{
  public:
    FxToggle(int slotIdx, std::string slotName, std::string fxName)
        : juce::Button(slotName), slot(slotIdx), slotLabel(std::move(slotName)),
          fxLabel(std::move(fxName))
    {
        setTriggeredOnMouseDown(true);
        setTitle(slotLabel + " " + fxLabel);
    }

    int slot;

    void paintButton(juce::Graphics &g, bool highlighted, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced(1.f);
        bool on = getToggleState();
        auto bg = on ? Colours::accentDim : Colours::panel;
        if (down)
            bg = bg.brighter(0.2f);
        g.setColour(bg);
        g.fillRoundedRectangle(r, 6.f);
        g.setColour(on ? Colours::accent : Colours::outline);
        g.drawRoundedRectangle(r, 6.f, on ? 2.f : 1.f);

        auto tr = getLocalBounds().reduced(4, 2);
        bool twoLines = tr.getHeight() >= 34;
        g.setColour(on ? Colours::text : Colours::textDim);
        if (twoLines)
        {
            g.setFont(perfFont(11.f));
            g.drawText(slotLabel, tr.removeFromTop(tr.getHeight() / 2),
                       juce::Justification::centredBottom, true);
            g.setFont(perfFont(13.f, juce::Font::bold));
            g.drawText(fxLabel, tr, juce::Justification::centredTop, true);
        }
        else
        {
            g.setFont(perfFont(12.f, juce::Font::bold));
            g.drawText(slotLabel + " " + fxLabel, tr, juce::Justification::centred, true);
        }
    }

  private:
    std::string slotLabel, fxLabel;
};

//==============================================================================

PerformanceView::PerformanceView(EditorGetter getter)
    : getEditor(std::move(getter)), saveDialog([this]() { return getSynth(); })
{
    lookAndFeel = std::make_unique<PerformanceLookAndFeel>();
    setLookAndFeel(lookAndFeel.get());
    setOpaque(true);

    // --- Top bar

    exitButton.onClick = [this]() {
        if (onExitRequested)
            onExitRequested();
    };
    optionsButton.onClick = [this]() {
        if (onOptionsRequested)
            onOptionsRequested();
    };
    helpButton.onClick = [this]() {
        if (onHelpRequested)
            onHelpRequested();
    };
    prevButton.onClick = [this]() { jogPatch(false); };
    nextButton.onClick = [this]() { jogPatch(true); };
    randomButton.onClick = [this]() { randomPatch(); };
    saveButton.onClick = [this]() {
        if (isNarrow())
            setBrowserOpen(false);
        saveDialog.open();
    };
    favButton.onClick = [this]() { toggleFavorite(); };

    for (juce::Button *b : std::initializer_list<juce::Button *>{
             &exitButton, &randomButton, &prevButton, &nextButton, &favButton,
             &saveButton, &optionsButton, &helpButton})
    {
        b->setTriggeredOnMouseDown(true);
        addAndMakeVisible(*b);
    }

    patchNameLabel.setFont(perfFont(20.f, juce::Font::bold));
    patchNameLabel.setJustificationType(juce::Justification::centred);
    patchNameLabel.setColour(juce::Label::textColourId, Colours::text);
    patchNameLabel.setInterceptsMouseClicks(false, false);
    patchCategoryLabel.setFont(perfFont(12.f));
    patchCategoryLabel.setJustificationType(juce::Justification::centred);
    patchCategoryLabel.setColour(juce::Label::textColourId, Colours::textDim);
    patchCategoryLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(patchNameLabel);
    addAndMakeVisible(patchCategoryLabel);

    // --- Browser
    browser.onCloseRequested = [this]() { setBrowserOpen(false); };
    browser.onPatchChosen = [this](int id) {
        loadPatch(id);
        if (isNarrow())
            setBrowserOpen(false);
    };
    addChildComponent(browser);

    // --- Save panel (an overlay, shown on demand)
    addChildComponent(saveDialog);

    // --- Master volume
    masterVolume = std::make_unique<PerfControl>(
        true, paramBinding([this]() -> Parameter * {
            auto s = getSynth();
            return s ? &s->storage.getPatch().volume : nullptr;
        },
                           "Master"));
    addAndMakeVisible(*masterVolume);

    // --- Scene
    auto sceneParam = [this](std::function<Parameter *(SurgeSceneStorage &)> f) {
        return [this, f]() -> Parameter * {
            auto sge = getSGE();
            if (!sge)
                return nullptr;
            auto sc = juce::jlimit(0, n_scenes - 1, sge->getCurrentScene());
            return f(sge->synth->storage.getPatch().scene[sc]);
        };
    };

    auto filterOn = [this](int unit) {
        return [this, unit]() {
            auto sge = getSGE();
            if (!sge)
                return false;
            auto sc = juce::jlimit(0, n_scenes - 1, sge->getCurrentScene());
            return sge->synth->storage.getPatch().scene[sc].filterunit[unit].type.val.i !=
                   sst::filters::FilterType::fut_none;
        };
    };

    // Cutoff and Reso follow the F1/F2 tab
    auto currentFilter = [this](auto member) {
        return [this, member](SurgeSceneStorage &s) {
            return &(s.filterunit[juce::jlimit(0, 1, filterTabIdx)].*member);
        };
    };
    auto currentFilterOn = [this]() {
        auto sge = getSGE();
        if (!sge)
            return false;
        auto sc = juce::jlimit(0, n_scenes - 1, sge->getCurrentScene());
        return sge->synth->storage.getPatch()
                   .scene[sc]
                   .filterunit[juce::jlimit(0, 1, filterTabIdx)]
                   .type.val.i != sst::filters::FilterType::fut_none;
    };

    {
        auto cutoff = paramBinding(sceneParam(currentFilter(&FilterStorage::cutoff)), "Cutoff");
        cutoff.available = currentFilterOn;
        auto reso = paramBinding(sceneParam(currentFilter(&FilterStorage::resonance)), "Reso");
        reso.available = currentFilterOn;

        auto drive = paramBinding(
            sceneParam([](SurgeSceneStorage &s) { return &s.wsunit.drive; }), "Drive");
        drive.available = [this]() {
            auto sge = getSGE();
            if (!sge)
                return false;
            auto sc = juce::jlimit(0, n_scenes - 1, sge->getCurrentScene());
            return sge->synth->storage.getPatch().scene[sc].wsunit.type.val.i !=
                   (int)sst::waveshapers::WaveshaperType::wst_none;
        };

        auto porta = paramBinding(
            sceneParam([](SurgeSceneStorage &s) { return &s.portamento; }), "Porta");

        for (auto *b : {&cutoff, &reso, &drive, &porta})
        {
            auto c = std::make_unique<PerfControl>(false, std::move(*b));
            addAndMakeVisible(*c);
            sceneControls.push_back(std::move(c));
        }
    }

    {
        auto vol =
            paramBinding(sceneParam([](SurgeSceneStorage &s) { return &s.volume; }), "Volume");
        sceneVolume = std::make_unique<PerfControl>(true, std::move(vol));
        addAndMakeVisible(*sceneVolume);
    }

    filterTab = std::make_unique<SegmentedButtons>(std::vector<std::string>{"F1", "F2"}, 1);
    filterTab->onSelect = [this](int i) {
        filterTabIdx = i;
        filterTab->setSelected(i);
        syncFromSynth();
    };
    filterTab->setSelected(0);
    addAndMakeVisible(*filterTab);

    playModeButton.setTitle("Play Mode");
    playModeButton.onClick = [this]() { showPlayModeMenu(); };
    addAndMakeVisible(playModeButton);

    octaveDownButton.setTitle("Octave Down");
    octaveUpButton.setTitle("Octave Up");
    octaveDownButton.onClick = [this]() { shiftOctave(-1); };
    octaveUpButton.onClick = [this]() { shiftOctave(1); };
    addAndMakeVisible(octaveDownButton);
    addAndMakeVisible(octaveUpButton);
    // Two lines ("Oct" over the value) to keep the stepper narrow
    octaveLabel.setFont(perfFont(13.f, juce::Font::bold));
    octaveLabel.setBorderSize({0, 0, 0, 0});
    octaveLabel.setJustificationType(juce::Justification::centred);
    octaveLabel.setColour(juce::Label::textColourId, Colours::text);
    addAndMakeVisible(octaveLabel);

    voicesDownButton.setTitle("Fewer Voices");
    voicesUpButton.setTitle("More Voices");
    voicesDownButton.onClick = [this]() { stepVoices(-1); };
    voicesUpButton.onClick = [this]() { stepVoices(1); };
    addAndMakeVisible(voicesDownButton);
    addAndMakeVisible(voicesUpButton);
    voicesLabel.setFont(perfFont(13.f, juce::Font::bold));
    voicesLabel.setBorderSize({0, 0, 0, 0});
    voicesLabel.setJustificationType(juce::Justification::centred);
    voicesLabel.setColour(juce::Label::textColourId, Colours::text);
    addAndMakeVisible(voicesLabel);

    sceneA.setRadioGroupId(0x5ce4e);
    sceneB.setRadioGroupId(0x5ce4e);
    sceneA.setTitle("Scene A");
    sceneB.setTitle("Scene B");
    sceneA.setTriggeredOnMouseDown(true);
    sceneB.setTriggeredOnMouseDown(true);
    sceneA.onClick = [this]() {
        if (auto sge = getSGE())
            sge->selectScene(0);
        syncFromSynth();
    };
    sceneB.onClick = [this]() {
        if (auto sge = getSGE())
            sge->selectScene(1);
        syncFromSynth();
    };
    addAndMakeVisible(sceneA);
    addAndMakeVisible(sceneB);

    sceneMode = std::make_unique<SegmentedButtons>(
        std::vector<std::string>{"Single", "Split", "Dual", "Ch. Split"}, 2);
    sceneMode->onSelect = [this](int i) {
        if (auto s = getSynth())
            setIntParam(&s->storage.getPatch().scenemode, i);
        syncFromSynth();
    };
    addAndMakeVisible(*sceneMode);

    // --- Envelopes (EG 0 is the amp EG, EG 1 the filter EG)
    auto addEg = [&](std::vector<std::unique_ptr<PerfControl>> &dest, int eg) {
        struct EgCtl
        {
            const char *title;
            Parameter ADSRStorage::*p;
        };
        for (auto &e : {EgCtl{"Attack", &ADSRStorage::a}, EgCtl{"Decay", &ADSRStorage::d},
                        EgCtl{"Sustain", &ADSRStorage::s}, EgCtl{"Release", &ADSRStorage::r}})
        {
            auto mem = e.p;
            auto c = std::make_unique<PerfControl>(
                false, paramBinding(sceneParam([eg, mem](SurgeSceneStorage &s) {
                                        return &(s.adsr[eg].*mem);
                                    }),
                                    e.title));
            addChildComponent(*c);
            dest.push_back(std::move(c));
        }
    };
    addEg(ampEg, 0);
    addEg(filterEg, 1);

    auto addExtra = [&](std::vector<std::unique_ptr<PerfControl>> &dest, PerfBinding b) {
        auto c = std::make_unique<PerfControl>(false, std::move(b));
        addChildComponent(*c);
        dest.push_back(std::move(c));
    };

    // How much velocity changes the amp level, then the amp's output level (as in the full UI)
    addExtra(ampEg, paramBinding(sceneParam([](SurgeSceneStorage &s) { return &s.vca_velsense; }),
                                 "Vel > Gain"));
    addExtra(ampEg, paramBinding(sceneParam([](SurgeSceneStorage &s) { return &s.vca_level; }),
                                 "Gain"));

    // How far (in semitones) the filter EG moves each filter's cutoff
    for (int unit = 0; unit < 2; ++unit)
    {
        auto b = paramBinding(sceneParam([unit](SurgeSceneStorage &s) {
                                  return &s.filterunit[unit].envmod;
                              }),
                              unit == 0 ? "F1 Env" : "F2 Env");
        b.available = filterOn(unit);
        addExtra(filterEg, std::move(b));
    }

    lowerPage = std::make_unique<SegmentedButtons>(
        std::vector<std::string>{"Filter EG", "Amp EG", "Macros"}, 1);
    lowerPage->onSelect = [this](int i) {
        lowerPageIdx = i;
        lowerPage->setSelected(i);
        resized();
        repaint();
    };
    lowerPage->setSelected(lowerPageIdx);
    addChildComponent(*lowerPage);

    // --- FX
    fxBypass = std::make_unique<SegmentedButtons>(
        std::vector<std::string>{"All FX", "No Send", "Scene FX", "FX Off"}, 2);
    fxBypass->onSelect = [this](int i) {
        if (auto s = getSynth())
            setIntParam(&s->storage.getPatch().fx_bypass, i);
        syncFromSynth();
    };
    addAndMakeVisible(*fxBypass);

    // --- Macros
    for (int i = 0; i < n_customcontrollers; ++i)
    {
        auto c = std::make_unique<PerfControl>(true, macroBinding(i));
        addAndMakeVisible(*c);
        macros.push_back(std::move(c));
    }

    setWantsKeyboardFocus(false);
}

PerformanceView::~PerformanceView()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

SurgeGUIEditor *PerformanceView::getSGE() const
{
    if (auto ed = getEditor ? getEditor() : nullptr)
        return ed->getSurgeGUIEditor();
    return nullptr;
}

SurgeSynthesizer *PerformanceView::getSynth() const
{
    auto sge = getSGE();
    return sge ? sge->synth : nullptr;
}

PerfBinding PerformanceView::paramBinding(std::function<Parameter *()> resolve, std::string title)
{
    // The parameter a gesture started on, so begin/end always pair up
    auto gestureParam = std::make_shared<Parameter *>(nullptr);

    PerfBinding b;
    b.title = [title]() { return title; };
    b.get01 = [this, resolve]() {
        auto s = getSynth();
        auto p = resolve();
        return (s && p) ? s->getParameter01(s->idForParameter(p)) : 0.f;
    };
    b.display = [resolve]() { return paramDisplay(resolve()); };
    b.default01 = [resolve]() {
        auto p = resolve();
        return p ? p->get_default_value_f01() : 0.f;
    };
    b.begin = [this, resolve, gestureParam]() {
        auto p = resolve();
        auto ed = getEditor ? getEditor() : nullptr;
        *gestureParam = p;
        if (ed && p)
            ed->beginParameterEdit(p);
    };
    b.set01 = [this, resolve, gestureParam](float v) {
        auto s = getSynth();
        auto p = *gestureParam ? *gestureParam : resolve();
        if (!s || !p)
            return;
        auto id = s->idForParameter(p);
        s->setParameter01(id, v);
        s->sendParameterAutomation(id, s->getParameter01(id));
        s->storage.getPatch().isDirty = true;
    };
    b.end = [this, gestureParam]() {
        auto ed = getEditor ? getEditor() : nullptr;
        auto p = *gestureParam;
        if (ed && p)
            ed->endParameterEdit(p);
        *gestureParam = nullptr;
        if (auto s = getSynth())
            s->refresh_editor = true;
    };
    return b;
}

PerfBinding PerformanceView::macroBinding(int macro)
{
    auto adapter = [this, macro]() -> SurgeMacroToJuceParamAdapter * {
        auto ed = getEditor ? getEditor() : nullptr;
        if (!ed || macro >= (int)ed->processor.macrosById.size())
            return nullptr;
        return ed->processor.macrosById[macro];
    };
    auto bipolar = [this, macro]() {
        auto s = getSynth();
        return s && s->storage.getPatch().scene[0].modsources[ms_ctrl1 + macro]->is_bipolar();
    };

    PerfBinding b;
    b.title = [this, macro]() {
        auto s = getSynth();
        std::string label = s ? s->storage.getPatch().CustomControllerLabel[macro] : "";
        if (label.empty() || label == "-")
            return fmt::format("Macro {}", macro + 1);
        return label;
    };
    b.get01 = [this, macro]() {
        auto s = getSynth();
        return s ? s->getMacroParameterTarget01(macro) : 0.f;
    };
    b.display = [this, macro, bipolar]() {
        auto s = getSynth();
        if (!s)
            return std::string();
        auto v = s->getMacroParameterTarget01(macro);
        if (bipolar())
            return fmt::format("{:+.0f} %", (v * 2.f - 1.f) * 100.f);
        return fmt::format("{:.0f} %", v * 100.f);
    };
    b.default01 = [bipolar]() { return bipolar() ? 0.5f : 0.f; };
    b.begin = [adapter]() {
        if (auto a = adapter())
            a->beginChangeGesture();
    };
    b.set01 = [this, adapter](float v) {
        if (auto a = adapter())
            a->setValueNotifyingHost(v);
        if (auto s = getSynth())
            s->storage.getPatch().isDirty = true;
    };
    b.end = [adapter]() {
        if (auto a = adapter())
            a->endChangeGesture();
    };
    return b;
}

void PerformanceView::setIntParam(Parameter *p, int value)
{
    if (!p || p->val_max.i == p->val_min.i)
        return;
    auto b = paramBinding([p]() { return p; }, "");
    b.begin();
    b.set01((value - p->val_min.i) * 1.f / (p->val_max.i - p->val_min.i));
    b.end();
}

namespace
{
// Short names for the play mode button; the menu shows the full names
const char *playModeShortName(int m)
{
    static const char *names[] = {"Poly", "Mono", "Mono ST", "Mono FP", "Mono ST+FP", "Latch"};
    return (m >= 0 && m < (int)(sizeof(names) / sizeof(names[0]))) ? names[m] : "Poly";
}
} // namespace

void PerformanceView::showPlayModeMenu()
{
    auto sge = getSGE();
    if (!sge)
        return;
    auto sc = juce::jlimit(0, n_scenes - 1, sge->getCurrentScene());
    auto *p = &sge->synth->storage.getPatch().scene[sc].polymode;

    juce::PopupMenu menu;
    menu.addSectionHeader(sc == 1 ? "Scene B Play Mode" : "Scene A Play Mode");
    for (int m = 0; m < n_play_modes; ++m)
        menu.addItem(play_mode_names[m], true, p->val.i == m, [this, p, m]() {
            setIntParam(p, m);
            syncFromSynth();
        });
    // Drawn inside this view rather than as a separate native window, which is
    // unreliable on Android
    menu.showMenuAsync(juce::PopupMenu::Options()
                           .withParentComponent(this)
                           .withTargetComponent(&playModeButton)
                           .withMinimumWidth(260)
                           .withStandardItemHeight(40));
}

void PerformanceView::shiftOctave(int delta)
{
    auto sge = getSGE();
    if (!sge)
        return;
    auto sc = juce::jlimit(0, n_scenes - 1, sge->getCurrentScene());
    auto &p = sge->synth->storage.getPatch().scene[sc].octave;
    setIntParam(&p, juce::jlimit(p.val_min.i, p.val_max.i, p.val.i + delta));
    syncFromSynth();
}

namespace
{
// Polyphony steps: fine at the low end where each voice matters, coarse above
constexpr int voiceSteps[] = {2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 48, 64};
} // namespace

void PerformanceView::stepVoices(int dir)
{
    auto sge = getSGE();
    if (!sge)
        return;
    auto &p = sge->synth->storage.getPatch().polylimit;
    int cur = p.val.i, next = cur;
    if (dir > 0)
    {
        for (auto v : voiceSteps)
            if (v > cur)
            {
                next = v;
                break;
            }
    }
    else
    {
        for (auto v : voiceSteps)
            if (v < cur)
                next = v;
    }
    setIntParam(&p, juce::jlimit(p.val_min.i, p.val_max.i, next));
    syncFromSynth();
}

void PerformanceView::loadPatch(int patchId)
{
    auto sge = getSGE();
    if (!sge || patchId < 0 || patchId >= (int)sge->synth->storage.patch_list.size())
        return;
    sge->queuePatchFileLoad(sge->synth->storage.patch_list[patchId].path.u8string());
}

void PerformanceView::jogPatch(bool next)
{
    // Perform mode deliberately skips the "unsaved changes" prompt: tweaking the perform
    // controls marks the patch dirty, and the prompt lives in the hidden editor.
    if (auto s = getSynth())
        s->jogPatchOrCategory(next, /*isCategory=*/false, /*insideCategory=*/true);
}

void PerformanceView::randomPatch()
{
    auto s = getSynth();
    if (!s || s->storage.patch_list.empty())
        return;

    // Any patch in the library other than the current one
    int n = (int)s->storage.patch_list.size();
    int pick = juce::Random::getSystemRandom().nextInt(n);
    if (n > 1 && pick == s->patchid)
        pick = (pick + 1 + juce::Random::getSystemRandom().nextInt(n - 1)) % n;
    loadPatch(pick);
}

void PerformanceView::toggleFavorite()
{
    auto sge = getSGE();
    if (!sge)
        return;
    auto s = sge->synth;
    if (s->patchid < 0 || s->patchid >= (int)s->storage.patch_list.size())
        return;
    sge->setSpecificPatchAsFavorite(s->patchid, !s->storage.patch_list[s->patchid].isFavorite);
    browser.refreshPatches();
    syncFromSynth();
}

void PerformanceView::setActive(bool active)
{
    if (active)
    {
        if (auto s = getSynth())
            browser.rebuild(&s->storage);
        syncFromSynth();
        startTimerHz(20);
    }
    else
    {
        stopTimer();
    }
}

void PerformanceView::timerCallback() { syncFromSynth(); }

void PerformanceView::syncFromSynth()
{
    auto sge = getSGE();
    if (!sge)
        return;
    auto synth = sge->synth;
    auto &storage = synth->storage;
    auto &patch = storage.getPatch();

    // Patch list may change after a save or import
    if (browser.getBuiltPatchCount() != storage.patch_list.size())
    {
        browser.rebuild(&storage);
        lastPatchId = -2;
    }

    int pid = synth->patchid;
    if (pid != lastPatchId || patch.name != lastPatchName || patch.category != lastPatchCategory)
    {
        lastPatchId = pid;
        lastPatchName = patch.name;
        lastPatchCategory = patch.category;
        patchNameLabel.setText(patch.name, juce::dontSendNotification);
        patchCategoryLabel.setText(patch.category, juce::dontSendNotification);
        browser.setCurrentPatch(pid);
    }

    bool fav = pid >= 0 && pid < (int)storage.patch_list.size() && storage.patch_list[pid].isFavorite;
    favButton.setToggleState(fav, juce::dontSendNotification);
    favButton.setEnabled(pid >= 0 && pid < (int)storage.patch_list.size());

    int scene = sge->getCurrentScene();
    sceneA.setToggleState(scene == 0, juce::dontSendNotification);
    sceneB.setToggleState(scene == 1, juce::dontSendNotification);
    sceneMode->setSelected(patch.scenemode.val.i);
    fxBypass->setSelected(patch.fx_bypass.val.i);

    masterVolume->sync();
    for (auto &c : sceneControls)
        c->sync();
    sceneVolume->sync();

    {
        auto &sc = patch.scene[juce::jlimit(0, n_scenes - 1, scene)];
        playModeButton.setButtonText(playModeShortName(sc.polymode.val.i));
        auto oct = sc.octave.val.i;
        octaveLabel.setText(oct == 0 ? juce::String("Oct\n0") : juce::String::formatted("Oct\n%+d", oct),
                            juce::dontSendNotification);
        octaveDownButton.setEnabled(oct > sc.octave.val_min.i);
        octaveUpButton.setEnabled(oct < sc.octave.val_max.i);

        auto &pl = patch.polylimit;
        voicesLabel.setText("Voices\n" + juce::String(pl.val.i), juce::dontSendNotification);
        voicesDownButton.setEnabled(pl.val.i > pl.val_min.i);
        voicesUpButton.setEnabled(pl.val.i < pl.val_max.i);
    }
    for (auto &m : macros)
        m->sync();
    for (auto &c : ampEg)
        c->sync();
    for (auto &c : filterEg)
        c->sync();

    // FX slots: rebuild when types change, then reflect the deactivated bitmask
    bool typesChanged = !fxTypesValid;
    for (int i = 0; i < n_fx_slots && i < (int)lastFxTypes.size(); ++i)
    {
        if (patch.fx[i].type.val.i != lastFxTypes[i])
        {
            lastFxTypes[i] = patch.fx[i].type.val.i;
            typesChanged = true;
        }
    }
    if (typesChanged)
    {
        fxTypesValid = true;
        rebuildFxToggles();
    }

    int mask = patch.fx_disable.val.i;
    for (auto &t : fxToggles)
        t->setToggleState(!(mask & (1 << t->slot)), juce::dontSendNotification);
}

void PerformanceView::rebuildFxToggles()
{
    fxToggles.clear();

    auto s = getSynth();
    if (!s)
        return;

    // Display order: Scene A, Scene B, Send, Global (each 1-4)
    static constexpr int order[] = {0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15};
    static constexpr const char *labels[] = {"A1", "A2", "B1", "B2", "S1", "S2", "G1", "G2",
                                             "A3", "A4", "B3", "B4", "S3", "S4", "G3", "G4"};

    auto &patch = s->storage.getPatch();
    for (int slot : order)
    {
        if (slot >= n_fx_slots)
            continue;
        int t = patch.fx[slot].type.val.i;
        if (t == fxt_off || t < 0 || t >= n_fx_types)
            continue;

        auto b = std::make_unique<FxToggle>(slot, labels[slot], fx_type_shortnames[t]);
        b->onClick = [this, slot]() {
            auto sge = getSGE();
            if (!sge)
                return;
            int mask = sge->synth->storage.getPatch().fx_disable.val.i;
            sge->setFXDeactivatedBitmask(mask ^ (1 << slot));
            sge->synth->storage.getPatch().isDirty = true;
            syncFromSynth();
        };
        addAndMakeVisible(*b);
        fxToggles.push_back(std::move(b));
    }

    resized();
    repaint();
}

void PerformanceView::mouseUp(const juce::MouseEvent &e)
{
    // Tapping the patch name opens the browser (phones) or shows the current patch in it
    if (patchTitleArea.contains(e.getPosition()) && !e.mouseWasDraggedSinceMouseDown())
    {
        if (isNarrow() && !browserOpen)
            setBrowserOpen(true);
        else if (isNarrow())
        {
            setBrowserOpen(false);
            return;
        }
        browser.revealCurrentPatch();
    }
}

bool PerformanceView::isNarrow() const { return getWidth() < narrowWidth; }

void PerformanceView::setBrowserOpen(bool open)
{
    browserOpen = open;
    browser.setShowCloseButton(isNarrow());
    resized();
    repaint(); // the title highlight shows while the browser is open
}

void PerformanceView::paint(juce::Graphics &g)
{
    g.fillAll(Colours::background);

    // On narrow screens the patch title opens the browser; highlight it while open
    if (isNarrow() && browserOpen && !patchTitleArea.isEmpty())
    {
        auto t = patchTitleArea.toFloat().reduced(0.f, 1.f);
        g.setColour(Colours::accentDim);
        g.fillRoundedRectangle(t, 6.f);
        g.setColour(Colours::accent);
        g.drawRoundedRectangle(t, 6.f, 2.f);
    }

    auto drawPanel = [&g](juce::Rectangle<int> r, const char *title) {
        if (r.isEmpty())
            return;
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(r.toFloat(), 8.f);
        g.setColour(Colours::header);
        g.setFont(perfFont(11.f, juce::Font::bold));
        g.drawText(title, r.reduced(10, 4).removeFromTop(14), juce::Justification::topLeft);
    };

    drawPanel(sceneArea, "SCENE");
    drawPanel(filterArea, "FILTER");
    drawPanel(miscArea, "MISC");
    drawPanel(fxArea, "FX");
    drawPanel(macroArea, envelopesInOwnRow ? "MACROS" : "");
    drawPanel(ampEgArea, "AMP EG");
    drawPanel(filterEgArea, "FILTER EG");

    if (fxToggles.empty() && !fxToggleArea.isEmpty())
    {
        g.setColour(Colours::textDim);
        g.setFont(perfFont(13.f));
        g.drawText("No effects in this patch", fxToggleArea, juce::Justification::centred);
    }
}

void PerformanceView::resized()
{
    auto r = getLocalBounds().reduced(gap);
    bool narrow = isNarrow();

    // --- Top bar
    {
        auto top = r.removeFromTop(topBarHeight - gap);
        auto place = [&top](juce::Component &c, int w, bool fromRight = false) {
            auto b = fromRight ? top.removeFromRight(w) : top.removeFromLeft(w);
            c.setBounds(b);
            if (fromRight)
                top.removeFromRight(gap);
            else
                top.removeFromLeft(gap);
        };

        place(exitButton, 48);
        // Random sits on the left, away from the favorite star
        place(randomButton, 48);
        place(helpButton, 44, true);
        place(optionsButton, 48, true);
        place(saveButton, 48, true);
        place(favButton, 48, true);
        place(nextButton, 56, true);
        place(prevButton, 56);

        auto label = top;
        patchTitleArea = label;
        patchCategoryLabel.setBounds(label.removeFromTop(label.getHeight() * 2 / 5));
        patchNameLabel.setBounds(label);
    }
    r.removeFromTop(gap);

    // --- Browser
    juce::Rectangle<int> controls = r;
    browser.setShowCloseButton(narrow);
    if (narrow)
    {
        browser.setVisible(browserOpen);
        if (browserOpen)
            browser.setBounds(r);
    }
    else
    {
        browser.setVisible(true);
        auto bw = juce::jlimit(360, 560, (int)(r.getWidth() * 0.4f));
        browser.setBounds(controls.removeFromLeft(bw));
        controls.removeFromLeft(gap);
    }

    // --- Controls: [master | scene] / [amp eg | filter eg] / [macros] / [fx]
    // On short screens the envelopes share a tabbed panel with the macros instead.
    auto h = controls.getHeight();
    envelopesInOwnRow = h >= 600;

    auto rowA = controls.removeFromTop((int)(h * (envelopesInOwnRow ? 0.34f : 0.42f)));
    controls.removeFromTop(gap);
    juce::Rectangle<int> rowEg;
    if (envelopesInOwnRow)
    {
        rowEg = controls.removeFromTop((int)(h * 0.24f));
        controls.removeFromTop(gap);
    }
    // FX sits along the bottom, below the macros / envelopes
    auto rowB = controls.removeFromBottom(
        juce::jlimit(56, envelopesInOwnRow ? 88 : 96, (int)(h * (envelopesInOwnRow ? 0.13f : 0.18f))));
    controls.removeFromBottom(gap);
    auto rowC = controls;

    masterVolume->setBounds(rowA.removeFromLeft(72));
    rowA.removeFromLeft(gap);

    // Row A holds three inline panels: SCENE (A/B, scene mode, volume), FILTER (F1/F2,
    // cutoff, reso) and MISC (drive, porta, play mode, octave)
    {
        constexpr int pad = 8, titleH = 12, faderW = 56, tabW = 44;
        auto r = rowA;
        // When tall enough, play mode and octave sit under the MISC knobs instead of beside
        bool tall = r.getHeight() - 2 * pad - titleH >= 150;
        int sceneColW = juce::jlimit(110, 170, r.getWidth() / 6);
        // Wide enough for "Voices" between its - and + buttons
        int miscColW = tall ? 0 : juce::jlimit(100, 124, r.getWidth() / 7);
        int fixed = 3 * 2 * pad + 2 * gap + (sceneColW + gap + faderW) + (tabW + gap) +
                    (tall ? 0 : miscColW + gap);
        int kw = juce::jmax(40, (r.getWidth() - fixed) / 4);

        sceneArea = r.removeFromLeft(2 * pad + sceneColW + gap + faderW);
        r.removeFromLeft(gap);
        filterArea = r.removeFromLeft(2 * pad + tabW + gap + 2 * kw);
        r.removeFromLeft(gap);
        miscArea = r;

        auto content = [](juce::Rectangle<int> a) {
            a = a.reduced(pad);
            a.removeFromTop(titleH);
            return a;
        };
        auto twoKnobs = [](juce::Rectangle<int> c, PerfControl &a, PerfControl &b) {
            auto w = c.getWidth() / 2;
            a.setBounds(c.removeFromLeft(w).reduced(2, 0));
            b.setBounds(c.reduced(2, 0));
        };

        {
            auto c = content(sceneArea);
            sceneVolume->setBounds(c.removeFromRight(faderW));
            c.removeFromRight(gap);
            auto ab = c.removeFromTop(juce::jmin(44, (c.getHeight() - 4) / 3));
            sceneA.setBounds(ab.removeFromLeft(ab.getWidth() / 2 - 2));
            ab.removeFromLeft(4);
            sceneB.setBounds(ab);
            c.removeFromTop(4);
            sceneMode->setBounds(c);
        }

        {
            auto c = content(filterArea);
            auto tab = c.removeFromLeft(tabW);
            filterTab->setBounds(tab.withSizeKeepingCentre(tabW, juce::jmin(tab.getHeight(), 100)));
            c.removeFromLeft(gap);
            twoKnobs(c, *sceneControls[0], *sceneControls[1]);
        }

        {
            auto c = content(miscArea);
            juce::Rectangle<int> settings;
            if (tall)
            {
                settings = c.removeFromBottom(2 * 40 + 4);
                c.removeFromBottom(gap);
            }
            else
            {
                settings = c.removeFromRight(miscColW);
                c.removeFromRight(gap);
            }
            twoKnobs(c, *sceneControls[2], *sceneControls[3]);

            auto stepper = [](juce::Rectangle<int> a, juce::Component &down, juce::Component &label,
                              juce::Component &up) {
                auto stepW = juce::jmin(38, (int)(a.getWidth() * 0.3f));
                down.setBounds(a.removeFromLeft(stepW));
                up.setBounds(a.removeFromRight(stepW));
                label.setBounds(a);
            };
            if (tall)
            {
                // Play mode across the top, octave and voices side by side below
                playModeButton.setBounds(settings.removeFromTop((settings.getHeight() - 4) / 2));
                settings.removeFromTop(4);
                auto half = (settings.getWidth() - gap) / 2;
                stepper(settings.removeFromLeft(half), octaveDownButton, octaveLabel,
                        octaveUpButton);
                settings.removeFromLeft(gap);
                stepper(settings, voicesDownButton, voicesLabel, voicesUpButton);
            }
            else
            {
                // Stacked in the side column: play mode, octave, voices
                auto rowH = (settings.getHeight() - 8) / 3;
                playModeButton.setBounds(settings.removeFromTop(rowH));
                settings.removeFromTop(4);
                stepper(settings.removeFromTop(rowH), octaveDownButton, octaveLabel,
                        octaveUpButton);
                settings.removeFromTop(4);
                stepper(settings, voicesDownButton, voicesLabel, voicesUpButton);
            }
        }
    }

    fxArea = rowB;
    {
        auto f = rowB.reduced(8, 6);
        f.removeFromLeft(26); // panel title
        fxBypass->setBounds(f.removeFromLeft(juce::jlimit(160, 220, f.getWidth() / 4)));
        f.removeFromLeft(gap * 2);
        fxToggleArea = f;

        int n = (int)fxToggles.size();
        if (n > 0)
        {
            int cols = juce::jlimit(1, n, f.getWidth() / 68);
            int rows = (n + cols - 1) / cols;
            cols = (n + rows - 1) / rows;
            int bw = juce::jmin(110, (f.getWidth() - (cols - 1) * 4) / cols);
            int bh = (f.getHeight() - (rows - 1) * 4) / rows;
            for (int i = 0; i < n; ++i)
            {
                int c = i % cols, row = i / cols;
                fxToggles[i]->setBounds(f.getX() + c * (bw + 4), f.getY() + row * (bh + 4), bw,
                                        bh);
            }
        }
    }

    auto layoutRow = [](std::vector<std::unique_ptr<PerfControl>> &ctls, juce::Rectangle<int> a,
                        int maxW) {
        int n = (int)ctls.size();
        int cw = juce::jmin(maxW, a.getWidth() / juce::jmax(1, n));
        for (auto &c : ctls)
        {
            c->setVisible(true);
            c->setBounds(a.removeFromLeft(cw).reduced(2, 0));
        }
    };
    auto hideAll = [](std::vector<std::unique_ptr<PerfControl>> &ctls) {
        for (auto &c : ctls)
            c->setVisible(false);
    };

    macroArea = rowC;
    lowerPage->setVisible(!envelopesInOwnRow);

    if (envelopesInOwnRow)
    {
        // Split the envelope row in proportion to the number of knobs in each panel
        auto total = (float)(ampEg.size() + filterEg.size());
        // Filter EG first, then Amp EG, as in the full UI
        filterEgArea =
            rowEg.removeFromLeft((int)((rowEg.getWidth() - gap) * filterEg.size() / total));
        rowEg.removeFromLeft(gap);
        ampEgArea = rowEg;

        auto content = [](juce::Rectangle<int> a) {
            a = a.reduced(8);
            a.removeFromTop(12); // panel title
            return a;
        };
        layoutRow(ampEg, content(ampEgArea), 140);
        layoutRow(filterEg, content(filterEgArea), 140);

        auto m = content(rowC);
        int n = (int)macros.size();
        int cw = m.getWidth() / juce::jmax(1, n);
        for (auto &c : macros)
        {
            c->setVisible(true);
            c->setBounds(m.removeFromLeft(cw).reduced(4, 0));
        }
    }
    else
    {
        ampEgArea = {};
        filterEgArea = {};

        auto m = rowC.reduced(8);
        lowerPage->setBounds(m.removeFromLeft(juce::jlimit(84, 110, m.getWidth() / 8)));
        m.removeFromLeft(gap * 2);

        hideAll(macros);
        hideAll(ampEg);
        hideAll(filterEg);

        if (lowerPageIdx == pageAmpEg)
            layoutRow(ampEg, m, 160);
        else if (lowerPageIdx == pageFilterEg)
            layoutRow(filterEg, m, 160);
        else
        {
            int n = (int)macros.size();
            int cw = m.getWidth() / juce::jmax(1, n);
            for (auto &c : macros)
            {
                c->setVisible(true);
                c->setBounds(m.removeFromLeft(cw).reduced(4, 0));
            }
        }
    }

    // Slide-over browser sits above the controls on narrow screens
    if (narrow && browserOpen)
        browser.toFront(false);

    saveDialog.setBounds(getLocalBounds());
    if (saveDialog.isVisible())
        saveDialog.toFront(false);
}

} // namespace Mobile
} // namespace Surge
