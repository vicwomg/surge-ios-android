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

#ifndef SURGE_SRC_SURGE_XT_GUI_MOBILE_PERFORMANCESAVEDIALOG_H
#define SURGE_SRC_SURGE_XT_GUI_MOBILE_PERFORMANCESAVEDIALOG_H

#include <functional>
#include <string>

#include <juce_gui_basics/juce_gui_basics.h>

#include "PerformanceIcons.h"

class SurgeSynthesizer;

namespace Surge
{
namespace Mobile
{

/*
 * A compact save panel for the Perform view. It sits in the top part of the screen so
 * the on-screen keyboard (which takes the bottom half in landscape) never covers it, and
 * handles the overwrite confirmation itself since the editor's prompts are hidden.
 */
class PerformanceSaveDialog : public juce::Component
{
  public:
    explicit PerformanceSaveDialog(std::function<SurgeSynthesizer *()> synthGetter);

    std::function<void()> onClose;

    // Fills the fields from the current patch and shows the panel
    void open();
    void close();

    void paint(juce::Graphics &g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent &e) override;
    bool keyPressed(const juce::KeyPress &k) override;

  private:
    void save();
    void showCategoryMenu();
    void setMessage(const std::string &m, bool isError);
    void resetConfirmation();

    std::function<SurgeSynthesizer *()> getSynth;

    juce::TextEditor nameEd, categoryEd, authorEd, commentEd;
    juce::TextButton saveButton{"Save"}, cancelButton{"Cancel"};
    Icons::IconButton categoryButton{Icons::IconButton::Down, "Choose Category"};

    juce::Rectangle<int> panel;
    std::string message;
    bool messageIsError{false};
    bool confirmOverwrite{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceSaveDialog)
};

} // namespace Mobile
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_MOBILE_PERFORMANCESAVEDIALOG_H
