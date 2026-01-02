set(CHIPS_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/dosbox_opl3.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/dosbox_opl3.h"
    "${CMAKE_CURRENT_LIST_DIR}/esfmu_opl3.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/esfmu_opl3.h"
    "${CMAKE_CURRENT_LIST_DIR}/java_opl3.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/java_opl3.h"
    "${CMAKE_CURRENT_LIST_DIR}/mame_opl2.h"
    "${CMAKE_CURRENT_LIST_DIR}/mame_opl2.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/nuked_opl2.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/nuked_opl2.h"
    "${CMAKE_CURRENT_LIST_DIR}/nuked_opl3.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/nuked_opl3.h"
    "${CMAKE_CURRENT_LIST_DIR}/opal_opl3.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/opal_opl3.h"
    "${CMAKE_CURRENT_LIST_DIR}/opal/opal.c"
    "${CMAKE_CURRENT_LIST_DIR}/opal/opal.h"
    "${CMAKE_CURRENT_LIST_DIR}/esfmu/esfm.c"
    "${CMAKE_CURRENT_LIST_DIR}/esfmu/esfm.h"
    "${CMAKE_CURRENT_LIST_DIR}/esfmu/esfm_registers.c"
    "${CMAKE_CURRENT_LIST_DIR}/nuked/nukedopl2.c"
    "${CMAKE_CURRENT_LIST_DIR}/nuked/nukedopl2.h"
    "${CMAKE_CURRENT_LIST_DIR}/nuked/nukedopl3.c"
    "${CMAKE_CURRENT_LIST_DIR}/nuked/nukedopl3.h"
    "${CMAKE_CURRENT_LIST_DIR}/mame/opl.h"
    "${CMAKE_CURRENT_LIST_DIR}/mame/mame_fmopl.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/dosbox/dbopl.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/dosbox/dbopl.h"
    "${CMAKE_CURRENT_LIST_DIR}/nuked_opl3_v174.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/nuked_opl3_v174.h"
    "${CMAKE_CURRENT_LIST_DIR}/nuked/nukedopl3_174.c"
    "${CMAKE_CURRENT_LIST_DIR}/nuked/nukedopl3_174.h"
    "${CMAKE_CURRENT_LIST_DIR}/ymf262_lle.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ymf262_lle.h"
    "${CMAKE_CURRENT_LIST_DIR}/ymf262_lle/nuked_fmopl3.c"
    "${CMAKE_CURRENT_LIST_DIR}/ymf262_lle/nuked_fmopl3.h"
    "${CMAKE_CURRENT_LIST_DIR}/ymf262_lle/nopl3.c"
    "${CMAKE_CURRENT_LIST_DIR}/ymf262_lle/nopl3.h"
    "${CMAKE_CURRENT_LIST_DIR}/ym3812_lle.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ym3812_lle.h"
    "${CMAKE_CURRENT_LIST_DIR}/ym3812_lle/nuked_fmopl2.c"
    "${CMAKE_CURRENT_LIST_DIR}/ym3812_lle/nuked_fmopl2.h"
    "${CMAKE_CURRENT_LIST_DIR}/ym3812_lle/nopl2.c"
    "${CMAKE_CURRENT_LIST_DIR}/ym3812_lle/nopl2.h"
)

if(COMPILER_SUPPORTS_CXX14) # YMFM can be built in only condition when C++14 and newer were available
  set(YMFM_SOURCES
      "${CMAKE_CURRENT_LIST_DIR}/ymfm_opl2.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/ymfm_opl2.h"
      "${CMAKE_CURRENT_LIST_DIR}/ymfm_opl3.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/ymfm_opl3.h"
      "${CMAKE_CURRENT_LIST_DIR}/ymfm/ymfm.h"
      "${CMAKE_CURRENT_LIST_DIR}/ymfm/ymfm_opl.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/ymfm/ymfm_opl.h"
      "${CMAKE_CURRENT_LIST_DIR}/ymfm/ymfm_misc.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/ymfm/ymfm_misc.h"
      "${CMAKE_CURRENT_LIST_DIR}/ymfm/ymfm_pcm.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/ymfm/ymfm_pcm.h"
      "${CMAKE_CURRENT_LIST_DIR}/ymfm/ymfm_adpcm.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/ymfm/ymfm_adpcm.h"
      "${CMAKE_CURRENT_LIST_DIR}/ymfm/ymfm_ssg.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/ymfm/ymfm_ssg.h"
  )
  if(DEFINED FLAG_CPP14)
    set_source_files_properties(${YMFM_SOURCES} COMPILE_FLAGS ${FLAG_CPP14})
  endif()
  list(APPEND CHIPS_SOURCES ${YMFM_SOURCES})
endif()

if(ENABLE_OPL3_PROXY)
  list(APPEND CHIPS_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/win9x_opl_proxy.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/win9x_opl_proxy.h"
  )
endif()

if(ENABLE_SERIAL_PORT)
  list(APPEND CHIPS_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/opl_serial_port.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/opl_serial_port.h"
  )
endif()
