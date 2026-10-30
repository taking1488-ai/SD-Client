// SD-Client: custom TTF/OTF fonts. Imported fonts are copied to sd-client/fonts in the user data directory.
#ifndef GAME_CLIENT_COMPONENTS_SD_FONTS_H
#define GAME_CLIENT_COMPONENTS_SD_FONTS_H

#include <game/client/component.h>

#include <string>
#include <vector>

class CSdFonts : public CComponent
{
public:
	static constexpr const char *FONT_DIR = "sd-client/fonts";

	struct SFont
	{
		std::string m_File; // file name inside FONT_DIR
		std::string m_FaceName; // filled once the font was loaded
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnUpdate() override;

	void RefreshList();
	const std::vector<SFont> &Fonts() const { return m_vFonts; }

	// Copies a TTF/OTF file from an absolute path into the font directory and validates it.
	bool Import(const char *pSourcePath, char *pMessage, int MessageSize);
	// Selects an imported font as the main font. The change is applied between frames.
	bool Select(const char *pFile);
	// Returns to the font shipped with the game.
	void ResetToDefault();
	// Deletes an imported font file (it must not be the selected one).
	bool Remove(const char *pFile);
	// Loads the font (if needed) and returns its face name for previews, or nullptr.
	const char *EnsureLoaded(int Index);
	bool IsSelected(const char *pFile) const;

private:
	std::vector<SFont> m_vFonts;
	bool m_ApplyPending = false;
	std::string m_PendingFace; // empty = default font
};

#endif
