// SD-Client: hook / hit notifications built only from real snapshot data.
//  - hook use:   the character's hook state changes to HOOK_FLYING
//  - hook hit:   the hook state changes to HOOK_GRABBED with a hooked player
//  - hammer hit: a real NETEVENTTYPE_HAMMERHIT event that matches a hammer attack of a character in the same snapshot
#ifndef GAME_CLIENT_COMPONENTS_SD_ACTIONS_H
#define GAME_CLIENT_COMPONENTS_SD_ACTIONS_H

#include <engine/shared/protocol.h>

#include <game/client/component.h>

class CSdActions : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnNewSnapshot() override;

private:
	struct SPrevState
	{
		bool m_Valid = false;
		int m_HookState = 0;
		int m_HookedPlayer = -1;
		int m_AttackTick = 0;
	};
	SPrevState m_aPrev[MAX_CLIENTS];

	bool IsMine(int ClientId) const;
	bool Wanted(bool Mine, bool Hit) const;
	void Notify(bool Mine, const char *pText);
	const char *Name(int ClientId, char *pBuf, int BufSize) const;
};

#endif
