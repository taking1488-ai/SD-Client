// SD-Client: grenade trajectory preview.
// Uses the same equations as CProjectile (CalcPos with the active tuning) and CCollision::IntersectLine.
// It only draws a line: no projectile is created and nothing is sent to the server.
#ifndef GAME_CLIENT_COMPONENTS_SD_GRENADE_H
#define GAME_CLIENT_COMPONENTS_SD_GRENADE_H

#include <base/vmath.h>

#include <game/client/component.h>

#include <vector>

class CSdGrenade : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;

private:
	std::vector<vec2> m_vPoints;
	// Simulates the flight. Returns true if the grenade hits the map before its lifetime ends.
	bool Simulate(vec2 Start, vec2 Direction);
};

#endif
