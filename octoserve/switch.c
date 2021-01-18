#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include "octoserve.h"

extern uint32_t debug;

int readreg(int fd, uint32_t reg, uint32_t *val)
{
	struct ddb_reg ddbreg = { .reg = reg };
	int ret;
	
	ret = ioctl(fd, IOCTL_DDB_READ_REG, &ddbreg);
	if (ret < 0) 
		return ret;
	if (val)
		*val = ddbreg.val;
	return 0;
}

int writereg(int fd, uint32_t reg, uint32_t val)
{
	struct ddb_reg ddbreg = { .reg = reg, .val = val};

	return ioctl(fd, IOCTL_DDB_WRITE_REG, &ddbreg);
}

uint16_t mdio_readreg(int fd, uint8_t adr, uint8_t reg, uint16_t *val)
{
#if 0
	uint32_t tmp;

	writereg(fd, 0x24, adr);
	writereg(fd, 0x28, reg);
	writereg(fd, 0x20, 0x07);
	do {
		readreg(fd, 0x20, &tmp);
	} while (tmp & 0x02);
	readreg(fd, 0x2c, &tmp);
	*val = tmp;
#else
	struct ddb_mdio mdio = { .adr = adr, .reg = reg};
	ioctl(fd, IOCTL_DDB_READ_MDIO, &mdio);
	*val = mdio.val;
#endif
	return *val;
}

int mdio_writereg(int fd, uint8_t adr, uint8_t reg, uint16_t val)
{
#if 0
	uint32_t tmp = val;

	writereg(fd, 0x24, adr);
	writereg(fd, 0x28, reg);
	writereg(fd, 0x2c, tmp);
	writereg(fd, 0x20, 0x03);
	do {
		readreg(fd, 0x20, &tmp);
	} while (tmp & 0x02);
	return 0;
#else
	struct ddb_mdio mdio = { .adr = adr, .reg = reg, .val = val};
	return ioctl(fd, IOCTL_DDB_WRITE_MDIO, &mdio);
#endif
}

int mdio_wait_switch(int fd, uint8_t adr, uint8_t reg)
{
	uint16_t val;
	
	do {
		mdio_readreg(fd, adr, reg, &val);
	} while (val & 0x8000);
}

int mdio_open()
{
	int i, r;

	for (i = 0; i < 100; i++) {
		r = open("/dev/ddbridge/card0", O_RDWR);
		if (r >= 0)
			return r;
		if (r < 0)
			if (errno != EBUSY)
				return r;
			else 
				usleep(100000);
	}
	dbgprintf(DEBUG_SWITCH, "MDIO BUSY\n");
	return r;
}

int mdio_close(int fd)
{
	close(fd);
}

int switch_set_entry(uint8_t mac[6], uint8_t vec, uint8_t type)
{
	int fd = mdio_open();

	dbgprintf(DEBUG_SWITCH, "switch_set_entry %02x:%02x:%02x:%02x:%02x:%02x = %02x %02x\n", 
		  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], vec, type);
	if (fd < 0) 
		return -1;
	if (vec)
		mdio_writereg(fd, 0x1b, 0x0c, (vec << 4) | type);
	else
		mdio_writereg(fd, 0x1b, 0x0c, 0);
	mdio_writereg(fd, 0x1b, 0x0d, ((uint16_t) mac[0] << 8) | mac[1]);
	mdio_writereg(fd, 0x1b, 0x0e, ((uint16_t) mac[2] << 8) | mac[3]);
	mdio_writereg(fd, 0x1b, 0x0f, ((uint16_t) mac[4] << 8) | mac[5]);
	mdio_writereg(fd, 0x1b, 0x0b, 0xb000);
	mdio_wait_switch(fd, 0x1b, 0x0b);
	mdio_close(fd);	
	return 0;
}

int switch_get_port(uint8_t mac[6])
{
	int fd = mdio_open();
	uint16_t vector, state;
	uint16_t r0c, aw1, aw2, aw3;
	uint16_t w1, w2, w3;
	uint64_t m = ((uint64_t) mac[0] << 40) | ((uint64_t) mac[1] << 32) | ((uint64_t) mac[2] << 24) |
		((uint64_t) mac[3] << 16) | ((uint64_t) mac[4] << 8) | (uint64_t) mac[5];
	w1 = (mac[0] << 8) | mac[1];
	w2 = (mac[2] << 8) | mac[3];
	w3 = (mac[4] << 8) | mac[5];

	m--;
	mdio_writereg(fd, 0x1b, 0x0d, (m >> 32) & 0xffff);
	mdio_writereg(fd, 0x1b, 0x0e, (m >> 16) & 0xffff);
	mdio_writereg(fd, 0x1b, 0x0f, (m >>  0) & 0xffff);
	mdio_writereg(fd, 0x1b, 0x0b, 0xc000);
	mdio_wait_switch(fd, 0x1b, 0x0b);
	mdio_readreg(fd, 0x1b, 0x0c, &r0c);
	
	if (!(r0c & 0x0f)) {
		mdio_close(fd);
		return -1;
	}
	vector = (r0c >> 4) & 0x7f;
	state = r0c & 0x0f;

	mdio_readreg(fd, 0x1b, 0x0d, &aw1);
	mdio_readreg(fd, 0x1b, 0x0e, &aw2);
	mdio_readreg(fd, 0x1b, 0x0f, &aw3);
	mdio_close(fd);

	if (w1 == aw1 && w2 == aw2 && w3 == aw3) {
		dbgprintf(DEBUG_SWITCH, "%02x: %02x:%02x:%02x:%02x:%02x:%02x %d\n", 
			  vector,
			  aw1 >> 8, aw1 & 0xff, 
			  aw2 >> 8, aw2 & 0xff, 
			  aw3 >> 8, aw3 & 0xff, 
			  state);
		return vector;
	}
	return -1;
}


int switch_set_multicast(uint8_t mac[6], uint8_t vec)
{
	int fd = mdio_open();

	dbgprintf(DEBUG_SWITCH, "switch_set_multicast %02x:%02x:%02x:%02x:%02x:%02x = %02x\n", 
		  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], vec);
	if (fd < 0) 
		return -1;
	if (vec)
		mdio_writereg(fd, 0x1b, 0x0c, (vec << 4) | 7);
	else
		mdio_writereg(fd, 0x1b, 0x0c, 0);
	mdio_writereg(fd, 0x1b, 0x0d, ((uint16_t) mac[0] << 8) | mac[1]);
	mdio_writereg(fd, 0x1b, 0x0e, ((uint16_t) mac[2] << 8) | mac[3]);
	mdio_writereg(fd, 0x1b, 0x0f, ((uint16_t) mac[4] << 8) | mac[5]);
	mdio_writereg(fd, 0x1b, 0x0b, 0xb000);
	mdio_wait_switch(fd, 0x1b, 0x0b);
	mdio_close(fd);	
	return 0;
}

int switch_test(void)
{
	int fd = mdio_open();
	int res;

	res = mdio_writereg(fd, 0x1b, 0x0d, 0xffff);
	if (res < 0)
		return 0;
	mdio_writereg(fd, 0x1b, 0x0e, 0xffff);
	mdio_writereg(fd, 0x1b, 0x0f, 0xffff);

	while (1) {
		uint16_t r0c, aw1, aw2, aw3;
		uint16_t vector, state;
		
		mdio_writereg(fd, 0x1b, 0x0b, 0xc000);
		mdio_wait_switch(fd, 0x1b, 0x0b);
		mdio_readreg(fd, 0x1b, 0x0c, &r0c);
		if (!(r0c & 0x0f))
			break;
		vector = (r0c >> 4) & 0x7f;
		state = r0c & 0x0f;

		mdio_readreg(fd, 0x1b, 0x0d, &aw1);
		mdio_readreg(fd, 0x1b, 0x0e, &aw2);
		mdio_readreg(fd, 0x1b, 0x0f, &aw3);

		dbgprintf(DEBUG_SWITCH, "%02x: %02x:%02x:%02x:%02x:%02x:%02x %d\n", 
			  vector,
			  aw1 >> 8, aw1 & 0xff, 
			  aw2 >> 8, aw2 & 0xff, 
			  aw3 >> 8, aw3 & 0xff, 
			  state);
	}
	mdio_close(fd);
	return 1;
}
