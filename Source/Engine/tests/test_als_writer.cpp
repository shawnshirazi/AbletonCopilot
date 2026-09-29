// Regression tests for Engine::buildAlsXml/writeAlsFile (Ableton Live Set
// export) and Engine::gzipStored. The full structural validation against
// real Live-saved sets lives in Tools/als_template/validate_als.py; these
// checks keep the core invariants inside the plain-C++ test suite.
#include "../AlsWriter.h"
#include "../AlsTemplate.h"
#include "../Gzip.h"
#include "TestSupport.h"
#include <cstring>
#include <map>
#include <regex>
#include <set>
#include <string>

using namespace Engine;

namespace
{
    int countOf(const std::string& hay, const std::string& needle)
    {
        int n = 0;
        for (size_t p = hay.find(needle); p != std::string::npos; p = hay.find(needle, p + needle.size()))
            ++n;
        return n;
    }
}

int main()
{
    // --- gzip ---
    {
        const char* check = "123456789";
        CHECK(crc32(reinterpret_cast<const uint8_t*>(check), 9) == 0xCBF43926u); // standard CRC-32 check value

        const std::string payload(150000, 'x');
        const std::vector<uint8_t> gz = gzipStored(payload);
        CHECK(gz.size() > 18);
        CHECK(gz[0] == 0x1f && gz[1] == 0x8b && gz[2] == 8);
        // 3 stored blocks (65535 + 65535 + 18930): 5-byte headers each.
        CHECK(gz.size() == 10 + payload.size() + 3 * 5 + 8);
        const size_t n = gz.size();
        const uint32_t isize = gz[n - 4] | (gz[n - 3] << 8) | (gz[n - 2] << 16) | ((uint32_t) gz[n - 1] << 24);
        CHECK(isize == payload.size());
        CHECK(gzipStored("").size() == 10 + 5 + 8);
    }

    SongParams p;
    p.seed = 11;
    p.bpm  = 126.0;
    const Song song = generateSong(p);
    const std::string xml = buildAlsXml(song);

    // --- template filled completely ---
    CHECK(xml.rfind("<?xml version=\"1.0\" encoding=\"UTF-8\"?>", 0) == 0);
    CHECK(xml.find("MinorVersion=\"11.0_11300\"") != std::string::npos); // Live 11.3 schema - opens in Live 11 and 12
    CHECK(xml.find('@') == std::string::npos);                          // no unfilled placeholder
    CHECK(xml.find("MxDevice") == std::string::npos);
    CHECK(countOf(xml, "<MidiTrack Id=") == (int) song.tracks.size());
    CHECK(countOf(xml, "</Ableton>") == 1);

    // --- track names / colours ---
    for (const SongTrack& t : song.tracks)
    {
        CHECK(xml.find("<EffectiveName Value=\"" + t.name + "\" />") != std::string::npos);
        CHECK(xml.find("<UserName Value=\"" + t.name + "\" />") != std::string::npos);
    }

    // --- tempo in both places ---
    CHECK(xml.find("<Manual Value=\"126\" />") != std::string::npos);
    CHECK(xml.find("Time=\"-63072000\" Value=\"126\"") != std::string::npos);

    // --- clips / notes / locators ---
    int clips = 0, notes = 0;
    for (const SongTrack& t : song.tracks)
    {
        clips += (int) t.clips.size();
        notes += t.noteCount();
    }
    CHECK(countOf(xml, "<MidiClip Id=") == clips);
    CHECK(countOf(xml, "<MidiNoteEvent ") == notes);
    CHECK(countOf(xml, "<Locator Id=") == (int) song.sections.size());
    CHECK(xml.find("<Name Value=\"Drop 2\" />") != std::string::npos);

    // --- global pointee pool: unique ids, NextPointeeId == max + 1 ---
    {
        const std::regex poolRe("<(\\w*Target|Pointee|ControllerTargets\\.\\d+) Id=\"(\\d+)\"");
        std::set<long> ids;
        long maxId = 0;
        int count = 0;
        for (auto it = std::sregex_iterator(xml.begin(), xml.end(), poolRe); it != std::sregex_iterator(); ++it)
        {
            const long id = std::stol((*it)[2].str());
            ids.insert(id);
            maxId = std::max(maxId, id);
            ++count;
        }
        CHECK(count == (int) ids.size());
        CHECK(count > (int) song.tracks.size() * AlsTemplate::kPoolIdsPerTrack);
        std::smatch m;
        CHECK(std::regex_search(xml, m, std::regex("<NextPointeeId Value=\"(\\d+)\" />")));
        CHECK(std::stol(m[1].str()) == maxId + 1);
    }

    // --- escaping ---
    {
        Song odd = song;
        odd.tracks[0].name = "Kick & <Sub> \"1\"";
        odd.sections[0].name = "A&B";
        const std::string x = buildAlsXml(odd);
        CHECK(x.find("Kick &amp; &lt;Sub&gt; &quot;1&quot;") != std::string::npos);
        CHECK(x.find("Kick & <Sub>") == std::string::npos);
        CHECK(x.find("<Name Value=\"A&amp;B\" />") != std::string::npos);
    }

    // --- file bytes are the gzip of the XML ---
    {
        const std::vector<uint8_t> file = writeAlsFile(song);
        CHECK(file.size() > xml.size());
        CHECK(file[0] == 0x1f && file[1] == 0x8b);
        CHECK(std::memcmp(file.data() + 15, xml.data(), 64) == 0); // first stored block starts after 10+5 header bytes
    }

    TEST_SUMMARY_AND_EXIT();
}
