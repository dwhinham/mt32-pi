################################################################################
#
# libmt32emu
#
################################################################################

LIBMT32EMU_VERSION = 2.0.1
LIBMT32EMU_SOURCE = libmt32emu_2_0_1.tar.gz 
LIBMT32EMU_SITE = https://github.com/munt/munt/archive
LIBMT32EMU_INSTALL_STAGING = YES
LIBMT32EMU_CONF_OPTS = -DCMAKE_VERBOSE_MAKEFILE=TRUE -DCMAKE_BUILD_TYPE=Release -Dmunt_WITH_MT32EMU_QT=FALSE -Dmunt_WITH_MT32EMU_SMF2WAV=FALSE

$(eval $(cmake-package))
