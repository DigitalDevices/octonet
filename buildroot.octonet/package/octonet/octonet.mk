OCTONET_VERSION = 0.1
OCTONET_SOURCE =
OCTONET_SITE = ../../dddvb/apps/octonet
OCTONET_SITE_METHOD = local
OCTONET_INSTALL_TARGET = YES

define OCTONET_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D)
endef

define OCTONET_INSTALL_TARGET_CMDS
        $(INSTALL) -m 0755 -D $(@D)/ddflash $(TARGET_DIR)/usr/bin/ddflash
        $(INSTALL) -m 0755 -D $(@D)/octokey $(TARGET_DIR)/usr/bin/octokey
        $(INSTALL) -m 0755 -D $(@D)/octonet $(TARGET_DIR)/usr/bin/octonet
        $(INSTALL) -m 0755 -D $(@D)/ddtest $(TARGET_DIR)/usr/bin/ddtest
endef

$(eval $(generic-package))
