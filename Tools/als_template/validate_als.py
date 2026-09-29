#!/usr/bin/env python3
"""Structural checks for a generated .als, against the rules real Live-saved
sets obey (see the comments in Source/Engine/AlsWriter.h). Live itself is not
needed. Exit status 0 = every check passed.

  python3 Tools/als_template/validate_als.py song.als [song.mid]

With a .mid (written by the same generate_track run) it also checks that the
Live Set contains exactly the same notes as the MIDI file.
"""
import gzip
import re
import struct
import sys
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
POOL_TAG = re.compile(r"^(\w*Target|Pointee|ControllerTargets\.\d+)$")

# Child order of a real Live 11.3 arrangement MidiClip.
CLIP_CHILDREN = [
    "LomId", "LomIdView", "CurrentStart", "CurrentEnd", "Loop", "Name", "Annotation", "Color", "LaunchMode",
    "LaunchQuantisation", "TimeSignature", "Envelopes", "ScrollerTimePreserver", "TimeSelection", "Legato", "Ram",
    "GrooveSettings", "Disabled", "VelocityAmount", "FollowAction", "Grid", "FreezeStart", "FreezeEnd", "IsWarped",
    "TakeId", "Notes", "BankSelectCoarse", "BankSelectFine", "ProgramChange", "NoteEditorFoldInZoom",
    "NoteEditorFoldInScroll", "NoteEditorFoldOutZoom", "NoteEditorFoldOutScroll", "NoteEditorFoldScaleZoom",
    "NoteEditorFoldScaleScroll", "ScaleInformation", "IsInKey", "NoteSpellingPreference", "PreferFlatRootNote",
    "ExpressionGrid",
]
NOTE_ATTRS = ["Time", "Duration", "Velocity", "VelocityDeviation", "OffVelocity", "Probability", "IsEnabled", "NoteId"]

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)


def skeleton(elem, skip_events=False):
    """Tag/attribute-name tree, used to compare a generated track with the template's."""
    out = []

    def walk(e, depth):
        out.append((depth, e.tag, tuple(sorted(k for k in e.attrib if k != "Value" and k != "Id")), "Value" in e.attrib))
        if skip_events and e.tag == "Events":
            return
        for c in e:
            walk(c, depth + 1)

    walk(elem, 0)
    return out


def midi_notes(path):
    data = Path(path).read_bytes()
    pos, notes = 14, []
    ppq = struct.unpack(">H", data[12:14])[0]
    track = 0
    while pos < len(data):
        length = struct.unpack(">I", data[pos + 4:pos + 8])[0]
        i, end, tick, open_notes = pos + 8, pos + 8 + length, 0, {}
        while i < end:
            delta = 0
            while True:
                b = data[i]; i += 1
                delta = (delta << 7) | (b & 0x7F)
                if not b & 0x80:
                    break
            tick += delta
            status = data[i]
            if status == 0xFF:
                i += 2
                l = 0
                while True:
                    b = data[i]; i += 1
                    l = (l << 7) | (b & 0x7F)
                    if not b & 0x80:
                        break
                i += l
            else:
                kind, pitch, vel = status & 0xF0, data[i + 1], data[i + 2]
                i += 3
                if kind == 0x90 and vel > 0:
                    open_notes[pitch] = (tick, vel)
                elif kind == 0x80 or kind == 0x90:
                    on, v = open_notes.pop(pitch)
                    notes.append((track, round(on / ppq, 4), pitch, v))
        pos = end
        track += 1
    return notes


def main():
    als = sys.argv[1]
    raw = Path(als).read_bytes()
    xml = gzip.decompress(raw).decode("utf-8")
    root = ET.fromstring(xml)
    template_root = ET.parse(HERE / "Live11_3_template.xml").getroot()

    # Header copied verbatim from the Live-saved template.
    check(root.attrib == template_root.attrib, f"<Ableton> header differs from template: {root.attrib}")
    live = root.find("LiveSet")

    # Global pointee pool: unique, NextPointeeId = max + 1.
    pool = [int(e.get("Id")) for e in root.iter() if POOL_TAG.match(e.tag) and "Id" in e.attrib]
    dupes = [k for k, n in Counter(pool).items() if n > 1]
    check(not dupes, f"duplicate pointee ids: {dupes[:10]}")
    next_id = int(live.find("NextPointeeId").get("Value"))
    check(next_id == max(pool) + 1, f"NextPointeeId {next_id} != max pool id {max(pool)} + 1")
    refs = {int(e.get("Value")) for e in root.iter("PointeeId")}
    check(refs <= set(pool), f"orphan PointeeId references: {sorted(refs - set(pool))}")

    # List-local ids unique within each parent (Live: "Non-unique list ids").
    for parent in root.iter():
        ids = Counter((c.tag, c.get("Id")) for c in parent if "Id" in c.attrib and not POOL_TAG.match(c.tag))
        bad = [k for k, n in ids.items() if n > 1]
        check(not bad, f"non-unique list ids under <{parent.tag}>: {bad[:5]}")

    tracks = list(live.find("Tracks"))
    scenes = len(live.find("Scenes"))
    returns = [t for t in tracks if t.tag == "ReturnTrack"]
    track_ids = [t.get("Id") for t in tracks]
    check(len(track_ids) == len(set(track_ids)), "duplicate track ids")

    template_track = template_root.find("LiveSet/Tracks/MidiTrack")
    # The generator deliberately strips the template's Max device.
    template_devices = template_track.find("DeviceChain/DeviceChain/Devices")
    for d in list(template_devices):
        template_devices.remove(d)
    template_skel = skeleton(template_track, skip_events=True)

    total_notes = 0
    als_notes = []
    for ti, t in enumerate(tracks):
        name = t.find("Name/EffectiveName").get("Value")
        check(t.tag == "MidiTrack", f"unexpected track type {t.tag}")
        check(t.find("Name/UserName").get("Value") == name, f"{name}: UserName != EffectiveName")
        color = int(t.find("Color").get("Value"))
        check(0 <= color <= 69, f"{name}: colour {color} out of range")
        check(skeleton(t, skip_events=True) == template_skel, f"{name}: track structure differs from the Live-saved template")
        for seq in ("MainSequencer", "FreezeSequencer"):
            slots = t.find(f"DeviceChain/{seq}/ClipSlotList")
            check(len(slots) == scenes, f"{name}: {seq} has {len(slots)} clip slots for {scenes} scenes")
        check(len(t.find("DeviceChain/Mixer/Sends")) == len(returns), f"{name}: sends != return tracks")
        check(len(t.find("DeviceChain/FreezeSequencer/Sample/ArrangerAutomation/Events")) == 0,
              f"{name}: FreezeSequencer must stay empty")

        prev_end = -1.0
        for clip in t.find("DeviceChain/MainSequencer/ClipTimeable/ArrangerAutomation/Events"):
            cname = clip.find("Name").get("Value")
            where = f"{name}/{cname}"
            check(clip.tag == "MidiClip", f"{where}: not a MidiClip")
            check([c.tag for c in clip] == CLIP_CHILDREN, f"{where}: MidiClip children differ from Live 11 layout")
            start = float(clip.get("Time"))
            cur_start = float(clip.find("CurrentStart").get("Value"))
            cur_end = float(clip.find("CurrentEnd").get("Value"))
            loop_start = float(clip.find("Loop/LoopStart").get("Value"))
            loop_end = float(clip.find("Loop/LoopEnd").get("Value"))
            check(start == cur_start, f"{where}: Time != CurrentStart")
            check(abs((cur_end - cur_start) - (loop_end - loop_start)) < 1e-9, f"{where}: length mismatch")
            check(clip.find("Loop/LoopOn").get("Value") == "false", f"{where}: LoopOn must be false")
            check(cur_start >= prev_end - 1e-9, f"{where}: overlaps the previous clip")
            prev_end = cur_end
            check(int(clip.find("Color").get("Value")) == color, f"{where}: clip colour differs from track colour")

            keys = [int(k.find("MidiKey").get("Value")) for k in clip.find("Notes/KeyTracks")]
            check(keys == sorted(keys) and len(keys) == len(set(keys)), f"{where}: KeyTracks not sorted/unique by MidiKey")
            note_ids = []
            for kt in clip.find("Notes/KeyTracks"):
                check([c.tag for c in kt] == ["Notes", "MidiKey"], f"{where}: KeyTrack child order")
                key = int(kt.find("MidiKey").get("Value"))
                last_end = -1.0
                for n in kt.find("Notes"):
                    check(list(n.attrib) == NOTE_ATTRS, f"{where}: MidiNoteEvent attributes {list(n.attrib)}")
                    nt, dur, vel = float(n.get("Time")), float(n.get("Duration")), int(n.get("Velocity"))
                    check(0 <= nt and dur > 0 and nt + dur <= loop_end + 1e-9, f"{where}: note outside clip")
                    check(1 <= vel <= 127, f"{where}: velocity {vel}")
                    check(nt >= last_end - 1e-9, f"{where}: overlapping notes on key {key}")
                    last_end = nt + dur
                    note_ids.append(int(n.get("NoteId")))
                    als_notes.append((ti + 1, round(cur_start + nt, 4), key, vel))
            check(len(note_ids) == len(set(note_ids)), f"{where}: duplicate NoteIds")
            next_note = int(clip.find("Notes/NoteIdGenerator/NextId").get("Value"))
            check(not note_ids or next_note > max(note_ids), f"{where}: NoteIdGenerator.NextId too low")
            total_notes += len(note_ids)

    # Tempo in both places.
    tempo_manual = live.find("MasterTrack/DeviceChain/Mixer/Tempo/Manual").get("Value")
    tempo_target = live.find("MasterTrack/DeviceChain/Mixer/Tempo/AutomationTarget").get("Id")
    tempo_event = None
    for env in live.find("MasterTrack/AutomationEnvelopes/Envelopes"):
        if env.find("EnvelopeTarget/PointeeId").get("Value") == tempo_target:
            tempo_event = env.find("Automation/Events/FloatEvent").get("Value")
    check(tempo_event == tempo_manual, f"tempo envelope {tempo_event} != Tempo/Manual {tempo_manual}")

    locators = live.findall("Locators/Locators/Locator")
    times = [float(l.find("Time").get("Value")) for l in locators]
    check(times == sorted(times), "locators not in time order")

    if len(sys.argv) > 2:
        mid = sorted(midi_notes(sys.argv[2]))
        check(sorted(als_notes) == mid, f"notes differ from the .mid ({len(als_notes)} vs {len(mid)})")

    print(f"{Path(als).name}: {len(tracks)} tracks, {total_notes} notes, {len(locators)} locators, "
          f"tempo {tempo_manual}, {len(pool)} pointee ids")
    if failures:
        for f in failures[:40]:
            print("FAIL", f)
        print(f"{len(failures)} failure(s)")
        return 1
    print("all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
