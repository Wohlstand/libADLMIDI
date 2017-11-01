TEMPLATE=app
CONFIG-=qt
CONFIG+=console

CONFIG -= c++11

TARGET=adlmidiplay
DESTDIR=$$PWD/bin/

#INCLUDEPATH += $$PWD/AudioCodecs/build/install/include
#LIBS += -L$$PWD/AudioCodecs/build/install/lib
INCLUDEPATH += $$PWD/src $$PWD/include
#LIBS += -Wl,-Bstatic -lSDL2 -Wl,-Bdynamic -lpthread -ldl
LIBS += -lSDL2 -lpthread -ldl

#DEFINES += DEBUG_TIME_CALCULATION
#DEFINES += DEBUG_SEEKING_TEST
#DEFINES += DISABLE_EMBEDDED_BANKS
#DEFINES += ADLMIDI_USE_DOSBOX_OPL
#DEFINES += ENABLE_BEGIN_SILENCE_SKIPPING
#DEFINES += DEBUG_TRACE_ALL_EVENTS

QMAKE_CFLAGS    += -std=c90 -pedantic
QMAKE_CXXFLAGS  += -std=c++98 -pedantic

HEADERS += \
    include/adlmidi.h \
    src/adlbank.h \
    src/adldata.hh \
    src/adlmidi_mus2mid.h \
    src/adlmidi_private.hpp \
    src/adlmidi_xmi2mid.h \
    src/fraction.h \
    src/nukedopl3.h \
    src/dbopl.h \
    src/midiplay/wave_writer.h

SOURCES += \
    src/adldata.cpp \
    \
    src/adlmidi.cpp \
    src/adlmidi_load.cpp \
    src/adlmidi_midiplay.cpp \
    src/adlmidi_mus2mid.c \
    src/adlmidi_opl3.cpp \
    src/adlmidi_private.cpp \
    src/adlmidi_xmi2mid.c \
    src/nukedopl3.c \
    src/dbopl.cpp \
    utils/midiplay/adlmidiplay.cpp \
    utils/midiplay/wave_writer.c
