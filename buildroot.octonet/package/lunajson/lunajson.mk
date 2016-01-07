################################################################################
#
# lunajson
#
################################################################################

LUNAJSON_VERSION = 1.1-0
LUNAJSON_LICENSE = MIT/X11

LUNAJSON_SUBDIR = lunajson
LUNAJSON_LICENSE_FILE = $(LUNAJSON_SUBDIR)/LICENSE


$(eval $(luarocks-package))
