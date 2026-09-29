//  SuperTuxKart - a fun racing game with go-kart
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
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#ifndef HEADER_TOUCH_UPDATE_STATUS_HPP
#define HEADER_TOUCH_UPDATE_STATUS_HPP

#include <irrString.h>

#include <string>

/**
 * \brief The game's half of the Android self-updater.
 *
 * STKUpdateChecker can only ask its question once, at launch, in a dialog that
 * is gone before anyone is racing. Everything a player wants afterwards --
 * which build am I on, how far behind, check again, stop asking -- belongs in
 * Options, and Options is C++ while the updater is Java.
 *
 * Rather than reach across that with JNI, the two halves pass line-based files
 * through the user config directory. The Java side owns update-status.txt and
 * only writes it; this side owns update-request.txt and only writes that.
 * Neither reads its own file back, so there is no shared state to race over --
 * a torn read costs one stale second and the screen re-reads on a timer.
 *
 * Xonotic Touch runs the identical contract between its Java updater and its
 * QuakeC menu; the file format is described the same way in both projects, in
 * this header and in that project's touch_update_util.qh. Keep them in step.
 */
namespace TouchUpdate
{
    /** Bump only alongside the Java writer's FORMAT. */
    const int FORMAT = 1;

    enum State
    {
        /** No check has run yet this session. */
        STATE_IDLE,
        /** A check is in flight. */
        STATE_CHECKING,
        /** Checked, and this is the newest build there is. */
        STATE_UPTODATE,
        /** A newer release exists and can be installed from here. */
        STATE_AVAILABLE,
        /** Streaming the new build. */
        STATE_DOWNLOADING,
        /** Handed to the system installer; it owns the screen now. */
        STATE_INSTALLING,
        /** Downloaded, but Android will not install without permission. */
        STATE_NEEDS_PERMISSION,
        /** The last check or install failed; m_message says why. */
        STATE_ERROR,
        /** A package manager owns updates here -- show the version, offer no install. */
        STATE_MANAGED
    };

    struct Status
    {
        State       m_state;
        std::string m_installed;
        std::string m_latest;
        int         m_behind;
        int         m_percent;
        std::string m_message;
        bool        m_auto_update;
        /** False when no platform layer has published anything at all. */
        bool        m_published;

        Status()
            : m_state(STATE_IDLE), m_behind(0), m_percent(0),
              m_auto_update(true), m_published(false)
        {
        }

        /** True while the platform layer is working -- check, download, install. */
        bool isBusy() const
        {
            return m_state == STATE_CHECKING || m_state == STATE_DOWNLOADING ||
                   m_state == STATE_INSTALLING;
        }

        /** False where a package manager owns updates and an in-app install
         *  would be both wrong and impossible. */
        bool canSelfInstall() const { return m_state != STATE_MANAGED; }

        /** True when a newer build is known to exist. */
        bool isBehind() const
        {
            return m_behind > 0 || m_state == STATE_AVAILABLE ||
                   m_state == STATE_DOWNLOADING || m_state == STATE_INSTALLING ||
                   m_state == STATE_NEEDS_PERMISSION;
        }
    };

    /** Read the status file. Safe when it is absent, truncated or a format we
     *  do not recognise -- all three come back as an unpublished Status rather
     *  than a guess at the version, which is the one thing worse than
     *  "unknown". On a platform with no updater behind that file at all, that
     *  Status is STATE_MANAGED, because that is the true answer there. */
    Status read();

    /** Leave one token for the platform layer: check, install, skip, auto-on,
     *  auto-off. False when it could not be written, so a caller can say so
     *  instead of leaving a button that silently does nothing. */
    bool request(const std::string& token);

    /** "SuperTuxKart Touch 1.0.19", or a plain fallback before anything is
     *  published. stringw because that is what the widgets take, and because
     *  these strings are translated. */
    irr::core::stringw versionLine(const Status& status);

    /** "4 versions behind", "Up to date", "Checking..." -- one short line. */
    irr::core::stringw shortStatus(const Status& status);
}

#endif
