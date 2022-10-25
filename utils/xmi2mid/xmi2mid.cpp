
#include "cvt_xmi2mid.hpp"
#include <stdio.h>
#include <sys/stat.h>
#if !defined(_WIN32)
#include <unistd.h>
#else
#include <io.h>
#define fileno(fd) _fileno(fd)
#define isatty(fd) _isatty(fd)
#endif

#include <string>


int main(int argc, char *argv[])
{
    int songNumber = 0;
    bool allMode = false;
    std::string outDir;
    std::vector<std::vector<uint8_t > > song_buf;

    (void)Convert_xmi2midi; /* Shut up the warning */

    if(argc != 2 && argc != 3 && argc != 4)
    {
        fprintf(stderr, "Usage: xmi2mid <midi-file> [song-number 0...N-1] [--all <out-dir>]\n");
        return 1;
    }

    std::string filename = std::string(argv[1]);

    if(argc > 2)
    {
        if(!strcmp(argv[2], "--all") && argc > 3)
        {
            allMode = true;
            outDir = std::string(argv[3]);
        }
        else
            songNumber = atoi(argv[2]);
    }

    FILE *fh = fopen(filename.c_str(), "rb");
    if(!fh)
    {
        fprintf(stderr, "Error opening file.\n");
        return 1;
    }

    struct stat st;
    if(fstat(fileno(fh), &st) != 0)
    {
        fprintf(stderr, "Error reading file status.\n");
        return 1;
    }

    size_t insize = (size_t)st.st_size;
    if(insize > 8 * 1024 * 1024)
    {
        fprintf(stderr, "File too large.\n");
        return 1;
    }

    uint8_t *filedata = new uint8_t[insize];
    if(fread(filedata, 1, insize, fh) != insize)
    {
        fprintf(stderr, "Error reading file data.\n");
        return 1;
    }

    if(Convert_xmi2midi_multi(filedata, insize, song_buf, XMIDI_CONVERT_NOCONVERSION) < 0)
    {
        fprintf(stderr, "Error converting XMI to SMF.\n");
        return 1;
    }

    if(allMode)
    {
        char outFile[2048];
        size_t len;
        size_t ls = filename.find_last_of('/');

        const char *fileName = ls != std::string::npos ? filename.c_str() + ls + 1 : NULL;
        if(!fileName)
            fileName = filename.c_str();

        ls = filename.find_last_of('.');
        const char *dot = ls != std::string::npos ? filename.c_str() + ls : NULL;

        if(dot)
            len = (dot - fileName);
        else
            len = strlen(fileName);

        for(size_t i = 0; i < song_buf.size(); ++i)
        {
            size_t outSize = song_buf[i].size();
            sprintf(outFile, "%s/%s-%u.mid", outDir.c_str(), std::string(fileName, len).c_str(), (unsigned)i);
            fprintf(stderr, "Writing file %s ...\n", outFile);

            FILE *out = fopen(outFile, "wb");
            if (!out || fwrite(song_buf[i].data(), 1, outSize, out) != outSize || fflush(out) != 0)
            {
                fprintf(stderr, "Error writing SMF data (%u, %s).\n", (unsigned)i, outFile);
                return 1;
            }
            fclose(out);
        }
    }
    else
    {
        FILE *out = stdout;
        if(isatty(fileno(out)))
        {
            fprintf(stderr, "Not writing SMF data on the text terminal.\n");
        }
        else
        {
            if (fwrite(song_buf[songNumber].data(), 1, song_buf[songNumber].size(), out) != song_buf[songNumber].size() || fflush(out) != 0)
            {
                fprintf(stderr, "Error writing SMF data.\n");
                return 1;
            }
        }
    }

    return 0;
}
