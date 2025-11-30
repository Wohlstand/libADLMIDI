SOURCES+= \
    $$PWD/dosbox_opl3.cpp \
    $$PWD/esfmu_opl3.cpp \
    $$PWD/java_opl3.cpp \
    $$PWD/mame_opl2.cpp \
    $$PWD/mame/mame_fmopl.cpp \
    $$PWD/nuked_opl3.cpp \
    $$PWD/opal_opl3.cpp \
    $$PWD/esfmu/esfm.c \
    $$PWD/esfmu/esfm_registers.c \
    $$PWD/opal/opal.c \
    $$PWD/nuked/nukedopl3.c \
    $$PWD/dosbox/dbopl.cpp \
    $$PWD/nuked_opl3_v174.cpp \
    $$PWD/nuked/nukedopl3_174.c \
    $$PWD/ymf262_lle.cpp \
    $$PWD/ymf262_lle/nuked_fmopl3.c \
    $$PWD/ymf262_lle/nopl3.c \
    $$PWD/ym3812_lle.cpp \
    $$PWD/ym3812_lle/nuked_fmopl2.c \
    $$PWD/ym3812_lle/nopl2.c

HEADERS+= \
    $$PWD/opl_chip_base.h \
    $$PWD/opl_chip_base.tcc \
    $$PWD/dosbox_opl3.h \
    $$PWD/esfmu_opl3.h \
    $$PWD/java_opl3.h \
    $$PWD/mame_opl2.h \
    $$PWD/mame/opl.h \
    $$PWD/nuked_opl3.h \
    $$PWD/opal_opl3.h \
    $$PWD/opal/opal.h \
    $$PWD/opl_chip_base.h \
    $$PWD/esfmu/esfm.h \
    $$PWD/java/JavaOPL3.hpp \
    $$PWD/nuked/nukedopl3.h \
    $$PWD/dosbox/dbopl.h \
    $$PWD/nuked_opl3_v174.h \
    $$PWD/nuked/nukedopl3_174.h \
    $$PWD/ymf262_lle.h \
    $$PWD/ymf262_lle/nuked_fmopl3.h \
    $$PWD/ymf262_lle/nopl3.h \
    $$PWD/ym3812_lle.h \
    $$PWD/ym3812_lle/nuked_fmopl2.h \
    $$PWD/ym3812_lle/nopl2.h

# Available when C++14 is supported
enable_ymfm: {
DEFINES += ENABLE_YMFM_EMULATOR

SOURCES+= \
    $$PWD/ymfm_opl2.cpp \
    $$PWD/ymfm_opl3.cpp \
    $$PWD/ymfm/ymfm_adpcm.cpp \
    $$PWD/ymfm/ymfm_misc.cpp \
    $$PWD/ymfm/ymfm_opl.cpp \
    $$PWD/ymfm/ymfm_pcm.cpp \
    $$PWD/ymfm/ymfm_ssg.cpp

HEADERS+= \
    $$PWD/ymfm_opl2.h \
    $$PWD/ymfm_opl3.h \
    $$PWD/ymfm/ymfm.h \
    $$PWD/ymfm/ymfm_adpcm.h \
    $$PWD/ymfm/ymfm_fm.h \
    $$PWD/ymfm/ymfm_fm.ipp \
    $$PWD/ymfm/ymfm_misc.h \
    $$PWD/ymfm/ymfm_opl.h \
    $$PWD/ymfm/ymfm_pcm.h \
    $$PWD/ymfm/ymfm_ssg.h
}
