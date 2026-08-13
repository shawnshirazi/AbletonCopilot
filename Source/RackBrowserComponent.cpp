#include "RackBrowserComponent.h"
#include <algorithm>
#include <map>

namespace
{
    struct RackDef { const char* id; const char* display; };

    static const RackDef kRackDefs[] = {
        { "KICK",   "Kicks" },
        { "SNARE",  "Snares" },
        { "CLAP",   "Claps" },
        { "HIHAT",  "Hi-Hats" },
        { "CYMBAL", "Cymbals" },
        { "PERC",   "Percussion" },
        { "BASS",   "Bass" },
        { "LOOP",   "Loops" },
        { "FX",     "FX" },
        { "VOCAL",  "Vocals" },
        { "MISC",   "Other" },
    };

    // Loops run several seconds or more; real one-shots are brief. Duration
    // wins over filename every time — a "Kick_Loop.wav" that's actually 4
    // bars long cannot be a kick one-shot no matter what it's called, and
    // triggering it as one on every kick step is exactly what sounded "off".
    constexpr double kLoopDurationThresholdSecs = 2.0;

    juce::StringArray tokenize(const juce::String& stem)
    {
        auto normalised = stem.toLowerCase()
                               .replaceCharacter('_', ' ')
                               .replaceCharacter('-', ' ')
                               .replaceCharacter('.', ' ');
        juce::StringArray tokens;
        tokens.addTokens(normalised, " ", "");
        tokens.removeEmptyStrings();
        return tokens;
    }

    // Classify by *whole-word* filename token, not substring-anywhere — the
    // old "contains" check matched "hh" inside "Ahh_Vocal.wav" and "hat"
    // inside "Whatever.wav", which is how a vocal chop ended up in Hi-Hats.
    // Anything that doesn't hit an exact token falls through to MISC rather
    // than risk a false positive.
    juce::String classifyByTokens(const juce::StringArray& tokens)
    {
        auto has = [&](std::initializer_list<const char*> kws)
        {
            for (auto* kw : kws)
                if (tokens.contains(kw))
                    return true;
            return false;
        };

        if (has({ "loop", "loops", "break", "breaks" }))                          return "LOOP";
        if (has({ "kick", "kck" }))                                               return "KICK";
        if (has({ "snare", "snr" }))                                              return "SNARE";
        if (has({ "clap", "clp" }))                                               return "CLAP";
        if (has({ "hihat", "hihats", "hat", "hats", "hh", "chh", "ohh" }))        return "HIHAT";
        if (has({ "cymbal", "crash", "ride" }))                                   return "CYMBAL";
        if (has({ "perc", "percussion", "shaker", "tom", "conga", "bongo" }))     return "PERC";
        if (has({ "bass", "sub", "808" }))                                        return "BASS";
        if (has({ "fx", "riser", "impact", "sweep", "noise" }))                   return "FX";
        if (has({ "vocal", "vocals", "vox", "adlib", "acapella" }))               return "VOCAL";
        return "MISC";
    }

    // Duration (when readable) overrides the name — see kLoopDurationThresholdSecs.
    juce::String classifySample(const juce::File& file, juce::AudioFormatManager& formatManager)
    {
        if (auto reader = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(file)))
        {
            if (reader->sampleRate > 0.0
                && (double) reader->lengthInSamples / reader->sampleRate >= kLoopDurationThresholdSecs)
                return "LOOP";
        }

        return classifyByTokens(tokenize(file.getFileNameWithoutExtension()));
    }
}

//==============================================================================
void RackListComponent::setRacks(const std::vector<Rack>& newRacks)
{
    racks = newRacks;
    selectedIndex = racks.empty() ? -1 : 0;
    repaint();
}

void RackListComponent::setSelected(int index)
{
    selectedIndex = index;
    repaint();
}

void RackListComponent::paint(juce::Graphics& g)
{
    static const juce::Colour kBg      { 0xff181818 };
    static const juce::Colour kRowSel  { 0xff2a2a2a };
    static const juce::Colour kAccent  { 0xfffc8c00 };
    static const juce::Colour kText    { 0xffdddddd };
    static const juce::Colour kTextDim { 0xff777777 };

    g.fillAll(kBg);

    if (racks.empty())
    {
        g.setColour(kTextDim);
        g.setFont(juce::FontOptions(11.0f));
        g.drawFittedText("No racks yet \xe2\x80\x94 choose a sample library.",
                          getLocalBounds().reduced(8), juce::Justification::centredLeft, 4);
        return;
    }

    const int w = getWidth();
    for (int i = 0; i < (int) racks.size(); ++i)
    {
        auto r = juce::Rectangle<int>(0, i * kRowHeight, w, kRowHeight);
        const bool sel = (i == selectedIndex);

        if (sel)
        {
            g.setColour(kRowSel);
            g.fillRect(r);
            g.setColour(kAccent);
            g.fillRect(r.removeFromLeft(3));
        }

        auto inner = r.reduced(10, 0);
        g.setColour(sel ? kAccent : kText);
        g.setFont(juce::FontOptions(12.0f).withStyle(sel ? "Bold" : "Regular"));
        g.drawText(racks[(size_t) i].display, inner, juce::Justification::centredLeft);

        g.setColour(kTextDim);
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(juce::String((int) racks[(size_t) i].samples.size()),
                   inner, juce::Justification::centredRight);
    }
}

void RackListComponent::resized() {}

void RackListComponent::mouseDown(const juce::MouseEvent& e)
{
    const int idx = e.y / kRowHeight;
    if (idx >= 0 && idx < (int) racks.size())
    {
        setSelected(idx);
        if (onSelect)
            onSelect(idx);
    }
}

//==============================================================================
RackContentComponent::RackContentComponent()
{
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
}

void RackContentComponent::setSamples(juce::String rackDisplayName, const std::vector<RackEntry>& entries)
{
    rackName = std::move(rackDisplayName);
    samples  = entries;
    rebuildRows();
}

void RackContentComponent::rebuildRows()
{
    rows.clear();

    static constexpr int kRowHeight = 24;
    int y = 0;
    const int w = juce::jmax(200, viewport.getWidth() - viewport.getScrollBarThickness());

    for (auto& s : samples)
    {
        auto* row = rows.add(new StackRowComponent(s.name, s.file, false));
        row->setBounds(0, y, w, kRowHeight);
        content.addAndMakeVisible(row);
        y += kRowHeight;
    }

    content.setSize(w, juce::jmax(y, viewport.getHeight()));
    viewport.repaint();
}

void RackContentComponent::resized()
{
    viewport.setBounds(getLocalBounds());
    rebuildRows();
}

void RackContentComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff141414));
    g.fillRect(getLocalBounds());

    if (samples.empty())
    {
        g.setColour(juce::Colour(0xff777777));
        g.setFont(juce::FontOptions(12.0f));
        auto msg = rackName.isNotEmpty() ? "No " + rackName + " found."
                                          : "Choose a sample library to browse racks.";
        g.drawText(msg, getLocalBounds(), juce::Justification::centred);
    }
}

//==============================================================================
RackBrowserComponent::RackBrowserComponent() : juce::Thread("RackBrowserScan")
{
    addAndMakeVisible(rackList);
    addAndMakeVisible(rackContent);

    rackList.onSelect = [this](int idx) { selectRack(idx); };
}

RackBrowserComponent::~RackBrowserComponent()
{
    stopThread(4000);
}

void RackBrowserComponent::setLibraryDir(const juce::File& dir)
{
    if (isThreadRunning())
        stopThread(2000);

    pendingDir = dir;
    startThread();
}

void RackBrowserComponent::run()
{
    auto dir = pendingDir;
    std::vector<Rack> built;

    if (dir.isDirectory())
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::map<juce::String, Rack> byId;
        for (auto& d : kRackDefs)
            byId[d.id] = Rack{ d.id, d.display, {} };

        auto files = dir.findChildFiles(juce::File::findFiles, true,
                                         "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

        for (auto& f : files)
        {
            if (threadShouldExit())
                return;

            auto id = classifySample(f, formatManager);
            byId[id].samples.push_back({ f.getFileNameWithoutExtension(), f });
        }

        for (auto& d : kRackDefs)
        {
            auto& r = byId[d.id];
            if (r.samples.empty())
                continue;

            std::sort(r.samples.begin(), r.samples.end(),
                      [](const RackEntry& a, const RackEntry& b) { return a.name < b.name; });
            built.push_back(std::move(r));
        }
    }

    if (threadShouldExit())
        return;

    juce::Component::SafePointer<RackBrowserComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, built]() mutable
    {
        if (auto* comp = safeThis.getComponent())
            comp->applyRacks(std::move(built));
    });
}

void RackBrowserComponent::applyRacks(std::vector<Rack> newRacks)
{
    racks = std::move(newRacks);
    rackList.setRacks(racks);
    selected = racks.empty() ? -1 : 0;

    if (selected >= 0)
        rackContent.setSamples(racks[(size_t) selected].display, racks[(size_t) selected].samples);
    else
        rackContent.setSamples({}, {});

    if (onRacksChanged)
        onRacksChanged(racks);
}

void RackBrowserComponent::selectRack(int index)
{
    if (index < 0 || index >= (int) racks.size())
        return;

    selected = index;
    rackList.setSelected(index);
    rackContent.setSamples(racks[(size_t) index].display, racks[(size_t) index].samples);
}

void RackBrowserComponent::resized()
{
    auto area = getLocalBounds();
    rackList.setBounds(area.removeFromLeft(kSidebarWidth));
    rackContent.setBounds(area);
}

void RackBrowserComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff2e2e2e));
    g.drawVerticalLine(kSidebarWidth, 0.0f, (float) getHeight());
}
