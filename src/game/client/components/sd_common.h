// SD-Client: shared helpers
#ifndef GAME_CLIENT_COMPONENTS_SD_COMMON_H
#define GAME_CLIENT_COMPONENTS_SD_COMMON_H

#include <base/color.h>
#include <base/vmath.h>

// Converts a packed HSL config color (MACRO_CONFIG_COL without alpha) to RGBA.
inline ColorRGBA SdColor(unsigned ConfigValue, float Alpha = 1.0f)
{
	ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(ConfigValue).UnclampLighting(ColorHSLA::DARKEST_LGT));
	Color.a = Alpha;
	return Color;
}

// DDNet's world Y axis points DOWN: a smaller Y means "higher up" on screen.
enum ESdVertical
{
	SD_VERT_UNKNOWN = -1,
	SD_VERT_ABOVE = 0,
	SD_VERT_SAME,
	SD_VERT_BELOW,
};

#endif
