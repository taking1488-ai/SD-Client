#include "sd_playercopy.h"

#include "sd_common.h"
#include "sd_toasts.h"

#include <base/color.h>
#include <base/math.h>
#include <base/str.h>

#include <engine/input.h>
#include <engine/shared/config.h>

#include <game/client/components/controls.h>
#include <game/client/gameclient.h>

static void FormatColor(char *pBuf, int BufSize, unsigned HslValue, bool Custom)
{
	const ColorRGBA Rgb = color_cast<ColorRGBA>(ColorHSLA(HslValue).UnclampLighting(ColorHSLA::DARKEST_LGT));
	str_format(pBuf, BufSize, "%u (#%02X%02X%02X)%s", HslValue,
		(int)std::round(Rgb.r * 255.0f), (int)std::round(Rgb.g * 255.0f), (int)std::round(Rgb.b * 255.0f),
		Custom ? "" : " [custom colors disabled]");
}

void CSdPlayerCopy::BuildText(int ClientId, int Kind, char *pBuf, int BufSize) const
{
	const CGameClient::CClientData &Data = GameClient()->m_aClients[ClientId];
	char aBody[96];
	char aFeet[96];
	FormatColor(aBody, sizeof(aBody), Data.m_ColorBody, Data.m_UseCustomColor);
	FormatColor(aFeet, sizeof(aFeet), Data.m_ColorFeet, Data.m_UseCustomColor);

	switch(Kind)
	{
	case COPY_NAME:
		str_copy(pBuf, Data.m_aName, BufSize);
		break;
	case COPY_CLAN:
		str_copy(pBuf, Data.m_aClan, BufSize);
		break;
	case COPY_SKIN:
		str_copy(pBuf, Data.m_aSkinName, BufSize);
		break;
	case COPY_SKIN_COLOR:
		str_format(pBuf, BufSize, "Body Color: %s\nFeet Color: %s", aBody, aFeet);
		break;
	default:
		str_format(pBuf, BufSize, "Name: %s\nClan: %s\nSkin: %s\nBody Color: %s\nFeet Color: %s",
			Data.m_aName, Data.m_aClan, Data.m_aSkinName, aBody, aFeet);
		break;
	}
}

bool CSdPlayerCopy::Copy(int ClientId, int Kind)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !GameClient()->m_aClients[ClientId].m_Active)
		return false;

	char aText[512];
	BuildText(ClientId, Kind, aText, sizeof(aText));
	Input()->SetClipboardText(aText);
	if(g_Config.m_SdCopyToast)
		GameClient()->m_SdToasts.Add(CSdToasts::CHANNEL_COPY, "Copied to clipboard!", SdColor(g_Config.m_SdCopyColor));
	return true;
}

int CSdPlayerCopy::PlayerUnderCursor() const
{
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0)
		return -1;
	const vec2 Cursor = GameClient()->m_aClients[LocalId].m_RenderPos + GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy];

	int Best = -1;
	float BestDistance = 40.0f;
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(ClientId == LocalId || !GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
			continue;
		const float Distance = distance(GameClient()->m_aClients[ClientId].m_RenderPos, Cursor);
		if(Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = ClientId;
		}
	}
	return Best;
}

void CSdPlayerCopy::ConCopy(IConsole::IResult *pResult, void *pUserData)
{
	SCommand *pCommand = static_cast<SCommand *>(pUserData);
	CSdPlayerCopy *pThis = pCommand->m_pThis;
	const int ClientId = pResult->NumArguments() > 0 ? pResult->GetInteger(0) : pThis->PlayerUnderCursor();
	if(!pThis->Copy(ClientId, pCommand->m_Kind))
		pThis->GameClient()->m_SdToasts.Add(CSdToasts::CHANNEL_COPY, "No player to copy (aim at a player or pass a client id)", ColorRGBA(1.0f, 0.4f, 0.4f, 1.0f));
}

void CSdPlayerCopy::OnConsoleInit()
{
	for(int Kind = 0; Kind < 5; ++Kind)
		m_aCommands[Kind] = {this, Kind};
	Console()->Register("sd_copy_name", "?i[client-id]", CFGFLAG_CLIENT, ConCopy, &m_aCommands[COPY_NAME], "Copy the name of a player (default: player under the cursor) to the clipboard");
	Console()->Register("sd_copy_clan", "?i[client-id]", CFGFLAG_CLIENT, ConCopy, &m_aCommands[COPY_CLAN], "Copy the clan tag of a player to the clipboard");
	Console()->Register("sd_copy_skin", "?i[client-id]", CFGFLAG_CLIENT, ConCopy, &m_aCommands[COPY_SKIN], "Copy the skin name of a player to the clipboard");
	Console()->Register("sd_copy_skin_color", "?i[client-id]", CFGFLAG_CLIENT, ConCopy, &m_aCommands[COPY_SKIN_COLOR], "Copy the skin colors of a player to the clipboard");
	Console()->Register("sd_copy_full", "?i[client-id]", CFGFLAG_CLIENT, ConCopy, &m_aCommands[COPY_FULL], "Copy all information about a player to the clipboard");
}
