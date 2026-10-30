#include "sd_ping.h"

#include "sd_common.h"
#include "sd_toasts.h"

#include <base/str.h>
#include <base/time.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <algorithm>
#include <cmath>

void CSdPing::OnReset()
{
	m_Head = 0;
	m_NumSamples = 0;
	m_LastSampleTime = 0;
	m_ConsecutiveHigh = 0;
	m_NumHighPingEvents = 0;
	m_WasHigh = false;
}

int CSdPing::LocalPing() const
{
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || LocalId >= MAX_CLIENTS || !GameClient()->m_Snap.m_apPlayerInfos[LocalId])
		return -1;
	return GameClient()->m_Snap.m_apPlayerInfos[LocalId]->m_Latency;
}

void CSdPing::AddSample(int Ping)
{
	// ring buffer: m_Head is the index of the oldest sample once the buffer is full
	if(m_NumSamples < NUM_SAMPLES)
	{
		m_aSamples[m_NumSamples++] = Ping;
	}
	else
	{
		m_aSamples[m_Head] = Ping;
		m_Head = (m_Head + 1) % NUM_SAMPLES;
	}
}

CSdPing::SStats CSdPing::Stats() const
{
	SStats Stats{};
	if(m_NumSamples == 0)
		return Stats;
	Stats.m_Valid = true;
	Stats.m_NumSamples = m_NumSamples;
	Stats.m_Current = Sample(m_NumSamples - 1);
	const int OneSecondAgo = std::max(0, m_NumSamples - 1 - 4);
	Stats.m_Delta = Stats.m_Current - Sample(OneSecondAgo);
	Stats.m_Min = Stats.m_Max = Stats.m_Current;
	double Sum = 0.0;
	double JitterSum = 0.0;
	for(int i = 0; i < m_NumSamples; ++i)
	{
		const int Value = Sample(i);
		Stats.m_Min = std::min(Stats.m_Min, Value);
		Stats.m_Max = std::max(Stats.m_Max, Value);
		Sum += Value;
		if(i > 0)
			JitterSum += std::abs(Value - Sample(i - 1));
	}
	Stats.m_Average = (float)(Sum / m_NumSamples);
	Stats.m_Jitter = m_NumSamples > 1 ? (float)(JitterSum / (m_NumSamples - 1)) : 0.0f;
	Stats.m_PredictionTimeMs = Client()->GetPredictionTime();
	Stats.m_NumHighPingEvents = m_NumHighPingEvents;
	return Stats;
}

void CSdPing::OnRender()
{
	if(!g_Config.m_SdEnabled)
		return;
	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	// sample the server reported latency 4 times per second
	const int64_t Now = time_get();
	if(Now - m_LastSampleTime >= time_freq() / 4)
	{
		m_LastSampleTime = Now;
		const int Ping = LocalPing();
		if(Ping >= 0)
		{
			AddSample(Ping);

			const bool High = Ping >= g_Config.m_SdPingThreshold;
			m_ConsecutiveHigh = High ? m_ConsecutiveHigh + 1 : 0;
			if(High && !m_WasHigh && m_ConsecutiveHigh >= 2)
			{
				m_WasHigh = true;
				m_NumHighPingEvents++;
				if(g_Config.m_SdPingWarn && Now - m_LastWarnTime > 10 * time_freq())
				{
					m_LastWarnTime = Now;
					char aBuf[96];
					str_format(aBuf, sizeof(aBuf), "High ping: %d ms", Ping);
					GameClient()->m_SdToasts.Add(CSdToasts::CHANNEL_PING, aBuf, SdColor(g_Config.m_SdPingHighColor), 3000);
				}
			}
			else if(!High && m_ConsecutiveHigh == 0)
			{
				m_WasHigh = false;
			}
		}
	}

	if(!g_Config.m_SdPingShow || !g_Config.m_ClShowhud || m_NumSamples == 0)
		return;

	const SStats Stats = this->Stats();
	const CUIRect Screen = *Ui()->Screen();
	Ui()->MapScreen();

	const float Size = (float)g_Config.m_SdPingSize;
	const float X = g_Config.m_SdPingX * Screen.w / 100.0f;
	float Y = g_Config.m_SdPingY * Screen.h / 100.0f;
	const bool High = Stats.m_Current >= g_Config.m_SdPingThreshold;
	const ColorRGBA Color = SdColor(High ? g_Config.m_SdPingHighColor : g_Config.m_SdPingColor);

	char aBuf[96];
	if(g_Config.m_SdPingDelta)
		str_format(aBuf, sizeof(aBuf), "Ping: %d ms (%+d)", Stats.m_Current, Stats.m_Delta);
	else
		str_format(aBuf, sizeof(aBuf), "Ping: %d ms", Stats.m_Current);
	TextRender()->TextColor(Color);
	TextRender()->Text(X, Y, Size, aBuf);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Y += Size * 1.3f;

	if(g_Config.m_SdPingGraph && m_NumSamples > 1)
	{
		const float GraphW = Size * 8.0f;
		const float GraphH = Size * 2.5f;
		const float Scale = (float)std::max(Stats.m_Max, g_Config.m_SdPingThreshold);

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.35f);
		IGraphics::CQuadItem Background(X, Y, GraphW, GraphH);
		Graphics()->QuadsDrawTL(&Background, 1);
		// threshold line
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.25f);
		IGraphics::CQuadItem Threshold(X, Y + GraphH - GraphH * g_Config.m_SdPingThreshold / Scale, GraphW, 0.4f);
		Graphics()->QuadsDrawTL(&Threshold, 1);
		const float BarW = GraphW / NUM_SAMPLES;
		for(int i = 0; i < m_NumSamples; ++i)
		{
			const int Value = Sample(i);
			const float BarH = std::max(0.5f, GraphH * Value / Scale);
			const ColorRGBA BarColor = SdColor(Value >= g_Config.m_SdPingThreshold ? g_Config.m_SdPingHighColor : g_Config.m_SdPingColor, 0.85f);
			Graphics()->SetColor(BarColor);
			IGraphics::CQuadItem Bar(X + (NUM_SAMPLES - m_NumSamples + i) * BarW, Y + GraphH - BarH, BarW, BarH);
			Graphics()->QuadsDrawTL(&Bar, 1);
		}
		Graphics()->QuadsEnd();
	}
}
