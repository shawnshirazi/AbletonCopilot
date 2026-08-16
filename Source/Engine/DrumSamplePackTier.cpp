#include "DrumSamplePackTier.h"
#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace Engine
{
    namespace
    {
        // Whole-word tokens only, same philosophy as RackClassification::
        // tokenize/classifyByTokens (a "contains" check on the raw string
        // would match "hh" inside "Ahh" or "tech" inside "biotechno") -
        // but broader than RackClassification's tokenizer: this splits on
        // EVERY non-alphanumeric character (path separators, parens,
        // '+', '&', ...), not just '_'/'-'/'.', because pack folder names
        // in this library use all of those inconsistently (compare
        // "PML - Melodic Techno - Sound Pack - Mirage (PML341)" against
        // "Voltage+Vol.2+for+Serum+2+").
        std::unordered_set<std::string> tokenize(const std::string& path)
        {
            std::unordered_set<std::string> tokens;
            std::string current;
            auto flush = [&]()
            {
                if (!current.empty())
                {
                    tokens.insert(current);
                    current.clear();
                }
            };
            for (char c : path)
            {
                if (std::isalnum(static_cast<unsigned char>(c)))
                    current += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                else
                    flush();
            }
            flush();
            return tokens;
        }

        bool has(const std::unordered_set<std::string>& tokens, const char* a)
        {
            return tokens.count(a) != 0;
        }

        bool has(const std::unordered_set<std::string>& tokens, const char* a, const char* b)
        {
            return has(tokens, a) && has(tokens, b);
        }

        bool has(const std::unordered_set<std::string>& tokens, const char* a, const char* b, const char* c)
        {
            return has(tokens, a) && has(tokens, b) && has(tokens, c);
        }
    }

    PackClassification classifyPackTier(const std::string& absolutePath)
    {
        const auto t = tokenize(absolutePath);

        // ---- Tier 4: explicit off-genre fallback, checked first as a
        // conservative safety net (see header comment - in practice no
        // real Tier-1 pack in this library also contains "tech"+"house"
        // as separate tokens, so this ordering never shadows a real
        // Melodic Techno match). ----
        if (has(t, "tech", "house") || has(t, "techhouse"))
            return { PackTier::OffGenreFallback, "Tech House" };
        if (has(t, "speed", "vocal") && has(t, "house"))
            return { PackTier::OffGenreFallback, "Speed Vocal Bass House" };
        if (has(t, "future", "rave"))
            return { PackTier::OffGenreFallback, "Future Rave" };
        if (has(t, "trance"))
            return { PackTier::OffGenreFallback, "Trance" };
        if (has(t, "alkemist"))
            return { PackTier::OffGenreFallback, "Kick & Bass - Alkemist" };
        if (has(t, "image", "line"))
            return { PackTier::OffGenreFallback, "Image-Line factory content" };
        if (has(t, "heatwave"))
            return { PackTier::OffGenreFallback, "THM Heatwave Tech House Vocals" };

        // ---- Tier 1: explicit Melodic Techno packs ----
        if (has(t, "pml", "mirage"))
            return { PackTier::MelodicTechno, "PML Mirage (PML341)" };
        if (has(t, "pml", "mystique"))
            return { PackTier::MelodicTechno, "PML Mystique (PML354)" };
        if (has(t, "odd", "frequency") && has(t, "melodic"))
            return { PackTier::MelodicTechno, "Odd Frequency Exo/Exo2" };
        if (has(t, "ekko", "mirage"))
            return { PackTier::MelodicTechno, "Ekko Mirage Melodic House & Techno" };
        if (has(t, "cercle", "diva") && has(t, "melodic"))
            return { PackTier::MelodicTechno, "PML Cercle Diva Melodic Techno" };
        if (has(t, "arrangement", "academy"))
            return { PackTier::MelodicTechno, "PML Complete Arrangement Academy (PML361)" };
        if (has(t, "music", "theory"))
            return { PackTier::MelodicTechno, "PML Music Theory for Melodic House & Techno (PML285)" };
        if (has(t, "voltage"))
            return { PackTier::MelodicTechno, "Voltage Vol.2 for Serum 2" };

        // ---- Tier 2: adjacent Techno / Progressive Techno ----
        if (has(t, "odd", "frequency") && has(t, "grid"))
            return { PackTier::AdjacentTechno, "Odd Frequency Grid" };
        if (has(t, "afterhours"))
            return { PackTier::AdjacentTechno, "Afterhours - Progressive & Tech" };
        if (has(t, "progressive", "techno"))
            return { PackTier::AdjacentTechno, "Progressive Techno - Sample Tools by Cr2" };
        if (has(t, "tops", "atmo"))
            return { PackTier::AdjacentTechno, "PML Tops & Atmo Loops" };
        if (has(t, "cercle", "diva") && has(t, "bonus"))
            return { PackTier::AdjacentTechno, "Diva Cercle Sounds - Bonus Loops" };

        // ---- Tier 3 explicit (driving/dark techno) - same neutral
        // weight as the unmatched default, kept as a distinct branch
        // only so real diagnostics (Part D) can report a real pack name
        // instead of "unknown". ----
        if (has(t, "dark", "techno"))
            return { PackTier::OtherElectronic, "PML Dark Techno" };
        if (has(t, "toolroom"))
            return { PackTier::OtherElectronic, "Toolroom Essential Techno" };
        if (has(t, "acid", "techno"))
            return { PackTier::OtherElectronic, "Acid Techno" };
        // "Electro x Tech" is a real, previously-unclassified pack found
        // during this session's library audit - generic electro/tech
        // techno, not confirmed Melodic Techno-branded and not confirmed
        // off-genre (Tech House etc.) either, so neutral Tier 3 rather
        // than a guess in either direction (same treatment as the other
        // explicit Tier 3 entries above).
        if (has(t, "electro") && has(t, "tech"))
            return { PackTier::OtherElectronic, "Electro x Tech + Warehouse Vocals" };

        // Unmatched/unreviewed pack - neutral default, not a penalty.
        return { PackTier::OtherElectronic, "" };
    }
}
