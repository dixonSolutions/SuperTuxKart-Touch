#!/usr/bin/env python3
"""Synthesise touchscreen gestures on the tablet, for testing the touch HUD.

Creates a virtual multitouch screen through uinput and plays back gestures, so
the HUD can be exercised the same way a finger would without anyone holding the
device. Runs *on* the tablet.

    inject-touch.py tap 2400,1300
    inject-touch.py drag 400,1500 700,1500 --hold-ms 1500
    inject-touch.py hold 2400,1300 --hold-ms 2000

Multiple gestures separated by '+' run simultaneously as separate fingers:

    inject-touch.py drag 400,1500 700,1500 + hold 2360,1280
"""

from __future__ import annotations

import argparse
import sys
import time

from evdev import UInput, AbsInfo, ecodes as e

SCREEN_W = 2880
SCREEN_H = 1920
SLOTS = 4


def make_device() -> UInput:
    caps = {
        e.EV_KEY: [e.BTN_TOUCH],
        e.EV_ABS: [
            (e.ABS_MT_SLOT, AbsInfo(0, 0, SLOTS - 1, 0, 0, 0)),
            (e.ABS_MT_TRACKING_ID, AbsInfo(0, 0, 65535, 0, 0, 0)),
            (e.ABS_MT_POSITION_X, AbsInfo(0, 0, SCREEN_W - 1, 0, 0, 0)),
            (e.ABS_MT_POSITION_Y, AbsInfo(0, 0, SCREEN_H - 1, 0, 0, 0)),
            (e.ABS_X, AbsInfo(0, 0, SCREEN_W - 1, 0, 0, 0)),
            (e.ABS_Y, AbsInfo(0, 0, SCREEN_H - 1, 0, 0, 0)),
        ],
    }
    # INPUT_PROP_DIRECT is what makes the compositor treat this as a
    # touchscreen bound to the display rather than as a trackpad.
    return UInput(caps, name="stk-touch-test", version=0x1,
                  input_props=[e.INPUT_PROP_DIRECT])


class Finger:
    """One tracked contact, addressed by its multitouch slot."""

    _next_tracking_id = 100

    def __init__(self, ui: UInput, slot: int):
        self.ui = ui
        self.slot = slot
        self.down = False

    def touch_down(self, x: int, y: int) -> None:
        Finger._next_tracking_id += 1
        self.ui.write(e.EV_ABS, e.ABS_MT_SLOT, self.slot)
        self.ui.write(e.EV_ABS, e.ABS_MT_TRACKING_ID,
                      Finger._next_tracking_id)
        self.ui.write(e.EV_ABS, e.ABS_MT_POSITION_X, x)
        self.ui.write(e.EV_ABS, e.ABS_MT_POSITION_Y, y)
        self.ui.write(e.EV_ABS, e.ABS_X, x)
        self.ui.write(e.EV_ABS, e.ABS_Y, y)
        self.ui.write(e.EV_KEY, e.BTN_TOUCH, 1)
        self.down = True

    def move(self, x: int, y: int) -> None:
        self.ui.write(e.EV_ABS, e.ABS_MT_SLOT, self.slot)
        self.ui.write(e.EV_ABS, e.ABS_MT_POSITION_X, x)
        self.ui.write(e.EV_ABS, e.ABS_MT_POSITION_Y, y)
        self.ui.write(e.EV_ABS, e.ABS_X, x)
        self.ui.write(e.EV_ABS, e.ABS_Y, y)

    def touch_up(self) -> None:
        self.ui.write(e.EV_ABS, e.ABS_MT_SLOT, self.slot)
        self.ui.write(e.EV_ABS, e.ABS_MT_TRACKING_ID, -1)
        self.ui.write(e.EV_KEY, e.BTN_TOUCH, 0)
        self.down = False


def parse_point(text: str) -> tuple[int, int]:
    x, y = text.split(",")
    return int(x), int(y)


def split_gestures(argv: list[str]) -> list[list[str]]:
    groups: list[list[str]] = [[]]
    for token in argv:
        if token == "+":
            groups.append([])
        else:
            groups[-1].append(token)
    return groups


def parse_gesture(tokens: list[str]):
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("kind", choices=["tap", "hold", "drag"])
    parser.add_argument("points", nargs="+")
    parser.add_argument("--hold-ms", type=int, default=800)
    return parser.parse_args(tokens)


def main() -> int:
    argv = sys.argv[1:]
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0

    gestures = [parse_gesture(g) for g in split_gestures(argv) if g]

    ui = make_device()
    # The compositor needs a moment to notice the new device before the first
    # events are meaningful.
    time.sleep(1.0)

    fingers = [Finger(ui, i) for i in range(len(gestures))]
    paths = []
    for gesture in gestures:
        points = [parse_point(p) for p in gesture.points]
        if gesture.kind == "drag" and len(points) < 2:
            print("drag needs two points", file=sys.stderr)
            return 2
        paths.append(points)

    hold_ms = max(g.hold_ms for g in gestures)
    steps = 24
    step_delay = (hold_ms / 1000.0) / steps

    for finger, points in zip(fingers, paths):
        finger.touch_down(*points[0])
    ui.syn()

    for i in range(1, steps + 1):
        t = i / steps
        for finger, (gesture, points) in zip(fingers, zip(gestures, paths)):
            if gesture.kind == "drag":
                x0, y0 = points[0]
                x1, y1 = points[-1]
                finger.move(int(x0 + (x1 - x0) * t), int(y0 + (y1 - y0) * t))
            else:
                finger.move(*points[0])
        ui.syn()
        time.sleep(step_delay)

    for finger in fingers:
        finger.touch_up()
    ui.syn()
    time.sleep(0.2)
    ui.close()
    print("gesture done")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
