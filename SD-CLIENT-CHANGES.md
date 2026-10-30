# SD-Client — what changed compared to DDNet

New components (src/game/client/components/):
sd_abovebelow, sd_actions (hook/hit), sd_fonts, sd_grenade, sd_ping, sd_playercopy,
sd_profiles, sd_toasts, sd_common.h, menus_settings_sd.cpp (settings tab).

Modified: config_variables.h (sd_* variables), gameclient.h/.cpp (component registration),
menus.h / menus_settings.cpp (new "SD-Client" tab), scoreboard.cpp/.h (copy buttons in the player popup),
engine/textrender.h + engine/client/text.cpp (custom default font API), graphics_threaded.cpp (window title),
CMakeLists.txt (new sources). Upstream CI workflows were moved to .github/workflows-disabled;
.github/workflows/sd-client-windows.yml builds SD-Client-Complete.zip.
