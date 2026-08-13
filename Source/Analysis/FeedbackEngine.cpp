#include "FeedbackEngine.h"
#include <cmath>

//==============================================================================
// Helpers
//==============================================================================

FeedbackItem FeedbackEngine::good(const juce::String& cat, const juce::String& headline)
{
    return { Severity::Good, cat, headline, {}, {} };
}

FeedbackItem FeedbackEngine::warn(const juce::String& cat, const juce::String& headline,
                                   const juce::String& detail, const juce::String& fix)
{
    return { Severity::Warning, cat, headline, detail, fix };
}

FeedbackItem FeedbackEngine::critical(const juce::String& cat, const juce::String& headline,
                                       const juce::String& detail, const juce::String& fix)
{
    return { Severity::Critical, cat, headline, detail, fix };
}

juce::String FeedbackEngine::db(float v)
{
    return juce::String(v, 1) + " dB";
}

juce::String FeedbackEngine::pct(float v)
{
    return juce::String((int)std::round(v * 100.0f)) + "%";
}

//==============================================================================
// Entry point
//==============================================================================

std::vector<FeedbackItem> FeedbackEngine::generate(const AudioFeatures& f,
                                                    const GenreProfile& p) const
{
    std::vector<FeedbackItem> items;
    checkLoudness(f, p, items);
    checkDynamics(f, p, items);
    checkSpectral(f, p, items);
    checkStereo(f, p, items);
    checkRhythm(f, p, items);
    checkKey(f, p, items);
    suggestChain(f, p, items);
    return items;
}

//==============================================================================
// Loudness
//==============================================================================

void FeedbackEngine::checkLoudness(const AudioFeatures& f, const GenreProfile& p,
                                    std::vector<FeedbackItem>& out) const
{
    if (f.lufs < p.lufsMin)
    {
        float gap = p.lufsMin - f.lufs;
        out.push_back(critical("LOUDNESS",
            "Track is " + db(gap) + " below target loudness",
            "Integrated loudness is " + db(f.lufs) + ". " +
            p.displayName + " targets " + db(p.lufsTarget) +
            " (range: " + db(p.lufsMin) + " to " + db(p.lufsMax) + ").",
            "Push the master limiter harder, add a compressor on the master bus, "
            "or increase the gain of your loudest elements."));
    }
    else if (f.lufs > p.lufsMax)
    {
        float gap = f.lufs - p.lufsMax;
        out.push_back(warn("LOUDNESS",
            "Track may be over-compressed (" + db(gap) + " above target)",
            "Integrated loudness is " + db(f.lufs) + ". Above " +
            db(p.lufsMax) + " for " + p.displayName + ", dynamics start to suffer.",
            "Pull back the master limiter ceiling, reduce bus compression ratio, "
            "or lower the gain staging earlier in the chain."));
    }
    else
    {
        out.push_back(good("LOUDNESS",
            "Loudness " + db(f.lufs) + " - within " + p.displayName + " target range."));
    }
}

//==============================================================================
// Dynamics
//==============================================================================

void FeedbackEngine::checkDynamics(const AudioFeatures& f, const GenreProfile& p,
                                    std::vector<FeedbackItem>& out) const
{
    if (f.dynamicRangeDb < p.dynamicRangeMin)
    {
        out.push_back(warn("DYNAMICS",
            "Dynamic range is very tight (" + db(f.dynamicRangeDb) + ")",
            "Peak-to-LUFS ratio is " + db(f.dynamicRangeDb) + ". Below " +
            db(p.dynamicRangeMin) + " usually means heavy limiting is squashing transients.",
            "Increase the limiter ceiling, reduce the attack speed on bus compressors, "
            "or use parallel compression to preserve punch."));
    }
    else if (f.dynamicRangeDb > p.dynamicRangeMax)
    {
        out.push_back(warn("DYNAMICS",
            "Very wide dynamic range (" + db(f.dynamicRangeDb) + ") - may lack energy",
            "Peak-to-LUFS ratio of " + db(f.dynamicRangeDb) + " suggests the track "
            "is not being compressed/limited to genre standards.",
            "Add a limiter on the master bus targeting your genre's LUFS range. "
            "Light bus compression (2:1 ratio, 1-2 dB GR) helps glue the mix."));
    }
    else
    {
        out.push_back(good("DYNAMICS",
            "Dynamic range " + db(f.dynamicRangeDb) + " - healthy for " + p.displayName + "."));
    }
}

//==============================================================================
// Spectral balance
//==============================================================================

void FeedbackEngine::checkSpectral(const AudioFeatures& f, const GenreProfile& p,
                                    std::vector<FeedbackItem>& out) const
{
    float tol = p.spectralTolerance;

    // Sub (20-80 Hz)
    float subDelta = f.subPct - p.subPctTarget;
    if (subDelta < -tol)
        out.push_back(warn("FREQ - SUB",
            "Sub bass thin (" + pct(f.subPct) + " vs target " + pct(p.subPctTarget) + ")",
            "Sub frequencies (20-80 Hz) are underrepresented. "
            "The low end may feel weak on club speakers or subwoofers.",
            "Add or layer a sub bass element (sine oscillator or sampled bass), "
            "or boost 50-70 Hz on the bass bus."));
    else if (subDelta > tol)
        out.push_back(warn("FREQ - SUB",
            "Excessive sub energy (" + pct(f.subPct) + " vs target " + pct(p.subPctTarget) + ")",
            "Too much energy below 80 Hz can create muddiness and reduce loudness headroom.",
            "Apply a high-pass filter at 30-40 Hz on the master, "
            "or reduce sub content from non-bass elements (pads, reverb tails)."));
    else
        out.push_back(good("FREQ - SUB", "Sub balance " + pct(f.subPct) + " looks good."));

    // Low-mid (250 Hz - 2 kHz)
    float loMidDelta = f.lowMidPct - p.lowMidPctTarget;
    if (loMidDelta < -tol)
        out.push_back(warn("FREQ - MID",
            "Midrange sounds thin (" + pct(f.lowMidPct) + " vs target " + pct(p.lowMidPctTarget) + ")",
            "The 250 Hz-2 kHz region carries most of the perceived fullness and warmth. "
            "A thin midrange often means a missing pad, chord, or bass mid content.",
            "Add a pad layer, widen existing synth patches in the mid frequencies, "
            "or boost the 300-600 Hz region on the bass or lead bus."));
    else if (loMidDelta > tol)
        out.push_back(warn("FREQ - MID",
            "Midrange crowded (" + pct(f.lowMidPct) + " vs target " + pct(p.lowMidPctTarget) + ")",
            "Excess energy in 250 Hz-2 kHz can make a mix sound boxy or muddy.",
            "Cut 300-500 Hz slightly on elements competing in this range. "
            "High-pass pads above 150-200 Hz if they don't need low-mid content."));
    else
        out.push_back(good("FREQ - MID", "Low-mid balance " + pct(f.lowMidPct) + " looks good."));

    // Air (8 kHz+)
    float airDelta = f.airPct - p.airPctTarget;
    if (airDelta < -tol)
        out.push_back(warn("FREQ - AIR",
            "High-end lacks air (" + pct(f.airPct) + " vs target " + pct(p.airPctTarget) + ")",
            "Frequencies above 8 kHz give the mix brightness and presence. "
            "Low air content makes a mix sound dull on quality monitors.",
            "Add a high shelf boost (+1-3 dB at 10-12 kHz) on the master or air bus. "
            "Ensure hi-hats and percussion are not over-filtered."));
    else
        out.push_back(good("FREQ - AIR", "High-end air " + pct(f.airPct) + " looks good."));
}

//==============================================================================
// Stereo width
//==============================================================================

void FeedbackEngine::checkStereo(const AudioFeatures& f, const GenreProfile& p,
                                  std::vector<FeedbackItem>& out) const
{
    if (f.stereoWidth < p.stereoWidthMin - 0.08f)
    {
        out.push_back(warn("STEREO WIDTH",
            "Mix sounds narrow (width " + juce::String(f.stereoWidth, 2) +
            ", target " + juce::String(p.stereoWidthMin, 2) + "+)",
            p.displayName + " benefits from wide pads and synths to fill the stereo image.",
            "Apply a stereo imager to pad and synth buses. Try Haas delay (10-25 ms) "
            "on one side of a synth, or use Mid-Side EQ to boost the Side signal. "
            "Keep kick and bass mono."));
    }
    else if (f.stereoWidth > 0.88f)
    {
        out.push_back(warn("STEREO WIDTH",
            "Mix is excessively wide (width " + juce::String(f.stereoWidth, 2) + ")",
            "Very high stereo width can cause phase issues and bass cancellation "
            "when played in mono (club systems often sum to mono).",
            "Check the mix in mono. Use a correlation meter - keep it above -0.5. "
            "Reduce stereo widening on bass elements, keep sub bass mono."));
    }
    else
    {
        out.push_back(good("STEREO WIDTH",
            "Stereo width " + juce::String(f.stereoWidth, 2) +
            " - appropriate for " + p.displayName + "."));
    }
}

//==============================================================================
// Rhythm / BPM
//==============================================================================

void FeedbackEngine::checkRhythm(const AudioFeatures& f, const GenreProfile& p,
                                  std::vector<FeedbackItem>& out) const
{
    if (f.bpm < 1.0f)
    {
        out.push_back(warn("TEMPO",
            "Session BPM not available",
            "Ableton did not report a session tempo. "
            "This can happen in standalone mode or when no MIDI clock is running.",
            "Set the session BPM in Ableton before re-running analysis."));
        return;
    }

    if (f.bpm < p.bpmMin - 3.0f || f.bpm > p.bpmMax + 3.0f)
    {
        out.push_back(warn("TEMPO",
            juce::String(f.bpm, 1) + " BPM is outside " + p.displayName + " range "
            "(" + juce::String(p.bpmMin, 0) + "-" + juce::String(p.bpmMax, 0) + ")",
            "Session tempo is " + juce::String(f.bpm, 1) + " BPM. Tracks outside the "
            "genre range can be harder to DJ-mix with other " + p.displayName + " tracks.",
            "Adjust session BPM in Ableton, or ignore if the tempo is intentional."));
    }
    else
    {
        out.push_back(good("TEMPO",
            juce::String(f.bpm, 1) + " BPM (Ableton) - within " + p.displayName +
            " range (" + juce::String(p.bpmMin, 0) + "-" + juce::String(p.bpmMax, 0) + ")."));
    }
}

//==============================================================================
// Effect chain and sample suggestions
//==============================================================================

void FeedbackEngine::suggestChain(const AudioFeatures& f, const GenreProfile& p,
                                   std::vector<FeedbackItem>& out) const
{
    auto tip = [&](const juce::String& headline, const juce::String& detail = {})
    {
        out.push_back(good("CHAIN", headline + (detail.isNotEmpty() ? " - " + detail : "")));
    };

    // --- Genre-specific starter chain ---
    if (p.id == "melodic_techno")
    {
        tip("Master bus", "SSL-style buss comp (2:1, slow attack, 1-2 dB GR) -> EQ high shelf +1 dB at 12kHz -> Limiter at -0.3 dBTP");
        tip("Reverb", "Long plate reverb (4-8s decay) on a send for pads and leads - use high-pass on the return at 200 Hz");
        tip("Kick", "Transient shaper (e.g. Transient Master) to tighten the attack; keep kick sub mono below 80 Hz");
    }
    else if (p.id == "tech_house")
    {
        tip("Master bus", "Hard-knee comp (4:1, medium attack 10ms, 2-3 dB GR) for glue -> Limiter at -0.1 dBTP");
        tip("Kick", "Layer punchy sample with a sub thump. Add parallel saturation (e.g. Decapitator) for grit");
        tip("Bass", "Sidechain compress bass to kick at 4:1 ratio, fast attack (5-10ms), medium release (100ms)");
    }
    else if (p.id == "deep_house")
    {
        tip("Master bus", "Subtle glue comp (1.5:1, soft knee, 1 dB GR max) to preserve dynamics");
        tip("Chords/Pads", "Warm saturation on the piano or pad bus (RC-20, Decapitator, or Softube Saturation Knob)");
        tip("Kick", "Use brushed or softer samples. Layer with an acoustic room sample for organic feel");
    }
    else if (p.id == "minimal_techno")
    {
        tip("Master bus", "Very light comp (1.5:1, slow attack). Let transients breathe - minimal limiting");
        tip("Kick bus", "Parallel saturation track for texture without adding volume. Try tape saturation");
        tip("Space", "Stereo ping-pong delay on hi-hats/percussion instead of reverb for a dryer feel");
    }
    else if (p.id == "afro_house")
    {
        tip("Percussion bus", "Parallel compression + warm tube saturation. Light high shelf for presence");
        tip("Bass", "Warm analog-style sub (Moog-style oscillator). Sidechain to kick at 3:1 ratio");
        tip("Texture", "Layer field recordings or organic foley under the groove for depth and warmth");
    }
    else if (p.id == "progressive_house")
    {
        tip("Synths", "Long attack on pads with heavy reverb (8-12s). Automate filter cutoff through the build");
        tip("Master bus", "Transparent comp + high shelf +1-2 dB at 10-12 kHz for the trance-adjacent brightness");
        tip("Build-up", "Automated high-pass filter sweeping up to 500 Hz creates tension before the drop");
    }

    // --- Issue-specific conditional suggestions ---
    if (f.subPct < p.subPctTarget - p.spectralTolerance)
        tip("Sub bass thin", "Layer an 808 sine or Moog-style sub under the kick. Keep below 80 Hz, mono only");

    if (f.subPct > p.subPctTarget + p.spectralTolerance)
        tip("Excess sub", "FabFilter Pro-Q 3 dynamic EQ to notch resonant sub frequencies. HP non-bass elements at 80 Hz");

    if (f.lowMidPct < p.lowMidPctTarget - p.spectralTolerance)
        tip("Thin mids", "Add harmonic saturation on the pad bus (Decapitator, Satin, or UAD Studer A800). Boost 300-500 Hz on bass");

    if (f.airPct < p.airPctTarget - p.spectralTolerance)
        tip("Dull high end", "Pultec-style shelf at 10-12 kHz on the master (+2-3 dB). Try Gullfoss or Waves Vitamin for presence");

    if (f.stereoWidth < p.stereoWidthMin - 0.10f)
        tip("Narrow stereo", "Haas delay (15-20ms) panned hard L/R on one synth. Use Ozone Imager 2 on the pad bus. Keep bass mono");

    if (f.stereoWidth > 0.88f)
        tip("Phase risk", "Check mono compatibility with a correlation meter. Keep it above -0.5. Reduce widening on bass elements");

    if (f.dynamicRangeDb < p.dynamicRangeMin)
        tip("Over-compressed", "Try a clipper before the limiter (FabFilter Saturn clip mode or Kazrog True Iron). Use parallel compression at 30/70 ratio");

    if (f.dynamicRangeDb > p.dynamicRangeMax)
        tip("No limiter?", "Add a final transparent limiter if not present (FabFilter Pro-L 2, Limitless, or Ozone Maximizer). Check for reverb tail headroom issues");
}

//==============================================================================
// Key / tonality
//==============================================================================

void FeedbackEngine::checkKey(const AudioFeatures& f, const GenreProfile& p,
                               std::vector<FeedbackItem>& out) const
{
    if (f.key.isEmpty() || f.key == "Unknown")
    {
        out.push_back(warn("KEY",
            "Key detection inconclusive",
            "The tonal content is too sparse or atonal to detect a key reliably.",
            "Capture audio that includes melodic or harmonic elements."));
        return;
    }

    bool isMajor = f.key.containsIgnoreCase("major");
    bool isMinor = f.key.containsIgnoreCase("minor");

    if (p.keyPreference == "minor" && isMajor)
    {
        out.push_back(warn("KEY",
            "Detected key " + f.key + " - " + p.displayName + " favours minor keys",
            "A major key can work, but " + p.displayName + " typically uses minor tonality "
            "for its darker, more introspective character.",
            "This is not a hard rule - if the track feels right in major, keep it. "
            "Consider whether a relative minor fits better."));
    }
    else if (p.keyPreference == "major" && isMinor)
    {
        out.push_back(warn("KEY",
            "Detected key " + f.key + " - " + p.displayName + " favours major keys",
            "Minor key can work, but " + p.displayName + " typically uses major tonality.",
            "This is not a hard rule - trust your ear."));
    }
    else
    {
        juce::String pref = p.keyPreference == "any" ? "no key restriction"
                                                      : p.keyPreference + " key preference";
        out.push_back(good("KEY",
            f.key + " - matches " + p.displayName + " (" + pref + ")."));
    }
}
