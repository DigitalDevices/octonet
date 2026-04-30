DDDVB_VERSION = 0.9.38
DDDVB_SITE = $(call github,DigitalDevices,dddvb,$(DDDVB_VERSION))
DDDVB_SOURCE = dddvb-$(DDDVB_VERSION).tar.gz
DDDVB_DEPENDENCIES = linux
DDDVB_INSTALL_TARGET = YES

define DDDVB_BUILD_CMDS
	# 1. Module
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) \
		$(TARGET_CONFIGURE_OPTS) \
		KDIR=$(LINUX_DIR) \
		ARCH=$(KERNEL_ARCH) \
		KCFLAGS="-fms-extensions -Dddoctonet -I$(@D)/ddbridge -I$(@D)/dvb-core -I$(LINUX_DIR)/drivers/media/dvb-core" \
		all
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/apps \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS) -fms-extensions -Dddoctonet -I$(@D)/include -I$(@D)/ddbridge"
endef

define DDDVB_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/extra/
	
	cd $(@D) && find . -name "*.ko" | xargs -I{} cp --parents {} $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/extra/
	
	$(INSTALL) -m 0755 -D $(@D)/apps/ddtest $(TARGET_DIR)/usr/bin/ddtest
	$(INSTALL) -m 0755 -D $(@D)/apps/cit $(TARGET_DIR)/usr/bin/cit
	$(INSTALL) -m 0755 -D $(@D)/apps/citin $(TARGET_DIR)/usr/bin/citin
	$(INSTALL) -m 0755 -D $(@D)/apps/ddflash $(TARGET_DIR)/usr/bin/ddflash
	$(INSTALL) -m 0755 -D $(@D)/apps/flashprog $(TARGET_DIR)/usr/bin/flashprog
	$(INSTALL) -m 0755 -D $(@D)/apps/setmod $(TARGET_DIR)/usr/bin/setmod
	$(INSTALL) -m 0755 -D $(@D)/apps/modt $(TARGET_DIR)/usr/bin/modt
	$(INSTALL) -m 0755 -D $(@D)/apps/pls $(TARGET_DIR)/usr/bin/pls
	$(INSTALL) -m 0755 -D $(@D)/apps/setmod2 $(TARGET_DIR)/usr/bin/setmod2
	$(INSTALL) -m 0755 -D $(@D)/apps/setmod3 $(TARGET_DIR)/usr/bin/setmod3
endef


$(eval $(generic-package))

