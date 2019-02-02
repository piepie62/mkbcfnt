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
    const char *options[] = {
        "", "--help or -h", "show this usage",
    };
    std::printf("Usage: %s [OPTIONS...] <input>.ttf\n", prog);

    std::printf("Options:\n");
    for(int optIndex = 0; optIndex < sizeof(options) / sizeof(options[optIndex]); optIndex += 3){
        std::printf("%-2s  %-12s  %s\n", options[optIndex], options[optIndex + 1], options[optIndex + 2]);
    }
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

inline void swapPixel(Magick::PixelPacket& p1, Magick::PixelPacket& p2)
{
    Magick::Color tmp = p1;
    p1 = p2;
    p2 = tmp;
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

    // (un)swizzle each pair
    swapPixel(p[12], p[18]);
    swapPixel(p[13], p[19]);
    swapPixel(p[44], p[50]);
    swapPixel(p[45], p[51]);
}

/** @brief Swizzle an image (Morton order)
 *  @param[in] img     Image to swizzle
 *  @param[in] reverse Whether to unswizzle
 */
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
    const int baseline = -font->size->metrics.ascender / 64;
    for (int page = 0; page < indices + 170; page += 170)
    {
        Magick::Image image(Magick::Geometry(256, 512), Magick::Color("#00000000"));
        Magick::Pixels cache(image);
        for (int glyph = 0; glyph < 170; glyph++) // 10 x 17 * 24 x 30 = 240 x 510 which is a good size
        {
            if (page + glyph < indices)
            {
                // if (FT_Load_Glyph(font, page + glyph, FT_LOAD_RENDER))
                // {
                //     printf("Cleanup on line 152");
                // }
                int x = (glyph % 10) * 24;
                int y = (glyph / 10) * 30;
                Magick::PixelPacket* glyphData = cache.get(x, y, 24, 30);
                for (int gy = 0; gy < font->glyph->bitmap.rows; gy++)
                {
                    for (int gx = 0; gx < font->glyph->bitmap.width; gx++)
                    {
                        FT_Render_Glyph(font->glyph, FT_RENDER_MODE_NORMAL);
                        int px = gx + font->glyph->bitmap_left;
                        int py = gy + (baseline - font->glyph->bitmap_top);
                        uint8_t pixelVal = font->glyph->bitmap.buffer[gy * font->glyph->bitmap.width + gx];
                        glyphData[py * 24 + px].red = 0;
                        glyphData[py * 24 + px].blue = 0;
                        glyphData[py * 24 + px].green = 0;
                        glyphData[py * 24 + px].opacity = pixelVal;
                    }
                }
                cache.sync();
            }
            else
            {
                break;
            }
        }
        swizzle(image, false);
        uint8_t* data = (uint8_t*)malloc(128*512); // A4
        Magick::PixelPacket* pixelData = cache.get(0, 0, 256, 512);
        for (int pixel = 0; pixel < 256*512; pixel += 2)
        {
            data[pixel / 2] = (pixelData[pixel].opacity / 16) | ((pixelData[pixel + 1].opacity / 16) << 4);
        }
        outFont.addSheet(data);
        free(data);
    }
}

int mkbcfnt(char* path, char* input)
{
    Magick::InitializeMagick(path);
    int error;
    if (error = FT_Init_FreeType(&library))
    {
        std::printf("Bad things have occured: %d\n", error);
        return error;
    }
    FT_Face font;
    error = FT_New_Face(library, input, 0, &font);
    if (error == FT_Err_Unknown_File_Format)
    {
        std::printf("`%s` is not a recognized font format\n", input);
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
        std::printf("`%s` does not have a Unicode character map\n", input);
        return error;
    }
    // Sets glyph sizes
    FT_Set_Pixel_Sizes(font, 24, 30);

    BCFNT outFont(font);
    int indices = parseEasyData(outFont, font);
    parseImageData(outFont, font, indices); // Currently crashes

    auto fontData = outFont.toStruct();
    FILE* out = fopen("out.bcfnt", "wb");
    fwrite(fontData.first, 1, fontData.second, out);
    fclose(out);

    FT_Done_FreeType(library);
    return 0;
}

/* dispatch option char */
bool getOptionChar(char* prog, const char opt)
{
    switch(opt)
    {
    case 'h':
        printHelp(prog);
        return true;
    }
    return false;
}

/* dispatch option string */
bool getOptionStr(char* prog, const char* optStr)
{
    if(strcmp(optStr, "help") == 0){
        printHelp(prog);
    }
    else{
        return false;
    }
    return true;
}

int main(int argc, char* argv[])
{
    int argi = 1;
    int argci;

    if(argc < 2){
        printHelp(argv[0]);
    }
    else{
        /* options */
        while((argi < argc) && (argv[argi][0] == '-'))
        {
            if(argv[argi][1] == '-'){
                /* --string */
                if(!getOptionStr(argv[0], &argv[argi][2])){
                    printf("Invalid option.\n");
                    return 1;
                }
            }
            else{
                /* -letters */
                argci = 1;
                while(argv[argi][argci] != '\0')
                {
                    if(!getOptionChar(argv[0], argv[argi][argci])){
                        printf("Invalid option.\n");
                        return 1;
                    }
                    argci ++;
                }
            }
            argi++;
        }
    }

    /* input files */
    for(; argi < argc; argi ++){
        mkbcfnt(*argv, argv[argi]);
    }
    return 0;
}
