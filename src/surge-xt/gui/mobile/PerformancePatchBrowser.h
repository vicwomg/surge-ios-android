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

#ifndef SURGE_SRC_SURGE_XT_GUI_MOBILE_PERFORMANCEPATCHBROWSER_H
#define SURGE_SRC_SURGE_XT_GUI_MOBILE_PERFORMANCEPATCHBROWSER_H

#include <functional>
#include <set>
#include <string>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "PerformanceIcons.h"

class SurgeStorage;

namespace Surge
{
namespace Mobile
{

/*
 * A touch-friendly two-column patch browser: categories on the left, patches on the right,
 * with a search field above. Rows are large and the lists scroll by dragging.
 */
class PerformancePatchBrowser : public juce::Component
{
  public:
    PerformancePatchBrowser();
    ~PerformancePatchBrowser() override;

    // Called whenever the browser wants a patch loaded (index into storage->patch_list).
    std::function<void(int)> onPatchChosen;

    // Called when the close button is tapped. The button only shows when enabled here,
    // i.e. when the browser is a slide-over panel.
    std::function<void()> onCloseRequested;
    void setShowCloseButton(bool show)
    {
        if (show != closeButton.isVisible())
        {
            closeButton.setVisible(show);
            resized();
        }
    }

    // Rebuilds the category list from storage. Call when the patch list changes.
    void rebuild(SurgeStorage *storage);

    // Highlights (and, if needed, selects the category of) the current patch.
    void setCurrentPatch(int patchId);

    // Clears any search and shows the current patch in its category
    void revealCurrentPatch();

    // Re-filters the patch column (e.g. after favorites changed).
    void refreshPatches();

    size_t getBuiltPatchCount() const { return builtPatchCount; }

    void resized() override;
    void paint(juce::Graphics &g) override;

    static constexpr int rowHeight = 46;

  private:
    struct CategoryRow
    {
        enum Kind
        {
            Header,
            Favorites,
            Category
        } kind{Category};
        std::string label;
        int categoryId{-1};
        int depth{0};
        int sectionStart{0}, sectionEnd{0}; // range in patchCategoryOrdering for descendants

        // Third party patches are grouped by author and collapsed until tapped
        bool collapsible{false}, expanded{false};
        std::string rootName; // first path component of the category name
    };

    struct PatchRow
    {
        int patchId;
        std::string label;
        std::string detail;
    };

    struct CategoryModel;
    struct PatchModel;

    void buildCategoryRows();
    void categoryRowClicked(int row);
    void selectCategoryRow(int row);
    int findCategoryRow(int categoryId) const;
    bool expandThirdPartyForPatch(int patchId);
    void updatePatchRows();
    void clearSearch();
    bool categoryContainsPatch(const CategoryRow &cr, int patchId) const;
    int categoryRowForPatch(int patchId) const;

    SurgeStorage *storage{nullptr};
    size_t builtPatchCount{0};
    int currentPatch{-1};
    int selectedCategoryRow{-1};

    std::vector<CategoryRow> categoryRows;
    std::set<std::string> expandedThirdParty;
    std::vector<PatchRow> patchRows;

    std::unique_ptr<CategoryModel> categoryModel;
    std::unique_ptr<PatchModel> patchModel;

    juce::TextEditor searchField;
    Icons::IconButton topButton{Icons::IconButton::Up, "Scroll to Top"};
    Icons::IconButton favoritesButton{Icons::IconButton::StarFilled, "Show Favorites"};
    Icons::IconButton closeButton{Icons::IconButton::Close, "Close Patch Browser"};
    Icons::IconButton clearSearchButton{Icons::IconButton::Clear, "Clear Search"};
    juce::ListBox categoryList, patchList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformancePatchBrowser)
};

} // namespace Mobile
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_MOBILE_PERFORMANCEPATCHBROWSER_H
