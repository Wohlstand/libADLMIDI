#pragma once
#ifndef REG_SETUP_HHHH
#define REG_SETUP_HHHH

#include <windef.h>
#include <winreg.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct DriverSettings_t
{
    BOOL    useExternalBank;
    int     bankId;
    WCHAR   bankPath[MAX_PATH];
    int     emulatorId;

    BOOL    flagDeepTremolo;
    BOOL    flagDeepVibrato;

    BOOL    flagSoftPanning;
    BOOL    flagScaleModulators;
    BOOL    flagFullBrightness;

    int     volumeModel;
    int     chanAlloc;
    int     numChips;
    int     num4ops;

    UINT    outputDevice;

    UINT    gain100;
} DriverSettings;

extern const WCHAR g_adlSignalMemory[];

extern void setupDefault(DriverSettings *setup);
extern void loadSetup(DriverSettings *setup);
extern void saveSetup(DriverSettings *setup);

extern void saveGain(DriverSettings *setup);
extern void getGain(DriverSettings *setup);


#define DRV_SIGNAL_RELOAD_SETUP 1
#define DRV_SIGNAL_RESET_SYNTH  2
#define DRV_SIGNAL_UPDATE_GAIN  3

// Client
/**
 * @brief Ping the running driver to immediately reload the settings
 */
extern void sendSignal(int sig);

#ifdef ENABLE_REG_SERVER
// Server
extern void openSignalListener();
extern int  hasReloadSetupSignal();
extern void resetSignal();
extern void closeSignalListener();
#endif

#ifdef __cplusplus
}
#endif


#endif
