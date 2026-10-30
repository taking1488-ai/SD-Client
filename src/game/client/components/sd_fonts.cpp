#include "sd_fonts.h"

#include <base/io.h>
#include <base/log.h>
#include <base/str.h>

#include <engine/client.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>

#include <algorithm>

static bool IsFontFileName(const char *pName)
{
	return str_endswith_nocase(pName, ".ttf") != nullptr || str_endswith_nocase(pName, ".otf") != nullptr;
}

static int ListFontsCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	if(IsDir || !IsFontFileName(pName))
		return 0;
	auto *pFonts = static_cast<std::vector<CSdFonts::SFont> *>(pUser);
	pFonts->push_back({pName, ""});
	return 0;
}

void CSdFonts::RefreshList()
{
	// keep already resolved face names
	std::vector<SFont> vOld;
	vOld.swap(m_vFonts);
	Storage()->CreateFolder("sd-client", IStorage::TYPE_SAVE);
	Storage()->CreateFolder(FONT_DIR, IStorage::TYPE_SAVE);
	Storage()->ListDirectory(IStorage::TYPE_SAVE, FONT_DIR, ListFontsCallback, &m_vFonts);
	std::sort(m_vFonts.begin(), m_vFonts.end(), [](const SFont &A, const SFont &B) { return str_comp_nocase(A.m_File.c_str(), B.m_File.c_str()) < 0; });
	for(SFont &Font : m_vFonts)
	{
		for(const SFont &Old : vOld)
		{
			if(Old.m_File == Font.m_File)
				Font.m_FaceName = Old.m_FaceName;
		}
	}
}

const char *CSdFonts::EnsureLoaded(int Index)
{
	if(Index < 0 || Index >= (int)m_vFonts.size())
		return nullptr;
	SFont &Font = m_vFonts[Index];
	if(!Font.m_FaceName.empty())
		return Font.m_FaceName.c_str();
	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "%s/%s", FONT_DIR, Font.m_File.c_str());
	char aFace[128];
	if(!TextRender()->LoadCustomFontFile(aPath, aFace, sizeof(aFace)))
		return nullptr;
	Font.m_FaceName = aFace;
	return Font.m_FaceName.c_str();
}

bool CSdFonts::IsSelected(const char *pFile) const
{
	return g_Config.m_SdFontFile[0] != '\0' && str_comp(g_Config.m_SdFontFile, pFile) == 0;
}

void CSdFonts::OnInit()
{
	RefreshList();
	if(g_Config.m_SdFontFile[0] != '\0' && !Select(g_Config.m_SdFontFile))
	{
		log_error("sd-client", "Could not load the saved custom font '%s', using the default font", g_Config.m_SdFontFile);
		g_Config.m_SdFontFile[0] = '\0';
	}
}

bool CSdFonts::Select(const char *pFile)
{
	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "%s/%s", FONT_DIR, pFile);
	if(!IsFontFileName(pFile) || !Storage()->FileExists(aPath, IStorage::TYPE_SAVE))
		return false;
	char aFace[128];
	if(!TextRender()->LoadCustomFontFile(aPath, aFace, sizeof(aFace)))
		return false;
	str_copy(g_Config.m_SdFontFile, pFile);
	m_PendingFace = aFace;
	m_ApplyPending = true;
	return true;
}

void CSdFonts::ResetToDefault()
{
	g_Config.m_SdFontFile[0] = '\0';
	m_PendingFace.clear();
	m_ApplyPending = true;
}

void CSdFonts::OnUpdate()
{
	if(!m_ApplyPending)
		return;
	m_ApplyPending = false;
	// Done between frames because changing the font clears the glyph atlas.
	TextRender()->SetPreviewFont(nullptr);
	if(m_PendingFace.empty())
		TextRender()->ResetDefaultFont();
	else if(!TextRender()->SetCustomDefaultFont(m_PendingFace.c_str()))
	{
		log_error("sd-client", "Failed to activate custom font '%s'", m_PendingFace.c_str());
		g_Config.m_SdFontFile[0] = '\0';
		TextRender()->ResetDefaultFont();
	}
	Client()->OnWindowResize(); // drop all cached text containers
}

bool CSdFonts::Remove(const char *pFile)
{
	if(IsSelected(pFile))
		return false;
	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "%s/%s", FONT_DIR, pFile);
	const bool Ok = Storage()->RemoveFile(aPath, IStorage::TYPE_SAVE);
	RefreshList();
	return Ok;
}

bool CSdFonts::Import(const char *pSourcePath, char *pMessage, int MessageSize)
{
	// strip quotes and whitespace ("Copy as path" in the Windows explorer adds quotes)
	char aSource[IO_MAX_PATH_LENGTH];
	str_copy(aSource, pSourcePath);
	str_utf8_trim_right(aSource);
	char *pStart = aSource;
	while(*pStart == ' ' || *pStart == '"')
		++pStart;
	size_t Len = str_length(pStart);
	while(Len > 0 && (pStart[Len - 1] == '"' || pStart[Len - 1] == ' '))
		pStart[--Len] = '\0';

	if(Len == 0)
	{
		str_copy(pMessage, "Enter the full path of a .ttf or .otf file first", MessageSize);
		return false;
	}
	if(!IsFontFileName(pStart))
	{
		str_copy(pMessage, "Only .ttf and .otf files are supported", MessageSize);
		return false;
	}

	const char *pName = pStart;
	for(const char *p = pStart; *p; ++p)
	{
		if(*p == '/' || *p == '\\')
			pName = p + 1;
	}
	char aFileName[128];
	str_copy(aFileName, pName);
	for(char *p = aFileName; *p; ++p)
	{
		if(*p == ':' || *p == '*' || *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|')
			*p = '_';
	}

	IOHANDLE Source = io_open(pStart, IOFLAG_READ);
	if(!Source)
	{
		str_copy(pMessage, "Could not open the file (check the path)", MessageSize);
		return false;
	}
	void *pData;
	unsigned DataSize;
	const bool ReadOk = io_read_all(Source, &pData, &DataSize);
	io_close(Source);
	if(!ReadOk || DataSize == 0 || DataSize > 64 * 1024 * 1024)
	{
		if(ReadOk)
			free(pData);
		str_copy(pMessage, "Could not read the file or it is too large", MessageSize);
		return false;
	}

	Storage()->CreateFolder("sd-client", IStorage::TYPE_SAVE);
	Storage()->CreateFolder(FONT_DIR, IStorage::TYPE_SAVE);
	char aDest[IO_MAX_PATH_LENGTH];
	str_format(aDest, sizeof(aDest), "%s/%s", FONT_DIR, aFileName);
	IOHANDLE Dest = Storage()->OpenFile(aDest, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!Dest)
	{
		free(pData);
		str_copy(pMessage, "Could not write into the user data directory", MessageSize);
		return false;
	}
	const bool WriteOk = io_write(Dest, pData, DataSize) == DataSize;
	io_close(Dest);
	free(pData);
	if(!WriteOk)
	{
		Storage()->RemoveFile(aDest, IStorage::TYPE_SAVE);
		str_copy(pMessage, "Could not write the font file", MessageSize);
		return false;
	}

	char aFace[128];
	if(!TextRender()->LoadCustomFontFile(aDest, aFace, sizeof(aFace)))
	{
		Storage()->RemoveFile(aDest, IStorage::TYPE_SAVE);
		str_copy(pMessage, "This file is not a valid font", MessageSize);
		return false;
	}
	RefreshList();
	str_format(pMessage, MessageSize, "Imported: %s", aFace);
	return true;
}
