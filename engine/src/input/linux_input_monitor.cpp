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

#include "input/linux_input_monitor.hpp"

#ifdef STK_LINUX_INPUT_DETECT
#include <cerrno>
#include <linux/netlink.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#endif

namespace
{
    /** A plug-in is a burst: a USB keyboard registers two or three input
     *  devices and udev then fixes up each node's permissions, all within
     *  a few tens of milliseconds. Re-read once, after it settles. */
    const float SETTLE_TIME = 0.05f;
    /** Re-read interval when there is no event source at all. */
    const float FALLBACK_POLL_INTERVAL = 3.0f;
}

// ----------------------------------------------------------------------------
LinuxInputMonitor::LinuxInputMonitor()
    : m_epoll(-1), m_inotify(-1), m_netlink(-1), m_backend("none"),
      m_started(false), m_devices_dirty(false), m_settle(0.0f),
      m_poll_timer(0.0f), m_rescans(0)
{
}   // LinuxInputMonitor

// ----------------------------------------------------------------------------
LinuxInputMonitor::~LinuxInputMonitor()
{
    closeSources();
}   // ~LinuxInputMonitor

// ----------------------------------------------------------------------------
void LinuxInputMonitor::start()
{
    if (m_started)
        return;
    m_started = true;
#ifdef STK_LINUX_INPUT_DETECT
    openSources();
    refreshDevices(/*force*/true);
#endif
}   // start

// ----------------------------------------------------------------------------
void LinuxInputMonitor::openSources()
{
#ifdef STK_LINUX_INPUT_DETECT
    m_epoll = epoll_create1(EPOLL_CLOEXEC);
    if (m_epoll < 0)
    {
        m_backend = "procfs-poll";
        return;
    }

    m_inotify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (m_inotify >= 0)
    {
        // CREATE/DELETE: a device came or went. ATTRIB: udev applied the
        // seat ACL, so a switch node we could not open may open now.
        // Deliberately not IN_ACCESS/IN_MODIFY: those would fire on every
        // input event anyone reads.
        if (inotify_add_watch(m_inotify, "/dev/input",
                              IN_CREATE | IN_DELETE | IN_ATTRIB |
                              IN_MOVED_TO | IN_MOVED_FROM) < 0)
        {
            close(m_inotify);
            m_inotify = -1;
        }
    }
    if (m_inotify >= 0)
    {
        watchFd(m_inotify);
        m_backend = "inotify";
    }
    else
    {
        // No /dev/input in this sandbox (or no inotify): kernel uevents
        // still say when an input device registers or goes.
        m_netlink = socket(AF_NETLINK,
                           SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                           NETLINK_KOBJECT_UEVENT);
        if (m_netlink >= 0)
        {
            struct sockaddr_nl addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.nl_family = AF_NETLINK;
            addr.nl_groups = 1; // kernel uevents
            if (bind(m_netlink, (struct sockaddr*)&addr, sizeof(addr)) < 0)
            {
                close(m_netlink);
                m_netlink = -1;
            }
        }
        if (m_netlink >= 0)
        {
            watchFd(m_netlink);
            m_backend = "netlink";
        }
        else
            m_backend = "procfs-poll";
    }
#endif
}   // openSources

// ----------------------------------------------------------------------------
void LinuxInputMonitor::closeSources()
{
#ifdef STK_LINUX_INPUT_DETECT
    if (m_inotify >= 0)
        close(m_inotify);
    if (m_netlink >= 0)
        close(m_netlink);
    if (m_epoll >= 0)
        close(m_epoll);
#endif
    m_inotify = m_netlink = m_epoll = -1;
}   // closeSources

// ----------------------------------------------------------------------------
void LinuxInputMonitor::watchFd(int fd)
{
#ifdef STK_LINUX_INPUT_DETECT
    if (m_epoll < 0 || fd < 0)
        return;
    struct epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &ev) < 0 && errno == EEXIST)
        epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, &ev);
#else
    (void)fd;
#endif
}   // watchFd

// ----------------------------------------------------------------------------
void LinuxInputMonitor::unwatchFd(int fd)
{
#ifdef STK_LINUX_INPUT_DETECT
    if (m_epoll >= 0 && fd >= 0)
        epoll_ctl(m_epoll, EPOLL_CTL_DEL, fd, NULL);
#else
    (void)fd;
#endif
}   // unwatchFd

// ----------------------------------------------------------------------------
bool LinuxInputMonitor::drainInotify()
{
    bool relevant = false;
#ifdef STK_LINUX_INPUT_DETECT
    alignas(struct inotify_event) char buf[4096];
    while (true)
    {
        const ssize_t n = read(m_inotify, buf, sizeof(buf));
        if (n <= 0)
            break;
        for (ssize_t off = 0; off < n;)
        {
            const struct inotify_event* ev =
                (const struct inotify_event*)(buf + off);
            // Only the evdev nodes matter; js*, mouse*, by-id/ and by-path/
            // change together with them.
            if ((ev->mask & IN_Q_OVERFLOW) ||
                (ev->len > 0 && std::strncmp(ev->name, "event", 5) == 0))
                relevant = true;
            off += (ssize_t)sizeof(struct inotify_event) + ev->len;
        }
    }
#endif
    return relevant;
}   // drainInotify

// ----------------------------------------------------------------------------
bool LinuxInputMonitor::drainNetlink()
{
    bool relevant = false;
#ifdef STK_LINUX_INPUT_DETECT
    char buf[8192];
    while (true)
    {
        const ssize_t n = recv(m_netlink, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
            break;
        buf[n] = 0;
        // "add@/devices/...\0ACTION=add\0...\0SUBSYSTEM=input\0..."
        for (ssize_t off = 0; off < n;)
        {
            const char* field = buf + off;
            if (std::strcmp(field, "SUBSYSTEM=input") == 0)
            {
                relevant = true;
                break;
            }
            off += (ssize_t)std::strlen(field) + 1;
        }
    }
#endif
    return relevant;
}   // drainNetlink

// ----------------------------------------------------------------------------
bool LinuxInputMonitor::drainSwitch(int fd, bool* gone)
{
    bool relevant = false;
    *gone = false;
#ifdef STK_LINUX_INPUT_DETECT
    struct input_event evs[64];
    while (true)
    {
        const ssize_t n = read(fd, evs, sizeof(evs));
        if (n < 0)
        {
            if (errno == ENODEV)
                *gone = true;
            break;
        }
        if (n == 0)
            break;
        const size_t count = (size_t)n / sizeof(struct input_event);
        for (size_t i = 0; i < count; i++)
        {
            if ((evs[i].type == EV_SW && evs[i].code == SW_TABLET_MODE) ||
                (evs[i].type == EV_SYN && evs[i].code == SYN_DROPPED))
                relevant = true;
        }
    }
#else
    (void)fd;
#endif
    return relevant;
}   // drainSwitch

// ----------------------------------------------------------------------------
bool LinuxInputMonitor::refreshDevices(bool force)
{
#ifdef STK_LINUX_INPUT_DETECT
    const std::string text = LinuxTouchDetect::readProcBusInput();
    LinuxTouchDetect::TabletSwitch& sw = LinuxTouchDetect::tabletSwitch();
    if (!force && text == m_proc_text && !sw.missing())
        return false;
    m_rescans++;
    if (force || text != m_proc_text)
    {
        m_proc_text = text;
        m_devices = LinuxTouchDetect::parseProcBusInput(text);
    }
    std::vector<int> opened;
    sw.sync(LinuxTouchDetect::tabletSwitchNodes(m_devices), &opened, NULL);
    if (force)
    {
        // The switch may have been opened before we had an epoll set (an
        // early supportsTouchDevice() call); make sure all are watched.
        opened = sw.fds();
    }
    for (size_t i = 0; i < opened.size(); i++)
        watchFd(opened[i]);

    const LinuxTouchDetect::Snapshot s =
        LinuxTouchDetect::interpret(m_devices, sw.engaged(), sw.found());
    const bool changed = s != m_snapshot || force;
    m_snapshot = s;
    LinuxTouchDetect::store(s);
    return changed;
#else
    (void)force;
    return false;
#endif
}   // refreshDevices

// ----------------------------------------------------------------------------
bool LinuxInputMonitor::refreshSwitch()
{
    LinuxTouchDetect::TabletSwitch& sw = LinuxTouchDetect::tabletSwitch();
    const bool engaged = sw.engaged();
    if (engaged == m_snapshot.m_tablet_mode &&
        sw.found() == m_snapshot.m_has_tablet_switch)
        return false;
    // Tablet mode feeds into interpret() (Ubuntu Touch and chassis rules do
    // not depend on it, but keep one code path).
    m_snapshot = LinuxTouchDetect::interpret(m_devices, engaged, sw.found());
    LinuxTouchDetect::store(m_snapshot);
    return true;
}   // refreshSwitch

// ----------------------------------------------------------------------------
bool LinuxInputMonitor::poll(float dt)
{
#ifdef STK_LINUX_INPUT_DETECT
    if (!m_started)
        start();

    bool switch_dirty = false;
    if (m_epoll >= 0)
    {
        struct epoll_event evs[8];
        const int n = epoll_wait(m_epoll, evs, 8, 0);
        for (int i = 0; i < n; i++)
        {
            const int fd = evs[i].data.fd;
            if (fd == m_inotify)
            {
                if (drainInotify())
                {
                    m_devices_dirty = true;
                    m_settle = 0.0f;
                }
            }
            else if (fd == m_netlink)
            {
                if (drainNetlink())
                {
                    m_devices_dirty = true;
                    m_settle = 0.0f;
                }
            }
            else
            {
                LinuxTouchDetect::TabletSwitch& sw =
                    LinuxTouchDetect::tabletSwitch();
                bool gone = false;
                if (drainSwitch(fd, &gone))
                    switch_dirty = true;
                if (gone || (evs[i].events & (EPOLLERR | EPOLLHUP)))
                {
                    unwatchFd(fd);
                    sw.drop(fd);
                    switch_dirty = true;
                    m_devices_dirty = true;
                    m_settle = 0.0f;
                }
                else if (!sw.owns(fd))
                    unwatchFd(fd);
            }
        }
    }

    bool changed = false;
    if (m_devices_dirty)
    {
        m_settle += dt;
        if (m_settle >= SETTLE_TIME)
        {
            m_devices_dirty = false;
            changed = refreshDevices(false) || changed;
            switch_dirty = false;
        }
    }
    else if (m_inotify < 0 && m_netlink < 0)
    {
        m_poll_timer += dt;
        if (m_poll_timer >= FALLBACK_POLL_INTERVAL)
        {
            m_poll_timer = 0.0f;
            changed = refreshDevices(false) || changed;
            // Without an epoll set the switch cannot wake us either.
            if (m_epoll < 0)
                switch_dirty = true;
        }
    }
    if (switch_dirty)
        changed = refreshSwitch() || changed;
    return changed;
#else
    (void)dt;
    return false;
#endif
}   // poll
