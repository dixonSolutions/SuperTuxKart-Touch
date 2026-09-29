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

#ifndef SERVER_ONLY // No GUI files in server builds
#ifndef __HEADER_OPTIONS_SCREEN_UPDATES_HPP__
#define __HEADER_OPTIONS_SCREEN_UPDATES_HPP__

#include <string>

#include "guiengine/screen.hpp"
#include "utils/touch_update_status.hpp"

namespace GUIEngine { class Widget; }

/**
  * \brief Update management: which build is installed, how far behind it is,
  *        and what to do about it.
  * \ingroup states_screens
  */
class OptionsScreenUpdates : public GUIEngine::Screen,
                             public GUIEngine::ScreenSingleton<OptionsScreenUpdates>
{
    OptionsScreenUpdates();

    /** Last status read from the platform layer. */
    TouchUpdate::Status m_status;

    /** Seconds until the next read. The status file is written by another
     *  process, so there is nothing to subscribe to -- but re-reading it every
     *  frame would be a file open per frame for a value that moves once a
     *  second at most. */
    float m_poll_timer;

    /** Auto-update value last handed to the platform layer, so a player
     *  flipping the checkbox is reported exactly once. */
    int m_published_auto;

    /** Set when a request could not be written, so the screen can say so
     *  rather than leaving a button that silently does nothing. */
    bool m_request_failed;

    void refresh();

public:
    friend class GUIEngine::ScreenSingleton<OptionsScreenUpdates>;

    /** \brief implement callback from parent class GUIEngine::Screen */
    virtual void loadedFromFile() OVERRIDE;

    /** \brief implement callback from parent class GUIEngine::Screen */
    virtual void eventCallback(GUIEngine::Widget* widget, const std::string& name,
                               const int playerID) OVERRIDE;

    /** \brief implement callback from parent class GUIEngine::Screen */
    virtual void init() OVERRIDE;

    /** \brief implement callback from parent class GUIEngine::Screen */
    virtual void tearDown() OVERRIDE;

    /** \brief implement callback from parent class GUIEngine::Screen */
    virtual void onUpdate(float dt) OVERRIDE;

    /** \brief implement optional callback from parent class GUIEngine::Screen */
    virtual void unloaded() OVERRIDE;
};

#endif
#endif // ifndef SERVER_ONLY
