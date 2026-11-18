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

#ifndef SURGE_SRC_SURGE_XT_GUI_MOBILE_PERFORMANCEICONS_H
#define SURGE_SRC_SURGE_XT_GUI_MOBILE_PERFORMANCEICONS_H

#include <juce_gui_basics/juce_gui_basics.h>

#include "PerformanceColours.h"

namespace Surge
{
namespace Mobile
{
namespace Icons
{
// Vector icons drawn with juce::Path, so they look the same everywhere
// (unicode symbols render as emoji on some platforms).

inline juce::Path star(juce::Rectangle<float> r)
{
    juce::Path p;
    auto radius = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f;
    p.addStar(r.getCentre(), 5, radius * 0.45f, radius);
    return p;
}

// A chevron pointing left (prev) or right (next)
inline juce::Path chevron(juce::Rectangle<float> r, bool pointsLeft)
{
    auto s = juce::jmin(r.getWidth(), r.getHeight());
    auto b = r.withSizeKeepingCentre(s * 0.5f, s);
    juce::Path p;
    if (pointsLeft)
    {
        p.startNewSubPath(b.getRight(), b.getY());
        p.lineTo(b.getX(), b.getCentreY());
        p.lineTo(b.getRight(), b.getBottom());
    }
    else
    {
        p.startNewSubPath(b.getX(), b.getY());
        p.lineTo(b.getRight(), b.getCentreY());
        p.lineTo(b.getX(), b.getBottom());
    }
    return p;
}

// A chevron pointing up
inline juce::Path chevronUp(juce::Rectangle<float> r)
{
    auto s = juce::jmin(r.getWidth(), r.getHeight());
    auto b = r.withSizeKeepingCentre(s, s * 0.5f);
    juce::Path p;
    p.startNewSubPath(b.getX(), b.getBottom());
    p.lineTo(b.getCentreX(), b.getY());
    p.lineTo(b.getRight(), b.getBottom());
    return p;
}

// A cog with eight teeth and a hole in the middle
inline juce::Path gear(juce::Rectangle<float> r)
{
    auto c = r.getCentre();
    auto outer = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f;
    auto inner = outer * 0.74f;
    constexpr int teeth = 8;
    juce::Path p;
    for (int i = 0; i < teeth * 4; ++i)
    {
        // Each tooth: rise, top, fall, root
        auto a = juce::MathConstants<float>::twoPi * (i - 0.5f) / (teeth * 4);
        auto rad = (i % 4 == 1 || i % 4 == 2) ? outer : inner;
        auto pt = c.getPointOnCircumference(rad, a);
        if (i == 0)
            p.startNewSubPath(pt);
        else
            p.lineTo(pt);
    }
    p.closeSubPath();
    p.addEllipse(juce::Rectangle<float>(outer * 0.7f, outer * 0.7f).withCentre(c));
    p.setUsingNonZeroWinding(false);
    return p;
}

// A floppy disk outline
inline void drawSave(juce::Graphics &g, juce::Rectangle<float> r)
{
    auto s = juce::jmin(r.getWidth(), r.getHeight());
    auto b = r.withSizeKeepingCentre(s, s);
    auto cut = s * 0.22f;
    juce::Path body;
    body.startNewSubPath(b.getX(), b.getY());
    body.lineTo(b.getRight() - cut, b.getY());
    body.lineTo(b.getRight(), b.getY() + cut);
    body.lineTo(b.getRight(), b.getBottom());
    body.lineTo(b.getX(), b.getBottom());
    body.closeSubPath();
    g.strokePath(body, {2.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded});

    // Shutter at the top and label at the bottom
    g.fillRect(juce::Rectangle<float>(b.getX() + s * 0.24f, b.getY(), s * 0.42f, s * 0.3f));
    g.drawRect(juce::Rectangle<float>(b.getX() + s * 0.18f, b.getY() + s * 0.55f, s * 0.64f,
                                      s * 0.45f),
               2.f);
}

// A small disclosure triangle pointing right (collapsed) or down (expanded)
inline juce::Path disclosure(juce::Rectangle<float> r, bool expanded)
{
    auto s = juce::jmin(r.getWidth(), r.getHeight());
    auto b = r.withSizeKeepingCentre(s, s);
    juce::Path p;
    if (expanded)
        p.addTriangle(b.getX(), b.getY() + s * 0.25f, b.getRight(), b.getY() + s * 0.25f,
                      b.getCentreX(), b.getBottom() - s * 0.15f);
    else
        p.addTriangle(b.getX() + s * 0.25f, b.getY(), b.getX() + s * 0.25f, b.getBottom(),
                      b.getRight() - s * 0.15f, b.getCentreY());
    return p;
}

/*
 * A button matching the perform view's TextButtons, but drawing a vector icon.
 */
class IconButton : public juce::Button
{
  public:
    enum Kind
    {
        Prev,
        Next,
        Up,
        Down,
        Star,
        Random,
        Gear,
        Save,
        Close,
        StarFilled, // always filled, e.g. "go to favorites"
        Expand,     // two diagonal arrows pointing out, e.g. "full UI"
        Clear // a round "x", drawn without a button background (for inside text fields)
    };

    IconButton(Kind k, const juce::String &name) : juce::Button(name), kind(k)
    {
        setTitle(name);
    }

    void paintButton(juce::Graphics &g, bool highlighted, bool down) override
    {
        if (kind == Clear)
        {
            auto s = juce::jmin(22.f, (float)juce::jmin(getWidth(), getHeight()) - 4.f);
            auto c = getLocalBounds().toFloat().withSizeKeepingCentre(s, s);
            g.setColour(down ? Colours::text : Colours::textDim);
            g.fillEllipse(c);
            auto x = c.reduced(s * 0.32f);
            g.setColour(Colours::panel);
            g.drawLine({x.getTopLeft(), x.getBottomRight()}, 2.f);
            g.drawLine({x.getBottomLeft(), x.getTopRight()}, 2.f);
            return;
        }

        auto r = getLocalBounds().toFloat().reduced(1.f);
        auto bg = Colours::panelRaised;
        if (down)
            bg = bg.brighter(0.25f);
        else if (highlighted)
            bg = bg.brighter(0.08f);
        g.setColour(bg);
        g.fillRoundedRectangle(r, 6.f);
        g.setColour(Colours::outline);
        g.drawRoundedRectangle(r, 6.f, 1.f);

        auto iconArea = r.withSizeKeepingCentre(r.getHeight() * 0.5f, r.getHeight() * 0.5f);
        auto fg = isEnabled() ? Colours::text : Colours::textDim;

        switch (kind)
        {
        case Prev:
        case Next:
            g.setColour(fg);
            g.strokePath(chevron(iconArea, kind == Prev),
                         {2.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded});
            break;
        case Up:
        case Down:
        {
            auto p = chevronUp(iconArea);
            if (kind == Down)
                p.applyTransform(juce::AffineTransform::verticalFlip(iconArea.getY() * 2.f +
                                                                     iconArea.getHeight()));
            g.setColour(fg);
            g.strokePath(p, {2.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded});
            break;
        }
        case Random:
        {
            // A die showing five
            auto die = iconArea.expanded(2.f);
            g.setColour(fg);
            g.drawRoundedRectangle(die, die.getWidth() * 0.2f, 2.f);
            auto pip = die.getWidth() * 0.16f;
            auto in = die.reduced(die.getWidth() * 0.27f);
            for (auto pt : {in.getTopLeft(), in.getTopRight(), in.getCentre(), in.getBottomLeft(),
                            in.getBottomRight()})
                g.fillEllipse(juce::Rectangle<float>(pip, pip).withCentre(pt));
            break;
        }
        case Gear:
            g.setColour(fg);
            g.fillPath(gear(iconArea.expanded(2.f)));
            break;
        case Save:
            g.setColour(fg);
            drawSave(g, iconArea);
            break;
        case Close:
        {
            auto x = iconArea.reduced(iconArea.getWidth() * 0.1f);
            g.setColour(fg);
            g.drawLine({x.getTopLeft(), x.getBottomRight()}, 2.5f);
            g.drawLine({x.getBottomLeft(), x.getTopRight()}, 2.5f);
            break;
        }
        case StarFilled:
            g.setColour(fg);
            g.fillPath(star(iconArea.expanded(2.f)));
            break;
        case Expand:
        {
            // Arrows from the middle out to the top-right and bottom-left corners
            auto b = iconArea.expanded(1.f);
            auto at = [&b](float x, float y) {
                return juce::Point<float>(b.getX() + x * b.getWidth(), b.getY() + y * b.getHeight());
            };
            juce::Path p;
            p.startNewSubPath(at(0.58f, 0.42f));
            p.lineTo(at(1.f, 0.f));
            p.startNewSubPath(at(0.6f, 0.f));
            p.lineTo(at(1.f, 0.f));
            p.lineTo(at(1.f, 0.4f));
            p.startNewSubPath(at(0.42f, 0.58f));
            p.lineTo(at(0.f, 1.f));
            p.startNewSubPath(at(0.f, 0.6f));
            p.lineTo(at(0.f, 1.f));
            p.lineTo(at(0.4f, 1.f));
            g.setColour(fg);
            g.strokePath(p, {2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded});
            break;
        }
        case Clear:
            break;
        case Star:
        {
            auto p = star(iconArea.expanded(2.f));
            if (getToggleState())
            {
                g.setColour(Colours::accent);
                g.fillPath(p);
            }
            else
            {
                g.setColour(fg);
                g.strokePath(p, {1.8f, juce::PathStrokeType::mitered});
            }
            break;
        }
        }
    }

  private:
    Kind kind;
};

} // namespace Icons
} // namespace Mobile
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_MOBILE_PERFORMANCEICONS_H
