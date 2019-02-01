#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdio>
#include "bcfnt.hpp"
#include <stdint.h>
#include <vector>
#include <Magick++.h>

FT_Library library;

void printHelp(char* prog)
{
    std::printf("Usage: %s [OPTIONS...] <input>\n", prog);

    std::printf(
        "  Options:\n"
        "    <input>                      Input file\n\n");
}

CMAP_s* makeCMAP(std::vector<std::pair<int, int>> pairs)
{
    CMAP_s* ret = new CMAP_s;
    ret->codeBegin = pairs.front().first;
    ret->codeBegin = pairs.back().first;
    ret->indexOffset = pairs.front().second;
    ret->next = 0;
    return ret;
}

CWDH_s* makeCWDH(FT_Face font, std::vector<std::pair<int, int>> pairs)
{
    CWDH_s* ret = (CWDH_s*) malloc(sizeof(CWDH_s) + 3 * pairs.size());
    ret->startIndex = pairs.front().second;
    ret->endIndex = pairs.back().second;
    ret->next = 0;
    for (size_t i = 0; i < pairs.size(); i++)
    {
        charWidthInfo_s* info = (charWidthInfo_s*)((char*)(ret) + sizeof(CWDH_s) + 3 * i);
        FT_Load_Glyph(font, pairs[i].second, FT_LOAD_RENDER);
        info->left = font->glyph->metrics.horiBearingX / 64; // Because they're 26.6 fractional pixel format
        info->glyphWidth = font->glyph->metrics.width / 64;
        info->charWidth = font->glyph->metrics.horiAdvance / 64;
    }
    return ret;
}

// Does both CMAP and CWDH data. Returns the number of indices
int parseEasyData(BCFNT& outFont, FT_Face font)
{
    bool continuous = false;
    uint16_t lastValidCodepoint = 0xFFFD;
    std::vector<std::pair<int, int>> indices;
    indices.push_back({0xFFFD, 0}); // The missing glyph
    int processedChars = 1;
    for (uint16_t i = 0; i < 0xFFFF; i++)
    {
        int index = FT_Get_Char_Index(font, i);
        if (index != 0)
        {
            processedChars++;
            if (lastValidCodepoint == i - 1)
            {
                indices.push_back({i, index});
            }
            else
            {
                outFont.addCMAP(makeCMAP(indices), sizeof(CMAP_s));
                outFont.addCWDH(makeCWDH(font, indices), sizeof(CWDH_s) + 3 * indices.size());
                indices.clear();
                indices.push_back({i, index});
            }
            lastValidCodepoint = i;
        }
    }
    outFont.addCMAP(makeCMAP(indices), sizeof(CMAP_s));
    outFont.addCWDH(makeCWDH(font, indices), sizeof(CWDH_s) + 3 * indices.size());
    return processedChars;
}

void swizzle(Magick::PixelPacket* p, bool reverse)
{
  // swizzle foursome table
    static const unsigned char table[][4] =
    {
        {  2,  8, 16,  4, },
        {  3,  9, 17,  5, },
        {  6, 10, 24, 20, },
        {  7, 11, 25, 21, },
        { 14, 26, 28, 22, },
        { 15, 27, 29, 23, },
        { 34, 40, 48, 36, },
        { 35, 41, 49, 37, },
        { 38, 42, 56, 52, },
        { 39, 43, 57, 53, },
        { 46, 58, 60, 54, },
        { 47, 59, 61, 55, },
    };

    if(!reverse)
    {
        // swizzle each foursome
        for(const auto &entry: table)
        {
            Magick::Color tmp = p[entry[0]];
            p[entry[0]]       = p[entry[1]];
            p[entry[1]]       = p[entry[2]];
            p[entry[2]]       = p[entry[3]];
            p[entry[3]]       = tmp;
        }
    }
    else
    {
        // unswizzle each foursome
        for(const auto &entry: table)
        {
            Magick::Color tmp = p[entry[3]];
            p[entry[3]]       = p[entry[2]];
            p[entry[2]]       = p[entry[1]];
            p[entry[1]]       = p[entry[0]];
            p[entry[0]]       = tmp;
        }
    }
}

void swizzle(Magick::Image &img, bool reverse)
{
    Magick::Pixels cache(img);
    size_t height = img.rows();
    size_t width  = img.columns();

    // (un)swizzle each tile
    for(size_t j = 0; j < height; j += 8)
    {
        for(size_t i = 0; i < width; i += 8)
        {
            Magick::PixelPacket* p = cache.get(i, j, 8, 8);
            swizzle(p, reverse);
            cache.sync();
        }
    }
}

void parseImageData(BCFNT& outFont, FT_Face font, const int indices)
{
    for (int i = 0; i < indices; i += 170)
    {
        Magick::Image image(Magick::Geometry(256, 512), Magick::Color("#00000000"));
        for (int j = 0; j < 170; j++) // 10 x 17 * 24 x 30 = 240 x 510 which is a good size
        {
            if (i + j < indices)
            {
                if (FT_Load_Glyph(font, i + j, FT_LOAD_RENDER))
                {
                    printf("Cleanup on line 152");
                }
                int x = (j % 10) * 24;
                int y = (j / 10) * 30;
                for (int px = 0; px < font->glyph->bitmap.width; px++)
                {
                    for (int py = 0; py < font->glyph->bitmap.rows; py++)
                    {
                        FT_Render_Glyph(font->glyph, FT_RENDER_MODE_NORMAL);
                        uint8_t pixelVal = font->glyph->bitmap.buffer[py * font->glyph->bitmap.width + px];
                        Magick::PixelPacket* pixel = image.setPixels(x + px, y + py, 1, 1);
                        pixel->blue = pixelVal;
                        pixel->green = pixelVal;
                        pixel->red = pixelVal;
                        if (pixelVal)
                        {
                            pixel->opacity = 0xFF;
                        }
                        else
                        {
                            pixel->opacity = 0;
                        }
                        image.syncPixels();
                    }
                }
            }
        }
        swizzle(image, false);
        image.magick("RGBA");
        Magick::Blob rawData;
        image.write(&rawData);
        outFont.addSheet((uint8_t*)rawData.data());
    }
}

int main(int argc, char* argv[])
{
    Magick::InitializeMagick(*argv);
    int error;
    if (argc < 2)
    {
        printHelp(argv[0]);
        return 1;
    }
    if (error = FT_Init_FreeType(&library))
    {
        std::printf("Bad things have occured: %d\n", error);
        return error;
    }
    FT_Face font;
    error = FT_New_Face(library, argv[1], 0, &font);
    if (error == FT_Err_Unknown_File_Format)
    {
        std::printf("`%s` is not a recognized font format\n", argv[1]);
        return error;
    }
    else if (error)
    {
        std::printf("Reading the font file failed! Error code: %d\n", error);
        return error;
    }
    error = FT_Select_Charmap(font, FT_ENCODING_UNICODE);
    if (error)
    {
        std::printf("`%s` does not have a Unicode character map\n", argv[1]);
        return error;
    }
    // Sets glyph sizes
    FT_Set_Pixel_Sizes(font, 24, 30);

    BCFNT outFont(font);
    int indices = parseEasyData(outFont, font);
    // parseImageData(outFont, font, indices);

    auto fontData = outFont.toStruct();
    FILE* out = fopen("out.bcfnt", "wb");
    fwrite(fontData.first, 1, fontData.second, out);
    fclose(out);

    FT_Done_FreeType(library);
    return 0;
}