#include "bcfnt.hpp"
#include <cstdlib>

static constexpr int PIXEL_SIZE = 1;

BCFNT::BCFNT(FT_Face font)
{
    FT_Load_Glyph(font, 0, FT_LOAD_RENDER);
    FT_Render_Glyph(font->glyph, FT_RENDER_MODE_NORMAL);
    cfnt.finf.lineFeed = font->size->metrics.height / 64;
    cfnt.finf.defaultWidth.left = font->glyph->metrics.horiBearingX / 64;
    cfnt.finf.defaultWidth.glyphWidth = font->glyph->metrics.width / 64;
    cfnt.finf.defaultWidth.charWidth = font->glyph->metrics.horiAdvance / 64;
    cfnt.finf.height = (font->bbox.yMax - font->bbox.yMin) / 64;
    cfnt.finf.width = (font->bbox.xMax - font->bbox.xMin) / 64;
    cfnt.finf.ascent = font->size->metrics.ascender / 64;
    cfnt.finf.padding = 0;

    tglp = new TGLP_s;
    tglp->cellHeight = (font->bbox.yMax - font->bbox.yMin) / 64;
    tglp->cellWidth = (font->bbox.xMax - font->bbox.xMin) / 64;
    tglp->baselinePos = -font->size->metrics.ascender / 64;
    tglp->maxCharWidth = font->size->metrics.max_advance / 64;

    tglp->nRows = 10;
    tglp->nLines = 17;

    tglp->sheetWidth = 128;
    tglp->sheetHeight = 512;
    tglp->sheetSize = tglp->sheetWidth * tglp->sheetHeight * PIXEL_SIZE; // 4-bit, so virtually multiplied by 2
    tglp->sheetFmt = 0x8; //0x0 is RGBA8, but 0xB is 4-bit alpha, which I probably want to work towards. This is going to be a nuisance
    tglp->nSheets = 0;
    tglp->sheetData = 0;
}

BCFNT::~BCFNT()
{
    delete[] tglp;
    free(sheetData);
    for (auto cwdh : cwdhs)
    {
        free(cwdh.first);
    }
    for (auto cmap : cmaps)
    {
        free(cmap.first);
    }
}

void BCFNT::addCMAP(CMAP_s* cmap, uint32_t size)
{
    cmaps.push_back({cmap, size});
}

void BCFNT::addCWDH(CWDH_s* cwdh, uint32_t size)
{
    cwdhs.push_back({cwdh, size});
}

void BCFNT::addSheet(uint8_t* sheet)
{
    if (!sheetData)
    {
        sheetData = (uint8_t*)malloc(tglp->sheetSize);
    }
    else
    {
        sheetData = (uint8_t*) realloc(sheetData, (tglp->nSheets + 1) * tglp->sheetSize);
    }
    std::copy(sheet, sheet + tglp->sheetSize * tglp->nSheets, sheetData + tglp->sheetSize * tglp->nSheets);
    tglp->nSheets++;
}

std::pair<CFNT_s*, size_t> BCFNT::toStruct()
{
    size_t size = sizeof(CFNT_s);
    for (auto cmap : cmaps)
    {
        size += cmap.second + 8; // Add CMAP header
    }
    for (auto cwdh : cwdhs)
    {
        size += cwdh.second + 8; // Add CWDH header
    }
    size += sizeof(TGLP_s) + 8; // Add TGLP header

    CFNT_s* font = (CFNT_s*)malloc(size);
    std::copy(&cfnt, &cfnt + 1, font);
    uint8_t* fontRaw = (uint8_t*) font; // For readability
    size_t writtenBytes = 0;
    font->signature = 0x544E4643;
    font->endianness = 0xFEFF;
    font->headerSize = 0x14;
    font->version = 0x03000000;
    font->fileSize = size;
    font->nBlocks = cmaps.size() + cwdhs.size() + 2; // CMAPs, CWDHs, FINF, and TGLP
    writtenBytes += 0x14;

    font->finf.signature = 0x464E4946;
    font->finf.sectionSize = sizeof(FINF_s);
    font->finf.fontType = 1; // ?????
    //font->finf.lineFeed Done in constructor
    //font->defaultWidth Done in constructor
    font->finf.encoding = 1; // ?????
    //TGLP, CWDH, and CMAP handled soon
    //Height, width, and ascent done in constructor
    writtenBytes += sizeof(FINF_s);

    //TGLP data handled in constructor, but endianization needed
    uint32_t totalSheetSize = tglp->sheetSize * tglp->nSheets;
    fontRaw[writtenBytes++] = 'T'; // TGLP header
    fontRaw[writtenBytes++] = 'G';
    fontRaw[writtenBytes++] = 'L';
    fontRaw[writtenBytes++] = 'P';
    *(uint32_t*)(fontRaw + writtenBytes) = sizeof(TGLP_s) + totalSheetSize + 8; // TGLP size
    writtenBytes += 4;
    font->finf.tglp = writtenBytes; // Set FINF TGLP pointer
    std::copy((uint8_t*)tglp, (uint8_t*)(tglp) + sizeof(TGLP_s), fontRaw + writtenBytes);
    writtenBytes += sizeof(TGLP_s);
    std::copy(sheetData, sheetData + totalSheetSize, fontRaw + writtenBytes);
    ((TGLP_s*)(fontRaw + font->finf.tglp))->sheetData = writtenBytes; // Set TGLP sheetData pointer
    writtenBytes += totalSheetSize;

    for (size_t i = 0; i < cwdhs.size(); i++)
    {
        fontRaw[writtenBytes++] = 'C';
        fontRaw[writtenBytes++] = 'W';
        fontRaw[writtenBytes++] = 'D';
        fontRaw[writtenBytes++] = 'H';
        *(uint32_t*)(fontRaw + writtenBytes) = cwdhs[i].second + 8;
        writtenBytes += 4;
        if (i == 0)
        {
            font->finf.cwdh = writtenBytes;
        }
        std::copy((uint8_t*)cwdhs[i].first, (uint8_t*)(cwdhs[i].first) + cwdhs[i].second, fontRaw + writtenBytes);
        writtenBytes += cwdhs[i].second;
    }
    
    for (size_t i = 0; i < cmaps.size(); i++)
    {
        fontRaw[writtenBytes++] = 'C';
        fontRaw[writtenBytes++] = 'M';
        fontRaw[writtenBytes++] = 'A';
        fontRaw[writtenBytes++] = 'P';
        *(uint32_t*)(fontRaw + writtenBytes) = cmaps[i].second + 8;
        writtenBytes += 4;
        if (i == 0)
        {
            font->finf.cmap = writtenBytes;
        }
        std::copy((uint8_t*)cmaps[i].first, (uint8_t*)(cmaps[i].first) + cmaps[i].second, fontRaw + writtenBytes);
        writtenBytes += cmaps[i].second;
    }

    if (writtenBytes != size)
    {
        printf("What have you done");
    }
    return {font, size};
}