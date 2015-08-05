CFLAGS ?= -Os
#CFLAGS += -Wall
CFLAGS += -D_GNU_SOURCE -DIGNORE_NS
#CFLAGS += -ansi
CC = gcc

all: octoserve 

octoserve: octoserve.h octoserve.o ssdp.o ntools.o dvb.o config.o http.o igmp.o switch.o
	$(CC) $(CFLAGS) -ldvbcfg -ldvben50221 -lucsi -ldvbsec -ldvbapi -lpthread -o $@ $^

setmod: octoserve.h octoserve.o dvb.o config.o
	$(CC) $(CFLAGS) -ldvbcfg -ldvben50221 -lucsi -ldvbsec -ldvbapi -lpthread -o $@ $^

clean:
	rm *.o
