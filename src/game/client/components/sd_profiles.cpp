#include "sd_profiles.h"

#include "sd_common.h"
#include "sd_toasts.h"

#include <base/io.h>
#include <base/log.h>
#include <base/str.h>

#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>

#include <string>

static void JsonEscape(const char *pIn, std::string &Out)
{
	Out += '"';
	for(const char *p = pIn; *p; ++p)
	{
		const unsigned char c = (unsigned char)*p;
		if(c == '"' || c == '\\')
		{
			Out += '\\';
			Out += (char)c;
		}
		else if(c < 0x20)
		{
			char aBuf[8];
			str_format(aBuf, sizeof(aBuf), "\\u%04x", c);
			Out += aBuf;
		}
		else
		{
			Out += (char)c;
		}
	}
	Out += '"';
}

int CSdProfiles::Find(const char *pProfileName) const
{
	for(int i = 0; i < (int)m_vProfiles.size(); ++i)
	{
		if(str_comp_nocase(m_vProfiles[i].m_aProfileName, pProfileName) == 0)
			return i;
	}
	return -1;
}

SSdProfile CSdProfiles::FromCurrentSettings(const char *pProfileName) const
{
	SSdProfile Profile;
	str_copy(Profile.m_aProfileName, pProfileName);
	str_copy(Profile.m_aName, g_Config.m_PlayerName);
	str_copy(Profile.m_aClan, g_Config.m_PlayerClan);
	str_copy(Profile.m_aSkin, g_Config.m_ClPlayerSkin);
	Profile.m_UseCustomColor = g_Config.m_ClPlayerUseCustomColor != 0;
	Profile.m_ColorBody = g_Config.m_ClPlayerColorBody;
	Profile.m_ColorFeet = g_Config.m_ClPlayerColorFeet;
	Profile.m_Country = g_Config.m_PlayerCountry;
	Profile.m_Emote = g_Config.m_ClPlayerDefaultEyes;
	return Profile;
}

int CSdProfiles::Add(const SSdProfile &Profile)
{
	if(Profile.m_aProfileName[0] == '\0' || Find(Profile.m_aProfileName) >= 0)
		return -1;
	m_vProfiles.push_back(Profile);
	Save();
	return (int)m_vProfiles.size() - 1;
}

bool CSdProfiles::Update(int Index, const SSdProfile &Profile)
{
	if(Index < 0 || Index >= Num() || Profile.m_aProfileName[0] == '\0')
		return false;
	const int Existing = Find(Profile.m_aProfileName);
	if(Existing >= 0 && Existing != Index)
		return false;
	m_vProfiles[Index] = Profile;
	return Save();
}

bool CSdProfiles::Remove(int Index)
{
	if(Index < 0 || Index >= Num())
		return false;
	m_vProfiles.erase(m_vProfiles.begin() + Index);
	if(m_Active == Index)
		m_Active = -1;
	else if(m_Active > Index)
		m_Active--;
	return Save();
}

bool CSdProfiles::Apply(int Index)
{
	if(Index < 0 || Index >= Num())
		return false;
	const SSdProfile &Profile = m_vProfiles[Index];
	str_copy(g_Config.m_PlayerName, Profile.m_aName);
	str_copy(g_Config.m_PlayerClan, Profile.m_aClan);
	str_copy(g_Config.m_ClPlayerSkin, Profile.m_aSkin);
	g_Config.m_ClPlayerUseCustomColor = Profile.m_UseCustomColor ? 1 : 0;
	g_Config.m_ClPlayerColorBody = Profile.m_ColorBody;
	g_Config.m_ClPlayerColorFeet = Profile.m_ColorFeet;
	g_Config.m_PlayerCountry = Profile.m_Country;
	g_Config.m_ClPlayerDefaultEyes = std::clamp(Profile.m_Emote, 0, 5);
	m_Active = Index;
	GameClient()->m_Menus.SdSetNeedSendInfo();

	char aBuf[96];
	str_format(aBuf, sizeof(aBuf), "Profile loaded: %s", Profile.m_aProfileName);
	GameClient()->m_SdToasts.Add(CSdToasts::CHANNEL_COPY, aBuf, SdColor(g_Config.m_SdCopyColor), 2000);
	return true;
}

void CSdProfiles::ApplyNext(int Step)
{
	if(m_vProfiles.empty())
		return;
	const int Count = Num();
	const int Start = m_Active < 0 ? (Step > 0 ? -1 : 0) : m_Active;
	Apply(((Start + Step) % Count + Count) % Count);
}

bool CSdProfiles::Load()
{
	m_vProfiles.clear();
	m_Active = -1;

	void *pData;
	unsigned Length;
	if(!Storage()->ReadFile(PROFILES_FILE, IStorage::TYPE_SAVE, &pData, &Length))
		return true; // no profiles yet

	json_settings Settings{};
	char aError[256];
	json_value *pJson = JsonParseEx(&Settings, static_cast<const json_char *>(pData), Length, aError);
	free(pData);
	if(pJson == nullptr)
	{
		log_error("sd-client", "Failed to parse %s: %s", PROFILES_FILE, aError);
		return false;
	}
	if(pJson->type == json_object)
	{
		const json_value &Profiles = (*pJson)["profiles"];
		if(Profiles.type == json_array)
		{
			for(unsigned i = 0; i < Profiles.u.array.length; ++i)
			{
				const json_value &Entry = Profiles[i];
				if(Entry.type != json_object)
					continue;
				SSdProfile Profile;
				const json_value &ProfileName = Entry["profile"];
				if(ProfileName.type != json_string)
					continue;
				str_copy(Profile.m_aProfileName, ProfileName.u.string.ptr);
				if(Entry["name"].type == json_string)
					str_copy(Profile.m_aName, Entry["name"].u.string.ptr);
				if(Entry["clan"].type == json_string)
					str_copy(Profile.m_aClan, Entry["clan"].u.string.ptr);
				if(Entry["skin"].type == json_string)
					str_copy(Profile.m_aSkin, Entry["skin"].u.string.ptr);
				if(Entry["custom_color"].type == json_boolean)
					Profile.m_UseCustomColor = Entry["custom_color"].u.boolean != 0;
				if(Entry["color_body"].type == json_integer)
					Profile.m_ColorBody = (unsigned)Entry["color_body"].u.integer;
				if(Entry["color_feet"].type == json_integer)
					Profile.m_ColorFeet = (unsigned)Entry["color_feet"].u.integer;
				if(Entry["country"].type == json_integer)
					Profile.m_Country = (int)Entry["country"].u.integer;
				if(Entry["emote"].type == json_integer)
					Profile.m_Emote = (int)Entry["emote"].u.integer;
				if(Find(Profile.m_aProfileName) < 0)
					m_vProfiles.push_back(Profile);
			}
		}
	}
	json_value_free(pJson);
	return true;
}

bool CSdProfiles::Save()
{
	Storage()->CreateFolder("sd-client", IStorage::TYPE_SAVE);

	std::string Out = "{\n\t\"version\": 1,\n\t\"profiles\": [";
	for(size_t i = 0; i < m_vProfiles.size(); ++i)
	{
		const SSdProfile &Profile = m_vProfiles[i];
		char aBuf[64];
		Out += i == 0 ? "\n\t\t{" : ",\n\t\t{";
		Out += "\n\t\t\t\"profile\": ";
		JsonEscape(Profile.m_aProfileName, Out);
		Out += ",\n\t\t\t\"name\": ";
		JsonEscape(Profile.m_aName, Out);
		Out += ",\n\t\t\t\"clan\": ";
		JsonEscape(Profile.m_aClan, Out);
		Out += ",\n\t\t\t\"skin\": ";
		JsonEscape(Profile.m_aSkin, Out);
		Out += Profile.m_UseCustomColor ? ",\n\t\t\t\"custom_color\": true" : ",\n\t\t\t\"custom_color\": false";
		str_format(aBuf, sizeof(aBuf), ",\n\t\t\t\"color_body\": %u", Profile.m_ColorBody);
		Out += aBuf;
		str_format(aBuf, sizeof(aBuf), ",\n\t\t\t\"color_feet\": %u", Profile.m_ColorFeet);
		Out += aBuf;
		str_format(aBuf, sizeof(aBuf), ",\n\t\t\t\"country\": %d", Profile.m_Country);
		Out += aBuf;
		str_format(aBuf, sizeof(aBuf), ",\n\t\t\t\"emote\": %d\n\t\t}", Profile.m_Emote);
		Out += aBuf;
	}
	Out += m_vProfiles.empty() ? "]\n}\n" : "\n\t]\n}\n";

	const char *pTmp = "sd-client/profiles.json.tmp";
	IOHANDLE File = Storage()->OpenFile(pTmp, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
	{
		log_error("sd-client", "Failed to open %s for writing", pTmp);
		return false;
	}
	const bool Ok = io_write(File, Out.data(), (unsigned)Out.size()) == Out.size();
	io_close(File);
	if(!Ok)
		return false;
	return Storage()->RenameFile(pTmp, PROFILES_FILE, IStorage::TYPE_SAVE);
}

void CSdProfiles::OnInit()
{
	Load();
}

void CSdProfiles::ConProfile(IConsole::IResult *pResult, void *pUserData)
{
	CSdProfiles *pThis = static_cast<CSdProfiles *>(pUserData);
	const int Index = pThis->Find(pResult->GetString(0));
	if(Index < 0)
	{
		pThis->GameClient()->m_SdToasts.Add(CSdToasts::CHANNEL_COPY, "Profile not found", ColorRGBA(1.0f, 0.4f, 0.4f, 1.0f));
		return;
	}
	pThis->Apply(Index);
}

void CSdProfiles::ConProfileNext(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CSdProfiles *>(pUserData)->ApplyNext(1);
}

void CSdProfiles::ConProfilePrev(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CSdProfiles *>(pUserData)->ApplyNext(-1);
}

void CSdProfiles::OnConsoleInit()
{
	Console()->Register("sd_profile", "r[profile-name]", CFGFLAG_CLIENT, ConProfile, this, "Load an SD-Client profile by name");
	Console()->Register("sd_profile_next", "", CFGFLAG_CLIENT, ConProfileNext, this, "Quick switch to the next SD-Client profile");
	Console()->Register("sd_profile_prev", "", CFGFLAG_CLIENT, ConProfilePrev, this, "Quick switch to the previous SD-Client profile");
}
