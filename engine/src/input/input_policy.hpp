//
//  SuperTuxKart Touch - which on-screen input to show, from system presence
//  Copyright (C) 2026 SuperTuxKart-Touch contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.

#ifndef HEADER_INPUT_POLICY_HPP
#define HEADER_INPUT_POLICY_HPP

/** The decisions of docs/TOUCH_DETECTION.md as pure functions, so they can
 *  be tested without an engine (tests/input-presence). Every "listed" input
 *  is what the system says is attached; what the player does only picks
 *  between those, it never adds one. */
namespace InputPolicy
{
    /** What the player last drove with (InputHotplug's confirmation layer).
     *  Keyboard and gamepad are only ever set for a device the system
     *  lists; mouse use is not recorded at all. */
    enum Confirmed
    {
        CONFIRMED_NONE,
        CONFIRMED_TOUCH,
        CONFIRMED_KEYBOARD,
        CONFIRMED_GAMEPAD
    };

    /** Whether the on-screen race controls are shown.
     *  \param mode multitouch_active: 0 off, 1 auto, 2 always.
     *  \param touch_only multitouch_touch_only: in Auto, put the controls
     *         away while a listed keyboard or gamepad is in use.
     *  \param touchscreen The system lists a touchscreen.
     *  Touch input itself is never switched off: this only decides whether
     *  the buttons are drawn and fed. A keyboard or mouse merely being
     *  there never hides them. */
    inline bool touchControlsShown(int mode, bool touch_only, bool touchscreen,
                                   Confirmed confirmed)
    {
        if (mode == 0)
            return false;
        if (mode > 1)
            return true;
        if (confirmed == CONFIRMED_TOUCH)
            return true;
        if (!touchscreen)
            return false;
        if (touch_only && (confirmed == CONFIRMED_KEYBOARD ||
                           confirmed == CONFIRMED_GAMEPAD))
            return false;
        return true;
    }

    /** Whether an on-screen keyboard (STK's, or the system's) should come up
     *  for a text box: only when the system lists no keyboard, and there is
     *  a touchscreen or a gamepad to type with. */
    inline bool screenKeyboardWanted(bool enabled, bool keyboard_listed,
                                     bool touchscreen, bool gamepad_listed)
    {
        if (!enabled || keyboard_listed)
            return false;
        return touchscreen || gamepad_listed;
    }

    /** Whether a key press or gamepad activation may count towards hiding
     *  the touch controls: only for a device class the system lists. */
    inline bool confirmationCounts(bool device_listed)
    {
        return device_listed;
    }
}

#endif
