# Rebuilding the melody corpus

These scripts build `corpus.json`, the input of `../train_melody_model.py`, from the Hooktheory
TheoryTab data published with Sheet Sage (github.com/chrisdonahue/sheetsage-data, CC BY-NC-SA 3.0).
The downloaded data and the corpus are **not** committed: they contain transcriptions of
copyrighted songs.

1. Download `Hooktheory.json.gz` and `Hooktheory_Raw.json.gz` from raw.githubusercontent.com
   (chrisdonahue/sheetsage-data) into `Tools/melody_model/raw/`.
2. From this folder, run `python3 build_hooktheory.py && python3 build_corpus.py && python3 analyze.py`.
   This writes `Tools/melody_model/corpus.json` and the statistics.
3. From the repo root, run `python3 Tools/melody_model/train_melody_model.py Tools/melody_model/corpus.json`.
   This regenerates `Source/Engine/MelodyModelData.h`.

`build_corpus.py` also looks for a folder of MIDI-pack files; those are reported separately and
are not used for training.
