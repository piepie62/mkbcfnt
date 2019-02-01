#pragma once
#include <stdint.h>
#include <vector>
#include <variant>
#include <ft2build.h>
#include FT_FREETYPE_H

/// Character width information structure.
typedef struct
{
	int8_t left;       ///< Horizontal offset to draw the glyph with.
	uint8_t glyphWidth; ///< Width of the glyph.
	uint8_t charWidth;  ///< Width of the character, that is, horizontal distance to advance.
} charWidthInfo_s;

/// Font texture sheet information.
typedef struct
{
	uint8_t cellWidth;    ///< Width of a glyph cell.
	uint8_t cellHeight;   ///< Height of a glyph cell.
	uint8_t baselinePos;  ///< Vertical position of the baseline.
	uint8_t maxCharWidth; ///< Maximum character width.

	uint32_t sheetSize; ///< Size in bytes of a texture sheet.
	uint16_t nSheets;   ///< Number of texture sheets.
	uint16_t sheetFmt;  ///< GPU texture format (GPU_TEXCOLOR).

	uint16_t nRows;  ///< Number of glyphs per row per sheet.
	uint16_t nLines; ///< Number of glyph rows per sheet.

	uint16_t sheetWidth;  ///< Texture sheet width.
	uint16_t sheetHeight; ///< Texture sheet height.
	uint32_t sheetData;   ///< Pointer to texture sheet data.
} TGLP_s;

/// Font character width information block type.
typedef struct tag_CWDH_s CWDH_s;

/// Font character width information block structure.
struct tag_CWDH_s
{
	uint16_t startIndex; ///< First Unicode codepoint the block applies to.
	uint16_t endIndex;   ///< Last Unicode codepoint the block applies to.
	uint32_t next;   ///< Pointer to the next block.

	charWidthInfo_s widths[0]; ///< Table of character width information structures.
};

/// Font character map methods.
enum
{
	CMAP_TYPE_DIRECT = 0, ///< Identity mapping.
	CMAP_TYPE_TABLE = 1,  ///< Mapping using a table.
	CMAP_TYPE_SCAN = 2,   ///< Mapping using a list of mapped characters.
};

/// Font character map type.
typedef struct tag_CMAP_s CMAP_s;

/// Font character map structure.
struct tag_CMAP_s
{
	uint16_t codeBegin;     ///< First Unicode codepoint the block applies to.
	uint16_t codeEnd;       ///< Last Unicode codepoint the block applies to.
	uint16_t mappingMethod; ///< Mapping method.
	uint16_t reserved;
	uint32_t next;      ///< Pointer to the next map.

	union
	{
		uint16_t indexOffset;   ///< For CMAP_TYPE_DIRECT: index of the first glyph.
		uint16_t indexTable[0]; ///< For CMAP_TYPE_TABLE: table of glyph indices.
		/// For CMAP_TYPE_SCAN: Mapping data.
		struct
		{
			uint16_t nScanEntries; ///< Number of pairs.
			/// Mapping pairs.
			struct
			{
				uint16_t code;       ///< Unicode codepoint.
				uint16_t glyphIndex; ///< Mapped glyph index.
			} scanEntries[0];
		};
	};
};

/// Font information structure.
typedef struct
{
	uint32_t signature;   ///< Signature (FINF).
	uint32_t sectionSize; ///< Section size.

	uint8_t fontType;                  ///< Font type
	uint8_t lineFeed;                  ///< Line feed vertical distance.
	uint16_t alterCharIndex;           ///< Glyph index of the replacement character.
	charWidthInfo_s defaultWidth; ///< Default character width information.
	uint8_t encoding;                  ///< Font encoding (?)

	uint32_t tglp; ///< Pointer to texture sheet information.
	uint32_t cwdh; ///< Pointer to the first character width information block.
	uint32_t cmap; ///< Pointer to the first character map.

	uint8_t height;  ///< Font height.
	uint8_t width;   ///< Font width.
	uint8_t ascent;  ///< Font ascent.
	uint8_t padding;
} FINF_s;

/// Font structure.
typedef struct
{
	uint32_t signature;  ///< Signature (CFNU).
	uint16_t endianness; ///< Endianness constant (0xFEFF).
	uint16_t headerSize; ///< Header size.
	uint32_t version;    ///< Format version.
	uint32_t fileSize;   ///< File size.
	uint32_t nBlocks;    ///< Number of blocks.

	FINF_s finf; ///< Font information.
} CFNT_s;

class BCFNT
{
private:
	CFNT_s cfnt;
	TGLP_s* tglp = nullptr;
	uint8_t* sheetData = nullptr;
	std::vector<std::pair<CWDH_s*, size_t>> cwdhs;
	std::vector<std::pair<CMAP_s*, size_t>> cmaps;
public:
	BCFNT(FT_Face font);
	~BCFNT();
	// Literally just a pointer copy, so don't delete/free it afterward!
	void addCMAP(CMAP_s* cmap, uint32_t size);
	// Literally just a pointer copy, so don't delete/free it afterward!
	void addCWDH(CWDH_s* cwdh, uint32_t size);
	// Actually copies data in, so you're good to delete/free
	void addSheet(uint8_t* sheet);
	// Free this after you're done with it: pointer and size
	std::pair<CFNT_s*, size_t> toStruct();
};