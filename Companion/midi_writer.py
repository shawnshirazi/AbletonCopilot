"""
Writes Standard MIDI Files (format 0) from pattern note lists.
No external dependencies — pure stdlib.
"""

import struct


def _varint(n: int) -> bytes:
    result = []
    result.append(n & 0x7F)
    n >>= 7
    while n:
        result.append(0x80 | (n & 0x7F))
        n >>= 7
    return bytes(reversed(result))


def write(path: str, notes: list, bpm: float, ticks_per_beat: int = 480):
    """
    Write a MIDI file.

    notes : list of (pitch, time_beats, duration_beats, velocity, mute)
    bpm   : tempo in BPM — embedded as a meta event so Ableton imports at
            the correct tempo.
    """
    usec = int(60_000_000 / max(bpm, 1))

    events = []
    for pitch, t_b, d_b, vel, mute in notes:
        if mute:
            continue
        t  = int(round(t_b * ticks_per_beat))
        d  = max(1, int(round(d_b * ticks_per_beat)))
        events.append((t,     1, pitch & 0x7F, vel & 0x7F))   # note on
        events.append((t + d, 0, pitch & 0x7F, 0))            # note off

    # note-off before note-on at identical ticks
    events.sort(key=lambda e: (e[0], e[1]))

    track = bytearray()

    # Tempo
    track += _varint(0)
    track += b'\xff\x51\x03'
    track += usec.to_bytes(3, 'big')

    last = 0
    for tick, kind, note, vel in events:
        track += _varint(tick - last)
        last = tick
        track += bytes([0x90 if kind else 0x80, note, vel])

    track += b'\x00\xff\x2f\x00'  # end of track

    header = b'MThd' + struct.pack('>IHHH', 6, 0, 1, ticks_per_beat)
    chunk  = b'MTrk' + struct.pack('>I', len(track)) + bytes(track)

    with open(path, 'wb') as f:
        f.write(header + chunk)
