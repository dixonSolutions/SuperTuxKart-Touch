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

#ifndef HEADER_RACE_GUI_MULTITOUCH_HPP
#define HEADER_RACE_GUI_MULTITOUCH_HPP

#include <irrString.h>
#include <rect.h>
#include <vector2d.h>

namespace irr
{
    namespace video
    {
        class ITexture;
    }
}
using namespace irr;

class AbstractKart;
class MultitouchDevice;
class RaceGUIBase;
struct MultitouchButton;

class RaceGUIMultitouch
{
private:
    RaceGUIBase* m_race_gui;
    MultitouchDevice* m_device;
    
    bool m_gui_action;
    bool m_is_spectator_mode;
    unsigned int m_height;
    
    video::ITexture* m_steering_wheel_tex;
    video::ITexture* m_steering_wheel_tex_mask_up;
    video::ITexture* m_steering_wheel_tex_mask_down;
    video::ITexture* m_accelerator_tex;
    video::ITexture* m_accelerator_handle_tex;
    video::ITexture* m_pause_tex;
    video::ITexture* m_nitro_tex;
    video::ITexture* m_nitro_empty_tex;
    video::ITexture* m_wing_mirror_tex;
    video::ITexture* m_thunderbird_reset_tex;
    video::ITexture* m_drift_tex;
    video::ITexture* m_bg_button_tex;
    video::ITexture* m_bg_button_focus_tex;
    video::ITexture* m_gui_action_tex;
    video::ITexture* m_up_tex;
    video::ITexture* m_down_tex;
    video::ITexture* m_screen_tex;
    // Glass touch HUD (SuperTuxKart-Touch)
    video::ITexture* m_glass_btn_tex;
    video::ITexture* m_glass_btn_focus_tex;
    video::ITexture* m_glass_stick_base_tex;
    video::ITexture* m_glass_stick_knob_tex;
    video::ITexture* m_glass_btn_item_tex;
    video::ITexture* m_glass_btn_drift_tex;
    video::ITexture* m_glass_btn_nitro_tex;
    video::ITexture* m_glass_btn_nitro_fill_tex;
    video::ITexture* m_glass_btn_look_tex;
    video::ITexture* m_glass_btn_rescue_tex;
    video::ITexture* m_glass_stick_accel_tex;
    video::ITexture* m_glass_stick_brake_tex;
    bool m_use_glass_ui;

    /** Radius of the drawn steering stick base, in pixels. The steering hit
     *  region is much larger than this; the base is drawn wherever the thumb
     *  landed inside it. */
    int m_stick_radius;
    /** Where the stick base rests while no finger is steering. */
    int m_stick_home_x;
    int m_stick_home_y;

    void init();
    void createRaceGUI();
    void createSpectatorGUI();
    void close();
    void drawSteering(const MultitouchButton* button, const AbstractKart* kart);
    void drawActionButton(const MultitouchButton* button,
                          const AbstractKart* kart,
                          const core::recti &viewport,
                          const core::vector2df &scaling);
    static void onCustomButtonPress(unsigned int button_id, bool pressed);

public:
     RaceGUIMultitouch(RaceGUIBase* race_gui);
    ~RaceGUIMultitouch();

    void draw(const AbstractKart* kart, const core::recti &viewport,
              const core::vector2df &scaling);
                                
    unsigned int getHeight() {return m_height;}
    bool isSpectatorMode() {return m_is_spectator_mode;}
    void setGuiAction(bool enabled = true) {m_gui_action = enabled;}
    void reset();
    void recreate();
                                 
};   // RaceGUIMultitouch

#endif
