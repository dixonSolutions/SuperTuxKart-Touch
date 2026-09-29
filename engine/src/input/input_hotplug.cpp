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
#include "guiengine/engine.hpp"
#include "guiengine/message_queue.hpp"
#include "guiengine/screen_keyboard.hpp"
#include "input/device_manager.hpp"
#include "input/input_manager.hpp"
#include "input/keyboard_config.hpp"
#include "input/linux_input_monitor.hpp"
#include "input/linux_touch_detect.hpp"
#include "modes/world.hpp"
#include "states_screens/race_gui_base.hpp"
#include "utils/log.hpp"
#include "utils/string_utils.hpp"
#include "utils/time.hpp"
#include "utils/translation.hpp"

#include <IEventReceiver.h>
#include <IrrlichtDevice.h>
#include <atomic>
#include <bitset>
#include <cstdlib>
#include <deque>

#ifdef ANDROID
#include <SDL_system.h>
#include <jni.h>
#endif

namespace
{
    // ---- Last-input thresholds ---------------------------------------------
    /** How far back key presses count towards "keyboard in use". */
    const uint64_t ACTIVITY_WINDOW_MS = 2500;
    /** Distinct presses of bound keys, with no touch in between, that mean
     *  someone is driving with the keyboard rather than brushing it. */
    const size_t KEY_PRESSES_TO_HIDE = 4;
    /** Gamepad activations: pads are picked up on purpose, but a single
     *  bump of a stick lying on the sofa should not count. */
    const size_t PAD_ACTIVATIONS_TO_HIDE = 2;
    /** After the layer switches, hiding waits at least this long, so a
     *  player alternating finger and keys does not strobe the HUD. Showing
     *  on a touch is always immediate. */
    const uint64_t HIDE_DWELL_MS = 1500;

    bool g_initialised = false;
    bool g_startup_logged = false;

#if defined(STK_LINUX_INPUT_DETECT)
    LinuxInputMonitor g_monitor;
#endif

    /** Android's answer, written by the UI thread and read on the game
     *  thread. -1 = never told, 0 = no keyboard, 1 = keyboard. */
    std::atomic<int> g_android_keyboard(-1);
    int g_android_keyboard_seen = -1;

    bool g_last_keyboard = false;
    bool g_last_touch = false;

    /** The last-input layer: g_active is what the policy uses, g_latest
     *  what the events so far say; update() moves one to the other. */
    InputHotplug::ActiveInput g_active = InputHotplug::AI_NONE;
    InputHotplug::ActiveInput g_latest = InputHotplug::AI_NONE;
    uint64_t g_last_switch_ms = 0;
    std::deque<uint64_t> g_key_presses;
    std::deque<uint64_t> g_pad_activations;
    /** Keys held down, so auto-repeat is not counted as presses. */
    std::bitset<1024> g_keys_down;

    uint64_t nowMs() { return StkTime::getMonoTimeMs(); }

    void prune(std::deque<uint64_t>* q, uint64_t now)
    {
        while (!q->empty() && now - q->front() > ACTIVITY_WINDOW_MS)
            q->pop_front();
    }

    bool autoMode()
    {
        return UserConfigParams::m_multitouch_active == 1;
    }

#ifdef ANDROID
    /** Ask the activity directly. Its listener pushes changes, but the
     *  first push can happen before the natives are registered and be
     *  lost, so the game thread pulls the starting state itself. */
    int queryAndroidKeyboard()
    {
        JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
        if (!env)
            return -1;
        jobject activity = (jobject)SDL_AndroidGetActivity();
        if (!activity)
            return -1;
        int result = -1;
        jclass cls = env->GetObjectClass(activity);
        if (cls)
        {
            jmethodID mid = env->GetMethodID(cls, "queryHardwareKeyboard", "()Z");
            if (mid)
                result = env->CallBooleanMethod(activity, mid) ? 1 : 0;
            if (env->ExceptionCheck())
            {
                env->ExceptionClear();
                result = -1;
            }
            env->DeleteLocalRef(cls);
        }
        env->DeleteLocalRef(activity);
        return result;
    }
#endif

    bool keyboardNow()
    {
#if defined(ANDROID) || defined(IOS_STK)
        return g_android_keyboard_seen == 1;
#elif defined(STK_LINUX_INPUT_DETECT)
        return g_monitor.snapshot().usableKeyboard();
#else
        return true;
#endif
    }

    bool touchNow()
    {
#if defined(ANDROID) || defined(IOS_STK)
        return true;
#elif defined(STK_LINUX_INPUT_DETECT)
        if (g_monitor.snapshot().m_touch)
            return true;
        return irr_driver && irr_driver->getDevice() &&
               irr_driver->getDevice()->supportsTouchDevice();
#else
        return irr_driver && irr_driver->getDevice() &&
               irr_driver->getDevice()->supportsTouchDevice();
#endif
    }

    /** Pick up hardware news. \return true when something changed. */
    bool scan(float dt)
    {
        bool changed = false;
#if defined(STK_LINUX_INPUT_DETECT)
        changed = g_monitor.poll(dt);
#else
        (void)dt;
#endif
        const int android = g_android_keyboard.load();
        if (android >= 0 && android != g_android_keyboard_seen)
        {
            g_android_keyboard_seen = android;
            changed = true;
        }
        return changed;
    }

    void initialise()
    {
        if (g_initialised)
            return;
#if defined(STK_LINUX_INPUT_DETECT)
        g_monitor.start();
#endif
#ifdef ANDROID
        if (g_android_keyboard.load() < 0)
        {
            const int k = queryAndroidKeyboard();
            if (k >= 0)
            {
                // Only fill in if the UI thread has not spoken meanwhile.
                int expected = -1;
                g_android_keyboard.compare_exchange_strong(expected, k);
            }
        }
#endif
        scan(0.0f);
        g_last_keyboard = keyboardNow();
        g_last_touch = touchNow();
        g_initialised = true;
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

    void toast(const core::stringw& msg)
    {
        if (!msg.empty())
            MessageQueue::add(MessageQueue::MT_GENERIC, msg);
    }

    void announceHardware(bool keyboard_changed, bool keyboard,
                          bool touch_changed, bool touch, bool hud_changed)
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
        toast(msg);
    }

    void announceActivity(InputHotplug::ActiveInput a)
    {
        switch (a)
        {
        case InputHotplug::AI_TOUCH:
            //I18N: Toast shown when the player touches the screen while the
            //I18N: touch controls were hidden (e.g. a keyboard is attached)
            toast(_("Touch detected: touch controls shown"));
            break;
        case InputHotplug::AI_KEYBOARD:
            //I18N: Toast shown when the player drives with the keyboard while
            //I18N: the touch controls were shown
            toast(_("Keyboard in use: touch controls hidden"));
            break;
        case InputHotplug::AI_GAMEPAD:
            //I18N: Toast shown when the player picks up a gamepad while the
            //I18N: touch controls were shown
            toast(_("Gamepad in use: touch controls hidden"));
            break;
        default:
            break;
        }
    }

    const char* activeName(InputHotplug::ActiveInput a)
    {
        switch (a)
        {
        case InputHotplug::AI_TOUCH:    return "touch";
        case InputHotplug::AI_KEYBOARD: return "keyboard";
        case InputHotplug::AI_GAMEPAD:  return "gamepad";
        default:                        return "none";
        }
    }

    /** The screen keyboard follows the same policy; dismiss STK's own one
     *  if it should no longer be up. */
    void syncScreenKeyboard()
    {
        if (GUIEngine::ScreenKeyboard::isActive() &&
            !GUIEngine::ScreenKeyboard::shouldUseScreenKeyboard())
            GUIEngine::ScreenKeyboard::dismiss();
    }

    /** Hardware changed (or first look): re-apply, log, toast. */
    void evaluateHardware(bool announce_changes)
    {
        const bool keyboard = keyboardNow();
        const bool touch = touchNow();
        const bool keyboard_changed = keyboard != g_last_keyboard;
        const bool touch_changed = touch != g_last_touch;
        g_last_keyboard = keyboard;
        g_last_touch = touch;

        // What was plugged in or out says more than what was used before
        // it: start the last-input layer over.
        if (keyboard_changed || touch_changed)
        {
            g_active = g_latest = InputHotplug::AI_NONE;
            g_key_presses.clear();
            g_pad_activations.clear();
        }

        const bool hud_changed = apply();
        if (keyboard_changed || touch_changed || hud_changed)
        {
#if defined(STK_LINUX_INPUT_DETECT)
            const LinuxTouchDetect::Snapshot& s = g_monitor.snapshot();
            Log::info("InputHotplug",
                      "keyboard=%d (external=%d) touch=%d tablet_mode=%d "
                      "-> touch controls %s",
                      keyboard, s.m_external_keyboard, touch, s.m_tablet_mode,
                      irr_driver && irr_driver->isMultitouchEnabled() ? "on"
                                                                       : "off");
#else
            Log::info("InputHotplug", "keyboard=%d touch=%d -> touch controls %s",
                      keyboard, touch,
                      irr_driver && irr_driver->isMultitouchEnabled() ? "on"
                                                                       : "off");
#endif
            syncScreenKeyboard();
        }
        if (announce_changes && (keyboard_changed || touch_changed))
            announceHardware(keyboard_changed, keyboard, touch_changed, touch,
                             hud_changed);
    }

    /** The events asked for a different layer state: apply it. */
    void evaluateActivity()
    {
        const InputHotplug::ActiveInput previous = g_active;
        g_active = g_latest;
        g_last_switch_ms = nowMs();
        const bool hud_changed = apply();
        Log::info("InputHotplug", "last input %s -> %s: touch controls %s%s",
                  activeName(previous), activeName(g_active),
                  irr_driver && irr_driver->isMultitouchEnabled() ? "on" : "off",
                  hud_changed ? "" : " (unchanged)");
        syncScreenKeyboard();
        if (hud_changed)
            announceActivity(g_active);
    }

    /** Evidence for keyboard or gamepad driving; switch when there is
     *  enough of it and the dwell has passed. */
    void noteDriving(std::deque<uint64_t>* q, size_t needed,
                     InputHotplug::ActiveInput kind)
    {
        const uint64_t now = nowMs();
        q->push_back(now);
        prune(q, now);
        if (q->size() < needed || g_latest == kind)
            return;
        if (g_latest == InputHotplug::AI_TOUCH &&
            now - g_last_switch_ms < HIDE_DWELL_MS)
            return;
        g_latest = kind;
    }

    /** Whether a key press is driving (or menu driving) input: bound in a
     *  keyboard configuration, and not typing into a text field. */
    bool isDrivingKey(int key)
    {
        if (key <= 0 || key >= (int)g_keys_down.size())
            return false;
        // Escape is also Android's back gesture; modifiers are chords.
        if (key == irr::IRR_KEY_ESCAPE || key == irr::IRR_KEY_SHIFT ||
            key == irr::IRR_KEY_CONTROL || key == irr::IRR_KEY_MENU ||
            key == irr::IRR_KEY_LSHIFT || key == irr::IRR_KEY_RSHIFT ||
            key == irr::IRR_KEY_LCONTROL || key == irr::IRR_KEY_RCONTROL ||
            key == irr::IRR_KEY_LMENU || key == irr::IRR_KEY_RMENU)
            return false;
        if (GUIEngine::isWithinATextBox() ||
            GUIEngine::ScreenKeyboard::isActive())
            return false;
        if (!input_manager)
            return false;
        DeviceManager* dm = input_manager->getDeviceManager();
        if (!dm)
            return false;
        for (int i = 0; i < dm->getKeyboardConfigAmount(); i++)
        {
            KeyboardConfig* config = dm->getKeyboardConfig(i);
            if (config && config->isEnabled() && config->hasBindingFor(key))
                return true;
        }
        return false;
    }
}   // namespace

// ----------------------------------------------------------------------------
void InputHotplug::update(float dt)
{
    if (!g_initialised || !g_startup_logged)
    {
        initialise();
        g_startup_logged = true;
#if defined(STK_LINUX_INPUT_DETECT)
        const LinuxTouchDetect::Snapshot& s = g_monitor.snapshot();
        Log::info("InputHotplug",
                  "startup: keyboard=%d (external=%d) touch=%d tablet_mode=%d "
                  "tablet_switch=%d watcher=%s -> touch controls %s",
                  g_last_keyboard, s.m_external_keyboard, g_last_touch,
                  s.m_tablet_mode, s.m_has_tablet_switch, g_monitor.backend(),
                  irr_driver && irr_driver->isMultitouchEnabled() ? "on" : "off");
#else
        Log::info("InputHotplug",
                  "startup: keyboard=%d touch=%d -> touch controls %s",
                  g_last_keyboard, g_last_touch,
                  irr_driver && irr_driver->isMultitouchEnabled() ? "on" : "off");
#endif
        // Startup state is not news; just make the HUD agree with it.
        apply();
        return;
    }

    if (scan(dt))
        evaluateHardware(true);

    if (!autoMode())
        g_latest = AI_NONE;
    if (g_latest != g_active)
        evaluateActivity();
}   // update

// ----------------------------------------------------------------------------
void InputHotplug::refresh(bool announce_changes)
{
    initialise();
    scan(0.0f);
    g_active = g_latest = AI_NONE;
    g_key_presses.clear();
    g_pad_activations.clear();
    evaluateHardware(announce_changes);
}   // refresh

// ----------------------------------------------------------------------------
bool InputHotplug::hasHardwareKeyboard()
{
    initialise();
    return keyboardNow();
}   // hasHardwareKeyboard

// ----------------------------------------------------------------------------
bool InputHotplug::hasTouchscreen()
{
    initialise();
    return touchNow();
}   // hasTouchscreen

// ----------------------------------------------------------------------------
void InputHotplug::setAndroidHardwareKeyboard(bool present)
{
    g_android_keyboard.store(present ? 1 : 0);
}   // setAndroidHardwareKeyboard

// ----------------------------------------------------------------------------
void InputHotplug::onInputEvent(const irr::SEvent& event)
{
    if (!autoMode())
        return;
    if (event.EventType == irr::EET_TOUCH_INPUT_EVENT)
    {
        // A finger, or a stylus where the platform reports it as touch.
        // Touches SDL makes up from a mouse (Android does, by default) are
        // not someone reaching for the screen.
        if (event.TouchInput.Event != irr::ETIE_PRESSED_DOWN ||
            event.TouchInput.Simulated)
            return;
        g_key_presses.clear();
        g_pad_activations.clear();
        if (g_latest != AI_TOUCH)
            g_latest = AI_TOUCH;
    }
    else if (event.EventType == irr::EET_KEY_INPUT_EVENT)
    {
        const int key = event.KeyInput.Key;
        if (key <= 0 || key >= (int)g_keys_down.size())
            return;
        if (!event.KeyInput.PressedDown)
        {
            g_keys_down.reset(key);
            return;
        }
        // Auto-repeat of a held key is not another press.
        if (g_keys_down.test(key))
            return;
        g_keys_down.set(key);
        if (isDrivingKey(key))
            noteDriving(&g_key_presses, KEY_PRESSES_TO_HIDE, AI_KEYBOARD);
    }
    // Mouse events neither show nor hide the controls: nobody steers a kart
    // with a mouse, and SDL also makes mouse events up from touches.
}   // onInputEvent

// ----------------------------------------------------------------------------
void InputHotplug::onGamepadActivity()
{
    if (!autoMode())
        return;
    noteDriving(&g_pad_activations, PAD_ACTIVATIONS_TO_HIDE, AI_GAMEPAD);
}   // onGamepadActivity

// ----------------------------------------------------------------------------
InputHotplug::ActiveInput InputHotplug::activeInput()
{
    return autoMode() ? g_active : AI_NONE;
}   // activeInput

// ----------------------------------------------------------------------------
InputHotplug::ActiveInput InputHotplug::latestInput()
{
    return autoMode() ? g_latest : AI_NONE;
}   // latestInput
