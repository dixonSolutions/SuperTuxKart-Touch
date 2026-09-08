#ifndef HEADER_LINUX_TOUCH_DETECT_HPP
#define HEADER_LINUX_TOUCH_DETECT_HPP

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

#if defined(__linux__) && !defined(ANDROID)
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#endif

/** Linux /proc, /dev/input and DMI helpers for touchscreen vs keyboard.
 *  Used by Irrlicht SDL (supportsTouchDevice) and STK (touch-only policy,
 *  live hot-plug watcher). Everything here is header-only so the Irrlicht
 *  library and the game share one implementation. */
namespace LinuxTouchDetect
{
    const int KEY_A_BIT = 30;
    const int ABS_MT_POSITION_X_BIT = 53;
    const int INPUT_PROP_DIRECT_BIT = 1;

    /** One reading of the input hardware. */
    struct Snapshot
    {
        /** A direct-touch input device (touchscreen) exists. */
        bool m_touch;
        /** A real alphabetic keyboard exists (virtual and button-only
         *  devices are ignored). */
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

    /** Devices that advertise KEY_A without being a keyboard anyone types on:
     *  buttons, media controls, and the virtual keyboards that remappers
     *  (keyd, ydotool, xdotool, uinput tools) keep permanently plugged in. */
    inline bool ignoredKeyboardName(const char* name)
    {
        if (!name || !name[0])
            return true;
        return containsI(name, "power button") ||
               containsI(name, "sleep button") ||
               containsI(name, "lid switch") ||
               containsI(name, "video bus") ||
               containsI(name, "gpio-keys") ||
               containsI(name, "headset") ||
               containsI(name, "hdmi") ||
               containsI(name, "sof-hda") ||
               containsI(name, "consumer control") ||
               containsI(name, "tablet mode") ||
               containsI(name, "keyd") ||
               containsI(name, "virtual") ||
               containsI(name, "uinput") ||
               containsI(name, "ydotool") ||
               containsI(name, "xdotool") ||
               containsI(name, "wlroots") ||
               containsI(name, "remote desktop");
    }

    inline bool bitmapHasBit(const char* hex, unsigned bit)
    {
        unsigned long words[32];
        int n = 0;
        int word_bits = 32;
        const char* p = hex;
        std::memset(words, 0, sizeof(words));
        while (p && *p && n < 32)
        {
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
                p++;
            if (!*p)
                break;
            const char* start = p;
            char* end = NULL;
            words[n] = std::strtoul(p, &end, 16);
            if (end == p)
                break;
            if ((int)(end - start) > 8)
                word_bits = 64;
            n++;
            p = end;
        }
        if (n == 0)
            return false;
        const unsigned word_from_low = bit / (unsigned)word_bits;
        const unsigned bit_in_word = bit % (unsigned)word_bits;
        const int idx = n - 1 - (int)word_from_low;
        if (idx < 0 || idx >= n)
            return false;
        return (words[idx] & (1UL << bit_in_word)) != 0;
    }

    /** Linux input bus ids that mean "plugged in by the player". */
    const unsigned INPUT_BUS_USB_ID = 0x03;
    const unsigned INPUT_BUS_BLUETOOTH_ID = 0x05;

    /** The whole of /proc/bus/input/devices. About 0.1 ms; safe to read
     *  every second, and its text changing is the only reason to touch
     *  /dev/input, which is hundreds of times more expensive. */
    inline std::string readProcBusInput()
    {
        std::string out;
        FILE* f = std::fopen("/proc/bus/input/devices", "r");
        if (!f)
            return out;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
            out.append(buf, n);
        std::fclose(f);
        return out;
    }

    inline void scanProcBusInput(bool* has_touch, bool* has_keyboard,
                                 bool* has_external_keyboard = NULL)
    {
        FILE* f = std::fopen("/proc/bus/input/devices", "r");
        if (!f)
            return;
        char line[512];
        char name[256];
        unsigned prop = 0;
        unsigned bus = 0;
        bool key_a = false;
        bool abs_mt = false;
        name[0] = 0;
        while (std::fgets(line, sizeof(line), f))
        {
            if (std::strncmp(line, "I: Bus=", 7) == 0)
                bus = (unsigned)std::strtoul(line + 7, NULL, 16);
            else if (std::strncmp(line, "N: Name=\"", 9) == 0)
            {
                name[0] = 0;
                std::sscanf(line, "N: Name=\"%255[^\"]\"", name);
            }
            else if (std::strncmp(line, "B: PROP=", 8) == 0)
                prop = (unsigned)std::strtoul(line + 8, NULL, 16);
            else if (std::strncmp(line, "B: KEY=", 7) == 0)
                key_a = bitmapHasBit(line + 7, KEY_A_BIT);
            else if (std::strncmp(line, "B: ABS=", 7) == 0)
                abs_mt = bitmapHasBit(line + 7, ABS_MT_POSITION_X_BIT);
            else if (line[0] == '\n' || line[0] == '\r' || line[0] == 0)
            {
                if ((prop & (1u << INPUT_PROP_DIRECT_BIT)) ||
                    containsI(name, "touchscreen"))
                    *has_touch = true;
                else if (abs_mt && !(prop & 1u) && containsI(name, "touch"))
                    *has_touch = true;
                if (key_a && !ignoredKeyboardName(name))
                {
                    *has_keyboard = true;
                    if (has_external_keyboard &&
                        (bus == INPUT_BUS_USB_ID || bus == INPUT_BUS_BLUETOOTH_ID))
                        *has_external_keyboard = true;
                }
                name[0] = 0;
                prop = 0;
                bus = 0;
                key_a = false;
                abs_mt = false;
            }
        }
        std::fclose(f);
    }

    /** The SW_TABLET_MODE switch devices, opened once and kept.
     *
     *  Opening every /dev/input node costs about 0.4 s on a Surface (the
     *  IPTS virtual devices are slow to open), which is a visible hitch if
     *  done per poll. The switch fds are found once and re-found only when
     *  the procfs device list changes; reading their state is one ioctl,
     *  well under a microsecond. Flatpak needs --device=input for any of
     *  this; without it there is simply no switch. */
    class TabletSwitch
    {
    public:
        TabletSwitch() : m_scanned(false) {}
        ~TabletSwitch() { closeAll(); }

        /** Walk /dev/input once and keep the fds that have the switch. */
        void rescan()
        {
            closeAll();
            m_scanned = true;
#if defined(__linux__) && !defined(ANDROID)
            DIR* dir = opendir("/dev/input");
            if (!dir)
                return;
            struct dirent* ent;
            while ((ent = readdir(dir)) != NULL)
            {
                if (std::strncmp(ent->d_name, "event", 5) != 0)
                    continue;
                std::string path = std::string("/dev/input/") + ent->d_name;
                int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
                if (fd < 0)
                    continue;
                unsigned long caps[(SW_MAX + 1 + 8 * sizeof(long) - 1) /
                                   (8 * sizeof(long))];
                std::memset(caps, 0, sizeof(caps));
                if (ioctl(fd, EVIOCGBIT(EV_SW, sizeof(caps)), caps) >= 0 &&
                    (caps[SW_TABLET_MODE / (8 * sizeof(long))] &
                     (1UL << (SW_TABLET_MODE % (8 * sizeof(long))))))
                    m_fds.push_back(fd);
                else
                    close(fd);
            }
            closedir(dir);
#endif
        }

        bool scanned() const { return m_scanned; }
        bool found() const { return !m_fds.empty(); }

        /** Current state from the kept fds. A device that went away makes
         *  the ioctl fail; the caller rescans on the next list change. */
        bool engaged() const
        {
#if defined(__linux__) && !defined(ANDROID)
            for (size_t i = 0; i < m_fds.size(); i++)
            {
                unsigned long state[(SW_MAX + 1 + 8 * sizeof(long) - 1) /
                                    (8 * sizeof(long))];
                std::memset(state, 0, sizeof(state));
                if (ioctl(m_fds[i], EVIOCGSW(sizeof(state)), state) >= 0 &&
                    (state[SW_TABLET_MODE / (8 * sizeof(long))] &
                     (1UL << (SW_TABLET_MODE % (8 * sizeof(long))))))
                    return true;
            }
#endif
            return false;
        }

    private:
        void closeAll()
        {
#if defined(__linux__) && !defined(ANDROID)
            for (size_t i = 0; i < m_fds.size(); i++)
                close(m_fds[i]);
#endif
            m_fds.clear();
        }
        std::vector<int> m_fds;
        bool m_scanned;
    };

    /** Process-wide switch reader; see TabletSwitch. */
    inline TabletSwitch& tabletSwitch()
    {
        static TabletSwitch s;
        return s;
    }

    /** Read the SW_TABLET_MODE switch through the kept fds, scanning
     *  /dev/input the first time only.
     *  \param found Set true when at least one device has the switch.
     *  \return true when tablet mode is engaged on any such device. */
    inline bool readTabletMode(bool* found)
    {
        TabletSwitch& sw = tabletSwitch();
        if (!sw.scanned())
            sw.rescan();
        *found = sw.found();
        return sw.engaged();
    }

    inline int chassisType()
    {
        FILE* f = std::fopen("/sys/class/dmi/id/chassis_type", "r");
        if (!f)
            return -1;
        int type = -1;
        if (std::fscanf(f, "%d", &type) != 1)
            type = -1;
        std::fclose(f);
        return type;
    }

    inline bool osReleaseHas(const char* needle)
    {
        FILE* f = std::fopen("/etc/os-release", "r");
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
        return false;
    }

    inline bool isUbuntuTouch()
    {
        if (osReleaseHas("Ubuntu Touch") || osReleaseHas("UBUNTU_TOUCH") ||
            osReleaseHas("VARIANT_ID=touch") || osReleaseHas("lomiri"))
            return true;
#ifndef _WIN32
        if (access("/usr/share/ubports", F_OK) == 0)
            return true;
#endif
        const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
        if (desktop && (containsI(desktop, "Lomiri") || containsI(desktop, "Unity8")))
            return true;
        const char* click = std::getenv("CLICK_FRAMEWORK");
        return click && click[0];
    }

    /** Take one full reading of the hardware. Cheap enough to run once a
     *  second: one small procfs read and a handful of ioctls. */
    inline Snapshot snapshot()
    {
        Snapshot s;
        scanProcBusInput(&s.m_touch, &s.m_keyboard, &s.m_external_keyboard);
        s.m_tablet_mode = readTabletMode(&s.m_has_tablet_switch);
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

    inline bool hasTouchscreen()
    {
        return snapshot().m_touch;
    }

    inline bool hasHardwareKeyboard()
    {
        return snapshot().usableKeyboard();
    }

    inline bool isTouchOnly()
    {
        const Snapshot s = snapshot();
        return s.m_touch && !s.usableKeyboard();
    }
}

#endif
