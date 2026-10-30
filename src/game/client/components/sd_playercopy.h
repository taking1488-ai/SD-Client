// SD-Client: copy player information to the system clipboard.
#ifndef GAME_CLIENT_COMPONENTS_SD_PLAYERCOPY_H
#define GAME_CLIENT_COMPONENTS_SD_PLAYERCOPY_H

#include <game/client/component.h>

#include <engine/console.h>

class CSdPlayerCopy : public CComponent
{
public:
	enum EKind
	{
		COPY_NAME = 0,
		COPY_CLAN,
		COPY_SKIN,
		COPY_SKIN_COLOR,
		COPY_FULL,
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;

	// Copies the requested information about a player to the clipboard and shows a notification.
	bool Copy(int ClientId, int Kind);
	// Client id of the tee closest to the aim cursor (within a tee's reach), or -1.
	int PlayerUnderCursor() const;

	// Builds the text which is copied for the given kind.
	void BuildText(int ClientId, int Kind, char *pBuf, int BufSize) const;

private:
	struct SCommand
	{
		CSdPlayerCopy *m_pThis;
		int m_Kind;
	};
	SCommand m_aCommands[5];
	static void ConCopy(IConsole::IResult *pResult, void *pUserData);
};

#endif
