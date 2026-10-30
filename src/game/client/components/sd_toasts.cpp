#include "sd_toasts.h"

#include <base/time.h>
#include <algorithm>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>

void CSdToasts::OnReset()
{
	for(auto &vToasts : m_avToasts)
		vToasts.clear();
}

void CSdToasts::Add(int Channel, const char *pText, ColorRGBA Color, int DurationMs)
{
	if(Channel < 0 || Channel >= NUM_CHANNELS)
		return;

	int MaxLines = 4;
	if(Channel == CHANNEL_HOOK_HIT)
	{
		MaxLines = g_Config.m_SdHhMaxLines;
		if(DurationMs <= 0)
			DurationMs = g_Config.m_SdHhDuration;
	}
	else if(Channel == CHANNEL_COPY)
	{
		MaxLines = 2;
		if(DurationMs <= 0)
			DurationMs = g_Config.m_SdCopyDuration;
	}
	else if(DurationMs <= 0)
	{
		DurationMs = 2500;
	}

	std::vector<SToast> &vToasts = m_avToasts[Channel];
	// Do not stack identical messages, just refresh them.
	for(SToast &Toast : vToasts)
	{
		if(Toast.m_Text == pText)
		{
			Toast.m_Start = time_get();
			Toast.m_Duration = (int64_t)DurationMs * time_freq() / 1000;
			Toast.m_Color = Color;
			return;
		}
	}
	vToasts.push_back({pText, Color, time_get(), (int64_t)DurationMs * time_freq() / 1000});
	while((int)vToasts.size() > MaxLines)
		vToasts.erase(vToasts.begin());
}

void CSdToasts::OnRender()
{
	if(!g_Config.m_SdEnabled || !g_Config.m_ClShowhud)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const int64_t Now = time_get();
	const CUIRect Screen = *Ui()->Screen();
	Ui()->MapScreen();

	for(int Channel = 0; Channel < NUM_CHANNELS; ++Channel)
	{
		std::vector<SToast> &vToasts = m_avToasts[Channel];
		vToasts.erase(std::remove_if(vToasts.begin(), vToasts.end(), [&](const SToast &Toast) {
			return Now - Toast.m_Start > Toast.m_Duration;
		}),
			vToasts.end());
		if(vToasts.empty())
			continue;

		float X = 0.0f, Y = 0.0f, Size = 12.0f;
		bool Centered = false;
		switch(Channel)
		{
		case CHANNEL_ABOVE_BELOW:
			X = g_Config.m_SdAbX * Screen.w / 100.0f;
			Y = g_Config.m_SdAbY * Screen.h / 100.0f + g_Config.m_SdAbSize * (g_Config.m_SdAbMaxLines + 2) * 1.2f;
			Size = g_Config.m_SdAbSize;
			Centered = true;
			break;
		case CHANNEL_HOOK_HIT:
			X = g_Config.m_SdHhX * Screen.w / 100.0f;
			Y = g_Config.m_SdHhY * Screen.h / 100.0f;
			Size = g_Config.m_SdHhSize;
			break;
		case CHANNEL_COPY:
			X = g_Config.m_SdCopyX * Screen.w / 100.0f;
			Y = g_Config.m_SdCopyY * Screen.h / 100.0f;
			Size = g_Config.m_SdCopySize;
			Centered = true;
			break;
		case CHANNEL_PING:
			X = g_Config.m_SdPingX * Screen.w / 100.0f;
			Y = g_Config.m_SdPingY * Screen.h / 100.0f + g_Config.m_SdPingSize * 6.0f;
			Size = g_Config.m_SdPingSize;
			break;
		}

		for(const SToast &Toast : vToasts)
		{
			const float Elapsed = (float)(Now - Toast.m_Start) / (float)time_freq();
			const float Total = (float)Toast.m_Duration / (float)time_freq();
			const float Remaining = Total - Elapsed;
			float Alpha = Toast.m_Color.a;
			if(Remaining < 0.3f)
				Alpha *= std::max(0.0f, Remaining / 0.3f);

			float DrawX = X;
			if(Centered)
				DrawX -= TextRender()->TextWidth(Size, Toast.m_Text.c_str()) / 2.0f;
			else if(DrawX + TextRender()->TextWidth(Size, Toast.m_Text.c_str()) > Screen.w)
				DrawX = Screen.w - TextRender()->TextWidth(Size, Toast.m_Text.c_str()) - 2.0f;

			TextRender()->TextColor(ColorRGBA(Toast.m_Color.r, Toast.m_Color.g, Toast.m_Color.b, Alpha));
			TextRender()->Text(DrawX, Y, Size, Toast.m_Text.c_str());
			Y += Size * 1.25f;
		}
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}
