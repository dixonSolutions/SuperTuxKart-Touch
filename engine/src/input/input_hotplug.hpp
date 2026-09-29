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

namespace irr
{
    struct SEvent;
}

/**
 * \brief Keeps the touch controls in line with the hardware and with what
 *        the player is actually using.
 *
 * Two layers, in Auto mode (multitouch_active = 1):
 *
 *  1. Hardware presence, the starting point. Auto shows the touch controls
 *     on a touch-only device and hides them once a real keyboard is there.
 *     Hardware changes while the game is open -- a Type Cover clicks on, a
 *     Bluetooth keyboard pairs, a convertible folds -- so it is followed
 *     live, in menus and mid-race. Linux learns of it from inotify / netlink
 *     and the tablet-mode switch (LinuxInputMonitor), Android from the
 *     activity's InputManager.InputDeviceListener.
 *
 *  2. The last input actually used, on top, the way games switch their
 *     button prompts: a finger on the screen brings the controls back at
 *     once even with a keyboard attached; sustained keyboard driving (a few
 *     bound keys within a couple of seconds and no touch) or a gamepad being
 *     used puts them away. Hiding waits out a short dwell after the last
 *     switch so the two cannot flap. A hardware change resets this layer.
 *
 * When the answer flips, the multitouch device and race HUD are created or
 * torn down in place and a toast says what happened.
 */
namespace InputHotplug
{
    /** What the player last drove with, as far as the touch controls care. */
    enum ActiveInput
    {
        AI_NONE,      //!< No evidence yet, or hardware just changed.
        AI_TOUCH,     //!< A finger (or stylus) on the screen.
        AI_KEYBOARD,  //!< Sustained use of bound keyboard keys.
        AI_GAMEPAD    //!< Gamepad buttons, hat or sticks.
    };

    /** Called every frame from the main loop. Idle cost on Linux is one
     *  epoll_wait with a zero timeout. */
    void update(float dt);

    /** Re-read the hardware now and apply the result, e.g. after the touch
     *  settings dialog changed the policy. Forgets the last-input layer. */
    void refresh(bool announce);

    /** Whether a keyboard the player can type on is attached right now. */
    bool hasHardwareKeyboard();

    /** Whether a touchscreen is present right now. */
    bool hasTouchscreen();

    /** Android: the activity's view of the keyboard, delivered from the UI
     *  thread. Picked up by the next update() on the game thread. */
    void setAndroidHardwareKeyboard(bool present);

    /** Every device event, before the GUI sees it (EventHandler::OnEvent):
     *  touches and key presses feed the last-input layer. */
    void onInputEvent(const irr::SEvent& event);

    /** A gamepad button, hat or a stick pushed well past its dead zone. */
    void onGamepadActivity();

    /** The last-input layer as the touch policy uses it: AI_NONE unless in
     *  Auto mode. Changes only in update(), together with the HUD, so the
     *  policy and the multitouch device never disagree within a frame. */
    ActiveInput activeInput();

    /** The same, but including evidence from events this frame that
     *  update() has not applied yet. For decisions taken while handling
     *  that very event, such as whether a tapped text box opens STK's
     *  screen keyboard. */
    ActiveInput latestInput();
}

#endif
