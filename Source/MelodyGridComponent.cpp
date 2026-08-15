#include "MelodyGridComponent.h"
#include "UIStyle.h"
#include "RowHeaderWidgets.h"
#include "MusicTheory/MelodicTechnoTheory.h"
#include "MusicTheory/MelodyCritic.h"
#include "MusicTheory/MelodyScorer.h"
#include "MusicTheory/MelodyMotifGenerator.h"
#include <algorithm>

using namespace RowHeaderWidgets;

MelodyGridComponent::MelodyGridComponent()
{
    addAndMakeVisible(headerColumn);
    stepViewport.setViewedComponent(&stepGridContent, false);
    stepViewport.setScrollBarsShown(false, true); // horizontal only
    addAndMakeVisible(stepViewport);
}

int MelodyGridComponent::addTrack(const juce::String& badgeLabel, juce::Colour colour)
{
    MelodyRow track;
    track.elementName = badgeLabel;
    track.colour      = colour;
    track.presetName  = "(no preset yet)";
    track.offsets.fill(kMelodyOff);

    tracks.push_back(std::move(track));
    resized();
    headerColumn.repaint();
    stepGridContent.repaint();
    return (int) tracks.size() - 1;
}

std::array<int8_t, MelodyGridComponent::kNumSteps> MelodyGridComponent::getTrackOffsets(int trackIndex) const
{
    return tracks[(size_t) trackIndex].offsets;
}

bool MelodyGridComponent::isTrackMuted(int trackIndex) const
{
    return tracks[(size_t) trackIndex].muted;
}

bool MelodyGridComponent::isTrackSolo(int trackIndex) const
{
    return tracks[(size_t) trackIndex].solo;
}

void MelodyGridComponent::setTrackPresetName(int trackIndex, const juce::String& name)
{
    if (trackIndex < 0 || trackIndex >= (int) tracks.size())
        return;

    tracks[(size_t) trackIndex].presetName = name;
    headerColumn.repaint();
}

void MelodyGridComponent::setTrackIdentity(int trackIndex, const juce::String& badgeLabel, juce::Colour colour)
{
    if (trackIndex < 0 || trackIndex >= (int) tracks.size())
        return;

    auto& track = tracks[(size_t) trackIndex];
    track.elementName = badgeLabel;
    track.colour      = colour;
    headerColumn.repaint();
    stepGridContent.repaint();
}

void MelodyGridComponent::setTrackMuted(int trackIndex, bool muted)
{
    if (trackIndex < 0 || trackIndex >= (int) tracks.size())
        return;

    auto& track = tracks[(size_t) trackIndex];
    track.muted = muted;
    headerColumn.repaint();
    stepGridContent.repaint();

    if (onTrackMuteToggled)
        onTrackMuteToggled(trackIndex, track.muted);
}

void MelodyGridComponent::toggleMute(int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= (int) tracks.size())
        return;

    auto& track = tracks[(size_t) trackIndex];
    track.muted = !track.muted;
    headerColumn.repaint();
    stepGridContent.repaint();

    if (onTrackMuteToggled)
        onTrackMuteToggled(trackIndex, track.muted);
}

void MelodyGridComponent::toggleSolo(int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= (int) tracks.size())
        return;

    auto& track = tracks[(size_t) trackIndex];
    track.solo = !track.solo;
    headerColumn.repaint();
    stepGridContent.repaint();

    if (onTrackSoloToggled)
        onTrackSoloToggled(trackIndex, track.solo);
}

namespace
{
    // Row is kRowHeight tall; map that vertically onto a 2-octave pitch
    // range (+12 at the top down to -12 at the bottom, root/0 in the middle).
    constexpr int kMelodyPitchRange = 12;

    int8_t pitchFromYWithinRow(int yWithinRow, int rowHeight)
    {
        const float t = juce::jlimit(0.0f, 1.0f, (float) yWithinRow / (float) rowHeight);
        const int semis = juce::roundToInt(juce::jmap(t, 1.0f, 0.0f,
                                                        (float) -kMelodyPitchRange,
                                                        (float) kMelodyPitchRange));
        return (int8_t) juce::jlimit(-kMelodyPitchRange, kMelodyPitchRange, semis);
    }
}

void MelodyGridComponent::setStepFromY(int trackIndex, int step, int yWithinRow)
{
    if (trackIndex < 0 || trackIndex >= (int) tracks.size() || step < 0 || step >= kNumSteps)
        return;

    tracks[(size_t) trackIndex].offsets[(size_t) step] = pitchFromYWithinRow(yWithinRow, kRowHeight);
    stepGridContent.repaint();

    if (onTrackChanged)
        onTrackChanged(trackIndex);
}

void MelodyGridComponent::setStepOffset(int trackIndex, int step, int8_t newOffset)
{
    if (trackIndex < 0 || trackIndex >= (int) tracks.size() || step < 0 || step >= kNumSteps)
        return;

    tracks[(size_t) trackIndex].offsets[(size_t) step] = newOffset;
    stepGridContent.repaint();

    if (onTrackChanged)
        onTrackChanged(trackIndex);
}

void MelodyGridComponent::toggleStep(int trackIndex, int step, int yWithinRow)
{
    if (trackIndex < 0 || trackIndex >= (int) tracks.size() || step < 0 || step >= kNumSteps)
        return;

    auto& offsets = tracks[(size_t) trackIndex].offsets;
    if (offsets[(size_t) step] == kMelodyOff)
        offsets[(size_t) step] = pitchFromYWithinRow(yWithinRow, kRowHeight);
    else
        offsets[(size_t) step] = kMelodyOff;

    stepGridContent.repaint();

    if (onTrackChanged)
        onTrackChanged(trackIndex);
}

namespace
{
    // The motif-shape tables and per-pass arc progression used to live
    // here - extracted to Source/MusicTheory/MelodyMotifGenerator.cpp so
    // the loop-generator workflow (Source/Engine/MusicIdentity.h's JUCE-
    // side melody counterpart) can call the same generator this file's
    // own generateMelodyFromMotifShapes calls, instead of the codebase
    // ending up with two independent melody-motif generators.

    juce::File bassFragmentsFile()
    {
        return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                   .getChildFile("Developer/AbletonCopilot/MLPipeline/fragments/melodic_family_bass_fragments.json");
    }

    // Per-pass semitone shift across the 4 x 2-bar phrase a real fragment is
    // carried through — steady with a lift, rising, a dip-then-lift, or a
    // held climax at the end. Semitone (not scale-degree) shifts, since the
    // fragment's own note relationships are already real and shouldn't be
    // re-quantized onto a different scale.
    constexpr int kFragmentArcShapes[][4] = {
        { 0, 0, 12, 0 },
        { 0, 7, 12, 0 },
        { 0, -5, 5, 0 },
        { 0, 0, 0, 12 },
    };

    // The genre's actual "workhorse" bass rhythm, not an invented shape —
    // sourced from real production guides (Myloops' bassline-construction
    // guide and corroborating sources): notes on the off-beat 8ths (the
    // "and" of each beat - steps 2/6/10/14 per 16-step bar), because a
    // melodic-techno kick's 300-500ms decay leaves no room for a low note
    // on the downbeat. Mostly root, with the fifth/minor-3rd/flat-7 as the
    // named harmonic tools for variation - all four are members of the
    // Aeolian scale by construction, so this needs no scale-snapping for
    // minor keys (still passed through nearestInScaleTone for major-key
    // safety, same as every other candidate).
    void buildCanonicalOffbeatBassCandidate(std::array<int8_t, 128>& dest, bool isMinor, juce::Random& rng)
    {
        dest.fill(MelodyGridComponent::kMelodyOff);
        static const int kOffbeatSteps[4] = { 2, 6, 10, 14 };
        static const int kVariationTones[3] = { 3, 7, 10 }; // minor-3rd, fifth, flat-7 above root

        for (int bar = 0; bar < 8; ++bar)
        {
            for (int s : kOffbeatSteps)
            {
                const int step = bar * 16 + s;
                const bool isLastStepOfLoop = (bar == 7 && s == 14);

                int semitone = 0; // root ~70% of the time
                if (!isLastStepOfLoop && rng.nextFloat() < 0.30f)
                    semitone = kVariationTones[rng.nextInt(3)];

                dest[(size_t) step] = MelodyCritic::nearestInScaleTone((int8_t) semitone, isMinor);
            }
        }
    }
}

bool MelodyGridComponent::parseFragmentsFile(const juce::File& file,
                                              std::vector<std::array<int8_t, kFragmentSteps>>& outFragments) const
{
    if (!file.existsAsFile())
        return false;

    auto parsed = juce::JSON::parse(file);
    if (!parsed.isArray())
        return false;

    auto* array = parsed.getArray();
    outFragments.reserve(outFragments.size() + (size_t) array->size());

    for (auto& entry : *array)
    {
        if (!entry.isObject())
            continue;

        auto* steps = entry.getProperty("steps", {}).getArray();
        if (steps == nullptr || steps->size() != kFragmentSteps)
            continue;

        std::array<int8_t, kFragmentSteps> fragment;
        fragment.fill(kMelodyOff);

        for (int i = 0; i < kFragmentSteps; ++i)
        {
            const auto& v = (*steps)[i];
            if (!v.isVoid())
                fragment[(size_t) i] = (int8_t) juce::jlimit(-24, 24, (int) v);
        }

        outFragments.push_back(fragment);
    }

    return true;
}

void MelodyGridComponent::loadBassFragmentsIfNeeded()
{
    if (fragmentsLoaded)
        return;
    fragmentsLoaded = true;

    if (!parseFragmentsFile(bassFragmentsFile(), bassFragments))
        return;

    // Bucket by note density (steps active out of 32) so a style word
    // actually changes the output — thresholds picked from the real
    // library's distribution (median 8, p25 6, p75 11 across 3184
    // fragments), so both buckets land at a healthy ~1000 fragments.
    for (size_t i = 0; i < bassFragments.size(); ++i)
    {
        const auto& fragment = bassFragments[i];
        const int density = (int) std::count_if(fragment.begin(), fragment.end(),
                                                  [](int8_t v) { return v != kMelodyOff; });
        if (density <= 6)
            sparseBassFragmentIndices.push_back((int) i);
        else if (density >= 11)
            denseBassFragmentIndices.push_back((int) i);
    }
}

void MelodyGridComponent::loadReferenceFragments(const juce::File& jsonFile)
{
    referenceBassFragments.clear();
    parseFragmentsFile(jsonFile, referenceBassFragments);
    stepGridContent.repaint();
}

void MelodyGridComponent::clearReferenceFragments()
{
    referenceBassFragments.clear();
}

void MelodyGridComponent::generateBassFromFragments(int trackIndex, int keyRootSemitone, bool isMinor, MelodyStyle style)
{
    juce::ignoreUnused(keyRootSemitone); // fragments are stored root-relative already

    if (trackIndex < 0 || trackIndex >= (int) tracks.size())
        return;

    auto& track = tracks[(size_t) trackIndex];

    juce::Random rng;

    // A loaded reference track (Tier 2) fully overrides the generic
    // style-based pool - the point of loading one is to generate like that
    // specific track, not to blend it with the generic corpus. No density
    // bucketing here either: a single reference track typically yields far
    // fewer fragments than the thresholds below were tuned for (median 8 /
    // p25 6 / p75 11 across 3184 corpus fragments), so style is ignored
    // while a reference is active.
    const bool usingReference = !referenceBassFragments.empty();
    const auto& fragmentSource = usingReference ? referenceBassFragments : bassFragments;

    const std::vector<int>* pool = nullptr;
    if (!usingReference)
    {
        if (style == MelodyStyle::Rolling && !denseBassFragmentIndices.empty())
            pool = &denseBassFragmentIndices;
        else if (style == MelodyStyle::Atmospheric && !sparseBassFragmentIndices.empty())
            pool = &sparseBassFragmentIndices;
    }

    const int stepsPerPass = kNumSteps / 4; // 32 steps = 2 bars per pass
    const auto& reg = MelodicTechnoTheory::kCategoryRegister[(int) MelodyCategory::Bass];

    // Constrain, then rank the legal space: try a few fragment/arc picks
    // (each already register-clamped/scale-snapped by construction) and
    // keep the most idiomatic one instead of the first roll.
    constexpr int kNumCandidates = 4;
    std::array<int8_t, 128> best {};
    float bestScore = -1.0f;

    for (int candidate = 0; candidate < kNumCandidates; ++candidate)
    {
        std::array<int8_t, 128> buf;
        buf.fill(kMelodyOff);

        const int fragmentIndex = pool != nullptr
            ? (*pool)[(size_t) rng.nextInt((int) pool->size())]
            : rng.nextInt((int) fragmentSource.size());

        const auto& fragment = fragmentSource[(size_t) fragmentIndex];
        const auto& arc = kFragmentArcShapes[rng.nextInt((int) (sizeof(kFragmentArcShapes) / sizeof(kFragmentArcShapes[0])))];

        for (int pass = 0; pass < 4; ++pass)
        {
            const int segStart = pass * stepsPerPass;

            for (int i = 0; i < kFragmentSteps; ++i)
            {
                if (fragment[(size_t) i] == kMelodyOff)
                    continue;

                const int step = segStart + i;
                if (step < 0 || step >= kNumSteps)
                    continue;

                // Resolve the final note of the final pass back to the root, so
                // the 8-bar loop restarts cleanly instead of hanging mid-phrase.
                const bool isLastNoteOfLastPass = pass == 3
                    && [&]
                       {
                           for (int j = kFragmentSteps - 1; j > i; --j)
                               if (fragment[(size_t) j] != kMelodyOff)
                                   return false;
                           return true;
                       }();

                int semitone = isLastNoteOfLastPass ? 0 : (int) fragment[(size_t) i] + arc[pass];

                // Real transcribed fragments are raw audio-derived semitone data -
                // unlike the degree-based motif generator, nothing guarantees
                // they respect the selected key or a sane bass register. Fix
                // both here rather than just flagging it after the fact.
                while (semitone > reg.maxSemitone) semitone -= 12;
                while (semitone < reg.minSemitone) semitone += 12;
                semitone = juce::jlimit(reg.minSemitone, reg.maxSemitone, semitone);

                buf[(size_t) step] = MelodyCritic::nearestInScaleTone((int8_t) semitone, isMinor);
            }
        }

        const float score = MelodyScorer::scoreTrack(buf, MelodyCategory::Bass, keyRootSemitone, isMinor).total;
        if (score > bestScore)
        {
            bestScore = score;
            best = buf;
        }
    }

    // The genre's actual workhorse pattern gets a seat at the table too -
    // competes on the same scoring function as the fragment-derived
    // candidates rather than being forced in, but for Bass specifically it
    // has a real, sourced claim to being the idiomatic default.
    {
        std::array<int8_t, 128> offbeatBuf;
        buildCanonicalOffbeatBassCandidate(offbeatBuf, isMinor, rng);
        const float score = MelodyScorer::scoreTrack(offbeatBuf, MelodyCategory::Bass, keyRootSemitone, isMinor).total;
        if (score > bestScore)
        {
            bestScore = score;
            best = offbeatBuf;
        }
    }

    track.offsets = best;

    stepGridContent.repaint();

    if (onTrackChanged)
        onTrackChanged(trackIndex);
}

void MelodyGridComponent::generateMelodyFromMotifShapes(int trackIndex, int keyRootSemitone, bool isMinor,
                                                          MelodyCategory category, MelodyStyle style)
{
    if (trackIndex < 0 || trackIndex >= (int) tracks.size())
        return;

    // "Call again for a different variation — it's randomized each time"
    // (see generateForTrack's own doc comment) - this manual-editing UI
    // path keeps that behaviour by drawing a fresh system-random seed on
    // every call, unlike the loop-generator workflow's own call into the
    // same generator (Source/Engine/MusicIdentity.h's JUCE-side melody
    // counterpart), which passes an explicit seed for determinism.
    const uint32_t seed = (uint32_t) juce::Random::getSystemRandom().nextInt();
    tracks[(size_t) trackIndex].offsets =
        MelodyMotifGenerator::generateMelodyMotif(keyRootSemitone, isMinor, category, style, seed);

    stepGridContent.repaint();

    if (onTrackChanged)
        onTrackChanged(trackIndex);
}

void MelodyGridComponent::generateForTrack(int trackIndex, int keyRootSemitone, bool isMinor,
                                            MelodyCategory category, MelodyStyle style)
{
    if (category == MelodyCategory::Bass)
    {
        loadBassFragmentsIfNeeded();
        if (!bassFragments.empty() || !referenceBassFragments.empty())
        {
            generateBassFromFragments(trackIndex, keyRootSemitone, isMinor, style);
            return;
        }
    }

    generateMelodyFromMotifShapes(trackIndex, keyRootSemitone, isMinor, category, style);
}

void MelodyGridComponent::setPlayheadStep(int step, bool visible)
{
    if (playheadStep == step && playheadVisible == visible)
        return;

    playheadStep    = step;
    playheadVisible = visible;
    stepGridContent.repaint();
}

void MelodyGridComponent::resized()
{
    auto area = getLocalBounds();
    headerColumn.setBounds(area.removeFromLeft(kHeaderWidth));
    stepViewport.setBounds(area);

    const int rowCount = (int) tracks.size();
    headerColumn.setSize(kHeaderWidth, kRowHeight * rowCount);
    stepGridContent.setSize(kNumSteps * kStepWidth, kRowHeight * rowCount);
}

//==============================================================================

void MelodyGridComponent::HeaderColumn::paint(juce::Graphics& g)
{
    g.fillAll(UIStyle::kBg);

    for (int i = 0; i < (int) owner.tracks.size(); ++i)
    {
        auto& track = owner.tracks[(size_t) i];
        const int rowTop = i * MelodyGridComponent::kRowHeight;
        auto rowArea = juce::Rectangle<int>(0, rowTop, getWidth(), MelodyGridComponent::kRowHeight);

        g.setColour(UIStyle::kBorder);
        g.drawHorizontalLine(rowTop, 0.0f, (float) getWidth());

        const bool dimmed = track.muted;
        auto header = rowArea.reduced(10, 2);

        auto badgeArea = header.removeFromLeft(MelodyGridComponent::kBadgeDiameter);
        paintRingBadge(g, badgeArea.toFloat(), track.colour, track.elementName, dimmed);
        header.removeFromLeft(8);

        auto arrowsArea = header.removeFromRight(2 * MelodyGridComponent::kArrowWidth);
        auto leftArrow  = arrowsArea.removeFromLeft(MelodyGridComponent::kArrowWidth);
        auto rightArrow = arrowsArea;
        header.removeFromRight(6);

        auto msColumn = header.removeFromLeft(MelodyGridComponent::kMuteWidth);
        header.removeFromLeft(6);
        auto ms = layoutMuteSolo(msColumn);
        paintToggleButton(g, ms.mute, "M", track.muted, UIStyle::kMuteActive);
        paintToggleButton(g, ms.solo, "S", track.solo,  UIStyle::kSoloActive);

        g.setColour(UIStyle::kTextDim);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("<", leftArrow,  juce::Justification::centred);
        g.drawText(">", rightArrow, juce::Justification::centred);

        g.setColour(dimmed ? UIStyle::kTextDim.withMultipliedAlpha(0.7f) : UIStyle::kTextDim);
        g.setFont(UIStyle::body());
        g.drawText(track.presetName, header, juce::Justification::centredLeft);
    }
}

void MelodyGridComponent::HeaderColumn::mouseDown(const juce::MouseEvent& e)
{
    const int rowIdx = e.y / MelodyGridComponent::kRowHeight;
    if (rowIdx < 0 || rowIdx >= (int) owner.tracks.size())
        return;

    auto header = juce::Rectangle<int>(0, rowIdx * MelodyGridComponent::kRowHeight,
                                        MelodyGridComponent::kHeaderWidth,
                                        MelodyGridComponent::kRowHeight).reduced(10, 2);
    header.removeFromLeft(MelodyGridComponent::kBadgeDiameter + 8);

    auto arrowsArea = header.removeFromRight(2 * MelodyGridComponent::kArrowWidth);
    auto leftArrow  = arrowsArea.removeFromLeft(MelodyGridComponent::kArrowWidth);
    auto rightArrow = arrowsArea;
    header.removeFromRight(6);

    auto msColumn = header.removeFromLeft(MelodyGridComponent::kMuteWidth);
    auto ms = layoutMuteSolo(msColumn);

    if (leftArrow.contains(e.getPosition()))
    {
        if (owner.onTrackPresetCycle) owner.onTrackPresetCycle(rowIdx, -1);
    }
    else if (rightArrow.contains(e.getPosition()))
    {
        if (owner.onTrackPresetCycle) owner.onTrackPresetCycle(rowIdx, 1);
    }
    else if (ms.mute.contains(e.getPosition()))
    {
        owner.toggleMute(rowIdx);
    }
    else if (ms.solo.contains(e.getPosition()))
    {
        owner.toggleSolo(rowIdx);
    }
}

//==============================================================================

void MelodyGridComponent::StepGridContent::paint(juce::Graphics& g)
{
    static const juce::Colour kBarLine { 0xff333438 };

    const int w = getWidth();

    for (int i = 0; i < (int) owner.tracks.size(); ++i)
    {
        auto& track = owner.tracks[(size_t) i];
        const int rowTop = i * MelodyGridComponent::kRowHeight;

        g.setColour(i % 2 == 0 ? UIStyle::kPanelAlt : UIStyle::kRowAlt);
        g.fillRect(juce::Rectangle<int>(0, rowTop, w, MelodyGridComponent::kRowHeight));

        // Reference gridlines every half-octave, root line brighter — real
        // vertical room now (110px vs. the old 42px) makes these worth
        // drawing at all.
        for (int semi = -12; semi <= 12; semi += 6)
        {
            const float t = juce::jmap((float) semi, -12.0f, 12.0f, 1.0f, 0.0f);
            const float y = (float) rowTop + t * (float) MelodyGridComponent::kRowHeight;
            g.setColour(semi == 0 ? kBarLine.brighter(0.35f) : kBarLine.withAlpha(0.5f));
            g.drawHorizontalLine((int) y, 0.0f, (float) w);
        }

        for (int s = 0; s < MelodyGridComponent::kNumSteps; ++s)
        {
            const int8_t offset = track.offsets[(size_t) s];
            if (offset == MelodyGridComponent::kMelodyOff)
                continue;

            const float t = juce::jmap((float) offset, -12.0f, 12.0f, 1.0f, 0.0f);
            const float cellX = (float) (s * MelodyGridComponent::kStepWidth);
            const float noteH = 16.0f; // taller note blocks now there's real room
            const float noteY = (float) rowTop + t * ((float) MelodyGridComponent::kRowHeight - noteH);

            auto cell = juce::Rectangle<float>(cellX, noteY,
                                               (float) MelodyGridComponent::kStepWidth, noteH)
                            .reduced(2.5f, 0.0f);
            if (s % 4 == 0 && s > 0)
                cell = cell.withX(cell.getX() + 3.0f).withWidth(cell.getWidth() - 3.0f);

            g.setColour(track.muted ? track.colour.withMultipliedAlpha(0.35f) : track.colour);
            g.fillRoundedRectangle(cell, 4.0f);
        }
    }

    // Bar dividers every 16 steps.
    g.setColour(kBarLine);
    for (int bar = 1; bar < MelodyGridComponent::kNumBars; ++bar)
        g.drawVerticalLine(bar * MelodyGridComponent::kStepsPerBar * MelodyGridComponent::kStepWidth,
                           0.0f, (float) getHeight());

    if (owner.playheadVisible && owner.playheadStep >= 0 && owner.playheadStep < MelodyGridComponent::kNumSteps)
    {
        const float x = (float) (owner.playheadStep * MelodyGridComponent::kStepWidth);
        g.setColour(UIStyle::kAccent.withAlpha(0.5f));
        g.fillRect(juce::Rectangle<float>(x, 0.0f, (float) MelodyGridComponent::kStepWidth, (float) getHeight()));
    }
}

void MelodyGridComponent::StepGridContent::mouseDown(const juce::MouseEvent&)
{
    // Wait for mouseUp/mouseDrag to decide toggle vs. pitch-drag.
    dragStarted = false;
}

void MelodyGridComponent::StepGridContent::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragStarted && e.getDistanceFromDragStart() < 4)
        return;

    dragStarted = true;
    const int rowIdx     = e.y / MelodyGridComponent::kRowHeight;
    const int step       = e.x / MelodyGridComponent::kStepWidth;
    const int yWithinRow = e.y - rowIdx * MelodyGridComponent::kRowHeight;
    owner.setStepFromY(rowIdx, step, yWithinRow);
}

void MelodyGridComponent::StepGridContent::mouseUp(const juce::MouseEvent& e)
{
    if (dragStarted)
    {
        dragStarted = false;
        return; // was a drag, not a click
    }

    const int rowIdx     = e.y / MelodyGridComponent::kRowHeight;
    const int step       = e.x / MelodyGridComponent::kStepWidth;
    const int yWithinRow = e.y - rowIdx * MelodyGridComponent::kRowHeight;
    owner.toggleStep(rowIdx, step, yWithinRow);
}
