// SD-Client: on-screen notification stacks shared by all SD-Client features
#ifndef GAME_CLIENT_COMPONENTS_SD_TOASTS_H
#define GAME_CLIENT_COMPONENTS_SD_TOASTS_H

#include <game/client/component.h>

#include <base/color.h>

#include <cstdint>
#include <string>
#include <vector>

class CSdToasts : public CComponent
{
public:
	enum
	{
		CHANNEL_ABOVE_BELOW = 0,
		CHANNEL_HOOK_HIT,
		CHANNEL_COPY,
		CHANNEL_PING,
		NUM_CHANNELS
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnRender() override;

	// Adds a notification. DurationMs <= 0 uses the channel default.
	void Add(int Channel, const char *pText, ColorRGBA Color, int DurationMs = 0);

private:
	struct SToast
	{
		std::string m_Text;
		ColorRGBA m_Color;
		int64_t m_Start;
		int64_t m_Duration;
	};
	std::vector<SToast> m_avToasts[NUM_CHANNELS];
};

#endif
