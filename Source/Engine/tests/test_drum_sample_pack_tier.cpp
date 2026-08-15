// Regression test for Engine::classifyPackTier (Part C of the bass-
// runtime-bug + sample-selection investigation). Every path below is a
// REAL directory name confirmed present under
// /Users/shawnshirazi/shawn music stuff (see
// MLPipeline/musical_target/melodic_techno_research.md section 9) - not
// synthetic examples, so this test proves the classifier actually
// recognizes this user's real library, not just an idealized case.
#include "../DrumSamplePackTier.h"
#include "TestSupport.h"

using namespace Engine;

int main()
{
    // ---- Tier 1: explicit Melodic Techno ----
    CHECK(classifyPackTier("/lib/FL studio/Techno/PML - Melodic Techno - Sound Pack - Mirage (PML341)/One Shots/Kick/Kick 1.wav").tier == PackTier::MelodicTechno);
    CHECK(classifyPackTier("/lib/FL studio/Techno/PML - Melodic Techno - Sound Pack - Mystique (PML354)/Percussion/Perc 03.wav").tier == PackTier::MelodicTechno);
    CHECK(classifyPackTier("/lib/FL studio/Techno/Odd Frequency - Modern Melodic Techno Mega Bundle/Odd Frequency - Exo - Full Bundle/One Shots/Hats/Closed 1.wav").tier == PackTier::MelodicTechno);
    CHECK(classifyPackTier("/lib/FL studio/Techno/Odd Frequency - Modern Melodic Techno Mega Bundle/Odd Frequency - Exo 2 - Full Bundle/Kicks/K2.wav").tier == PackTier::MelodicTechno);
    CHECK(classifyPackTier("/lib/FL studio/Techno/Ekko - Mirage Melodic House & Techno/Claps/Clap 4.wav").tier == PackTier::MelodicTechno);
    CHECK(classifyPackTier("/lib/PML Cercle - Diva Melodic Techno Presets + MIDI V1.1/Presets/Bass 1.fxp").tier == PackTier::MelodicTechno);
    CHECK(classifyPackTier("/lib/FL studio/Techno/PML - Complete Arrangement Academy (PML361)/Project/kick.wav").tier == PackTier::MelodicTechno);
    CHECK(classifyPackTier("/lib/PML_-_Music_Theory_for_Melodic_House_&_Techno_(PML285)/MIDI/chords.mid").tier == PackTier::MelodicTechno);
    CHECK(classifyPackTier("/lib/FL studio/Voltage+Vol.2+for+Serum+2+/BS - Deep Sub.fxp").tier == PackTier::MelodicTechno);
    CHECK(classifyPackTier("/lib/FL studio/Techno/Voltage Vol.2 for Serum 2/BS - Growl.fxp").tier == PackTier::MelodicTechno);

    // ---- Tier 2: adjacent Techno / Progressive Techno ----
    CHECK(classifyPackTier("/lib/FL studio/Techno/Odd Frequency - Grid Mega Bundle/Odd Frequency - Grid - Full Bundle/Hats/Open 2.wav").tier == PackTier::AdjacentTechno);
    CHECK(classifyPackTier("/lib/FL studio/Techno/Odd Frequency - Grid Mega Bundle/Odd Frequency - Grid 2 - Full Bundle/Perc/P1.wav").tier == PackTier::AdjacentTechno);
    CHECK(classifyPackTier("/lib/FL studio/Techno/Afterhours - Progressive & Tech/Kicks/K1.wav").tier == PackTier::AdjacentTechno);
    CHECK(classifyPackTier("/lib/FL studio/Techno/Progressive Techno - Sample Tools by Cr2/One Shots/Clap 2.wav").tier == PackTier::AdjacentTechno);
    CHECK(classifyPackTier("/lib/FL studio/Techno/PML Tops & Atmo Loops Pack V1/Tops/T1.wav").tier == PackTier::AdjacentTechno);
    CHECK(classifyPackTier("/lib/FL studio/Techno/Diva Cercle Sounds - Bonus Loops/Loop 1.wav").tier == PackTier::AdjacentTechno);

    // ---- Tier 3: unmatched/unreviewed packs default to the neutral tier, not a penalty ----
    CHECK(classifyPackTier("/lib/FL studio/Techno/Roland-TR-909/Kick.wav").tier == PackTier::OtherElectronic);
    CHECK(classifyPackTier("/lib/FL studio/TPS - Selection/Perc/P9.wav").tier == PackTier::OtherElectronic);
    CHECK(classifyPackTier("/lib/some/never/inventoried/pack/kick.wav").tier == PackTier::OtherElectronic);
    // Explicitly-named driving/dark techno - same neutral weight as the
    // default, but should still report a real name for diagnostics.
    {
        auto c = classifyPackTier("/lib/FL studio/Techno/PML - Dark Techno Sample Pack 2/Kicks/K1.wav");
        CHECK(c.tier == PackTier::OtherElectronic);
        CHECK(c.packName == "PML Dark Techno");
    }
    CHECK(classifyPackTier("/lib/FL studio/Techno/Toolroom_-_Essential_Techno_Vol._4/Hats/H1.wav").tier == PackTier::OtherElectronic);
    CHECK(classifyPackTier("/lib/FL studio/Techno/Acid Techno/Perc/P1.wav").tier == PackTier::OtherElectronic);

    // ---- Tier 4: explicit off-genre fallback ----
    CHECK(classifyPackTier("/lib/FL studio/Techno/Odd Frequency - Tech House Mega Bundle/Kicks/K1.wav").tier == PackTier::OffGenreFallback);
    CHECK(classifyPackTier("/lib/FL studio/Techno/PML_-_Complete_Tech_House_Start_to_Finish_Academy_(PML338)_v1.1/Claps/C1.wav").tier == PackTier::OffGenreFallback);
    CHECK(classifyPackTier("/lib/FL studio/Techno/Secrets-Tech-House-Vol.-2/Perc/P1.wav").tier == PackTier::OffGenreFallback);
    CHECK(classifyPackTier("/lib/FL studio/Techno/Ekko-Secrets-Tech-House-kkuqfq/Hats/H1.wav").tier == PackTier::OffGenreFallback);
    CHECK(classifyPackTier("/lib/FL studio/Techno/THM - Heatwave - Tech House Vocals/Vox/V1.wav").tier == PackTier::OffGenreFallback);
    CHECK(classifyPackTier("/lib/FL studio/Techno/TechHouse/Kick/K1.wav").tier == PackTier::OffGenreFallback);
    CHECK(classifyPackTier("/lib/FL studio/Techno/Speed Vocal Bass House/Bass/B1.wav").tier == PackTier::OffGenreFallback);
    CHECK(classifyPackTier("/lib/FL studio/Future Rave/Kicks/K1.wav").tier == PackTier::OffGenreFallback);
    CHECK(classifyPackTier("/lib/FL studio/Trance/Leads/L1.wav").tier == PackTier::OffGenreFallback);
    CHECK(classifyPackTier("/lib/FL studio/Techno/KICK & BASS - ALKEMIST VOL.1 -3/Kicks/K1.wav").tier == PackTier::OffGenreFallback);
    CHECK(classifyPackTier("/lib/FL studio/Image-Line/Data/Patches/kick.wav").tier == PackTier::OffGenreFallback);

    // ---- Real conflict guards: packs sharing a common word with an
    // off-genre or different-tier pack must not cross-classify. ----
    // "Ekko - Mirage..." (Tier1) vs "Ekko-Secrets-Tech-House..." (Tier4) -
    // both start with "ekko" but must land in different tiers.
    CHECK(classifyPackTier("/lib/FL studio/Techno/Ekko - Mirage Melodic House & Techno/Kicks/K1.wav").tier == PackTier::MelodicTechno);
    CHECK(classifyPackTier("/lib/FL studio/Techno/Ekko-Secrets-Tech-House-kkuqfq/Kicks/K1.wav").tier == PackTier::OffGenreFallback);
    // "PML - Complete Arrangement Academy" (Tier1) vs "PML - Complete Tech
    // House ... Academy" (Tier4) - both contain "academy" and "pml".
    CHECK(classifyPackTier("/lib/FL studio/Techno/PML - Complete Arrangement Academy (PML361)/x.wav").tier == PackTier::MelodicTechno);
    CHECK(classifyPackTier("/lib/FL studio/Techno/PML_-_Complete_Tech_House_Start_to_Finish_Academy_(PML338)_v1.1/x.wav").tier == PackTier::OffGenreFallback);
    // "Diva Cercle Sounds - Bonus Loops" (Tier2) vs "PML Cercle - Diva
    // Melodic Techno..." (Tier1) - both contain "cercle" and "diva".
    CHECK(classifyPackTier("/lib/FL studio/Diva Cercle Sounds - Bonus Loops/x.wav").tier == PackTier::AdjacentTechno);
    CHECK(classifyPackTier("/lib/PML Cercle - Diva Melodic Techno Presets + MIDI V1.1/x.wav").tier == PackTier::MelodicTechno);
    // "Odd Frequency - Grid..." (Tier2) vs "Odd Frequency - Modern
    // Melodic Techno..." (Tier1) vs "Odd Frequency - Tech House..."
    // (Tier4) - all three share "odd"+"frequency".
    CHECK(classifyPackTier("/lib/FL studio/Techno/Odd Frequency - Grid Mega Bundle/x.wav").tier == PackTier::AdjacentTechno);
    CHECK(classifyPackTier("/lib/FL studio/Techno/Odd Frequency - Modern Melodic Techno Mega Bundle/x.wav").tier == PackTier::MelodicTechno);
    CHECK(classifyPackTier("/lib/FL studio/Techno/Odd Frequency - Tech House Mega Bundle/x.wav").tier == PackTier::OffGenreFallback);

    // ---- Weight ordering must match the user's stated priority. ----
    CHECK(tierWeight(PackTier::MelodicTechno) > tierWeight(PackTier::AdjacentTechno));
    CHECK(tierWeight(PackTier::AdjacentTechno) > tierWeight(PackTier::OtherElectronic));
    CHECK(tierWeight(PackTier::OtherElectronic) > tierWeight(PackTier::OffGenreFallback));

    TEST_SUMMARY_AND_EXIT();
}
