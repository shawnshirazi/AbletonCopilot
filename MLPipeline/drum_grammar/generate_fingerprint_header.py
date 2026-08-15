#!/usr/bin/env python3
"""
Generates Source/Engine/DrumSampleFingerprint.h from
MLPipeline/drum_grammar/output/drum_grammar.json's sound_fingerprints
block - the ONLY place the measured Melodic Techno target numbers used by
Engine::scoreForRole (Source/Engine/DrumSampleScoring.cpp) come from.

Run this whenever drum_grammar.json is regenerated (analyze_drum_grammar.py)
to keep the C++ scoring target in sync with the real measurement. Never
hand-edit the generated header - if a number there looks wrong, fix the
analysis or the corpus, then regenerate.

Only maps the fingerprint dimensions DrumSampleFeatures (Source/Engine/
DrumSampleFeatures.h) can actually measure at runtime (duration/attack/
zero-crossing-rate/pitch/peak/rms) - spectral_centroid_hz,
decay_ms_to_minus40db and sub_bass_fraction_20_100hz are real, measured,
and kept in drum_grammar.json for the human-readable record, but the
runtime plugin has no FFT and doesn't compute them, so they can't be
scored against yet without extending DrumSampleAnalysis.cpp too (a
separate, larger change - out of scope for this pass).
"""

import datetime
import json
from pathlib import Path

JSON_PATH = Path(__file__).parent / "output" / "drum_grammar.json"
HEADER_PATH = Path(__file__).parents[2] / "Source" / "Engine" / "DrumSampleFingerprint.h"

ROLES = ["KICK", "CLAP", "HAT", "PERC"]


def dim(fp, key_ms_or_hz, scale=1.0):
    v = fp[key_ms_or_hz]
    return v["mean"] * scale, v["stddev"] * scale


def main():
    data = json.loads(JSON_PATH.read_text())
    fingerprints = data["sound_fingerprints"]

    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("// GENERATED FILE - do not hand-edit.")
    lines.append("//")
    lines.append("// Regenerate with:")
    lines.append("//   MLPipeline/venv/bin/python3 MLPipeline/drum_grammar/generate_fingerprint_header.py")
    lines.append("// which reads MLPipeline/drum_grammar/output/drum_grammar.json's")
    lines.append("// sound_fingerprints block - real mean/stddev measurements (duration,")
    lines.append("// attack time, zero-crossing rate, estimated pitch, peak, RMS) taken")
    lines.append("// directly from one-shot samples in four Melodic-Techno-branded sample")
    lines.append("// packs already in the user's library (PML Mirage, PML Mystique, Odd")
    lines.append("// Frequency Exo, Odd Frequency Exo 2) - see")
    lines.append("// MLPipeline/drum_grammar/analyze_drum_grammar.py for the measurement")
    lines.append("// methodology. Not hand-tuned guesses - if a number here looks wrong,")
    lines.append("// fix the analysis or the corpus and regenerate, don't edit this file.")
    lines.append(f"// Generated: {datetime.date.today().isoformat()}")
    for role in ROLES:
        lines.append(f"// {role}: n={fingerprints[role]['num_samples']} one-shot samples analyzed")
    lines.append("")
    lines.append('#include "DrumVoiceSynth.h" // DrumRole')
    lines.append("")
    lines.append("namespace Engine")
    lines.append("{")
    lines.append("    struct FingerprintDim { float mean; float stddev; };")
    lines.append("")
    lines.append("    // One dimension per DrumSampleFeatures field the runtime plugin can")
    lines.append("    // actually measure (see Source/DrumSampleAnalysis.cpp) - stddev is")
    lines.append("    // the real measured spread, used by scoreForRole as a Gaussian")
    lines.append("    // similarity width, not a hand-picked tolerance.")
    lines.append("    struct DrumRoleFingerprint")
    lines.append("    {")
    lines.append("        FingerprintDim durationSec;")
    lines.append("        FingerprintDim attackMs;")
    lines.append("        FingerprintDim zeroCrossingHz;")
    lines.append("        FingerprintDim estimatedPitchHz;")
    lines.append("        FingerprintDim peak;")
    lines.append("        FingerprintDim rms;")
    lines.append("    };")
    lines.append("")

    role_to_var = {"KICK": "kKickFingerprint", "CLAP": "kClapFingerprint", "HAT": "kHatFingerprint", "PERC": "kPercFingerprint"}

    for role in ROLES:
        fp = fingerprints[role]
        dur_mean, dur_sd = dim(fp, "duration_ms", scale=0.001)
        atk_mean, atk_sd = dim(fp, "attack_ms")
        zcr_mean, zcr_sd = dim(fp, "zero_crossing_rate_hz")
        pitch_mean, pitch_sd = dim(fp, "estimated_pitch_hz")
        peak_mean, peak_sd = dim(fp, "peak")
        rms_mean, rms_sd = dim(fp, "rms")

        # A zero measured stddev (e.g. CLAP's pitch - no clap in the
        # corpus had a detectable fundamental) would divide-by-zero in a
        # Gaussian similarity computation; floor it to a small-but-real
        # tolerance rather than silently treating that dimension as an
        # exact-match requirement.
        def floor_sd(sd, minimum):
            return sd if sd > minimum else minimum

        lines.append(f"    constexpr DrumRoleFingerprint {role_to_var[role]} {{")
        lines.append(f"        {{ {dur_mean:.6f}f, {floor_sd(dur_sd, 0.01):.6f}f }},   // durationSec")
        lines.append(f"        {{ {atk_mean:.4f}f, {floor_sd(atk_sd, 0.5):.4f}f }},   // attackMs")
        lines.append(f"        {{ {zcr_mean:.3f}f, {floor_sd(zcr_sd, 50.0):.3f}f }},   // zeroCrossingHz")
        lines.append(f"        {{ {pitch_mean:.3f}f, {floor_sd(pitch_sd, 5.0):.3f}f }},   // estimatedPitchHz")
        lines.append(f"        {{ {peak_mean:.4f}f, {floor_sd(peak_sd, 0.02):.4f}f }},   // peak")
        lines.append(f"        {{ {rms_mean:.4f}f, {floor_sd(rms_sd, 0.01):.4f}f }},   // rms")
        lines.append("    };")
        lines.append("")

    lines.append("    inline const DrumRoleFingerprint& targetFingerprintForRole(DrumRole role)")
    lines.append("    {")
    lines.append("        switch (role)")
    lines.append("        {")
    lines.append(f"            case DrumRole::Kick:  return {role_to_var['KICK']};")
    lines.append(f"            case DrumRole::Clap:  return {role_to_var['CLAP']};")
    lines.append(f"            case DrumRole::Hat:   return {role_to_var['HAT']};")
    lines.append(f"            case DrumRole::Perc:  return {role_to_var['PERC']};")
    lines.append(f"            case DrumRole::Count: return {role_to_var['KICK']};")
    lines.append("        }")
    lines.append(f"        return {role_to_var['KICK']};")
    lines.append("    }")
    lines.append("}")
    lines.append("")

    HEADER_PATH.write_text("\n".join(lines))
    print(f"Wrote {HEADER_PATH}")


if __name__ == "__main__":
    main()
