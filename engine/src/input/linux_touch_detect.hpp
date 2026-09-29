//
//  SuperTuxKart Touch - touchscreen / keyboard presence on Linux
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

#ifndef HEADER_LINUX_TOUCH_DETECT_HPP
#define HEADER_LINUX_TOUCH_DETECT_HPP

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

/** Desktop Linux only. Android is a Linux kernel too, but there the answers
 *  come from the activity (InputManager), and /proc, /sys DMI, /dev/input and
 *  the Ubuntu Touch / Click markers are either unreadable to an app or
 *  meaningless, so none of that is compiled in: every reader below returns
 *  "nothing found" without touching the filesystem. */
#if defined(__linux__) && !defined(ANDROID)
#define STK_LINUX_INPUT_DETECT 1
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

/** Touchscreen vs keyboard, read from /proc/bus/input/devices, the
 *  SW_TABLET_MODE switch, the DMI chassis type and Ubuntu Touch markers.
 *
 *  Used by Irrlicht's SDL device (supportsTouchDevice) and by STK's touch
 *  policy and live watcher (input/input_hotplug.cpp, which keeps the cached
 *  reading here current through input/linux_input_monitor.cpp). Header-only
 *  so the Irrlicht library and the game share one implementation and one
 *  cache. */
namespace LinuxTouchDetect
{
    /** One reading of the input hardware. */
    struct Snapshot
    {
        /** A direct-touch input device (touchscreen or pen digitizer). */
        bool m_touch;
        /** A real alphabetic keyboard exists (virtual, button-only and
         *  gamepad devices are ignored). */
        bool m_keyboard;
        /** One of those keyboards is on USB or Bluetooth -- plugged in by
         *  the player rather than part of the chassis. */
        bool m_external_keyboard;
        /** The firmware reports a tablet-mode switch, and it is engaged:
         *  a detachable keyboard is detached or folded away. */
        bool m_tablet_mode;
        /** Whether a tablet-mode switch was found at all. */
        bool m_has_tablet_switch;

        Snapshot()
            : m_touch(false), m_keyboard(false), m_external_keyboard(false),
              m_tablet_mode(false), m_has_tablet_switch(false)
        {
        }

        bool operator==(const Snapshot& o) const
        {
            return m_touch == o.m_touch && m_keyboard == o.m_keyboard &&
                   m_external_keyboard == o.m_external_keyboard &&
                   m_tablet_mode == o.m_tablet_mode &&
                   m_has_tablet_switch == o.m_has_tablet_switch;
        }
        bool operator!=(const Snapshot& o) const { return !(*this == o); }

        /** A keyboard the player can actually type on right now. A tablet
         *  mode switch that says "tablet" overrides the built-in keyboard: a
         *  Surface keeps its Type Cover listed while it is folded back. A
         *  USB or Bluetooth keyboard is not part of the chassis, so it counts
         *  whatever the switch says. */
        bool usableKeyboard() const
        {
            if (m_external_keyboard)
                return true;
            return m_keyboard && !m_tablet_mode;
        }
    };

    inline bool containsI(const char* hay, const char* needle)
    {
        if (!hay || !needle || !needle[0])
            return false;
        const size_t nlen = std::strlen(needle);
        const size_t hlen = std::strlen(hay);
        if (nlen > hlen)
            return false;
        for (size_t i = 0; i + nlen <= hlen; i++)
        {
            size_t j = 0;
            for (; j < nlen; j++)
            {
                if (std::tolower((unsigned char)hay[i + j]) !=
                    std::tolower((unsigned char)needle[j]))
                    break;
            }
            if (j == nlen)
                return true;
        }
        return false;
    }

    // Linux input codes, spelled out so this header also compiles where
    // <linux/input.h> is not included (Android, other platforms).
    const unsigned KEY_Q_BIT = 16;
    const unsigned KEY_ENTER_BIT = 28;
    const unsigned KEY_A_BIT = 30;
    const unsigned KEY_Z_BIT = 44;
    const unsigned KEY_SPACE_BIT = 57;
    const unsigned BTN_JOYSTICK_BIT = 0x120;
    const unsigned BTN_GAMEPAD_BIT = 0x130;
    const unsigned ABS_MT_POSITION_X_BIT = 53;
    const unsigned SW_TABLET_MODE_BIT = 1;
    const unsigned INPUT_PROP_POINTER_BIT = 0;
    const unsigned INPUT_PROP_DIRECT_BIT = 1;

    /** Linux input bus ids that mean "plugged in by the player". */
    const unsigned INPUT_BUS_USB_ID = 0x03;
    const unsigned INPUT_BUS_BLUETOOTH_ID = 0x05;

    /** Devices that advertise letter keys without being a keyboard anyone
     *  types on: buttons, media controls, vendor hotkey drivers, and the
     *  virtual keyboards that remappers (keyd, ydotool, xdotool, uinput
     *  tools) keep permanently plugged in. A controller that also exposes a
     *  keyboard interface (the Steam Deck's, for Steam's desktop mode) is a
     *  gamepad to the player. */
    inline bool ignoredKeyboardName(const char* name)
    {
        if (!name || !name[0])
            return true;
        static const char* const ignored[] =
        {
            "power button", "sleep button", "lid switch", "video bus",
            "gpio-keys", "gpio_keys", "headset", "hdmi", "sof-hda",
            "consumer control", "system control", "tablet mode",
            "intel hid", "wmi hotkeys", "extra buttons", "hotkeys",
            "keyd", "virtual", "uinput", "ydotool", "xdotool", "wlroots",
            "remote desktop", "steam deck", "steam controller",
            "fingerprint",
            NULL
        };
        for (int i = 0; ignored[i]; i++)
        {
            if (containsI(name, ignored[i]))
                return true;
        }
        return false;
    }

    /** Word size the kernel prints /proc/bus/input bitmaps in: its
     *  unsigned long, which is not necessarily ours (a 32-bit Click on a
     *  64-bit kernel), so ask uname once. */
    inline unsigned kernelWordBits()
    {
        static unsigned bits = 0;
        if (bits == 0)
        {
            bits = (unsigned)(sizeof(long) * 8);
#ifdef STK_LINUX_INPUT_DETECT
            struct utsname u;
            if (uname(&u) == 0 && std::strstr(u.machine, "64"))
                bits = 64;
#endif
        }
        return bits;
    }

    /** Whether a /proc/bus/input bitmap ("B: KEY=1f 0 ffff...") has a bit.
     *  Words are printed most significant first. */
    inline bool bitmapHasBit(const char* hex, unsigned bit)
    {
        unsigned long long words[32];
        int n = 0;
        unsigned word_bits = kernelWordBits();
        const char* p = hex;
        while (p && *p && n < 32)
        {
            while (*p == ' ' || *p == '\t')
                p++;
            if (!*p || *p == '\n' || *p == '\r')
                break;
            const char* start = p;
            char* end = NULL;
            words[n] = std::strtoull(p, &end, 16);
            if (end == p)
                break;
            if ((int)(end - start) > 8)
                word_bits = 64;
            n++;
            p = end;
        }
        if (n == 0)
            return false;
        const unsigned word_from_low = bit / word_bits;
        const unsigned bit_in_word = bit % word_bits;
        const int idx = n - 1 - (int)word_from_low;
        if (idx < 0 || idx >= n)
            return false;
        return (words[idx] & (1ULL << bit_in_word)) != 0;
    }

    /** One entry of /proc/bus/input/devices. */
    struct ProcDevice
    {
        unsigned m_bus;
        unsigned m_prop;
        std::string m_name;
        /** "S: Sysfs=" path, e.g. /devices/virtual/input/input14. */
        std::string m_sysfs;
        /** The evdev node from "H: Handlers=", e.g. "event6", or empty. */
        std::string m_event_node;
        std::string m_key;
        std::string m_abs;
        std::string m_sw;

        ProcDevice() : m_bus(0), m_prop(0) {}

        bool hasKey(unsigned bit) const
        {
            return !m_key.empty() && bitmapHasBit(m_key.c_str(), bit);
        }

        /** Created through /dev/uinput by a program (keyd, ydotool, Steam
         *  Input, a remote desktop, our own test scripts). Kernel drivers,
         *  and Bluetooth LE keyboards (uhid, under /devices/virtual/misc/),
         *  live elsewhere. */
        bool isUinput() const
        {
            return m_sysfs.compare(0, 23, "/devices/virtual/input/") == 0;
        }

        bool isGamepad() const
        {
            return hasKey(BTN_GAMEPAD_BIT) || hasKey(BTN_JOYSTICK_BIT);
        }

        /** A touchscreen or a pen digitizer on the screen. Touchpads and
         *  drawing tablets are pointer devices (INPUT_PROP_POINTER), not
         *  direct ones. */
        bool isTouchscreen() const
        {
            if (m_prop & (1u << INPUT_PROP_DIRECT_BIT))
                return true;
            if (m_prop & (1u << INPUT_PROP_POINTER_BIT))
                return false;
            if (containsI(m_name.c_str(), "touchpad") ||
                containsI(m_name.c_str(), "trackpad"))
                return false;
            if (containsI(m_name.c_str(), "touchscreen"))
                return true;
            return containsI(m_name.c_str(), "touch") && !m_abs.empty() &&
                   bitmapHasBit(m_abs.c_str(), ABS_MT_POSITION_X_BIT);
        }

        /** A keyboard with letters, a space bar and Enter. KEY_A alone is
         *  not enough: vendor hotkey drivers, remotes and gamepads in
         *  keyboard mode carry odd subsets of keys. */
        bool isKeyboard(bool trust_uinput) const
        {
            if (!hasKey(KEY_A_BIT) || !hasKey(KEY_Q_BIT) ||
                !hasKey(KEY_Z_BIT) || !hasKey(KEY_SPACE_BIT) ||
                !hasKey(KEY_ENTER_BIT))
                return false;
            if (isGamepad() || (m_prop & (1u << INPUT_PROP_DIRECT_BIT)))
                return false;
            if (isUinput() && !trust_uinput)
                return false;
            return !ignoredKeyboardName(m_name.c_str());
        }

        /** Plugged in by the player. A Surface Type Cover sits on USB on
         *  some models but is part of the chassis: the tablet-mode switch
         *  decides whether it is usable. */
        bool isExternal() const
        {
            if (m_bus != INPUT_BUS_USB_ID && m_bus != INPUT_BUS_BLUETOOTH_ID)
                return false;
            return !containsI(m_name.c_str(), "type cover");
        }

        bool hasTabletSwitch() const
        {
            return !m_sw.empty() &&
                   bitmapHasBit(m_sw.c_str(), SW_TABLET_MODE_BIT);
        }
    };

    /** STK_INPUT_TRUST_UINPUT=1 counts uinput keyboards as real, for
     *  testing with scripts/fake-keyboard.py. Off by default: the uinput
     *  keyboards on a normal system are remappers and remote desktops. */
    inline bool trustUinput()
    {
        static int trust = -1;
        if (trust < 0)
        {
            const char* env = std::getenv("STK_INPUT_TRUST_UINPUT");
            trust = (env && env[0] && env[0] != '0') ? 1 : 0;
        }
        return trust == 1;
    }

    /** The whole of /proc/bus/input/devices (about 0.1 ms). */
    inline std::string readProcBusInput()
    {
        std::string out;
#ifdef STK_LINUX_INPUT_DETECT
        FILE* f = std::fopen("/proc/bus/input/devices", "re");
        if (!f)
            return out;
        char buf[8192];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
            out.append(buf, n);
        std::fclose(f);
#endif
        return out;
    }

    inline std::vector<ProcDevice> parseProcBusInput(const std::string& text)
    {
        std::vector<ProcDevice> out;
        ProcDevice cur;
        bool any = false;
        size_t pos = 0;
        while (pos < text.size())
        {
            size_t eol = text.find('\n', pos);
            if (eol == std::string::npos)
                eol = text.size();
            const std::string line = text.substr(pos, eol - pos);
            pos = eol + 1;
            if (line.empty() || line == "\r")
            {
                if (any)
                    out.push_back(cur);
                cur = ProcDevice();
                any = false;
                continue;
            }
            any = true;
            const char* l = line.c_str();
            if (std::strncmp(l, "I: Bus=", 7) == 0)
                cur.m_bus = (unsigned)std::strtoul(l + 7, NULL, 16);
            else if (std::strncmp(l, "N: Name=\"", 9) == 0)
            {
                cur.m_name = line.substr(9);
                if (!cur.m_name.empty() &&
                    cur.m_name[cur.m_name.size() - 1] == '"')
                    cur.m_name.erase(cur.m_name.size() - 1);
            }
            else if (std::strncmp(l, "S: Sysfs=", 9) == 0)
                cur.m_sysfs = line.substr(9);
            else if (std::strncmp(l, "H: Handlers=", 12) == 0)
            {
                const char* ev = std::strstr(l + 12, "event");
                if (ev)
                {
                    size_t len = 5;
                    while (ev[len] >= '0' && ev[len] <= '9')
                        len++;
                    cur.m_event_node.assign(ev, len);
                }
            }
            else if (std::strncmp(l, "B: PROP=", 8) == 0)
                cur.m_prop = (unsigned)std::strtoul(l + 8, NULL, 16);
            else if (std::strncmp(l, "B: KEY=", 7) == 0)
                cur.m_key = line.substr(7);
            else if (std::strncmp(l, "B: ABS=", 7) == 0)
                cur.m_abs = line.substr(7);
            else if (std::strncmp(l, "B: SW=", 6) == 0)
                cur.m_sw = line.substr(6);
        }
        if (any)
            out.push_back(cur);
        return out;
    }

    /** The SW_TABLET_MODE switch devices, opened once and kept.
     *
     *  Which nodes carry the switch is read from /proc/bus/input/devices
     *  ("B: SW=" and "H: Handlers=eventN"), so only those few are ever
     *  opened. Walking /dev/input instead costs about 0.4 s on a Surface,
     *  whose IPTS nodes are slow to open. The live watcher reads the kept
     *  fds without blocking when the kernel has an event for them (see
     *  LinuxInputMonitor); the state itself is one EVIOCGSW ioctl. Flatpak
     *  needs --device=input for any of this; without it there is simply no
     *  switch. */
    class TabletSwitch
    {
    public:
        TabletSwitch() : m_synced(false), m_missing(false) {}
        ~TabletSwitch() { closeAll(); }

        /** Keep exactly the nodes in \p wanted open. Nodes already open stay
         *  open; the ones no longer wanted are closed.
         *  \param opened Receives fds that were newly opened.
         *  \param closed Receives fds that were closed. */
        void sync(const std::vector<std::string>& wanted,
                  std::vector<int>* opened = NULL,
                  std::vector<int>* closed = NULL)
        {
            m_synced = true;
            m_missing = false;
#ifdef STK_LINUX_INPUT_DETECT
            for (size_t i = 0; i < m_nodes.size();)
            {
                bool keep = false;
                for (size_t j = 0; j < wanted.size(); j++)
                    keep = keep || wanted[j] == m_nodes[i].m_name;
                if (keep)
                {
                    i++;
                    continue;
                }
                if (closed)
                    closed->push_back(m_nodes[i].m_fd);
                close(m_nodes[i].m_fd);
                m_nodes.erase(m_nodes.begin() + i);
            }
            for (size_t j = 0; j < wanted.size(); j++)
            {
                bool have = false;
                for (size_t i = 0; i < m_nodes.size(); i++)
                    have = have || m_nodes[i].m_name == wanted[j];
                if (have)
                    continue;
                const std::string path = "/dev/input/" + wanted[j];
                int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
                if (fd < 0)
                {
                    // Usually udev has not applied the seat ACL yet; the
                    // attribute change that follows brings us back here.
                    m_missing = true;
                    continue;
                }
                Node n;
                n.m_name = wanted[j];
                n.m_fd = fd;
                m_nodes.push_back(n);
                if (opened)
                    opened->push_back(fd);
            }
#else
            (void)wanted;
            (void)opened;
            (void)closed;
#endif
        }

        /** Forget and close one fd whose device went away. */
        void drop(int fd)
        {
            for (size_t i = 0; i < m_nodes.size(); i++)
            {
                if (m_nodes[i].m_fd != fd)
                    continue;
#ifdef STK_LINUX_INPUT_DETECT
                close(fd);
#endif
                m_nodes.erase(m_nodes.begin() + i);
                return;
            }
        }

        bool owns(int fd) const
        {
            for (size_t i = 0; i < m_nodes.size(); i++)
            {
                if (m_nodes[i].m_fd == fd)
                    return true;
            }
            return false;
        }

        bool synced() const { return m_synced; }
        bool found() const { return !m_nodes.empty(); }
        /** A switch node is listed but could not be opened (yet). */
        bool missing() const { return m_missing; }
        std::vector<int> fds() const
        {
            std::vector<int> out;
            for (size_t i = 0; i < m_nodes.size(); i++)
                out.push_back(m_nodes[i].m_fd);
            return out;
        }

        /** Current state from the kept fds: one ioctl each. */
        bool engaged() const
        {
#ifdef STK_LINUX_INPUT_DETECT
            for (size_t i = 0; i < m_nodes.size(); i++)
            {
                unsigned long state[(SW_MAX + 1 + 8 * sizeof(long) - 1) /
                                    (8 * sizeof(long))];
                std::memset(state, 0, sizeof(state));
                if (ioctl(m_nodes[i].m_fd, EVIOCGSW(sizeof(state)), state) >= 0 &&
                    (state[SW_TABLET_MODE / (8 * sizeof(long))] &
                     (1UL << (SW_TABLET_MODE % (8 * sizeof(long))))))
                    return true;
            }
#endif
            return false;
        }

    private:
        struct Node
        {
            std::string m_name;
            int m_fd;
        };

        void closeAll()
        {
#ifdef STK_LINUX_INPUT_DETECT
            for (size_t i = 0; i < m_nodes.size(); i++)
                close(m_nodes[i].m_fd);
#endif
            m_nodes.clear();
        }

        std::vector<Node> m_nodes;
        bool m_synced;
        bool m_missing;
    };

    /** Process-wide switch reader; see TabletSwitch. */
    inline TabletSwitch& tabletSwitch()
    {
        static TabletSwitch s;
        return s;
    }

    /** The evdev nodes that carry SW_TABLET_MODE. */
    inline std::vector<std::string> tabletSwitchNodes(
        const std::vector<ProcDevice>& devices)
    {
        std::vector<std::string> out;
        for (size_t i = 0; i < devices.size(); i++)
        {
            if (devices[i].hasTabletSwitch() && !devices[i].m_event_node.empty())
                out.push_back(devices[i].m_event_node);
        }
        return out;
    }

    /** SMBIOS chassis type, read once: it does not change while we run. */
    inline int chassisType()
    {
        static int type = -2;
        if (type != -2)
            return type;
        type = -1;
#ifdef STK_LINUX_INPUT_DETECT
        FILE* f = std::fopen("/sys/class/dmi/id/chassis_type", "re");
        if (f)
        {
            if (std::fscanf(f, "%d", &type) != 1)
                type = -1;
            std::fclose(f);
        }
#endif
        return type;
    }

    inline bool osReleaseHas(const char* needle)
    {
#ifdef STK_LINUX_INPUT_DETECT
        FILE* f = std::fopen("/etc/os-release", "re");
        if (!f)
            return false;
        char line[512];
        while (std::fgets(line, sizeof(line), f))
        {
            if (containsI(line, needle))
            {
                std::fclose(f);
                return true;
            }
        }
        std::fclose(f);
#else
        (void)needle;
#endif
        return false;
    }

    /** Ubuntu Touch / Lomiri / a Click package. Fixed for the life of the
     *  process, so worked out once. */
    inline bool isUbuntuTouch()
    {
        static int ut = -1;
        if (ut >= 0)
            return ut == 1;
        ut = 0;
#ifdef STK_LINUX_INPUT_DETECT
        if (osReleaseHas("Ubuntu Touch") || osReleaseHas("UBUNTU_TOUCH") ||
            osReleaseHas("VARIANT_ID=touch") || osReleaseHas("lomiri") ||
            access("/usr/share/ubports", F_OK) == 0)
            ut = 1;
        const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
        if (desktop && (containsI(desktop, "Lomiri") || containsI(desktop, "Unity8")))
            ut = 1;
        const char* click = std::getenv("CLICK_FRAMEWORK");
        if (click && click[0])
            ut = 1;
#endif
        return ut == 1;
    }

    /** Turn a device list and the switch state into a Snapshot, applying
     *  what the platform says about built-in keyboards. */
    inline Snapshot interpret(const std::vector<ProcDevice>& devices,
                              bool tablet_mode, bool has_switch)
    {
        Snapshot s;
        const bool trust = trustUinput();
        for (size_t i = 0; i < devices.size(); i++)
        {
            const ProcDevice& d = devices[i];
            if (d.isTouchscreen())
                s.m_touch = true;
            if (d.isKeyboard(trust))
            {
                s.m_keyboard = true;
                if (d.isExternal())
                    s.m_external_keyboard = true;
            }
        }
        s.m_tablet_mode = tablet_mode;
        s.m_has_tablet_switch = has_switch;
        if (isUbuntuTouch())
        {
            s.m_touch = true;
            // A phone with a keyboard paired is still a keyboard-first
            // device while it is paired; only the built-in assumption goes.
            s.m_keyboard = s.m_external_keyboard;
        }
        const int chassis = chassisType();
        // SMBIOS: 11 hand held, 30 tablet. Convertibles (31) and
        // detachables (32) are decided by the tablet-mode switch above.
        // A plugged-in keyboard counts on any chassis.
        if ((chassis == 11 || chassis == 30) && s.m_touch)
            s.m_keyboard = s.m_external_keyboard;
        return s;
    }

    /** The last reading, shared by everything in the process. */
    struct Cache
    {
        Snapshot m_snapshot;
        bool m_valid;
        Cache() : m_valid(false) {}
    };

    inline Cache& cache()
    {
        static Cache c;
        return c;
    }

    inline void store(const Snapshot& s)
    {
        cache().m_snapshot = s;
        cache().m_valid = true;
    }

    /** Take a full reading now (one procfs read; the switch fds synced and
     *  read) and remember it. */
    inline Snapshot snapshot()
    {
        Snapshot s;
#ifdef STK_LINUX_INPUT_DETECT
        const std::vector<ProcDevice> devices =
            parseProcBusInput(readProcBusInput());
        TabletSwitch& sw = tabletSwitch();
        sw.sync(tabletSwitchNodes(devices));
        s = interpret(devices, sw.engaged(), sw.found());
#endif
        store(s);
        return s;
    }

    /** The remembered reading; the first call takes one. The live watcher
     *  keeps it current, so this is what per-frame and per-event callers
     *  use: no file is read here after the first time. */
    inline const Snapshot& current()
    {
        if (!cache().m_valid)
            snapshot();
        return cache().m_snapshot;
    }

    inline bool hasTouchscreen()
    {
        return current().m_touch;
    }

    inline bool hasHardwareKeyboard()
    {
        return current().usableKeyboard();
    }

    inline bool isTouchOnly()
    {
        const Snapshot& s = current();
        return s.m_touch && !s.usableKeyboard();
    }
}

#endif
