#!/bin/bash
export WATCOM=/home/wohlstand/Qt/Tools/ow-snapshot-2.0/
export EDPATH=$WATCOM/eddat
export WIPFC=$WATCOM/wipfc
export INCLUDE="$djgpp_prefix/lh"
WATCOM_FLAGS="-blinux"
export CFLAGS="$WATCOM_FLAGS -xc -std=wc"
export CXXFLAGS="$WATCOM_FLAGS -xc++ -xs -feh -std=c++11"

# export PKG_CONFIG_LIBDIR="${WATCOM}/lib/pkgconfig"
# djgpp_c_flags="-O2 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions --param=ssp-buffer-size=4"


CUSTOM_PATH=/home/wohlstand/_git_repos/libADLMIDI/cmake/openwattcom:${WATCOM}/binl:$PATH

if [[ "$1" != '--build' ]]; then
    echo "KEK [${CUSTOM_PATH}]"

    PATH=${CUSTOM_PATH} cmake \
        -DCMAKE_INSTALL_PREFIX:PATH=${WATCOM} \
        -DCMAKE_INSTALL_LIBDIR:PATH=${WATCOM}/lib386 \
        -DINCLUDE_INSTALL_DIR:PATH=${WATCOM}/lib386 \
        -DLIB_INSTALL_DIR:PATH=${WATCOM}/lib \
        -DSYSCONF_INSTALL_DIR:PATH=${WATCOM}/etc \
        -DSHARE_INSTALL_DIR:PATH=${WATCOM}/share \
        -DBUILD_SHARED_LIBS:BOOL=OFF \
        -DCMAKE_TOOLCHAIN_FILE=/home/wohlstand/_git_repos/libADLMIDI/cmake/openwattcom/toolchain-ow.cmake \
        "$@"

else
    PATH=${CUSTOM_PATH} cmake "$@"
fi

#-DCMAKE_CROSSCOMPILING_EMULATOR=/usr/bin/i686-pc-msdosdjgpp-wine
