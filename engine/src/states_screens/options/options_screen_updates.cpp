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

#ifndef SERVER_ONLY

#include "states_screens/options/options_screen_updates.hpp"

#include "guiengine/screen.hpp"
#include "guiengine/widget.hpp"
#include "guiengine/widgets/button_widget.hpp"
#include "guiengine/widgets/check_box_widget.hpp"
#include "guiengine/widgets/label_widget.hpp"
#include "guiengine/widgets/progress_bar_widget.hpp"
#include "guiengine/widgets/ribbon_widget.hpp"
#include "states_screens/options/options_common.hpp"
#include "utils/touch_update_status.hpp"
#include "utils/translation.hpp"

#include <string>

using namespace GUIEngine;

namespace
{
    /** RibbonWidget has no per-item enable, only findItemNamed plus the child
     *  list, so reach through that rather than deactivating the whole bar and
     *  losing the buttons that are still usable. */
    void setRibbonItemActive(RibbonWidget* ribbon, const char* item, bool active)
    {
        const int index = ribbon->findItemNamed(item);
        if (index < 0)
            return;
        ribbon->getRibbonChildren()[index].setActive(active);
    }
}   // namespace

// ----------------------------------------------------------------------------
OptionsScreenUpdates::OptionsScreenUpdates()
    : Screen("options/options_updates.stkgui"),
      m_poll_timer(0.0f), m_published_auto(-1), m_request_failed(false)
{
}   // OptionsScreenUpdates

// ----------------------------------------------------------------------------
void OptionsScreenUpdates::loadedFromFile()
{
}   // loadedFromFile

// ----------------------------------------------------------------------------
void OptionsScreenUpdates::init()
{
    Screen::init();
    RibbonWidget* ribbon = getWidget<RibbonWidget>("options_choice");
    assert(ribbon != NULL);
    ribbon->select("tab_updates", PLAYER_ID_GAME_MASTER);
    // By name, not by index: this tab is last today, and a hardcoded position
    // silently tooltips the wrong tab the moment another one is added.
    const int tab_index = ribbon->findItemNamed("tab_updates");
    if (tab_index >= 0)
        ribbon->getRibbonChildren()[tab_index].setTooltip(_("Updates"));

    OptionsCommon::setTabStatus();

    m_status = TouchUpdate::read();
    m_published_auto = m_status.m_auto_update ? 1 : 0;
    m_request_failed = false;
    m_poll_timer = 0.0f;

    CheckBoxWidget* automatic = getWidget<CheckBoxWidget>("auto-update");
    if (automatic != NULL)
        automatic->setState(m_status.m_auto_update);

    // Opening this screen is the question a check answers, so ask it -- but not
    // where a package manager owns updates, and not on top of a check already
    // running.
    if (m_status.canSelfInstall() && !m_status.isBusy())
        TouchUpdate::request("check");

    refresh();
}   // init

// ----------------------------------------------------------------------------
void OptionsScreenUpdates::onUpdate(float dt)
{
    m_poll_timer -= dt;
    if (m_poll_timer > 0.0f)
        return;

    m_poll_timer = 1.0f;
    m_status = TouchUpdate::read();
    refresh();
}   // onUpdate

// ----------------------------------------------------------------------------
/** Re-point the labels and re-gate the buttons from the last status read. */
void OptionsScreenUpdates::refresh()
{
    LabelWidget* installed = getWidget<LabelWidget>("installed_version");
    LabelWidget* state = getWidget<LabelWidget>("update_state");
    LabelWidget* message = getWidget<LabelWidget>("update_message");
    ProgressBarWidget* progress = getWidget<ProgressBarWidget>("update_progress");

    if (installed != NULL)
        installed->setText(TouchUpdate::versionLine(m_status), false);

    if (state != NULL)
    {
        core::stringw line = TouchUpdate::shortStatus(m_status);
        if (!m_status.m_latest.empty() && m_status.m_latest != m_status.m_installed)
        {
            line += L"  -  ";
            line += _("Latest: %s", m_status.m_latest.c_str());
        }
        state->setText(line, false);
    }

    if (message != NULL)
    {
        core::stringw text;
        if (m_request_failed)
        {
            text = _("Could not reach the updater. Restart SuperTuxKart and try again.");
        }
        else if (!m_status.canSelfInstall())
        {
            text = _("This copy is installed by your package manager. Update it the same way you installed it.");
        }
        else if (m_status.m_state == TouchUpdate::STATE_NEEDS_PERMISSION)
        {
            // Worth spelling out: the installer flashing open and shut is
            // exactly what this looks like from the player's side, and it
            // reads as a bug rather than a missing permission.
            text = _("Android needs 'install unknown apps' granted to SuperTuxKart before it will finish.");
        }
        else if (!m_status.m_message.empty())
        {
            text = StringUtils::utf8ToWide(m_status.m_message);
        }
        message->setText(text, false);
    }

    if (progress != NULL)
    {
        if (m_status.m_state == TouchUpdate::STATE_DOWNLOADING)
            progress->setValue(m_status.m_percent);
        else if (m_status.m_state == TouchUpdate::STATE_INSTALLING)
            progress->setValue(100);
        else
            progress->setValue(0);
    }

    // Deactivate rather than hide. A control that vanishes mid-gesture hands
    // the press to whatever moves into its place, which on a touchscreen means
    // installing an update the player was trying not to.
    const bool managed = !m_status.canSelfInstall();
    const bool busy = m_status.isBusy();
    const bool actionable = m_status.isBehind();

    RibbonWidget* actions = getWidget<RibbonWidget>("update_actions");
    if (actions != NULL)
    {
        actions->setActive(!managed && !busy);
        setRibbonItemActive(actions, "check_now", !managed && !busy);
        setRibbonItemActive(actions, "install_update", !managed && !busy && actionable);
        setRibbonItemActive(actions, "skip_version", !managed && !busy && actionable);
    }

    CheckBoxWidget* automatic = getWidget<CheckBoxWidget>("auto-update");
    if (automatic != NULL)
        automatic->setActive(!managed);
}   // refresh

// ----------------------------------------------------------------------------
void OptionsScreenUpdates::eventCallback(Widget* widget, const std::string& name,
                                         const int playerID)
{
    if (name == "options_choice")
    {
        const std::string& selection =
            ((RibbonWidget*)widget)->getSelectionIDString(PLAYER_ID_GAME_MASTER);
        if (selection != "tab_updates")
            OptionsCommon::switchTab(selection);
    }
    else if (name == "back")
    {
        StateManager::get()->escapePressed();
    }
    else if (name == "auto-update")
    {
        CheckBoxWidget* automatic = getWidget<CheckBoxWidget>("auto-update");
        const int want = automatic->getState() ? 1 : 0;
        if (want != m_published_auto)
        {
            m_published_auto = want;
            // A flipped checkbox is only a widget until the platform layer is
            // told, which is what actually stops the launch-time prompt.
            m_request_failed = !TouchUpdate::request(want ? "auto-on" : "auto-off");
            refresh();
        }
    }
    else if (name == "update_actions")
    {
        const std::string& action =
            ((RibbonWidget*)widget)->getSelectionIDString(PLAYER_ID_GAME_MASTER);
        if (action == "check_now")
            m_request_failed = !TouchUpdate::request("check");
        else if (action == "install_update")
            m_request_failed = !TouchUpdate::request("install");
        else if (action == "skip_version")
            m_request_failed = !TouchUpdate::request("skip");

        // Show the intent at once. The platform layer publishes its own state
        // within the second, but a button that looks inert until then reads as
        // broken on a touchscreen, where there is no cursor to show a press.
        if (!m_request_failed && action == "check_now")
        {
            m_status.m_state = TouchUpdate::STATE_CHECKING;
        }
        refresh();
    }
}   // eventCallback

// ----------------------------------------------------------------------------
void OptionsScreenUpdates::tearDown()
{
    Screen::tearDown();
}   // tearDown

// ----------------------------------------------------------------------------
void OptionsScreenUpdates::unloaded()
{
}   // unloaded

#endif // ifndef SERVER_ONLY
