#include "DrumMachineComponent.h"
#include "UIStyle.h"
#include "RowHeaderWidgets.h"
#include "Analysis/FeatureExtractor.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace RowHeaderWidgets;

namespace
{
    struct RowTemplate
    {
        const char*  id;         // rack id — shared between a category's primary/variant rows
        const char*  display;
        const char*  badge;      // short label inside the row's ring badge
        juce::Colour colour;
        bool         isVariant;  // KICK2/CLAP2-style layer — only spawned if the rack has >=2 samples
    };

    // Row order = priority when several categories have content. LOOP is
    // deliberately never a row — loops aren't one-shots, sequencing them as
    // repeated short retriggers is exactly what sounded broken before.
    //
    // A freshly-spawned row starts completely blank — no sample picked, no
    // steps filled. This used to auto-fill a hand-invented hardcoded
    // pattern (four-on-the-floor kick, 2-and-4 backbeat, etc.) the moment a
    // library scan found matching samples, meaning every project opened
    // with the exact same pre-baked pattern regardless of genre or any
    // reference track — invented content presented as if it meant
    // something. Now the user deliberately builds the pattern by hand, or
    // applies a real reference-derived one (see
    // DrumMachineComponent::applyReferenceDrumPattern).
    const RowTemplate kRowTemplates[] = {
        { "KICK",   "Kick",     "KICK", UIStyle::kKick,    false },
        { "SNARE",  "Snare",    "SNAR", UIStyle::kSnare,   false },
        { "CLAP",   "Clap",     "CLAP", UIStyle::kClap,    false },
        { "CLAP",   "Clap 2",   "CLP2", UIStyle::kClapAlt, true  },
        { "HIHAT",  "Hi-Hat",   "HHAT", UIStyle::kHihat,   false },
        { "CYMBAL", "Open Hat", "OPHT", UIStyle::kCymbal,  false },
        { "PERC",   "Perc",     "PERC", UIStyle::kPerc,    false },
        { "BASS",   "Bass",     "BASS", UIStyle::kBass,    false },
        { "FX",     "FX",       "FX",   UIStyle::kFx,      false },
        { "VOCAL",  "Vocal",    "VOCL", UIStyle::kVocal,   false },
        { "MISC",   "Other",    "OTHR", UIStyle::kMisc,    false },
    };
}

DrumMachineComponent::DrumMachineComponent()
{
    addAndMakeVisible(headerColumn);
    stepViewport.setViewedComponent(&stepGridContent, false);
    stepViewport.setScrollBarsShown(false, true); // horizontal only
    addAndMakeVisible(stepViewport);
}

void DrumMachineComponent::refreshFromRacks(const std::vector<Rack>& racks)
{
    auto findRack = [&](const juce::String& id) -> const Rack*
    {
        for (auto& r : racks)
            if (r.id == id)
                return &r;
        return nullptr;
    };

    std::vector<DrumRow> oldRows = std::move(rows);
    rows.clear();

    for (auto& tmpl : kRowTemplates)
    {
        const Rack* rack = findRack(tmpl.id);
        if (rack == nullptr || rack->samples.empty())
            continue;
        if (tmpl.isVariant && rack->samples.size() < 2)
            continue; // not enough distinct samples to justify a layered second row

        DrumRow row;
        row.elementName = tmpl.display;
        row.badgeLabel  = tmpl.badge;
        row.rackId      = tmpl.id;
        row.colour      = tmpl.colour;
        row.available   = rack->samples;

        // Preserve manual step edits (and sample choice) across a rescan of
        // a rack that was already showing as a row. Keyed on (rackId,
        // badge) rather than rackId alone, since a category can now have
        // two rows (e.g. both "KICK" entries) sharing the same rackId.
        auto oldIt = std::find_if(oldRows.begin(), oldRows.end(),
                                   [&](const DrumRow& r) { return r.rackId == tmpl.id && r.badgeLabel == tmpl.badge; });

        // A brand-new row starts genuinely blank - no sample auto-picked,
        // no pattern auto-filled. The old behaviour (defaulting to
        // tmpl.startSampleIndex + a hardcoded buildArrangedPattern() step
        // template) meant every project opened with the exact same
        // pre-baked kick/clap/hat pattern regardless of genre or any
        // reference track, which is exactly the kind of invented-not-real
        // content this plugin is trying to get away from. The user now
        // deliberately picks a sample (cycle arrows) and either hand-edits
        // steps or applies a reference-derived pattern.
        if (oldIt != oldRows.end())
        {
            row.sampleIndex = oldIt->sampleIndex;
            row.steps       = oldIt->steps;
            row.muted       = oldIt->muted;
            row.solo        = oldIt->solo;
        }
        else
        {
            row.sampleIndex = -1;
            // row.steps already zero-initialized (all false) by DrumRow's
            // default member initializer.
        }

        rows.push_back(std::move(row));
    }

    resized();
    headerColumn.repaint();
    stepGridContent.repaint();
}

void DrumMachineComponent::setPlayheadStep(int step, bool visible)
{
    if (playheadStep == step && playheadVisible == visible)
        return;

    playheadStep    = step;
    playheadVisible = visible;
    stepGridContent.repaint();
}

juce::String DrumMachineComponent::getRowName(int rowIndex) const
{
    return rows[(size_t) rowIndex].elementName;
}

juce::String DrumMachineComponent::getRowRackId(int rowIndex) const
{
    return rows[(size_t) rowIndex].rackId;
}

std::array<bool, DrumMachineComponent::kNumSteps> DrumMachineComponent::getRowSteps(int rowIndex) const
{
    return rows[(size_t) rowIndex].steps;
}

juce::File DrumMachineComponent::getSampleForRow(int rowIndex) const
{
    auto& row = rows[(size_t) rowIndex];
    return row.sampleIndex >= 0 ? row.available[(size_t) row.sampleIndex].file : juce::File();
}

bool DrumMachineComponent::isRowMuted(int rowIndex) const
{
    return rows[(size_t) rowIndex].muted;
}

bool DrumMachineComponent::isRowSolo(int rowIndex) const
{
    return rows[(size_t) rowIndex].solo;
}

void DrumMachineComponent::cycleSample(int rowIndex, int direction)
{
    auto& row = rows[(size_t) rowIndex];
    if (row.available.empty())
        return;

    const int n = (int) row.available.size();
    row.sampleIndex = ((row.sampleIndex + direction) % n + n) % n;
    headerColumn.repaint();

    if (onSampleCycled)
        onSampleCycled(rowIndex);
}

juce::File DrumMachineComponent::getSampleForRackId(const juce::String& rackId) const
{
    for (auto& row : rows)
        if (row.rackId.equalsIgnoreCase(rackId))
            return row.sampleIndex >= 0 ? row.available[(size_t) row.sampleIndex].file : juce::File();
    return {};
}

bool DrumMachineComponent::setRowSampleByFile(const juce::String& rackId, const juce::File& file)
{
    for (int rowIndex = 0; rowIndex < (int) rows.size(); ++rowIndex)
    {
        auto& row = rows[(size_t) rowIndex];
        if (!row.rackId.equalsIgnoreCase(rackId))
            continue;

        for (int i = 0; i < (int) row.available.size(); ++i)
        {
            if (row.available[(size_t) i].file != file)
                continue;

            row.sampleIndex = i;
            headerColumn.repaint();

            if (onSampleCycled)
                onSampleCycled(rowIndex);
            return true;
        }
    }
    return false;
}

namespace
{
    // Decodes a whole audio file and runs it through FeatureExtractor - the
    // same spectral-band-balance analysis already used for the reference-
    // track/genre-comparison work, reused here as a timbral "fingerprint"
    // for a single drum one-shot rather than a whole mix. Returns an
    // invalid (AudioFeatures::valid == false) result if the file can't be
    // read or is too short for the FFT window.
    AudioFeatures analyzeOneShot(const juce::File& file)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
        if (reader == nullptr)
            return {};

        juce::AudioBuffer<float> buf(juce::jmax(1, (int) reader->numChannels),
                                      (int) reader->lengthInSamples);
        reader->read(&buf, 0, (int) reader->lengthInSamples, 0, true, true);

        FeatureExtractor extractor;
        return extractor.extract(buf, reader->sampleRate);
    }

    // Sum of absolute differences across the spectral-band-balance +
    // brightness features - small, explainable, and reuses fields that
    // already mean something (percentage of energy in each band), rather
    // than a black-box fingerprint. Not a perceptual-similarity model, just
    // "how similar do these two one-shots sit in the spectrum" - good
    // enough to prefer a real kick over a real hi-hat, not meant to be
    // exact.
    float oneShotDistance(const AudioFeatures& a, const AudioFeatures& b)
    {
        if (!a.valid || !b.valid)
            return std::numeric_limits<float>::max();

        float d = 0.0f;
        d += std::abs(a.subPct - b.subPct);
        d += std::abs(a.lowPct - b.lowPct);
        d += std::abs(a.lowMidPct - b.lowMidPct);
        d += std::abs(a.highMidPct - b.highMidPct);
        d += std::abs(a.airPct - b.airPct);
        d += std::abs(a.spectralCentroidHz - b.spectralCentroidHz) / 5000.0f; // rough normalization
        return d;
    }
}

bool DrumMachineComponent::applyReferenceDrumPattern(const juce::String& rackId, const std::array<bool, 128>& steps,
                                                       const juce::File& sampleFile)
{
    auto finish = [this](int rowIndex, DrumRow& row, const std::array<bool, 128>& newSteps)
    {
        row.steps = newSteps;
        headerColumn.repaint();
        stepGridContent.repaint();

        if (onSampleCycled)
            onSampleCycled(rowIndex);
        if (onStepToggled)
            for (int step = 0; step < kNumSteps; ++step)
                onStepToggled(rowIndex, step, newSteps[(size_t) step]);
    };

    for (int rowIndex = 0; rowIndex < (int) rows.size(); ++rowIndex)
    {
        auto& row = rows[(size_t) rowIndex];
        if (!row.rackId.equalsIgnoreCase(rackId))
            continue;

        // Prefer a real match from the user's own already-scanned library
        // over the raw extracted one-shot itself - Demucs separation
        // leaves real artifacts (phase, bleed, reverb tail from the mix),
        // so a properly-produced sample the user already owns that's
        // spectrally closest to the reference hit usually sounds cleaner
        // than playing the separated audio directly. Only matches within
        // this row's own candidates (same rack/category) - never borrows
        // from a different role.
        int bestIndex = -1;
        if (!row.available.empty())
        {
            const AudioFeatures target = analyzeOneShot(sampleFile);
            if (target.valid)
            {
                float bestDist = std::numeric_limits<float>::max();
                for (int i = 0; i < (int) row.available.size(); ++i)
                {
                    const float d = oneShotDistance(target, analyzeOneShot(row.available[(size_t) i].file));
                    if (d < bestDist) { bestDist = d; bestIndex = i; }
                }
            }
        }

        if (bestIndex >= 0)
        {
            row.sampleIndex = bestIndex;
        }
        else
        {
            // No real candidates to match against (or analysis failed) -
            // the raw extracted one-shot is still real audio from the
            // reference track, so it's a reasonable fallback on its own.
            RackEntry refEntry;
            refEntry.name = "From Reference";
            refEntry.file = sampleFile;
            row.available.insert(row.available.begin(), refEntry);
            row.sampleIndex = 0;
        }

        finish(rowIndex, row, steps);
        return true;
    }

    // No existing row for this role - the user's own library scan never
    // found real samples for it, so refreshFromRacks() never created a row
    // (rows are never invented from nothing - see its comment). Nothing to
    // match against yet, so the raw extracted one-shot is the only option -
    // still real audio from the reference track, enough on its own to
    // build a working row from scratch using the same badge/colour/display
    // metadata kRowTemplates already defines for this rackId. Self-serve:
    // doesn't require the user's separate sample library to already have a
    // matching category.
    for (auto& tmpl : kRowTemplates)
    {
        if (!juce::String(tmpl.id).equalsIgnoreCase(rackId) || tmpl.isVariant)
            continue;

        DrumRow row;
        row.elementName = tmpl.display;
        row.badgeLabel  = tmpl.badge;
        row.rackId      = tmpl.id;
        row.colour      = tmpl.colour;

        RackEntry refEntry;
        refEntry.name = "From Reference";
        refEntry.file = sampleFile;
        row.available.push_back(refEntry);
        row.sampleIndex = 0;

        const int rowIndex = (int) rows.size();
        rows.push_back(std::move(row));
        resized();

        finish(rowIndex, rows[(size_t) rowIndex], steps);
        return true;
    }

    return false;
}

void DrumMachineComponent::toggleMute(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= (int) rows.size())
        return;

    auto& row = rows[(size_t) rowIndex];
    row.muted = !row.muted;
    headerColumn.repaint();
    stepGridContent.repaint();

    if (onMuteToggled)
        onMuteToggled(rowIndex, row.muted);
}

void DrumMachineComponent::toggleSolo(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= (int) rows.size())
        return;

    auto& row = rows[(size_t) rowIndex];
    row.solo = !row.solo;
    headerColumn.repaint();
    stepGridContent.repaint();

    if (onSoloToggled)
        onSoloToggled(rowIndex, row.solo);
}

void DrumMachineComponent::toggleStep(int rowIndex, int step)
{
    if (rowIndex < 0 || rowIndex >= (int) rows.size() || step < 0 || step >= kNumSteps)
        return;

    auto& row = rows[(size_t) rowIndex];
    row.steps[(size_t) step] = !row.steps[(size_t) step];
    stepGridContent.repaint();

    if (onStepToggled)
        onStepToggled(rowIndex, step, row.steps[(size_t) step]);
}

void DrumMachineComponent::resized()
{
    auto area = getLocalBounds();
    headerColumn.setBounds(area.removeFromLeft(kHeaderWidth));
    stepViewport.setBounds(area);

    const int rowCount = (int) rows.size();
    stepGridContent.setSize(kNumSteps * kStepWidth, kRowHeight * rowCount);
    headerColumn.setSize(kHeaderWidth, kRowHeight * rowCount);
}

//==============================================================================

void DrumMachineComponent::HeaderColumn::paint(juce::Graphics& g)
{
    g.fillAll(UIStyle::kBg);

    for (int i = 0; i < (int) owner.rows.size(); ++i)
    {
        auto& row = owner.rows[(size_t) i];
        const int rowTop = i * DrumMachineComponent::kRowHeight;
        auto rowArea = juce::Rectangle<int>(0, rowTop, getWidth(), DrumMachineComponent::kRowHeight);

        g.setColour(UIStyle::kBorder);
        g.drawHorizontalLine(rowTop, 0.0f, (float) getWidth());

        const bool dimmed = row.muted;
        auto header = rowArea.reduced(10, 2);

        auto badgeArea = header.removeFromLeft(DrumMachineComponent::kBadgeDiameter);
        paintRingBadge(g, badgeArea.toFloat(), row.colour, row.badgeLabel, dimmed);
        header.removeFromLeft(8);

        auto arrowsArea = header.removeFromRight(2 * DrumMachineComponent::kArrowWidth);
        auto leftArrow  = arrowsArea.removeFromLeft(DrumMachineComponent::kArrowWidth);
        auto rightArrow = arrowsArea;
        header.removeFromRight(6); // gap before arrows

        auto msColumn = header.removeFromLeft(DrumMachineComponent::kMuteWidth);
        header.removeFromLeft(6); // gap before name
        auto ms = layoutMuteSolo(msColumn);
        paintToggleButton(g, ms.mute, "M", row.muted, UIStyle::kMuteActive);
        paintToggleButton(g, ms.solo, "S", row.solo,  UIStyle::kSoloActive);

        g.setColour(UIStyle::kTextDim);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("<", leftArrow,  juce::Justification::centred);
        g.drawText(">", rightArrow, juce::Justification::centred);

        g.setColour(dimmed ? UIStyle::kTextDim.withMultipliedAlpha(0.7f) : UIStyle::kTextDim);
        g.setFont(UIStyle::body());
        juce::String sampleName = row.sampleIndex >= 0
                                       ? row.available[(size_t) row.sampleIndex].name
                                       : "(no sample)";
        g.drawText(sampleName, header, juce::Justification::centredLeft);
    }
}

void DrumMachineComponent::HeaderColumn::mouseDown(const juce::MouseEvent& e)
{
    const int rowIdx = e.y / DrumMachineComponent::kRowHeight;
    if (rowIdx < 0 || rowIdx >= (int) owner.rows.size())
        return;

    auto header = juce::Rectangle<int>(0, rowIdx * DrumMachineComponent::kRowHeight,
                                        DrumMachineComponent::kHeaderWidth,
                                        DrumMachineComponent::kRowHeight).reduced(10, 2);
    header.removeFromLeft(DrumMachineComponent::kBadgeDiameter + 8); // badge + gap

    auto arrowsArea = header.removeFromRight(2 * DrumMachineComponent::kArrowWidth);
    auto leftArrow  = arrowsArea.removeFromLeft(DrumMachineComponent::kArrowWidth);
    auto rightArrow = arrowsArea;
    header.removeFromRight(6); // gap before arrows

    auto msColumn = header.removeFromLeft(DrumMachineComponent::kMuteWidth);
    auto ms = layoutMuteSolo(msColumn);

    if (leftArrow.contains(e.getPosition()))
        owner.cycleSample(rowIdx, -1);
    else if (rightArrow.contains(e.getPosition()))
        owner.cycleSample(rowIdx, 1);
    else if (ms.mute.contains(e.getPosition()))
        owner.toggleMute(rowIdx);
    else if (ms.solo.contains(e.getPosition()))
        owner.toggleSolo(rowIdx);
}

//==============================================================================
namespace
{
    // Flat-top hexagon (flat horizontal edges top/bottom, pointed left/right)
    // inscribed in bounds — tiles cleanly in a horizontal step row, matching
    // XO's actual step-button shape.
    juce::Path makeHexagon(juce::Rectangle<float> bounds)
    {
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float rx = bounds.getWidth()  * 0.5f;
        const float ry = bounds.getHeight() * 0.5f;

        juce::Path p;
        for (int i = 0; i < 6; ++i)
        {
            const float angle = juce::MathConstants<float>::twoPi * (float) i / 6.0f;
            const float x = cx + rx * std::cos(angle);
            const float y = cy + ry * std::sin(angle);
            if (i == 0)
                p.startNewSubPath(x, y);
            else
                p.lineTo(x, y);
        }
        p.closeSubPath();
        return p;
    }
}

void DrumMachineComponent::StepGridContent::paint(juce::Graphics& g)
{
    static const juce::Colour kCellOff { 0xff2a2b2f };
    static const juce::Colour kBarLine { 0xff333438 };

    const int w = getWidth();

    for (int i = 0; i < (int) owner.rows.size(); ++i)
    {
        auto& row = owner.rows[(size_t) i];
        const int rowTop = i * DrumMachineComponent::kRowHeight;

        g.setColour(i % 2 == 0 ? UIStyle::kPanelAlt : UIStyle::kRowAlt);
        g.fillRect(juce::Rectangle<int>(0, rowTop, w, DrumMachineComponent::kRowHeight));

        for (int s = 0; s < DrumMachineComponent::kNumSteps; ++s)
        {
            auto cell = juce::Rectangle<float>((float) (s * DrumMachineComponent::kStepWidth),
                                               (float) rowTop,
                                               (float) DrumMachineComponent::kStepWidth,
                                               (float) DrumMachineComponent::kRowHeight)
                            .reduced(2.0f, 8.0f);
            if (s % 4 == 0 && s > 0)
                cell = cell.withX(cell.getX() + 3.0f).withWidth(cell.getWidth() - 3.0f);

            auto hex = makeHexagon(cell);
            const bool  on     = row.steps[(size_t) s];
            const float dimmed = row.muted ? 0.35f : 1.0f;

            if (on)
            {
                g.setColour(row.colour.withMultipliedAlpha(dimmed));
                g.fillPath(hex);
            }
            else
            {
                g.setColour(kCellOff.withMultipliedAlpha(dimmed));
                g.strokePath(hex, juce::PathStrokeType(1.2f));
            }
        }
    }

    // Bar dividers every 16 steps.
    g.setColour(kBarLine);
    for (int bar = 1; bar < DrumMachineComponent::kNumBars; ++bar)
        g.drawVerticalLine(bar * DrumMachineComponent::kStepsPerBar * DrumMachineComponent::kStepWidth,
                           0.0f, (float) getHeight());

    if (owner.playheadVisible && owner.playheadStep >= 0 && owner.playheadStep < DrumMachineComponent::kNumSteps)
    {
        const float x = (float) (owner.playheadStep * DrumMachineComponent::kStepWidth);
        g.setColour(UIStyle::kAccent.withAlpha(0.5f));
        g.fillRect(juce::Rectangle<float>(x, 0.0f, (float) DrumMachineComponent::kStepWidth, (float) getHeight()));
    }
}

void DrumMachineComponent::StepGridContent::mouseDown(const juce::MouseEvent& e)
{
    const int rowIdx = e.y / DrumMachineComponent::kRowHeight;
    if (rowIdx < 0 || rowIdx >= (int) owner.rows.size())
        return;

    owner.toggleStep(rowIdx, e.x / DrumMachineComponent::kStepWidth);
}
