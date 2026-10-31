#include "sd_grenade.h"

#include "sd_common.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/components/controls.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/gamecore.h>

#include <generated/protocol.h>

#include <algorithm>
#include <cmath>

bool CSdGrenade::Simulate(vec2 Start, vec2 Direction)
{
	m_vPoints.clear();

	// The grenade uses the tuning of the tune zone the shooter is standing in (zone 0 = global tuning).
	int TuneZone = 0;
	if(GameClient()->m_GameWorld.m_WorldConfig.m_UseTuneZones && GameClient()->m_aLocalTuneZone[g_Config.m_ClDummy] > 0)
		TuneZone = GameClient()->m_aLocalTuneZone[g_Config.m_ClDummy];
	const CTuningParams &Tuning = *GameClient()->GetTuning(TuneZone);
	const float Curvature = Tuning.m_GrenadeCurvature;
	const float Speed = Tuning.m_GrenadeSpeed;
	const int TickSpeed = Client()->GameTickSpeed();
	const int Lifetime = std::clamp((int)(TickSpeed * Tuning.m_GrenadeLifetime), 1, 2000);

	m_vPoints.push_back(Start);
	for(int Tick = 1; Tick <= Lifetime; ++Tick)
	{
		const float PrevTime = (Tick - 1) / (float)TickSpeed;
		const float CurTime = Tick / (float)TickSpeed;
		const vec2 PrevPos = CalcPos(Start, Direction, Curvature, Speed, PrevTime);
		const vec2 CurPos = CalcPos(Start, Direction, Curvature, Speed, CurTime);

		vec2 CollisionPos, BeforeCollision;
		if(GameClient()->Collision()->IntersectLine(PrevPos, CurPos, &CollisionPos, &BeforeCollision))
		{
			// a grenade explodes at the collision point (it does not bounce)
			m_vPoints.push_back(CollisionPos);
			return true;
		}
		m_vPoints.push_back(CurPos);
	}
	return false;
}

void CSdGrenade::OnRender()
{
	if(!g_Config.m_SdEnabled || !g_Config.m_SdGrEnable)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;

	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || !GameClient()->m_Snap.m_pLocalCharacter || !GameClient()->m_Snap.m_aCharacters[LocalId].m_Active)
		return;
	if(GameClient()->m_aClients[LocalId].m_Predicted.m_ActiveWeapon != WEAPON_GRENADE && GameClient()->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_GRENADE)
		return;
	if(GameClient()->m_aClients[LocalId].m_FreezeEnd != 0 || GameClient()->m_aClients[LocalId].m_Paused)
		return;

	// Same direction the client sends to the server.
	vec2 Aim = GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy];
	if(length(Aim) < 0.001f)
		Aim = vec2(1.0f, 0.0f);
	const vec2 Direction = normalize(Aim);
	const vec2 Pos = GameClient()->m_aClients[LocalId].m_RenderPos;
	// The server spawns the projectile in front of the tee: Pos + Direction * ProximityRadius * 0.75
	const vec2 Start = Pos + Direction * (CCharacterCore::PhysicalSize() * 0.75f);

	const bool Hit = Simulate(Start, Direction);
	if(m_vPoints.size() < 2)
		return;

	const ColorRGBA Color = SdColor(g_Config.m_SdGrColor, g_Config.m_SdGrAlpha / 100.0f);
	const float HalfWidth = g_Config.m_SdGrWidth * 0.5f;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);
	std::vector<IGraphics::CFreeformItem> vItems;
	vItems.reserve(m_vPoints.size());
	for(size_t i = 1; i < m_vPoints.size(); ++i)
	{
		const vec2 A = m_vPoints[i - 1];
		const vec2 B = m_vPoints[i];
		vec2 Delta = B - A;
		if(length(Delta) < 0.001f)
			continue;
		const vec2 Normal = normalize(vec2(-Delta.y, Delta.x)) * HalfWidth;
		vItems.emplace_back(A - Normal, A + Normal, B - Normal, B + Normal);
	}
	if(!vItems.empty())
		Graphics()->QuadsDrawFreeform(vItems.data(), vItems.size());
	if(Hit && g_Config.m_SdGrMarker)
	{
		Graphics()->SetColor(ColorRGBA(Color.r, Color.g, Color.b, std::min(1.0f, Color.a + 0.2f)));
		Graphics()->DrawCircle(m_vPoints.back().x, m_vPoints.back().y, 5.0f, 16);
	}
	Graphics()->QuadsEnd();
}
