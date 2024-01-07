#ifndef __COMMON_H__
#define __COMMON_H__

// Defined functions:
//static void RemoveNewLines(char* String);
//static void RemoveQuotationMarks(char* String);
//static void ReadFilename(char* buffer, size_t bufsize);
//INLINE void DblClickWait(const char* argv0);
//static void PrintMinSec(const UINT32 SamplePos, char* TempStr); [WIN32 only]

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wunused-function"
#endif


#include <string.h>
#include <stdio.h>      // for sprintf()

#ifdef _WIN32
#include <conio.h>      // for _getch()
#include <windows.h>    // for OemToChar()
#else
#include <limits.h>
#define MAX_PATH    PATH_MAX
#endif


#ifdef _MSC_VER
#define strdup      _strdup
#define stricmp     _stricmp
#define strnicmp    _strnicmp
#else
#define _getch      getchar
#define stricmp     strcasecmp
#define strnicmp    strncasecmp
#endif
#ifdef _WIN32
// Note: Both, MS VC++ and MinGW use _wcsicmp.
#define wcsicmp     _wcsicmp
#else
#define wcsicmp     wcscasecmp
#endif


// Console Quotation Mark (Windows: ", Linux: ')
#ifdef _WIN32
#define QMARK_CHR   '\"'
#else
#define QMARK_CHR   '\''
#endif


#define ZLIB_SEEKBUG_CHECK(vgmhdr) \
    if (vgmhdr.fccVGM != FCC_VGM) \
    { \
        printf("VGM signature matched on the first read, but not on the second one!\n"); \
        printf("This is a known zlib bug where gzseek fails. Please install a fixed zlib.\n"); \
        goto OpenErr; \
    }



static void RemoveNewLines(char *String)
{
    char *strPtr;

    strPtr = String + strlen(String) - 1;
    while(strPtr >= String && (UINT8)*strPtr < 0x20)    // <0x20 -> is Control Character
    {
        *strPtr = '\0';
        strPtr --;
    }

    return;
}

static void RemoveQuotationMarks(char *String)
{
    size_t strLen;
    char *endQMark;

    if(String[0] != QMARK_CHR)
        return;

    strLen = strlen(String);
    memmove(String, String + 1, strLen);    // Remove first char
    endQMark = strrchr(String, QMARK_CHR);
    if(endQMark != NULL)
        *endQMark = '\0';   // Remove last Quot.-Mark

    return;
}

//static void ReadFilename(char *buffer, size_t bufsize)
//{
//    char *retStr;

//    retStr = fgets(buffer, bufsize, stdin);
//    if(retStr == NULL)
//        buffer[0] = '\0';

//#ifdef _WIN32
//    if(GetConsoleCP() == GetOEMCP())
//        OemToChar(buffer, buffer);  // OEM -> ANSI conversion
//#endif

//    RemoveNewLines(buffer);
//    RemoveQuotationMarks(buffer);

//    return;
//}

//INLINE void DblClickWait(const char *argv0)
//{
//#ifdef _WIN32
//    char *msystem;

//    if(argv0[1] != ':')
//        return; // not called with an absolute path

//    msystem = getenv("MSYSTEM");
//    if(msystem != NULL)
//    {
//        // return, if we're within a MinGW/MSYS environment
//        // (MSYS always calls using absolute paths)
//        if(! strncmp(msystem, "MINGW", 5) ||
//           ! strncmp(msystem, "MSYS", 4))
//            return;
//    }

//    // Executed by Double-Clicking (or Drag and Drop),
//    // so let's keep the window open until the user presses a key.
//    if(_kbhit())
//        _getch();
//    _getch();

//    return;
//#else
//    // Double-clicking a console application on Unix doesn't open
//    // a console window, so don't do anything.
//    return;
//#endif
//}

// Note: Really used by vgm2txt and vgmlpfnd only
//       most other tools use it for progress display under Windows
static void PrintMinSec(const UINT32 SamplePos, char *TempStr)
{
    float TimeSec;
    UINT16 TimeMin;

    TimeSec = (float)SamplePos / 44100.0f;
    TimeMin = (UINT16)(TimeSec / 60);
    TimeSec -= TimeMin * 60;
    sprintf(TempStr, "%02u:%05.2f", TimeMin, TimeSec);

    return;
}

#endif  // __COMMON_H__
