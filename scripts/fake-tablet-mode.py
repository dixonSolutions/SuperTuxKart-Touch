#!/usr/bin/env python3
"""Pretend to be a convertible's SW_TABLET_MODE switch.

    scripts/fake-tablet-mode.py on:5 off:3 on:4   # engaged 5 s, released 3 s, ...

Each step sets the switch and holds it for the given seconds; the device is
removed at the end. With the laptop's own keyboard present, "on" is what a
Surface with its Type Cover folded back looks like: the game should treat
the built-in keyboard as gone and, if a touchscreen is present (see
fake-touch.py wait:MS), show the touch controls.

Needs write access to /dev/uinput (the `input` group or an ACL) and
python-evdev.
"""
import sys
import time

from evdev import UInput, ecodes as e

steps = sys.argv[1:] or ["on:5", "off:3"]
with UInput({e.EV_SW: [e.SW_TABLET_MODE]}, name="Test Tablet Mode Switch",
            bustype=e.BUS_HOST) as ui:
    print("fake tablet switch attached as", ui.device.path, flush=True)
    time.sleep(0.5)
    for step in steps:
        state, _, secs = step.partition(":")
        value = 1 if state == "on" else 0
        ui.write(e.EV_SW, e.SW_TABLET_MODE, value)
        ui.syn()
        print("%.3f tablet mode %s" % (time.time(), state), flush=True)
        time.sleep(float(secs or 3))
print("%.3f fake tablet switch removed" % time.time(), flush=True)
