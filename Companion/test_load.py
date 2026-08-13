import sys
from collections import namedtuple
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import stack_renderer

genre = "house"
bpm   = 128.0

Entry = namedtuple("Entry", ["path"])
matches = {
    "KICK":  [Entry("/Users/shawnshirazi/SAMPLES_TEST2/Odd Frequency - Exo 2 - Kick 9.wav")],
    "SNARE": [Entry("/Users/shawnshirazi/SAMPLES_TEST2/Odd Frequency - Grid Vol 2 - Clap:Snare 19.wav")],
    "HIHAT": [Entry("/Users/shawnshirazi/SAMPLES_TEST2/Odd Frequency - Grid Vol 2 - Open Hat 19.wav")],
}

out_dir = Path.home() / "Library" / "AbletonCopilot" / "Session"
stack_dir = stack_renderer.build(matches, genre, bpm, out_dir)
print("Stack rendered to:", stack_dir)
