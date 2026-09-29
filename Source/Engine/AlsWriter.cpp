#include "AlsWriter.h"
#include "AlsTemplate.h"
#include "Gzip.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace Engine
{
    namespace
    {
        std::string xmlEscape(const std::string& s)
        {
            std::string out;
            for (char c : s)
            {
                switch (c)
                {
                    case '&':  out += "&amp;";  break;
                    case '<':  out += "&lt;";   break;
                    case '>':  out += "&gt;";   break;
                    case '"':  out += "&quot;"; break;
                    case '\'': out += "&apos;"; break;
                    default:   out += c;        break;
                }
            }
            return out;
        }

        // Shortest round-trippable decimal ("16", "0.25", "124.5").
        std::string num(double v)
        {
            char buf[40];
            std::snprintf(buf, sizeof(buf), "%.10g", v);
            return buf;
        }

        // Replaces @NAME@ tokens. @P<k>@ becomes poolBase + k; every other
        // token is looked up in `values` (an unknown token is a bug in the
        // template/writer pairing, so it aborts loudly in debug builds and is
        // left in place otherwise).
        void fillTemplate(std::string& out, const char* tpl, const std::map<std::string, std::string>& values,
                          int poolBase)
        {
            const char* p = tpl;
            while (*p != '\0')
            {
                const char* at = std::strchr(p, '@');
                if (at == nullptr)
                {
                    out += p;
                    break;
                }
                out.append(p, (size_t) (at - p));
                const char* end = std::strchr(at + 1, '@');
                if (end == nullptr)
                {
                    out += at;
                    break;
                }
                const std::string key(at + 1, (size_t) (end - at - 1));
                if (key.size() > 1 && key[0] == 'P' && std::strspn(key.c_str() + 1, "0123456789") == key.size() - 1)
                {
                    out += std::to_string(poolBase + std::atoi(key.c_str() + 1));
                }
                else
                {
                    const auto it = values.find(key);
                    if (it != values.end())
                        out += it->second;
                    else
                        out.append(at, (size_t) (end - at + 1));
                }
                p = end + 1;
            }
        }

        void appendClip(std::string& x, const SongClip& clip, int clipListId, int color)
        {
            const double start = clip.startBar * 4.0;
            const double len   = clip.bars * 4.0;
            const std::string t = "\t\t\t\t\t\t\t\t\t";

            x += t + "<MidiClip Id=\"" + std::to_string(clipListId) + "\" Time=\"" + num(start) + "\">\n";
            x += t + "\t<LomId Value=\"0\" />\n";
            x += t + "\t<LomIdView Value=\"0\" />\n";
            x += t + "\t<CurrentStart Value=\"" + num(start) + "\" />\n";
            x += t + "\t<CurrentEnd Value=\"" + num(start + len) + "\" />\n";
            x += t + "\t<Loop>\n";
            x += t + "\t\t<LoopStart Value=\"0\" />\n";
            x += t + "\t\t<LoopEnd Value=\"" + num(len) + "\" />\n";
            x += t + "\t\t<StartRelative Value=\"0\" />\n";
            x += t + "\t\t<LoopOn Value=\"false\" />\n";
            x += t + "\t\t<OutMarker Value=\"" + num(len) + "\" />\n";
            x += t + "\t\t<HiddenLoopStart Value=\"0\" />\n";
            x += t + "\t\t<HiddenLoopEnd Value=\"" + num(len) + "\" />\n";
            x += t + "\t</Loop>\n";
            x += t + "\t<Name Value=\"" + xmlEscape(clip.name) + "\" />\n";
            x += t + "\t<Annotation Value=\"\" />\n";
            x += t + "\t<Color Value=\"" + std::to_string(color) + "\" />\n";
            x += t + "\t<LaunchMode Value=\"0\" />\n";
            x += t + "\t<LaunchQuantisation Value=\"0\" />\n";
            x += t + "\t<TimeSignature>\n";
            x += t + "\t\t<TimeSignatures>\n";
            x += t + "\t\t\t<RemoteableTimeSignature Id=\"0\">\n";
            x += t + "\t\t\t\t<Numerator Value=\"4\" />\n";
            x += t + "\t\t\t\t<Denominator Value=\"4\" />\n";
            x += t + "\t\t\t\t<Time Value=\"0\" />\n";
            x += t + "\t\t\t</RemoteableTimeSignature>\n";
            x += t + "\t\t</TimeSignatures>\n";
            x += t + "\t</TimeSignature>\n";
            x += t + "\t<Envelopes>\n";
            x += t + "\t\t<Envelopes />\n";
            x += t + "\t</Envelopes>\n";
            x += t + "\t<ScrollerTimePreserver>\n";
            x += t + "\t\t<LeftTime Value=\"0\" />\n";
            x += t + "\t\t<RightTime Value=\"" + num(len) + "\" />\n";
            x += t + "\t</ScrollerTimePreserver>\n";
            x += t + "\t<TimeSelection>\n";
            x += t + "\t\t<AnchorTime Value=\"0\" />\n";
            x += t + "\t\t<OtherTime Value=\"0\" />\n";
            x += t + "\t</TimeSelection>\n";
            x += t + "\t<Legato Value=\"false\" />\n";
            x += t + "\t<Ram Value=\"false\" />\n";
            x += t + "\t<GrooveSettings>\n";
            x += t + "\t\t<GrooveId Value=\"-1\" />\n";
            x += t + "\t</GrooveSettings>\n";
            x += t + "\t<Disabled Value=\"false\" />\n";
            x += t + "\t<VelocityAmount Value=\"0\" />\n";
            x += t + "\t<FollowAction>\n";
            x += t + "\t\t<FollowTime Value=\"4\" />\n";
            x += t + "\t\t<IsLinked Value=\"true\" />\n";
            x += t + "\t\t<LoopIterations Value=\"1\" />\n";
            x += t + "\t\t<FollowActionA Value=\"4\" />\n";
            x += t + "\t\t<FollowActionB Value=\"0\" />\n";
            x += t + "\t\t<FollowChanceA Value=\"100\" />\n";
            x += t + "\t\t<FollowChanceB Value=\"0\" />\n";
            x += t + "\t\t<JumpIndexA Value=\"1\" />\n";
            x += t + "\t\t<JumpIndexB Value=\"1\" />\n";
            x += t + "\t\t<FollowActionEnabled Value=\"false\" />\n";
            x += t + "\t</FollowAction>\n";
            x += t + "\t<Grid>\n";
            x += t + "\t\t<FixedNumerator Value=\"1\" />\n";
            x += t + "\t\t<FixedDenominator Value=\"16\" />\n";
            x += t + "\t\t<GridIntervalPixel Value=\"20\" />\n";
            x += t + "\t\t<Ntoles Value=\"2\" />\n";
            x += t + "\t\t<SnapToGrid Value=\"true\" />\n";
            x += t + "\t\t<Fixed Value=\"false\" />\n";
            x += t + "\t</Grid>\n";
            x += t + "\t<FreezeStart Value=\"0\" />\n";
            x += t + "\t<FreezeEnd Value=\"0\" />\n";
            x += t + "\t<IsWarped Value=\"true\" />\n";
            x += t + "\t<TakeId Value=\"1\" />\n";

            // Notes: one KeyTrack per pitch, ascending MidiKey, Notes before MidiKey.
            std::map<int, std::vector<const SongNote*>> byPitch;
            for (const SongNote& n : clip.notes)
                byPitch[std::max(0, std::min(127, n.pitch))].push_back(&n);

            x += t + "\t<Notes>\n";
            x += t + "\t\t<KeyTracks>\n";
            int keyTrackId = 0;
            int noteId = 1;
            for (auto& [pitch, notes] : byPitch)
            {
                std::stable_sort(notes.begin(), notes.end(),
                                 [](const SongNote* a, const SongNote* b) { return a->startBeat < b->startBeat; });
                x += t + "\t\t\t<KeyTrack Id=\"" + std::to_string(keyTrackId++) + "\">\n";
                x += t + "\t\t\t\t<Notes>\n";
                for (const SongNote* n : notes)
                {
                    const int vel = std::max(1, std::min(127, n->velocity));
                    x += t + "\t\t\t\t\t<MidiNoteEvent Time=\"" + num(n->startBeat) + "\" Duration=\"" + num(n->lengthBeats)
                         + "\" Velocity=\"" + std::to_string(vel)
                         + "\" VelocityDeviation=\"0\" OffVelocity=\"64\" Probability=\"1\" IsEnabled=\"true\" NoteId=\""
                         + std::to_string(noteId++) + "\" />\n";
                }
                x += t + "\t\t\t\t</Notes>\n";
                x += t + "\t\t\t\t<MidiKey Value=\"" + std::to_string(pitch) + "\" />\n";
                x += t + "\t\t\t</KeyTrack>\n";
            }
            x += t + "\t\t</KeyTracks>\n";
            x += t + "\t\t<PerNoteEventStore>\n";
            x += t + "\t\t\t<EventLists />\n";
            x += t + "\t\t</PerNoteEventStore>\n";
            x += t + "\t\t<NoteIdGenerator>\n";
            x += t + "\t\t\t<NextId Value=\"" + std::to_string(noteId) + "\" />\n";
            x += t + "\t\t</NoteIdGenerator>\n";
            x += t + "\t</Notes>\n";
            x += t + "\t<BankSelectCoarse Value=\"-1\" />\n";
            x += t + "\t<BankSelectFine Value=\"-1\" />\n";
            x += t + "\t<ProgramChange Value=\"-1\" />\n";
            x += t + "\t<NoteEditorFoldInZoom Value=\"-1\" />\n";
            x += t + "\t<NoteEditorFoldInScroll Value=\"0\" />\n";
            x += t + "\t<NoteEditorFoldOutZoom Value=\"-1\" />\n";
            x += t + "\t<NoteEditorFoldOutScroll Value=\"0\" />\n";
            x += t + "\t<NoteEditorFoldScaleZoom Value=\"-1\" />\n";
            x += t + "\t<NoteEditorFoldScaleScroll Value=\"0\" />\n";
            x += t + "\t<ScaleInformation>\n";
            x += t + "\t\t<RootNote Value=\"0\" />\n";
            x += t + "\t\t<Name Value=\"Major\" />\n";
            x += t + "\t</ScaleInformation>\n";
            x += t + "\t<IsInKey Value=\"false\" />\n";
            x += t + "\t<NoteSpellingPreference Value=\"3\" />\n";
            x += t + "\t<PreferFlatRootNote Value=\"false\" />\n";
            x += t + "\t<ExpressionGrid>\n";
            x += t + "\t\t<FixedNumerator Value=\"1\" />\n";
            x += t + "\t\t<FixedDenominator Value=\"16\" />\n";
            x += t + "\t\t<GridIntervalPixel Value=\"20\" />\n";
            x += t + "\t\t<Ntoles Value=\"2\" />\n";
            x += t + "\t\t<SnapToGrid Value=\"false\" />\n";
            x += t + "\t\t<Fixed Value=\"false\" />\n";
            x += t + "\t</ExpressionGrid>\n";
            x += t + "</MidiClip>\n";
        }
    }

    std::string buildAlsXml(const Song& song)
    {
        const int numTracks = (int) song.tracks.size();
        const int nextPointeeId = AlsTemplate::kFirstTrackPoolId + numTracks * AlsTemplate::kPoolIdsPerTrack;

        std::string x;
        x.reserve(2u << 20);
        fillTemplate(x, AlsTemplate::kAlsHead, { { "NEXT_POINTEE_ID", std::to_string(nextPointeeId) } }, 0);

        for (int i = 0; i < numTracks; ++i)
        {
            const SongTrack& track = song.tracks[(size_t) i];
            const int color = std::max(0, std::min(69, track.colorIndex));

            std::string clips;
            if (track.clips.empty())
                clips = "<Events />";
            else
            {
                clips = "<Events>\n";
                int clipId = 0;
                for (const SongClip& c : track.clips)
                    appendClip(clips, c, clipId++, color);
                clips += "\t\t\t\t\t\t\t\t</Events>";
            }

            fillTemplate(x, AlsTemplate::kAlsMidiTrack,
                         { { "TRACK_ID", std::to_string(12 + i) },
                           { "NAME", xmlEscape(track.name) },
                           { "COLOR", std::to_string(color) },
                           { "ARRANGEMENT_CLIPS", clips } },
                         AlsTemplate::kFirstTrackPoolId + i * AlsTemplate::kPoolIdsPerTrack);
        }

        std::string locators;
        if (!song.sections.empty())
        {
            locators += "\n";
            int id = 0;
            for (const SongSection& s : song.sections)
            {
                locators += "\t\t\t\t<Locator Id=\"" + std::to_string(id++) + "\">\n";
                locators += "\t\t\t\t\t<LomId Value=\"0\" />\n";
                locators += "\t\t\t\t\t<Time Value=\"" + num(s.startBar * 4.0) + "\" />\n";
                locators += "\t\t\t\t\t<Name Value=\"" + xmlEscape(s.name) + "\" />\n";
                locators += "\t\t\t\t\t<Annotation Value=\"\" />\n";
                locators += "\t\t\t\t\t<IsSongStart Value=\"false\" />\n";
                locators += "\t\t\t\t</Locator>\n";
            }
            locators += "\t\t\t";
        }

        fillTemplate(x, AlsTemplate::kAlsTail,
                     { { "TEMPO", num(std::max(60.0, std::min(200.0, song.bpm))) }, { "LOCATORS", locators } }, 0);
        return x;
    }

    std::vector<uint8_t> writeAlsFile(const Song& song)
    {
        return gzipStored(buildAlsXml(song));
    }

    bool writeAlsFileToPath(const Song& song, const std::string& path)
    {
        const std::vector<uint8_t> bytes = writeAlsFile(song);
        FILE* f = std::fopen(path.c_str(), "wb");
        if (f == nullptr)
            return false;
        const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
        return std::fclose(f) == 0 && ok;
    }
}
