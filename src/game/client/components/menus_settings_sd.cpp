// SD-Client settings tab
#include "menus.h"

#include "sd_common.h"
#include "sd_toasts.h"

#include <base/color.h>
#include <base/fs.h>
#include <base/io.h>
#include <base/str.h>

#include <engine/client.h>
#include <engine/config.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>

// Resets every SD-Client config variable whose script name starts with pPrefix to its default value.
static void SdResetConfig(const char *pPrefix)
{
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) \
	if(str_startswith(#ScriptName, pPrefix)) \
		g_Config.m_##Name = Def;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) \
	if(str_startswith(#ScriptName, pPrefix)) \
		g_Config.m_##Name = Def;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) \
	if(str_startswith(#ScriptName, pPrefix)) \
		str_copy(g_Config.m_##Name, Def, Len);
#include <engine/shared/config_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR
}

void CMenus::RenderSettingsSd(CUIRect MainView)
{
	enum
	{
		PAGE_GENERAL = 0,
		PAGE_ABOVE_BELOW,
		PAGE_PROFILES,
		PAGE_FONTS,
		PAGE_ANTI_PING,
		PAGE_PLAYER_INFO,
		PAGE_HOOK_HIT,
		PAGE_GRENADE,
		NUM_PAGES
	};
	static const char *const s_apPageNames[NUM_PAGES] = {"General", "Above / Below", "Profiles", "Custom Fonts", "Anti-Ping", "Player Info", "Hook / Hit", "Grenade"};
	static const char *const s_apResetPrefixes[NUM_PAGES] = {"sd_", "sd_ab_", "", "sd_font", "sd_ping_", "sd_copy_", "sd_hh_", "sd_gr_"};
	static CButtonContainer s_aTabButtons[NUM_PAGES];
	static CButtonContainer s_aColorResets[16];
	static CButtonContainer s_SaveButton, s_ResetButton;
	static char s_aStatus[128] = "";

	CUIRect TabBar, Content, BottomBar, Button;
	MainView.HSplitTop(24.0f, &TabBar, &MainView);
	MainView.HSplitTop(8.0f, nullptr, &MainView);
	MainView.HSplitBottom(24.0f, &Content, &BottomBar);
	Content.HSplitBottom(6.0f, &Content, nullptr);

	const float TabWidth = TabBar.w / (float)NUM_PAGES;
	for(int i = 0; i < NUM_PAGES; ++i)
	{
		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);
		if(DoButton_MenuTab(&s_aTabButtons[i], s_apPageNames[i], g_Config.m_SdPage == i, &Button, IGraphics::CORNER_T))
			g_Config.m_SdPage = i;
	}
	const int Page = std::clamp((int)g_Config.m_SdPage, 0, NUM_PAGES - 1);

	// ---- widget helpers ----
	int ColorCounter = 0;
	const auto DoCheck = [&](CUIRect &Column, const char *pText, int *pValue) {
		CUIRect Row;
		Column.HSplitTop(20.0f, &Row, &Column);
		if(DoButton_CheckBox(pValue, pText, *pValue, &Row))
			*pValue ^= 1;
	};
	const auto DoSlider = [&](CUIRect &Column, const char *pText, int *pValue, int Min, int Max, const char *pSuffix = "") {
		CUIRect Row;
		Column.HSplitTop(20.0f, &Row, &Column);
		Column.HSplitTop(2.0f, nullptr, &Column);
		Ui()->DoScrollbarOption(pValue, pValue, &Row, pText, Min, Max, &CUi::ms_LinearScrollbarScale, 0u, pSuffix);
	};
	const auto DoColor = [&](CUIRect &Column, const char *pText, unsigned *pValue, unsigned DefaultValue) {
		DoLine_ColorPicker(&s_aColorResets[ColorCounter++ % 16], 22.0f, 13.0f, 2.0f, &Column, pText, pValue, SdColor(DefaultValue), false);
	};
	const auto DoLabelRow = [&](CUIRect &Column, const char *pText, float Size = 12.0f, float Height = 18.0f) {
		CUIRect Row;
		Column.HSplitTop(Height, &Row, &Column);
		Ui()->DoLabel(&Row, pText, Size, TEXTALIGN_ML);
	};
	const auto DoHeading = [&](CUIRect &Column, const char *pText) {
		CUIRect Row;
		Column.HSplitTop(22.0f, &Row, &Column);
		Ui()->DoLabel(&Row, pText, 15.0f, TEXTALIGN_ML);
	};
	const auto DoDropDownRow = [&](CUIRect &Column, CUIRect &Rect, CUIRect *pRowOut = nullptr) {
		Column.HSplitTop(22.0f, &Rect, &Column);
		Column.HSplitTop(2.0f, nullptr, &Column);
	};

	CUIRect Row;
	CUIRect Left, Right;
	Content.VSplitMid(&Left, &Right, 16.0f);

	char aBuf[256];

	if(Page == PAGE_GENERAL)
	{
		DoHeading(Left, "SD-Client");
		DoCheck(Left, "Enable SD-Client features", &g_Config.m_SdEnabled);
		Left.HSplitTop(8.0f, nullptr, &Left);
		DoLabelRow(Left, "Modified DDNet client. Compatible with the original DDNet servers:");
		DoLabelRow(Left, "SD-Client only reads data the server already sends and never");
		DoLabelRow(Left, "sends additional game actions.");
		Left.HSplitTop(8.0f, nullptr, &Left);
		DoLabelRow(Left, "Custom console commands (bindable):");
		DoLabelRow(Left, "sd_copy_name / _clan / _skin / _skin_color / _full [id]");
		DoLabelRow(Left, "sd_profile <name>   sd_profile_next   sd_profile_prev");

		DoHeading(Right, "Features");
		DoCheck(Right, "Above / Below player", &g_Config.m_SdAbEnable);
		DoCheck(Right, "Hook / Hit notifications", &g_Config.m_SdHhEnable);
		DoCheck(Right, "Grenade trajectory", &g_Config.m_SdGrEnable);
		DoCheck(Right, "Ping display", &g_Config.m_SdPingShow);
		DoCheck(Right, "Copy notifications", &g_Config.m_SdCopyToast);
		Right.HSplitTop(8.0f, nullptr, &Right);
		str_format(aBuf, sizeof(aBuf), "Profiles saved: %d", GameClient()->m_SdProfiles.Num());
		DoLabelRow(Right, aBuf);
		str_format(aBuf, sizeof(aBuf), "Main font: %s", g_Config.m_SdFontFile[0] ? g_Config.m_SdFontFile : "default");
		DoLabelRow(Right, aBuf);
	}
	else if(Page == PAGE_ABOVE_BELOW)
	{
		DoHeading(Left, "Above / Below player");
		DoCheck(Left, "Show Above / Below", &g_Config.m_SdAbEnable);
		DoCheck(Left, "Notifications on change", &g_Config.m_SdAbNotify);
		DoCheck(Left, "Notify when above", &g_Config.m_SdAbNotifyAbove);
		DoCheck(Left, "Notify when at the same height", &g_Config.m_SdAbNotifySame);
		DoCheck(Left, "Notify when below", &g_Config.m_SdAbNotifyBelow);
		Left.HSplitTop(4.0f, nullptr, &Left);
		DoSlider(Left, "Same height tolerance", &g_Config.m_SdAbSameTolerance, 0, 64, " u");
		DoSlider(Left, "Max listed players", &g_Config.m_SdAbMaxLines, 1, 16);
		DoLabelRow(Left, "Above = the other tee is higher (smaller Y; the DDNet Y axis points down).", 10.0f, 16.0f);

		DoHeading(Right, "Appearance");
		DoSlider(Right, "Text X position", &g_Config.m_SdAbX, 0, 100, "%");
		DoSlider(Right, "Text Y position", &g_Config.m_SdAbY, 0, 100, "%");
		DoSlider(Right, "Text size", &g_Config.m_SdAbSize, 6, 60);
		DoColor(Right, "Text color", &g_Config.m_SdAbColor, 0x0000FF);
		Right.HSplitTop(6.0f, nullptr, &Right);
		DoLabelRow(Right, "Tracked player:");

		// Player dropdown: "All visible players" + every connected player
		static char s_aaNames[MAX_CLIENTS + 1][MAX_NAME_LENGTH + 8];
		static const char *s_apNames[MAX_CLIENTS + 1];
		static int s_aIds[MAX_CLIENTS + 1];
		int Num = 0;
		str_copy(s_aaNames[Num], "All visible players");
		s_aIds[Num] = -1;
		s_apNames[Num] = s_aaNames[Num];
		Num++;
		int Current = 0;
		for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
		{
			if(!GameClient()->m_aClients[ClientId].m_Active || ClientId == GameClient()->m_aLocalIds[0])
				continue;
			str_copy(s_aaNames[Num], GameClient()->m_aClients[ClientId].m_aName);
			s_aIds[Num] = ClientId;
			s_apNames[Num] = s_aaNames[Num];
			if(ClientId == g_Config.m_SdAbTarget)
				Current = Num;
			Num++;
		}
		CUIRect DropDownRect;
		DoDropDownRow(Right, DropDownRect);
		static CUi::SDropDownState s_TargetState;
		static CScrollRegion s_TargetScroll;
		s_TargetState.m_SelectionPopupContext.m_pScrollRegion = &s_TargetScroll;
		const int NewTarget = Ui()->DoDropDown(&DropDownRect, Current, s_apNames, Num, s_TargetState);
		if(NewTarget != Current)
			g_Config.m_SdAbTarget = s_aIds[NewTarget];
	}
	else if(Page == PAGE_PROFILES)
	{
		CSdProfiles &Profiles = GameClient()->m_SdProfiles;
		static CListBox s_ListBox;
		static int s_Selected = -1;
		static SSdProfile s_Edit;
		static CLineInputBuffered<32> s_ProfileNameInput;
		static CLineInputBuffered<MAX_NAME_LENGTH> s_NameInput;
		static CLineInputBuffered<MAX_CLAN_LENGTH> s_ClanInput;
		static CLineInputBuffered<MAX_SKIN_LENGTH> s_SkinInput;
		static CButtonContainer s_NewButton, s_SaveProfileButton, s_LoadButton, s_DeleteButton, s_CaptureButton, s_BodyReset, s_FeetReset;
		static bool s_Init = false;

		const auto FillEdit = [&](const SSdProfile &Profile) {
			s_Edit = Profile;
			s_ProfileNameInput.Set(Profile.m_aProfileName);
			s_NameInput.Set(Profile.m_aName);
			s_ClanInput.Set(Profile.m_aClan);
			s_SkinInput.Set(Profile.m_aSkin);
		};
		const auto ReadEdit = [&]() {
			str_copy(s_Edit.m_aProfileName, s_ProfileNameInput.GetString());
			str_copy(s_Edit.m_aName, s_NameInput.GetString());
			str_copy(s_Edit.m_aClan, s_ClanInput.GetString());
			str_copy(s_Edit.m_aSkin, s_SkinInput.GetString());
		};
		if(!s_Init)
		{
			s_Init = true;
			FillEdit(Profiles.FromCurrentSettings("New profile"));
		}

		// left: list
		CUIRect ListArea, ListButtons;
		Left.HSplitBottom(22.0f, &ListArea, &ListButtons);
		ListArea.HSplitBottom(4.0f, &ListArea, nullptr);
		s_ListBox.DoStart(22.0f, Profiles.Num(), 1, 3, s_Selected, &ListArea);
		for(int i = 0; i < Profiles.Num(); ++i)
		{
			const CListboxItem Item = s_ListBox.DoNextItem(&Profiles.Get(i), s_Selected == i);
			if(!Item.m_Visible)
				continue;
			CUIRect Label = Item.m_Rect;
			Label.VMargin(4.0f, &Label);
			str_format(aBuf, sizeof(aBuf), "%s%s  (%s)", Profiles.ActiveIndex() == i ? "> " : "", Profiles.Get(i).m_aProfileName, Profiles.Get(i).m_aName);
			Ui()->DoLabel(&Label, aBuf, 12.0f, TEXTALIGN_ML);
		}
		const int NewSelected = s_ListBox.DoEnd();
		if(NewSelected != s_Selected)
		{
			s_Selected = NewSelected;
			if(s_Selected >= 0 && s_Selected < Profiles.Num())
				FillEdit(Profiles.Get(s_Selected));
		}

		CUIRect LoadRect, DeleteRect;
		ListButtons.VSplitMid(&LoadRect, &DeleteRect, 4.0f);
		if(DoButton_Menu(&s_LoadButton, "Load (switch)", 0, &LoadRect) && s_Selected >= 0)
		{
			Profiles.Apply(s_Selected);
			str_copy(s_aStatus, "Profile loaded");
		}
		if(DoButton_Menu(&s_DeleteButton, "Delete", 0, &DeleteRect) && s_Selected >= 0)
		{
			Profiles.Remove(s_Selected);
			s_Selected = -1;
			str_copy(s_aStatus, "Profile deleted");
		}

		// right: editor
		DoHeading(Right, "Profile editor");
		CUIRect Label;
		const auto DoInput = [&](const char *pLabel, CLineInput *pInput) {
			Right.HSplitTop(20.0f, &Row, &Right);
			Right.HSplitTop(2.0f, nullptr, &Right);
			Row.VSplitLeft(90.0f, &Label, &Row);
			Ui()->DoLabel(&Label, pLabel, 12.0f, TEXTALIGN_ML);
			Ui()->DoEditBox(pInput, &Row, 12.0f);
		};
		DoInput("Profile name", &s_ProfileNameInput);
		DoInput("Player name", &s_NameInput);
		DoInput("Clan tag", &s_ClanInput);
		DoInput("Skin", &s_SkinInput);
		ReadEdit();

		int CustomColor = s_Edit.m_UseCustomColor ? 1 : 0;
		DoCheck(Right, "Custom skin colors", &CustomColor);
		s_Edit.m_UseCustomColor = CustomColor != 0;
		DoLine_ColorPicker(&s_BodyReset, 22.0f, 13.0f, 2.0f, &Right, "Body color", &s_Edit.m_ColorBody, ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f), false);
		DoLine_ColorPicker(&s_FeetReset, 22.0f, 13.0f, 2.0f, &Right, "Feet color", &s_Edit.m_ColorFeet, ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f), false);

		// emote dropdown
		static const char *s_apEmotes[6] = {"Normal", "Pain", "Happy", "Surprise", "Angry", "Blink"};
		CUIRect EmoteRow, EmoteLabel;
		DoDropDownRow(Right, EmoteRow);
		EmoteRow.VSplitLeft(90.0f, &EmoteLabel, &EmoteRow);
		Ui()->DoLabel(&EmoteLabel, "Emote", 12.0f, TEXTALIGN_ML);
		static CUi::SDropDownState s_EmoteState;
		static CScrollRegion s_EmoteScroll;
		s_EmoteState.m_SelectionPopupContext.m_pScrollRegion = &s_EmoteScroll;
		s_Edit.m_Emote = Ui()->DoDropDown(&EmoteRow, std::clamp(s_Edit.m_Emote, 0, 5), s_apEmotes, 6, s_EmoteState);

		// preview + flag
		CUIRect Preview;
		Right.HSplitTop(60.0f, &Preview, &Right);
		CUIRect TeeRect, FlagRect;
		Preview.VSplitLeft(70.0f, &TeeRect, &Preview);
		Preview.VSplitLeft(60.0f, &FlagRect, &Preview);
		{
			const CSkin *pDefaultSkin = GameClient()->m_Skins.Find("default");
			const CSkins::CSkinContainer *pContainer = GameClient()->m_Skins.FindContainerOrNullptr(s_Edit.m_aSkin[0] == '\0' ? "default" : s_Edit.m_aSkin);
			if(pContainer != nullptr && pContainer->IsSpecial())
				pContainer = nullptr;
			CTeeRenderInfo Info;
			Info.Apply(pContainer == nullptr || pContainer->Skin() == nullptr ? pDefaultSkin : pContainer->Skin().get());
			Info.ApplyColors(s_Edit.m_UseCustomColor, s_Edit.m_ColorBody, s_Edit.m_ColorFeet);
			Info.m_Size = 50.0f;
			RenderTools()->RenderIdleTeeAtCenter(&Info, vec2(TeeRect.x + TeeRect.w / 2.0f, TeeRect.y + TeeRect.h / 2.0f), std::clamp(s_Edit.m_Emote, 0, 5));
		}
		GameClient()->m_CountryFlags.Render(s_Edit.m_Country, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), FlagRect.x, FlagRect.y + 15.0f, 40.0f, 26.0f);
		Ui()->DoLabel(&Preview, "Preview (flag comes from\n'Capture current')", 11.0f, TEXTALIGN_ML);

		CUIRect NewRect, SaveRect, CaptureRect;
		Right.HSplitTop(22.0f, &Row, &Right);
		Row.VSplitLeft(Row.w / 3.0f - 2.0f, &NewRect, &Row);
		Row.VSplitMid(&SaveRect, &CaptureRect, 4.0f);
		if(DoButton_Menu(&s_NewButton, "New", 0, &NewRect))
		{
			ReadEdit();
			const int Index = Profiles.Add(s_Edit);
			if(Index >= 0)
			{
				s_Selected = Index;
				str_copy(s_aStatus, "Profile created");
			}
			else
				str_copy(s_aStatus, "Enter a unique profile name");
		}
		if(DoButton_Menu(&s_SaveProfileButton, "Save changes", 0, &SaveRect) && s_Selected >= 0)
		{
			ReadEdit();
			str_copy(s_aStatus, Profiles.Update(s_Selected, s_Edit) ? "Profile saved" : "Name empty or already used");
		}
		if(DoButton_Menu(&s_CaptureButton, "Capture current", 0, &CaptureRect))
		{
			FillEdit(Profiles.FromCurrentSettings(s_ProfileNameInput.GetString()[0] ? s_ProfileNameInput.GetString() : "New profile"));
			str_copy(s_aStatus, "Captured your current player settings");
		}
	}
	else if(Page == PAGE_FONTS)
	{
		CSdFonts &Fonts = GameClient()->m_SdFonts;
		static CListBox s_ListBox;
		static int s_Selected = -1;
		static CLineInputBuffered<IO_MAX_PATH_LENGTH> s_PathInput;
		static char s_aFontMessage[160] = "";
		static CButtonContainer s_ImportButton, s_UseButton, s_DefaultButton, s_DeleteButton, s_RefreshButton, s_FolderButton;
		static bool s_Init = false;
		if(!s_Init)
		{
			s_Init = true;
			Fonts.RefreshList();
		}

		DoHeading(Left, "Imported fonts");
		CUIRect ListArea, ListButtons;
		Left.HSplitBottom(22.0f, &ListArea, &ListButtons);
		ListArea.HSplitBottom(4.0f, &ListArea, nullptr);
		s_ListBox.DoStart(22.0f, Fonts.Fonts().size(), 1, 3, s_Selected, &ListArea);
		for(size_t i = 0; i < Fonts.Fonts().size(); ++i)
		{
			const CListboxItem Item = s_ListBox.DoNextItem(&Fonts.Fonts()[i], s_Selected == (int)i);
			if(!Item.m_Visible)
				continue;
			CUIRect Label = Item.m_Rect;
			Label.VMargin(4.0f, &Label);
			str_format(aBuf, sizeof(aBuf), "%s%s", Fonts.IsSelected(Fonts.Fonts()[i].m_File.c_str()) ? "> " : "", Fonts.Fonts()[i].m_File.c_str());
			Ui()->DoLabel(&Label, aBuf, 12.0f, TEXTALIGN_ML);
		}
		s_Selected = s_ListBox.DoEnd();

		CUIRect RefreshRect, FolderRect;
		ListButtons.VSplitMid(&RefreshRect, &FolderRect, 4.0f);
		if(DoButton_Menu(&s_RefreshButton, "Refresh list", 0, &RefreshRect))
			Fonts.RefreshList();
		if(DoButton_Menu(&s_FolderButton, "Open fonts folder", 0, &FolderRect))
		{
			Storage()->CreateFolder("sd-client", IStorage::TYPE_SAVE);
			Storage()->CreateFolder(CSdFonts::FONT_DIR, IStorage::TYPE_SAVE);
			char aPath[IO_MAX_PATH_LENGTH];
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, CSdFonts::FONT_DIR, aPath, sizeof(aPath));
			Client()->ViewFile(aPath);
		}

		DoHeading(Right, "Import (TTF / OTF)");
		Right.HSplitTop(20.0f, &Row, &Right);
		Ui()->DoEditBox(&s_PathInput, &Row, 12.0f);
		Right.HSplitTop(2.0f, nullptr, &Right);
		DoLabelRow(Right, "Paste the full path of the font file (e.g. C:\\Fonts\\My.ttf), then Import.", 10.0f, 15.0f);
		Right.HSplitTop(22.0f, &Row, &Right);
		Right.HSplitTop(2.0f, nullptr, &Right);
		if(DoButton_Menu(&s_ImportButton, "Import font", 0, &Row))
		{
			Fonts.Import(s_PathInput.GetString(), s_aFontMessage, sizeof(s_aFontMessage));
		}
		if(s_aFontMessage[0])
			DoLabelRow(Right, s_aFontMessage, 11.0f, 16.0f);

		CUIRect UseRect, DefaultRect, DeleteRect;
		Right.HSplitTop(22.0f, &Row, &Right);
		Row.VSplitLeft(Row.w / 3.0f - 2.0f, &UseRect, &Row);
		Row.VSplitMid(&DefaultRect, &DeleteRect, 4.0f);
		if(DoButton_Menu(&s_UseButton, "Use as main font", 0, &UseRect) && s_Selected >= 0 && s_Selected < (int)Fonts.Fonts().size())
		{
			str_copy(s_aFontMessage, Fonts.Select(Fonts.Fonts()[s_Selected].m_File.c_str()) ? "Font applied" : "Could not load this font");
		}
		if(DoButton_Menu(&s_DefaultButton, "Default font", 0, &DefaultRect))
		{
			Fonts.ResetToDefault();
			str_copy(s_aFontMessage, "Default font restored");
		}
		if(DoButton_Menu(&s_DeleteButton, "Delete file", 0, &DeleteRect) && s_Selected >= 0 && s_Selected < (int)Fonts.Fonts().size())
		{
			str_copy(s_aFontMessage, Fonts.Remove(Fonts.Fonts()[s_Selected].m_File.c_str()) ? "Font deleted" : "Cannot delete the font in use");
			s_Selected = -1;
		}

		Right.HSplitTop(8.0f, nullptr, &Right);
		str_format(aBuf, sizeof(aBuf), "Main font: %s", g_Config.m_SdFontFile[0] ? g_Config.m_SdFontFile : "default (DejaVu Sans)");
		DoLabelRow(Right, aBuf);
		DoLabelRow(Right, "Preview of the selected font:", 11.0f, 16.0f);
		const char *pFace = s_Selected >= 0 ? Fonts.EnsureLoaded(s_Selected) : nullptr;
		TextRender()->SetPreviewFont(pFace);
		CUIRect PreviewRect = Right;
		Ui()->DoLabel(&PreviewRect, "The quick brown fox jumps over the lazy dog\nABCDEFG abcdefg 0123456789 !?@#\n\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD1\x96\xD1\x82, \xD1\x81\xD0\xB2\xD1\x96\xD1\x82\xD0\xB5!  \xC3\x84\xC3\x96\xC3\x9C \xC3\xA4\xC3\xB6\xC3\xBC", 17.0f, TEXTALIGN_TL);
		TextRender()->SetPreviewFont(nullptr);
	}
	else if(Page == PAGE_ANTI_PING)
	{
		DoHeading(Left, "Ping display");
		DoCheck(Left, "Show current ping", &g_Config.m_SdPingShow);
		DoCheck(Left, "Show ping change", &g_Config.m_SdPingDelta);
		DoCheck(Left, "Show ping graph", &g_Config.m_SdPingGraph);
		DoCheck(Left, "Notify about high ping", &g_Config.m_SdPingWarn);
		DoSlider(Left, "High ping threshold", &g_Config.m_SdPingThreshold, 10, 1000, " ms");
		DoSlider(Left, "Text X position", &g_Config.m_SdPingX, 0, 100, "%");
		DoSlider(Left, "Text Y position", &g_Config.m_SdPingY, 0, 100, "%");
		DoSlider(Left, "Text size", &g_Config.m_SdPingSize, 6, 40);
		DoColor(Left, "Ping color", &g_Config.m_SdPingColor, 0x0000FF);
		DoColor(Left, "High ping color", &g_Config.m_SdPingHighColor, 0x00FF80);

		DoHeading(Right, "Connection diagnostics");
		const CSdPing::SStats Stats = GameClient()->m_SdPing.Stats();
		if(Stats.m_Valid)
		{
			str_format(aBuf, sizeof(aBuf), "Ping: %d ms (%+d in the last second)", Stats.m_Current, Stats.m_Delta);
			DoLabelRow(Right, aBuf, 12.0f, 16.0f);
			str_format(aBuf, sizeof(aBuf), "Min / avg / max: %d / %.0f / %d ms", Stats.m_Min, Stats.m_Average, Stats.m_Max);
			DoLabelRow(Right, aBuf, 12.0f, 16.0f);
			str_format(aBuf, sizeof(aBuf), "Jitter: %.1f ms   Prediction margin: %d ms", Stats.m_Jitter, Stats.m_PredictionTimeMs);
			DoLabelRow(Right, aBuf, 12.0f, 16.0f);
			str_format(aBuf, sizeof(aBuf), "High ping events: %d   Samples: %d", Stats.m_NumHighPingEvents, Stats.m_NumSamples);
			DoLabelRow(Right, aBuf, 12.0f, 16.0f);
		}
		else
		{
			DoLabelRow(Right, "Join a server to see live measurements.", 12.0f, 16.0f);
		}
		DoLabelRow(Right, "Values come from the server's own latency report;", 10.0f, 14.0f);
		DoLabelRow(Right, "SD-Client never fakes or lowers your real ping.", 10.0f, 14.0f);
		Right.HSplitTop(6.0f, nullptr, &Right);
		DoHeading(Right, "DDNet prediction (display only)");
		DoCheck(Right, "Antiping prediction", &g_Config.m_ClAntiPing);
		DoCheck(Right, "Predict grenades", &g_Config.m_ClAntiPingGrenade);
		DoCheck(Right, "Predict weapon projectiles", &g_Config.m_ClAntiPingWeapons);
		DoCheck(Right, "Smooth other players", &g_Config.m_ClAntiPingSmooth);
		DoSlider(Right, "Extra delay limit", &g_Config.m_ClAntiPingLimit, 0, 500, " ms");
	}
	else if(Page == PAGE_PLAYER_INFO)
	{
		static CButtonContainer s_TestButton;
		DoHeading(Left, "Player information copy");
		DoCheck(Left, "Show notification after copying", &g_Config.m_SdCopyToast);
		DoSlider(Left, "Notification duration", &g_Config.m_SdCopyDuration, 300, 10000, " ms");
		DoSlider(Left, "Text X position", &g_Config.m_SdCopyX, 0, 100, "%");
		DoSlider(Left, "Text Y position", &g_Config.m_SdCopyY, 0, 100, "%");
		DoSlider(Left, "Text size", &g_Config.m_SdCopySize, 6, 40);
		DoColor(Left, "Text color", &g_Config.m_SdCopyColor, 0x55FF80);
		Left.HSplitTop(6.0f, nullptr, &Left);
		Left.HSplitTop(22.0f, &Row, &Left);
		if(DoButton_Menu(&s_TestButton, "Test: copy my own info", 0, &Row))
		{
			const int LocalId = GameClient()->m_aLocalIds[0];
			if(!GameClient()->m_SdPlayerCopy.Copy(LocalId, CSdPlayerCopy::COPY_FULL))
				GameClient()->m_SdToasts.Add(CSdToasts::CHANNEL_COPY, "Join a server first", ColorRGBA(1.0f, 0.4f, 0.4f, 1.0f));
		}

		DoHeading(Right, "How to copy");
		DoLabelRow(Right, "1. Open the scoreboard and click a player:");
		DoLabelRow(Right, "   Copy Name / Clan Tag / Skin / Skin Color / Full Info.");
		DoLabelRow(Right, "2. Or bind a key to copy the player under your cursor:");
		DoLabelRow(Right, "   bind x sd_copy_full   (also sd_copy_name,");
		DoLabelRow(Right, "   sd_copy_clan, sd_copy_skin, sd_copy_skin_color)");
		DoLabelRow(Right, "3. Console: sd_copy_full <client id>");
	}
	else if(Page == PAGE_HOOK_HIT)
	{
		static CButtonContainer s_TestButton;
		DoHeading(Left, "Hook / hit notifications");
		DoCheck(Left, "Enable notifications", &g_Config.m_SdHhEnable);
		CUIRect ModeRow, ModeLabel;
		DoDropDownRow(Left, ModeRow);
		ModeRow.VSplitLeft(50.0f, &ModeLabel, &ModeRow);
		Ui()->DoLabel(&ModeLabel, "Mode", 12.0f, TEXTALIGN_ML);
		static const char *s_apModes[6] = {"Custom (checkboxes)", "Only my hooks", "Only my hits", "Only other players' hooks", "Only other players' hits", "Show everything"};
		static CUi::SDropDownState s_ModeState;
		static CScrollRegion s_ModeScroll;
		s_ModeState.m_SelectionPopupContext.m_pScrollRegion = &s_ModeScroll;
		g_Config.m_SdHhMode = Ui()->DoDropDown(&ModeRow, g_Config.m_SdHhMode, s_apModes, 6, s_ModeState);
		DoLabelRow(Left, "MY ACTIONS", 12.0f, 18.0f);
		DoCheck(Left, "Show my hooks", &g_Config.m_SdHhMyHooks);
		DoCheck(Left, "Show my hits", &g_Config.m_SdHhMyHits);
		DoLabelRow(Left, "OTHER PLAYERS", 12.0f, 18.0f);
		DoCheck(Left, "Show other players' hooks", &g_Config.m_SdHhOtherHooks);
		DoCheck(Left, "Show other players' hits", &g_Config.m_SdHhOtherHits);
		if(g_Config.m_SdHhMode != 0)
			DoLabelRow(Left, "(checkboxes apply only in Custom mode)", 10.0f, 14.0f);
		DoLabelRow(Left, "Hits = hammer hits (real hammer-hit events).", 10.0f, 14.0f);

		DoHeading(Right, "Appearance");
		DoSlider(Right, "Duration", &g_Config.m_SdHhDuration, 300, 15000, " ms");
		DoSlider(Right, "Text size", &g_Config.m_SdHhSize, 6, 40);
		DoSlider(Right, "Max lines", &g_Config.m_SdHhMaxLines, 1, 20);
		DoSlider(Right, "Position X", &g_Config.m_SdHhX, 0, 100, "%");
		DoSlider(Right, "Position Y", &g_Config.m_SdHhY, 0, 100, "%");
		DoColor(Right, "My actions color", &g_Config.m_SdHhColorMine, 0x55FF80);
		DoColor(Right, "Other players color", &g_Config.m_SdHhColorOther, 0x2BFF80);
		Right.HSplitTop(4.0f, nullptr, &Right);
		Right.HSplitTop(22.0f, &Row, &Right);
		if(DoButton_Menu(&s_TestButton, "Show preview notifications", 0, &Row))
		{
			GameClient()->m_SdToasts.Add(CSdToasts::CHANNEL_HOOK_HIT, "You: hook", SdColor(g_Config.m_SdHhColorMine));
			GameClient()->m_SdToasts.Add(CSdToasts::CHANNEL_HOOK_HIT, "You hit Player", SdColor(g_Config.m_SdHhColorMine));
			GameClient()->m_SdToasts.Add(CSdToasts::CHANNEL_HOOK_HIT, "Other hooked You", SdColor(g_Config.m_SdHhColorOther));
		}
	}
	else if(Page == PAGE_GRENADE)
	{
		DoHeading(Left, "Grenade trajectory");
		DoCheck(Left, "Show trajectory while holding the grenade launcher", &g_Config.m_SdGrEnable);
		DoCheck(Left, "Mark the impact point", &g_Config.m_SdGrMarker);
		DoSlider(Left, "Opacity", &g_Config.m_SdGrAlpha, 5, 100, "%");
		DoSlider(Left, "Line thickness", &g_Config.m_SdGrWidth, 1, 12);
		DoColor(Left, "Line color", &g_Config.m_SdGrColor, 0x2BFF80);

		DoHeading(Right, "How it works");
		DoLabelRow(Right, "The path uses the game's projectile equations (CalcPos),");
		DoLabelRow(Right, "the grenade speed, curvature and lifetime of the current");
		DoLabelRow(Right, "tune zone, and the map collision. Grenades explode on");
		DoLabelRow(Right, "impact (no bounce). The line is drawn locally only.");
	}

	// ---- bottom bar ----
	CUIRect SaveRect, ResetRect, StatusRect;
	BottomBar.VSplitLeft(140.0f, &SaveRect, &BottomBar);
	BottomBar.VSplitLeft(8.0f, nullptr, &BottomBar);
	BottomBar.VSplitLeft(180.0f, &ResetRect, &BottomBar);
	BottomBar.VSplitLeft(8.0f, nullptr, &StatusRect);
	if(DoButton_Menu(&s_SaveButton, "Save settings", 0, &SaveRect))
	{
		str_copy(s_aStatus, ConfigManager()->Save() ? "Settings saved" : "Saving failed");
	}
	if(Page != PAGE_PROFILES)
	{
		if(DoButton_Menu(&s_ResetButton, Page == PAGE_GENERAL ? "Reset all SD settings" : "Reset this section", 0, &ResetRect))
		{
			SdResetConfig(s_apResetPrefixes[Page]);
			if(Page == PAGE_GENERAL || Page == PAGE_FONTS)
				GameClient()->m_SdFonts.ResetToDefault();
			g_Config.m_SdPage = Page;
			str_copy(s_aStatus, "Defaults restored");
		}
	}
	Ui()->DoLabel(&StatusRect, s_aStatus, 12.0f, TEXTALIGN_ML);
}
