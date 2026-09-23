#!/usr/bin/env python3
"""Plug a pretend USB keyboard in for N seconds, to exercise the live
touch-controls hot-plug path without hardware, and optionally drive with it.

    scripts/fake-keyboard.py 10                 # appears for 10 s, then goes away
    scripts/fake-keyboard.py 8 --press up,left,right,left --after 2 --gap 0.3

--press taps the named keys (evdev names without KEY_, comma separated) one
by one, --after seconds after plugging in and --gap seconds apart: sustained
driving, which the game's last-input layer answers by hiding the touch
controls. The window under test must have keyboard focus.

A uinput keyboard lives under /devices/virtual/input, which the detector
ignores on purpose (remappers and remote desktops put permanent virtual
keyboards there). Run the game with STK_INPUT_TRUST_UINPUT=1 to have this
one counted as a plugged-in keyboard. Key presses reach the game either way.

Needs write access to /dev/uinput (the `input` group or an ACL) and
python-evdev.
"""
import argparse
import time

from evdev import UInput, ecodes as e

p = argparse.ArgumentParser()
p.add_argument("seconds", nargs="?", type=float, default=10.0)
p.add_argument("--press", default="")
p.add_argument("--after", type=float, default=2.0)
p.add_argument("--gap", type=float, default=0.3)
a = p.parse_args()

keys = [getattr(e, "KEY_%s" % c) for c in "QWERTYUIOPASDFGHJKLZXCVBNM"]
keys += [e.KEY_ENTER, e.KEY_SPACE, e.KEY_LEFTSHIFT, e.KEY_ESC, e.KEY_1, e.KEY_0,
         e.KEY_UP, e.KEY_DOWN, e.KEY_LEFT, e.KEY_RIGHT]
start = time.time()
with UInput({e.EV_KEY: keys}, name="Test USB Keyboard", vendor=0x046d,
            product=0xc31c, version=1, bustype=e.BUS_USB) as ui:
    print("%.3f fake keyboard attached as %s" % (time.time(), ui.device.path),
          flush=True)
    presses = [k.strip().upper() for k in a.press.split(",") if k.strip()]
    if presses:
        time.sleep(a.after)
        for name in presses:
            code = getattr(e, "KEY_" + name)
            ui.write(e.EV_KEY, code, 1)
            ui.syn()
            time.sleep(0.08)
            ui.write(e.EV_KEY, code, 0)
            ui.syn()
            print("%.3f pressed %s" % (time.time(), name), flush=True)
            time.sleep(a.gap)
    time.sleep(max(0.0, a.seconds - (time.time() - start)))
print("%.3f fake keyboard removed" % time.time(), flush=True)
