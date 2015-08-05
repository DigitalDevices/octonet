OCTOSERVE_VERSION = 0.1
OCTOSERVE_SOURCE = 
OCTOSERVE_SITE = ../octoserve
OCTOSERVE_SITE_METHOD = local
OCTOSERVE_INSTALL_TARGET = YES

define OCTOSERVE_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D)
endef

define OCTOSERVE_INSTALL_TARGET_CMDS
        $(INSTALL) -m 0755 -D $(@D)/octoserve $(TARGET_DIR)/usr/bin/octoserve
	cp -rd $(@D)/var $(TARGET_DIR)/
        $(INSTALL) -m 0755 -d $(TARGET_DIR)/boot
        $(INSTALL) -m 0755 $(@D)/boot/* $(TARGET_DIR)/boot/
endef

$(eval $(generic-package))
