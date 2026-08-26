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

#include "utils/touch_update_status.hpp"

#include "io/file_manager.hpp"
#include "utils/file_utils.hpp"
#include "utils/string_utils.hpp"
#include "utils/translation.hpp"

#include <cstdio>
#include <fstream>
#include <string>

#ifdef ANDROID
#include <SDL_system.h>
#endif

namespace
{
    const char* STATUS_FILE = "update-status.txt";
    const char* REQUEST_FILE = "update-request.txt";

    TouchUpdate::State parseState(const std::string& name)
    {
        if (name == "checking")         return TouchUpdate::STATE_CHECKING;
        if (name == "uptodate")         return TouchUpdate::STATE_UPTODATE;
        if (name == "available")        return TouchUpdate::STATE_AVAILABLE;
        if (name == "downloading")      return TouchUpdate::STATE_DOWNLOADING;
        if (name == "installing")       return TouchUpdate::STATE_INSTALLING;
        if (name == "needs-permission") return TouchUpdate::STATE_NEEDS_PERMISSION;
        if (name == "error")            return TouchUpdate::STATE_ERROR;
        if (name == "managed")          return TouchUpdate::STATE_MANAGED;
        return TouchUpdate::STATE_IDLE;
    }

    /** Where both halves agree the handshake files live.
     *
     *  On Android that is the app's files directory, which SDL reports as the
     *  internal storage path and Java knows as getFilesDir(). Going through
     *  that rather than the STK config dir is deliberate: the config dir is
     *  built from HOME plus .config/supertuxkart inside assets_android.cpp, and
     *  duplicating that derivation in Java is a second copy to get wrong. Here
     *  both sides name the same directory by construction.
     *
     *  Everywhere else there is no Java half at all -- the package manager owns
     *  updates -- so the config dir is the natural home. */
    std::string updateFilePath(const char* name)
    {
#ifdef ANDROID
        const char* files_dir = SDL_AndroidGetInternalStoragePath();
        if (files_dir != NULL)
            return std::string(files_dir) + "/" + name;
#endif
        return file_manager->getUserConfigFile(name);
    }

    /** Trim the trailing CR a file written on one platform and read on another
     *  can carry; a stray \\r turns "managed" into a state we do not know. */
    std::string trimLine(const std::string& in)
    {
        std::string out = in;
        while (!out.empty() && (out[out.size() - 1] == '\r' || out[out.size() - 1] == '\n'))
            out.erase(out.size() - 1);
        return out;
    }
}   // namespace

// ----------------------------------------------------------------------------
TouchUpdate::Status TouchUpdate::read()
{
    Status status;
#ifndef ANDROID
    // No platform layer here writes the status file, and none would serve what
    // the screen asked of it: whoever installed this build owns updating it.
    // Start from that answer, so everything we cannot read leaves it standing
    // rather than an updater the player can drive but nothing is behind.
    status.m_state = STATE_MANAGED;
#endif

    std::string path = updateFilePath(STATUS_FILE);
    std::ifstream in(FileUtils::getPortableReadingPath(path).c_str());
    if (!in.is_open())
    {
        // Nothing has published anything. Not an error: a desktop build with no
        // updater behind it looks exactly like this.
        return status;
    }

    std::string format, state, installed, latest, behind, percent, message, auto_pref;
    std::getline(in, format);
    std::getline(in, state);
    std::getline(in, installed);
    std::getline(in, latest);
    std::getline(in, behind);
    std::getline(in, percent);
    std::getline(in, message);
    std::getline(in, auto_pref);
    in.close();

    // An unrecognised format means a newer platform layer than this build knows
    // about. Stand down rather than guess at the line order -- showing
    // "unknown" is recoverable, showing the wrong version is not.
    if (atoi(trimLine(format).c_str()) != FORMAT)
        return status;

    status.m_published   = true;
    status.m_state       = parseState(trimLine(state));
    status.m_installed   = trimLine(installed);
    status.m_latest      = trimLine(latest);
    status.m_behind      = std::max(0, atoi(trimLine(behind).c_str()));
    status.m_percent     = std::min(100, std::max(0, atoi(trimLine(percent).c_str())));
    status.m_message     = trimLine(message);
    status.m_auto_update = trimLine(auto_pref) != "off";
    return status;
}   // read

// ----------------------------------------------------------------------------
bool TouchUpdate::request(const std::string& token)
{
    std::string path = updateFilePath(REQUEST_FILE);
    std::ofstream out(FileUtils::getPortableWritingPath(path).c_str(),
                      std::ios::out | std::ios::trunc);
    if (!out.is_open())
        return false;

    out << token << "\n";
    out.close();
    return !out.fail();
}   // request

// ----------------------------------------------------------------------------
irr::core::stringw TouchUpdate::versionLine(const Status& status)
{
    if (status.m_installed.empty())
        return _("SuperTuxKart Touch (development build)");
    return _("SuperTuxKart Touch %s", status.m_installed.c_str());
}   // versionLine

// ----------------------------------------------------------------------------
irr::core::stringw TouchUpdate::shortStatus(const Status& status)
{
    switch (status.m_state)
    {
    case STATE_CHECKING:
        return _("Checking for updates...");
    case STATE_DOWNLOADING:
        return _("Downloading update... %i%%", status.m_percent);
    case STATE_INSTALLING:
        return _("Installing update...");
    case STATE_NEEDS_PERMISSION:
        return _("Update ready - permission needed");
    case STATE_ERROR:
        return _("Update check failed");
    case STATE_MANAGED:
        return _("Updates managed by the system");
    default:
        break;
    }

    // Singular spelled out rather than "1 version(s)": this line is read at a
    // glance from the main menu, where the plural form is the noisy bit.
    if (status.m_behind == 1)
        return _("1 version behind");
    if (status.m_behind > 1)
        return _("%i versions behind", status.m_behind);

    if (status.m_state == STATE_AVAILABLE)
        return _("Update available");
    if (status.m_state == STATE_UPTODATE)
        return _("Up to date");
    return L"";
}   // shortStatus
