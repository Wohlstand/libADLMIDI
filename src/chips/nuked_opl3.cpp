#include "nuked_opl3.h"
#include "nuked/nukedopl3.h"
#include <cstring>

NukedOPL3::NukedOPL3() :
    OPLChipBase()
{
    m_chip = new opl3_chip;
    reset(m_rate);
}

NukedOPL3::NukedOPL3(const NukedOPL3 &c):
    OPLChipBase(c)
{
    m_chip = new opl3_chip;
    std::memset(m_chip, 0, sizeof(opl3_chip));
    reset(c.m_rate);
}

NukedOPL3::~NukedOPL3()
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    delete chip_r;
}

void NukedOPL3::setRate(uint32_t rate)
{
    OPLChipBase::setRate(rate);
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    std::memset(chip_r, 0, sizeof(opl3_chip));
    OPL3_Reset(chip_r, rate);
}

void NukedOPL3::reset()
{
    setRate(m_rate);
}

void NukedOPL3::reset(uint32_t rate)
{
    setRate(rate);
}

void NukedOPL3::writeReg(uint16_t addr, uint8_t data)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3_WriteRegBuffered(chip_r, addr, data);
}

int NukedOPL3::generate(int16_t *output, size_t frames)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3_GenerateStream(chip_r, output, (Bit32u)frames);
    return (int)frames;
}

int NukedOPL3::generateAndMix(int16_t *output, size_t frames)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3_GenerateStreamMix(chip_r, output, (Bit32u)frames);
    return (int)frames;
}

int NukedOPL3::generate32(int32_t *output, size_t frames)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    for(size_t i = 0; i < frames; ++i) {
        int16_t frame[2];
        OPL3_GenerateResampled(chip_r, frame);
        output[0] = (int32_t)frame[0];
        output[1] = (int32_t)frame[1];
        output += 2;
    }
    return (int)frames;
}

int NukedOPL3::generateAndMix32(int32_t *output, size_t frames)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    for(size_t i = 0; i < frames; ++i) {
        int16_t frame[2];
        OPL3_GenerateResampled(chip_r, frame);
        output[0] += (int32_t)frame[0];
        output[1] += (int32_t)frame[1];
        output += 2;
    }
    return (int)frames;
}

const char *const NukedOPL3::staticEmulatorName = "Nuked OPL3 (v 1.8)";

const char *NukedOPL3::emulatorName()
{
    return staticEmulatorName;
}
