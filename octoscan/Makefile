all: octoscan

install: all
	install -m 0755 octoscan $(DESTDIR)/usr/bin

octoscan: octoscan.c
	$(CC) -o octoscan octoscan.c
