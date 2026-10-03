//
//  SuperTuxKart Touch - Ubuntu Touch display wake lock
//  Copyright (C) 2026 SuperTuxKart-Touch contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_UBUNTU_TOUCH_SCREEN_HPP
#define HEADER_UBUNTU_TOUCH_SCREEN_HPP

/** Keeps the phone display awake while the game runs.
 *
 *  SDL asks org.freedesktop.ScreenSaver to inhibit blanking. Ubuntu Touch's
 *  AppArmor confinement denies that call, so under Lomiri the screen dims and
 *  sleeps mid-race (issue #5). The "keep-display-on" policy group the click
 *  manifest already requests grants a different interface instead —
 *  com.canonical.Unity.Screen on the system bus — which is what this uses.
 *
 *  Both calls are no-ops off Ubuntu Touch: everywhere else SDL's own inhibit
 *  works and must stay in charge.
 */
namespace UbuntuTouchScreen
{
    void keepDisplayOn();
    void releaseDisplay();
}   // namespace UbuntuTouchScreen

#endif
