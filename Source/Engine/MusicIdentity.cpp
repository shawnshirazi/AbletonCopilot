#include "MusicIdentity.h"
#include "Grid.h"
#include <algorithm>

namespace Engine
{
    namespace
    {
        // Breakdown mix defaults - see MusicIdentity.h's own header
        // comment and the plan this was built from: kick/hatOpen/percB
        // fully removed (a real "state/mix change," never a pattern
        // rewrite), hatClosed/percA reduced but not silent (a genuine
        // mix-level gain cut, not a thinned grid), clap and melody left
        // alone, bass voice muted by default (no normal bass groove).
        // These are a judgment call, not a researched fact - flagged as
        // such for the user to react to by ear and adjust.
        constexpr float kBreakdownReducedGain = 0.4f;
    }

    MusicIdentity generateMusicIdentity(const MusicIdentityParams& params)
    {
        MusicIdentity id;
        id.seed     = params.seed;
        id.bpm      = params.bpm;
        id.rootNote = params.rootNote;
        id.isMinor  = params.isMinor;

        // Drums: Engine::generateDrop() is already a native 16-bar/256-step
        // generator - StepGridConfig{} defaults to stepsPerBar=16,
        // numBars=16 (see Grid.h), i.e. exactly kLoopTotalSteps. No tiling
        // needed, unlike bass below.
        StepGridConfig grid;
        DrumPatternParams drumParams;
        drumParams.density     = 0.5f;
        drumParams.syncopation = 0.3f;
        drumParams.variation   = 0.2f;
        drumParams.seed        = params.seed;
        id.drumMotif = generateDrop(grid, drumParams, DrumSection::Drop);

        // Bass: generateBassLoop16 - a native 16-bar generator that is
        // REALLY aware of this identity's own drumMotif (real kick/
        // hatClosed/percA occupancy, not a synthetic four-on-the-floor
        // stand-in) and develops across all 4 blocks instead of looping
        // the first 8 bars unchanged - see BassEngine.h's own comment on
        // generateBassLoop16 for the evidence behind both differences from
        // the plain generateBassPattern() this replaced. Reuses the SAME
        // seed (not a derived one) so this identity's bass and drum
        // motifs are both fully determined by params.seed alone, per "for
        // seed X, Generate -> same loop every time."
        BassPatternParams bassParams;
        bassParams.density   = 0.5f;
        bassParams.variation = 0.2f;
        bassParams.seed      = params.seed;
        const BassLoop16 bassLoop = generateBassLoop16(id.drumMotif, bassParams);
        id.bassMotif           = bassLoop.pitchOffsets;
        id.bassGateLengthSteps = bassLoop.gateLengthSteps;

        return id;
    }

    namespace
    {
        // Applies a role's RoleMix by construction - drum StepArray data
        // itself is never touched; only the RoleMix returned alongside it
        // differs. This helper just keeps renderMode's body from repeating
        // the same four-field struct-init six times.
        RoleMix dropMix()      { return RoleMix { false, 1.0f }; }
        RoleMix removedMix()   { return RoleMix { true,  1.0f }; }
        RoleMix reducedMix()   { return RoleMix { false, kBreakdownReducedGain }; }
    }

    RenderedLoop renderMode(const MusicIdentity& identity, RenderMode mode)
    {
        RenderedLoop out;
        out.drum = identity.drumMotif; // always byte-identical to the identity - see header comment
        out.bass = identity.bassMotif; // always byte-identical to the identity - see header comment
        out.bassGateLengthSteps = identity.bassGateLengthSteps; // ditto

        if (mode == RenderMode::Drop)
        {
            out.kick = out.clap = out.hatClosed = out.hatOpen = out.percA = out.percB = dropMix();
            out.bassMuted = false;
        }
        else // Breakdown
        {
            out.kick      = removedMix();
            out.hatOpen   = removedMix();
            out.percB     = removedMix();
            out.hatClosed = reducedMix();
            out.percA     = reducedMix();
            out.clap      = dropMix(); // left alone - a judgment call, see header comment
            out.bassMuted = true;
        }

        return out;
    }
}
