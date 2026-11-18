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

#ifndef SURGE_SRC_SURGE_XT_GUI_MOBILE_PERFORMANCECOLOURS_H
#define SURGE_SRC_SURGE_XT_GUI_MOBILE_PERFORMANCECOLOURS_H

#include <juce_graphics/juce_graphics.h>

#include "SurgeXTBinary.h"

namespace Surge
{
namespace Mobile
{
namespace Colours
{
// Roughly matches the dark Surge skin
inline const juce::Colour background{0xff16181b};
inline const juce::Colour panel{0xff23262b};
inline const juce::Colour panelRaised{0xff30343a};
inline const juce::Colour outline{0xff3c4148};
inline const juce::Colour accent{0xffff9000}; // Surge orange
inline const juce::Colour accentDim{0xff6b4210};
inline const juce::Colour text{0xffe8e8e8};
inline const juce::Colour textDim{0xff8d939b};
inline const juce::Colour header{0xff5c636c};
} // namespace Colours

/*
 * The perform UI always draws with Surge's embedded Lato. JUCE's default font has to be
 * found among the system's font files, which fails on some Android versions and then
 * crashes. Don't call setBold()/boldened() on these fonts: changing the style drops the
 * embedded typeface and goes back to the system lookup. Ask perfFont for bold instead.
 */
inline juce::Typeface::Ptr perfTypeface(bool bold)
{
    static juce::Typeface::Ptr regular = juce::Typeface::createSystemTypefaceFor(
        SurgeXTBinary::LatoRegular_ttf, SurgeXTBinary::LatoRegular_ttfSize);
    static juce::Typeface::Ptr boldFace = juce::Typeface::createSystemTypefaceFor(
        SurgeXTBinary::LatoBold_ttf, SurgeXTBinary::LatoBold_ttfSize);
    return bold ? boldFace : regular;
}

inline juce::Font perfFont(float height, int style = juce::Font::plain)
{
    return juce::Font(
        juce::FontOptions(perfTypeface((style & juce::Font::bold) != 0)).withHeight(height));
}
} // namespace Mobile
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_MOBILE_PERFORMANCECOLOURS_H
