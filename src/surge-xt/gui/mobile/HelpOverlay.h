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

#ifndef SURGE_SRC_SURGE_XT_GUI_MOBILE_HELPOVERLAY_H
#define SURGE_SRC_SURGE_XT_GUI_MOBILE_HELPOVERLAY_H

#include <string>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "PerformanceColours.h"

namespace Surge
{
namespace Mobile
{

/*
 * A compact help panel for landscape phones and tablets: sections flow into two columns
 * when there's room, with small text and tight spacing, and the panel scrolls if it
 * still doesn't fit.
 */
class HelpOverlay : public juce::Component
{
  public:
    struct Section
    {
        std::string title, body;
    };

    HelpOverlay(std::string titleText, std::vector<Section> s)
        : title(std::move(titleText)), sections(std::move(s))
    {
        okButton.setColour(juce::TextButton::buttonColourId, Colours::accent);
        okButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        okButton.onClick = [this]() { setVisible(false); };
        addAndMakeVisible(okButton);

        viewport.setViewedComponent(&content, false);
        viewport.setScrollBarsShown(true, false);
        viewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
        addAndMakeVisible(viewport);
    }

    void show()
    {
        setVisible(true);
        toFront(false);
        viewport.setViewPosition(0, 0);
    }

    void paint(juce::Graphics &g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.55f));
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(panel.toFloat(), 10.f);
        g.setColour(Colours::outline);
        g.drawRoundedRectangle(panel.toFloat(), 10.f, 1.f);

        g.setColour(Colours::text);
        g.setFont(perfFont(17.f, juce::Font::bold));
        g.drawText(title, header, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(12, 8);
        panel = b.withSizeKeepingCentre(juce::jmin(b.getWidth(), 980), b.getHeight());

        auto r = panel.reduced(14, 8);
        header = r.removeFromTop(36);
        okButton.setBounds(header.removeFromRight(90).reduced(0, 2));
        r.removeFromTop(4);

        viewport.setBounds(r);
        content.layout(sections, r.getWidth() - viewport.getScrollBarThickness());

        // Shrink the panel to the content when it's short
        auto spare = r.getHeight() - content.getHeight();
        if (spare > 0)
        {
            panel = panel.withSizeKeepingCentre(panel.getWidth(), panel.getHeight() - spare);
            resizedWithinPanel();
        }
    }

    void mouseUp(const juce::MouseEvent &e) override
    {
        if (!panel.contains(e.getPosition()))
            setVisible(false);
    }

  private:
    void resizedWithinPanel()
    {
        auto r = panel.reduced(14, 8);
        header = r.removeFromTop(36);
        okButton.setBounds(header.removeFromRight(90).reduced(0, 2));
        r.removeFromTop(4);
        viewport.setBounds(r);
    }

    struct Content : public juce::Component
    {
        static constexpr float titleSize = 14.f, bodySize = 13.f;
        static constexpr int colGap = 24, sectionGap = 8;

        struct Placed
        {
            juce::TextLayout layout;
            juce::Rectangle<float> area;
        };
        std::vector<Placed> placed;

        void layout(const std::vector<Section> &sections, int width)
        {
            int cols = width >= 640 ? 2 : 1;
            float colW = (float)(width - (cols - 1) * colGap) / cols;

            std::vector<juce::TextLayout> layouts;
            float total = 0;
            for (auto &s : sections)
            {
                juce::AttributedString as;
                as.append(s.title + "\n", perfFont(titleSize, juce::Font::bold), Colours::accent);
                as.append(s.body, perfFont(bodySize), Colours::text);
                as.setLineSpacing(1.f);
                juce::TextLayout tl;
                tl.createLayout(as, colW);
                total += tl.getHeight() + sectionGap;
                layouts.push_back(std::move(tl));
            }

            // Fill columns in order, moving on once a column holds its share
            placed.clear();
            float target = total / cols, y = 0, maxY = 0;
            int col = 0;
            for (auto &tl : layouts)
            {
                auto h = tl.getHeight();
                if (col < cols - 1 && y > 0 && y + h / 2 > target)
                {
                    ++col;
                    y = 0;
                }
                placed.push_back({tl, {col * (colW + colGap), y, colW, h}});
                y += h + sectionGap;
                maxY = juce::jmax(maxY, y);
            }
            setSize(width, (int)std::ceil(maxY));
            repaint();
        }

        void paint(juce::Graphics &g) override
        {
            for (auto &p : placed)
                p.layout.draw(g, p.area);
        }
    };

    std::string title;
    std::vector<Section> sections;
    juce::TextButton okButton{"OK"};
    juce::Viewport viewport;
    Content content;
    juce::Rectangle<int> panel, header;
};

} // namespace Mobile
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_MOBILE_HELPOVERLAY_H
