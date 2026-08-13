#include "TheoryAdvisor.h"
#include "GenreProfiles.h"
#include <algorithm>
#include <cmath>
#include <climits>

namespace
{
    // Matches MelodyGridComponent::kMelodyOff — kept local, same convention
    // already used in MelodyCritic.cpp, so this module stays decoupled from
    // the UI-layer component.
    constexpr int8_t kNoNote = -128;

    const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F",
                                    "F#", "G", "G#", "A", "A#", "B" };

    juce::String noteName(int semitone)
    {
        return kNoteNames[((semitone % 12) + 12) % 12];
    }

    // "F# minor" -> {6, true}; returns {-1, false} if unparseable.
    struct ParsedKey { int root = -1; bool isMinor = false; };
    ParsedKey parseKey(const juce::String& key)
    {
        ParsedKey result;
        if (key.isEmpty() || key.equalsIgnoreCase("unknown"))
            return result;

        for (int i = 0; i < 12; ++i)
        {
            if (key.startsWithIgnoreCase(kNoteNames[i]))
            {
                // Guard "C" vs "C#" - only accept the longer match if it's
                // actually there, else a sharp key would match its natural first.
                bool isSharp = juce::String(kNoteNames[i]).endsWithChar('#');
                if (!isSharp && key.length() > 1 && key[juce::String(kNoteNames[i]).length()] == '#')
                    continue;
                result.root = i;
                break;
            }
        }
        result.isMinor = key.containsIgnoreCase("minor");
        return result;
    }

    const Rack* findRack(const std::vector<Rack>& racks, const char* id)
    {
        for (auto& r : racks)
            if (r.id.equalsIgnoreCase(id) && !r.samples.empty())
                return &r;
        return nullptr;
    }

    const PresetEntry* findPreset(const std::vector<PresetEntry>& presets, MelodyCategory category)
    {
        for (auto& p : presets)
            if (p.category == category)
                return &p;
        return nullptr;
    }

    bool isMelodicCategory(MelodyCategory c)
    {
        return c == MelodyCategory::Bass || c == MelodyCategory::Lead || c == MelodyCategory::Pad
            || c == MelodyCategory::Pluck || c == MelodyCategory::Synth;
    }

    // Nearest consonant offset (minor/major third or fifth, either direction)
    // relative to anchor, closest to current - used to propose a clash fix
    // that moves as little as possible.
    int8_t nearestConsonantTarget(int anchor, int current)
    {
        static const int kIntervals[] = { 3, 4, 7, -3, -4, -7 };
        int best = current;
        int bestDist = INT_MAX;
        for (int iv : kIntervals)
        {
            int candidate = juce::jlimit(-24, 24, anchor + iv);
            int dist = std::abs(candidate - current);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = candidate;
            }
        }
        return (int8_t) best;
    }
}

FeedbackItem TheoryAdvisor::idea(const juce::String& category, const juce::String& headline,
                                  const juce::String& detail, const juce::String& fix)
{
    return { Severity::Good, category, headline, detail, fix };
}

AdvisorResult TheoryAdvisor::generate(const AdvisorContext& ctx) const
{
    AdvisorResult out;
    suggestSection(ctx, out);
    suggestKeyAndProgression(ctx, out);
    suggestGroove(ctx, out);
    checkTrackClashes(ctx, out);
    suggestPresets(ctx, out);
    suggestSamples(ctx, out);
    suggestTechnique(ctx, out);
    return out;
}

void TheoryAdvisor::suggestKeyAndProgression(const AdvisorContext& ctx, AdvisorResult& out) const
{
    const auto& f = ctx.features;
    auto parsed = parseKey(f.key);
    if (parsed.root < 0)
    {
        out.informational.push_back({ Severity::Warning, "KEY",
            "Key detection inconclusive",
            "The tonal content captured so far is too sparse to detect a key reliably.",
            "Play a section with clearer bass or melodic content, then Analyze again." });
        return;
    }

    // Cross-check against the plugin's own selected key — exact ground
    // truth for what this plugin's tracks are set to, versus what the
    // audio-detection heard. A mismatch isn't necessarily wrong (could be a
    // different reference track playing), just worth knowing about - and
    // it's a real, applicable fix if the user wants the key picker to match.
    if (parsed.root != ((ctx.keyRootSemitone % 12) + 12) % 12 || parsed.isMinor != ctx.isMinor)
    {
        juce::String selectedName = noteName(ctx.keyRootSemitone) + (ctx.isMinor ? " minor" : " major");
        out.informational.push_back({ Severity::Warning, "KEY",
            "Audio sounds like " + f.key + ", but tracks are set to " + selectedName,
            "The plugin's own melody tracks are generating in " + selectedName +
            ", while the audio just captured reads more like " + f.key + ".",
            "Approve below to set the key picker to " + f.key +
            ", or ignore if you're intentionally auditioning something else." });

        out.keyChange = KeyChangeSuggestion{ parsed.root, parsed.isMinor };
    }

    juce::Random rng;
    const auto& move = MelodicTechnoTheory::kProgressions[rng.nextInt(MelodicTechnoTheory::kNumProgressions)];

    juce::String chordNotes;
    for (int i = 0; i < 4; ++i)
    {
        int semi = parsed.root + MelodicTechnoTheory::degreeToSemitone(move.degrees[i], false);
        chordNotes << noteName(semi) << (i < 3 ? " - " : "");
    }

    int dorianSixthSemi = parsed.root + MelodicTechnoTheory::degreeToSemitone(5, true);

    out.informational.push_back({ Severity::Good, "KEY & PROGRESSION",
        "Detected " + f.key + " - try " + juce::String(move.romanNumerals),
        move.feel + juce::String(" Roots: ") + chordNotes + ".",
        "For extra lift, borrow Dorian mode (raise the 6th to " + noteName(dorianSixthSemi) +
        ") on a lead over the same progression." });
}

void TheoryAdvisor::suggestSection(const AdvisorContext& ctx, AdvisorResult& out) const
{
    const auto& g = MelodicTechnoTheory::kSectionGuidance[(int) ctx.section];
    out.informational.push_back({ Severity::Good, "SECTION",
        juce::String(g.label) + " - what should be happening here",
        juce::String(g.note),
        {} });
}

void TheoryAdvisor::suggestGroove(const AdvisorContext& ctx, AdvisorResult& out) const
{
    const auto& f = ctx.features;
    juce::String bpmNote = f.bpm > 0.0f
        ? "Session is running at " + juce::String(f.bpm, 1) + " BPM. "
        : juce::String();

    juce::Random rng;
    const char* idiomNote = MelodicTechnoTheory::kDrumIdiomNotes[
        rng.nextInt(MelodicTechnoTheory::kNumDrumIdiomNotes)];

    out.informational.push_back({ Severity::Good, "GROOVE",
        "Swing & drum idiom",
        bpmNote + juce::String(MelodicTechnoTheory::kSwingGuidance),
        juce::String(idiomNote) });
}

void TheoryAdvisor::checkTrackClashes(const AdvisorContext& ctx, AdvisorResult& out) const
{
    constexpr int kMaxReports = 3;
    int reported = 0;

    for (size_t a = 0; a < ctx.tracks.size() && reported < kMaxReports; ++a)
    {
        for (size_t b = a + 1; b < ctx.tracks.size() && reported < kMaxReports; ++b)
        {
            const auto& ta = ctx.tracks[a];
            const auto& tb = ctx.tracks[b];

            for (int step = 0; step < (int) ta.offsets.size() && reported < kMaxReports; ++step)
            {
                if (ta.offsets[(size_t) step] == kNoNote || tb.offsets[(size_t) step] == kNoNote)
                    continue;

                int interval = ((ta.offsets[(size_t) step] - tb.offsets[(size_t) step]) % 12 + 12) % 12;
                if (interval != 1 && interval != 6 && interval != 11)
                    continue; // only flag minor 2nd / tritone (either direction)

                const int8_t target = nearestConsonantTarget(ta.offsets[(size_t) step], tb.offsets[(size_t) step]);

                out.informational.push_back({ Severity::Warning, "TRACK CLASH",
                    ta.label + " and " + tb.label + " clash at step " + juce::String(step),
                    ta.label + " plays " + noteName(ctx.keyRootSemitone + ta.offsets[(size_t) step]) +
                    " while " + tb.label + " plays " + noteName(ctx.keyRootSemitone + tb.offsets[(size_t) step]) +
                    " at the same step - a dissonant interval.",
                    "Approve below to move " + tb.label + " to " +
                    noteName(ctx.keyRootSemitone + target) + " instead." });

                out.clashFixes.push_back({ tb.trackIndex, tb.label, step, tb.offsets[(size_t) step], target,
                    "Resolves a clash with " + ta.label + " at step " + juce::String(step) + "." });

                ++reported;
            }
        }
    }
}

void TheoryAdvisor::suggestPresets(const AdvisorContext& ctx, AdvisorResult& out) const
{
    for (auto& track : ctx.tracks)
    {
        if (!isMelodicCategory(track.category))
            continue;

        if (const auto* preset = findPreset(ctx.presets, track.category))
        {
            out.informational.push_back(idea("PRESET IDEA",
                track.label + " - try \"" + preset->name + "\" (" + preset->pluginName + ")",
                "A real preset from your " + preset->pluginName + " library matches this track's category.",
                "Open " + preset->pluginName + "'s own browser on the " + track.label +
                " track and load \"" + preset->name + "\"."));
        }
    }
}

void TheoryAdvisor::suggestSamples(const AdvisorContext& ctx, AdvisorResult& out) const
{
    const auto& f      = ctx.features;
    const auto& racks  = ctx.racks;

    const auto* profile = GenreProfiles::getInstance().getProfile(ctx.profileId);
    const float subTarget = profile ? profile->subPctTarget     : 0.20f;
    const float airTarget = profile ? profile->airPctTarget     : 0.07f;
    const float tol       = profile ? profile->spectralTolerance : 0.07f;

    juce::Random rng;

    // Sub-bass: only fall back to a sample suggestion if there's no melodic
    // Bass track already getting a real Serum 2/Diva preset suggestion above
    // — a sample one-shot isn't a substitute for a synth preset.
    const bool bassHandledByPreset = findPreset(ctx.presets, MelodyCategory::Bass) != nullptr
        && std::any_of(ctx.tracks.begin(), ctx.tracks.end(),
                        [](const MelodyTrackContext& t) { return t.category == MelodyCategory::Bass; });

    // Thin low end / air is the *correct* choice outside Build/Drop - an
    // Intro or Breakdown isn't wrong for being sparse, so don't flag it.
    using MelodicTechnoTheory::SongSection;
    const bool fullMixExpected = ctx.section == SongSection::Build || ctx.section == SongSection::Drop;

    if (fullMixExpected && f.subPct < subTarget - tol && !bassHandledByPreset)
    {
        if (const auto* rack = findRack(racks, "BASS"))
        {
            auto& pick = rack->samples[(size_t) rng.nextInt((int) rack->samples.size())];
            out.informational.push_back(idea("SAMPLE IDEA",
                "Sub feels thin - try " + pick.name,
                "Detected sub energy is below what melodic techno typically carries.",
                "Approve below to assign \"" + pick.name + "\" to your Bass row, or layer it in manually - keep it mono below 80 Hz."));
            out.sampleAssigns.push_back({ rack->id, pick.file });
        }
        else
        {
            out.informational.push_back(idea("SAMPLE IDEA",
                "Sub feels thin - no bass samples found in your library",
                "Detected sub energy is below what melodic techno typically carries, "
                "and no samples are tagged as Bass in your current library.",
                "Add some sub/bass one-shots to your sample library, or lean on Serum 2's own sub oscillator."));
        }
    }

    if (fullMixExpected && f.airPct < airTarget - tol)
    {
        const auto* rack = findRack(racks, "HIHAT");
        if (rack == nullptr)
            rack = findRack(racks, "CYMBAL");

        if (rack != nullptr)
        {
            auto& pick = rack->samples[(size_t) rng.nextInt((int) rack->samples.size())];
            out.informational.push_back(idea("SAMPLE IDEA",
                "High end lacks air - try " + pick.name,
                "Detected high-frequency content (8kHz+) is below target.",
                "Approve below to assign \"" + pick.name + "\" to that row for extra top-end presence."));
            out.sampleAssigns.push_back({ rack->id, pick.file });
        }
        else
        {
            out.informational.push_back(idea("SAMPLE IDEA",
                "High end lacks air - no hat/cymbal samples found",
                "Detected high-frequency content (8kHz+) is below target, and your "
                "library has no samples tagged Hi-Hat or Cymbal.",
                "Add some hi-hat or cymbal one-shots to your sample library."));
        }
    }
}

void TheoryAdvisor::suggestTechnique(const AdvisorContext& ctx, AdvisorResult& out) const
{
    const unsigned bit = MelodicTechnoTheory::sectionBit(ctx.section);

    std::vector<const char*> matching;
    for (int i = 0; i < MelodicTechnoTheory::kNumHiddenTechniques; ++i)
    {
        const auto& t = MelodicTechnoTheory::kHiddenTechniques[i];
        if ((t.sectionMask & bit) != 0)
            matching.push_back(t.text);
    }
    if (matching.empty()) // shouldn't happen given current tags, but never go silent
        for (int i = 0; i < MelodicTechnoTheory::kNumHiddenTechniques; ++i)
            matching.push_back(MelodicTechnoTheory::kHiddenTechniques[i].text);

    juce::Random rng;
    const char* tip = matching[(size_t) rng.nextInt((int) matching.size())];

    out.informational.push_back({ Severity::Good, "TECHNIQUE", "Something to try", juce::String(tip), {} });
}
