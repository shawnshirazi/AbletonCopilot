"""
Shared transcription + windowing logic for the fragment-extraction scripts.

Both extract_fragments.py (batch corpus builder, an isolated bass stem per
song segment from the user's existing stem library) and
extract_reference_fragments.py (single reference track, bass isolated via
Demucs) need identical basic-pitch transcription and step-grid windowing so
the two produce fragments in the exact same shape the C++ side already
parses (MelodyGridComponent::loadBassFragmentsIfNeeded /
loadReferenceFragments) - this module is the one place that logic lives.
"""

from pathlib import Path

STEPS_PER_BAR = 16
BARS_PER_FRAGMENT = 2
STEPS_PER_FRAGMENT = STEPS_PER_BAR * BARS_PER_FRAGMENT  # 32
MELODY_OFF = None  # matches the plugin's null-for-no-note JSON convention
MIN_NOTES_PER_FRAGMENT = 3  # discard near-silent windows


def transcribe(wav_path: Path):
    """Run basic-pitch in-process, return a pretty_midi.PrettyMIDI or None."""
    from basic_pitch.inference import predict
    from basic_pitch import ICASSP_2022_MODEL_PATH

    try:
        _, midi_data, _ = predict(str(wav_path), ICASSP_2022_MODEL_PATH)
        return midi_data
    except Exception as e:
        print(f"    transcription failed: {e}")
        return None


def extract_drum_patterns(drums_wav: Path, bpm: float, output_dir: Path) -> dict:
    """Approximate per-role (KICK/SNARE/CLAP/HIHAT) 8-bar step patterns and a
    representative one-shot sample, from an isolated drum stem.

    Honesty note: Demucs' "drums" stem is kick+snare+hat+perc all mixed
    together - there's no clean per-instrument separation the way the bass
    stem gets its own isolated line. Roles are inferred from which
    frequency band an onset's energy falls in (kick = low, hat = high,
    snare/clap = broadband mid transient) - a real, standard onset-detection
    technique, but a heuristic. Snare and clap share the same mid-band
    signal (nothing here can tell them apart), so both get the same
    steps/sample - the C++ side applies whichever role actually has a row.

    Returns {role: {"steps": [bool x 128], "sample": "<absolute wav path>"}},
    only for roles where onsets were actually found.
    """
    import numpy as np
    import scipy.signal as sig
    import soundfile as sf
    import librosa

    y, sr = librosa.load(str(drums_wav), sr=None, mono=True)

    sec_per_step = (60.0 / bpm) / 4.0
    steps_per_bar = STEPS_PER_BAR
    bars = 8
    total_steps = steps_per_bar * bars
    window_secs = total_steps * sec_per_step
    bar_secs = steps_per_bar * sec_per_step

    def band_filter(low, high):
        nyq = sr / 2.0
        if high is None:
            b, a = sig.butter(4, low / nyq, btype="highpass")
        elif low is None:
            b, a = sig.butter(4, high / nyq, btype="lowpass")
        else:
            b, a = sig.butter(4, [low / nyq, high / nyq], btype="bandpass")
        return sig.filtfilt(b, a, y)

    bands = {
        "KICK":       band_filter(None, 150.0),
        "SNARE_CLAP": band_filter(150.0, 4000.0),
        "HIHAT":      band_filter(6000.0, None),
    }

    hop_length = 512
    # wait ~35ms between onsets in the same band - short enough not to
    # suppress genuine fast (16th-note) hits, long enough to stop a single
    # hit's own attack/decay ringing from registering as two or three
    # separate onsets, which is the main source of an implausibly dense
    # ("every other step") detected pattern.
    wait_frames = max(1, int(round(0.035 * sr / hop_length)))

    onset_times_raw = {
        role: librosa.onset.onset_detect(y=band, sr=sr, hop_length=hop_length,
                                          units="time", backtrack=False, wait=wait_frames)
        for role, band in bands.items()
    }

    def peak_amp(band, t):
        chunk = band[int(t * sr):int(t * sr) + int(0.03 * sr)]
        return float(np.max(np.abs(chunk))) if len(chunk) > 0 else 0.0

    # Drop onsets that are weak relative to this role's own strongest hit -
    # low-frequency bleed from other instruments (e.g. bass) into the KICK
    # band tends to show up as many quiet, spurious "onsets" alongside the
    # real, much louder kick hits; a relative amplitude floor filters most
    # of that out without needing a fixed, track-independent threshold.
    onset_times = {}
    for role, times in onset_times_raw.items():
        if len(times) == 0:
            onset_times[role] = times
            continue
        amps = np.array([peak_amp(bands[role], t) for t in times])
        floor = 0.35 * float(np.max(amps))
        onset_times[role] = times[amps >= floor]

    # Find a musically COHERENT 8-bar window, not just whichever 8 bars have
    # the most raw onsets. Picking by raw count alone can straddle a real
    # section boundary - e.g. half a breakdown (no kick, sparse hats) plus
    # half a drop (kick back in) can out-count a genuinely steady drop,
    # producing a pattern that's an incoherent blend of two different
    # sections rather than one real groove.
    #
    # The signal that actually matters here is KICK presence specifically,
    # not combined onset density across all bands - a real melodic-techno
    # breakdown pulls the kick out entirely but usually keeps hi-hats/perc
    # going (this genre's own convention - see MelodicTechnoTheory.cpp's
    # kSectionGuidance), so a combined-band density metric barely dips
    # during a breakdown and fails to separate it from a real drop. Kick
    # alone is close to binary: ~4 hits/bar (one per beat) when it's a
    # steady four-on-the-floor drop, 0 when it's pulled for a breakdown -
    # verified directly against a real track's per-bar kick count before
    # landing on this approach.
    duration = len(y) / sr
    num_bars = int(np.ceil(duration / bar_secs))

    kick_onsets = onset_times.get("KICK", np.array([]))
    kick_density = np.zeros(max(num_bars, 1))
    for t in kick_onsets:
        b = int(t / bar_secs)
        if 0 <= b < len(kick_density):
            kick_density[b] += 1

    best_start = None
    if num_bars >= bars and np.max(kick_density) > 0:
        # >=3 kicks in a bar is "steady four-on-the-floor here", not just a
        # stray hit or a buildup fill (which spikes well above 4, see the
        # verification above - a fill/riser bar hit 12-17, clearly not a
        # normal beat).
        is_high = kick_density >= 3

        runs, run_start = [], None
        for i, high in enumerate(is_high):
            if high and run_start is None:
                run_start = i
            elif not high and run_start is not None:
                runs.append((run_start, i))
                run_start = None
        if run_start is not None:
            runs.append((run_start, len(is_high)))

        long_runs = [r for r in runs if r[1] - r[0] >= bars]
        if long_runs:
            # Longest steady-kick run, then the most internally stable 8
            # bars inside it (lowest kick-count variance) - avoids picking
            # a window right at the run's own start (which can still catch
            # the tail of a buildup fill).
            run_start, run_end = max(long_runs, key=lambda r: r[1] - r[0])
            best_var = None
            for start_bar in range(run_start, run_end - bars + 1):
                window = kick_density[start_bar:start_bar + bars]
                var = float(np.var(window))
                if best_var is None or var < best_var:
                    best_var, best_start = var, start_bar * bar_secs

    if best_start is None:
        # Fallback: busiest single window (short/uneven material where no
        # stable-density run reached a full 8 bars).
        best_start, best_count = 0.0, -1
        start = 0.0
        while start + window_secs <= duration:
            count = sum(int(np.sum((onsets >= start) & (onsets < start + window_secs)))
                        for onsets in onset_times.values())
            if count > best_count:
                best_count, best_start = count, start
            start += bar_secs

    role_results = {}
    for role, onsets in onset_times.items():
        in_window = onsets[(onsets >= best_start) & (onsets < best_start + window_secs)]
        if len(in_window) == 0:
            continue

        steps = [False] * total_steps
        for t in in_window:
            step = int(round((t - best_start) / sec_per_step))
            if 0 <= step < total_steps:
                steps[step] = True

        # Representative one-shot: the loudest onset in the window, sliced
        # from the FULL-band stem (not the filtered band) so it keeps its
        # real timbre, not the band-limited signal used only for detection.
        amps = []
        for t in in_window:
            chunk = y[int(t * sr):int(t * sr) + int(0.05 * sr)]
            amps.append(float(np.max(np.abs(chunk))) if len(chunk) > 0 else 0.0)
        hit_time = float(in_window[int(np.argmax(amps))])

        slice_len = int(0.25 * sr)
        start_sample = max(0, int(hit_time * sr))
        end_sample = min(len(y), start_sample + slice_len)
        one_shot = y[start_sample:end_sample].copy()

        fade_len = min(int(0.03 * sr), len(one_shot))
        if fade_len > 0:
            one_shot[-fade_len:] *= np.linspace(1.0, 0.0, fade_len)

        sample_path = output_dir / f"{role.lower()}.wav"
        # Explicit PCM_16 rather than soundfile's default (32-bit float for
        # a float32 array) - standard, universally-readable one-shot sample
        # format, removes any doubt about whether the extracted file is a
        # WAV subtype every downstream reader definitely supports.
        sf.write(str(sample_path), one_shot, sr, subtype="PCM_16")

        role_results[role] = {"steps": steps, "sample": str(sample_path)}

    # Split the shared mid-band result into both named rows the C++ side
    # actually knows about (see DrumMachineComponent's rackIds).
    output = {r: v for r, v in role_results.items() if r != "SNARE_CLAP"}
    if "SNARE_CLAP" in role_results:
        output["SNARE"] = role_results["SNARE_CLAP"]
        output["CLAP"]  = role_results["SNARE_CLAP"]

    return output


def notes_to_fragments(midi_data, bpm: float, key_root: int):
    """Quantize notes onto the step grid, slice into 2-bar windows."""
    sec_per_step = (60.0 / bpm) / 4.0  # one 16th note

    step_offsets: dict[int, list[int]] = {}
    for inst in midi_data.instruments:
        for note in inst.notes:
            step = int(round(note.start / sec_per_step))
            offset = int(note.pitch) - (36 + key_root)  # relative to plugin's bass register (C2 + root)
            step_offsets.setdefault(step, []).append(offset)

    if not step_offsets:
        return []

    max_step = max(step_offsets)
    fragments = []
    for win_start in range(0, max_step + 1, STEPS_PER_FRAGMENT):
        win_steps: list[int | None] = [None] * STEPS_PER_FRAGMENT
        count = 0
        for s in range(win_start, win_start + STEPS_PER_FRAGMENT):
            if s in step_offsets:
                # a step can have multiple simultaneous onsets (transcription
                # noise or a real chord stab) — take the lowest, bass-appropriate
                win_steps[s - win_start] = min(step_offsets[s])
                count += 1
        if count >= MIN_NOTES_PER_FRAGMENT:
            fragments.append(win_steps)

    return fragments
