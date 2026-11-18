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

#include "PerformanceSaveDialog.h"
#include "PerformanceColours.h"

#include "SurgeSynthesizer.h"
#include "SurgeStorage.h"

#include <set>

namespace Surge
{
namespace Mobile
{

namespace
{
constexpr int rowH = 42;
constexpr int pad = 10;
constexpr int labelW = 74;

void styleEditor(juce::TextEditor &ed, const juce::String &placeholder)
{
    ed.setFont(perfFont(17.f));
    ed.setIndents(10, 0);
    ed.setJustification(juce::Justification::centredLeft);
    ed.setTextToShowWhenEmpty(placeholder, Colours::textDim);
    ed.setColour(juce::TextEditor::backgroundColourId, Colours::panelRaised);
    ed.setColour(juce::TextEditor::textColourId, Colours::text);
    ed.setColour(juce::TextEditor::outlineColourId, Colours::outline);
    ed.setColour(juce::TextEditor::focusedOutlineColourId, Colours::accent);
    ed.setColour(juce::TextEditor::highlightColourId, Colours::accentDim);
    ed.setColour(juce::CaretComponent::caretColourId, Colours::accent);
}

std::string trimmed(const juce::TextEditor &ed) { return ed.getText().trim().toStdString(); }
} // namespace

PerformanceSaveDialog::PerformanceSaveDialog(std::function<SurgeSynthesizer *()> synthGetter)
    : getSynth(std::move(synthGetter))
{
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);

    styleEditor(nameEd, "Patch name");
    styleEditor(categoryEd, "Category");
    styleEditor(authorEd, "Author");
    styleEditor(commentEd, "Comment");

    // Return moves through the fields, then saves
    nameEd.onReturnKey = [this]() { categoryEd.grabKeyboardFocus(); };
    categoryEd.onReturnKey = [this]() { authorEd.grabKeyboardFocus(); };
    authorEd.onReturnKey = [this]() { commentEd.grabKeyboardFocus(); };
    commentEd.onReturnKey = [this]() { save(); };

    for (auto *ed : {&nameEd, &categoryEd, &authorEd, &commentEd})
    {
        ed->setSelectAllWhenFocused(false);
        ed->onEscapeKey = [this]() { close(); };
        addAndMakeVisible(*ed);
    }
    // Changing where the patch goes means the overwrite check has to happen again
    nameEd.onTextChange = [this]() { resetConfirmation(); };
    categoryEd.onTextChange = [this]() { resetConfirmation(); };

    saveButton.onClick = [this]() { save(); };
    cancelButton.onClick = [this]() { close(); };
    categoryButton.onClick = [this]() { showCategoryMenu(); };
    addAndMakeVisible(saveButton);
    addAndMakeVisible(cancelButton);
    addAndMakeVisible(categoryButton);
}

void PerformanceSaveDialog::open()
{
    auto s = getSynth ? getSynth() : nullptr;
    if (!s)
        return;

    auto &patch = s->storage.getPatch();
    nameEd.setText(patch.name, juce::dontSendNotification);
    categoryEd.setText(patch.category, juce::dontSendNotification);
    authorEd.setText(patch.author, juce::dontSendNotification);
    commentEd.setText(patch.comment, juce::dontSendNotification);
    resetConfirmation();
    setMessage("Saved to your user patches", false);

    setVisible(true);
    toFront(true);
    nameEd.grabKeyboardFocus();
    nameEd.selectAll();
}

void PerformanceSaveDialog::close()
{
    // Drop focus so the on-screen keyboard goes away
    if (auto *f = juce::Component::getCurrentlyFocusedComponent(); f && isParentOf(f))
        f->giveAwayKeyboardFocus();
    setVisible(false);
    if (onClose)
        onClose();
}

void PerformanceSaveDialog::setMessage(const std::string &m, bool isError)
{
    message = m;
    messageIsError = isError;
    repaint();
}

void PerformanceSaveDialog::resetConfirmation()
{
    if (confirmOverwrite)
    {
        confirmOverwrite = false;
        saveButton.setButtonText("Save");
        setMessage("Saved to your user patches", false);
    }
}

void PerformanceSaveDialog::showCategoryMenu()
{
    auto s = getSynth ? getSynth() : nullptr;
    if (!s)
        return;
    auto &st = s->storage;

    // Your own categories first, then the factory and third party ones
    std::vector<std::string> user, other;
    std::set<std::string> seen;
    for (int i = 0; i < (int)st.patch_category.size(); ++i)
    {
        auto &name = st.patch_category[i].name;
        if (name.empty() || !seen.insert(name).second)
            continue;
        (i >= st.firstUserCategory ? user : other).push_back(name);
    }
    std::sort(user.begin(), user.end());
    std::sort(other.begin(), other.end());

    juce::PopupMenu menu;
    auto add = [this, &menu](const std::string &c) {
        menu.addItem(c, [this, c]() {
            categoryEd.setText(c, juce::sendNotification);
            resetConfirmation();
        });
    };
    if (!user.empty())
    {
        menu.addSectionHeader("User");
        for (auto &c : user)
            add(c);
    }
    if (!other.empty())
    {
        juce::PopupMenu sub;
        for (auto &c : other)
            sub.addItem(c, [this, c]() { categoryEd.setText(c, juce::sendNotification); });
        menu.addSubMenu("Factory & Third Party", sub);
    }

    // Drawn inside this panel rather than as a separate native window, which is
    // unreliable on Android
    menu.showMenuAsync(juce::PopupMenu::Options()
                           .withParentComponent(this)
                           .withTargetComponent(&categoryButton)
                           .withMinimumWidth(240)
                           .withStandardItemHeight(40));
}

void PerformanceSaveDialog::save()
{
    auto s = getSynth ? getSynth() : nullptr;
    if (!s)
        return;
    auto &storage = s->storage;

    auto name = trimmed(nameEd);
    auto category = trimmed(categoryEd);
    if (category.empty())
        category = "Unsorted";

    if (name.empty())
    {
        setMessage("Please enter a patch name", true);
        nameEd.grabKeyboardFocus();
        return;
    }
    if (name.find_first_of("/\\:") != std::string::npos)
    {
        setMessage("Patch names can't contain / \\ or :", true);
        nameEd.grabKeyboardFocus();
        return;
    }
    if (category.find("..") != std::string::npos || category[0] == '/' || category[0] == '\\')
    {
        setMessage("Categories must be a folder name, like Bass or Leads/Mono", true);
        categoryEd.grabKeyboardFocus();
        return;
    }

    auto target = storage.userPatchesPath / string_to_path(category) / string_to_path(name + ".fxp");
    std::error_code ec;
    if (!confirmOverwrite && fs::exists(target, ec))
    {
        confirmOverwrite = true;
        saveButton.setButtonText("Overwrite");
        setMessage("'" + name + "' already exists in " + category + ". Tap Overwrite to replace it.",
                   true);
        return;
    }

    auto &patch = storage.getPatch();
    patch.name = name;
    patch.category = category;
    patch.author = trimmed(authorEd);
    patch.comment = commentEd.getText().toStdString();
    // As in the full save dialog: ignore whatever came from a host
    patch.dawExtraState.isPopulated = false;

    // The overwrite question was already answered above
    s->savePatch(/*factoryInPlace=*/false, /*skipOverwrite=*/true);
    s->refresh_editor = true;

    close();
}

void PerformanceSaveDialog::mouseUp(const juce::MouseEvent &e)
{
    // Tapping the dimmed area outside the panel cancels
    if (!panel.contains(e.getPosition()))
        close();
}

bool PerformanceSaveDialog::keyPressed(const juce::KeyPress &k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }
    return false;
}

void PerformanceSaveDialog::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.55f));

    g.setColour(Colours::panel);
    g.fillRoundedRectangle(panel.toFloat(), 10.f);
    g.setColour(Colours::outline);
    g.drawRoundedRectangle(panel.toFloat(), 10.f, 1.f);

    auto r = panel.reduced(pad);
    auto header = r.removeFromTop(rowH);
    g.setColour(Colours::text);
    g.setFont(perfFont(20.f, juce::Font::bold));
    g.drawText("Save Patch", header, juce::Justification::centredLeft);

    // Field labels
    g.setFont(perfFont(13.f, juce::Font::bold));
    g.setColour(Colours::textDim);
    for (auto *ed : {&nameEd, &categoryEd, &authorEd, &commentEd})
    {
        auto b = ed->getBounds();
        g.drawText(ed->getTextToShowWhenEmpty(),
                   juce::Rectangle<int>(b.getX() - labelW, b.getY(), labelW - 8, b.getHeight()),
                   juce::Justification::centredRight);
    }

    auto msg = panel.reduced(pad).removeFromBottom(20);
    g.setFont(perfFont(13.f));
    g.setColour(messageIsError ? Colours::accent : Colours::textDim);
    g.drawText(message, msg, juce::Justification::centredLeft, true);
}

void PerformanceSaveDialog::resized()
{
    auto b = getLocalBounds();
    int w = juce::jmin(920, b.getWidth() - 2 * pad);
    int h = pad + rowH + 8 + rowH + 8 + rowH + 6 + 20 + pad;
    // Keep to the top of the screen: the keyboard covers the bottom half in landscape
    panel = juce::Rectangle<int>(0, 0, w, h).withCentre({b.getCentreX(), 0}).withY(pad);

    auto r = panel.reduced(pad);
    auto header = r.removeFromTop(rowH);
    saveButton.setBounds(header.removeFromRight(130));
    header.removeFromRight(8);
    cancelButton.setBounds(header.removeFromRight(110));
    r.removeFromTop(8);

    auto layoutRow = [](juce::Rectangle<int> row, juce::Component &left, juce::Component &right,
                        juce::Component *extra) {
        auto half = (row.getWidth() - 12) / 2;
        auto l = row.removeFromLeft(half);
        row.removeFromLeft(12);
        l.removeFromLeft(labelW);
        left.setBounds(l);
        row.removeFromLeft(labelW);
        if (extra)
        {
            extra->setBounds(row.removeFromRight(rowH));
            row.removeFromRight(4);
        }
        right.setBounds(row);
    };

    layoutRow(r.removeFromTop(rowH), nameEd, categoryEd, &categoryButton);
    r.removeFromTop(8);
    layoutRow(r.removeFromTop(rowH), authorEd, commentEd, nullptr);
}

} // namespace Mobile
} // namespace Surge
