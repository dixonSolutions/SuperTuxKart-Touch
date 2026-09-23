//
//  SuperTuxKart Touch - event-driven input hot-plug on Linux
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

#ifndef HEADER_LINUX_INPUT_MONITOR_HPP
#define HEADER_LINUX_INPUT_MONITOR_HPP

#include "input/linux_touch_detect.hpp"

#include <string>

/**
 * \brief Tells the game, without polling, when input hardware changes.
 *
 * One epoll set holds:
 *  - an inotify watch on /dev/input: the kernel creates and removes the
 *    eventN nodes itself (devtmpfs), so a keyboard, touchscreen or cover
 *    arriving or leaving shows up here the moment it registers, and udev's
 *    permission change right after shows up as an attribute event;
 *  - or, where /dev/input is not visible (a Flatpak without
 *    --device=input), a NETLINK_KOBJECT_UEVENT socket filtered to the input
 *    subsystem;
 *  - the SW_TABLET_MODE switch nodes, so folding a convertible or clicking
 *    a cover off is an EV_SW event read the frame it happens.
 *
 * poll() is one epoll_wait(..., 0) per frame: a single syscall that returns
 * at once with nothing to do while nothing changes. /proc/bus/input/devices
 * is re-read only after one of those fds woke up (and only once for a burst
 * of them); a plain timer re-read every few seconds is kept only as the
 * fallback when neither inotify nor netlink can be had.
 *
 * Linux desktop builds only; on other platforms every method is a no-op.
 */
class LinuxInputMonitor
{
public:
    LinuxInputMonitor();
    ~LinuxInputMonitor();

    /** Take the first reading and open the event sources. Idempotent. */
    void start();

    /** Called every frame. \return true when the reading changed. */
    bool poll(float dt);

    const LinuxTouchDetect::Snapshot& snapshot() const { return m_snapshot; }

    /** Which event source is in use, for the startup log:
     *  "inotify", "netlink", "procfs-poll" or "none". */
    const char* backend() const { return m_backend; }

    /** Number of full /proc/bus/input/devices re-reads so far. */
    unsigned rescans() const { return m_rescans; }

private:
    void openSources();
    void closeSources();
    void watchFd(int fd);
    void unwatchFd(int fd);
    bool drainInotify();
    bool drainNetlink();
    /** \return true when the switch reported something. */
    bool drainSwitch(int fd, bool* gone);
    /** Re-read the device list; re-sync the switch fds if it changed. */
    bool refreshDevices(bool force);
    bool refreshSwitch();

    LinuxTouchDetect::Snapshot m_snapshot;
    std::string m_proc_text;
    std::vector<LinuxTouchDetect::ProcDevice> m_devices;
    int m_epoll;
    int m_inotify;
    int m_netlink;
    const char* m_backend;
    bool m_started;
    /** A device came or went; re-read once the burst has settled. */
    bool m_devices_dirty;
    float m_settle;
    /** Fallback polling timer, when there is no event source. */
    float m_poll_timer;
    unsigned m_rescans;
};

#endif
