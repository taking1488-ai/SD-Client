#include "sd_abovebelow.h"

#include "sd_toasts.h"

#include <base/str.h>

#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <algorithm>
#include <cmath>

// U+22C0 N-ARY LOGICAL AND and U+22C1 N-ARY LOGICAL OR (included in the bundled DejaVu Sans)
static const char *const gs_pAboveText = "Above \xE2\x8B\x80";
static const char *const gs_pBelowText = "Below \xE2\x8B\x81";
static const char *const gs_pSameText = "Same level =";

const char *CSdAboveBelow::StateText(int State)
{
	switch(State)
	{
	case SD_VERT_ABOVE: return gs_pAboveText;
	case SD_VERT_BELOW: return gs_pBelowText;
	case SD_VERT_SAME: return gs_pSameText;
	}
	return "";
}

void CSdAboveBelow::OnReset()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_aState[i] = SD_VERT_UNKNOWN;
		m_aKnown[i] = false;
	}
	m_ReferenceId = -1;
}

int CSdAboveBelow::UpdateState(int ClientId, float DeltaY)
{
	// Hysteresis prevents flickering when a player hovers exactly on the tolerance border.
	const float Tolerance = (float)g_Config.m_SdAbSameTolerance;
	const float LeaveSame = Tolerance;
	const float EnterSame = Tolerance * 0.5f;
	const int Previous = m_aKnown[ClientId] ? m_aState[ClientId] : SD_VERT_UNKNOWN;

	int NewState;
	if(Previous == SD_VERT_ABOVE)
		NewState = DeltaY > -EnterSame ? (DeltaY > LeaveSame ? SD_VERT_BELOW : SD_VERT_SAME) : SD_VERT_ABOVE;
	else if(Previous == SD_VERT_BELOW)
		NewState = DeltaY < EnterSame ? (DeltaY < -LeaveSame ? SD_VERT_ABOVE : SD_VERT_SAME) : SD_VERT_BELOW;
	else if(DeltaY < -LeaveSame)
		NewState = SD_VERT_ABOVE;
	else if(DeltaY > LeaveSame)
		NewState = SD_VERT_BELOW;
	else
		NewState = SD_VERT_SAME;
	return NewState;
}

void CSdAboveBelow::OnRender()
{
	if(!g_Config.m_SdEnabled || !g_Config.m_SdAbEnable || !g_Config.m_ClShowhud)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	// The reference is our own tee, or the tee we are spectating.
	int ReferenceId = GameClient()->m_Snap.m_LocalClientId;
	if(GameClient()->m_Snap.m_SpecInfo.m_Active)
		ReferenceId = GameClient()->m_Snap.m_SpecInfo.m_SpectatorId;
	if(ReferenceId < 0 || ReferenceId >= MAX_CLIENTS || !GameClient()->m_Snap.m_aCharacters[ReferenceId].m_Active)
		return;
	if(ReferenceId != m_ReferenceId)
	{
		// Reference changed (e.g. spectating someone else): do not emit notifications for the switch.
		m_ReferenceId = ReferenceId;
		for(int i = 0; i < MAX_CLIENTS; ++i)
			m_aKnown[i] = false;
	}

	const vec2 MyPos = GameClient()->m_aClients[ReferenceId].m_RenderPos;

	struct SEntry
	{
		int m_ClientId;
		int m_State;
		float m_DeltaY;
		float m_Distance;
	};
	std::vector<SEntry> vEntries;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(ClientId == ReferenceId)
			continue;
		if(g_Config.m_SdAbTarget >= 0 && ClientId != g_Config.m_SdAbTarget)
			continue;
		const auto &Character = GameClient()->m_Snap.m_aCharacters[ClientId];
		if(!Character.m_Active || !GameClient()->m_aClients[ClientId].m_Active)
			continue;

		const vec2 Pos = GameClient()->m_aClients[ClientId].m_RenderPos;
		// Y grows downwards in DDNet: a smaller Y means the other tee is higher up (above us).
		const float DeltaY = Pos.y - MyPos.y;
		const int State = UpdateState(ClientId, DeltaY);

		if(m_aKnown[ClientId] && m_aState[ClientId] != State && g_Config.m_SdAbNotify)
		{
			const bool Wanted = (State == SD_VERT_ABOVE && g_Config.m_SdAbNotifyAbove) ||
					    (State == SD_VERT_SAME && g_Config.m_SdAbNotifySame) ||
					    (State == SD_VERT_BELOW && g_Config.m_SdAbNotifyBelow);
			if(Wanted)
			{
				char aBuf[160];
				str_format(aBuf, sizeof(aBuf), "%s: %s", GameClient()->m_aClients[ClientId].m_aName, StateText(State));
				GameClient()->m_SdToasts.Add(CSdToasts::CHANNEL_ABOVE_BELOW, aBuf, SdColor(g_Config.m_SdAbColor), 2000);
			}
		}
		m_aState[ClientId] = State;
		m_aKnown[ClientId] = true;

		// Only players which are on screen (or the explicitly tracked player) are listed.
		vEntries.push_back({ClientId, State, DeltaY, distance(Pos, MyPos)});
	}

	// Forget players which left.
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(!GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
			m_aKnown[ClientId] = false;
	}

	const CUIRect Screen = *Ui()->Screen();
	Ui()->MapScreen();

	const float Size = (float)g_Config.m_SdAbSize;
	float Y = g_Config.m_SdAbY * Screen.h / 100.0f;
	const float CenterX = g_Config.m_SdAbX * Screen.w / 100.0f;
	const ColorRGBA Color = SdColor(g_Config.m_SdAbColor);

	if(vEntries.empty())
	{
		if(g_Config.m_SdAbTarget >= 0 && GameClient()->m_aClients[g_Config.m_SdAbTarget].m_Active)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "%s: not visible", GameClient()->m_aClients[g_Config.m_SdAbTarget].m_aName);
			TextRender()->TextColor(ColorRGBA(Color.r, Color.g, Color.b, 0.6f));
			TextRender()->Text(CenterX - TextRender()->TextWidth(Size, aBuf) / 2.0f, Y, Size, aBuf);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		return;
	}

	std::sort(vEntries.begin(), vEntries.end(), [](const SEntry &A, const SEntry &B) { return A.m_Distance < B.m_Distance; });
	const int MaxLines = g_Config.m_SdAbTarget >= 0 ? 1 : g_Config.m_SdAbMaxLines;
	for(int i = 0; i < (int)vEntries.size() && i < MaxLines; ++i)
	{
		char aBuf[160];
		str_format(aBuf, sizeof(aBuf), "%s  %s", GameClient()->m_aClients[vEntries[i].m_ClientId].m_aName, StateText(vEntries[i].m_State));
		TextRender()->TextColor(Color);
		TextRender()->Text(CenterX - TextRender()->TextWidth(Size, aBuf) / 2.0f, Y, Size, aBuf);
		Y += Size * 1.25f;
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}
