// SD-Client: unlimited named player profiles stored in sd-client/profiles.json (user data directory).
#ifndef GAME_CLIENT_COMPONENTS_SD_PROFILES_H
#define GAME_CLIENT_COMPONENTS_SD_PROFILES_H

#include <engine/console.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include <vector>

struct SSdProfile
{
	char m_aProfileName[32] = "";
	char m_aName[MAX_NAME_LENGTH] = "";
	char m_aClan[MAX_CLAN_LENGTH] = "";
	char m_aSkin[MAX_SKIN_LENGTH] = "default";
	bool m_UseCustomColor = false;
	unsigned m_ColorBody = 0;
	unsigned m_ColorFeet = 0;
	int m_Country = -1;
	int m_Emote = 0; // 0 normal, 1 pain, 2 happy, 3 surprise, 4 angry, 5 blink
};

class CSdProfiles : public CComponent
{
public:
	static constexpr const char *PROFILES_FILE = "sd-client/profiles.json";

	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnInit() override;

	int Num() const { return (int)m_vProfiles.size(); }
	const SSdProfile &Get(int Index) const { return m_vProfiles[Index]; }
	int ActiveIndex() const { return m_Active; }
	int Find(const char *pProfileName) const;

	// Snapshot of the current player settings (name, clan, skin, colors, flag, emote).
	SSdProfile FromCurrentSettings(const char *pProfileName) const;
	// Returns the index of the new profile or -1 (empty or duplicate name).
	int Add(const SSdProfile &Profile);
	bool Update(int Index, const SSdProfile &Profile);
	bool Remove(int Index);
	// Writes the profile into the player settings and sends the new info to the server.
	bool Apply(int Index);
	void ApplyNext(int Step);

	bool Load();
	bool Save();

private:
	std::vector<SSdProfile> m_vProfiles;
	int m_Active = -1;

	static void ConProfile(IConsole::IResult *pResult, void *pUserData);
	static void ConProfileNext(IConsole::IResult *pResult, void *pUserData);
	static void ConProfilePrev(IConsole::IResult *pResult, void *pUserData);
};

#endif
