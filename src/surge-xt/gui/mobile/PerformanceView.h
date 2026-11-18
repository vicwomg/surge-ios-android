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

#ifndef SURGE_SRC_SURGE_XT_GUI_MOBILE_PERFORMANCEVIEW_H
#define SURGE_SRC_SURGE_XT_GUI_MOBILE_PERFORMANCEVIEW_H

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "PerformancePatchBrowser.h"
#include "PerformanceIcons.h"
#include "PerformanceSaveDialog.h"

class SurgeSynthEditor;
class SurgeGUIEditor;
class SurgeSynthesizer;
class Parameter;

namespace Surge
{
namespace Mobile
{

class PerfControl;
class SegmentedButtons;
class FxToggle;
struct PerfBinding;

/*
 * A full-screen, touch-first "Perform" view for the mobile standalone apps. It sits beside
 * the regular Surge editor (which stays alive but hidden) and talks to the synth through
 * the same SurgeGUIEditor, polling its state on a timer so it follows MIDI, program changes
 * and patch loads.
 */
class PerformanceView : public juce::Component, private juce::Timer
{
  public:
    using EditorGetter = std::function<SurgeSynthEditor *()>;

    explicit PerformanceView(EditorGetter getter);
    ~PerformanceView() override;

    std::function<void()> onExitRequested;
    std::function<void()> onOptionsRequested;
    std::function<void()> onHelpRequested;

    // Starts or stops synth polling. Call when the view is shown or hidden.
    void setActive(bool active);

    void paint(juce::Graphics &g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent &e) override;

  private:
    void timerCallback() override;
    void syncFromSynth();
    void rebuildFxToggles();
    void setBrowserOpen(bool open);
    bool isNarrow() const;

    SurgeGUIEditor *getSGE() const;
    SurgeSynthesizer *getSynth() const;

    void loadPatch(int patchId);
    void jogPatch(bool next);
    void randomPatch();
    void toggleFavorite();
    void setIntParam(Parameter *p, int value);
    void showPlayModeMenu();
    void shiftOctave(int delta);
    void stepVoices(int dir);

    PerfBinding paramBinding(std::function<Parameter *()> resolve, std::string title);
    PerfBinding macroBinding(int macro);

    EditorGetter getEditor;

    std::unique_ptr<juce::LookAndFeel_V4> lookAndFeel;

    // Top bar
    juce::TextButton helpButton{"?"};
    Icons::IconButton exitButton{Icons::IconButton::Expand, "Full UI"};
    Icons::IconButton prevButton{Icons::IconButton::Prev, "Previous Patch"},
        nextButton{Icons::IconButton::Next, "Next Patch"},
        randomButton{Icons::IconButton::Random, "Random Patch"},
        favButton{Icons::IconButton::Star, "Toggle Favorite"},
        saveButton{Icons::IconButton::Save, "Save Patch"},
        optionsButton{Icons::IconButton::Gear, "Options"};
    juce::Label patchCategoryLabel, patchNameLabel;

    // Browser
    PerformancePatchBrowser browser;
    bool browserOpen{false};

    PerformanceSaveDialog saveDialog;

    // Controls
    std::unique_ptr<PerfControl> masterVolume;
    std::vector<std::unique_ptr<PerfControl>> sceneControls;
    std::unique_ptr<PerfControl> sceneVolume;

    // Which filter (0 or 1) the Cutoff and Reso knobs edit
    std::unique_ptr<SegmentedButtons> filterTab;
    int filterTabIdx{0};

    juce::TextButton playModeButton, octaveDownButton{"-"}, octaveUpButton{"+"};
    juce::Label octaveLabel;
    // Patch polyphony limit, the main CPU lever on slower devices
    juce::TextButton voicesDownButton{"-"}, voicesUpButton{"+"};
    juce::Label voicesLabel;
    std::vector<std::unique_ptr<PerfControl>> macros;
    std::vector<std::unique_ptr<PerfControl>> ampEg, filterEg;
    juce::TextButton sceneA{"A"}, sceneB{"B"};
    std::unique_ptr<SegmentedButtons> sceneMode, fxBypass;

    // On short screens the macro panel becomes tabbed: Macros / Amp EG / Filter EG
    std::unique_ptr<SegmentedButtons> lowerPage;
    // Tabs follow the full UI's flow: Filter EG, Amp EG, Macros (shown first)
    enum LowerPage
    {
        pageFilterEg = 0,
        pageAmpEg,
        pageMacros
    };
    int lowerPageIdx{pageMacros};
    bool envelopesInOwnRow{true};
    std::vector<std::unique_ptr<FxToggle>> fxToggles;
    std::array<int, 16> lastFxTypes{};
    bool fxTypesValid{false};

    juce::Rectangle<int> patchTitleArea;
    juce::Rectangle<int> sceneArea, filterArea, miscArea, fxArea, macroArea, fxToggleArea, ampEgArea, filterEgArea;

    int lastPatchId{-2};
    std::string lastPatchName, lastPatchCategory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceView)
};

} // namespace Mobile
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_MOBILE_PERFORMANCEVIEW_H
