//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2014-2015 SuperTuxKart-Team
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

#include "states_screens/race_gui_multitouch.hpp"

using namespace irr;

#include <algorithm>
#include <cmath>

#include "config/user_config.hpp"
#include "graphics/camera/camera.hpp"
#include "graphics/camera/camera_debug.hpp"
#include "graphics/2dutils.hpp"
#include "graphics/irr_driver.hpp"
#include "graphics/material.hpp"
#include "guiengine/scalable_font.hpp"
#include "input/device_manager.hpp"
#include "input/multitouch_device.hpp"
#include "items/powerup.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/controller/kart_control.hpp"
#include "karts/kart_properties.hpp"
#include "network/protocols/client_lobby.hpp"
#include "states_screens/race_gui_base.hpp"
#include "utils/log.hpp"

#include <IrrlichtDevice.h>

//-----------------------------------------------------------------------------
/** The multitouch GUI constructor
 */
RaceGUIMultitouch::RaceGUIMultitouch(RaceGUIBase* race_gui)
{
    m_race_gui = race_gui;
    m_gui_action = false;
    m_is_spectator_mode = false;
    m_height = 0;
    m_steering_wheel_tex = NULL;
    m_steering_wheel_tex_mask_up = NULL;
    m_steering_wheel_tex_mask_down = NULL;
    m_accelerator_tex = NULL;
    m_accelerator_handle_tex = NULL;
    m_pause_tex = NULL;
    m_nitro_tex = NULL;
    m_nitro_empty_tex = NULL;
    m_wing_mirror_tex = NULL;
    m_thunderbird_reset_tex = NULL;
    m_drift_tex = NULL;
    m_bg_button_tex = NULL;
    m_bg_button_focus_tex = NULL;
    m_gui_action_tex = NULL;
    m_up_tex = NULL;
    m_down_tex = NULL;
    m_screen_tex = NULL;
    m_glass_btn_tex = NULL;
    m_glass_btn_focus_tex = NULL;
    m_glass_stick_base_tex = NULL;
    m_glass_stick_knob_tex = NULL;
    m_glass_btn_item_tex = NULL;
    m_glass_btn_drift_tex = NULL;
    m_glass_btn_nitro_tex = NULL;
    m_glass_btn_nitro_fill_tex = NULL;
    m_glass_btn_look_tex = NULL;
    m_glass_btn_rescue_tex = NULL;
    m_glass_stick_accel_tex = NULL;
    m_glass_stick_brake_tex = NULL;
    m_stick_radius = 0;
    m_stick_home_x = 0;
    m_stick_home_y = 0;
#ifdef TOUCH_STK
    m_use_glass_ui = true;
#else
    m_use_glass_ui = false;
#endif

    m_device = input_manager->getDeviceManager()->getMultitouchDevice();

    init();
}   // RaceGUIMultitouch

//-----------------------------------------------------------------------------
/** The multitouch GUI destructor
 */
RaceGUIMultitouch::~RaceGUIMultitouch()
{
    close();
}   // ~RaceGUIMultitouch

//-----------------------------------------------------------------------------
/** Sets the multitouch race gui to its initial state
 */
void RaceGUIMultitouch::reset()
{
    if (m_device != NULL)
    {
        m_device->reset();
    }
}   // reset

//-----------------------------------------------------------------------------
/** Recreate multitouch race gui when config was changed
 */
void RaceGUIMultitouch::recreate()
{
    close();
    reset();
    init();
}   // recreate


//-----------------------------------------------------------------------------
/** Clears all previously created buttons in the multitouch device
 */
void RaceGUIMultitouch::close()
{
    if (m_device)
    {
        m_device->clearButtons();

        if (m_device->isAccelerometerActive())
        {
            m_device->deactivateAccelerometer();
        }

        if (m_device->isGyroscopeActive())
        {
            m_device->deactivateGyroscope();
        }
    }
}   // close

//-----------------------------------------------------------------------------
/** Initializes multitouch race gui
 */
void RaceGUIMultitouch::init()
{
    if (UserConfigParams::m_multitouch_scale > 1.6f)
    {
        UserConfigParams::m_multitouch_scale = 1.6f;
    }
    else if (UserConfigParams::m_multitouch_scale < 0.8f)
    {
        UserConfigParams::m_multitouch_scale = 0.8f;
    }
    
    m_steering_wheel_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                                  "android/steering_wheel.png");
    m_accelerator_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                               "android/accelerator.png");
    m_accelerator_handle_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                               "android/accelerator_handle.png");
    m_pause_tex = irr_driver->getTexture(FileManager::GUI_ICON, "android/pause.png");
    m_nitro_tex = irr_driver->getTexture(FileManager::GUI_ICON, "android/nitro.png");
    m_nitro_empty_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                                     "android/nitro_empty.png");
    m_wing_mirror_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                                     "android/wing_mirror.png");
    m_thunderbird_reset_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                               "android/thunderbird_reset.png");
    m_drift_tex = irr_driver->getTexture(FileManager::GUI_ICON, "android/drift.png");
    m_bg_button_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                                  "android/blur_bg_button.png");
    m_bg_button_focus_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                            "android/blur_bg_button_focus.png");
    m_gui_action_tex = irr_driver->getTexture(FileManager::GUI_ICON,"challenge.png");
    m_up_tex = irr_driver->getTexture(FileManager::GUI_ICON, "up.png");
    m_down_tex = irr_driver->getTexture(FileManager::GUI_ICON, "down.png");
    m_screen_tex = irr_driver->getTexture(FileManager::GUI_ICON, "screen_other.png");
    m_steering_wheel_tex_mask_up = irr_driver->getTexture(FileManager::GUI_ICON,
                                        "android/steering_wheel_mask_up.png");
    m_steering_wheel_tex_mask_down = irr_driver->getTexture(FileManager::GUI_ICON,
                                        "android/steering_wheel_mask_down.png");

    m_glass_btn_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                             "android/glass_btn.png");
    m_glass_btn_focus_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                                   "android/glass_btn_focus.png");
    m_glass_stick_base_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                                    "android/glass_stick_base.png");
    m_glass_stick_knob_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                                    "android/glass_stick_knob.png");
    m_glass_btn_item_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                                  "android/glass_btn_item.png");
    m_glass_btn_drift_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                                   "android/glass_btn_drift.png");
    m_glass_btn_nitro_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                                   "android/glass_btn_nitro.png");
    m_glass_btn_look_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                                  "android/glass_btn_look.png");
    m_glass_btn_nitro_fill_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                            "android/glass_btn_nitro_fill.png");
    m_glass_btn_rescue_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                                "android/glass_btn_rescue.png");
    m_glass_stick_accel_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                            "android/glass_stick_accel.png");
    m_glass_stick_brake_tex = irr_driver->getTexture(FileManager::GUI_ICON,
                                            "android/glass_stick_brake.png");
    // Prefer dedicated glass art; fall back to existing translucent android plates
    // so the stick HUD still activates on TOUCH_STK builds.
    if (!m_glass_btn_tex)
        m_glass_btn_tex = m_bg_button_tex;
    if (!m_glass_btn_focus_tex)
        m_glass_btn_focus_tex = m_bg_button_focus_tex;
    if (!m_glass_stick_base_tex)
        m_glass_stick_base_tex = m_bg_button_tex;
    if (!m_glass_stick_knob_tex)
        m_glass_stick_knob_tex = m_bg_button_focus_tex;
    if (!m_glass_btn_item_tex)
        m_glass_btn_item_tex = m_glass_btn_tex;
    if (!m_glass_btn_drift_tex)
        m_glass_btn_drift_tex = m_glass_btn_tex;
    if (!m_glass_btn_nitro_tex)
        m_glass_btn_nitro_tex = m_glass_btn_tex;
    if (!m_glass_btn_look_tex)
        m_glass_btn_look_tex = m_glass_btn_tex;
    if (!m_glass_btn_rescue_tex)
        m_glass_btn_rescue_tex = m_glass_btn_tex;
    if (!m_glass_btn_nitro_fill_tex)
        m_glass_btn_nitro_fill_tex = m_glass_btn_focus_tex;
    if (m_use_glass_ui && m_glass_stick_base_tex && m_glass_stick_knob_tex)
        Log::info("RaceGUIMultitouch", "Glass touch HUD enabled.");
    else
    {
        Log::warn("RaceGUIMultitouch", "Glass touch HUD unavailable; classic widgets.");
        m_use_glass_ui = false;
    }

    auto cl = LobbyProtocol::get<ClientLobby>();
    
    if (cl && cl->isSpectator())
    {
        createSpectatorGUI();
        m_is_spectator_mode = true;
    }
    else
    {
        createRaceGUI();
    }
}

//-----------------------------------------------------------------------------
/** Determines the look of multitouch race GUI interface
 */
void RaceGUIMultitouch::createRaceGUI()
{
    if (m_device == NULL)
        return;
        
    if (UserConfigParams::m_multitouch_controls == MULTITOUCH_CONTROLS_ACCELEROMETER)
    {
        m_device->activateAccelerometer();
    }
    if (UserConfigParams::m_multitouch_controls == MULTITOUCH_CONTROLS_GYROSCOPE)
    {
        m_device->activateAccelerometer();
        m_device->activateGyroscope();
    }

    const float scale = UserConfigParams::m_multitouch_scale;

    int w = irr_driver->getActualScreenSize().Width;
    if (w - irr_driver->getDevice()->getRightPadding() > 0)
        w -= irr_driver->getDevice()->getRightPadding();

    const int h = irr_driver->getActualScreenSize().Height;

    float left_padding = 0.0f;
    if (irr_driver->getDevice()->getLeftPadding() > 0)
        left_padding = irr_driver->getDevice()->getLeftPadding();

    // Layout tokens; see docs/TOUCH_UI_DESIGN.md section 3. Everything derives
    // from screen height so the HUD is resolution independent.
    const float unit = h * scale;
    const float btn = 0.115f * unit;
    const float margin = 0.05f * unit;
    const float arc_inner = 1.75f * btn;
    const float arc_outer = arc_inner + 1.10f * btn;

    const bool inverted = UserConfigParams::m_multitouch_inverted;
    // The action cluster is laid out as an arc around the thumb's pivot rather
    // than as a grid, so every button costs the same reach and the most used one
    // sits at the thumb's neutral rotation.
    const float pivot_x = inverted ? margin + btn / 2 + left_padding
                                   : w - margin - btn / 2;
    const float pivot_y = h - margin - btn / 2;
    const float mirror = inverted ? -1.0f : 1.0f;

    // Topmost pixel the touch controls reach. RaceGUI uses getHeight() to keep
    // the minimap and the kart position icons clear of the touch HUD, so it has
    // to describe the real extent of the cluster, not just the bottom row.
    float hud_top = (float)h;

    auto place = [&](MultitouchButtonType type, float radius, float degrees,
                     float size)
    {
        const float radians = degrees * 3.14159265f / 180.0f;
        const float cx = pivot_x + mirror * radius * cosf(radians);
        const float cy = pivot_y - radius * sinf(radians);
        m_device->addButton(type, int(cx - size / 2), int(cy - size / 2),
                            int(size), int(size));
        hud_top = std::min(hud_top, cy - size / 2);
    };

    const bool tilt_controls =
        UserConfigParams::m_multitouch_controls == MULTITOUCH_CONTROLS_ACCELEROMETER ||
        UserConfigParams::m_multitouch_controls == MULTITOUCH_CONTROLS_GYROSCOPE;

    if (tilt_controls)
    {
        // Tilt steers, so the left side only needs an accelerate/brake pedal.
        const float pedal_w = 0.09f * unit;
        const float pedal_h = 0.34f * unit;
        const float pedal_x = inverted ? w - margin - pedal_w
                                       : margin + left_padding;
        m_device->addButton(BUTTON_UP_DOWN, int(pedal_x),
                            int(h - margin - pedal_h),
                            int(pedal_w), int(pedal_h));
        hud_top = std::min(hud_top, h - margin - pedal_h);
    }
    else
    {
        // The steering hit region covers the whole lower corner; the stick
        // graphic materialises under the thumb wherever it lands inside it.
        m_stick_radius = (int)(0.15f * unit);
        const int region_w = (int)(w * 0.44f - left_padding);
        const int region_h = (int)(h * 0.66f);
        const int region_x = inverted ? w - region_w : (int)left_padding;
        const int region_y = h - region_h;
        m_device->addButton(BUTTON_STEERING, region_x, region_y,
                            region_w, region_h);
        // Full lock well before the edge of the base, so a comfortable thumb
        // sweep is enough to reach it.
        m_device->setSteeringRadius((int)(0.70f * m_stick_radius));

        m_stick_home_x = inverted
            ? region_x + region_w - (int)margin - m_stick_radius
            : region_x + (int)margin + m_stick_radius;
        m_stick_home_y = h - (int)margin - m_stick_radius;
        hud_top = std::min(hud_top, (float)(m_stick_home_y - m_stick_radius));
    }

    // Inner arc: the three actions used constantly during a lap. Drift sits at
    // the neutral thumb rotation and is the largest target.
    place(BUTTON_FIRE,            arc_inner, 180.0f, 1.00f * btn);
    place(BUTTON_SKIDDING,        arc_inner, 135.0f, 1.15f * btn);
    place(BUTTON_NITRO,           arc_inner,  90.0f, 1.00f * btn);
    // Outer arc: reachable but a deliberate stretch. Look back neighbours fire
    // because the two are used together to shoot backwards; rescue is kept away
    // from every primary button because triggering it by accident costs a race.
    place(BUTTON_LOOK_BACKWARDS,  arc_outer, 157.5f, 0.72f * btn);
    place(BUTTON_RESCUE,          arc_outer, 112.5f, 0.72f * btn);

    // Pause lives in the top strip, clear of the kart position icons that
    // drawGlobalPlayerIcons owns on the left edge.
    const float pause_size = 0.62f * btn;
    const float pause_x = inverted ? w - 0.3f * h - pause_size : 0.3f * h;
    m_device->addButton(BUTTON_ESCAPE, int(pause_x), int(0.5f * margin),
                        int(pause_size), int(pause_size));

    m_height = (unsigned int)std::max(0.0f, h - hud_top);

    Log::info("RaceGUIMultitouch",
              "Layout: screen %dx%d scale %.2f btn %.0f margin %.0f "
              "pivot %.0f,%.0f stick r %d home %d,%d hud height %u",
              w, h, scale, btn, margin, pivot_x, pivot_y,
              m_stick_radius, m_stick_home_x, m_stick_home_y, m_height);
} // createRaceGUI

//-----------------------------------------------------------------------------
/** Determines the look of spectator GUI interface
 */
void RaceGUIMultitouch::createSpectatorGUI()
{
    if (m_device == NULL)
        return;
        
    const float scale = UserConfigParams::m_multitouch_scale;

    const int h = irr_driver->getActualScreenSize().Height;
    const float btn_size = 0.125f * h * scale;
    const float margin = 0.075f * h * scale;
    const float margin_top = 0.3f * h;

    const float small_ratio = 0.75f;
    const float btn_small_size = small_ratio * btn_size;
    const float margin_small = small_ratio * margin;
    
    m_height = (unsigned int)(btn_size + 2 * margin);
    
    m_device->addButton(BUTTON_ESCAPE,
                        int(margin_top), int(margin_small),
                        int(btn_small_size), int(btn_small_size));
                        
    m_device->addButton(BUTTON_CUSTOM,
                    int(margin), int(h - margin - btn_size),
                    int(btn_size), int(btn_size), onCustomButtonPress);
    
    m_device->addButton(BUTTON_CUSTOM,
                    int(margin * 2 + btn_size), int(h - margin - btn_size),
                    int(btn_size), int(btn_size), onCustomButtonPress);
                    
    m_device->addButton(BUTTON_CUSTOM,
                    int(margin * 3 + btn_size * 2), int(h - margin - btn_size),
                    int(btn_size), int(btn_size), onCustomButtonPress);

    m_device->addButton(BUTTON_CUSTOM,
                    int(margin * 4 + btn_size * 3), int(h - margin - btn_size),
                    int(btn_size), int(btn_size), onCustomButtonPress);
} // createSpectatorGUI

//-----------------------------------------------------------------------------
/** Callback function when custom button is pressed
 */
void RaceGUIMultitouch::onCustomButtonPress(unsigned int button_id,
                                            bool pressed)
{
    if (!pressed)
        return;
        
    auto cl = LobbyProtocol::get<ClientLobby>();
    
    if (!cl || !cl->isSpectator())
        return;

    switch (button_id)
    {
    case 1:
        cl->changeSpectateTarget(PA_STEER_LEFT, Input::MAX_VALUE,
                                 Input::IT_KEYBOARD);
        break;
    case 2:
        cl->changeSpectateTarget(PA_STEER_RIGHT, Input::MAX_VALUE,
                                 Input::IT_KEYBOARD);
        break;
    case 3:
        cl->changeSpectateTarget(PA_LOOK_BACK, Input::MAX_VALUE,
                                 Input::IT_KEYBOARD);
        break;
    case 4:
        cl->changeSpectateTarget(PA_ACCEL, Input::MAX_VALUE,
                                 Input::IT_KEYBOARD);
        break;
    }
}

//-----------------------------------------------------------------------------
/** Draws the buttons for multitouch race GUI.
 *  \param kart The kart for which to show the data.
 *  \param viewport The viewport to use.
 *  \param scaling Which scaling to apply to the buttons.
 */
/** Draws the steering stick.
 *  The hit region is the whole lower corner of the screen; the base is drawn
 *  where the thumb landed, and the knob shows the deflection from there.
 */
void RaceGUIMultitouch::drawSteering(const MultitouchButton* button,
                                     const AbstractKart* kart)
{
#ifndef SERVER_ONLY
    const core::position2d<s32> pos_zero = core::position2d<s32>(0, 0);
    const bool glass = m_use_glass_ui && m_glass_stick_base_tex &&
                       m_glass_stick_knob_tex && m_stick_radius > 0;

    // Keep the base fully on screen even when the thumb lands near an edge. The
    // knob is placed relative to the drawn base rather than to the raw contact
    // point, so the two never drift apart.
    int base_x = button->pressed ? button->origin_x : m_stick_home_x;
    int base_y = button->pressed ? button->origin_y : m_stick_home_y;
    const int radius = glass ? m_stick_radius
                             : std::min(button->width, button->height) / 2;
    base_x = core::clamp(base_x, button->x + radius,
                         button->x + button->width - radius);
    base_y = core::clamp(base_y, button->y + radius,
                         button->y + button->height - radius);

    const core::rect<s32> base_pos(base_x - radius, base_y - radius,
                                   base_x + radius, base_y + radius);

    if (!glass)
    {
        video::SColor color((unsigned)-1);
        core::rect<s32> coords(pos_zero, m_steering_wheel_tex->getSize());
        const float rotation =
            (button->axis_y >= 0 ? -1 : 1) * button->axis_x;
        draw2DImageRotationColor(m_steering_wheel_tex, base_pos, coords, NULL,
                                 rotation, color);
        if (kart)
        {
            const float accel = kart->getControls().getAccel();
            core::rect<s32> mask_coords(pos_zero,
                                    m_steering_wheel_tex_mask_up->getSize());
            color.setAlpha(core::clamp(
                (int)(accel >= 0.0f ? accel * 128.0f : 0), 0, 255));
            draw2DImageRotationColor(m_steering_wheel_tex_mask_up, base_pos,
                                     mask_coords, NULL, rotation, color);
            color.setAlpha(kart->getControls().getBrake() ? 128 : 0);
            draw2DImageRotationColor(m_steering_wheel_tex_mask_down, base_pos,
                                     mask_coords, NULL, rotation, color);
        }
        return;
    }

    core::rect<s32> base_coords(pos_zero, m_glass_stick_base_tex->getSize());
    draw2DImage(m_glass_stick_base_tex, base_pos, base_coords, NULL,
                video::SColor(255, 255, 255, 255), true);

    // Auto-acceleration makes the stick's vertical axis invisible otherwise:
    // the arcs are the only cue that pulling down brakes.
    if (kart != NULL)
    {
        const float accel = kart->getControls().getAccel();
        if (m_glass_stick_accel_tex != NULL && accel > 0.01f)
        {
            core::rect<s32> coords(pos_zero,
                                   m_glass_stick_accel_tex->getSize());
            const int alpha = core::clamp((int)(accel * 210.0f), 0, 210);
            draw2DImage(m_glass_stick_accel_tex, base_pos, coords, NULL,
                        video::SColor(alpha, 255, 255, 255), true);
        }
        if (m_glass_stick_brake_tex != NULL && kart->getControls().getBrake())
        {
            core::rect<s32> coords(pos_zero,
                                   m_glass_stick_brake_tex->getSize());
            draw2DImage(m_glass_stick_brake_tex, base_pos, coords, NULL,
                        video::SColor(225, 255, 255, 255), true);
        }
    }

    const int knob_radius = (int)(radius * 0.42f);
    const float travel = 0.54f * radius;
    const float knob_x = base_x + core::clamp(button->axis_x, -1.0f, 1.0f) * travel;
    const float knob_y = base_y + core::clamp(button->axis_y, -1.0f, 1.0f) * travel;
    core::rect<s32> knob_pos((int)(knob_x - knob_radius),
                             (int)(knob_y - knob_radius),
                             (int)(knob_x + knob_radius),
                             (int)(knob_y + knob_radius));
    core::rect<s32> knob_coords(pos_zero, m_glass_stick_knob_tex->getSize());
    const video::SColor knob_color = button->pressed
        ? video::SColor(255, 180, 230, 255) : video::SColor(200, 255, 255, 255);
    draw2DImage(m_glass_stick_knob_tex, knob_pos, knob_coords, NULL,
                knob_color, true);
#endif
} // drawSteering

//-----------------------------------------------------------------------------
/** Draws the buttons for multitouch race GUI.
 *  \param kart The kart for which to show the data.
 *  \param viewport The viewport to use.
 *  \param scaling Which scaling to apply to the buttons.
 */
void RaceGUIMultitouch::draw(const AbstractKart* kart,
                             const core::recti &viewport,
                             const core::vector2df &scaling)
{
#ifndef SERVER_ONLY
    if (m_device == NULL)
        return;

    for (unsigned int i = 0; i < m_device->getButtonsCount(); i++)
    {
        MultitouchButton* button = m_device->getButton(i);

        core::rect<s32> btn_pos(button->x, button->y, button->x + button->width,
                                button->y + button->height);

        const core::position2d<s32> pos_zero = core::position2d<s32>(0,0);

        if (button->type == MultitouchButtonType::BUTTON_STEERING)
        {
            const AbstractKart* steering_kart = kart;
            if (steering_kart == NULL)
            {
                Camera* c = Camera::getActiveCamera();
                if (c)
                    steering_kart = c->getKart();
            }
            drawSteering(button, steering_kart);
            continue;
        }
        if (button->type == MultitouchButtonType::BUTTON_UP_DOWN)
        {
            video::ITexture* btn_texture = m_accelerator_tex;
            core::rect<s32> coords(pos_zero, btn_texture->getSize());
            draw2DImage(btn_texture, btn_pos, coords, NULL, NULL, true);
            AbstractKart* k = NULL;
            Camera* c = Camera::getActiveCamera();
            if (c)
                k = c->getKart();
            if (k)
            {
                float upper_corner;
                if (k->getControls().getBrake())
                {
                    upper_corner = button->y + button->height - button->width / 2;
                }
                else
                {
                    upper_corner = button->y + button->height / 2 - button->width / 4;
                    upper_corner -= (int)((float)(button->height / 2 - button->width / 4) * (k->getControls().getAccel()));
                }
                core::rect<s32> handle_pos(button->x, upper_corner, button->x + button->width / 2,
                                           upper_corner + button->width / 2);
                core::rect<s32> handle_coords(pos_zero, m_accelerator_handle_tex->getSize());
                draw2DImage(m_accelerator_handle_tex, handle_pos, handle_coords, NULL, NULL, true);
            }
        }
        else
        {
            drawActionButton(button, kart, viewport, scaling);
        }
    }
#endif
} // draw

//-----------------------------------------------------------------------------
/** Draws one action button: plate, availability state, icon and any overlay.
 *  Buttons keep their position, size and accent colour for the whole race; only
 *  opacity, fill and icon change, so the player's muscle memory stays valid.
 */
void RaceGUIMultitouch::drawActionButton(const MultitouchButton* button,
                                         const AbstractKart* kart,
                                         const core::recti &viewport,
                                         const core::vector2df &scaling)
{
#ifndef SERVER_ONLY
    const core::position2d<s32> pos_zero = core::position2d<s32>(0, 0);
    const core::rect<s32> btn_pos(button->x, button->y,
                                  button->x + button->width,
                                  button->y + button->height);

    bool available = true;
    video::ITexture* icon = NULL;
    video::ITexture* plate = m_glass_btn_tex;
    // Fraction of the plate filled from the bottom, used for the nitro level.
    float fill = 0.0f;

    switch (button->type)
    {
    case MultitouchButtonType::BUTTON_ESCAPE:
        icon = m_pause_tex;
        break;
    case MultitouchButtonType::BUTTON_FIRE:
    {
        plate = m_glass_btn_item_tex;
        const Powerup* powerup = kart->getPowerup();
        if (m_gui_action == true)
        {
            icon = m_gui_action_tex;
        }
        else if (powerup->getType() != PowerupManager::POWERUP_NOTHING
                 && !kart->hasFinishedRace())
        {
            icon = powerup->getIcon()->getTexture();
        }
        else
        {
            // Empty slot: the plate stays so the cluster never reflows, but it
            // dims and loses its icon.
            available = false;
        }
        break;
    }
    case MultitouchButtonType::BUTTON_NITRO:
    {
        plate = m_glass_btn_nitro_tex;
        available = kart->getEnergy() > 0;
        icon = available ? m_nitro_tex : m_nitro_empty_tex;
        const float nitro_max = kart->getKartProperties()->getNitroMax();
        if (nitro_max > 0.0f)
            fill = core::clamp(kart->getEnergy() / nitro_max, 0.0f, 1.0f);
        break;
    }
    case MultitouchButtonType::BUTTON_LOOK_BACKWARDS:
        plate = m_glass_btn_look_tex;
        icon = m_wing_mirror_tex;
        break;
    case MultitouchButtonType::BUTTON_RESCUE:
        plate = m_glass_btn_rescue_tex;
        icon = m_thunderbird_reset_tex;
        break;
    case MultitouchButtonType::BUTTON_SKIDDING:
        plate = m_glass_btn_drift_tex;
        icon = m_drift_tex;
        break;
    case MultitouchButtonType::BUTTON_CUSTOM:
        if (button->id == 1)
            icon = m_up_tex;
        else if (button->id == 2)
            icon = m_down_tex;
        else if (button->id == 3)
            icon = m_wing_mirror_tex;
        else if (button->id == 4)
            icon = m_screen_tex;
        break;
    default:
        return;
    }

    if (!m_use_glass_ui)
    {
        // Classic upstream widgets: nothing is drawn for an unavailable button.
        if (icon == NULL)
            return;
        video::ITexture* btn_bg = (available && button->pressed) ?
                                  m_bg_button_focus_tex : m_bg_button_tex;
        const core::rect<s32> btn_pos_bg(
            (int)(button->x - button->width * 0.2f),
            (int)(button->y - button->height * 0.2f),
            (int)(button->x + button->width * 1.2f),
            (int)(button->y + button->height * 1.2f));
        core::rect<s32> coords_bg(pos_zero, btn_bg->getSize());
        draw2DImage(btn_bg, btn_pos_bg, coords_bg, NULL, NULL, true);
        core::rect<s32> coords(pos_zero, icon->getSize());
        draw2DImage(icon, btn_pos, coords, NULL, NULL, true);

        if (button->type == MultitouchButtonType::BUTTON_NITRO &&
            m_race_gui != NULL)
        {
            float meter_scale = UserConfigParams::m_multitouch_scale *
                (float)(irr_driver->getActualScreenSize().Height) / 760.0f;
            m_race_gui->drawEnergyMeter(int(button->x + button->width * 1.15f),
                                        int(button->y + button->height * 1.15f),
                                        kart, viewport,
                                        core::vector2df(meter_scale, meter_scale));
        }
        return;
    }

    if (plate == NULL)
        plate = m_bg_button_tex;

    // The plate is drawn inside the hit rectangle, never outside it: the player
    // must never be able to tap something that looks like a button but is not.
    const int plate_alpha = available ? 255 : 120;
    core::rect<s32> plate_coords(pos_zero, plate->getSize());
    draw2DImage(plate, btn_pos, plate_coords, NULL,
                video::SColor(plate_alpha, 255, 255, 255), true);

    // Nitro level rises inside the button. Both the destination and the source
    // rectangle are cropped to the same fraction, so the art is not squashed.
    if (fill > 0.0f && m_glass_btn_nitro_fill_tex != NULL)
    {
        const int filled_h = (int)(button->height * fill);
        const core::dimension2du& tex_size = m_glass_btn_nitro_fill_tex->getSize();
        const int filled_tex_h = (int)(tex_size.Height * fill);
        core::rect<s32> fill_pos(button->x, button->y + button->height - filled_h,
                                 button->x + button->width,
                                 button->y + button->height);
        core::rect<s32> fill_coords(0, tex_size.Height - filled_tex_h,
                                    tex_size.Width, tex_size.Height);
        draw2DImage(m_glass_btn_nitro_fill_tex, fill_pos, fill_coords, NULL,
                    video::SColor(255, 255, 255, 255), true);
    }

    // Pressed state brightens the plate but keeps its accent colour, so a held
    // button is still identifiable at a glance.
    if (available && button->pressed && m_glass_btn_focus_tex != NULL)
    {
        core::rect<s32> focus_coords(pos_zero, m_glass_btn_focus_tex->getSize());
        draw2DImage(m_glass_btn_focus_tex, btn_pos, focus_coords, NULL,
                    video::SColor(255, 255, 255, 255), true);
    }

    if (icon != NULL)
    {
        const int inset_x = button->width / 6;
        const int inset_y = button->height / 6;
        const core::rect<s32> icon_pos(button->x + inset_x,
                                       button->y + inset_y,
                                       button->x + button->width - inset_x,
                                       button->y + button->height - inset_y);
        core::rect<s32> coords(pos_zero, icon->getSize());
        draw2DImage(icon, icon_pos, coords, NULL,
                    video::SColor(available ? 255 : 110, 255, 255, 255), true);
    }

    if (button->type == MultitouchButtonType::BUTTON_FIRE &&
        kart->getPowerup()->getNum() > 1 && !kart->hasFinishedRace() &&
        m_gui_action == false)
    {
        // Stack count in the lower right of the plate, where it does not cover
        // the powerup art the player is trying to recognise.
        gui::ScalableFont* font = GUIEngine::getHighresDigitFont();
        core::rect<s32> pos(button->x + button->width / 2,
                            button->y + button->height / 2,
                            button->x + button->width,
                            button->y + button->height);
        font->setScale(UserConfigParams::m_multitouch_scale);
        font->setBlackBorder(true);
        font->draw(core::stringw(kart->getPowerup()->getNum()), pos,
                   video::SColor(255, 255, 255, 255), true, true);
        font->setScale(1.0f);
        font->setBlackBorder(false);
    }
#endif
} // drawActionButton
