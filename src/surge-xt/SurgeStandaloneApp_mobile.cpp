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
 */

#include <juce_core/system/juce_TargetPlatform.h>
#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>
#include <juce_audio_plugin_client/detail/juce_IncludeSystemHeaders.h>
#include <juce_audio_plugin_client/detail/juce_IncludeModuleHeaders.h>
#include <juce_gui_basics/native/juce_WindowsHooks_windows.h>
#include <juce_audio_plugin_client/detail/juce_PluginUtilities.h>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include <cstdlib>

#ifdef Component
#undef Component
#endif

#ifdef Point
#undef Point
#endif

#include "SurgeSynthEditor.h"
#include "gui/SurgeGUIEditor.h"

namespace Surge
{
namespace Standalone
{
namespace iOS
{
#if JUCE_ANDROID
namespace
{
constexpr const char *androidSurgeDataAssetPrefix = "assets/SurgeXTData/";
constexpr const char *androidAssetStampFile = "surge-xt-assets-version.txt";

juce::File getAndroidAppFilesDirectory()
{
    if (auto *home = std::getenv("HOME"); home != nullptr && home[0] != 0)
        return juce::File(home);

    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("files");
}

juce::String readZipEntryText(juce::ZipFile &zip, const juce::String &entryName)
{
    if (auto *entry = zip.getEntry(entryName, true); entry != nullptr)
    {
        if (auto stream = std::unique_ptr<juce::InputStream>(zip.createStreamForEntry(*entry)))
            return stream->readEntireStreamAsString().trim();
    }

    return {};
}

void extractBundledSurgeDataIfNeeded()
{
    auto apkFile = juce::File::getSpecialLocation(juce::File::invokedExecutableFile);
    juce::ZipFile apk(apkFile);

    auto dataPath = getAndroidAppFilesDirectory().getChildFile("SurgeXTData");
    auto stampFile = dataPath.getChildFile(androidAssetStampFile);
    auto bundledStamp =
        readZipEntryText(apk, juce::String(androidSurgeDataAssetPrefix) + androidAssetStampFile);
    auto installedStamp = stampFile.existsAsFile() ? stampFile.loadFileAsString().trim()
                                                   : juce::String{};
    auto hasFactoryData = dataPath.getChildFile("patches_factory").isDirectory();

    if (bundledStamp.isNotEmpty())
    {
        if (installedStamp == bundledStamp && hasFactoryData)
            return;
    }
    else if (hasFactoryData)
    {
        return;
    }

    if (dataPath.exists())
        dataPath.deleteRecursively();

    dataPath.createDirectory();

    for (int i = 0; i < apk.getNumEntries(); ++i)
    {
        auto *entry = apk.getEntry(i);
        if (entry == nullptr || !entry->filename.startsWith(androidSurgeDataAssetPrefix))
            continue;

        auto relativePath =
            entry->filename.fromFirstOccurrenceOf(androidSurgeDataAssetPrefix, false, false);
        if (relativePath.isEmpty() || relativePath.startsWithChar('/') ||
            relativePath.contains(".."))
            continue;

        auto targetFile = dataPath.getChildFile(relativePath);
        if (entry->filename.endsWithChar('/'))
        {
            targetFile.createDirectory();
            continue;
        }

        targetFile.getParentDirectory().createDirectory();
        if (auto inStream = std::unique_ptr<juce::InputStream>(apk.createStreamForEntry(*entry)))
        {
            juce::FileOutputStream outStream(targetFile);
            if (outStream.openedOk())
            {
                outStream.writeFromInputStream(*inStream, -1);
                outStream.flush();
            }
        }
    }

    if (bundledStamp.isNotEmpty())
        stampFile.replaceWithText(bundledStamp + "\n");
}
} // namespace
#endif

void disableFeedbackLoopMute(juce::StandalonePluginHolder &holder)
{
    holder.processorHasPotentialFeedbackLoop = false;
    holder.muteInput = false;
    holder.getMuteInputValue().setValue(false);
}

class PluginHolder final : public juce::StandalonePluginHolder
{
  public:
    using juce::StandalonePluginHolder::StandalonePluginHolder;

    void createPlugin() override
    {
        juce::StandalonePluginHolder::createPlugin();
        disableFeedbackLoopMute(*this);
    }
};

#if JUCE_ANDROID
class AndroidAudioDeviceSettingsComponent final : public juce::Component,
                                                  private juce::ChangeListener
{
  public:
    AndroidAudioDeviceSettingsComponent(juce::AudioDeviceManager &deviceManagerToUse,
                                        bool showMidiOutputSelector)
        : deviceManager(deviceManagerToUse),
          midiSelector(deviceManagerToUse, 0, 0, 0, 0, true, showMidiOutputSelector, true, false)
    {
        deviceLabel.setText("Device:", juce::dontSendNotification);
        sampleRateLabel.setText("Sample rate:", juce::dontSendNotification);
        bufferSizeLabel.setText("Audio buffer size:", juce::dontSendNotification);

        addAndMakeVisible(deviceLabel);
        addAndMakeVisible(deviceName);
        addAndMakeVisible(sampleRateLabel);
        addAndMakeVisible(sampleRateDropDown);
        addAndMakeVisible(bufferSizeLabel);
        addAndMakeVisible(bufferSizeDropDown);
        addAndMakeVisible(midiSelector);

        sampleRateDropDown.onChange = [this]() { applySampleRate(); };
        bufferSizeDropDown.onChange = [this]() { applyBufferSize(); };

        deviceManager.addChangeListener(this);
        refresh();
    }

    ~AndroidAudioDeviceSettingsComponent() override { deviceManager.removeChangeListener(this); }

    int getItemHeight() const { return itemHeight; }

    int getRecommendedHeight() const
    {
        return topPadding + (itemHeight + rowGap) * 3 + sectionGap + midiSelector.getHeight() +
               bottomPadding;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(horizontalPadding, topPadding);

        layoutRow(r, deviceLabel, deviceName);
        r.removeFromTop(rowGap);

        layoutRow(r, sampleRateLabel, sampleRateDropDown);
        r.removeFromTop(rowGap);

        layoutRow(r, bufferSizeLabel, bufferSizeDropDown);
        r.removeFromTop(sectionGap);

        midiSelector.setBounds(r.withHeight(midiSelector.getHeight()));
    }

  private:
    static constexpr int itemHeight = 24;
    static constexpr int horizontalPadding = 8;
    static constexpr int topPadding = 8;
    static constexpr int bottomPadding = 8;
    static constexpr int labelWidth = 130;
    static constexpr int rowGap = 8;
    static constexpr int sectionGap = 14;

    juce::AudioDeviceManager &deviceManager;
    juce::Label deviceLabel;
    juce::Label deviceName;
    juce::Label sampleRateLabel;
    juce::ComboBox sampleRateDropDown;
    juce::Label bufferSizeLabel;
    juce::ComboBox bufferSizeDropDown;
    juce::AudioDeviceSelectorComponent midiSelector;
    juce::ScopedMessageBox messageBox;
    bool isRefreshing = false;

    void layoutRow(juce::Rectangle<int> &r, juce::Component &label, juce::Component &control)
    {
        auto row = r.removeFromTop(itemHeight);
        label.setBounds(row.removeFromLeft(labelWidth));
        row.removeFromLeft(8);
        control.setBounds(row);
    }

    void changeListenerCallback(juce::ChangeBroadcaster *) override { refresh(); }

    void refresh()
    {
        const juce::ScopedValueSetter<bool> scope(isRefreshing, true);

        auto *device = deviceManager.getCurrentAudioDevice();
        const auto hasDevice = device != nullptr;

        deviceName.setText(hasDevice ? device->getTypeName() + " - " + device->getName()
                                     : juce::String("No audio device"),
                           juce::dontSendNotification);

        sampleRateDropDown.setEnabled(hasDevice);
        bufferSizeDropDown.setEnabled(hasDevice);
        sampleRateDropDown.clear(juce::dontSendNotification);
        bufferSizeDropDown.clear(juce::dontSendNotification);

        if (device == nullptr)
        {
            setSize(getWidth(), getRecommendedHeight());
            return;
        }

        auto currentRate = device->getCurrentSampleRate();
        if (juce::approximatelyEqual(currentRate, 0.0))
            currentRate = 48000.0;

        auto rates = device->getAvailableSampleRates();
        rates.sort();

        for (auto rate : rates)
        {
            const auto intRate = juce::roundToInt(rate);
            if (intRate > 0)
                sampleRateDropDown.addItem(getSampleRateText(intRate), intRate);
        }

        const auto intCurrentRate = juce::roundToInt(currentRate);
        if (sampleRateDropDown.indexOfItemId(intCurrentRate) >= 0)
            sampleRateDropDown.setSelectedId(intCurrentRate, juce::dontSendNotification);
        else
            sampleRateDropDown.setText(getSampleRateText(intCurrentRate), juce::dontSendNotification);

        auto bufferSizes = getConservativeAndroidBufferSizes(*device, currentRate);
        const auto currentBufferSize = device->getCurrentBufferSizeSamples();

        if (currentBufferSize > 0)
            bufferSizes.addIfNotAlreadyThere(currentBufferSize);

        bufferSizes.sort();

        for (auto bufferSize : bufferSizes)
            bufferSizeDropDown.addItem(getBufferSizeText(bufferSize, currentRate), bufferSize);

        if (bufferSizeDropDown.indexOfItemId(currentBufferSize) >= 0)
            bufferSizeDropDown.setSelectedId(currentBufferSize, juce::dontSendNotification);
        else if (currentBufferSize > 0)
            bufferSizeDropDown.setText(getBufferSizeText(currentBufferSize, currentRate),
                                       juce::dontSendNotification);

        setSize(getWidth(), getRecommendedHeight());
    }

    static juce::Array<int> getConservativeAndroidBufferSizes(juce::AudioIODevice &device,
                                                              double currentRate)
    {
        auto available = device.getAvailableBufferSizes();
        available.sort();

        juce::Array<int> result;
        static constexpr double targetDurationsMs[] = {8.0, 16.0, 24.0, 32.0, 40.0, 80.0};

        for (auto targetDurationMs : targetDurationsMs)
        {
            const auto targetSamples =
                juce::roundToInt(targetDurationMs * currentRate / 1000.0);

            int chosenSize = 0;
            for (auto availableSize : available)
            {
                if (availableSize >= targetSamples)
                {
                    chosenSize = availableSize;
                    break;
                }
            }

            if (chosenSize == 0 && !available.isEmpty())
                chosenSize = available.getLast();

            if (chosenSize > 0)
                result.addIfNotAlreadyThere(chosenSize);
        }

        return result;
    }

    static juce::String getSampleRateText(int rate) { return juce::String(rate) + " Hz"; }

    static juce::String getBufferSizeText(int bufferSize, double currentRate)
    {
        return juce::String(bufferSize) + " samples (" +
               juce::String(bufferSize * 1000.0 / currentRate, 1) + " ms)";
    }

    void applySampleRate()
    {
        if (isRefreshing || sampleRateDropDown.getSelectedId() <= 0)
            return;

        auto config = deviceManager.getAudioDeviceSetup();
        config.sampleRate = sampleRateDropDown.getSelectedId();
        applyConfig(config);
    }

    void applyBufferSize()
    {
        if (isRefreshing || bufferSizeDropDown.getSelectedId() <= 0)
            return;

        auto config = deviceManager.getAudioDeviceSetup();
        config.bufferSize = bufferSizeDropDown.getSelectedId();
        applyConfig(config);
    }

    void applyConfig(const juce::AudioDeviceManager::AudioDeviceSetup &config)
    {
        auto error = deviceManager.setAudioDeviceSetup(config, true);

        if (error.isNotEmpty())
        {
            messageBox = juce::AlertWindow::showScopedAsync(
                juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::WarningIcon)
                    .withTitle("Error when trying to open audio device!")
                    .withMessage(error)
                    .withButton("OK"),
                nullptr);
        }

        refresh();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AndroidAudioDeviceSettingsComponent)
};
#endif

class AudioSettingsComponent final : public juce::Component
{
  public:
    AudioSettingsComponent(juce::StandalonePluginHolder &pluginHolder,
                           juce::AudioDeviceManager &deviceManagerToUse,
                           int maxAudioInputChannels, int maxAudioOutputChannels)
        : owner(pluginHolder),
#if JUCE_ANDROID
          deviceSelector(deviceManagerToUse, pluginHolder.processor.get() != nullptr &&
                                                 pluginHolder.processor->producesMidi()),
#else
          deviceSelector(deviceManagerToUse, 0, maxAudioInputChannels, 0, maxAudioOutputChannels,
                         true,
                         (pluginHolder.processor.get() != nullptr &&
                          pluginHolder.processor->producesMidi()),
                         true, false),
#endif
          shouldMuteLabel("Feedback Loop:", "Feedback Loop:"),
          shouldMuteButton("Mute audio input"), closeButton("Close Settings")
    {
        setOpaque(true);

        shouldMuteButton.setClickingTogglesState(true);
        shouldMuteButton.getToggleStateValue().referTo(owner.shouldMuteInput);

        addAndMakeVisible(deviceSelector);
        addAndMakeVisible(closeButton);

        closeButton.onClick = [this]() {
            if (auto *w = findParentComponentOfClass<juce::DialogWindow>())
                w->exitModalState(0);
        };

        if (owner.getProcessorHasPotentialFeedbackLoop())
        {
            addAndMakeVisible(shouldMuteButton);
            addAndMakeVisible(shouldMuteLabel);
            shouldMuteLabel.attachToComponent(&shouldMuteButton, true);
        }
    }

    void paint(juce::Graphics &g) override
    {
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        const juce::ScopedValueSetter<bool> scope(isResizing, true);

        auto r = getLocalBounds();
        auto bottomArea = r.removeFromBottom(closeAreaHeight);
        closeButton.setBounds(bottomArea.reduced(20, 10));

        if (owner.getProcessorHasPotentialFeedbackLoop())
        {
            auto itemHeight = deviceSelector.getItemHeight();
            auto extra = r.removeFromTop(itemHeight);
            auto separatorHeight = (itemHeight >> 1);

            shouldMuteButton.setBounds(juce::Rectangle<int>(
                extra.proportionOfWidth(0.35f), separatorHeight, extra.proportionOfWidth(0.60f),
                deviceSelector.getItemHeight()));

            r.removeFromTop(separatorHeight);
        }

        deviceSelector.setBounds(r);
    }

    void childBoundsChanged(juce::Component *childComp) override
    {
        if (!isResizing && childComp == &deviceSelector)
            setToRecommendedSize();
    }

    void setToRecommendedSize()
    {
        const auto extraHeight = [this]() {
            if (!owner.getProcessorHasPotentialFeedbackLoop())
                return 0;

            const auto itemHeight = deviceSelector.getItemHeight();
            const auto separatorHeight = (itemHeight >> 1);
            return itemHeight + separatorHeight;
        }();

        setSize(getWidth(), deviceSelector.getHeight() + extraHeight + closeAreaHeight);
    }

  private:
    static constexpr int closeAreaHeight = 50;

    juce::StandalonePluginHolder &owner;
#if JUCE_ANDROID
    AndroidAudioDeviceSettingsComponent deviceSelector;
#else
    juce::AudioDeviceSelectorComponent deviceSelector;
#endif
    juce::Label shouldMuteLabel;
    juce::ToggleButton shouldMuteButton;
    juce::TextButton closeButton;
    bool isResizing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsComponent)
};

std::unique_ptr<juce::StandalonePluginHolder>
prepareStandalonePluginHolder(std::unique_ptr<juce::StandalonePluginHolder> holder)
{
    if (holder != nullptr)
        disableFeedbackLoopMute(*holder);

    return holder;
}

class StandaloneWindow final : public juce::StandaloneFilterWindow
{
  public:
    StandaloneWindow(const juce::String &title, juce::Colour backgroundColour,
                     std::unique_ptr<juce::StandalonePluginHolder> pluginHolderIn)
        : juce::StandaloneFilterWindow(title, backgroundColour,
                                       prepareStandalonePluginHolder(std::move(pluginHolderIn))),
          optionsButton("Options"),
          zoomInButton("zoom+"),
          zoomOutButton("zoom-"),
          zoomFitButton("fit"),
          helpButton("?")
    {
        juce::Component::addAndMakeVisible(&optionsButton);
        optionsButton.onClick = [this]() { showAudioSettingsDialog(); };
        optionsButton.setTriggeredOnMouseDown(true);
        optionsButton.setAlwaysOnTop(true);

        juce::Component::addAndMakeVisible(&zoomOutButton);
        zoomOutButton.onClick = [this]() { changeZoom(-10.0f); };
        zoomOutButton.setTriggeredOnMouseDown(true);
        zoomOutButton.setAlwaysOnTop(true);

        juce::Component::addAndMakeVisible(&zoomInButton);
        zoomInButton.onClick = [this]() { changeZoom(10.0f); };
        zoomInButton.setTriggeredOnMouseDown(true);
        zoomInButton.setAlwaysOnTop(true);

        juce::Component::addAndMakeVisible(&zoomFitButton);
        zoomFitButton.onClick = [this]() { fitZoom(); };
        zoomFitButton.setTriggeredOnMouseDown(true);
        zoomFitButton.setAlwaysOnTop(true);

        juce::Component::addAndMakeVisible(&helpButton);
        helpButton.onClick = [this]() { showHelpDialog(); };
        helpButton.setTriggeredOnMouseDown(true);
        helpButton.setAlwaysOnTop(true);

        setupiPhoneScrollIfNeeded();
    }

    ~StandaloneWindow() override
    {
        saveScrollPosition();
    }

    void saveScrollPosition()
    {
        if (scrollViewport && pluginHolder && pluginHolder->processor)
        {
            if (auto *ed = dynamic_cast<SurgeSynthEditor *>(pluginHolder->processor->getActiveEditor()))
            {
                if (auto *sge = ed->getSurgeGUIEditor())
                {
                    auto pos = scrollViewport->getViewPosition();
                    Surge::Storage::updateUserDefaultValue(&(sge->synth->storage),
                                                           Surge::Storage::MobileScrollPosition,
                                                           std::make_pair(pos.x, pos.y));
                }
            }
        }
    }

    void saveZoom(float zf)
    {
        if (pluginHolder && pluginHolder->processor)
        {
            if (auto *ed = dynamic_cast<SurgeSynthEditor *>(pluginHolder->processor->getActiveEditor()))
            {
                if (auto *sge = ed->getSurgeGUIEditor())
                {
                    Surge::Storage::updateUserDefaultValue(&(sge->synth->storage),
                                                           Surge::Storage::DefaultZoom,
                                                           (int)std::round(zf));
                }
            }
        }
    }

    void changeZoom(float delta)
    {
        if (pluginHolder && pluginHolder->processor)
        {
            if (auto* ed = dynamic_cast<SurgeSynthEditor*>(pluginHolder->processor->getActiveEditor()))
            {
                if (auto* sge = ed->getSurgeGUIEditor())
                {
                    float currentZoom = sge->getZoomFactor();
                    float newZoom = currentZoom + delta;
                    sge->resizeWindow(newZoom);
                    saveZoom(newZoom);
                    saveScrollPosition();
                }
            }
        }
    }

    // Calculates the zoom % that makes the synth's UI exactly fill the screen width,
    // using sge->getWindowSizeX() (skin-aware) rather than the hardcoded 913px constant.
    // Falls back to 100% if the editor is not yet available.
    float calcFitZoom() const
    {
        auto &displays = juce::Desktop::getInstance().getDisplays();
        auto *primary = displays.getPrimaryDisplay();
        if (primary == nullptr)
            return 100.f;

        auto userArea = primary->userArea;
        int screenW = juce::jmax(userArea.getWidth(), userArea.getHeight());

        if (pluginHolder && pluginHolder->processor)
        {
            if (auto *ed = dynamic_cast<SurgeSynthEditor *>(pluginHolder->processor->getActiveEditor()))
            {
                if (auto *sge = ed->getSurgeGUIEditor())
                {
                    float skinW = static_cast<float>(sge->getWindowSizeX());
                    if (skinW > 0)
                        return std::round(screenW / skinW * 100.f);
                }
            }
        }

        // Editor not ready yet — fall back to the default skin width.
        constexpr int surgeNativeW = 913;
        return std::round(static_cast<float>(screenW) / surgeNativeW * 100.f);
    }

    void fitZoom()
    {
        if (pluginHolder && pluginHolder->processor)
        {
            if (auto *ed = dynamic_cast<SurgeSynthEditor *>(pluginHolder->processor->getActiveEditor()))
            {
                if (auto *sge = ed->getSurgeGUIEditor())
                {
                    float fit = calcFitZoom();
                    sge->resizeWindow(fit);
                    saveZoom(fit);
                    saveScrollPosition();
                }
            }
        }
    }

    void resized() override
    {
        if (scrollViewport != nullptr)
        {
            // Don't call parent resized() — it would resize content to fill the window,
            // collapsing it and breaking the scroll. Instead, just fit the viewport.
            scrollViewport->setBounds(getLocalBounds());
        }
        else
        {
            juce::StandaloneFilterWindow::resized();
        }

        // Layout: [Options 60] [zoom- 45] [zoom+ 45] [fit 28] [? 24]
        int x = 20;
        optionsButton.setBounds(x, 12, 60, 22);  x += 60 + 10;
        zoomOutButton.setBounds(x, 12, 45, 22);  x += 45 + 5;
        zoomInButton.setBounds(x, 12, 45, 22);   x += 45 + 5;
        zoomFitButton.setBounds(x, 12, 28, 22);  x += 28 + 5;
        helpButton.setBounds(x, 12, 24, 22);

        optionsButton.toFront(false);
        zoomOutButton.toFront(false);
        zoomInButton.toFront(false);
        zoomFitButton.toFront(false);
        helpButton.toFront(false);
    }

  private:
    // Wraps the synth content with empty padding so the user can
    // scroll slightly past every edge, making extremity controls easier to tap.
    // On iPad, we add only vertical padding (top + bottom) to keep the Options
    // and Zoom buttons from overlapping the main UI.
    struct PaddingWrapper : public juce::Component
    {
        PaddingWrapper(juce::Component *contentToOwn, int hPad, int vPad)
            : child(contentToOwn), horizontalPad(hPad), verticalPad(vPad)
        {
            addAndMakeVisible(*child);
            child->setTopLeftPosition(horizontalPad, verticalPad);
            setSize(child->getWidth() + 2 * horizontalPad, child->getHeight() + 2 * verticalPad);
        }

        // If Surge internally changes its editor size (e.g. user changes zoom),
        // keep the child positioned and resize this wrapper to match.
        void childBoundsChanged(juce::Component *) override
        {
            child->setTopLeftPosition(horizontalPad, verticalPad);
            setSize(child->getWidth() + 2 * horizontalPad, child->getHeight() + 2 * verticalPad);
        }

        std::unique_ptr<juce::Component> child;
        int horizontalPad;
        int verticalPad;
    };

    std::unique_ptr<PaddingWrapper> paddingWrapper;

    struct TwoFingerScrollListener : public juce::MouseListener
    {
        StandaloneWindow *owner{nullptr};
        juce::Viewport *viewport{nullptr};
        std::map<int, juce::Point<float>> activeTouches;
        juce::Point<float> lastCentroid;
        bool isTwoFingerScrolling{false};

        TwoFingerScrollListener(StandaloneWindow *w, juce::Viewport *v)
            : owner(w), viewport(v) {}

        juce::Point<float> computeCentroid() const
        {
            if (activeTouches.empty())
                return {};
            float x = 0.f;
            float y = 0.f;
            for (const auto &[idx, pt] : activeTouches)
            {
                x += pt.x;
                y += pt.y;
            }
            return {x / static_cast<float>(activeTouches.size()),
                    y / static_cast<float>(activeTouches.size())};
        }

        bool wasMultiTouchGesture{false};

        void mouseDown(const juce::MouseEvent &e) override
        {
            auto pos = e.getScreenPosition().toFloat();
            activeTouches[e.source.getIndex()] = pos;

            if (activeTouches.size() >= 2)
            {
                wasMultiTouchGesture = true;
                Surge::GUI::setIsMultiTouchScrolling(true);
                juce::PopupMenu::dismissAllActiveMenus();
                lastCentroid = computeCentroid();
                isTwoFingerScrolling = true;
            }
        }

        void mouseDrag(const juce::MouseEvent &e) override
        {
            auto pos = e.getScreenPosition().toFloat();
            activeTouches[e.source.getIndex()] = pos;

            if (activeTouches.size() >= 2 && viewport != nullptr)
            {
                wasMultiTouchGesture = true;
                Surge::GUI::setIsMultiTouchScrolling(true);
                juce::PopupMenu::dismissAllActiveMenus();
                auto currentCentroid = computeCentroid();
                if (isTwoFingerScrolling)
                {
                    auto delta = currentCentroid - lastCentroid;
                    auto viewPos = viewport->getViewPosition();
                    viewport->setViewPosition(
                        viewPos.x - juce::roundToInt(delta.x),
                        viewPos.y - juce::roundToInt(delta.y));
                }
                lastCentroid = currentCentroid;
                isTwoFingerScrolling = true;
            }
        }

        void mouseUp(const juce::MouseEvent &e) override
        {
            activeTouches.erase(e.source.getIndex());

            if (activeTouches.size() >= 2)
            {
                lastCentroid = computeCentroid();
            }
            else if (activeTouches.empty())
            {
                if (isTwoFingerScrolling && owner)
                    owner->saveScrollPosition();
                isTwoFingerScrolling = false;
                wasMultiTouchGesture = false;
                Surge::GUI::setIsMultiTouchScrolling(false);
            }
        }
    };

    std::unique_ptr<juce::Viewport> scrollViewport;
    std::unique_ptr<TwoFingerScrollListener> smartDragListener;

    void restoreSavedZoomAndPosition(bool isIPhone, int hPad, int vPad)
    {
        if (pluginHolder && pluginHolder->processor)
        {
            if (auto *ed = dynamic_cast<SurgeSynthEditor *>(pluginHolder->processor->getActiveEditor()))
            {
                if (auto *sge = ed->getSurgeGUIEditor())
                {
                    auto *storage = &(sge->synth->storage);
                    int savedZoom = Surge::Storage::getUserDefaultValue(
                        storage, Surge::Storage::DefaultZoom, 0);

                    if (savedZoom > 0)
                    {
                        sge->resizeWindow(static_cast<float>(savedZoom));
                    }
                    else
                    {
                        if (isIPhone)
                        {
                            sge->resizeWindow(125.0f);
                        }
                        else
                        {
                            fitZoom();
                        }
                    }

                    int sentinel = -1000004;
                    auto savedPos = Surge::Storage::getUserDefaultValue(
                        storage, Surge::Storage::MobileScrollPosition,
                        std::make_pair(sentinel, sentinel));

                    if (savedPos.first != sentinel && savedPos.second != sentinel)
                    {
                        if (scrollViewport)
                            scrollViewport->setViewPosition(savedPos.first, savedPos.second);
                    }
                    else
                    {
                        if (scrollViewport)
                            scrollViewport->setViewPosition(hPad, vPad);
                    }
                }
            }
        }
    }

    void setupiPhoneScrollIfNeeded()
    {
        // Detect iPhone vs iPad: iPad short edge >= 768pt.
        auto &displays = juce::Desktop::getInstance().getDisplays();
        auto *primary = displays.getPrimaryDisplay();
        if (primary == nullptr)
            return;

        auto userArea = primary->userArea;
        int shortEdge = juce::jmin(userArea.getWidth(), userArea.getHeight());
        bool isIPhone = (shortEdge < 768);
        // Surge's native canvas size (from globals.h BASE_WINDOW_SIZE_X/Y).
        constexpr int surgeNativeW = 913;
        constexpr int surgeNativeH = 569;

        int screenW = juce::jmax(userArea.getWidth(), userArea.getHeight());
        int screenH = shortEdge;

        juce::Component *content = getContentComponent();
        if (content == nullptr)
            return;

        int hPad, vPad;

        if (isIPhone)
        {
            // Initial content size for iPhone
            constexpr float initialPhoneZoom = 1.25f;
            constexpr int edgePadding = 50; // px of empty space on each side

            int contentW = juce::roundToInt(surgeNativeW * initialPhoneZoom);
            int contentH = juce::roundToInt(surgeNativeH * initialPhoneZoom);

            content->setSize(contentW, contentH);

            hPad = edgePadding;
            vPad = edgePadding;
        }
        else
        {
            content->setSize(surgeNativeW, surgeNativeH);
            constexpr int iPadVerticalMargin = 40;
            hPad = 0;
            vPad = iPadVerticalMargin;
        }

        // Detach content from the window without deleting it.
        JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE("-Wdeprecated-declarations")
        setContentComponent(nullptr, /*deleteOldOne=*/false, /*resizeToFit=*/false);
        JUCE_END_IGNORE_WARNINGS_GCC_LIKE

        // Wrap the content in a padded container.
        paddingWrapper = std::make_unique<PaddingWrapper>(content, hPad, vPad);

        // Build the viewport.
        scrollViewport = std::make_unique<juce::Viewport>();
        scrollViewport->setViewedComponent(paddingWrapper.get(), /*deleteOnDetach=*/false);
        scrollViewport->setScrollBarsShown(/*vertical=*/true, /*horizontal=*/true);
        // Turn off native drag-to-scroll. We will handle dragging manually with our listener
        // so we can seamlessly ignore drags that start on sliders.
        scrollViewport->setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
        scrollViewport->setSize(screenW, screenH);
        // Scroll the viewport so the top-left of the content (minus padding) is visible.
        scrollViewport->setViewPosition(hPad, vPad);

        // Attach our custom two-finger drag-to-scroll listener
        smartDragListener = std::make_unique<TwoFingerScrollListener>(this, scrollViewport.get());
        paddingWrapper->addMouseListener(smartDragListener.get(), true);

        setContentNonOwned(scrollViewport.get(), false);
        scrollViewport->setBounds(getLocalBounds());

        // Restore saved zoom and scroll position asynchronously once the editor is ready
        juce::MessageManager::callAsync([this, isIPhone, hPad, vPad]() {
            restoreSavedZoomAndPosition(isIPhone, hPad, vPad);
        });
    }

    void showAudioSettingsDialog()
    {
        juce::DialogWindow::LaunchOptions options;

        int maxNumInputs = 0;
        int maxNumOutputs = 0;

        if (pluginHolder->channelConfiguration.size() > 0)
        {
            auto &defaultConfig = pluginHolder->channelConfiguration.getReference(0);
            maxNumInputs = juce::jmax(0, static_cast<int>(defaultConfig.numIns));
            maxNumOutputs = juce::jmax(0, static_cast<int>(defaultConfig.numOuts));
        }

        if (auto *bus = pluginHolder->processor->getBus(true, 0))
            maxNumInputs = juce::jmax(0, bus->getDefaultLayout().size());

        if (auto *bus = pluginHolder->processor->getBus(false, 0))
            maxNumOutputs = juce::jmax(0, bus->getDefaultLayout().size());

        auto content = std::make_unique<AudioSettingsComponent>(
            *pluginHolder, pluginHolder->deviceManager, maxNumInputs, maxNumOutputs);
        content->setSize(500, 550);
        content->setToRecommendedSize();

        auto *contentPtr = content.get();
        options.content.setOwned(content.release());
        options.dialogTitle = juce::translate("Audio/MIDI Settings");
        options.dialogBackgroundColour =
            contentPtr->getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;

        options.launchAsync();
    }

    void showHelpDialog()
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::NoIcon,
            "Surge XT Mobile Gestures",
            juce::String::fromUTF8(
                "- Two-Finger Scroll / Pan:\n"
                "Swipe anywhere on the screen with two fingers to scroll and pan the synthesizer interface.\n\n"
                "- Right-Click / Context Menu:\n"
                "Long-press (hold still for ~0.75s) with a single finger on any slider, knob, or button to open its context menu.\n\n"
                "- Single-Finger Controls:\n"
                "Tap or drag controls directly with a single finger.\n\n"
                "- Zoom Controls:\n"
                "Use [+] and [-] to zoom in/out, or [fit] to fit the synthesizer interface to your screen width."),
            "OK");
    }

    juce::TextButton optionsButton;
    juce::TextButton zoomInButton;
    juce::TextButton zoomOutButton;
    juce::TextButton zoomFitButton;
    juce::TextButton helpButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StandaloneWindow)
};

class StandaloneApp final : public juce::JUCEApplication
{
  public:
    StandaloneApp()
    {
        juce::PropertiesFile::Options options;

        options.applicationName = juce::CharPointer_UTF8(JucePlugin_Name);
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        options.folderName = "";

        appProperties.setStorageParameters(options);
    }

    const juce::String getApplicationName() override
    {
        return juce::CharPointer_UTF8(JucePlugin_Name);
    }

    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted(const juce::String &) override {}

    StandaloneWindow *createWindow()
    {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            jassertfalse;
            return nullptr;
        }

        return new StandaloneWindow(
            getApplicationName(),
            juce::LookAndFeel::getDefaultLookAndFeel().findColour(
                juce::ResizableWindow::backgroundColourId),
            createPluginHolder());
    }

    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
        constexpr auto autoOpenMidiDevices =
#if (JUCE_ANDROID || JUCE_IOS) && !JUCE_DONT_AUTO_OPEN_MIDI_DEVICES_ON_MOBILE
            true;
#else
            false;
#endif

#if JUCE_ANDROID
        // Surge XT is a synth on mobile, so avoid requesting microphone permission.
        constexpr juce::StandalonePluginHolder::PluginInOuts channels[]{{0, 2}};
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig(
            channels, juce::numElementsInArray(channels));
#elif defined(JucePlugin_PreferredChannelConfigurations)
        constexpr juce::StandalonePluginHolder::PluginInOuts channels[]{
            JucePlugin_PreferredChannelConfigurations};
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig(
            channels, juce::numElementsInArray(channels));
#else
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig;
#endif

        return std::make_unique<PluginHolder>(
            appProperties.getUserSettings(), false, juce::String{}, nullptr, channelConfig,
            autoOpenMidiDevices);
    }

    void initialise(const juce::String &) override
    {
#if JUCE_ANDROID
        extractBundledSurgeDataIfNeeded();
#endif

        // Lock to landscape on all iOS devices (iPhone and iPad).
        juce::Desktop::getInstance().setOrientationsEnabled(juce::Desktop::rotatedClockwise |
                                                            juce::Desktop::rotatedAntiClockwise);

        // Prevent the screen from sleeping entirely while the app is open
        juce::Desktop::getInstance().setScreenSaverEnabled(false);

        mainWindow.reset(createWindow());

        if (mainWindow != nullptr)
        {
#if JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE
            juce::Desktop::getInstance().setKioskModeComponent(mainWindow.get(), false);
#endif

            mainWindow->setVisible(true);
        }
        else
        {
            pluginHolder = prepareStandalonePluginHolder(createPluginHolder());
        }
    }

    void shutdown() override
    {
        pluginHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (pluginHolder != nullptr)
            pluginHolder->savePluginState();

        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay(100, []() {
                if (auto *app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

  private:
    juce::ApplicationProperties appProperties;
    std::unique_ptr<StandaloneWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};
} // namespace iOS
} // namespace Standalone
} // namespace Surge

//==============================================================================
// Native JNI implementations to satisfy JuceActivity.java without patching JUCE
#if JUCE_ANDROID
#include <jni.h>
extern "C" {
    JNIEXPORT void JNICALL Java_com_rmsl_juce_JuceActivity_appNewIntent(JNIEnv*, jobject, jobject) {
        // Surge does not use push notifications
    }
    
    JNIEXPORT void JNICALL Java_com_rmsl_juce_JuceActivity_appOnResume(JNIEnv*, jobject) {
        // Surge does not use in-app purchases
    }
}
#endif

#if JUCE_ANDROID
extern "C" __attribute__ ((visibility ("default"))) juce::JUCEApplicationBase* juce_CreateApplication()
{
    return new Surge::Standalone::iOS::StandaloneApp();
}
#else
START_JUCE_APPLICATION(Surge::Standalone::iOS::StandaloneApp)
#endif

#if JucePlugin_Build_Standalone && (JUCE_IOS || JUCE_ANDROID)

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE("-Wmissing-prototypes")

bool JUCE_CALLTYPE juce_isInterAppAudioConnected()
{
    if (auto *holder = juce::StandalonePluginHolder::getInstance())
        return holder->isInterAppAudioConnected();

    return false;
}

void JUCE_CALLTYPE juce_switchToHostApplication()
{
    if (auto *holder = juce::StandalonePluginHolder::getInstance())
        holder->switchToHostApplication();
}

juce::Image JUCE_CALLTYPE juce_getIAAHostIcon(int size)
{
    if (auto *holder = juce::StandalonePluginHolder::getInstance())
        return holder->getIAAHostIcon(size);

    return {};
}

JUCE_END_IGNORE_WARNINGS_GCC_LIKE

#endif
