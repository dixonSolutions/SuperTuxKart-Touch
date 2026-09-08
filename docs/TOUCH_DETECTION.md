# Touch hardware detection

Upstream SuperTuxKart already has the control plane:

| Param | Values |
|-------|--------|
| `multitouch_active` | 0 = off, 1 = auto (if available), 2 = always |
| `multitouch_draw_gui` | Show the on-screen race buttons |
| `multitouch_controls` | Steering wheel / accelerometer / gyroscope |

Android and iOS assume a touchscreen. Linux tablets do not. This port keeps that
0/1/2 switch and adds a real scan plus a touch-only filter.

## What was missing

`CIrrDeviceSDL::supportsTouchDevice()` was only `SDL_GetNumTouchDevices() > 0`.
On Linux that is often 0 until the first finger, and convertibles with a
keyboard were treated the same as a phone.

`main_touch.cpp` used to **force** `multitouch_active = 2` after config load, so
the Auto setting could not stick.

## What we added

| Piece | Role |
|-------|------|
| `src/input/linux_touch_detect.hpp` | `/proc/bus/input/devices`, DMI chassis, Ubuntu Touch |
| `CIrrDeviceSDL::supportsTouchDevice` | SDL **or** sysfs touchscreen |
| `CIrrDeviceSDL::hasHardwareKeyboard` | Real `KEY_A` keyboard, not gpio-keys |
| `multitouch_touch_only` | When Auto, require a touch-only device |
| `IrrDriver::isMultitouchEnabled()` | Single policy used by race GUI, input, options |
| Touch settings dialog | Off / Auto / Always spinner + touch-only checkbox |

## Defaults (this port)

- `multitouch_active` default **1 (Auto)** — no longer forced to Always
- `multitouch_touch_only` default **true** on `TOUCH_STK` builds
- On-screen buttons and the screen keyboard still default on
- Override: `STK_TOUCH_MODE=always|auto|off`

Official STK should keep `multitouch_touch_only` default **false** so existing
"if available" behaviour on any touchscreen does not change. The Touch port
opts into the stricter filter.

## Detection

Same rules as Xonotic Touch (`docs/TOUCH_DETECTION.md` there):

1. SDL touch devices
2. `INPUT_PROP_DIRECT` / name `touchscreen` in `/proc/bus/input/devices`
3. Chassis type 11 (handheld) or 30 (tablet)
4. Ubuntu Touch / Lomiri / `CLICK_FRAMEWORK`

A laptop with a Type Cover is not touch-only. A phone is.

## Upstream

- PR: https://github.com/supertuxkart/stk-code/pull/5830
- Official contact is GitHub + the [STK forums](https://supertuxkart.net/Community), not phone.
