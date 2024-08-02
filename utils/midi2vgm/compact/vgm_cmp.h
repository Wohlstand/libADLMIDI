#ifndef VGM_CMP_H
#define VGM_CMP_H

#include <stdint.h>
#include <string>

namespace VgmCMP
{

extern uint32_t NxtCmdPos;
extern uint8_t NxtCmdCommand;
extern uint16_t NxtCmdReg;
extern uint8_t NxtCmdVal;

extern bool JustTimerCmds;
extern bool DoOKI6258;

int vgm_cmp_main(const std::string &in_file, bool makeVgz, bool justtmr = false, bool do6258 = false);

}

#endif // VGM_CMP_H
