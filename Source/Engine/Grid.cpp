#include "Grid.h"

namespace Engine
{
    int totalSteps(const StepGridConfig& config)
    {
        return config.stepsPerBar * config.numBars;
    }

    double stepTimeSeconds(int step, double bpm, const StepGridConfig& config)
    {
        const double secPerBar  = (60.0 / bpm) * 4.0; // 4/4 time - 4 beats/bar
        const double secPerStep = secPerBar / (double) config.stepsPerBar;
        double t = (double) step * secPerStep;

        // Swing delays the off-beat 16th of each 8th-note pair (odd step
        // indices). config.swing 0..1 maps straight (0) to full triplet
        // (1): a triplet 8th-note pair splits 2:1, so the off-beat lands at
        // 2/3 of the pair instead of 1/2 - a (1/6)-of-an-8th-note shift at
        // full swing, scaled linearly by config.swing below that.
        if ((step % 2) != 0)
        {
            const double eighthNoteSec = secPerStep * 2.0;
            t += (double) config.swing * (eighthNoteSec / 6.0);
        }

        return t;
    }
}
