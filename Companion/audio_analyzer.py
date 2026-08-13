import hashlib
import json
import os
import sqlite3
import sys
import threading
import time
from concurrent.futures import as_completed

try:
    import librosa
    import numpy as np
    _LIBROSA_AVAILABLE = True
except ImportError:
    print("WARNING: librosa not installed — audio analysis unavailable, falling back to filename-only matching.", file=sys.stderr)
    _LIBROSA_AVAILABLE = False


_DB_PATH = os.path.expanduser("~/.cache/AbletonCopilot/samples.db")

_CREATE_TABLE = """
CREATE TABLE IF NOT EXISTS samples (
    hash              TEXT PRIMARY KEY,
    path              TEXT,
    bpm               REAL,
    key_root          INTEGER,
    is_minor          INTEGER,
    spectral_centroid REAL,
    sub_energy        REAL,
    low_energy        REAL,
    mid_energy        REAL,
    high_energy       REAL,
    transient_density REAL,
    rms               REAL,
    mfcc              TEXT,
    analyzed_at       REAL
)
"""


def _file_hash(path: str) -> str:
    """MD5 of the first 64 KB — fast enough for a 21k-file library."""
    h = hashlib.md5()
    with open(path, "rb") as f:
        h.update(f.read(65536))
    return h.hexdigest()


class SampleCache:
    "SQLite-backed cache for audio features."

    def __init__(self, db_path: str = _DB_PATH):
        os.makedirs(os.path.dirname(db_path), exist_ok=True)
        self._conn = sqlite3.connect(db_path, check_same_thread=False)
        self._conn.execute(_CREATE_TABLE)
        self._conn.commit()
        self._lock = threading.Lock()

    def get(self, path: str) -> dict | None:
        try:
            h = _file_hash(path)
        except OSError:
            return None
        row = self._conn.execute(
            "SELECT bpm, key_root, is_minor, spectral_centroid, sub_energy, "
            "low_energy, mid_energy, high_energy, transient_density, rms, mfcc "
            "FROM samples WHERE hash = ?", (h,)
        ).fetchone()
        if row is None:
            return None
        return {
            "bpm": row[0],
            "key_root": row[1],
            "is_minor": bool(row[2]) if row[2] is not None else None,
            "spectral_centroid": row[3],
            "sub_energy": row[4],
            "low_energy": row[5],
            "mid_energy": row[6],
            "high_energy": row[7],
            "transient_density": row[8],
            "rms": row[9],
            "mfcc": json.loads(row[10]) if row[10] else None,
        }

    def put(self, path: str, features: dict) -> None:
        try:
            h = _file_hash(path)
        except OSError:
            return
        with self._lock:
            self._conn.execute(
                "INSERT OR REPLACE INTO samples VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                (
                    h, path,
                    features.get("bpm"),
                    features.get("key_root"),
                    int(features["is_minor"]) if features.get("is_minor") is not None else None,
                    features.get("spectral_centroid"),
                    features.get("sub_energy"),
                    features.get("low_energy"),
                    features.get("mid_energy"),
                    features.get("high_energy"),
                    features.get("transient_density"),
                    features.get("rms"),
                    json.dumps(features["mfcc"]) if features.get("mfcc") is not None else None,
                    time.time(),
                ),
            )
            self._conn.commit()

    def close(self) -> None:
        self._conn.close()


def _band_energies(S: "np.ndarray", freqs: "np.ndarray") -> tuple:
    """Return (sub, low, mid, high) fractional energy from STFT magnitude."""
    power = S ** 2
    total = power.sum() or 1.0

    def band(lo, hi):
        mask = (freqs >= lo) & (freqs < hi)
        return float(power[mask].sum() / total)

    sub  = band(0,    80)
    low  = band(80,   300)
    mid  = band(300,  2000)
    high = band(2000, freqs[-1] + 1)
    return sub, low, mid, high


def _minor_strength(chroma_mean: "np.ndarray", root: int) -> float:
    """Sum chroma energy for a natural minor triad on `root`."""
    # minor triad: root, minor third (+3), fifth (+7)
    return float(chroma_mean[root % 12] + chroma_mean[(root + 3) % 12] + chroma_mean[(root + 7) % 12])


def _major_strength(chroma_mean: "np.ndarray", root: int) -> float:
    """Sum chroma energy for a major triad on `root`."""
    # major triad: root, major third (+4), fifth (+7)
    return float(chroma_mean[root % 12] + chroma_mean[(root + 4) % 12] + chroma_mean[(root + 7) % 12])


def analyze_file(path: str) -> dict | None:
    "Analyze a single audio file; returns feature dict or None on failure."
    if not _LIBROSA_AVAILABLE:
        return None
    try:
        import warnings
        warnings.filterwarnings('ignore')
        y, sr = librosa.load(path, sr=None, mono=True, duration=8.0)

        tempo, _ = librosa.beat.beat_track(y=y, sr=sr)
        bpm = float(np.atleast_1d(tempo)[0])

        chroma = librosa.feature.chroma_cqt(y=y, sr=sr)
        chroma_mean = chroma.mean(axis=1)
        key_root = int(chroma_mean.argmax())
        is_minor = _minor_strength(chroma_mean, key_root) >= _major_strength(chroma_mean, key_root)

        centroid = librosa.feature.spectral_centroid(y=y, sr=sr)
        spectral_centroid = float(centroid.mean())

        n_fft = 2048
        S = np.abs(librosa.stft(y, n_fft=n_fft))
        freqs = librosa.fft_frequencies(sr=sr, n_fft=n_fft)
        sub_e, low_e, mid_e, high_e = _band_energies(S, freqs)

        onset_env = librosa.onset.onset_strength(y=y, sr=sr)
        transient_density = float(onset_env.mean())

        rms = float(librosa.feature.rms(y=y).mean())

        mfcc = librosa.feature.mfcc(y=y, sr=sr, n_mfcc=13)
        mfcc_mean = mfcc.mean(axis=1).tolist()

        return {
            "bpm": bpm,
            "key_root": key_root,
            "is_minor": is_minor,
            "spectral_centroid": spectral_centroid,
            "sub_energy": sub_e,
            "low_energy": low_e,
            "mid_energy": mid_e,
            "high_energy": high_e,
            "transient_density": transient_density,
            "rms": rms,
            "mfcc": mfcc_mean,
        }
    except Exception:
        return None


def get_features(path: str, cache: SampleCache) -> dict | None:
    "Return cached features if available, otherwise analyze and cache."
    cached = cache.get(path)
    if cached is not None:
        return cached
    features = analyze_file(path)
    if features is not None:
        cache.put(path, features)
    return features


def analyze_batch(
    paths: list[str],
    cache: SampleCache,
    max_workers: int = 4,
) -> dict[str, dict]:
    "Analyze files in parallel using processes; skips hung files; returns path->features."
    from concurrent.futures import ProcessPoolExecutor, wait, FIRST_COMPLETED

    FILE_TIMEOUT = 30  # seconds before a file is considered hung and skipped

    to_analyze = [p for p in paths if cache.get(p) is None]
    already_cached = len(paths) - len(to_analyze)
    total = len(paths)
    done = already_cached
    results: dict[str, dict] = {}

    print(f"\r  Analyzing audio: {done} / {total}", end="", flush=True)

    with ProcessPoolExecutor(max_workers=max_workers) as pool:
        future_to_path = {pool.submit(analyze_file, p): p for p in to_analyze}
        pending = set(future_to_path.keys())

        while pending:
            newly_done, pending = wait(pending, timeout=FILE_TIMEOUT,
                                       return_when=FIRST_COMPLETED)

            if not newly_done:
                # Nothing finished within the timeout window — all remaining are hung
                print(f"\n  Skipping {len(pending)} unresponsive files")
                for fut in pending:
                    fut.cancel()
                done += len(pending)
                pending = set()
                break

            for fut in newly_done:
                path = future_to_path[fut]
                try:
                    feat = fut.result()
                    if feat is not None:
                        cache.put(path, feat)
                        results[path] = feat
                except Exception:
                    pass
                done += 1
                print(f"\r  Analyzing audio: {done} / {total}", end="", flush=True)

    print()
    return results
