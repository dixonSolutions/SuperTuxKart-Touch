//
//  SuperTuxKart Touch - live input hardware watcher
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

#ifndef HEADER_INPUT_HOTPLUG_HPP
#define HEADER_INPUT_HOTPLUG_HPP

/**
 * \brief Watches the input hardware while the game runs.
 *
 * The touch controls are a policy over hardware: Auto mode shows them on a
 * touch-only device and hides them once a real keyboard is there. Hardware
 * changes while the game is open -- a Type Cover clicks on, a Bluetooth
 * keyboard pairs, a convertible folds into a tablet -- so the policy has to be
 * re-evaluated live, in menus and mid-race, not once at startup.
 *
 * Linux polls /proc/bus/input/devices and the SW_TABLET_MODE switch once a
 * second (SDL2 has no keyboard hot-plug event). Android is told by the
 * activity through InputManager.InputDeviceListener. When the answer flips,
 * the multitouch device and race HUD are created or torn down in place and a
 * toast says what happened.
 */
namespace InputHotplug
{
    /** Called every frame from the main loop. Cheap: the actual scan runs
     *  about once a second. */
    void update(float dt);

    /** Re-read the hardware now and apply the result, e.g. after the touch
     *  settings dialog changed the policy. */
    void refresh(bool announce);

    /** Whether a keyboard the player can type on is attached right now. */
    bool hasHardwareKeyboard();

    /** Whether a touchscreen is present right now. */
    bool hasTouchscreen();

    /** Android: the activity's view of the keyboard, delivered from the UI
     *  thread. Picked up by the next update() on the game thread. */
    void setAndroidHardwareKeyboard(bool present);
}

#endif
