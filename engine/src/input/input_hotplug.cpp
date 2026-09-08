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

#include "input/input_hotplug.hpp"

#include "config/user_config.hpp"
#include "graphics/irr_driver.hpp"
#include "guiengine/message_queue.hpp"
#include "guiengine/screen_keyboard.hpp"
#include "input/device_manager.hpp"
#include "input/input_manager.hpp"
#include "input/linux_touch_detect.hpp"
#include "modes/world.hpp"
#include "states_screens/race_gui_base.hpp"
#include "utils/log.hpp"
#include "utils/string_utils.hpp"
#include "utils/translation.hpp"

#include <IrrlichtDevice.h>
#include <atomic>

namespace
{
    /** Seconds between hardware scans on Linux. A scan is one small procfs
     *  read plus a few ioctls, so this is about not waking the disk cache
     *  every frame rather than about cost. */
    const float SCAN_INTERVAL = 1.0f;

    bool g_initialised = false;
    bool g_startup_logged = false;
    float g_timer = 0.0f;

    /** Last hardware reading (Linux). */
    LinuxTouchDetect::Snapshot g_snapshot;
    /** /proc/bus/input/devices as of the last full scan. Cheap to read and
     *  to compare; it changes exactly when a device comes or goes, which
     *  is the only time the expensive /dev/input walk is worth doing. */
    std::string g_proc_text;

    /** Android's answer, written by the UI thread and read on the game
     *  thread. -1 = never told, 0 = no keyboard, 1 = keyboard. */
    std::atomic<int> g_android_keyboard(-1);
    int g_android_keyboard_seen = -1;

    bool g_last_keyboard = false;
    bool g_last_touch = false;

    bool keyboardNow()
    {
#if defined(ANDROID) || defined(IOS_STK)
        return g_android_keyboard_seen == 1;
#elif defined(__linux__)
        return g_snapshot.usableKeyboard();
#else
        return true;
#endif
    }

    bool touchNow()
    {
#if defined(ANDROID) || defined(IOS_STK)
        return true;
#elif defined(__linux__)
        if (g_snapshot.m_touch)
            return true;
        return irr_driver && irr_driver->getDevice() &&
               irr_driver->getDevice()->supportsTouchDevice();
#else
        return irr_driver && irr_driver->getDevice() &&
               irr_driver->getDevice()->supportsTouchDevice();
#endif
    }

    void scan()
    {
#if defined(__linux__) && !defined(ANDROID)
        const std::string proc = LinuxTouchDetect::readProcBusInput();
        if (proc != g_proc_text || !LinuxTouchDetect::tabletSwitch().scanned())
        {
            // A device came or went: parse the list again and re-find the
            // switch fds (the device numbering may have shifted).
            g_proc_text = proc;
            LinuxTouchDetect::tabletSwitch().rescan();
            g_snapshot = LinuxTouchDetect::snapshot();
        }
        else
        {
            // Same devices: only the switch state can have moved, and that
            // is one ioctl on an fd we already hold.
            bool found = false;
            g_snapshot.m_tablet_mode = LinuxTouchDetect::readTabletMode(&found);
            g_snapshot.m_has_tablet_switch = found;
        }
#endif
        const int android = g_android_keyboard.load();
        if (android >= 0)
            g_android_keyboard_seen = android;
    }

    /** Bring the multitouch device and the race HUD in line with the policy.
     *  \return true when something changed. */
    bool apply()
    {
        if (!input_manager || !irr_driver)
            return false;
        DeviceManager* dm = input_manager->getDeviceManager();
        if (!dm)
            return false;

        const bool want = irr_driver->isMultitouchEnabled();
        const bool had = dm->getMultitouchDevice() != NULL;
        if (want == had)
            return false;

        RaceGUIBase* gui = World::getWorld() ? World::getWorld()->getRaceGUI()
                                             : NULL;
        if (want)
        {
            // Device first: the HUD caches a pointer to it.
            dm->updateMultitouchAvailability();
            if (gui)
                gui->setMultitouchEnabled(true);
        }
        else
        {
            if (gui)
                gui->setMultitouchEnabled(false);
            dm->updateMultitouchAvailability();
        }
        return true;
    }

    void announce(bool keyboard_changed, bool keyboard, bool touch_changed,
                  bool touch, bool hud_changed)
    {
        core::stringw msg;
        if (keyboard_changed && keyboard)
        {
            //I18N: Toast shown when a physical keyboard is plugged in
            msg = hud_changed ? _("Keyboard connected: touch controls hidden")
                              : _("Keyboard connected");
        }
        else if (keyboard_changed)
        {
            //I18N: Toast shown when a physical keyboard is unplugged
            msg = hud_changed ? _("Keyboard disconnected: touch controls shown")
                              : _("Keyboard disconnected");
        }
        else if (touch_changed && touch)
        {
            //I18N: Toast shown when a touchscreen appears
            msg = hud_changed ? _("Touchscreen detected: touch controls shown")
                              : _("Touchscreen detected");
        }
        else if (touch_changed)
        {
            //I18N: Toast shown when a touchscreen disappears
            msg = _("Touchscreen removed");
        }
        else
            return;
        MessageQueue::add(MessageQueue::MT_GENERIC, msg);
    }

    void evaluate(bool announce_changes)
    {
        const bool keyboard = keyboardNow();
        const bool touch = touchNow();
        const bool keyboard_changed = keyboard != g_last_keyboard;
        const bool touch_changed = touch != g_last_touch;
        g_last_keyboard = keyboard;
        g_last_touch = touch;

        const bool hud_changed = apply();
        if (keyboard_changed || touch_changed || hud_changed)
        {
            Log::info("InputHotplug",
                      "keyboard=%d touch=%d tablet_mode=%d -> touch controls %s",
                      keyboard, touch, g_snapshot.m_tablet_mode,
                      irr_driver && irr_driver->isMultitouchEnabled() ? "on"
                                                                       : "off");
            // A hardware keyboard makes STK's own on-screen keyboard
            // pointless and in the way; drop it if one is open.
            if (keyboard && GUIEngine::ScreenKeyboard::isActive() &&
                !GUIEngine::ScreenKeyboard::shouldUseScreenKeyboard())
                GUIEngine::ScreenKeyboard::dismiss();
        }
        if (announce_changes && (keyboard_changed || touch_changed))
            announce(keyboard_changed, keyboard, touch_changed, touch,
                     hud_changed);
    }
}   // namespace

// ----------------------------------------------------------------------------
void InputHotplug::update(float dt)
{
    if (!g_initialised || !g_startup_logged)
    {
        if (!g_initialised)
        {
            scan();
            g_last_keyboard = keyboardNow();
            g_last_touch = touchNow();
            g_initialised = true;
        }
        g_startup_logged = true;
        g_timer = 0.0f;
        Log::info("InputHotplug",
                  "startup: keyboard=%d (external=%d) touch=%d tablet_mode=%d "
                  "tablet_switch=%d -> touch controls %s",
                  g_last_keyboard, g_snapshot.m_external_keyboard,
                  g_last_touch, g_snapshot.m_tablet_mode,
                  g_snapshot.m_has_tablet_switch,
                  irr_driver && irr_driver->isMultitouchEnabled() ? "on" : "off");
        // Startup state is not news; just make the HUD agree with it.
        apply();
        return;
    }

    g_timer += dt;
    const int android = g_android_keyboard.load();
    const bool android_changed = android >= 0 &&
                                 android != g_android_keyboard_seen;
    if (g_timer < SCAN_INTERVAL && !android_changed)
        return;
    g_timer = 0.0f;
    scan();
    evaluate(true);
}   // update

// ----------------------------------------------------------------------------
void InputHotplug::refresh(bool announce_changes)
{
    scan();
    if (!g_initialised)
    {
        g_last_keyboard = keyboardNow();
        g_last_touch = touchNow();
        g_initialised = true;
    }
    evaluate(announce_changes);
}   // refresh

// ----------------------------------------------------------------------------
bool InputHotplug::hasHardwareKeyboard()
{
    if (!g_initialised)
    {
        scan();
        g_last_keyboard = keyboardNow();
        g_last_touch = touchNow();
        g_initialised = true;
    }
    return keyboardNow();
}   // hasHardwareKeyboard

// ----------------------------------------------------------------------------
bool InputHotplug::hasTouchscreen()
{
    if (!g_initialised)
    {
        scan();
        g_last_keyboard = keyboardNow();
        g_last_touch = touchNow();
        g_initialised = true;
    }
    return touchNow();
}   // hasTouchscreen

// ----------------------------------------------------------------------------
void InputHotplug::setAndroidHardwareKeyboard(bool present)
{
    g_android_keyboard.store(present ? 1 : 0);
}   // setAndroidHardwareKeyboard
