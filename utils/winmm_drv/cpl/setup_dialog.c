#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

#include "setup_dialog.h"
#include "resource.h"

#ifndef CBM_FIRST
#define CBM_FIRST 0x1700
#endif
#ifndef CB_SETMINVISIBLE
#define CB_SETMINVISIBLE (CBM_FIRST+1)
#endif

typedef int (*BankNamesCount)(void);
typedef const char *const *(*BankNamesList)(void);

static const char *const volume_models_descriptions[] =
{
    "Auto (defined by bank)",
    "Generic",
    "OPL3 Native",
    "DMX",
    "Apogee Sound System",
    "Win9x SB16 driver",
    "DMX (Fixed AM)",
    "Apogee Sound System (Fixed AM)",
    "Audio Interfaces Library (AIL)",
    "Win9x Generic FM driver",
    "HMI Sound Operating System",
    "HMI Sound Operating System (Old)",
    NULL
};

static const char * const emulator_type_descriptions[] =
{
    "Nuked OPL3 1.8",
    "Nuked OPL3 1.7.4 (Optimized)",
    "DOSBox",
    "Opal",
    "Java OPL3",
    NULL
};

typedef struct DriverSettings_t
{
    BOOL    useExternalBank;
    int     bankId;
    char    bankPath[MAX_PATH * 4];
    int     emulatorId;

    BOOL    flagDeepTremolo;
    BOOL    flagDeepVibrato;

    BOOL    flagSoftPanning;
    BOOL    flagScaleModulators;
    BOOL    flagFullBrightness;

    int     volumeModel;
    int     numChips;
    int     num4ops;
} DriverSettings;

static DriverSettings g_setup;
static HINSTANCE      s_hModule;


static void syncBankType(HWND hwnd, int type);
static void setupDefault();

static void sync4ops(HWND hwnd);
static void syncWidget(HWND hwnd);
static void buildLists(HWND hwnd);
static void syncBankType(HWND hwnd, int type);
static void openCustomBank(HWND hwnd);
static void updateBankName(HWND hwnd, const char *filePath);


static void setupDefault()
{
    g_setup.useExternalBank = 0;
    g_setup.bankId = 68;
    ZeroMemory(g_setup.bankPath, sizeof(g_setup.bankPath));
    g_setup.emulatorId = 0;

    g_setup.flagDeepTremolo = BST_INDETERMINATE;
    g_setup.flagDeepVibrato = BST_INDETERMINATE;

    g_setup.flagSoftPanning = BST_CHECKED;
    g_setup.flagScaleModulators = BST_UNCHECKED;
    g_setup.flagFullBrightness = BST_UNCHECKED;

    g_setup.volumeModel = 0;
    g_setup.numChips = 4;
    g_setup.num4ops = -1;
}

static void sync4ops(HWND hwnd)
{
    char buff[10];
    int i;

    SendDlgItemMessageW(hwnd, IDC_NUM_4OPVO, CB_RESETCONTENT, 0, 0);
    SendDlgItemMessageA(hwnd, IDC_NUM_4OPVO, CB_ADDSTRING, (LPARAM)-1, (LPARAM)"AUTO");
    for(i = 0; i <= g_setup.numChips * 6; i++)
    {
        ZeroMemory(buff, 10);
        snprintf(buff, 10, "%d", i);
        SendDlgItemMessageA(hwnd, IDC_NUM_4OPVO, CB_ADDSTRING, (LPARAM)0, (LPARAM)buff);
    }
    SendDlgItemMessageA(hwnd, IDC_NUM_4OPVO, CB_SETCURSEL, (WPARAM)g_setup.num4ops + 1, (LPARAM)0);
}

static void syncWidget(HWND hwnd)
{
    char buff[10];
    int i;

    SendDlgItemMessage(hwnd, IDC_BANK_EXTERNAL, BM_SETCHECK, 0, 0);
    SendDlgItemMessage(hwnd, IDC_BANK_INTERNAL, BM_SETCHECK, 0, 0);

    if(g_setup.useExternalBank == 1)
        SendDlgItemMessage(hwnd, IDC_BANK_EXTERNAL, BM_SETCHECK, 1, 0);
    else
        SendDlgItemMessage(hwnd, IDC_BANK_INTERNAL, BM_SETCHECK, 1, 0);

    syncBankType(hwnd, g_setup.useExternalBank);

    SendDlgItemMessage(hwnd, IDC_FLAG_TREMOLO, BM_SETCHECK, g_setup.flagDeepTremolo, 0);
    SendDlgItemMessage(hwnd, IDC_FLAG_VIBRATO, BM_SETCHECK, g_setup.flagDeepVibrato, 0);
    SendDlgItemMessage(hwnd, IDC_FLAG_SOFTPAN, BM_SETCHECK, g_setup.flagSoftPanning, 0);
    SendDlgItemMessage(hwnd, IDC_FLAG_SCALE, BM_SETCHECK, g_setup.flagScaleModulators, 0);
    SendDlgItemMessage(hwnd, IDC_FLAG_FULLBRIGHT, BM_SETCHECK, g_setup.flagFullBrightness, 0);

    SendDlgItemMessageW(hwnd, IDC_NUM_CHIPS, CB_RESETCONTENT, 0, 0);
    for(i = 1; i <= 100; i++)
    {
        ZeroMemory(buff, 10);
        snprintf(buff, 10, "%d", i);
        SendDlgItemMessageA(hwnd, IDC_NUM_CHIPS, CB_ADDSTRING, (LPARAM)0, (LPARAM)buff);
    }

    SendDlgItemMessageA(hwnd, IDC_NUM_CHIPS, CB_SETCURSEL, (WPARAM)g_setup.numChips - 1, (LPARAM)0);
    SendDlgItemMessageA(hwnd, IDC_BANK_ID, CB_SETCURSEL, (WPARAM)g_setup.bankId, (LPARAM)0);
    updateBankName(hwnd, g_setup.bankPath);
    SendDlgItemMessageA(hwnd, IDC_EMULATOR, CB_SETCURSEL, (WPARAM)g_setup.emulatorId, (LPARAM)0);
    SendDlgItemMessageA(hwnd, IDC_VOLUMEMODEL, CB_SETCURSEL, (WPARAM)g_setup.volumeModel, (LPARAM)0);

    sync4ops(hwnd);
}

static void buildLists(HWND hwnd)
{
    int i, bMax;
    HMODULE lib;
    const char *const* list;
    BankNamesCount adl_getBanksCount;
    BankNamesList adl_getBankNames;

    lib = LoadLibraryW(L"adlmididrv.dll");
    if(lib)
    {
        adl_getBanksCount = (BankNamesCount)GetProcAddress(lib, "adl_getBanksCount");
        adl_getBankNames = (BankNamesList)GetProcAddress(lib, "adl_getBankNames");
        if(adl_getBanksCount && adl_getBankNames)
        {
            bMax = adl_getBanksCount();
            list = adl_getBankNames();
            for(i = 0; i < bMax; i++)
            {
                SendDlgItemMessageA(hwnd, IDC_BANK_ID, CB_ADDSTRING, (LPARAM)0, (LPARAM)list[i]);
            }
        }
        else
        {
            SendDlgItemMessageA(hwnd, IDC_BANK_ID, CB_ADDSTRING, (LPARAM)0, (LPARAM)"<Can't get calls>");
        }
        FreeLibrary(lib);
    }
    else
    {
        SendDlgItemMessageA(hwnd, IDC_BANK_ID, CB_ADDSTRING, (LPARAM)0, (LPARAM)"<Can't load library>");
    }

    // Volume models
    for(i = 0; volume_models_descriptions[i] != NULL; ++i)
    {
        SendDlgItemMessageA(hwnd, IDC_VOLUMEMODEL, CB_ADDSTRING, (LPARAM)0, (LPARAM)volume_models_descriptions[i]);
    }

    // Emulators list
    for(i = 0; emulator_type_descriptions[i] != NULL; ++i)
    {
        SendDlgItemMessageA(hwnd, IDC_EMULATOR, CB_ADDSTRING, (LPARAM)0, (LPARAM)emulator_type_descriptions[i]);
    }
}

static void syncBankType(HWND hwnd, int type)
{
    EnableWindow(GetDlgItem(hwnd, IDC_BANK_ID), !type);
    EnableWindow(GetDlgItem(hwnd, IDC_BROWSE_BANK), type);
}

static void updateBankName(HWND hwnd, const char *filePath)
{
    int i, len = strlen(filePath);
    const char *p = NULL;
    wchar_t label[40];

    for(i = 0; i < len; i++)
    {
        if(filePath[i] == '\\' || filePath[i] == '/')
            p = filePath + i + 1;
    }

    if(p == NULL)
        SendDlgItemMessage(hwnd, IDC_BANK_PATH, WM_SETTEXT, (WPARAM)NULL, (LPARAM)L"<none>");
    else
    {
        ZeroMemory(label, 40);
        MultiByteToWideChar(CP_UTF8, 0, p, strlen(p), label, 40);
        SendDlgItemMessage(hwnd, IDC_BANK_PATH, WM_SETTEXT, (WPARAM)NULL, (LPARAM)label);
    }
}

static void openCustomBank(HWND hwnd)
{
    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH];
    int ret;

    ZeroMemory(&ofn, sizeof(ofn));
    ZeroMemory(szFile, sizeof(szFile));

    MultiByteToWideChar(CP_UTF8, 0, g_setup.bankPath, strlen(g_setup.bankPath), szFile, sizeof(szFile));

    ofn.lStructSize = sizeof(ofn);
    ofn.hInstance = s_hModule;
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"WOPL bank file (*.wopl)\0*.WOPL\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrTitle = L"Open external bank file";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_READONLY | OFN_HIDEREADONLY | OFN_EXPLORER;

    if(GetOpenFileNameW(&ofn) == TRUE)
    {
        ret = WideCharToMultiByte(CP_UTF8, 0,
                                  ofn.lpstrFile, wcslen(ofn.lpstrFile),
                                  g_setup.bankPath, MAX_PATH * 4, 0, 0);
        if(ret >= 0)
        {
            g_setup.bankPath[ret] = '\0';
            updateBankName(hwnd, g_setup.bankPath);
        }
    }
}

BOOL CALLBACK ToolDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch(Message)
    {
    case WM_INITDIALOG:
        buildLists(hwnd);
        syncWidget(hwnd);
        return TRUE;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDC_ABOUT:
            MessageBoxW(hwnd,
                        L"libADLMIDI - a library, made by Vitaly Novichkov \"Wohlstand\".", L"About this driver",
                        MB_OK | MB_ICONINFORMATION);
        break;

        case IDC_NUM_CHIPS:
            if(HIWORD(wParam) == CBN_SELCHANGE)
            {
                g_setup.numChips = 1 + SendMessageW((HWND)lParam, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
                g_setup.num4ops = -1;
                sync4ops(hwnd);
            }
            break;

        case IDC_NUM_4OPVO:
            if(HIWORD(wParam) == CBN_SELCHANGE)
            {
                g_setup.num4ops = SendMessageW((HWND)lParam, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0) - 1;
            }
            break;

        case IDC_EMULATOR:
            if(HIWORD(wParam) == CBN_SELCHANGE)
            {
                g_setup.emulatorId = SendMessageW((HWND)lParam, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
            }
            break;

        case IDC_BANK_INTERNAL:
            g_setup.useExternalBank = 0;
            syncBankType(hwnd, FALSE);
            break;

        case IDC_BANK_EXTERNAL:
            g_setup.useExternalBank = 1;
            syncBankType(hwnd, TRUE);
            break;

        case IDC_BANK_ID:
            if(HIWORD(wParam) == CBN_SELCHANGE)
            {
                g_setup.bankId = SendMessageW((HWND)lParam, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
            }
            break;

        case IDC_BROWSE_BANK:
            openCustomBank(hwnd);
            break;

        case IDC_FLAG_TREMOLO:
            if(HIWORD(wParam) == BN_CLICKED)
            {
                g_setup.flagDeepTremolo = SendDlgItemMessage(hwnd, IDC_FLAG_TREMOLO, (UINT)BM_GETCHECK, 0, 0);
            }
            break;

        case IDC_FLAG_VIBRATO:
            if(HIWORD(wParam) == BN_CLICKED)
            {
                g_setup.flagDeepVibrato = SendDlgItemMessage(hwnd, IDC_FLAG_VIBRATO, (UINT)BM_GETCHECK, 0, 0);
            }
            break;

        case IDC_FLAG_SOFTPAN:
            if(HIWORD(wParam) == BN_CLICKED)
            {
                g_setup.flagSoftPanning = SendDlgItemMessage(hwnd, IDC_FLAG_SOFTPAN, (UINT)BM_GETCHECK, 0, 0);
            }
            break;

        case IDC_FLAG_SCALE:
            if(HIWORD(wParam) == BN_CLICKED)
            {
                g_setup.flagScaleModulators = SendDlgItemMessage(hwnd, IDC_FLAG_SCALE, (UINT)BM_GETCHECK, 0, 0);
            }
            break;

        case IDC_FLAG_FULLBRIGHT:
            if(HIWORD(wParam) == BN_CLICKED)
            {
                g_setup.flagFullBrightness = SendDlgItemMessage(hwnd, IDC_FLAG_FULLBRIGHT, (UINT)BM_GETCHECK, 0, 0);
            }
            break;


        case IDC_RESTORE_DEFAULTS:
            setupDefault();
            syncWidget(hwnd);
            break;

        case IDOK:
            EndDialog(hwnd, IDOK);
            break;

        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            break;
        }
    break;

    default:
        return FALSE;
    }

    return TRUE;

    UNREFERENCED_PARAMETER(lParam);
}


BOOL runAdlSetupBox(HINSTANCE hModule, HWND hwnd)
{
    int ret;

    s_hModule = hModule;

    setupDefault();

    ret = DialogBoxW(hModule, MAKEINTRESOURCEW(IDD_SETUP_BOX), hwnd, ToolDlgProc);

    if(ret == IDOK)
    {
        MessageBoxA(hwnd, "Dialog exited with IDOK.", "Notice", MB_OK | MB_ICONINFORMATION);
    }
    else if(ret == IDCANCEL)
    {
        MessageBoxA(hwnd, "Dialog exited with IDCANCEL.", "Notice", MB_OK | MB_ICONINFORMATION);
    }
    else if(ret == -1)
    {
        MessageBoxA(hwnd, "Dialog failed!", "Error", MB_OK | MB_ICONINFORMATION);
    }

    s_hModule = NULL;

    return TRUE;
}

WINBOOL initAdlSetupBox(HINSTANCE hModule, HWND hwnd)
{
    InitCommonControls();
    setupDefault();
    UNREFERENCED_PARAMETER(hModule);
    UNREFERENCED_PARAMETER(hwnd);
    return TRUE;
}
