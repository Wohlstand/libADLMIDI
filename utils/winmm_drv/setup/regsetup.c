#include <stdlib.h>
#include <stdio.h>

#include <windef.h>
#include <winbase.h>
#include <winuser.h>

#include "regsetup.h"


#define TOTAL_BYTES_READ    1024
#define OFFSET_BYTES 1024

static BOOL createRegistryKey(HKEY hKeyParent, const PWCHAR subkey)
{
    DWORD dwDisposition; //It verify new key is created or open existing key
    HKEY  hKey;
    DWORD ret;

    ret = RegCreateKeyExW(hKeyParent, subkey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
    if(ret != ERROR_SUCCESS)
    {
        return FALSE;
    }

    RegCloseKey(hKey); //close the key
    return TRUE;
}

static BOOL writeStringToRegistry(HKEY hKeyParent, const PWCHAR subkey, const PWCHAR valueName, PWCHAR strData)
{
    DWORD ret;
    HKEY hKey;

    //Check if the registry exists
    ret = RegOpenKeyExW(hKeyParent, subkey, 0, KEY_WRITE, &hKey);
    if(ret == ERROR_SUCCESS)
    {
        ret = RegSetValueExW(hKey, valueName, 0, REG_SZ, (LPBYTE)(strData), wcslen(strData) * sizeof(wchar_t));
        if(ret != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return FALSE;
        }

        RegCloseKey(hKey);
        return TRUE;
    }
    return FALSE;
}

static BOOL readStringFromRegistry(HKEY hKeyParent, const PWCHAR subkey, const PWCHAR valueName, PWCHAR *readData)
{
    HKEY hKey;
    DWORD len = TOTAL_BYTES_READ;
    DWORD readDataLen = len;
    PWCHAR readBuffer = (PWCHAR )malloc(sizeof(PWCHAR)* len);

    if (readBuffer == NULL)
        return FALSE;

    //Check if the registry exists
    DWORD ret = RegOpenKeyExW(hKeyParent, subkey, 0, KEY_READ, &hKey);

    if(ret == ERROR_SUCCESS)
    {
        ret = RegQueryValueExW(hKey, valueName, NULL, NULL, (BYTE*)readBuffer, &readDataLen);

        while(ret == ERROR_MORE_DATA)
        {
            // Get a buffer that is big enough.
            len += OFFSET_BYTES;
            readBuffer = (PWCHAR)realloc(readBuffer, len);
            readDataLen = len;
            ret = RegQueryValueExW(hKey, valueName, NULL, NULL, (BYTE*)readBuffer, &readDataLen);
        }
        if (ret != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return FALSE;
        }

        *readData = readBuffer;
        RegCloseKey(hKey);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static BOOL readIntFromRegistry(HKEY hKeyParent, const PWCHAR subkey, const PWCHAR valueName, int *readData)
{
    WCHAR *buf;
    BOOL ret;
    ret = readStringFromRegistry(hKeyParent, subkey, valueName, &buf);
    if(ret && readData)
    {
        *readData = _wtoi(buf);
    }
    if(buf)
        free(buf);
    return ret;
}

static BOOL writeIntToRegistry(HKEY hKeyParent, const PWCHAR subkey, const PWCHAR valueName, int intData)
{
    WCHAR buf[20];
    BOOL ret;

    ZeroMemory(buf, 20);
    _snwprintf(buf, 20, L"%d", intData);

    ret = writeStringToRegistry(hKeyParent, subkey, valueName, buf);
    return ret;
}




void setupDefault(DriverSettings *setup)
{
    setup->useExternalBank = 0;
    setup->bankId = 68;
    ZeroMemory(setup->bankPath, sizeof(setup->bankPath));
    setup->emulatorId = 0;

    setup->flagDeepTremolo = BST_INDETERMINATE;
    setup->flagDeepVibrato = BST_INDETERMINATE;

    setup->flagSoftPanning = BST_CHECKED;
    setup->flagScaleModulators = BST_UNCHECKED;
    setup->flagFullBrightness = BST_UNCHECKED;

    setup->volumeModel = 0;
    setup->numChips = 4;
    setup->num4ops = -1;
}


#define BUF_SIZE 4
const WCHAR g_adlSignalMemory[] = L"Global\\libADLMIDIDriverListener";

static const PWCHAR s_regPath = L"SOFTWARE\\Wohlstand\\libADLMIDI";

void loadSetup(DriverSettings *setup)
{
    int iVal;
    WCHAR *sVal = NULL;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"useExternalBank", &iVal))
        setup->useExternalBank = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"bankId", &iVal))
        setup->bankId = iVal;

    if(readStringFromRegistry(HKEY_CURRENT_USER, s_regPath, L"bankPath", &sVal))
        wcsncpy(setup->bankPath, sVal, MAX_PATH);
    if(sVal)
        free(sVal);
    sVal = NULL;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"emulatorId", &iVal))
        setup->emulatorId = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"flagDeepTremolo", &iVal))
        setup->flagDeepTremolo = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"flagDeepVibrato", &iVal))
        setup->flagDeepVibrato = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"flagSoftPanning", &iVal))
        setup->flagSoftPanning = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"flagScaleModulators", &iVal))
        setup->flagScaleModulators = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"flagFullBrightness", &iVal))
        setup->flagFullBrightness = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"volumeModel", &iVal))
        setup->volumeModel = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"numChips", &iVal))
        setup->numChips = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"num4ops", &iVal))
        setup->num4ops = iVal;
}

void saveSetup(DriverSettings *setup)
{
    createRegistryKey(HKEY_CURRENT_USER, s_regPath);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"useExternalBank", setup->useExternalBank);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"bankId", setup->bankId);
    writeStringToRegistry(HKEY_CURRENT_USER, s_regPath, L"bankPath", setup->bankPath);

    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"emulatorId", setup->emulatorId);

    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"flagDeepTremolo", setup->flagDeepTremolo);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"flagDeepVibrato", setup->flagDeepVibrato);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"flagSoftPanning", setup->flagSoftPanning);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"flagScaleModulators", setup->flagScaleModulators);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"flagFullBrightness", setup->flagFullBrightness);

    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"volumeModel", setup->volumeModel);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"numChips", setup->numChips);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"num4ops", setup->num4ops);
}

void sendSignal()
{
    HANDLE hMapFile;
    LPSTR pBuf;

    hMapFile = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, g_adlSignalMemory);
    if(hMapFile == NULL)
        return; // Nothing to do, driver is not runnig

    pBuf = (LPSTR)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, BUF_SIZE);

    if(pBuf == NULL)
    {
        CloseHandle(hMapFile);
        return;
    }

    pBuf[0] = 1; // Send '1' to tell driver reload the settings immediately

    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
}


#ifdef ENABLE_REG_SERVER

static HANDLE s_hMapFile = NULL;
static LPSTR  s_pBuf = NULL;

void openSignalListener()
{
    s_hMapFile = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, BUF_SIZE, g_adlSignalMemory);
    if(s_hMapFile == NULL)
        return; // Nothing to do

    s_pBuf = (LPSTR)MapViewOfFile(s_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, BUF_SIZE);
    if(s_pBuf == NULL)
    {
        CloseHandle(s_hMapFile);
        return;
    }
    ZeroMemory(s_pBuf, BUF_SIZE);
}

BOOL hasSignal()
{
    if(!s_pBuf)
        return FALSE;
    return s_pBuf[0] == 1;
}

void resetSignal()
{
    if(s_pBuf)
        ZeroMemory(s_pBuf, BUF_SIZE);
}

void closeSignalListener()
{
    if(s_pBuf)
        UnmapViewOfFile(s_pBuf);
    if(s_hMapFile)
        CloseHandle(s_hMapFile);
    s_pBuf = NULL;
    s_hMapFile = NULL;
}

#endif
