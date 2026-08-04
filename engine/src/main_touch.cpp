//
//  SuperTuxKart Touch - Linux tablet / phone defaults
//  Copyright (C) 2026 SuperTuxKart-Touch contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef TOUCH_STK

#include "config/user_config.hpp"
#include "utils/log.hpp"

#include <cstdlib>
#include <cstring>

namespace
{
/** thermal (default) | balanced | quality — from STK_TOUCH_PERF or env. */
const char *touchPerfProfile()
{
    const char *env = std::getenv("STK_TOUCH_PERF");
    if (env && *env)
        return env;
    return "thermal";
}
} // namespace

/** Apply touch-first defaults after config load. */
void override_default_params_for_touch()
{
    UserConfigParams::m_multitouch_active.setDefaultValue(2);
    UserConfigParams::m_multitouch_draw_gui.setDefaultValue(true);
    UserConfigParams::m_multitouch_controls.setDefaultValue(
        MULTITOUCH_CONTROLS_STEERING_WHEEL);
    UserConfigParams::m_multitouch_auto_acceleration.setDefaultValue(true);
    UserConfigParams::m_multitouch_scale.setDefaultValue(1.15f);
    UserConfigParams::m_screen_keyboard.setDefaultValue(1);

    UserConfigParams::m_multitouch_active = 2;
    UserConfigParams::m_multitouch_draw_gui = true;
    UserConfigParams::m_screen_keyboard = 1;
    if (UserConfigParams::m_multitouch_controls == MULTITOUCH_CONTROLS_UNDEFINED)
        UserConfigParams::m_multitouch_controls = MULTITOUCH_CONTROLS_STEERING_WHEEL;

    const char *perf = touchPerfProfile();
    // Always apply thermal/balanced/quality on touch builds (tablet product).
    if (std::strcmp(perf, "quality") == 0)
    {
        UserConfigParams::m_max_fps = 60;
        UserConfigParams::m_scale_rtts_factor = 0.85f;
        UserConfigParams::m_high_definition_textures = 1;
        UserConfigParams::m_max_texture_size = 512;
        UserConfigParams::m_dynamic_lights = true;
        UserConfigParams::m_geometry_level = 2;
        UserConfigParams::m_particles_effects = 2;
        UserConfigParams::m_anisotropic = 4;
        UserConfigParams::m_shadows_resolution = 256;
        UserConfigParams::m_animated_characters = true;
    }
    else if (std::strcmp(perf, "balanced") == 0)
    {
        UserConfigParams::m_max_fps = 45;
        UserConfigParams::m_scale_rtts_factor = 0.7f;
        UserConfigParams::m_high_definition_textures = 0;
        UserConfigParams::m_max_texture_size = 256;
        UserConfigParams::m_dynamic_lights = false;
        UserConfigParams::m_geometry_level = 1;
        UserConfigParams::m_particles_effects = 1;
        UserConfigParams::m_anisotropic = 0;
        UserConfigParams::m_shadows_resolution = 0;
        UserConfigParams::m_animated_characters = true;
    }
    else
    {
        // thermal — default for fanless tablets (Surface / Ultramarine)
        UserConfigParams::m_max_fps = 30;
        UserConfigParams::m_scale_rtts_factor = 0.55f;
        UserConfigParams::m_high_definition_textures = 0;
        UserConfigParams::m_max_texture_size = 256;
        UserConfigParams::m_dynamic_lights = false;
        UserConfigParams::m_bloom = false;
        UserConfigParams::m_glow = false;
        UserConfigParams::m_light_shaft = false;
        UserConfigParams::m_dof = false;
        UserConfigParams::m_ssr = false;
        UserConfigParams::m_ssao = false;
        UserConfigParams::m_mlaa = false;
        UserConfigParams::m_motionblur = false;
        UserConfigParams::m_light_scatter = false;
        UserConfigParams::m_geometry_level = 0;
        UserConfigParams::m_particles_effects = 1;
        UserConfigParams::m_anisotropic = 0;
        UserConfigParams::m_shadows_resolution = 0;
        UserConfigParams::m_animated_characters = false;
        UserConfigParams::m_swap_interval = 1;
        UserConfigParams::m_texture_compression = true;
        UserConfigParams::m_degraded_IBL = true;
    }

    Log::info("MainTouch",
              "Touch defaults applied (multitouch GUI + screen keyboard, perf=%s).",
              perf);
}

#endif // TOUCH_STK
