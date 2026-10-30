#include "sd_actions.h"

#include "sd_common.h"
#include "sd_toasts.h"

#include <base/math.h>
#include <base/str.h>

#include <engine/client.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/gamecore.h>

#include <generated/protocol.h>

#include <vector>

void CSdActions::OnReset()
{
	for(SPrevState &State : m_aPrev)
		State = SPrevState();
}

bool CSdActions::IsMine(int ClientId) const
{
	return ClientId == GameClient()->m_aLocalIds[0] || (Client()->DummyConnected() && ClientId == GameClient()->m_aLocalIds[1]);
}

bool CSdActions::Wanted(bool Mine, bool Hit) const
{
	switch(g_Config.m_SdHhMode)
	{
	case 1: return Mine && !Hit;
	case 2: return Mine && Hit;
	case 3: return !Mine && !Hit;
	case 4: return !Mine && Hit;
	case 5: return true;
	}
	if(Mine)
		return Hit ? g_Config.m_SdHhMyHits : g_Config.m_SdHhMyHooks;
	return Hit ? g_Config.m_SdHhOtherHits : g_Config.m_SdHhOtherHooks;
}

void CSdActions::Notify(bool Mine, const char *pText)
{
	GameClient()->m_SdToasts.Add(CSdToasts::CHANNEL_HOOK_HIT, pText, SdColor(Mine ? g_Config.m_SdHhColorMine : g_Config.m_SdHhColorOther));
}

const char *CSdActions::Name(int ClientId, char *pBuf, int BufSize) const
{
	if(IsMine(ClientId))
		str_copy(pBuf, "You", BufSize);
	else
		str_copy(pBuf, GameClient()->m_aClients[ClientId].m_aName, BufSize);
	return pBuf;
}

void CSdActions::OnNewSnapshot()
{
	if(!g_Config.m_SdEnabled || !g_Config.m_SdHhEnable || GameClient()->m_SuppressEvents)
	{
		OnReset();
		return;
	}
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	struct SAttack
	{
		int m_ClientId;
		vec2 m_ProjStart;
	};
	std::vector<SAttack> vAttacks;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		const auto &Info = GameClient()->m_Snap.m_aCharacters[ClientId];
		SPrevState &Prev = m_aPrev[ClientId];
		if(!Info.m_Active)
		{
			Prev.m_Valid = false;
			continue;
		}
		const CNetObj_Character &Cur = Info.m_Cur;

		if(Prev.m_Valid)
		{
			const bool Mine = IsMine(ClientId);
			char aName[MAX_NAME_LENGTH + 4];
			char aTarget[MAX_NAME_LENGTH + 4];
			char aBuf[160];

			// Hook thrown
			if(Cur.m_HookState == HOOK_FLYING && Prev.m_HookState != HOOK_FLYING && Wanted(Mine, false))
			{
				str_format(aBuf, sizeof(aBuf), "%s: hook", Name(ClientId, aName, sizeof(aName)));
				Notify(Mine, aBuf);
			}

			// Hook attached to a player
			if(Cur.m_HookState == HOOK_GRABBED && Cur.m_HookedPlayer >= 0 && Cur.m_HookedPlayer < MAX_CLIENTS &&
				(Prev.m_HookState != HOOK_GRABBED || Prev.m_HookedPlayer != Cur.m_HookedPlayer) &&
				Wanted(Mine, false))
			{
				str_format(aBuf, sizeof(aBuf), "%s hooked %s", Name(ClientId, aName, sizeof(aName)), Name(Cur.m_HookedPlayer, aTarget, sizeof(aTarget)));
				Notify(Mine, aBuf);
			}

			// Hammer attack (the server only sends a HAMMERHIT event when something was hit)
			if(Cur.m_Weapon == WEAPON_HAMMER && Cur.m_AttackTick != Prev.m_AttackTick)
			{
				const vec2 Pos(Cur.m_X, Cur.m_Y);
				const vec2 Dir = direction(Cur.m_Angle / 256.0f);
				vAttacks.push_back({ClientId, Pos + Dir * (CCharacterCore::PhysicalSize() * 0.75f)});
			}
		}

		Prev.m_Valid = true;
		Prev.m_HookState = Cur.m_HookState;
		Prev.m_HookedPlayer = Cur.m_HookedPlayer;
		Prev.m_AttackTick = Cur.m_AttackTick;
	}

	if(vAttacks.empty())
		return;

	const int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
	for(int Index = 0; Index < Num; ++Index)
	{
		const IClient::CSnapItem Item = Client()->SnapGetItem(IClient::SNAP_CURRENT, Index);
		if(Item.m_Type != NETEVENTTYPE_HAMMERHIT)
			continue;
		const CNetEvent_HammerHit *pEvent = (const CNetEvent_HammerHit *)Item.m_pData;
		const vec2 EventPos(pEvent->m_X, pEvent->m_Y);

		// The attacker is the character whose hammer start position is closest to the event position.
		int Attacker = -1;
		float BestAttackerDistance = 20.0f;
		for(const SAttack &Attack : vAttacks)
		{
			const float Distance = distance(Attack.m_ProjStart, EventPos);
			if(Distance < BestAttackerDistance)
			{
				BestAttackerDistance = Distance;
				Attacker = Attack.m_ClientId;
			}
		}
		if(Attacker < 0)
			continue;

		// The event is placed half a tee radius in front of the victim, towards the attacker.
		int Victim = -1;
		float BestVictimDistance = CCharacterCore::PhysicalSize() * 0.5f + 5.0f;
		for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
		{
			if(ClientId == Attacker || !GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
				continue;
			const CNetObj_Character &Cur = GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur;
			const float Distance = distance(vec2(Cur.m_X, Cur.m_Y), EventPos);
			if(Distance < BestVictimDistance)
			{
				BestVictimDistance = Distance;
				Victim = ClientId;
			}
		}
		if(Victim < 0)
			continue;

		const bool Mine = IsMine(Attacker);
		if(!Wanted(Mine, true))
			continue;
		char aAttacker[MAX_NAME_LENGTH + 4];
		char aVictim[MAX_NAME_LENGTH + 4];
		char aBuf[160];
		str_format(aBuf, sizeof(aBuf), "%s hit %s", Name(Attacker, aAttacker, sizeof(aAttacker)), Name(Victim, aVictim, sizeof(aVictim)));
		Notify(Mine, aBuf);
	}
}
