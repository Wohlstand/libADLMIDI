#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <string.h>
#include <stdio.h>

#include "setup_dialog.h"
#include "resource.h"

#ifndef CBM_FIRST
#define CBM_FIRST 0x1700
#endif
#ifndef CB_SETMINVISIBLE
#define CB_SETMINVISIBLE (CBM_FIRST+1)
#endif

typedef struct DriverSettings_t
{
    BOOL    useExternalBank;
    int     bankId;
    char    bankPath[MAX_PATH];
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

static void setupDefault()
{
    g_setup.useExternalBank = FALSE;
    g_setup.bankId = 64;
    memset(g_setup.bankPath, 0, sizeof(g_setup.bankPath));
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
        memset(buff, 0, 10);
        snprintf(buff, 10, "%d", i);
        SendDlgItemMessageA(hwnd, IDC_NUM_4OPVO, CB_ADDSTRING, (LPARAM)0, (LPARAM)buff);
    }
    SendDlgItemMessageA(hwnd, IDC_NUM_4OPVO, CB_SETCURSEL, (WPARAM)g_setup.num4ops + 1, (LPARAM)0);
    SendDlgItemMessageA(hwnd, IDC_NUM_4OPVO, CB_SETMINVISIBLE, (WPARAM)12, (LPARAM)0);
}

static void syncWidget(HWND hwnd)
{
    char buff[10];
    int i;

    SendDlgItemMessage(hwnd, IDC_FLAG_TREMOLO, BM_SETCHECK, g_setup.flagDeepTremolo, 0);
    SendDlgItemMessage(hwnd, IDC_FLAG_VIBRATO, BM_SETCHECK, g_setup.flagDeepVibrato, 0);
    SendDlgItemMessage(hwnd, IDC_FLAG_SOFTPAN, BM_SETCHECK, g_setup.flagSoftPanning, 0);
    SendDlgItemMessage(hwnd, IDC_FLAG_SCALE, BM_SETCHECK, g_setup.flagScaleModulators, 0);
    SendDlgItemMessage(hwnd, IDC_FLAG_FULLBRIGHT, BM_SETCHECK, g_setup.flagFullBrightness, 0);

    SendDlgItemMessageW(hwnd, IDC_NUM_CHIPS, CB_RESETCONTENT, 0, 0);
    for(i = 1; i <= 100; i++)
    {
        memset(buff, 0, 10);
        snprintf(buff, 10, "%d", i);
        SendDlgItemMessageA(hwnd, IDC_NUM_CHIPS, CB_ADDSTRING, (LPARAM)0, (LPARAM)buff);
    }
    SendDlgItemMessageA(hwnd, IDC_NUM_CHIPS, CB_SETCURSEL, (WPARAM)g_setup.numChips - 1, (LPARAM)0);
    SendDlgItemMessageA(hwnd, IDC_NUM_CHIPS, CB_SETMINVISIBLE, (WPARAM)12, (LPARAM)0);

    sync4ops(hwnd);
}

BOOL CALLBACK ToolDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch(Message)
    {
    case WM_INITDIALOG:
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
