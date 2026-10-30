// SD-Client: Above / Below player indicator
#ifndef GAME_CLIENT_COMPONENTS_SD_ABOVEBELOW_H
#define GAME_CLIENT_COMPONENTS_SD_ABOVEBELOW_H

#include "sd_common.h"

#include <engine/shared/protocol.h>

#include <game/client/component.h>

class CSdAboveBelow : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnRender() override;

	static const char *StateText(int State);

private:
	int m_aState[MAX_CLIENTS] = {};
	bool m_aKnown[MAX_CLIENTS] = {};
	int m_ReferenceId = -1;

	int UpdateState(int ClientId, float DeltaY);
};

#endif
