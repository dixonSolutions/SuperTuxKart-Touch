//  Checks what the engine reads out of /proc/bus/input/devices, and the
//  decisions built on it, against recorded listings. No engine build, no
//  display: run scripts/test-input-presence.sh. See docs/TOUCH_DETECTION.md.

#include "input/input_policy.hpp"
#include "input/linux_touch_detect.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    int g_failures = 0;
    int g_checks = 0;

    void check(bool ok, const std::string& what)
    {
        g_checks++;
        if (!ok)
        {
            g_failures++;
            std::printf("FAIL %s\n", what.c_str());
        }
    }

    std::string g_dir;

    std::vector<LinuxTouchDetect::ProcDevice> load(const char* fixture)
    {
        std::ifstream in((g_dir + "/" + fixture).c_str());
        if (!in)
        {
            std::printf("FAIL cannot read %s\n", fixture);
            g_failures++;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        return LinuxTouchDetect::parseProcBusInput(ss.str());
    }

    struct Expect
    {
        bool touch, keyboard, external, usable, pointer, tablet_switch;
    };

    void expect(const char* fixture, const LinuxTouchDetect::Snapshot& s,
                bool has_switch, const Expect& e, const char* how)
    {
        const std::string p = std::string(fixture) + " (" + how + "): ";
        check(s.m_touch == e.touch, p + "touch");
        check(s.m_keyboard == e.keyboard, p + "keyboard");
        check(s.m_external_keyboard == e.external, p + "external keyboard");
        check(s.usableKeyboard() == e.usable, p + "usable keyboard");
        check(s.m_pointer == e.pointer, p + "pointer");
        check(has_switch == e.tablet_switch, p + "tablet switch listed");
    }

    /** Parse, find the switch nodes, interpret as a plain desktop Linux
     *  (no uinput trust, not Ubuntu Touch, chassis unknown). */
    LinuxTouchDetect::Snapshot read(const char* fixture, bool tablet_mode,
                                    bool* has_switch, bool trust = false,
                                    bool ubuntu_touch = false,
                                    int chassis = -1)
    {
        const std::vector<LinuxTouchDetect::ProcDevice> d = load(fixture);
        *has_switch = !LinuxTouchDetect::tabletSwitchNodes(d).empty();
        return LinuxTouchDetect::interpretWith(d, tablet_mode, *has_switch,
                                               trust, ubuntu_touch, chassis);
    }

    void testFixtures()
    {
        bool sw = false;
        LinuxTouchDetect::Snapshot s;

        // This build box, verbatim: built-in keyboard, i2c mouse and
        // touchpad, button/hotkey devices, and keyd + ydotool uinput
        // keyboards and pointers that must not count.
        const char* laptop = "laptop-keyboard-touchpad-uinput.txt";
        s = read(laptop, false, &sw);
        expect(laptop, s, sw, {false, true, false, true, true, false},
               "default");

        const char* phone = "tablet-touch-only.txt";
        s = read(phone, false, &sw);
        expect(phone, s, sw, {true, false, false, false, false, false},
               "default");
        s = read(phone, false, &sw, false, true);
        expect(phone, s, sw, {true, false, false, false, false, false},
               "Ubuntu Touch");

        // Surface: Type Cover (USB but part of the chassis), its touchpad,
        // touchscreen, pen, tablet-mode switch.
        const char* surface = "surface-pro-type-cover.txt";
        s = read(surface, false, &sw, false, false, 32);
        expect(surface, s, sw, {true, true, false, true, true, true},
               "cover open");
        s = read(surface, true, &sw, false, false, 32);
        expect(surface, s, sw, {true, true, false, false, true, true},
               "cover folded back");

        // Our own fake-keyboard.py and a uinput mouse: not counted, unless
        // STK_INPUT_TRUST_UINPUT asks for it.
        const char* virt = "uinput-virtual-keyboard-mouse.txt";
        s = read(virt, false, &sw);
        expect(virt, s, sw, {false, false, false, false, false, false},
               "default");
        s = read(virt, false, &sw, true);
        expect(virt, s, sw, {false, true, true, true, true, false},
               "trust uinput");

        // A pad, and a pad in keyboard mode, on a tablet: neither is a
        // keyboard, a pointer or a touchscreen.
        const char* pads = "tablet-with-gamepads.txt";
        s = read(pads, false, &sw);
        expect(pads, s, sw, {true, false, false, false, false, false},
               "default");

        // A mouse on a tablet is a pointer and not a keyboard. Bluetooth LE
        // devices come through uhid (/devices/virtual/misc/) and count.
        const char* mouse = "tablet-touch-bluetooth-mouse.txt";
        s = read(mouse, false, &sw);
        expect(mouse, s, sw, {true, false, false, false, true, false},
               "default");

        const char* desk = "desktop-usb-keyboard-mouse.txt";
        s = read(desk, false, &sw);
        expect(desk, s, sw, {false, true, true, true, true, false},
               "default");
        // A tablet chassis drops built-in keyboards only; USB still counts.
        s = read(desk, false, &sw, false, false, 30);
        check(s.usableKeyboard(), "desktop on tablet chassis keeps USB kbd");
        s = read(surface, false, &sw, false, false, 30);
        check(!s.m_keyboard, "tablet chassis drops the built-in Type Cover");
    }

    void testPolicy()
    {
        using namespace InputPolicy;
        // Off / Always ignore everything.
        check(!touchControlsShown(0, true, true, CONFIRMED_TOUCH), "off");
        check(touchControlsShown(2, true, false, CONFIRMED_KEYBOARD),
              "always");
        // Auto: a touchscreen shows them; a keyboard merely being listed
        // does not hide them (only confirmation does).
        check(touchControlsShown(1, true, true, CONFIRMED_NONE),
              "auto, touchscreen, nothing used yet");
        check(!touchControlsShown(1, true, false, CONFIRMED_NONE),
              "auto, no touchscreen");
        check(!touchControlsShown(1, true, true, CONFIRMED_KEYBOARD),
              "auto, driving with a listed keyboard hides");
        check(!touchControlsShown(1, true, true, CONFIRMED_GAMEPAD),
              "auto, driving with a listed gamepad hides");
        check(touchControlsShown(1, true, true, CONFIRMED_TOUCH),
              "auto, a finger brings them back");
        check(touchControlsShown(1, false, true, CONFIRMED_KEYBOARD),
              "auto without touch-only keeps upstream 'if available'");
        // Confirmation only counts for listed devices.
        check(!confirmationCounts(false), "keys without a listed keyboard");
        check(confirmationCounts(true), "keys with a listed keyboard");
        // Screen keyboard: never with a listed keyboard.
        check(!screenKeyboardWanted(true, true, true, true),
              "screen keyboard with a listed keyboard");
        check(screenKeyboardWanted(true, false, true, false),
              "screen keyboard on a touchscreen without keyboard");
        check(screenKeyboardWanted(true, false, false, true),
              "screen keyboard for a gamepad without keyboard");
        check(!screenKeyboardWanted(true, false, false, false),
              "no screen keyboard for a mouse alone");
        check(!screenKeyboardWanted(false, false, true, false),
              "screen keyboard disabled");
    }

    void testBitmaps()
    {
        // 64-bit words, most significant first.
        check(LinuxTouchDetect::bitmapHasBit("30000 0 0 0 0", 0x110),
              "BTN_LEFT in word 4");
        check(!LinuxTouchDetect::bitmapHasBit("30000 0 0 0 0", 0x112),
              "no BTN_MIDDLE");
        check(LinuxTouchDetect::bitmapHasBit("3", 1), "REL_Y");
        check(!LinuxTouchDetect::bitmapHasBit("", 0), "empty bitmap");
    }
}

int main(int argc, char** argv)
{
    g_dir = argc > 1 ? argv[1] : "tests/input-presence/fixtures";
    testBitmaps();
    testFixtures();
    testPolicy();
    std::printf("%d/%d checks passed\n", g_checks - g_failures, g_checks);
    return g_failures == 0 ? 0 : 1;
}
