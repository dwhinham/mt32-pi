################################################################################
#
# mt32d
#
################################################################################

MT32D_VERSION = 2.0.1
MT32D_SOURCE = libmt32emu_2_0_1.tar.gz 
MT32D_SITE = https://github.com/munt/munt/archive
MT32D_INSTALL_STAGING = YES
MT32D_DEPENDENCIES = libmt32emu

define MT32D_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) CXXFLAGS+="-DANALOG_MODE=2" -C $(@D)/mt32emu_alsadrv mt32d
endef

define MT32D_INSTALL_STAGING_CMDS
	$(INSTALL) -D -m 0755 $(@D)/mt32emu_alsadrv/mt32d $(TARGET_DIR)/usr/bin/mt32d
endef

define MT32D_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/mt32emu_alsadrv/mt32d $(TARGET_DIR)/usr/bin/mt32d
endef

$(eval $(generic-package))
