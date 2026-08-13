"""
AbletonOSC client wrapper.

AbletonOSC must be installed as an Ableton Control Surface
(https://github.com/ideoforms/AbletonOSC).
  - Ableton listens on port 11000
  - Responses come back to port 11001
"""

import socket
import threading
import time
from pythonosc.osc_message_builder import OscMessageBuilder
from pythonosc.osc_message import OscMessage

ABLETON_PORT = 11000
LISTEN_PORT  = 11001


class AbletonOSC:
    def __init__(self, host: str = '127.0.0.1'):
        self._addr = (host, ABLETON_PORT)
        self._responses: dict = {}
        self._lock = threading.Lock()
        self._running = True

        # One UDP socket bound to LISTEN_PORT so Ableton knows where to reply
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.bind(('0.0.0.0', LISTEN_PORT))
        self._sock.settimeout(0.05)

        self._recv_thread = threading.Thread(target=self._recv_loop, daemon=True)
        self._recv_thread.start()

    # ------------------------------------------------------------------
    def _recv_loop(self):
        while self._running:
            try:
                data, _ = self._sock.recvfrom(65535)
                msg = OscMessage(data)
                with self._lock:
                    self._responses[msg.address] = list(msg)
            except socket.timeout:
                pass
            except Exception:
                time.sleep(0.01)

    def close(self):
        self._running = False
        self._sock.close()

    # ------------------------------------------------------------------
    def _build(self, address: str, *args) -> bytes:
        b = OscMessageBuilder(address=address)
        for a in args:
            if isinstance(a, bool):
                b.add_arg(int(a))
            elif isinstance(a, int):
                b.add_arg(a)
            elif isinstance(a, float):
                b.add_arg(a)
            elif isinstance(a, str):
                b.add_arg(a)
        return b.build().dgram

    def send(self, address: str, *args, delay: float = 0.05):
        self._sock.sendto(self._build(address, *args), self._addr)
        time.sleep(delay)

    def query(self, address: str, *args, timeout: float = 2.0):
        with self._lock:
            self._responses.pop(address, None)
        self.send(address, *args, delay=0.02)
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self._lock:
                if address in self._responses:
                    return self._responses[address]
            time.sleep(0.01)
        return None

    # ------------------------------------------------------------------
    def ping(self) -> bool:
        r = self.query('/live/application/get/version', timeout=1.5)
        return r is not None

    def get_num_tracks(self) -> int:
        r = self.query('/live/song/get/num_tracks')
        return int(r[0]) if r else 0

    def set_tempo(self, bpm: float):
        self.send('/live/song/set/tempo', float(bpm))

    # ------------------------------------------------------------------
    def create_midi_track(self, at_index: int = -1) -> int:
        """Create a MIDI track and return its index."""
        r = self.query('/live/song/create_midi_track', int(at_index), timeout=2.0)
        time.sleep(0.15)
        if r is not None:
            return int(r[0])
        # Fallback: assume it was appended at the end
        total = self.get_num_tracks()
        return max(0, total - 1)

    def set_track_name(self, track_idx: int, name: str):
        self.send('/live/track/set/name', int(track_idx), str(name))

    def set_track_color(self, track_idx: int, color: int):
        self.send('/live/track/set/color', int(track_idx), int(color))

    # ------------------------------------------------------------------
    def create_clip(self, track_idx: int, clip_slot: int, length_beats: float):
        self.send('/live/clip_slot/create_clip',
                  int(track_idx), int(clip_slot), float(length_beats), delay=0.2)

    def wait_for_clip(self, track_idx: int, clip_slot: int, timeout: float = 4.0) -> bool:
        """Poll until the clip slot reports has_clip=True."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            r = self.query('/live/clip_slot/get/has_clip',
                           int(track_idx), int(clip_slot), timeout=0.5)
            if r and r[0]:
                return True
            time.sleep(0.1)
        return False

    def get_note_count(self, track_idx: int, clip_slot: int) -> int:
        """Return number of notes in a clip (5 OSC args per note)."""
        r = self.query('/live/clip/get/notes', int(track_idx), int(clip_slot), timeout=2.0)
        if r is None:
            return -1
        return len(r) // 5

    def set_clip_name(self, track_idx: int, clip_slot: int, name: str):
        self.send('/live/clip/set/name', int(track_idx), int(clip_slot), str(name))

    def add_notes(self, track_idx: int, clip_slot: int, notes: list):
        """notes: list of (pitch, time, duration, velocity, mute)"""
        if not notes:
            return
        flat = []
        for n in notes:
            flat += [int(n[0]), float(n[1]), float(n[2]), int(n[3]), int(n[4])]
        self.send('/live/clip/add/notes', int(track_idx), int(clip_slot), *flat, delay=0.08)

    # ------------------------------------------------------------------
    def set_scene_name(self, scene_idx: int, name: str):
        self.send('/live/scene/set/name', int(scene_idx), str(name))

    def fire_scene(self, scene_idx: int):
        self.send('/live/scene/fire', int(scene_idx))

    # ------------------------------------------------------------------
    def stop_playing(self):
        self.send('/live/song/stop_playing')
