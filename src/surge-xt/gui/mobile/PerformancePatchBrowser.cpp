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

#include "PerformancePatchBrowser.h"
#include "PerformanceColours.h"
#include "PerformanceIcons.h"

#include "SurgeStorage.h"

#include <algorithm>

namespace Surge
{
namespace Mobile
{

namespace
{
bool isSep(char c) { return c == '/' || c == '\\'; }

std::string lastPathComponent(const std::string &s)
{
    auto pos = s.find_last_of("/\\");
    return pos == std::string::npos ? s : s.substr(pos + 1);
}

int pathDepth(const std::string &s) { return (int)std::count_if(s.begin(), s.end(), isSep); }

bool startsWithPath(const std::string &s, const std::string &prefix)
{
    return s.size() > prefix.size() && s.compare(0, prefix.size(), prefix) == 0 &&
           isSep(s[prefix.size()]);
}

void configureList(juce::ListBox &lb)
{
    lb.setRowHeight(PerformancePatchBrowser::rowHeight);
    lb.setColour(juce::ListBox::backgroundColourId, Colours::panel);
    lb.setColour(juce::ListBox::outlineColourId, Colours::outline);
    lb.setOutlineThickness(1);
    lb.setRowSelectedOnMouseDown(false);
    // Single-finger drag scrolls; a tap without dragging selects (handled by juce::ListBox)
    lb.getViewport()->setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    lb.getViewport()->setScrollBarThickness(6);
}
} // namespace

struct PerformancePatchBrowser::CategoryModel : public juce::ListBoxModel
{
    explicit CategoryModel(PerformancePatchBrowser &o) : owner(o) {}

    int getNumRows() override { return (int)owner.categoryRows.size(); }

    void paintListBoxItem(int row, juce::Graphics &g, int w, int h, bool) override
    {
        if (row < 0 || row >= (int)owner.categoryRows.size())
            return;

        const auto &cr = owner.categoryRows[row];
        auto r = juce::Rectangle<int>(0, 0, w, h);

        if (cr.kind == CategoryRow::Header)
        {
            g.setColour(Colours::background);
            g.fillRect(r);
            g.setColour(Colours::header);
            g.setFont(perfFont(13.f, juce::Font::bold));
            g.drawText(cr.label, r.reduced(12, 0), juce::Justification::centredLeft);
            return;
        }

        bool selected = row == owner.selectedCategoryRow && owner.searchField.isEmpty();

        if (selected)
        {
            g.setColour(Colours::accentDim);
            g.fillRect(r);
        }

        g.setColour(Colours::outline.withAlpha(0.5f));
        g.drawHorizontalLine(h - 1, 0.f, (float)w);

        auto tr = r.reduced(12, 0).withTrimmedLeft(cr.depth * 16);

        if (cr.collapsible)
        {
            g.setColour(Colours::textDim);
            g.fillPath(Icons::disclosure(tr.removeFromRight(14).toFloat().withSizeKeepingCentre(10, 10),
                                         cr.expanded));
            tr.removeFromRight(4);
        }

        if (cr.kind == CategoryRow::Favorites)
        {
            g.setColour(Colours::accent);
            g.fillPath(Icons::star(tr.removeFromLeft(18).toFloat().withSizeKeepingCentre(16, 16)));
            tr.removeFromLeft(8);
        }

        g.setColour(selected ? Colours::text : Colours::text.withAlpha(0.85f));
        g.setFont(perfFont(17.f));
        g.drawText(cr.label, tr, juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent &) override
    {
        owner.categoryRowClicked(row);
    }

    PerformancePatchBrowser &owner;
};

struct PerformancePatchBrowser::PatchModel : public juce::ListBoxModel
{
    explicit PatchModel(PerformancePatchBrowser &o) : owner(o) {}

    int getNumRows() override { return (int)owner.patchRows.size(); }

    void paintListBoxItem(int row, juce::Graphics &g, int w, int h, bool) override
    {
        if (row < 0 || row >= (int)owner.patchRows.size() || !owner.storage)
            return;

        const auto &pr = owner.patchRows[row];
        auto r = juce::Rectangle<int>(0, 0, w, h);
        bool current = pr.patchId == owner.currentPatch;

        if (current)
        {
            g.setColour(Colours::accentDim);
            g.fillRect(r);
            g.setColour(Colours::accent);
            g.fillRect(r.withWidth(4));
        }

        g.setColour(Colours::outline.withAlpha(0.5f));
        g.drawHorizontalLine(h - 1, 0.f, (float)w);

        auto tr = r.reduced(14, 0);

        bool fav = pr.patchId >= 0 && pr.patchId < (int)owner.storage->patch_list.size() &&
                   owner.storage->patch_list[pr.patchId].isFavorite;
        if (fav)
        {
            g.setColour(Colours::accent);
            g.fillPath(Icons::star(tr.removeFromRight(22).toFloat().withSizeKeepingCentre(14, 14)));
        }

        if (!pr.detail.empty())
        {
            g.setColour(Colours::textDim);
            g.setFont(perfFont(12.f));
            g.drawText(pr.detail, tr.removeFromBottom(h / 2 - 2).withTrimmedBottom(4),
                       juce::Justification::topLeft, true);
            tr = tr.withTrimmedTop(4);
        }

        g.setColour(current ? Colours::text : Colours::text.withAlpha(0.9f));
        g.setFont(perfFont(17.f, current ? juce::Font::bold : juce::Font::plain));
        g.drawText(pr.label, tr,
                   pr.detail.empty() ? juce::Justification::centredLeft
                                     : juce::Justification::bottomLeft,
                   true);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent &) override
    {
        if (row >= 0 && row < (int)owner.patchRows.size() && owner.onPatchChosen)
        {
            owner.onPatchChosen(owner.patchRows[row].patchId);
        }
    }

    PerformancePatchBrowser &owner;
};

PerformancePatchBrowser::PerformancePatchBrowser()
{
    categoryModel = std::make_unique<CategoryModel>(*this);
    patchModel = std::make_unique<PatchModel>(*this);

    categoryList.setModel(categoryModel.get());
    patchList.setModel(patchModel.get());
    configureList(categoryList);
    configureList(patchList);
    categoryList.setTitle("Patch Categories");
    patchList.setTitle("Patches");

    searchField.setTextToShowWhenEmpty("Search patches", Colours::textDim);
    searchField.setFont(perfFont(17.f));
    searchField.setIndents(10, 10);
    searchField.setColour(juce::TextEditor::backgroundColourId, Colours::panel);
    searchField.setColour(juce::TextEditor::outlineColourId, Colours::outline);
    searchField.setColour(juce::TextEditor::focusedOutlineColourId, Colours::accent);
    searchField.setColour(juce::TextEditor::textColourId, Colours::text);
    searchField.setTitle("Search Patches");
    // Keep typed text clear of the clear button at the right end
    searchField.setBorder(juce::BorderSize<int>(1, 1, 1, 36));
    searchField.onTextChange = [this]() {
        clearSearchButton.setVisible(!searchField.isEmpty());
        updatePatchRows();
        categoryList.repaint();
    };
    searchField.onReturnKey = [this]() { searchField.unfocusAllComponents(); };

    topButton.setTriggeredOnMouseDown(true);
    topButton.onClick = [this]() {
        categoryList.getViewport()->setViewPosition(0, 0);
        patchList.getViewport()->setViewPosition(0, 0);
    };

    addAndMakeVisible(searchField);
    addAndMakeVisible(topButton);

    // Jumps straight to the Favorites folder (always the first category row when present)
    favoritesButton.setTriggeredOnMouseDown(true);
    favoritesButton.setEnabled(false);
    favoritesButton.onClick = [this]() {
        if (categoryRows.empty() || categoryRows[0].kind != CategoryRow::Favorites)
            return;
        selectCategoryRow(0);
        categoryList.getViewport()->setViewPosition(0, 0);
    };
    addAndMakeVisible(favoritesButton);

    closeButton.onClick = [this]() {
        if (onCloseRequested)
            onCloseRequested();
    };
    addChildComponent(closeButton);

    clearSearchButton.setTriggeredOnMouseDown(true);
    clearSearchButton.setWantsKeyboardFocus(false);
    clearSearchButton.setMouseClickGrabsKeyboardFocus(false);
    clearSearchButton.onClick = [this]() { clearSearch(); };
    addChildComponent(clearSearchButton);
    addAndMakeVisible(categoryList);
    addAndMakeVisible(patchList);
}

PerformancePatchBrowser::~PerformancePatchBrowser()
{
    categoryList.setModel(nullptr);
    patchList.setModel(nullptr);
}

void PerformancePatchBrowser::rebuild(SurgeStorage *s)
{
    storage = s;
    builtPatchCount = storage ? storage->patch_list.size() : 0;

    if (currentPatch >= 0)
        expandThirdPartyForPatch(currentPatch);

    buildCategoryRows();

    if (!storage)
        return;

    // Keep the selection on something sensible
    int target = currentPatch >= 0 ? categoryRowForPatch(currentPatch) : -1;
    if (target < 0)
    {
        for (int i = 0; i < (int)categoryRows.size(); ++i)
        {
            if (categoryRows[i].kind != CategoryRow::Header)
            {
                target = i;
                break;
            }
        }
    }
    selectCategoryRow(target);
}

void PerformancePatchBrowser::buildCategoryRows()
{
    categoryRows.clear();

    if (!storage)
    {
        categoryList.updateContent();
        return;
    }

    if (!storage->favoritesOrdering.empty())
    {
        CategoryRow fav;
        fav.kind = CategoryRow::Favorites;
        fav.label = "Favorites";
        categoryRows.push_back(fav);
    }
    favoritesButton.setEnabled(!storage->favoritesOrdering.empty());

    const int n = (int)storage->patchCategoryOrdering.size();
    int tp = juce::jlimit(0, n, storage->firstThirdPartyCategory);
    int us = juce::jlimit(tp, n, storage->firstUserCategory);

    struct Section
    {
        const char *name;
        int start, end;
        bool collapsible;
    } sections[] = {{"USER", us, n, false},
                    {"FACTORY", 0, tp, false},
                    {"THIRD PARTY", tp, us, true}};

    for (auto &sec : sections)
    {
        bool headerAdded = false;

        for (int i = sec.start; i < sec.end; ++i)
        {
            int c = storage->patchCategoryOrdering[i];
            if (c < 0 || c >= (int)storage->patch_category.size())
                continue;

            const auto &cat = storage->patch_category[c];
            if (cat.numberOfPatchesInCategoryAndChildren == 0)
                continue;

            CategoryRow cr;
            cr.kind = CategoryRow::Category;
            cr.label = lastPathComponent(cat.name);
            cr.categoryId = c;
            cr.depth = pathDepth(cat.name);
            cr.sectionStart = sec.start;
            cr.sectionEnd = sec.end;
            auto sepPos = cat.name.find_first_of("/\\");
            cr.rootName = sepPos == std::string::npos ? cat.name : cat.name.substr(0, sepPos);

            if (sec.collapsible)
            {
                bool rootExpanded = expandedThirdParty.count(cr.rootName) > 0;

                // Only top-level (author) folders show until expanded
                if (cr.depth > 0 && !rootExpanded)
                    continue;

                if (cr.depth == 0)
                {
                    // Collapsible only if it has sub-folders
                    for (int j = sec.start; j < sec.end; ++j)
                    {
                        int cj = storage->patchCategoryOrdering[j];
                        if (cj >= 0 && cj < (int)storage->patch_category.size() &&
                            startsWithPath(storage->patch_category[cj].name, cat.name))
                        {
                            cr.collapsible = true;
                            break;
                        }
                    }
                    cr.expanded = cr.collapsible && rootExpanded;
                }
            }

            if (!headerAdded)
            {
                CategoryRow h;
                h.kind = CategoryRow::Header;
                h.label = sec.name;
                categoryRows.push_back(h);
                headerAdded = true;
            }

            categoryRows.push_back(cr);
        }
    }

    categoryList.updateContent();
    categoryList.repaint();
}

int PerformancePatchBrowser::findCategoryRow(int categoryId) const
{
    for (int i = 0; i < (int)categoryRows.size(); ++i)
    {
        if (categoryRows[i].kind == CategoryRow::Category &&
            categoryRows[i].categoryId == categoryId)
            return i;
    }
    return -1;
}

bool PerformancePatchBrowser::expandThirdPartyForPatch(int patchId)
{
    if (!storage || patchId < 0 || patchId >= (int)storage->patch_list.size())
        return false;

    int cat = storage->patch_list[patchId].category;
    if (cat < 0 || cat >= (int)storage->patch_category.size())
        return false;

    const int n = (int)storage->patchCategoryOrdering.size();
    int tp = juce::jlimit(0, n, storage->firstThirdPartyCategory);
    int us = juce::jlimit(tp, n, storage->firstUserCategory);

    for (int i = tp; i < us; ++i)
    {
        if (storage->patchCategoryOrdering[i] == cat)
        {
            const auto &name = storage->patch_category[cat].name;
            auto sepPos = name.find_first_of("/\\");
            if (sepPos == std::string::npos)
                return false; // top level, always visible
            return expandedThirdParty.insert(name.substr(0, sepPos)).second;
        }
    }
    return false;
}

void PerformancePatchBrowser::categoryRowClicked(int row)
{
    if (row < 0 || row >= (int)categoryRows.size())
        return;

    auto cr = categoryRows[row];

    if (cr.collapsible)
    {
        // First tap expands and selects; tapping the selected, expanded folder collapses it
        if (!cr.expanded)
            expandedThirdParty.insert(cr.rootName);
        else if (row == selectedCategoryRow && searchField.isEmpty())
            expandedThirdParty.erase(cr.rootName);

        buildCategoryRows();
        row = findCategoryRow(cr.categoryId);
    }

    selectCategoryRow(row);
}

bool PerformancePatchBrowser::categoryContainsPatch(const CategoryRow &cr, int patchId) const
{
    if (!storage || patchId < 0 || patchId >= (int)storage->patch_list.size())
        return false;

    const auto &p = storage->patch_list[patchId];

    if (cr.kind == CategoryRow::Favorites)
        return p.isFavorite;

    if (cr.kind != CategoryRow::Category)
        return false;

    if (p.category == cr.categoryId)
        return true;

    if (p.category < 0 || p.category >= (int)storage->patch_category.size())
        return false;

    // Descendant categories live in the same section and share the path prefix
    if (!startsWithPath(storage->patch_category[p.category].name,
                        storage->patch_category[cr.categoryId].name))
        return false;

    for (int i = cr.sectionStart; i < cr.sectionEnd; ++i)
    {
        if (storage->patchCategoryOrdering[i] == p.category)
            return true;
    }
    return false;
}

int PerformancePatchBrowser::categoryRowForPatch(int patchId) const
{
    if (!storage || patchId < 0 || patchId >= (int)storage->patch_list.size())
        return -1;

    return findCategoryRow(storage->patch_list[patchId].category);
}

void PerformancePatchBrowser::selectCategoryRow(int row)
{
    if (row < 0 || row >= (int)categoryRows.size() ||
        categoryRows[row].kind == CategoryRow::Header)
        return;

    selectedCategoryRow = row;

    clearSearch();

    updatePatchRows();
    categoryList.repaint();
    patchList.getViewport()->setViewPosition(0, 0);

    for (int i = 0; i < (int)patchRows.size(); ++i)
    {
        if (patchRows[i].patchId == currentPatch)
        {
            patchList.scrollToEnsureRowIsOnscreen(i);
            break;
        }
    }
}

void PerformancePatchBrowser::updatePatchRows()
{
    patchRows.clear();

    if (storage)
    {
        auto query = searchField.getText().trim();

        if (query.isNotEmpty())
        {
            for (auto p : storage->patchOrdering)
            {
                const auto &patch = storage->patch_list[p];
                if (juce::String(patch.name).containsIgnoreCase(query))
                {
                    std::string detail;
                    if (patch.category >= 0 && patch.category < (int)storage->patch_category.size())
                        detail = storage->patch_category[patch.category].name;
                    patchRows.push_back({p, patch.name, detail});
                }
            }
        }
        else if (selectedCategoryRow >= 0 && selectedCategoryRow < (int)categoryRows.size())
        {
            const auto &cr = categoryRows[selectedCategoryRow];

            if (cr.kind == CategoryRow::Favorites)
            {
                for (auto p : storage->favoritesOrdering)
                {
                    if (p < 0 || p >= (int)storage->patch_list.size())
                        continue;
                    const auto &patch = storage->patch_list[p];
                    std::string detail;
                    if (patch.category >= 0 && patch.category < (int)storage->patch_category.size())
                        detail = storage->patch_category[patch.category].name;
                    patchRows.push_back({p, patch.name, detail});
                }
            }
            else
            {
                for (auto p : storage->patchOrdering)
                {
                    if (!categoryContainsPatch(cr, p))
                        continue;

                    const auto &patch = storage->patch_list[p];
                    std::string detail;
                    if (patch.category != cr.categoryId)
                        detail = lastPathComponent(storage->patch_category[patch.category].name);
                    patchRows.push_back({p, patch.name, detail});
                }
            }
        }
    }

    patchList.updateContent();
    patchList.repaint();
}

void PerformancePatchBrowser::refreshPatches()
{
    // Favorites may have appeared or disappeared, which changes the category rows
    bool hasFavRow = !categoryRows.empty() && categoryRows[0].kind == CategoryRow::Favorites;
    bool needFavRow = storage && !storage->favoritesOrdering.empty();

    if (hasFavRow != needFavRow)
    {
        bool hadSel = selectedCategoryRow >= 0 && selectedCategoryRow < (int)categoryRows.size();
        auto prevKind = hadSel ? categoryRows[selectedCategoryRow].kind : CategoryRow::Header;
        int prevCat = hadSel ? categoryRows[selectedCategoryRow].categoryId : -1;

        buildCategoryRows();

        int row = -1;
        if (prevKind == CategoryRow::Category)
            row = findCategoryRow(prevCat);
        else if (prevKind == CategoryRow::Favorites && needFavRow)
            row = 0;
        if (row < 0)
            row = categoryRowForPatch(currentPatch);
        selectCategoryRow(row);
        return;
    }

    auto pos = patchList.getViewport()->getViewPosition();
    updatePatchRows();
    patchList.getViewport()->setViewPosition(pos);
}

void PerformancePatchBrowser::setCurrentPatch(int patchId)
{
    if (patchId == currentPatch)
        return;

    currentPatch = patchId;

    // Follow the loaded patch into its category unless the user is searching
    // or looking at favorites
    bool followCategory = searchField.isEmpty();
    if (followCategory && selectedCategoryRow >= 0 &&
        selectedCategoryRow < (int)categoryRows.size())
    {
        const auto &cr = categoryRows[selectedCategoryRow];
        if (categoryContainsPatch(cr, patchId))
            followCategory = false;
    }

    if (followCategory)
    {
        if (expandThirdPartyForPatch(patchId))
            buildCategoryRows();

        int row = categoryRowForPatch(patchId);
        if (row >= 0)
        {
            selectCategoryRow(row);
            categoryList.scrollToEnsureRowIsOnscreen(row);
            return;
        }
    }

    patchList.repaint();
    for (int i = 0; i < (int)patchRows.size(); ++i)
    {
        if (patchRows[i].patchId == currentPatch)
        {
            patchList.scrollToEnsureRowIsOnscreen(i);
            break;
        }
    }
}

void PerformancePatchBrowser::revealCurrentPatch()
{
    clearSearch();

    if (expandThirdPartyForPatch(currentPatch))
        buildCategoryRows();

    int row = categoryRowForPatch(currentPatch);
    if (row >= 0)
    {
        selectCategoryRow(row);
        categoryList.scrollToEnsureRowIsOnscreen(row);
    }
}

void PerformancePatchBrowser::clearSearch()
{
    if (searchField.isEmpty())
        return;
    searchField.setText({}, juce::sendNotification);
    clearSearchButton.setVisible(false);
}

void PerformancePatchBrowser::paint(juce::Graphics &g) { g.fillAll(Colours::background); }

void PerformancePatchBrowser::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop(40);
    if (closeButton.isVisible())
    {
        closeButton.setBounds(top.removeFromRight(48));
        top.removeFromRight(6);
    }
    topButton.setBounds(top.removeFromRight(48));
    top.removeFromRight(6);
    favoritesButton.setBounds(top.removeFromRight(48));
    top.removeFromRight(6);
    searchField.setBounds(top);
    clearSearchButton.setBounds(top.removeFromRight(40));
    clearSearchButton.toFront(false);
    r.removeFromTop(6);
    auto catW = juce::jlimit(120, 260, (int)(r.getWidth() * 0.42f));
    categoryList.setBounds(r.removeFromLeft(catW));
    r.removeFromLeft(6);
    patchList.setBounds(r);
}

} // namespace Mobile
} // namespace Surge
