OCTOSCAN_VERSION = 0.1
OCTOSCAN_SOURCE =
OCTOSCAN_SITE = ../octoscan
OCTOSCAN_SITE_METHOD = local
OCTOSCAN_INSTALL_TARGET = YES

define OCTOSCAN_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D)
endef

define OCTOSCAN_INSTALL_TARGET_CMDS
        $(INSTALL) -m 0755 -D $(@D)/octoscan $(TARGET_DIR)/usr/bin/octoscan
endef

$(eval $(generic-package))
