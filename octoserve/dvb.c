/*  
    (C) 2012-14 Digital Devices GmbH. 

    This file is part of the octoserve SAT>IP server.

    Octoserve is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Octoserve is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with octoserve.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "octoserve.h"

extern uint32_t debug;

#define MMI_STATE_CLOSED 0
#define MMI_STATE_OPEN 1
#define MMI_STATE_ENQ 2
#define MMI_STATE_MENU 3

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static int set_fmode(uint32_t fmode)
{
	FILE *f;

	if ((f = fopen ("/sys/class/ddbridge/ddbridge0/fmode1", "r+")) == NULL)
		return -1;
	fprintf(f, "%u", fmode);
	fclose(f);
	return 0;
}


#include <libucsi/dvb/section.h>
#include <libucsi/mpeg/section.h>
#include <libucsi/section.h>
#include <libucsi/section_buf.h>
#include <libdvben50221/en50221_stdcam.h>

int sectest(void)
{
	uint8_t ts[4096];
	struct dvb_nsd_ts nsdts;
	int nsd, r, secstat, len;
	struct section_buf secbuf;
	struct section *section;
	struct section_ext *section_ext = NULL;

	section_buf_init(&secbuf, 4096);
	memset (&nsdts, 0, sizeof(nsdts));
	nsdts.pid = 0;
	nsdts.ts = ts;
	nsdts.mode = 1;
	while ((nsd = open("/dev/dvb/adapter0/nsd0", O_RDWR)) < 0) {
		if (errno == EBUSY)
			usleep(10000);
		else 
			return -1;
	}
	ioctl(nsd, NSD_START_GET_TS, &nsdts);
	while ((r = ioctl(nsd, NSD_POLL_GET_TS, &nsdts)) < 0 && errno == EBUSY)
		usleep(1000);
	if (!r) {
#if 0
		len = section_buf_add(&secbuf, nsdts.ts, nsdts.len, &secstat);
		section = section_codec(section_buf_data(&secbuf), len);
		section_ext = section_ext_decode(section, 1);
#endif		
		dbgprintf(DEBUG_DVB, "read %d bytes:\n", nsdts.len);
		dump(ts, nsdts.len);
	}
	ioctl(nsd, NSD_STOP_GET_TS, &nsdts);
	close(nsd);
}

static pthread_mutex_t nsd_lock;

static int getsec(int input, uint16_t pid, uint16_t id, uint8_t table, uint8_t *sec)
{
	struct dvb_nsd_ts nsdts;
	int nsd, r, len;

	dbgprintf(DEBUG_DVB,
		  "getsec input %d, pid %04x, id %04x, table %d\n", input, pid, id, table);
	pthread_mutex_lock(&nsd_lock);
	memset (&nsdts, 0, sizeof(nsdts));
	nsdts.pid = pid;
	nsdts.ts = sec;
	nsdts.mode = 1;
	nsdts.table = table;
	nsdts.input = input;
	if (id != 0) {
		nsdts.filter_mask = 2;
		nsdts.section_id = id;
	}

	while ((nsd = open("/dev/dvb/adapter0/nsd0", O_RDWR)) < 0) {
		if (errno == EBUSY)
			usleep(100000);
		else 
			return -1;
	}
	ioctl(nsd, NSD_START_GET_TS, &nsdts);
	while ((r = ioctl(nsd, NSD_POLL_GET_TS, &nsdts)) < 0 && errno == EBUSY)
		usleep(1000);
	ioctl(nsd, NSD_STOP_GET_TS, &nsdts);
	close(nsd);
	pthread_mutex_unlock(&nsd_lock);
	if (!r)
		return nsdts.len;
	else 
		return -1;
}

static int set_property(int fd, uint32_t cmd, uint32_t data)
{
	struct dtv_property p;
	struct dtv_properties c;
	int ret;

	p.cmd = cmd;
	c.num = 1;
	c.props = &p;
	p.u.data = data;
	ret = ioctl(fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	return 0;
}


static int get_property(int fd, uint32_t cmd, uint32_t *data)
{
	struct dtv_property p;
	struct dtv_properties c;
	int ret;

	p.cmd = cmd;
	c.num = 1;
	c.props = &p;
	ret = ioctl(fd, FE_GET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_GET_PROPERTY returned %d\n", ret);
		return -1;
	}
	*data = p.u.data;
	return 0;
}


static int get_stat(int fd, uint32_t cmd, struct dtv_fe_stats *stats)
{
	struct dtv_property p;
	struct dtv_properties c;
	int ret;

	p.cmd = cmd;
	c.num = 1;
	c.props = &p;
	ret = ioctl(fd, FE_GET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_GET_PROPERTY returned %d\n", ret);
		return -1;
	}
	memcpy(stats, &p.u.st, sizeof(struct dtv_fe_stats));
	return 0;
}

static int set_fe_input(int fd, uint32_t fr, uint32_t sr, fe_delivery_system_t ds,
			uint32_t input)
{
	struct dtv_property p[] = {
		{ .cmd = DTV_CLEAR },
		{ .cmd = DTV_DELIVERY_SYSTEM, .u.data = ds },
		{ .cmd = DTV_FREQUENCY, .u.data = fr },
		{ .cmd = DTV_INVERSION, .u.data = INVERSION_AUTO },
		{ .cmd = DTV_SYMBOL_RATE, .u.data = sr },
		{ .cmd = DTV_INNER_FEC, .u.data = FEC_AUTO },
		{ .cmd = DTV_INPUT, .u.data = input },
//		{ .cmd = DTV_STREAM_ID, .u.data = fe->param[PARAM_ISI] },
		{ .cmd = DTV_TUNE },
	};		
	struct dtv_properties c;
	int ret;
	
	dbgprintf(DEBUG_DVB, "ds = %u, input = %u\n", ds, input);
	
	c.num = ARRAY_SIZE(p);
	c.props = p;
	ret = ioctl(fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	return 0;
}

static int set_fe(int fd, uint32_t fr, uint32_t sr, fe_delivery_system_t ds)
{
	struct dtv_property p[] = {
		{ .cmd = DTV_CLEAR },
		{ .cmd = DTV_DELIVERY_SYSTEM, .u.data = ds },
		{ .cmd = DTV_FREQUENCY, .u.data = fr },
		{ .cmd = DTV_INVERSION, .u.data = INVERSION_AUTO },
		{ .cmd = DTV_SYMBOL_RATE, .u.data = sr },
		{ .cmd = DTV_INNER_FEC, .u.data = FEC_AUTO },
//		{ .cmd = DTV_STREAM_ID, .u.data = fe->param[PARAM_ISI] },
		{ .cmd = DTV_TUNE },
	};		
	struct dtv_properties c;
	int ret;
	
	c.num = ARRAY_SIZE(p);
	c.props = p;
	ret = ioctl(fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	return 0;
}

static void diseqc_send_msg(int fd, fe_sec_voltage_t v, 
			    struct dvb_diseqc_master_cmd *cmd,
			    fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b, 
			    int wait)
{
	if (ioctl(fd, FE_SET_TONE, SEC_TONE_OFF) == -1)
		perror("FE_SET_TONE failed");
	if (ioctl(fd, FE_SET_VOLTAGE, v) == -1)
		perror("FE_SET_VOLTAGE failed");
	usleep(15 * 1000);
	if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, cmd) == -1)
		perror("FE_DISEQC_SEND_MASTER_CMD failed");
	usleep(wait * 1000);
	usleep(15 * 1000);
	if (ioctl(fd, FE_DISEQC_SEND_BURST, b) == -1)
		perror("FE_DISEQC_SEND_BURST failed");
	usleep(15 * 1000);
	if (ioctl(fd, FE_SET_TONE, t) == -1)
		perror("FE_SET_TONE failed");
}

static int diseqc(int fd, int sat, int hor, int band)
{
	struct dvb_diseqc_master_cmd cmd = {
		.msg = {0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00},
		.msg_len = 4
	};

	hor &= 1;
	cmd.msg[3] = 0xf0 | ( ((sat << 2) & 0x0c) | (band ? 1 : 0) | (hor ? 2 : 0));
	
	diseqc_send_msg(fd, hor ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13,
			&cmd, band ? SEC_TONE_ON : SEC_TONE_OFF,
			(sat & 1) ? SEC_MINI_B : SEC_MINI_A, 0);
	dbgprintf(DEBUG_DVB, "MS %02x %02x %02x %02x\n", 
		  cmd.msg[0], cmd.msg[1], cmd.msg[2], cmd.msg[3]);
	return 0;
}

static int set_en50494(int fd, uint32_t freq, uint32_t sr, 
		       int sat, int hor, int band, 
		       uint32_t slot, uint32_t ubfreq,
		       fe_delivery_system_t ds)
{
	struct dvb_diseqc_master_cmd cmd = {
		.msg = {0xe0, 0x11, 0x5a, 0x00, 0x00},
		.msg_len = 5
	};
	uint16_t t;

	t = (freq + ubfreq + 2) / 4 - 350;
	hor &= 1;

	cmd.msg[3] = ((t & 0x0300) >> 8) | 
		(slot << 5) | ((sat & 0x3f) ? 0x10 : 0) | (band ? 4 : 0) | (hor ? 8 : 0);
	cmd.msg[4] = t & 0xff;

	set_property(fd, DTV_INPUT, 3 & (sat >> 6));
	if (ioctl(fd, FE_SET_TONE, SEC_TONE_OFF) == -1)
		perror("FE_SET_TONE failed");
	if (ioctl(fd, FE_SET_VOLTAGE, SEC_VOLTAGE_18) == -1)
		perror("FE_SET_VOLTAGE failed");
	usleep(15000);
	if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1)
		perror("FE_DISEQC_SEND_MASTER_CMD failed");
	usleep(15000);
	if (ioctl(fd, FE_SET_VOLTAGE, SEC_VOLTAGE_13) == -1)
		perror("FE_SET_VOLTAGE failed");

	set_fe_input(fd, ubfreq * 1000, sr * 1000, ds, 3 & (sat >> 6));
	dbgprintf(DEBUG_DVB, "EN50494 %02x %02x %02x %02x %02x\n", 
		  cmd.msg[0], cmd.msg[1], cmd.msg[2], cmd.msg[3], cmd.msg[4]);
}

static int set_en50607(int fd, uint32_t freq, uint32_t sr, 
		       int sat, int hor, int band, 
		       uint32_t slot, uint32_t ubfreq,
		       fe_delivery_system_t ds)
{
	struct dvb_diseqc_master_cmd cmd = {
		.msg = {0x70, 0x00, 0x00, 0x00, 0x00},
		.msg_len = 4
	};
	uint32_t t = freq - 100;

	hor &= 1;
	cmd.msg[1] = slot << 3;
	cmd.msg[1] |= ((t >> 8) & 0x07);
	cmd.msg[2] = (t & 0xff);
	cmd.msg[3] = ((sat & 0x3f) << 2) | (hor ? 2 : 0) | (band ? 1 : 0);

	set_property(fd, DTV_INPUT, 3 & (sat >> 6));
	if (ioctl(fd, FE_SET_TONE, SEC_TONE_OFF) == -1)
		perror("FE_SET_TONE failed");
	if (ioctl(fd, FE_SET_VOLTAGE, SEC_VOLTAGE_18) == -1)
		perror("FE_SET_VOLTAGE failed");
	usleep(15000);
	if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1)
		perror("FE_DISEQC_SEND_MASTER_CMD failed");
	usleep(15000);
	if (ioctl(fd, FE_SET_VOLTAGE, SEC_VOLTAGE_13) == -1)
		perror("FE_SET_VOLTAGE failed");

	set_fe_input(fd, ubfreq * 1000, sr * 1000, ds, 3 & (sat >> 6));
	dbgprintf(DEBUG_DVB, "EN50607 %02x %02x %02x %02x\n", 
		  cmd.msg[0], cmd.msg[1], cmd.msg[2], cmd.msg[3]);
}

static int tune_sat(struct dvbfe *fe)
{
	uint32_t freq, hi = 0, src, lnb = 0, lnbc = 0, lofs;
	fe_delivery_system_t ds = fe->n_param[PARAM_MSYS] - 1;
	
	dbgprintf(DEBUG_DVB, "tune_sat\n");
	freq = fe->param[PARAM_FREQ];

	if (fe->param[PARAM_SRC])
		lnb = fe->param[PARAM_SRC] - 1;

	lnbc = lnb & (MAX_SOURCE - 1);
	lofs = fe->lofs[lnbc];
#if 0
	if (freq < 5000000) { //3400 - 4200 ->5150
		lofs = 5150000;
		if (freq > lofs)
			freq -= lofs;
		else
			freq = lofs - freq;
	} else 	if (freq > 19700000 && freq < 22000000) { //19700-22000 ->21200
		lofs = 21200000;
		if (freq > lofs)
			freq -= lofs;
		else
			freq = lofs - freq;
	} else 
#endif
	{
		if (lofs)
			hi = (freq > lofs) ? 1 : 0;
		if (hi) 
			freq -= fe->lof2[lnbc];
		else
			freq -= fe->lof1[lnbc];
	}
	if (fe->first) {
		fe->first = 0;
		dbgprintf(DEBUG_DVB, "pre voltage %d\n", fe->prev_delay[lnbc]);
		if (ioctl(fe->fd, FE_SET_VOLTAGE, SEC_VOLTAGE_13) == -1)
			perror("FE_SET_VOLTAGE failed");
		usleep(fe->prev_delay[lnbc]);
	}
	dbgprintf(DEBUG_DVB, "scif_type = %u\n", fe->scif_type);
	if (fe->scif_type == 1) { 
		pthread_mutex_lock(&fe->os->uni_lock);
		set_en50494(fe->fd, freq / 1000, fe->param[PARAM_SR],
			    lnb, fe->param[PARAM_POL] - 1, hi,
			    fe->scif_slot, fe->scif_freq, ds);
		pthread_mutex_unlock(&fe->os->uni_lock);
	} else if (fe->scif_type == 2) {
		pthread_mutex_lock(&fe->os->uni_lock);
		set_en50607(fe->fd, freq / 1000, fe->param[PARAM_SR],
			    lnb, fe->param[PARAM_POL] - 1, hi,
			    fe->scif_slot, fe->scif_freq, ds);
		pthread_mutex_unlock(&fe->os->uni_lock);
	} else {
		//set_property(fe->fd, DTV_INPUT, 3 & (lnb >> 6));
		diseqc(fe->fd, lnb, fe->param[PARAM_POL] - 1, hi);
		set_fe(fe->fd, freq, fe->param[PARAM_SR] * 1000, ds);
	}
}

static int tune_c(struct dvbfe *fe)
{
	struct dtv_property p[] = {
		{ .cmd = DTV_CLEAR },
		{ .cmd = DTV_FREQUENCY, .u.data = fe->param[PARAM_FREQ] * 1000 },
		{ .cmd = DTV_BANDWIDTH_HZ, .u.data = fe->param[PARAM_BW_HZ] ? : 8000000 },
		{ .cmd = DTV_SYMBOL_RATE, .u.data = fe->param[PARAM_SR] * 1000 },
		{ .cmd = DTV_INNER_FEC,
		  .u.data = fe->param[PARAM_FEC] ? (fe->param[PARAM_FEC] - 1) : FEC_AUTO },
		{ .cmd = DTV_MODULATION,
		  .u.data = fe->param[PARAM_MTYPE]  ? (fe->param[PARAM_MTYPE] - 1) : QAM_AUTO },
		{ .cmd = DTV_TUNE },
	};		
	struct dtv_properties c;
	int ret;

	set_property(fe->fd, DTV_DELIVERY_SYSTEM, SYS_DVBC_ANNEX_A);

	c.num = ARRAY_SIZE(p);
	c.props = p;
	ret = ioctl(fe->fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	return 0;
}

static int tune_cable(struct dvbfe *fe)
{
	uint32_t freq;
	struct dvb_frontend_parameters p = {
		.frequency = fe->param[PARAM_FREQ] * 1000,
		.u.qam.symbol_rate = fe->param[PARAM_SR] * 1000,
		.u.qam.fec_inner = fe->param[PARAM_FEC] ? (fe->param[PARAM_FEC] - 1) : FEC_AUTO,
		.u.qam.modulation = fe->param[PARAM_MTYPE] - 1,
	};
	set_property(fe->fd, DTV_DELIVERY_SYSTEM, SYS_DVBC_ANNEX_A);
	if (ioctl(fe->fd, FE_SET_FRONTEND, &p) == -1) {
		perror("FE_SET_FRONTEND error");
		return -1;
	}
	return 0;
}

static int tune_terr(struct dvbfe *fe)
{
	struct dtv_property p[] = {
		{ .cmd = DTV_CLEAR },
		{ .cmd = DTV_FREQUENCY, .u.data = fe->param[PARAM_FREQ] * 1000 },
		{ .cmd = DTV_BANDWIDTH_HZ, .u.data = fe->param[PARAM_BW_HZ] },
		{ .cmd = DTV_TUNE },
	};		
	struct dtv_properties c;
	int ret;

	set_property(fe->fd, DTV_DELIVERY_SYSTEM, SYS_DVBT);

	c.num = ARRAY_SIZE(p);
	c.props = p;
	ret = ioctl(fe->fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	return 0;
}
#if 0
static int tune_terr(struct dvbfe *fe)
{
	uint32_t freq;
	enum fe_bandwidth bw;
	struct dvb_frontend_parameters p = {
		.frequency = fe->param[PARAM_FREQ] * 1000,
		.inversion = INVERSION_AUTO,
		.u.ofdm.code_rate_HP = FEC_AUTO,
		.u.ofdm.code_rate_LP = FEC_AUTO,
		.u.ofdm.constellation = fe->param[PARAM_MTYPE] - 1,
		.u.ofdm.transmission_mode = TRANSMISSION_MODE_AUTO,
		.u.ofdm.guard_interval = GUARD_INTERVAL_AUTO,
		.u.ofdm.hierarchy_information = HIERARCHY_AUTO,
		.u.ofdm.bandwidth = fe->param[PARAM_BW] ? 
		                   (fe->param[PARAM_BW] - 1) : BANDWIDTH_AUTO,
	};
	set_property(fe->fd, DTV_DELIVERY_SYSTEM, SYS_DVBT);
	if (ioctl(fe->fd, FE_SET_FRONTEND, &p) == -1) {
		perror("FE_SET_FRONTEND error");
		return -1;
	}
	return 0;
}
#endif

static int tune_c2(struct dvbfe *fe)
{
	struct dtv_property p[] = {
		{ .cmd = DTV_CLEAR },
		{ .cmd = DTV_FREQUENCY, .u.data = fe->param[PARAM_FREQ] * 1000 },
		{ .cmd = DTV_BANDWIDTH_HZ, .u.data = fe->param[PARAM_BW_HZ] },
		{ .cmd = DTV_STREAM_ID, .u.data = fe->param[PARAM_PLP] },
		{ .cmd = DTV_TUNE },
	};		
	struct dtv_properties c;
	int ret;

	set_property(fe->fd, DTV_DELIVERY_SYSTEM, SYS_DVBC2);

	c.num = ARRAY_SIZE(p);
	c.props = p;
	ret = ioctl(fe->fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	return 0;
}

static int tune_terr2(struct dvbfe *fe)
{
	struct dtv_property p[] = {
		{ .cmd = DTV_CLEAR },
		{ .cmd = DTV_FREQUENCY, .u.data = fe->param[PARAM_FREQ] * 1000 },
		{ .cmd = DTV_BANDWIDTH_HZ, .u.data = fe->param[PARAM_BW_HZ] },
		{ .cmd = DTV_STREAM_ID, .u.data = fe->param[PARAM_PLP] },
		{ .cmd = DTV_TUNE },
	};		
	struct dtv_properties c;
	int ret;

	set_property(fe->fd, DTV_DELIVERY_SYSTEM, SYS_DVBT2);

	c.num = ARRAY_SIZE(p);
	c.props = p;
	ret = ioctl(fe->fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	return 0;
}

static int tune(struct dvbfe *fe)
{
	int ret;

	printf("tune()\n");
	
	switch (fe->n_param[PARAM_MSYS] - 1) {
	case SYS_DVBS:
	case SYS_DVBS2:
		ret = tune_sat(fe);
		break;
	case SYS_DVBC_ANNEX_A:
		ret = tune_c(fe);
		break;
	case SYS_DVBT:
		ret = tune_terr(fe);
		break;
	case SYS_DVBT2:
		ret = tune_terr2(fe);
		break;
	case SYS_DVBC2:
		ret = tune_c2(fe);
		break;
	default:
		break;
	}
	return ret;
}

static int open_dmx(struct dvbfe *fe)
{
	char fname[80];
	struct dmx_pes_filter_params pesFilterParams; 
	
	sprintf(fname, "/dev/dvb/adapter%u/demux%u", fe->anum, fe->fnum); 

	fe->dmx = open(fname, O_RDWR);
	if (fe->dmx < 0) 
		return -1;

	pesFilterParams.input = DMX_IN_FRONTEND; 
	pesFilterParams.output = DMX_OUT_TS_TAP; 
	pesFilterParams.pes_type = DMX_PES_OTHER; 
	pesFilterParams.flags = DMX_IMMEDIATE_START;
  	pesFilterParams.pid = 0x00;

	if (ioctl(fe->dmx, DMX_SET_PES_FILTER, &pesFilterParams) < 0)
		return -1;
	return 0;
}


static int open_fe(struct dvbfe *fe)
{
	char fname[80];

	sprintf(fname, "/dev/dvb/adapter%d/frontend%d", fe->anum, fe->fnum); 
	fe->fd = open(fname, O_RDWR);
	if (fe->fd < 0) 
		return -1;
	return 0;
}

static int32_t Log10x100(uint32_t x)
{
	static uint32_t LookupTable[100] = {
		101157945, 103514217, 105925373, 108392691, 110917482,
		113501082, 116144861, 118850223, 121618600, 124451461, // 800.5 - 809.5
		127350308, 130316678, 133352143, 136458314, 139636836,
		142889396, 146217717, 149623566, 153108746, 156675107, // 810.5 - 819.5
		160324539, 164058977, 167880402, 171790839, 175792361,
		179887092, 184077200, 188364909, 192752491, 197242274, // 820.5 - 829.5
		201836636, 206538016, 211348904, 216271852, 221309471,
		226464431, 231739465, 237137371, 242661010, 248313311, // 830.5 - 839.5
		254097271, 260015956, 266072506, 272270131, 278612117,
		285101827, 291742701, 298538262, 305492111, 312607937, // 840.5 - 849.5
		319889511, 327340695, 334965439, 342767787, 350751874,
		358921935, 367282300, 375837404, 384591782, 393550075, // 850.5 - 859.5
		402717034, 412097519, 421696503, 431519077, 441570447,
		451855944, 462381021, 473151259, 484172368, 495450191, // 860.5 - 869.5
		506990708, 518800039, 530884444, 543250331, 555904257,
		568852931, 582103218, 595662144, 609536897, 623734835, // 870.5 - 879.5
		638263486, 653130553, 668343918, 683911647, 699841996,
		716143410, 732824533, 749894209, 767361489, 785235635, // 880.5 - 889.5
		803526122, 822242650, 841395142, 860993752, 881048873,
		901571138, 922571427, 944060876, 966050879, 988553095, // 890.5 - 899.5
	};
	int32_t y = 800;
	int i = 0;

	if( x == 0 ) return 0;
	
	if( x >= 1000000000 ) {
		x /= 10;
		y += 100;
	}
	
	while (x < 100000000 ) {
		x *= 10;
		y -= 100;
	}
	
	while ( i < 100 && x > LookupTable[i] ) i += 1;
	y += i;
	return y;
}

static int32_t berq_rs(uint32_t BERNumerator, uint32_t BERDenominator)
{
     int32_t LogBER = Log10x100(BERDenominator) - Log10x100(BERNumerator);
     int32_t BERQual = 100;
     
     if ( BERNumerator == 0 )
	     return 100;
     if (LogBER < 700) {
	     if (LogBER <  300)
		     BERQual = 0;
	     else
		     BERQual = (LogBER + 5) / 5 - 40;
     }
     return BERQual;
}

static int32_t berq_bch(uint32_t BERNumerator, uint32_t BERDenominator)
{
     int32_t LogBER = Log10x100(BERDenominator) - Log10x100(BERNumerator);
     int32_t BERQual = 100;
     
     if (BERNumerator == 0 )
	     return 100;
     if (LogBER < 700) {
	     if( LogBER <  400 )
		     BERQual = 0;
	     else
		     BERQual = 40;
     }
     return BERQual;
}


static uint32_t ber_quality(struct dvbfe *fe)
{
	struct dtv_fe_stats ber;
	
	get_stat(fe->fd, DTV_STAT_PRE_ERROR_BIT_COUNT, &ber);
	get_stat(fe->fd, DTV_STAT_PRE_TOTAL_BIT_COUNT, &ber);
	
	return 100;
}

static int32_t dvbsq(uint32_t snr, uint32_t fec,
		     uint32_t ber_num, uint32_t ber_den)
{
	int32_t SignalToNoiseRel = -1000;
	int32_t Quality = 0;
	int32_t BERQuality = berq_rs(ber_num, ber_den);
	
	// SNR Values for quasi errorfree reception from Nordig 2.2
	static const int32_t DVBS_SN[] = {
		// 1/2 2/3 3/4    5/6    7/8
		0, 38, 56, 67, 0, 77, 0, 84
	};
	
	if (fec >= FEC_NONE && fec <= FEC_7_8 )
		SignalToNoiseRel = snr / 100 - DVBS_SN[fec];
	
	if( SignalToNoiseRel < -70 )
		Quality = 0;
	else if( SignalToNoiseRel < 30 )
		Quality = ((SignalToNoiseRel + 70) * BERQuality) / 100;
	else
		Quality = BERQuality;
	return (Quality * 3) / 20;
}


int32_t dvbs2q(int32_t snr, uint32_t fec, uint32_t mod,
	       uint32_t ber_num, uint32_t ber_den)
{
	static const int32_t DVBS2_SN_QPSK[] = {
		// 1/2 2/3 3/4 4/5 5/6 6/7 7/8 8/9 AUT  3/5 9/10 2/5 
		0, 20, 41, 50, 57, 62,  0,  0, 72,  0,  32,  74,  7, 
	};
	static const int32_t DVBS2_SN_8PSK[] = {
		// 1/2 2/3 3/4 4/5  5/6 6/7 7/8  8/9 AUT  3/5 9/10 2/5 
		0,  0, 76, 89,  0, 104,  0,  0, 117,  0,  65, 120,  0, 
	};
	static const int32_t DVBS2_SN_16APSK[] = {
		// 1/2  2/3  3/4  4/5  5/6 6/7 7/8  8/9 AUT  3/5 9/10 2/5 
		0,  0, 100, 112, 120, 126,  0,  0, 139,  0,   0, 141,  0, 
	};
	static const int32_t DVBS2_SN_32APSK[] = {
		// 1/2 2/3  3/4  4/5  5/6 6/7 7/8  8/9 AUT  3/5 9/10 2/5 
		0,  0,  0, 137, 146, 153,  0,  0, 167,  0,   0, 171,  0, 
	};
	int32_t BERQuality = berq_bch(ber_num, ber_den);
	int32_t Quality = 0;
	int32_t SignalToNoiseRel = -1000, snc = 0;
	
	if (fec > FEC_2_5 )
		return 0;
	switch (mod) {
	case QPSK:
		snc = DVBS2_SN_QPSK[fec];
		break;
	case PSK_8:
		snc = DVBS2_SN_8PSK[fec];
		break;
	case APSK_16:
		snc = DVBS2_SN_16APSK[fec];
		break;
	case APSK_32:
		snc = DVBS2_SN_32APSK[fec];
		break;
	default:
		return 0;
	}
	SignalToNoiseRel = snr / 100 - snc;
	
	if (SignalToNoiseRel < -30 )
		Quality = 0;
	else if( SignalToNoiseRel < 30 )
		Quality = ((SignalToNoiseRel + 30) * BERQuality) / 60;
	else
		Quality = 100;
	return (Quality * 3) / 20;
}

static int32_t dvbcq(int32_t snr, uint32_t mod,
		     uint32_t ber_num, uint32_t ber_den)
{
	int32_t SignalToNoiseRel = 0;
	int32_t Quality = 0;
	int32_t BERQuality = berq_rs(ber_num, ber_den);
	
	switch (mod) {
	case QAM_16: SignalToNoiseRel = snr - 200; break;
	case QAM_32: SignalToNoiseRel = snr - 230; break;
	case QAM_64:  SignalToNoiseRel = snr - 260; break;
	case QAM_128: SignalToNoiseRel = snr - 290; break;
	case QAM_256: SignalToNoiseRel = snr - 320; break;
	}

	if (SignalToNoiseRel < -70)
		Quality = 0;
	else if (SignalToNoiseRel < 30)
		Quality = ((SignalToNoiseRel + 70) * BERQuality) / 100;
	else
		Quality = BERQuality;
	return (Quality * 3) / 20;
}

int32_t dvbtq(int32_t snr, uint32_t mod, uint32_t fec,
	      uint32_t ber_num, uint32_t ber_den)
{
	int32_t Quality = 0;
	int32_t BERQuality = berq_rs(ber_num, ber_den);
	int32_t SignalToNoiseRel = -1000, snc = 0;
	
	// SNR Values for quasi error free reception from Nordig 2.2
	static const int32_t DVBT_SN_QPSK[] = {
		// 1/2 2/3 3/4 4/5 5/6 6/7 7/8 8/9 AUT  3/5 9/10 2/5 
		0, 51, 69, 79,  0, 89,  0, 97,  0,  0,   0,   0,  0, 
	};
	static const int32_t DVBT_SN_QAM16[] = {
		//  1/2  2/3  3/4 4/5  5/6 6/7  7/8 8/9 AUT  3/5 9/10 2/5 
		0, 108, 131, 146,  0, 156,  0, 160,  0,  0,   0,   0,  0, 
	};
	static const int32_t DVBT_SN_QAM64[] = {
		//  1/2  2/3  3/4 4/5  5/6 6/7  7/8 8/9 AUT  3/5 9/10 2/5 
		0, 165, 187, 202,  0, 216,  0, 225,  0,  0,   0,   0,  0, 
	};

	if (fec > FEC_2_5 )
		return 0;
	switch (mod) {
	case QPSK:
		snc = DVBT_SN_QPSK[fec];
		break;
	case QAM_16:
		snc = DVBT_SN_QAM16[fec];
		break;
	case QAM_64:
		snc = DVBT_SN_QAM64[fec];
		break;
	default:
		break;
	}
	SignalToNoiseRel = snr / 100 - snc;

	if (SignalToNoiseRel < -70 )
		Quality = 0;
	else if (SignalToNoiseRel < 30)
		Quality = ((SignalToNoiseRel + 70) * BERQuality)/100;
	else
		Quality = BERQuality;
	
	return (Quality * 3) / 20;
}

#if 0

int32_t dvbt2q(int32_t snr, uint32_t mod, uint32_t fec, uint32_t trans, uint32_t pilot,
	      uint32_t ber_num, uint32_t ber_den)
{
	int32_t Quality = 0;
	int32_t BERQuality = BERQualityBCH(BERNominator, BERDenominator);
	int32_t SignalToNoiseRel = -1000;
	
	static const int32_t QE_SN[] = {
		// 1/2, 3/5,  2/3,  3/4,  4/5,  5/6, 1/3, 2/5
		32,  49,   59,   68,   74,   80,  15,  24,  // 16K QPSK
		82, 104,  116,  130,  136,  141,  62,  74,  // 16K 16-QAM
		123, 151,  165,  181,  190,  197, 101, 114,  // 16K 64-QAM
		164, 202,  211,  232,  246,  255, 137, 153,  // 16K 256-QAM
		
		35,  47,   56,   66,   72,   77,  13,  22,  // 64K QPSK
		87, 101,  114,  125,  133,  138,  60,  72,  // 64K 16-QAM
		130, 148,  162,  177,  187,  194,  98, 111,  // 64K 64-QAM
		170, 194,  208,  229,  243,  251, 132, 148,  // 64K 256-QAM
	};
	
	if( Modulation <= DVBT2_256QAM && CodeRate <= DVBT2_CR_2_5 && FECType <= DVBT2_64K  )
	{
		int Index = int(FECType) * 32 + int(Modulation) * 8 + int(CodeRate);
		SignalToNoiseRel = SignalToNoise - QE_SN[Index];
		
		if( PilotPattern >= DVBT2_PP3 && PilotPattern <= DVBT2_PP4 ) SignalToNoiseRel += 5;
		else if( PilotPattern >= DVBT2_PP5 && PilotPattern <= DVBT2_PP8 ) SignalToNoiseRel += 10;
	}
	
	if( SignalToNoiseRel < -30 ) Quality = 0;
	else if( SignalToNoiseRel < 30 )
	{
		Quality = ((SignalToNoiseRel + 30) * BERQuality)/60;
	}
	else
		Quality = 100;
	
	return (Quality * 3) / 20;
}
#endif

static void calc_lq(struct dvbfe *fe)
{
	struct dtv_fe_stats st;
	int64_t str, snr;
	uint32_t mod, fec, ber_num, ber_den;
	
	get_stat(fe->fd, DTV_STAT_SIGNAL_STRENGTH, &st);
	str = st.stat[0].uvalue;
	dbgprintf(DEBUG_DVB, "fe%d: str=%lld\n", fe->nr, str);
	str = (str * 48) / 10000 + 344;
	if (str < 0)
		str = 0;
	if (str > 255)
		str = 255;
	fe->level = str;
	// str: 0-255: -25dbm = 224, -65dbm = 32
	// qual: 0-15 15=BER<2*10^-4 PER<10^-7
	get_stat(fe->fd, DTV_STAT_CNR, &st);
	snr = st.stat[0].uvalue;
	get_property(fe->fd, DTV_INNER_FEC, &fec);
	fe->param[PARAM_FEC] = fec + 1;
	get_property(fe->fd, DTV_MODULATION, &mod);
	fe->param[PARAM_MTYPE] = mod + 1;

	get_stat(fe->fd, DTV_STAT_PRE_ERROR_BIT_COUNT, &st);
	ber_num = st.stat[0].uvalue;
	get_stat(fe->fd, DTV_STAT_PRE_TOTAL_BIT_COUNT, &st);
	ber_den = st.stat[0].uvalue;

	dbgprintf(DEBUG_DVB, "fe%d: snr=%lld ber=%llu/%llu\n",
		  fe->nr, snr, ber_num, ber_den);
	dbgprintf(DEBUG_DVB, "fe%d: fec=%u mod=%u\n", fe->nr, fec, mod);
	switch (fe->n_param[PARAM_MSYS] - 1) {
	case SYS_DVBS:
		fe->quality = dvbsq(snr, fec, ber_num, ber_den);
		break;
	case SYS_DVBS2:
		fe->quality = dvbs2q(snr, fec, mod, ber_num, ber_den);
		break;
	case SYS_DVBC_ANNEX_A:
		fe->quality = dvbcq(snr, mod, ber_num, ber_den);
		break;
	case SYS_DVBT:
		fe->quality = dvbtq(snr, mod, fec, ber_num, ber_den);
		break;
	case SYS_DVBT2:
		//fe->quality = dvbtq(snr, mod, fec, ber_num, ber_den);
		break;
	case SYS_DVBC2:
		break;
	default:
		break;
	}
	dbgprintf(DEBUG_DVB, "fe%d: level=%u quality=%u\n", fe->nr, fe->level, fe->quality);
}

static void get_stats(struct dvbfe *fe)
{
	uint16_t sig = 0, snr = 0;
	fe_status_t stat;
	uint32_t str, cnr;
	int64_t val;
	struct dtv_fe_stats st;
	
	ioctl(fe->fd, FE_READ_STATUS, &stat);
	fe->stat = stat;
	fe->lock = (stat == 0x1f) ? 1 : 0;
	calc_lq(fe);
}

void handle_fe(struct dvbfe *fe)
{
	uint32_t newtune, count = 0, max, nolock = 0;
	int ret;

	fe->dmx = -1;
	fe->tune = 0;
	memset(fe->param, 0, sizeof(fe->param));
	fe->first = 1;
	open_fe(fe);
	open_dmx(fe);
	while (fe->state == 1) {
		pthread_mutex_lock(&fe->mutex);
		newtune = fe->n_tune;
		if (newtune == 1) {
			fe->n_tune = 0;
			if (!memcmp(fe->param, fe->n_param, sizeof(fe->param))) {
				dbgprintf(DEBUG_DVB, "same params\n");
				fe->tune = 2;
				count = 0;
				nolock = 10;
				max = 2;
			} else { 
				memcpy(fe->param, fe->n_param, sizeof(fe->param));
				fe->tune = 1;
			}
		}
		pthread_mutex_unlock(&fe->mutex);
		
		switch (fe->tune) {
		case 1:
			dbgprintf(DEBUG_DVB, "fe %d tune\n", fe->nr);
			tune(fe);
			nolock = 0;
			count = 0;
			max = 2;
			dbgprintf(DEBUG_DVB, "fe %d tune done\n", fe->nr);
			fe->tune = 2;
			break;
		case 2: 
			count++;
			if (count < max) 
				break;
			count = 0;
			get_stats(fe);
			if (fe->lock) {
				max = 20;
				nolock = 0;
			} else {
				max = 1;
				nolock++;
				if (nolock > 20)
					fe->tune = 1;
			}
			break;

		default:
			break;
		}
		if (fe->state != 1)
			break;
		usleep(50000);
	}
	close(fe->fd);
	if (fe->dmx > 0)
		close(fe->dmx);
	fe->fd = -1;
	fe->dmx = -1;
	fe->stat = fe->lock = fe->level = fe->quality = 0;
	fe->state = 0;
	dbgprintf(DEBUG_DVB, "fe %d done\n", fe->nr);
}

int dvb_tune(struct dvbfe *fe, struct dvb_params *p)
{
	int ret = 0;

	dbgprintf(DEBUG_DVB, "dvb_tune\n");
	pthread_mutex_lock(&fe->mutex);
	memcpy(fe->n_param, p->param, sizeof(fe->n_param));
	fe->n_tune = 1;
	pthread_mutex_unlock(&fe->mutex);
	pthread_yield();
	return ret;
}

static int init_fe(struct octoserve *os, int a, int f, int fd, int nodvbt, int noswitch)
{
	struct dtv_properties dps;
	struct dtv_property dp[10];
	struct dvbfe *fe;
	int r;
	uint32_t i, ds;

	dbgprintf(DEBUG_DVB, "detect_dvb a=%d f=%d\n", a,f);
	fe = &os->dvbfe[os->dvbfe_num];
	dps.num = 1;
	dps.props = dp;
	dp[0].cmd = DTV_ENUM_DELSYS;
	r = ioctl(fd, FE_GET_PROPERTY, &dps);
	if (r < 0)
		return -1;
	for (i = 0; i < dp[0].u.buffer.len; i++) {
		ds = dp[0].u.buffer.data[i];
		dbgprintf(DEBUG_DVB, "delivery system %d\n", ds);
		fe->type |= (1UL << ds);
	}
	if (nodvbt)
		fe->type &= ~((1UL << SYS_DVBT2) | (1UL << SYS_DVBT));

	if (!fe->type)
		return -1;

	if (fe->type & (1UL << SYS_DVBS2))
		os->dvbs2num++;
	if (fe->type & (1UL << SYS_DVBT2))
		os->dvbt2num++;
	else if (fe->type & (1UL << SYS_DVBT))
		os->dvbtnum++;
	if (fe->type & (1UL << SYS_DVBC2))
		os->dvbc2num++;
	else if (fe->type & (1UL << SYS_DVBC_ANNEX_A))
		os->dvbcnum++;
	fe->os = os;
	fe->anum = a;
	fe->fnum = f;
	fe->nr = os->dvbfe_num + 1;
	
	dps.num = 1;
	dps.props = dp;
	dp[0].cmd = DTV_INPUT;
	r = ioctl(fd, FE_GET_PROPERTY, &dps);
	if (r < 0)
		return -1;
	for (i = 0; i < dp[0].u.buffer.len; i++) {
		fe->input[i] = dp[0].u.buffer.data[i];
		dbgprintf(DEBUG_DVB, "input prop %u = %u\n", i, fe->input[i]);
	}
	if (fe->input[3]) {
		os->has_feswitch = 1;
		if (noswitch) {
			if (fe->input[2] >= fe->input[1]) {
				fe->type = 0;
				return -1;
			}
		} else
			os->do_feswitch = 1;
	}
	
	os->dvbfe_num++;
	pthread_mutex_init(&fe->mutex, 0);
	return 0;
}

static int scan_dvbfe(struct octoserve *os, int nodvbt, int noswitch)
{
	int a, f, fd;
	char fname[80];

	for (a = 0; a < 16; a++) {
		for (f = 0; f < 16; f++) {
			sprintf(fname, "/dev/dvb/adapter%d/frontend%d", a, f); 
			fd = open(fname, O_RDWR);
			if (fd >= 0) {
				init_fe(os, a, f, fd, nodvbt, noswitch);
				close(fd);
			}
		}
	}
	dbgprintf(DEBUG_DVB, "Found %d frontends\n", os->dvbfe_num);
}

static int ai_callback(void *arg, uint8_t slot_id, uint16_t session_number,
		       uint8_t application_type, uint16_t application_manufacturer,
		       uint16_t manufacturer_code, uint8_t menu_string_length,
		       uint8_t *menu_string)
{
	struct dvbca *ca = arg;

	dbgprintf(DEBUG_DVB, "Application type: %02x\n", application_type);
	dbgprintf(DEBUG_DVB, "Application manufacturer: %04x\n", application_manufacturer);
	dbgprintf(DEBUG_DVB, "Manufacturer code: %04x\n", manufacturer_code);
	dbgprintf(DEBUG_DVB, "Menu string: %.*s\n", menu_string_length, menu_string);

	return 0;
}

static int ca_info_callback(void *arg, uint8_t slot_id, uint16_t snum, 
			    uint32_t id_count, uint16_t *ids)
{
	struct dvbca *ca = arg;
	uint32_t i;
	
	dbgprintf(DEBUG_DVB, "CAM supports the following ca system ids:\n");
	for (i = 0; i < id_count; i++) {
		dbgprintf(DEBUG_DVB, "  0x%04x\n", ids[i]);
	}
	ca->resource_ready = 1;
	return 0;
}

#if 0
static int handle_pmt(struct dvbca *ca, uint8_t *buf, int size)
{
	int listmgmt = CA_LIST_MANAGEMENT_ONLY;
        uint8_t capmt[4096];
	struct section *section = section_codec(buf, size);
	struct section_ext *section_ext = section_ext_decode(section, 0);
	struct mpeg_pmt_section *pmt = mpeg_pmt_section_codec(section_ext);

	dbgprintf(DEBUG_DVB, "handle pmt\n");
	if (section_ext->version_number == ca->ca_pmt_version &&
		ca->pmt == ca->pmt_old)
		return;
	if (ca->pmt != ca->pmt_old) {
		ca->pmt_old = ca->pmt;
		ca->sentpmt = 0;
	}
	if (ca->resource_ready) {
		ca->data_pmt_version = pmt->head.version_number;
		
		if (ca->sentpmt) {
			listmgmt = CA_LIST_MANAGEMENT_UPDATE;
			//return;
		}
		ca->sentpmt = 1;
		dbgprintf(DEBUG_DVB, "set ca_pmt\n");
	
		if ((size = en50221_ca_format_pmt(pmt, capmt, sizeof(capmt), ca->moveca, listmgmt,
						  CA_PMT_CMD_ID_OK_DESCRAMBLING)) < 0) {
			dbgprintf(DEBUG_DVB, "Failed to format PMT\n");
			return -1;
		}
		if (en50221_app_ca_pmt(ca->stdcam->ca_resource, ca->stdcam->ca_session_number, capmt, size)) {
			dbgprintf(DEBUG_DVB, "Failed to send PMT\n");
			return -1;
		}
	}
	
}
#endif


static void handle_tdt(struct dvbca *ca)
{
	struct section *section;
	struct dvb_tdt_section *tdt;
        uint8_t sec[4096];
	time_t dvb_time;
	int len;
	
	if (ca->stdcam == NULL)
		return;
	if (ca->stdcam->dvbtime == NULL)
		return;
	len = getsec(ca->input, 0x14, 0, 0x70, sec); 
	if (len < 0)
		return;
	dbgprintf(DEBUG_DVB, "got tdt\n");

	section = section_codec(sec, len);
	if (section == NULL)
		return;
	tdt = dvb_tdt_section_codec(section);
	if (tdt == NULL)
		return;
	dvb_time = dvbdate_to_unixtime(tdt->utc_time);

	dbgprintf(DEBUG_DVB, "set dvbtime\n");
	if (ca->stdcam->dvbtime)
		ca->stdcam->dvbtime(ca->stdcam, dvb_time);
}

static int handle_pmts(struct dvbca *ca)
{
	int listmgmt = CA_LIST_MANAGEMENT_ONLY;
        uint8_t sec[4096], capmt[4096];
	struct section *section;
	struct section_ext *section_ext;
	struct mpeg_pmt_section *pmt;
	int i, size, num, len;

	if (!ca->resource_ready)
		return 0;
	dbgprintf(DEBUG_DVB, "handle pmts\n");
	for (i = num = 0; i < MAX_PMT; i++) 
		if (ca->pmt[i])
			num++;
	for (i = 0; i < num; i++) {
		len = getsec(ca->input, ca->pmt[i] & 0xffff, ca->pmt[i] >> 16, 2, sec); 
		if (len < 0)
			continue;
		section = section_codec(sec, len);
		section_ext = section_ext_decode(section, 0);
		pmt = mpeg_pmt_section_codec(section_ext);

		ca->ca_pmt_version[i] = section_ext->version_number;
		if (ca->sentpmt) {
			//return 0;
			listmgmt = CA_LIST_MANAGEMENT_UPDATE;
		} else {
			listmgmt = CA_LIST_MANAGEMENT_ONLY;
			if (num > 1) {
				listmgmt = CA_LIST_MANAGEMENT_MORE;
				if (i == 0)
					listmgmt = CA_LIST_MANAGEMENT_FIRST;
				if (i == num - 1)
					listmgmt = CA_LIST_MANAGEMENT_LAST;
			}
		}
		dbgprintf(DEBUG_DVB, "set ca_pmt\n");
	
		if ((size = en50221_ca_format_pmt(pmt, capmt, sizeof(capmt), ca->moveca, listmgmt,
						  CA_PMT_CMD_ID_OK_DESCRAMBLING)) < 0) {
			dbgprintf(DEBUG_DVB, "Failed to format PMT\n");
			return -1;
		}
		dump(capmt, size);
		if (en50221_app_ca_pmt(ca->stdcam->ca_resource, ca->stdcam->ca_session_number, capmt, size)) {
			dbgprintf(DEBUG_DVB, "Failed to send PMT\n");
			return -1;
		}
	}
	if (num)
		ca->sentpmt = 1;
	return 0;
}

static void proc_csock_msg(struct dvbca *ca, uint8_t *buf, int len)
{
	if (*buf == '\r') {
		return;
	} else if (*buf == '\n') {
		switch(ca->mmi_state) {
		case MMI_STATE_CLOSED:
		case MMI_STATE_OPEN:
			if ((ca->mmi_bufp == 0) && (ca->resource_ready)) {
				en50221_app_ai_entermenu(ca->stdcam->ai_resource, 
							 ca->stdcam->ai_session_number);
			}
			break;
			
		case MMI_STATE_ENQ:
			if (ca->mmi_bufp == 0) {
				en50221_app_mmi_answ(ca->stdcam->mmi_resource, 
						     ca->stdcam->mmi_session_number,
						     MMI_ANSW_ID_CANCEL, NULL, 0);
			} else {
				en50221_app_mmi_answ(ca->stdcam->mmi_resource, 
						     ca->stdcam->mmi_session_number,
						     MMI_ANSW_ID_ANSWER, 
						     ca->mmi_buf, ca->mmi_bufp);
			}
			ca->mmi_state = MMI_STATE_OPEN;
			break;
			
		case MMI_STATE_MENU:
			ca->mmi_buf[ca->mmi_bufp] = 0;
			en50221_app_mmi_menu_answ(ca->stdcam->mmi_resource, 
						  ca->stdcam->mmi_session_number,
						  atoi(ca->mmi_buf));
			ca->mmi_state = MMI_STATE_OPEN;
			break;
		}
		ca->mmi_bufp = 0;
	} else {
		if (ca->mmi_bufp < (sizeof(ca->mmi_buf) - 1)) {
			ca->mmi_buf[ca->mmi_bufp++] = *buf;
		}
	}
}

static int proc_csock(struct dvbca *ca)
{
	uint8_t buf[1024];
	int len, i, res;
	
	if (ca->stdcam == NULL)
		return;
	while ((len = recv(ca->sock, buf, 1, 0)) >= 0) {
		if (len == 0) 
			goto release;
		if (len < 0) {
			if (errno != EAGAIN) 
				goto release;
			return 0;
		}
		proc_csock_msg(ca, buf, len);
	}
	return 0;
release:
	close(ca->sock);
	ca->sock = -1;
	return -1;
}

static void handle_ci(struct dvbca *ca)
{
	uint8_t sec[4096];
	uint32_t pmt_count, tdt_count;
	int len;
	int sock, i;
	struct sockaddr sadr;
	char port[6];

	snprintf(port, sizeof(port), "%u", (uint16_t) (8888 + ca->nr));
	sock = streamsock(port, AF_INET, &sadr);
	if (listen(sock, 4) < 0) {
		dbgprintf(DEBUG_DVB, "listen error");
		return;
	}
	ca->sock = -1;
	
	while (!ca->os->exit) {
		struct timeval timeout;
		uint32_t count = 0;
		int num;
		int mfd;
		fd_set fds;
		
		timeout.tv_sec = 0;
		timeout.tv_usec = 200000;
		FD_ZERO(&fds);
		if (ca->sock < 0) { 
			FD_SET(sock, &fds);
			num = select(sock + 1, &fds, NULL, NULL, &timeout);
		} else {
			FD_SET(ca->sock, &fds);
			num = select(ca->sock + 1, &fds, NULL, NULL, &timeout);
		}
		if (num > 0) {
			if (ca->sock < 0) {
				if (FD_ISSET(sock, &fds)) {
					socklen_t len;
					struct sockaddr cadr;
					
					ca->sock = accept(sock, &cadr, &len);
					if (ca->sock >= 0) {
						set_nonblock(ca->sock);
					}
				}
			} else {
				if (FD_ISSET(ca->sock, &fds)) {
					proc_csock(ca);
				}
			}
		}
		
		pthread_mutex_lock(&ca->mutex);
		if (!ca->state) {
			pthread_mutex_unlock(&ca->mutex);
			continue;
		}
		if (ca->setpmt) {
			dbgprintf(DEBUG_DVB, "got new PMT %08x\n", ca->pmt_new); 
			memcpy(ca->pmt, ca->pmt_new, sizeof(ca->pmt));
			memset(ca->pmt_old, 0, sizeof(ca->pmt_old));
			for (i = 0; i < MAX_PMT; i++)
				ca->ca_pmt_version[i] = -1;
			ca->sentpmt = 0;
			ca->setpmt = 0;
			pmt_count = 0;
			tdt_count = 0;
		}
		pthread_mutex_unlock(&ca->mutex);

		if (!ca->sentpmt)
			handle_pmts(ca);
		else {
			pmt_count++;
			if (pmt_count == 10) {
				//handle_pmts(ca);
				pmt_count = 0;
			}
		}
		tdt_count++;
		if (tdt_count == 10) {
			handle_tdt(ca);
			tdt_count = 0;
		}
	}
}

int set_pmt(struct dvbca *ca, uint32_t *pmt)
{
	dbgprintf(DEBUG_DVB, "set_pmt %08x %08x %08x\n", pmt[0], pmt[1], pmt[2]);
	pthread_mutex_lock(&ca->mutex);
	ca->setpmt = 1;
	memcpy(ca->pmt_new, pmt, sizeof(ca->pmt_new));
	pthread_mutex_unlock(&ca->mutex);
	return 0;
}

static void ci_poll(struct dvbca *ca)
{
	while (!ca->os->exit) {
		ca->stdcam->poll(ca->stdcam);
		
	}
}

static int mmi_close_callback(void *arg, uint8_t slot_id, uint16_t snum,
			      uint8_t cmd_id, uint8_t delay)
{
	struct dvbca *ca = arg;
	
	ca->mmi_state = MMI_STATE_CLOSED;
	return 0;
}

static int mmi_display_control_callback(void *arg, uint8_t slot_id, uint16_t snum,
					uint8_t cmd_id, uint8_t mmi_mode)
{
	struct dvbca *ca = arg;
	struct en50221_app_mmi_display_reply_details reply;

	if (cmd_id != MMI_DISPLAY_CONTROL_CMD_ID_SET_MMI_MODE) {
		en50221_app_mmi_display_reply(ca->stdcam->mmi_resource, snum,
					      MMI_DISPLAY_REPLY_ID_UNKNOWN_CMD_ID, &reply);
		return 0;
	}

	// we only support high level mode
	if (mmi_mode != MMI_MODE_HIGH_LEVEL) {
		en50221_app_mmi_display_reply(ca->stdcam->mmi_resource, snum,
					      MMI_DISPLAY_REPLY_ID_UNKNOWN_MMI_MODE, &reply);
		return 0;
	}

	reply.u.mode_ack.mmi_mode = mmi_mode;
	en50221_app_mmi_display_reply(ca->stdcam->mmi_resource, snum,
				      MMI_DISPLAY_REPLY_ID_MMI_MODE_ACK, &reply);
	ca->mmi_state = MMI_STATE_OPEN;
	return 0;
}

static int mmi_enq_callback(void *arg, uint8_t slot_id, uint16_t snum,
			    uint8_t blind_answer, uint8_t expected_answer_length,
			    uint8_t *text, uint32_t text_size)
{
	struct dvbca *ca = arg;
	
	if (ca->sock >= 0) {
			sendstring(ca->sock, "%.*s: ", text_size, text);
	}
	//mmi_enq_blind = blind_answer;
	//mmi_enq_length = expected_answer_length;
	ca->mmi_state = MMI_STATE_ENQ;
	return 0;
}

static int mmi_menu_callback(void *arg, uint8_t slot_id, uint16_t snum,
			     struct en50221_app_mmi_text *title,
			     struct en50221_app_mmi_text *sub_title,
			     struct en50221_app_mmi_text *bottom,
			     uint32_t item_count, struct en50221_app_mmi_text *items,
			     uint32_t item_raw_length, uint8_t *items_raw)
{
	uint32_t i;
	struct dvbca *ca = arg;

	if (ca->sock >= 0) {
		if (title->text_length) 
			sendstring(ca->sock, "%.*s\n", title->text_length, title->text);
		if (sub_title->text_length) 
			sendstring(ca->sock, "%.*s\n", sub_title->text_length, sub_title->text);
		for (i = 0; i < item_count; i++) 
			sendstring(ca->sock, "%i. %.*s\n", i + 1, items[i].text_length, items[i].text);
		if (bottom->text_length) 
			sendstring(ca->sock, "%.*s\n", bottom->text_length, bottom->text);
	}
	ca->mmi_state = MMI_STATE_MENU;
	return 0;
}

static int init_ca_stack(struct dvbca *ca)
{
	ca->tl = en50221_tl_create(1, 16);
	if (ca->tl == NULL) {
		dbgprintf(DEBUG_DVB, "Failed to create transport layer\n");
		return -1;
	}
	ca->sl = en50221_sl_create(ca->tl, 16);
	if (ca->sl == NULL) {
		dbgprintf(DEBUG_DVB, "Failed to create session layer\n");
		en50221_tl_destroy(ca->tl);
		return -1;
	}

	ca->stdcam = en50221_stdcam_llci_create(ca->fd, 0, ca->tl, ca->sl);
	if (!ca->stdcam) {
		dbgprintf(DEBUG_DVB, "Failed to create stdcam\n");
		en50221_sl_destroy(ca->sl);
		en50221_tl_destroy(ca->tl);
		return -1;
	}
	if (ca->stdcam->ai_resource) {
		en50221_app_ai_register_callback(ca->stdcam->ai_resource, ai_callback, ca);
	}
	if (ca->stdcam->ca_resource) {
		en50221_app_ca_register_info_callback(ca->stdcam->ca_resource, ca_info_callback, ca);
	}
	if (ca->stdcam->mmi_resource) {
		en50221_app_mmi_register_close_callback(ca->stdcam->mmi_resource, mmi_close_callback, ca);
		en50221_app_mmi_register_display_control_callback(ca->stdcam->mmi_resource, 
								  mmi_display_control_callback, ca);
		en50221_app_mmi_register_enq_callback(ca->stdcam->mmi_resource, mmi_enq_callback, ca);
		en50221_app_mmi_register_menu_callback(ca->stdcam->mmi_resource, mmi_menu_callback, ca);
		en50221_app_mmi_register_list_callback(ca->stdcam->mmi_resource, mmi_menu_callback, ca);
	} else {
		dbgprintf(DEBUG_DVB,
			  "CAM Menus are not supported by this interface hardware\n");
	}
	return 0;
}

static int init_ca(struct octoserve *os, int a, int f, int fd)
{
	struct dvbca *ca;

	ca = &os->dvbca[os->dvbca_num];
	ca->os = os;
	ca->anum = a;
	ca->fnum = f;
	ca->nr = os->dvbca_num + 1;
	ca->fd = fd;
	pthread_mutex_init(&ca->mutex, 0);
	
	init_ca_stack(ca);

	pthread_create(&ca->poll_pt, NULL, (void *) ci_poll, ca); 
	pthread_create(&ca->pt, NULL, (void *) handle_ci, ca); 

	os->dvbca_num++;
	return 0;
}

static int scan_dvbca(struct octoserve *os)
{
	int a, f, fd;
	char fname[80];

	for (a = 0; a < 16; a++) {
		for (f = 0; f < 16; f++) {
			sprintf(fname, "/dev/dvb/adapter%d/ca%d", a, f); 
			fd = open(fname, O_RDWR);
			if (fd >= 0) {
				init_ca(os, a, f, fd);
				//close(fd);
			}
		}
	}
	dbgprintf(DEBUG_DVB, "Found %d CA interfaces\n", os->dvbca_num);
}

void scif_config(struct octoserve *os, char *name, char *val)
{
	if (!name || !val)
		return;
	
	if (!strncasecmp(name, "type", 4) &&
	    val[0] >= 0x30 && val[0] <= 0x32) {
		os->scif_type = val[0] - 0x30;
		dbgprintf(DEBUG_DVB, "setting type = %d\n", os->scif_type);
	}
	if (!strncasecmp(name, "tuner", 5) &&
	    name[5] >= 0x31 && name[5] <= 0x39) {
		int fe = strtol(name + 5, NULL, 10 );
		if (fe >= 1 && fe <= MAX_DVB_FE) {
			char *end;
			unsigned long int nr = strtoul(val, &end, 10), freq = 0;

			if (*end == ',') {
				val = end + 1;
				freq = strtoul(val, &end, 10);
				if (val == end)
					return;
			}
			fe--;
			if (nr == 0)
				os->dvbfe[fe].scif_type = 0;
			else {
				os->dvbfe[fe].scif_slot = nr - 1;
				os->dvbfe[fe].scif_freq = freq;
				os->dvbfe[fe].scif_type = os->scif_type;
			}
			dbgprintf(DEBUG_DVB, "fe%d: type=%d, slot=%d, freq=%d\n", fe,
				  os->dvbfe[fe].scif_type,
				  os->dvbfe[fe].scif_slot,
				  os->dvbfe[fe].scif_freq);
		}
	}
}


void set_lnb(struct octoserve *os, int tuner, 
	     uint32_t source, uint32_t lof1, uint32_t lof2, uint32_t lofs) 
{
	int i, j;
	int i1 = 0, i2 = MAX_DVB_FE;
	int j1 = 0, j2 = MAX_SOURCE;
	
	if (tuner > MAX_DVB_FE) 
		return;
	if (source > MAX_SOURCE) 
		return;
	
	if (tuner) {
		i1 = tuner - 1; 
		i2 = i1 + 1;
	}
	if (source) {
		j1 = source - 1;
		j2 = j1 + 1;
	}
	for (i = i1; i < i2; i++) {
		struct dvbfe *fe = &os->dvbfe[i];
		for (j = j1; j < j2; j++) {
			dbgprintf(DEBUG_DVB, "setting %d %d %u %u %u\n", 
				  i, j, lof1, lof2, lofs);
			fe->lof1[j] = lof1; 
			fe->lof2[j] = lof2; 
			fe->lofs[j] = lofs; 
			fe->prev_delay[j] = 250000;
		}
	}
}

void lnb_config(struct octoserve *os, char *name, char *val)
{
	static int lnb = -1;
	static uint32_t lof1, lof2, lofs, tuner, source;
	char *end;

	if (!name || !val) {
		if (lnb >= 0)
			set_lnb(os, tuner, source, 
				lof1 * 1000, lof2 * 1000, lofs * 1000);
		lnb++;
		tuner = source = lof1 = lof2 = lofs = 0;
		return;
	}
	if (!strcasecmp(name, "tuner")) {
		tuner = strtoul(val, &end, 10);
	} else if (!strcasecmp(name, "source")) {
		source = strtoul(val, &end, 10);
	} else if (!strcasecmp(name, "lof1")) {
		lof1 = strtoul(val, &end, 10);
	} else if (!strcasecmp(name, "lof2")) {
		lof2 = strtoul(val, &end, 10);
	} else if (!strcasecmp(name, "lofs")) {
		lofs = strtoul(val, &end, 10);
	}
}

int init_dvb(struct octoserve *os, int nodvbt, int noswitch)
{
	int i, j;

	pthread_mutex_init(&nsd_lock, 0);
	pthread_mutex_init(&os->uni_lock, 0);

	scan_dvbfe(os, nodvbt, noswitch);
	scan_dvbca(os);

	os->scif_type = 0;
	parse_config(os, "scif", &scif_config);

	if (os->has_feswitch) {
		uint32_t fmode = 0;
		
		if (os->do_feswitch) {
			fmode = 1;
			if (os->scif_type)
				fmode = 0;//3;
		}
		set_fmode(fmode);
	}
	set_lnb(os, 0, 0, 9750000, 10600000, 11700000);
	parse_config(os, "LNB", &lnb_config);
}

int release_dvb(struct octoserve *os)
{
	int i;

	for (i = 0; i < os->dvbfe_num; i++) {

	}
	
	for (i = 0; i < os->dvbca_num; i++) {
		struct dvbca *ca = &os->dvbca[i];
		
		pthread_join(ca->poll_pt, NULL);
		pthread_join(ca->pt, NULL);
	}
	pthread_mutex_destroy(&nsd_lock);
	pthread_mutex_destroy(&os->uni_lock);
}
