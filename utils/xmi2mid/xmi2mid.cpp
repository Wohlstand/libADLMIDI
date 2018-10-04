/*
 *  XMI to MIDI conversion console tool
 *
 *  Copyright (C) 2018-2018 Vitaly Novichkov <admin@wohlnet.ru>
 *  Copyright (C) 2018-2018 Jean Pierre Cimalando
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; If not, see <http://www.gnu.org/licenses/>.
 */

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

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        fprintf(stderr, "Usage: xmi2mid <midi-file>\n");
        return 1;
    }

    const char *filename = argv[1];

    FILE *fh = fopen(filename, "rb");
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

    uint8_t *xmidata = NULL;
    uint32_t xmisize = 0;
    if(Convert_xmi2midi(filedata, insize, &xmidata, &xmisize, XMIDI_CONVERT_NOCONVERSION) < 0)
    {
        fprintf(stderr, "Error converting XMI to SMF.\n");
        return 1;
    }

    FILE *out = stdout;
    if(isatty(fileno(out)))
    {
        fprintf(stderr, "Not writing SMF data on the text terminal.\n");
    }
    else
    {
        if (fwrite(xmidata, 1, xmisize, out) != xmisize || fflush(out) != 0)
        {
            fprintf(stderr, "Error writing SMF data.\n");
            return 1;
        }
    }

    return 0;
}
