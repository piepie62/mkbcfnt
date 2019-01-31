#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdio>

FT_Library library;

void printHelp(char* prog)
{
    std::printf("Usage: %s [OPTIONS...] <input>\n", prog);

    std::printf(
        "  Options:\n"
        "    <input>                      Input file\n\n");
}

int main(int argc, char* argv[])
{
    int error;
    if (argc < 2)
    {
        printHelp(argv[0]);
        return 1;
    }
    if (error = FT_Init_FreeType(&library))
    {
        printf("Bad things have occured: %d", error);
        return error;
    }
    return 0;
}