#include "DrumSampleSelection.h"
#include <random>

namespace Engine
{
    namespace
    {
        // Distinct from DrumEngine.cpp's pattern-generation salts
        // (kKickSalt etc.) - a deliberately different constant set so
        // sample selection and pattern generation never draw the same
        // pseudo-random sequence for the same nominal seed value, even
        // though in practice the caller passes a separate seed for each.
        uint32_t roleSalt(DrumRole role)
        {
            switch (role)
            {
                case DrumRole::Kick:  return 0x53414D50u ^ 0x4B49434Bu; // 'SAMP' ^ 'KICK'
                case DrumRole::Clap:  return 0x53414D50u ^ 0x434C4150u; // 'SAMP' ^ 'CLAP'
                case DrumRole::Hat:   return 0x53414D50u ^ 0x48415420u; // 'SAMP' ^ 'HAT '
                case DrumRole::Perc:  return 0x53414D50u ^ 0x50455243u; // 'SAMP' ^ 'PERC'
                case DrumRole::Count: return 0x53414D50u;
            }
            return 0x53414D50u;
        }
    }

    int selectSampleIndex(DrumRole role, uint32_t seed, int candidateCount)
    {
        if (candidateCount <= 0)
            return -1;
        if (candidateCount == 1)
            return 0;

        std::mt19937 rng(seed ^ roleSalt(role));
        std::uniform_int_distribution<int> dist(0, candidateCount - 1);
        return dist(rng);
    }
}
