/*  
    (C) 2012-15 Digital Devices GmbH. 

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

#include <getopt.h>

uint32_t debug;
uint32_t flags;
uint32_t conform = 0;
#define FLAGS_TRANSPORT 1


uint64_t mtime_nano(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts))
		return 0;
	return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

void dump(const uint8_t *b, int l)
{
	int i, j;
	
	for (j = 0; j < l; j += 16, b += 16) { 
		for (i = 0; i < 16; i++)
			if (i + j < l)
				printf("%02x ", b[i]);
			else
				printf("   ");
		printf(" | ");
		for (i = 0; i < 16; i++)
			if (i + j < l)
				putchar((b[i] > 31 && b[i] < 127) ? b[i] : '.');
		printf("\n");
	}
}

static void update_switch_vec(struct ossess *sess)
{
	struct osmcc *mcc;
	uint32_t vec = 0;

	for (mcc = sess->mccs.lh_first; mcc; mcc = mcc->mcc.le_next)
		vec |= mcc->port_vec;
	if (vec != sess->mcc_port_vec_set || sess->port_vec != sess->port_vec_set) {
		sess->mcc_port_vec_set = vec;
		sess->port_vec_set = sess->port_vec;
		switch_set_multicast(sess->trans.mcmac,
				     sess->mcc_port_vec_set | sess->port_vec_set);
	}
}

struct rtsp_error rtsp_errors[] = {
	{100, "Continue"},
	{200, "OK"},
	{400, "Bad Request"},
	{403, "Forbidden"},
	{404, "Not Found"},
	{405, "Method Not Allowed"},
	{406, "Not Acceptable"},
	{408, "Request Timeout"},
	{414, "Request-URI Too Long"},
	{453, "Not Enough Bandwidth"},
	{454, "Session Not Found"},
	{455, "Method Not Valid in This State"},
	{461, "Unsupported Transport"},
	{500, "Internal Server Error"},
	{501, "Not Implemented"},
	{503, "Service Unavailable"},
	{505, "Version Not Supported"},
	{551, "Option Not Supported"},
	{0, ""},
}; 

static int adrtoip(struct sockaddr *sa, uint8_t *ip)
{
	unsigned short af = sa->sa_family;
	void *adr;
	
	if (af == AF_INET) 
		adr = &((struct sockaddr_in *) sa)->sin_addr;
	else if (af == AF_INET6)
		adr = &((struct sockaddr_in6 *) sa)->sin6_addr;
	else 
		return -1;
	memcpy(ip, adr,  af == AF_INET ? 4 : 16);
	return 0;
}

static int check_self(struct sockaddr *sadr, uint8_t *mac)
{
	uint8_t ip[16];

	if (adrtoip(sadr, ip) < 0)
		return -1;
}

static int get_mac(char *ifname, struct sockaddr *sadr, uint8_t *mac)
{
	struct arpreq arpreq;
	int s;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return -1;
	memset(&arpreq, 0, sizeof(struct arpreq));
	memcpy(&arpreq.arp_pa, sadr, sizeof(struct sockaddr));

	arpreq.arp_ha.sa_family = ARPHRD_ETHER;
	strcpy(arpreq.arp_dev, ifname);
	if (ioctl(s, SIOCGARP, &arpreq) < 0) {
		perror("get_mac socket error on");
		dump((uint8_t *)sadr, sizeof(struct sockaddr));
		close(s);
		return -1;
	}
	close(s);
	memcpy(mac, arpreq.arp_ha.sa_data, 6);
	dbgprintf(DEBUG_NET, "%s=%02x:%02x:%02x:%02x:%02x:%02x\n", ifname,  
	       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return 0;
}

static int ip2mac(uint8_t *ip, uint8_t *mac)
{
	struct arpreq arpreq;
	struct sockaddr_in *sin = (struct sockaddr_in *) &arpreq.arp_pa;
	int s;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return -1;
	memset(&arpreq, 0, sizeof(struct arpreq));
	sin->sin_family = AF_INET;
	memcpy(&sin->sin_addr.s_addr, ip, 4);

	if (ioctl(s, SIOCGARP, &arpreq) < 0) {
		perror("ip2mac socket error");
		close(s);
		return -1;
	}
	close(s);
	memcpy(mac, arpreq.arp_ha.sa_data, 6);
	dbgprintf(DEBUG_NET, "%u.%u.%u.%u=%02x:%02x:%02x:%02x:%02x:%02x\n", 
		  ip[0], ip[1], ip[2], ip[3], 
		  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return 0;
}

static void add_fd(int fd, int *mfd, fd_set *fds)
{
	FD_SET(fd, fds);
	if (fd > *mfd)
		*mfd = fd;
}

int readmii(int id, int reg)
{
	int fd;
	struct ifreq ifr;
	struct mii_ioctl_data *mii = (struct mii_ioctl_data *) &ifr.ifr_data;
	
	strcpy(ifr.ifr_name, "eth0");
	mii->phy_id = id;
	mii->reg_num = reg;
	mii->val_in = 0;
	mii->val_out = 0;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ioctl(fd, SIOCGMIIREG, &ifr);
	dbgprintf(DEBUG_NET, "mii %02x.%02x = %02x\n", id, reg, mii->val_out);
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

void send_error(struct oscon *con, int err)
{
	uint8_t buf[256];
	int len;
	char *err_str;
	struct rtsp_error *rerr;

	for (rerr = rtsp_errors; rerr->number; rerr++)
		if (rerr->number == err)
			break;
	if (!rerr->number) {
		printf("Internal Error: invalid error number %d\n", err);
		return;
	}
	len=sprintf(buf, 
		    "RTSP/1.0 %d %s\r\n"
		    "CSeq: %d\r\n"
		    "\r\n", err, rerr->name, con->seq);
	sendlen(con->sock, buf, len);
	dbgprintf(DEBUG_RTSP, "Send Error:\n%s\n", buf);
}

static void *release_session(struct ossess *oss);

static void *release_ca(struct dvbca *ca)
{
	dbgprintf(DEBUG_SYS, "release ca %d\n", ca->nr);
	pthread_mutex_lock(&ca->os->lock);
	ca->stream->ca = NULL;
	ca->state = 0;
	pthread_mutex_unlock(&ca->os->lock);
}

static struct dvbca *alloc_ca_num(struct osstrm *str, int num)
{
	struct octoserve *os = str->os;
	struct dvbca *ca;

	if (num >= os->dvbca_num)
		return NULL;
	pthread_mutex_lock(&os->lock);
	ca = &os->dvbca[num];
	if (ca->state == 0) {
		printf("alloced ca %d\n", num);
		pthread_mutex_lock(&ca->mutex);
		memset(ca->pmt, 0, sizeof(ca->pmt));
		ca->state = 1;
		ca->stream = str;
		ca->input = str->fe->nr - 1;
		pthread_mutex_unlock(&ca->mutex);
		pthread_mutex_unlock(&os->lock);
		return ca;
	}
	pthread_mutex_unlock(&os->lock);
	return NULL;
}

static struct dvbca *alloc_ca(struct osstrm *str)
{
	struct octoserve *os = str->os;
	struct dvbca *ca;
	uint32_t i;

	printf("alloc ca\n");
	pthread_mutex_lock(&os->lock);
	for (i = 0; i < os->dvbca_num; i++) {
		ca = &os->dvbca[i];
		if (ca->state == 0) {
			printf("alloced ca %d\n", i);
			pthread_mutex_lock(&ca->mutex);
			memset(ca->pmt, 0, sizeof(ca->pmt));
			ca->state = 1;
			ca->stream = str;
			ca->input = str->fe->nr - 1;
			pthread_mutex_unlock(&ca->mutex);
			pthread_mutex_unlock(&os->lock);
			return ca;
			
		}
	}
	pthread_mutex_unlock(&os->lock);
	return NULL;
}

static void *release_fe(struct octoserve *os, struct dvbfe *fe)
{
	if (!fe)
		return;
	dbgprintf(DEBUG_SYS, "release fe %d\n", fe->nr);
	fe->state = 2;
	pthread_join(fe->pt, NULL);
}

static struct dvbfe *alloc_fe_num(struct octoserve *os, int i, int type)
{
	struct dvbfe *fe;

	if (i > os->dvbfe_num)
		return NULL;
	dbgprintf(DEBUG_SYS, "alloc_fe_num %d\n", i);
	pthread_mutex_lock(&os->lock);
	fe = &os->dvbfe[i];
	if (fe->state || !(fe->type & (1UL << type))) {
		pthread_mutex_unlock(&os->lock);
		return NULL;
	}
	fe->n_tune = 0;
	fe->state = 1;
	pthread_create(&fe->pt, NULL, (void *) handle_fe, fe); 
	pthread_mutex_unlock(&os->lock);
	dbgprintf(DEBUG_SYS, "Allocated fe %d = %d/%d, fd=%d\n",
		  fe->nr, fe->anum, fe->fnum, fe->fd);
	return fe;
}

static struct dvbfe *alloc_fe(struct octoserve *os, int type)
{
	int i;
	struct dvbfe *fe;

	pthread_mutex_lock(&os->lock);
	for (i = 0; i < os->dvbfe_num; i++) {
		fe = &os->dvbfe[i];
		if (fe->state == 0 && 
		    (fe->type & (1UL << type))) {
			pthread_mutex_unlock(&os->lock);
			return alloc_fe_num(os, i, type);
		}
	}
	pthread_mutex_unlock(&os->lock);
	return NULL;
}

static void release_con(struct oscon *con)
{
	close(con->sock);
	con->state = 0;
	dbgprintf(DEBUG_SYS, "releasing con %d\n", con->nr);
}

static void shutdown_con(struct oscon *con)
{
	con->session = 0;
	con->state = 3;
	con->timeout = mtime(NULL) + 10;
	dbgprintf(DEBUG_SYS, "shutdown con %u at %u\n", con->nr, con->timeout);
}

static void *_release_session(struct ossess *oss)
{
	struct octoserve *os = oss->os;
	int i;

	dbgprintf(DEBUG_SYS, "releasing session %d\n", oss->nr);
	for (i = 0; i < MAX_CONNECT; i++) {
		if (os->con[i].state &&
		    (os->con[i].session == oss))
			shutdown_con(&os->con[i]);
	}
	if (oss->nsfd >= 0) {
		dbgprintf(DEBUG_SYS, "closing netstream of session %d\n", oss->nr);
		close(oss->nsfd);
	}
	oss->playing = 0;
	oss->state = 0;
}

static void *release_stream(struct osstrm *str)
{
	struct octoserve *os = str->os;
	int i;
	dbgprintf(DEBUG_SYS, "release stream %d\n", str->nr);

	for (i = 0; i < MAX_SESSION; i++) {
		if (os->session[i].state && 
		    (os->session[i].stream == str))
			_release_session(&os->session[i]);
	}
	str->state = 0;
	release_fe(os, str->fe);
	if (str->ca)
		release_ca(str->ca);
}

static struct osstrm *alloc_stream(struct octoserve *os)
{
	int i;
	struct osstrm *str;

	pthread_mutex_lock(&os->lock);
	for (i = 0; i < MAX_STREAM; i++) {
		str = &os->stream[i];
		if (str->state == 0) {
			memset(str, 0, sizeof(struct osstrm));
			str->os = os;
			str->nr = i + 1;
			str->state = 1;
			str->sport = 8000 + 2 * i;
			str->sport2 = 8000 + 2 * i + 1;
			pthread_mutex_unlock(&os->lock);
			dbgprintf(DEBUG_SYS, "Allocated stream %d\n", str->nr);
			return str;}
	}
	pthread_mutex_unlock(&os->lock);
	return NULL;
}

static struct osstrm *get_stream(struct octoserve *os, int id)
{
	int i;
	struct osstrm *str, *r = NULL;

	pthread_mutex_lock(&os->lock);
	for (i = 0; i < MAX_STREAM; i++) {
		str = &os->stream[i];
		if (str->state && (str->nr == id)) {
			r = str;
			break;
		}
	}
	pthread_mutex_unlock(&os->lock);
	return r;
}

static void *release_session(struct ossess *oss)
{
	struct octoserve *os = oss->os;
	int i;
	
	dbgprintf(DEBUG_SYS, "release session nr %d id %010d\n", oss->nr, oss->id);
	pthread_mutex_lock(&os->lock);
	mc_del(oss);
	if (oss->stream && (oss->stream->session == oss)) // stream owner?
		release_stream(oss->stream);
	else 
		_release_session(oss);
	pthread_mutex_unlock(&os->lock);
}

static struct ossess *get_session(struct octoserve *os, uint32_t id)
{
	int i;
	struct ossess *oss;
	
	pthread_mutex_lock(&os->lock);
	for (i = 0; i < MAX_SESSION; i++) {
		oss = &os->session[i];
		if (oss->state && (oss->id == id)) {
			pthread_mutex_unlock(&os->lock);
			return oss;
		}
	}
	pthread_mutex_unlock(&os->lock);
	return NULL;
}

static void check_mccs(struct ossess *sess)
{
	time_t tdiff, t;
	int update = 0;
	
	if (!sess->trans.mcast)
		return;

	tdiff = mtime(&t) - sess->mcc_time;
	switch (sess->mcc_state) {
	case 1:
		if (tdiff >= 1) {
			sess->mcc_state = 2;
			/* all replying clients get new tag */ 
			sess->mcc_tag++;
			send_igmp_query(sess->os, sess->trans.mcip, 10);
			sess->mcc_time = t;
			printf("%u: mcc_state 1 done, tag = %d\n", t, sess->mcc_tag);
		}
		break;
	case 2:
		if (tdiff >= 1) {
			sess->mcc_state = 3;
			send_igmp_query(sess->os, sess->trans.mcip, 10);
			printf("%u: mcc_state 2 done, tag = %d\n", t, sess->mcc_tag);
		}
		break;
	case 3:
		if (tdiff >= 2) {
			sess->mcc_state = 0;
			sess->mcc_time = t;
			printf("%u: mcc_state 2 done, tag = %d\n", t, sess->mcc_tag);
			update = 1;
		}
		break;
	case 0:
		update = 1;
		break;
	}
	mc_check(sess, update);
}

void session_timeout(struct ossess *sess)
{
	mtime(&sess->timeout);	
	sess->timeout += sess->timeout_len;
	dbgprintf(DEBUG_RTSP, "new timeout %d\n", sess->timeout);
}

void check_session_timeouts(struct octoserve *os)
{
	time_t t;
	int i;
	struct ossess *sess;

	mtime(&t);
	pthread_mutex_lock(&os->lock);
	for (i = 0; i < MAX_SESSION; i++) {
		sess = &os->session[i];
		check_mccs(sess);

		if (sess->state &&
		    (sess->timeout < t)) {
			struct oscon *con;
			int j;
	
			/* also count open connections referencing this session, 
			   for VLC, mplayer, ... which do not send keep alives */
			for (j = 0; j < MAX_CONNECT; j++) {
				con = &os->con[j];
				if (con->state && con->session && 
				    (con->session == sess)) {
					session_timeout(sess);
					break;
				}
			}
			if (j == MAX_CONNECT)
				release_session(&os->session[i]);
		}
	}
	pthread_mutex_unlock(&os->lock);
}

static uint32_t get_id(struct octoserve *os)
{
	uint32_t id, i;
	struct ossess *oss;

	while (1) {
		id = random();
		for (i = 0; i < MAX_SESSION; i++) {
			oss = &os->session[i];
			if (oss->state && (oss->id == id))
				break;
		}
		if (i == MAX_SESSION)
			return id;
	}
}

static struct ossess *alloc_session(struct octoserve *os)
{
	int i;
	struct ossess *oss;

	pthread_mutex_lock(&os->lock);
	for (i = 0; i < MAX_SESSION; i++) {
		oss = &os->session[i];
		if (oss->state == 0) {
			memset(oss, 0, sizeof(struct ossess));
			oss->os = os;
			oss->nr = i;
			oss->id = get_id(os);
			os->sessionid++;
			oss->state = 1;
			oss->nsfd = -1;
			oss->timeout_len = 60;
			session_timeout(oss);
			LIST_INIT(&oss->mccs);
			mtime(&oss->mcc_time);
			dbgprintf(DEBUG_SYS,
				  "Allocated session nr=%d id=%d\n",
				  oss->nr, oss->id);
			pthread_mutex_unlock(&os->lock);
			return oss;
		}
	}
	pthread_mutex_unlock(&os->lock);
	return NULL;
}

static struct oscon *alloc_con(struct octoserve *os)
{
	int i;
	struct oscon *con;

	for (i = 0; i < MAX_CONNECT; i++) {
		con = &os->con[i];
		if (con->state == 0) {
			memset(con, 0, sizeof(struct oscon));
			con->os = os;
			con->nr = i;
			con->state = 1;
			return con;
		}
	}
	return NULL;
}

int die(char *msg)
{
	printf("%s\n", msg);
	exit(-1);
}

void sigchld_handler(int s)
{
	while (wait(NULL) > 0);
}

static void send_option(struct oscon *con)
{
	uint8_t buf[256], opt[256] ={ 0 };
	int len;

	if (con->session) 
		snprintf(opt, sizeof(opt), "Session: %010d\r\n", con->session->id);
	
	len = snprintf(buf, sizeof(buf),
		       "RTSP/1.0 200 OK\r\n" 
		       "CSeq: %d\r\n"
		       "Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN\r\n"
		       "%s"
		       "\r\n", 
		       con->seq, opt);
	if ((len > 0) && (len < sizeof(buf))) {
		sendlen(con->sock, buf, len);
		dbgprintf(DEBUG_RTSP, "Send: %s\n", buf);
	}
}

char *pol2str[] = {"", "v", "h", "r", "l", NULL};
char *msys2str [] = {"", "undef", "dvbc", "dvbcb", "dvbt", "dss", "dvbs", "dvbs2", "dvbh",
		      "isdbt", "isdbs", "isdbc", "atsc", "atscmh", "dtmb", "cmmb", "dab",
		      "dvbt2", "turbo", "dvbcc", "dvbc2", NULL};
char *mtype2str [] = {"", "qpsk", "16qam", "32qam",
		      "64qam", "128qam", "256qam", 
		      "autoqam", "8vsb", "16vsb", "8psk",
		      "16apsk", "32apsk", "dqpsk", "4qamnr", NULL};
char *pilot2str [] = {"", "on", "off", "auto", NULL};
char *roll2str [] = {"", "0.35", "0.20", "0.25", NULL};
char *fec2str [] = {"", "none", "12", "23", "34", "56", "78", "89", "35", "45", "910", "25", NULL};
char *bw2str [] = {"", "8", "7", "6", "auto", "5", "10", "1.712", NULL };
char *tmode2str [] = { "", "2k", "8k", "auto", "4k", "1k", "16k", "32k", "c1", "c3780", NULL};
char *gi2str [] = { "", "132", "116", "18", "14", "auto", "1128", "19128", "19256", "pn420", "pn595", "pn945", NULL};
char *num2str [] = {"", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", NULL};

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

static int session_string(struct ossess *sess, char *msg, int mlen)
{
	struct dvbfe *fe;
	char pids[512];
	struct dvb_params *p;
	int pid, len, len2, plen;
	int level = 0, lock = 0, quality = 0, tuner = 0;
	int all;
	struct osstrm *str = sess->stream;
	
	if (!sess->state || !str)
		return -1;
	if (str->fe){
		level = str->fe->level;
		lock = str->fe->lock;
		quality = str->fe->quality;
		tuner = str->fe->nr;
	}
	p = &sess->p;

	for (pid = 0, all = 1; pid < 1024; pid++) {
		if (p->pid[pid] != 0xff) {
			all = 0;
			break;
		}
	}
	if (all) 
		snprintf(pids, sizeof(pids), "=all");
	else {
		for (pid = 0, plen = 0; pid < 8192; pid++) {
			if (p->pid[pid >> 3] & (1 << (pid & 7))) {
				len2 = snprintf(pids + plen, sizeof(pids) - plen,
						",%u", pid);
				if (len2 < 0) 
					return -1;
				if (plen + len2 >= 200)
					break;
				plen += len2;
			}
		}
		pids[0] = '=';
		if (!plen) 
			snprintf(pids, sizeof(pids), "=none");
	}
	
	switch(p->param[PARAM_MSYS] - 1) {
	case SYS_DVBS:
	case SYS_DVBS2:
		len = snprintf(msg, mlen, 
			       "ver=1.0;src=%u;tuner=%u,%u,%u,%u,%u,%s,%s,%s,%s,%s,%u,%s;pids%s", 
			       p->param[PARAM_SRC], tuner, 
			       level, lock, quality,
			       p->param[PARAM_FREQ] / 1000,
			       pol2str[p->param[PARAM_POL]],
			       msys2str[p->param[PARAM_MSYS]],
			       mtype2str[p->param[PARAM_MTYPE]],
			       pilot2str[p->param[PARAM_PLTS]],
			       roll2str[p->param[PARAM_RO]],
			       p->param[PARAM_SR],
			       fec2str[p->param[PARAM_FEC]],
			       pids);
		break;
	case SYS_DVBT:
	case SYS_DVBT2:
		len = snprintf(msg, mlen, 
			       "ver=1.1;tuner=%u,%u,%u,%u,%u.%03u,%s,%s,%s,%s,%s,%s,%u,%u,%u;pids%s", 
			       tuner, level, lock, quality,
			       p->param[PARAM_FREQ] / 1000,
			       p->param[PARAM_FREQ] % 1000,
			       bw2str[p->param[PARAM_BW]],
			       msys2str[p->param[PARAM_MSYS]],
			       tmode2str[p->param[PARAM_TMODE]],
			       mtype2str[p->param[PARAM_MTYPE]],
			       gi2str[p->param[PARAM_GI]],
			       fec2str[p->param[PARAM_FEC]],
			       p->param[PARAM_PLP],
			       p->param[PARAM_T2ID],
			       p->param[PARAM_SM],
			       pids);
		break;
	case SYS_DVBC_ANNEX_A:
	case (SYS_DVBC_ANNEX_C + 1):
#if 0
		len = snprintf(msg, mlen, 
			       "ver=0.9;tuner=%u,%u,%u,%u,%u.%03u,%u,%s,%s;pids%s", 
			       tuner, level, lock, quality,
			       p->param[PARAM_FREQ] / 1000,
			       p->param[PARAM_FREQ] % 1000,
			       p->param[PARAM_SR],
			       msys2str[p->param[PARAM_MSYS]],
			       mtype2str[p->param[PARAM_MTYPE]],
			       pids);
#else
		len = snprintf(msg, mlen, 
			       "ver=1.2;tuner=%u,%u,%u,%u,%u.%03u,%u,%s,%s,%u,%u,%u,%u;pids%s", 
			       tuner, level, lock, quality,
			       p->param[PARAM_FREQ] / 1000,
			       p->param[PARAM_FREQ] % 1000,
			       p->param[PARAM_BW_HZ] / 1000000,
			       msys2str[p->param[PARAM_MSYS]],
			       mtype2str[p->param[PARAM_MTYPE]],
			       p->param[PARAM_SR],
			       p->param[PARAM_C2TFT],
			       p->param[PARAM_DS],
			       p->param[PARAM_PLP],
			       pids);
#endif
		break;
	}

	if (len >= mlen)
		return -1;
	return len;
}

static int send_describe(struct oscon *con, int only)
{
	struct osstrm *str;
	struct ostrans *t;
	uint8_t buf[4096], buf2[4096 + 1024], buf3[1024]; 
	int len, len2, i;
	int start = 0, end = MAX_STREAM;
	char *p;

	p = con->url;
	while (*p && *p != ' ')
		p++;
	*p = 0;

#if 1
	for (i = 0; i < MAX_STREAM; i++) {
		str = &con->os->stream[i];
		if (str->state)
			break;
	}
	if (i == MAX_STREAM) 
		return -404;
#endif
	if (only >= 0) {
		if (only >= MAX_STREAM)
			return -404;
		start = only - 1;
		end = only;
		str = &con->os->stream[start];
		if (!str->state)
			return -404;
	}
	if (con->os->dvbtnum + con->os->dvbt2num + con->os->dvbcnum + con->os->dvbc2num)
		len = snprintf(buf, sizeof(buf),
			       "v=0\r\n"
			       "o=- 5678901234 7890123456 IN %s %s\r\n"
			       "s=SatIPServer:1 %d,%d,%d\r\n"
			       "t=0 0\r\n",
			       con->trans.family == AF_INET ? "IP4" : "IP6", 
			       con->sadr_ip, 
			       con->os->dvbs2num,
			       con->os->dvbtnum + con->os->dvbt2num,
			       con->os->dvbcnum + con->os->dvbc2num		
			);
	else
		len = snprintf(buf, sizeof(buf),
			       "v=0\r\n"
			       "o=- 5678901234 7890123456 IN %s %s\r\n"
			       "s=SatIPServer:1 %d\r\n"
			       "t=0 0\r\n",
			       con->trans.family == AF_INET ? "IP4" : "IP6", 
			       con->sadr_ip, 
			       con->os->dvbs2num		
			);
		
	if (len <= 0 || len >= sizeof(buf))
		return -500;
	for (i = start; i < end; i++) {
		char *adr = "0.0.0.0", abuf[32];
		int j, sendonly = 0;
		int alen;

		str = &con->os->stream[i];
		if (!str->state)
			continue;
		t = &str->session->trans;

		for (j = sendonly = 0; j < MAX_SESSION; j++) {
			if (con->os->session[j].state && 
			    con->os->session[j].stream == str &&
			    con->os->session[j].playing)
				sendonly = 1;
		}
		session_string(str->session, buf3, sizeof(buf3));
		
		if (t->mcast) {
			alen = snprintf(abuf, sizeof(abuf), 
					"%d.%d.%d.%d/%u",
					t->mcip[0],
					t->mcip[1],
					t->mcip[2],
					t->mcip[3],
					t->ttl);
			if (alen < 0)
				return -500;
			adr = abuf;
		}
		
		len2 = snprintf(buf + len, sizeof(buf) - len,
				"m=video %d %s 33\r\n"
				"c=IN %s %s\r\n"
				"a=control:stream=%d\r\n"
				"a=fmtp:33 %s\r\n"
				"a=%s\r\n",
				t->mcast ? t->cport : 0,
				t->rtp ? "RTP/AVP" : "UDP",
				t->family == AF_INET ? "IP4" : "IP6",
				adr,
				str->nr,
				buf3,
				sendonly ? "sendonly" : "inactive"
			);
		if (len2 <= 0 || len2 >= sizeof(buf) - len)
			return -500;
		len += len2;
	}
	len2 = sprintf(buf2, "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
		       "Content-Type: application/sdp\r\n"
		       "Content-Base: rtsp://%s/\r\n"
		       "Content-Length: %d\r\n"
		       "\r\n", 
		       con->seq, con->sadr_ip, len);

	/* URGH, some receivers (cough Kathr** cough) cannot handle
           split RTSP messages */
#if 0
	sendlen(con->sock, buf2, len2);
	sendlen(con->sock, buf, len);
#else
	memcpy(buf2 + len2, buf, len);
	sendlen(con->sock, buf2, len + len2);
	//dump(buf2, len + len2);
#endif
	dbgprintf(DEBUG_RTSP, "Send:\n%s", buf2);
	dbgprintf(DEBUG_RTSP, "%s\n", buf);
	return 0;
}

static void send_describe2(struct oscon *con, char *url)
{
	uint8_t buf[1024], buf2[1024], *p;
	int len, len2;

	//while (*url && *url != '?')
	//url++;
	p = url;
	while (*p && *p != ' ')
		p++;
	*p = 0;

	len = snprintf(buf, sizeof(buf),
		       "v=0\r\n"
		       "o=- 9876543210 1 IN IP4 %s\r\n"
		       "s=MPEG TS\r\n"
		       "t=0 0\r\n"
		       "m=video 0 RTP/AVP 33\r\n"
		       "c=IN IP4 0.0.0.0\r\n"
		       "a=control:%s\r\n", 
		       con->sadr_ip,
		       url
		);
	if (len <= 0 || len >= sizeof(buf))
		return;
	len2=sprintf(buf2, "RTSP/1.0 200 OK\r\nCSeq: %d\r\nContent-Type: application/sdp\r\n"
		     "Content-Length: %d\r\n\r\n", con->seq, len);

	sendlen(con->sock, buf2, len2);
	sendlen(con->sock, buf, len);
	dbgprintf(DEBUG_RTSP, "Send:\n%s", buf2);
	dbgprintf(DEBUG_RTSP, "%s\n", buf);
}

#define MAX_PID 32

static int getsparam(char *b, char *name, char *q[], struct dvb_params *p, int type)
{
	int s, l = strlen(name), m;

	if (strncasecmp(b, name, l))
		return 0;
	b += l;
	if (b[0] == '&' || b[0] == ' ' || !b[m])
		return l;
	for (s = 1; q[s]; s++) {
		m = strlen(q[s]);
		if (!strncasecmp(b, q[s], m) &&
		    (b[m] == '&' || b[m] == ' ' || !b[m])) {
			p->param[type] = s; 
			p->set |= (1UL << type);
			dbgprintf(DEBUG_SYS, "%s%d(%s), ", name, p->param[type], q[s]);
			return l + m;
		}
	}
	return -1;
}

static int parse_url(struct oscon *con, int streamonly)
{
	struct dvb_params *p = &con->p;
	char *url, *end;
	struct ossess *oss;
	uint32_t pid;
	int s, l;
	
	dbgprintf(DEBUG_SYS, "parsing URL : %s\n", con->url);
	memset(p, 0, sizeof(struct dvb_params));

	for (url = con->url; *url == ' '; url++);
	if (strncasecmp(url, "rtsp://", 7))
		return -1;
	for (url += 7; *url != '/'; url++)
		if (*url == '\0')
			return 0;
	url++;
	if (*url == '\0' || *url == ' ')
		return 0;
	if (*url != '?') {
		if (strncasecmp(url, "stream=", 7))
			return -1;
		url += 7;
		errno = 0;
		p->param[PARAM_STREAMID] = strtoul(url, &end, 10);
		if (errno)
			return -1;
		p->set |= (1UL << PARAM_STREAMID);
		dbgprintf(DEBUG_SYS, "streamid = %d, ", p->param[PARAM_STREAMID]);
		url = end;
	} 
	if (*url != '\0' && *url != ' ' && *url != '?')
		return -1;
	if (!streamonly && *url == '?') {
		do {
			url++;	
			if (!strncasecmp(url, "src=", 4)) {
				url += 4;
				errno = 0;
				p->param[PARAM_SRC] = strtoul(url, &end, 10);
				if (url == end)
					return -1;
				p->set |= (1UL << PARAM_SRC);
				dbgprintf(DEBUG_SYS, "src=%d, ", p->param[PARAM_SRC]);
			} else if (!strncasecmp(url, "fe=", 3)) {
				url += 3;
				p->param[PARAM_FE] = strtoul(url, &end, 10);
				if (end == url)
					break;
				p->set |= (1UL << PARAM_FE);
				dbgprintf(DEBUG_SYS, "fe=%d, ", p->param[PARAM_FE]);
			} else if (!strncasecmp(url, "c2tft=", 6)) {
				url += 6;
				p->param[PARAM_C2TFT] = strtoul(url, &end, 10);
				if (end == url)
					break;
				p->set |= (1UL << PARAM_C2TFT);
				dbgprintf(DEBUG_SYS, "c2tft=%d, ", p->param[PARAM_C2TFT]);
			} else if (!strncasecmp(url, "ds=", 3)) {
				url += 3;
				p->param[PARAM_DS] = strtoul(url, &end, 10);
				if (end == url)
					break;
				p->set |= (1UL << PARAM_DS);
				dbgprintf(DEBUG_SYS, "ds=%d, ", p->param[PARAM_C2TFT]);
			} else if (!strncasecmp(url, "plp=", 4)) {
				url += 4;
				p->param[PARAM_PLP] = strtoul(url, &end, 10);
				if (end == url)
					break;
				p->set |= (1UL << PARAM_PLP);
				dbgprintf(DEBUG_SYS, "plp=%d, ", p->param[PARAM_PLP]);
			} else if (!strncasecmp(url, "specinv=", 8)) {
				url += 8;
				p->param[PARAM_SPECINV] = strtoul(url, &end, 10);
				if (end == url)
					break;
				p->set |= (1UL << PARAM_SPECINV);
				dbgprintf(DEBUG_SYS, "plp=%d, ", p->param[PARAM_PLP]);
			} else if (!strncasecmp(url, "sm=", 3)) {
				url += 3;
				p->param[PARAM_SM] = strtoul(url, &end, 10);
				if (end == url)
					break;
				p->set |= (1UL << PARAM_SM);
				dbgprintf(DEBUG_SYS, "sm=%d, ", p->param[PARAM_SM]);
			} else if (!strncasecmp(url, "t2id=", 5)) {
				url += 5;
				p->param[PARAM_T2ID] = strtoul(url, &end, 10);
				if (end == url)
					break;
				p->set |= (1UL << PARAM_T2ID);
				dbgprintf(DEBUG_SYS, "t2id=%d, ", p->param[PARAM_T2ID]);
			} else if (!strncasecmp(url, "isi=", 4)) {
				url += 4;
				p->param[PARAM_ISI] = strtoul(url, &end, 10);
				if (end == url)
					break;
				p->set |= (1UL << PARAM_ISI);
				dbgprintf(DEBUG_SYS, "isi=%d, ", p->param[PARAM_ISI]);
			} else if (!strncasecmp(url, "freq=", 5)) {
				float f;

				url += 5;
				f = strtof(url, &end);
				p->param[PARAM_FREQ] = f * 1000.0;
				if (url == end)
					return -1;
				p->set |= (1UL << PARAM_FREQ);
				dbgprintf(DEBUG_SYS, "freq=%u.%03u, ", 
					  p->param[PARAM_FREQ] / 1000,
					  p->param[PARAM_FREQ] % 1000);
			} else if ((l = getsparam(url, "pol=", pol2str, p, PARAM_POL))) {
				if (l < 0)
					return l;
				end = url + l;
			} else if ((l = getsparam(url, "msys=", msys2str, p, PARAM_MSYS))) {
				if (l < 0)
					return l;
				end = url + l;
			} else if ((l = getsparam(url, "ro=", roll2str, p, PARAM_RO))) {
				if (l < 0)
					return l;
				end = url + l;
			} else if ((l = getsparam(url, "mtype=", mtype2str, p, PARAM_MTYPE))) {
				if (l < 0)
					return l;
				end = url + l;
			} else if ((l = getsparam(url, "gi=", gi2str, p, PARAM_GI))) {
				if (l < 0)
					return l;
				end = url + l;
			} else if ((l = getsparam(url, "tmode=", tmode2str, p, PARAM_TMODE))) {
				if (l < 0)
					return l;
				end = url + l;
			} else if ((l = getsparam(url, "plts=", pilot2str, p, PARAM_PLTS))) {
				if (l < 0)
					return l;
				end = url + l;
			} else if (!strncasecmp(url, "sr=", 3)) {
				url += 3;
				p->param[PARAM_SR] = strtoul(url, &end, 10);
				p->set |= (1UL << PARAM_SR);
				dbgprintf(DEBUG_SYS, "sr=%d, ", p->param[PARAM_SR]);
#if 0 
			} else if ((l = getsparam(url, "bw=", bw2str, p, PARAM_BW))) {
				if (l < 0)
					return l;
				end = url + l;
#else
			} else if (!strncasecmp(url, "bw=", 3)) {
				float f;

				url += 3;
				f = strtof(url, &end);
				p->param[PARAM_BW_HZ] = f * 1000000.0;
				if (f == 5.0)
					p->param[PARAM_BW] = BANDWIDTH_5_MHZ;
				else if (f == 6.0)
					p->param[PARAM_BW] = BANDWIDTH_6_MHZ;
				else if (f == 7.0)
					p->param[PARAM_BW] = BANDWIDTH_7_MHZ;
				else if (f == 8.0)
					p->param[PARAM_BW] = BANDWIDTH_8_MHZ;
				else if (f == 10.0)
					p->param[PARAM_BW] = BANDWIDTH_10_MHZ;
				else if (f == 1.712)
					p->param[PARAM_BW] = BANDWIDTH_1_712_MHZ;
				else
					return -1;
				p->param[PARAM_BW]++;
				if (url == end)
					return -1;
				p->set |= (1UL << PARAM_BW);
				p->set |= (1UL << PARAM_BW_HZ);
#endif
			} else if ((l = getsparam(url, "fec=", fec2str, p, PARAM_FEC))) {
				if (l < 0)
					return l;
				end = url + l;
			} else if (!strncasecmp(url, "pids=", 5)) {
				if (p->set & ((1UL << PARAM_APID) | (1UL << PARAM_DPID)))
					return -1;
				url += 5;
				memset(p->pid, 0, 0x400);
				do {
					pid = strtoul(url, &end, 10);
					if (url == end) {
						if (!strncasecmp(url, "all", 3)) {
							memset(p->pid, 0xff, 0x400);
							end = url + 3;
						} else if (!strncasecmp(url, "none", 4)) {
							memset(p->pid, 0x00, 0x400);
							end = url + 4;
						} else
							return -1;
					} else {
						if (pid > 8191)
							return -1;
						p->pid[pid >> 3] |= (1 << (pid & 7));
					}
					url = end;
				} while (*(url++) == ',');
				p->set |= (1UL << PARAM_PID);
			} else if (!strncasecmp(url, "addpids=", 8)) {
				if (p->set & (1UL << PARAM_PID))
					return -1;
				url += 8;
				do {
					pid = strtoul(url, &end, 10);
					if (url == end)
						return -1;
					url = end;
					p->pid[pid >> 3] |= (1 << (pid & 7));
					dbgprintf(DEBUG_SYS, "add PID=%d, ", pid);
				} while (*(url++) == ',');
				p->set |= (1UL << PARAM_APID);
			} else if (!strncasecmp(url, "delpids=", 8)) {
				if (p->set & (1UL << PARAM_PID))
					return -1;
				url += 8;
				do {
					pid = strtoul(url, &end, 10);
					if (url == end)
						return -1;
					url = end;
					p->dpid[pid >> 3] |= (1 << (pid & 7));
					dbgprintf(DEBUG_SYS, "del PID=%d, ", pid);
				} while (*(url++) == ',');
				p->set |= (1UL << PARAM_DPID);
			} else if (!strncasecmp(url, "x_pmt=", 6)) {
				uint32_t pmt, sid, num = 0;
				
				url += 6;
				do {
					pmt = strtoul(url, &end, 10);
					if (url == end || pmt >= 8192) 
						return -1;
					if (*end == '.') {
						url = end + 1;
						sid = strtoul(url, &end, 10);
						if (url == end) 
							return -1;
						pmt |= (sid << 16);
					}
					if (num < MAX_PMT)
						p->pmt[num++] = pmt;
					dbgprintf(DEBUG_SYS, "+PMT=%08x, ", pmt);
					url = end;
				} while (*(url++) == ',');
				p->set |= (1UL << PARAM_PMT);
			} else if (!strncasecmp(url, "x_ci=", 5)) {
				url += 5;
				p->param[PARAM_CI] = strtoul(url, &end, 10);
				if (url == end)
					return -1;
				p->set |= (1UL << PARAM_CI);
				dbgprintf(DEBUG_SYS, "ci=%d, ", p->param[PARAM_CI]);
			} else {
				dbgprintf(DEBUG_SYS, "unknown parameter %s\n", url);
				while (*url != '&' && *url && *url != ' ')
					url++;
				end = url;
				/* ignore unknown parameters: return -1; */
			}
			url = end;
		} while (*url == '&');
	}
	while (*url == '/')
		url++;
	if (*url != ' ') 
		return -1;
	while (*url == ' ')
		url++;
	if (strncasecmp(url, "RTSP/1.0", 8))
		return -1;
	dbgprintf(DEBUG_SYS, "\n");
	return 0;
}

static int set_rtcp_msg(struct ossess *sess)
{
	char rtcp[512 - 96 - 102];
	int len;
	struct dvb_ns_rtcp rtcpm = { .msg = rtcp};

	if (!sess->state)
		return -1;
	len = session_string(sess, rtcp, sizeof(rtcp));
	if (len <= 0)
		return -1;
	rtcpm.len = len;
	dbgprintf(DEBUG_RTSP, "%s\n", rtcp);
	if (sess->nsfd < 0)
		return -1;
	return ioctl(sess->nsfd, NS_SET_RTCP_MSG, &rtcpm);
}

static int set_rtcp_msgs(struct osstrm *str)
{
	char rtcp[512 - 96 - 102];
	int len;
	struct dvb_ns_rtcp rtcpm = { .msg = rtcp};
	int i;
	struct ossess *sess;
	
	if (!str->state)
		return -1;
	for (i = 0; i < MAX_SESSION; i++) {
		sess = &str->os->session[i];
		if (!sess->state)
			continue;
		len = session_string(sess, rtcp, sizeof(rtcp));
		if (len <= 0)
			continue;
		rtcpm.len = len;
		dbgprintf(DEBUG_RTSP, "%s\n", rtcp);
		
		if (sess->stream && (sess->stream == str) && (sess->nsfd > 0))
			ioctl(sess->nsfd, NS_SET_RTCP_MSG, &rtcpm);
	}
	return 0;
}

static int set_ns(struct ossess *sess, struct dvb_ns_params *nsp)
{
	struct osstrm *str = sess->stream; 
	int fd = sess->nsfd;
	uint8_t *pids = sess->p.pid;
	
	if (fd < 0)
		return 0;
	dbgprintf(DEBUG_SYS, "%s\n", __FUNCTION__);
	ioctl(fd, NS_SET_NET, nsp);
	set_rtcp_msg(sess);
	ioctl(fd, NS_SET_PIDS, &pids);
	return 0;
}

static int get_ns(struct ossess *sess)
{
	struct dvbfe *fe = sess->stream->fe;
	char fname[80];

	sprintf(fname, "/dev/dvb/adapter%d/ns%d", fe->anum, fe->fnum); 
	sess->nsfd = open(fname, O_RDWR);
	if (sess->nsfd < 0 && errno != 2) {
		dbgprintf(DEBUG_SYS, "no ns free\n");
		return -1;
	}
	dbgprintf(DEBUG_SYS, "got fd %d, for %s\n", sess->nsfd, fname);
	return 0;
}

static int setup_nsp(struct ostrans *trans, struct dvb_ns_params *nsp)
{
	void *adr;
	
	memset(nsp, 0, sizeof(struct dvb_ns_params));
	memcpy(nsp->dmac, trans->cmac, 6);
	memcpy(nsp->smac, trans->smac, 6);
	nsp->smac[5] |= 1;
	nsp->sport = trans->sport;
	nsp->sport2 = trans->sport2;
	nsp->dport = trans->cport;
	nsp->dport2 = trans->cport2;
	if (trans->rtp) {
		nsp->flags = DVB_NS_RTP | DVB_NS_RTCP;
		if (!(trans->flags & TRANS_NO_RTP_TO))
			nsp->flags |= DVB_NS_RTP_TO;
	}
	nsp->qos = 5;
	nsp->vlan = 0;
	nsp->ttl = 64;
	dbgprintf(DEBUG_SYS, "ports: %d-%d %d-%d\n", 
		  nsp->sport, nsp->sport2, nsp->dport, nsp->dport2);

	memcpy(nsp->sip, trans->sip, 16);
	memcpy(nsp->dip, trans->cip, 16);

	if (trans->mcast) {
		uint8_t *mac = adr;

		memcpy(nsp->dmac, trans->mcmac, 6);
		memcpy(nsp->dip, trans->mcip, 16);

		nsp->ttl = trans->ttl ? trans->ttl : 2;

		//dump(nsp->dmac, 6);
		//dump(nsp->dip, 16);
		dbgprintf(DEBUG_SYS, "MC STREAM\n");
	}
	memcpy(nsp->ssrc, trans->ssrc, 4);
	return 0;
}

static int merge_pids(struct dvb_params *op, struct dvb_params *p)
{
	int i, r = 0;

	if (p->set & (1UL << PARAM_PID)) 
		for (i = 0 ; i < 1024 ; i++)
			if (op->pid[i] != p->pid[i]) {
				r |= 1;
				op->pid[i] = p->pid[i];
			}
	if (p->set & (1UL << PARAM_APID))
		for (i = 0 ; i < 1024 ; i++)
			if (!(op->pid[i] & p->pid[i])) {
				r |= 2;
				op->pid[i] |= p->pid[i];
			}
	if (p->set & (1UL << PARAM_DPID)) 
		for (i = 0 ; i < 1024 ; i++)
			if (op->pid[i] & p->dpid[i]) {
				r |= 4;
				op->pid[i] &= ~p->dpid[i];
			}
	return r;
}

static int merge_params(struct dvb_params *op, struct dvb_params *p)
{
	int i;

	if (p->set & 0xfff8) { 
		for (i = PARAM_FE ; i < PARAM_PID ; i++)
			if (p->set & (1UL << i)) {
				op->param[i] = p->param[i];
				dbgprintf(DEBUG_SYS, "para %d = %d\n", i, p->param[i]);
				op->set |= (1UL << i);
			}
	}
}

static void copy_params(struct ossess *s, struct ossess *t)
{
	memcpy(&s->p, &t->p, sizeof(struct dvb_params));
}

static int setup_session(struct oscon *con, int newtrans)
{
	struct dvbfe *fe;
	struct dvb_ns_params nsp;
	struct ossess *sess = con->session;
	struct dvb_params *p = &con->p;
	struct dvb_params *sp = &sess->p;
	struct ostrans *trans = &con->trans;
	struct osstrm *str = sess->stream;
	struct ossess *ownsess = str->session;
	int pidchange;
	int owner = 0;

	if (sess == ownsess)
		owner = 1;
	if (!str)
		return -500;
	pidchange = merge_pids(sp, p);
	
	if (owner) { /* stream owner */
		merge_params(sp, p);
		
		if (str->fe && (sp->set & (1UL << PARAM_FE)) &&
		    (sp->param[PARAM_FE] != str->fe->nr)) {
			release_fe(con->os, str->fe);
			if (sess->nsfd >= 0)
				close(sess->nsfd);
		}
		if (!str->fe) {
			if (!(sp->set & (1UL << PARAM_MSYS)))
				return 0;
			if (sp->set & (1UL << PARAM_FE))
				fe = alloc_fe_num(sess->os, 
						  sp->param[PARAM_FE] - 1,
						  sp->param[PARAM_MSYS] - 1);
			else
				fe = alloc_fe(sess->os, 
					      sp->param[PARAM_MSYS] - 1);
			if (!fe) {
				dbgprintf(DEBUG_SYS, "no fe\n");
				return -404;
			}
			str->fe = fe;
			dvb_tune(str->fe, sp);
		} else if (p->set & 0xffff8) { 
			dvb_tune(str->fe, sp);
		}
	} else {
		if (pidchange && conform) {
			copy_params(sess, ownsess);
			return -455;
		}
	}
	if (sess->nsfd < 0) {
#ifndef IGNORE_NS
		if (get_ns(sess) < 0)
			return -455;
#endif
		newtrans = 1;
	} 
 	if (newtrans) {
		if (str->session != sess &&
		    sess->trans.mcast &&
		    conform) {
			memcpy(&sess->trans, &str->session->trans, sizeof(struct ostrans));
			dbgprintf(DEBUG_RTSP, "non-owner tried to change transport parameters\n");
		}
		if (setup_nsp(&sess->trans, &nsp) < 0)
			return -1;
		if (set_ns(sess, &nsp) < 0)
			return -1;
	} else if (pidchange) {
		uint8_t *pids = (sp->pid);
		
		if (sess->nsfd >= 0)
			ioctl(sess->nsfd, NS_SET_PIDS, &pids);
	} 
	if (p->set & ((1UL << PARAM_CI))) {
		if (p->param[PARAM_CI] == 0 && str->ca)
			release_ca(str->ca);
		if (!str->ca) {
			if (sess->playing)
				return -455;
			str->ca = alloc_ca_num(str, p->param[PARAM_CI] - 1);
		}
		if (!str->ca)
			return -455;
	}
	if (p->set & ((1UL << PARAM_PMT))) {
		if (!str->ca) {
			if (sess->playing)
				return -455;
			str->ca = alloc_ca(str);
		}
		if (!str->ca)
			return -455;
		if (str->ca)
			set_pmt(str->ca, p->pmt);
	}
	if (con->session->trans.mcast) {
		if (con->os->has_switch)
			update_switch_vec(sess);
	}
	return 0;
}

static int stop_session(struct ossess *sess)
{
	if (!sess->playing)
		return 0;
	sess->playing &= ~1;
	if (sess->playing)
		return 0;
	printf("stopping session %d\n", sess->nr);
	if (sess->nsfd >= 0)
		ioctl(sess->nsfd, NS_STOP);
	return 0;
}

static int start_session(struct ossess *sess)
{
	if (sess->playing)
		return 0;
	dbgprintf(DEBUG_SYS, "start session %d\n", sess->nr);
	if (sess->stream->ca) {
		uint8_t canum = sess->stream->ca->nr - 1;
		if (sess->nsfd >= 0)
			ioctl(sess->nsfd, NS_SET_CI, &canum);
	}
	if (sess->nsfd >= 0)
		ioctl(sess->nsfd, NS_START);
	sess->playing |= 1;
	return 0;
}

static int play_session(struct oscon *con)
{
	int res;
	struct ossess *sess = con->session;
	struct dvb_params *p = &con->p;
	struct ostrans *trans = &con->trans;
	struct octoserve *os = sess->os;

	if (p) {
		res = setup_session(con, 0);
		if (res < 0)
			return res;
	}
#ifndef IGNORE_NS
	if (sess->nsfd < 0) 
		return -455;
#endif
	dbgprintf(DEBUG_SYS, "%s fd %d\n", __FUNCTION__, sess->nsfd);
	pthread_mutex_lock(&os->lock);
	start_session(sess);
	sess->playing |= 2;
	pthread_mutex_unlock(&os->lock);
	return 0;
}

static struct ossess *match_session(struct octoserve *os, uint8_t *group)
{
	struct ossess *sess;
	int i;

	for (i = 0; i < MAX_SESSION; i++) {
		sess = &os->session[i];
		if (!sess->state || !sess->trans.mcast)
			continue;
		if (!memcmp(sess->trans.mcip, group, 4))
			return sess;
	}
	return NULL;
}

void mc_check(struct ossess *sess, int update)
{
	struct osmcc *mcc, *next;
	struct octoserve *os = sess->os;

	if (sess->mcc_state && (os->igmp_mode & 1))
		return;
	pthread_mutex_lock(&os->lock);
	for (mcc = sess->mccs.lh_first; mcc; mcc = next) {
		next = mcc->mcc.le_next;
		if (((sess->mcc_state == 0) && (sess->mcc_tag != mcc->tag)) ||
		    (((os->igmp_mode & 1) == 0) && (os->igmp_tag != mcc->gtag))) {
			printf("removed client at %u.%u.%u.%u\n", 
			       mcc->ip[0], mcc->ip[1], mcc->ip[2], mcc->ip[3]);
			printf("mcc_tags: %d %d\n", sess->mcc_tag, mcc->tag);
			printf("gtags: %d %d\n", os->igmp_tag, mcc->gtag);
			LIST_REMOVE(mcc, mcc);
			free(mcc);
		}
	}
	if (update) {
		if (os->has_switch)
			update_switch_vec(sess);
		if (!sess->mccs.lh_first)
			stop_session(sess);
	}
	pthread_mutex_unlock(&os->lock);
}

void mc_del(struct ossess *sess)
{
	struct osmcc *mcc, *next;
	struct octoserve *os = sess->os;

	pthread_mutex_lock(&os->lock);
	for (mcc = sess->mccs.lh_first; mcc; mcc = next) {
		next = mcc->mcc.le_next;
		LIST_REMOVE(mcc, mcc);
		free(mcc);
	}
	if (os->has_switch)
		update_switch_vec(sess);
	pthread_mutex_unlock(&os->lock);
}

static void killall_sessions(struct octoserve *os)
{
	struct ossess *sess;
	int i;

	for (i = 0; i < MAX_SESSION; i++) {
		sess = &os->session[i];
		if (!sess->state)
			continue;
		release_session(sess);
	}
}

void mc_join(struct octoserve *os, uint8_t *ip, uint8_t *mac, uint8_t *group)
{
	struct ossess *sess;
	struct osmcc *mcc, *newmcc;

	pthread_mutex_lock(&os->lock);
	if ((sess = match_session(os, group)) == NULL)
		goto out;

	printf("matched session %d to join %u.%u.%u.%u\n", 
	       sess->nr, ip[0], ip[1], ip[2], ip[3]);
	for (mcc = sess->mccs.lh_first; mcc; mcc = mcc->mcc.le_next) 
		if (!memcmp(ip, mcc->ip, 4)) {
			mcc->tag = sess->mcc_tag;
			mcc->gtag = os->igmp_tag;
			printf("already in list, tag = %08x, gtag = %08x\n", mcc->tag, mcc->gtag);
			goto out;
		}
	newmcc = malloc(sizeof(struct osmcc));
	if (!newmcc) {
		printf("Could not allocate new multicast client entry\n");
		goto out;
	}
	memset(newmcc, 0, sizeof(struct osmcc));
	LIST_INSERT_HEAD(&sess->mccs, newmcc, mcc);
	memcpy(newmcc->ip, ip, 4);
	newmcc->tag = sess->mcc_tag;
	newmcc->gtag = os->igmp_tag;
	if (mac) {
		int port;

		memcpy(newmcc->mac, mac, 6);
		if (os->has_switch)
			port = switch_get_port(newmcc->mac);
		else 
			port = 0;
		printf("New client at port %02x\n", port);
		if (port >= 0) {
			newmcc->port_vec = port;
			sess->mcc_port_vec |= port;
		}
		if (os->has_switch)
			update_switch_vec(sess);
	}
	if (!sess->playing) 
		start_session(sess);
out:
	pthread_mutex_unlock(&os->lock);
}

void mc_query(struct ossess *sess)
{
	/* query in group if anybody still there */
	if (!sess->mcc_state) {
		mtime(&sess->mcc_time);
		sess->mcc_state = 1;
	}
}

void mc_leave(struct octoserve *os, uint8_t *ip, uint8_t *group)
{
	struct ossess *sess;
	struct osmcc *mcc, *next;
	
	pthread_mutex_lock(&os->lock);
	if ((sess = match_session(os, group)) == NULL)	
		goto out;
	
	printf("matched session %d to leave %u.%u.%u.%u\n", 
	       sess->nr, ip[0], ip[1], ip[2], ip[3]);
	mc_query(sess);

	for (mcc = sess->mccs.lh_first; mcc; mcc = next) {
		next = mcc->mcc.le_next;
		if (!memcmp(ip, mcc->ip, 4)) {
			LIST_REMOVE(mcc, mcc);
			free(mcc);
#if 0
			if (!sess->mccs.lh_first)
				stop_session(sess);
			mc_query(sess);
#endif
			break;
		}
	}
	if (os->has_switch)
		update_switch_vec(sess);
out:
	pthread_mutex_unlock(&os->lock);
}

static void send_play(struct oscon *con)
{
	uint8_t buf[1024];
	int len;

	len = sprintf(buf, 
		      "RTSP/1.0 200 OK\r\n"
		      "CSeq: %d\r\n"
		      "Session: %010d\r\n"
		      "RTP-Info: url=%s\r\n"
		      "\r\n", con->seq, con->session->id, con->url);
	sendlen(con->sock, buf, len);
	dbgprintf(DEBUG_RTSP, "Send: %s\n", buf);
}

static void send_teardown(struct oscon *con)
{
	uint8_t buf[256];
	int len;

	len=sprintf(buf, 
		    "RTSP/1.0 200 OK\r\n"
		    "CSeq: %d\r\n"
		    "Session: %010d\r\n"
		    "\r\n", con->seq, con->session->id);
	sendlen(con->sock, buf, len);
	dbgprintf(DEBUG_RTSP, "Send: %s\n", buf);
}

static int parse_transport(struct oscon *con, char *line)
{
	char *end, *arg;
	struct ostrans *t = &con->trans;
	int ttlset = 0;
	
	dbgprintf(DEBUG_RTSP, "parse: %s\n", line);

	for (arg = line; *arg == ' '; arg++);

	do {
		printf("arg:%s\n", arg);
		if (!strncasecmp(arg, "RTP/AVP/UDP", 11)) {
			t->rtp = 1;
			end = arg + 11;
		} else if (!strncasecmp(arg, "RTP/AVP", 7)) {
			t->rtp = 1;
			end = arg + 7;
		} else if (!strncasecmp(arg, "UDP", 3)) {
			t->rtp = 0;
			end = arg + 3;
		} else if (!strncasecmp(arg, "unicast", 7)) {
			t->mcast = 0;
			end = arg + 7;
		} else if (!strncasecmp(arg, "multicast", 9)) {
			t->mcast = 1;
			end = arg + 9;
		} else if (!strncasecmp(arg, "client_port=", 12)) {
			arg += 12;
			t->cport = strtoul(arg, &end, 10);
			if (*end == '-')
				t->cport2 = strtoul(end + 1, &end, 10);
			else
				t->cport2 = 0;
		} else if (!strncasecmp(arg, "port=", 5)) {
			arg += 5;
			t->cport = strtoul(arg, &end, 10);
			if (*end == '-')
				t->cport2 = strtoul(end + 1, &end, 10);
			else
				t->cport2 = 0;
		} else if (!strncasecmp(arg, "interleaved=", 12)) {
			return -461;
			arg += 12;
			strtoul(arg, &end, 10);
			strtoul(end + 1, &end, 10);
		} else if (!strncasecmp(arg, "ttl=", 4)) {
			arg += 4;
			t->ttl = strtoul(arg, &end, 10);
			ttlset = 1;
		} else if (!strncasecmp(arg, "destination=", 12)) {
			char c;
			
			arg += 12;
			end = arg;
			while (*end && *end != ';' && *end != ' ')
				end++;
			c = *end;
			*end = 0;
			inet_pton(AF_INET, arg, &t->mcip);
			t->mcmac[0] = 0x01;
			t->mcmac[1] = 0x00;
			t->mcmac[2] = 0x5e;
			t->mcmac[3] = t->mcip[1] & 0x7f;
			t->mcmac[4] = t->mcip[2];
			t->mcmac[5] = t->mcip[3];
			*end = c;
			t->flags |= TRANS_ALT_DEST;
		} else
			return -461;
		arg = end;
		if (*arg != ';' && *arg != ' ' && *arg != 0)
			return -461;
	} while (*(arg++) == ';');

	if (t->flags & TRANS_ALT_DEST && !t->mcast) {
		ip2mac(t->mcip, t->mcmac);		

	}
	if (!t->ttl && !ttlset)
		t->ttl = 2;
	dbgprintf(DEBUG_RTSP, "parsed\n");
	return 0;
}

static int parse_x_octonet(struct oscon *con, char *line)
{
	char *end, *arg;

	dbgprintf(DEBUG_RTSP, "parse: %s\n", line);

	for (arg = line; *arg == ' '; arg++);

	do {
		if (!strncasecmp(arg, "switch=", 7)) {
			uint32_t port;

			arg += 7;
			do {
				port = strtoul(arg, &end, 10);
				if (arg == end) 
					return -1;
				arg = end;
				if (port < 7)
					con->x_ports |= 1 << (port - 1);
			} while (*(arg++) == ',');
			printf("x_ports = %02x\n", con->x_ports);
		} else
			return -1;
		arg = end;
	} while (*(arg++) == ';');
	dbgprintf(DEBUG_RTSP, "parsed\n");
	return 0;
}

void send_setup(struct oscon *con)
{
	uint8_t buf[256];
	int len;
	struct ostrans *t = &con->session->trans;

	if (t->mcast) 
		len = sprintf(buf, 
			      "RTSP/1.0 200 OK\r\n"
			      "CSeq: %d\r\nSession: %010d;timeout=%u\r\n"
			      "Transport: %s;multicast;destination=%d.%d.%d.%d;"
			      "port=%d-%d;ttl=%d;source=%s\r\n"
			      "com.ses.streamID: %d\r\n\r\n", 
			      con->seq, con->session->id, con->session->timeout_len,
			      t->rtp ? "RTP/AVP" : "UDP",
			      t->mcip[0], t->mcip[1], t->mcip[2], t->mcip[3],
			      t->cport, t->cport2, t->ttl,
			      con->sadr_ip, con->session->stream->nr
			);
	else
		len = sprintf(buf, 
			      "RTSP/1.0 200 OK\r\n"
			      "CSeq: %d\r\nSession: %010d;timeout=%u\r\n"
			      "Transport: %s;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
			      "com.ses.streamID: %d\r\n\r\n", 
			      con->seq, con->session->id, con->session->timeout_len,
			      t->rtp ? "RTP/AVP" : "UDP",
			      t->cport, t->cport2, 
			      con->session->stream->sport,
			      con->session->stream->sport2,
			      con->session->stream->nr
			);
	sendlen(con->sock, buf, len);
	dbgprintf(DEBUG_RTSP, "Send: %s\n", buf);
}

static void cpyarg(char *d, char *s)
{
	while (*s == ' ')
		s++;
	strcpy(d, s);
}


static int cmp_trans(struct ostrans *t,  struct ostrans *u)
{
	if (t->mcast != u->mcast)
		return 1;
	
	if (t->cport != u->cport)
		return 2;
	if (t->cport2 != u->cport2)
		return 3;
	if (t->flags != u->flags)
		return 4;
	if (t->ttl != u->ttl)
		return 5;
	return 0;
}

static int proc_setup(struct oscon *con)
{
	struct osstrm *str = 0;
	int newtrans = 0;
	int res;
	
	if (!con->transport_parsed) {
		/* no proper transport params given */
		send_error(con, 400);
		return -1;
	}
	if (con->session_parsed && !con->session) {
		/* no session with given ID found */
		send_error(con, 454);
		return -1;
	}
	if (parse_url(con, 0) < 0) {
		/* invalid params in URL */
		send_error(con, 400);
		return -1;
	} 
	if (con->p.set & (1UL << PARAM_STREAMID)) {
		dbgprintf(DEBUG_SYS, "existing stream %d\n",
			  con->p.param[PARAM_STREAMID]);
		str = get_stream(con->os, con->p.param[PARAM_STREAMID]);
		if (!str) {
			/* no stream with given ID  */
			send_error(con, 400);
			return -1;
		}
		if (con->session && con->session != str->session) {
			/* if we already have a session ID we
			   have to be stream owner  */
			send_error(con, 400);
			return -1;
		}
	}
	if (!con->session) {
		/* alloc new session */
		con->session = alloc_session(con->os);
		if (!con->session) {
			send_error(con, 400);
			return -1;
		}
		con->state = 2;
		if (!con->session->stream) {
			if (str) {
				/* use existing stream and stream params*/
				con->session->stream = str;
				if (con->session->stream->session) {
					memcpy(&con->session->p,
					       &con->session->stream->session->p,
					       sizeof(struct dvb_params));
				}
			} else {
				con->session->stream = alloc_stream(con->os);
				if (!con->session->stream) {
					send_error(con, 400);
					return -1;
				}
				con->session->stream->session = con->session;
			}
		}
		newtrans = 1;
	}
	if (newtrans || cmp_trans(&con->session->trans, &con->trans)) {
		con->trans.sport = con->session->stream->sport;
		con->trans.sport2 = con->session->stream->sport2;
		
		/* set transport struct according to session
		   and transport parameters */
		if (con->trans.mcast && con->trans.cport == 0) {
			con->trans.cport = con->trans.sport;
			con->trans.cport2 = con->trans.sport2;
		}
		
		if (con->trans.mcast &&
		    !(con->trans.flags & TRANS_ALT_DEST)) {
			uint8_t mac[6] = { 0x01, 0x00, 0x5e, 
					   con->os->ssdp.devid & 0x7f, 1,
					   con->session->stream->nr };
			uint8_t ip[4]  = { 239, con->os->ssdp.devid, 1,
					   con->session->stream->nr };
			
			memcpy(con->trans.mcmac, mac, 6);
			memcpy(con->trans.mcip, ip, 4);
		}
		con->session->trans = con->trans;
		newtrans = 1;
	}
	res = setup_session(con, newtrans);
	if (res < 0) {
		release_session(con->session);
		send_error(con, -res);
	} else
		send_setup(con);
	return 0;
}


static int proc_line(struct oscon *con)
{
	char *line = con->buf;
	int res;

	if (con->ln == 0) {
		if (!strncasecmp(line, "OPTIONS", 7)) {
			cpyarg(con->url, line + 7); 
			con->cmd = M_OPTIONS;
		} else if (!strncasecmp(line, "DESCRIBE", 8)) {
			cpyarg(con->url, line + 8); 
			con->cmd = M_DESCRIBE;
		} else if (!strncasecmp(line, "SETUP", 5)) {
			cpyarg(con->url, line + 5); 
			con->cmd = M_SETUP;
		} else if (!strncasecmp(line, "PLAY", 4)) {
			cpyarg(con->url, line + 4); 
			con->cmd = M_PLAY;
		} else if (!strncasecmp(line, "TEARDOWN", 8)) {
			cpyarg(con->url, line + 8); 
			con->cmd = M_TEARDOWN;
		} else {
			con->cmd = M_UNKNOWN;
			return -1;
		}
		return 0;
	}

	if (line[0] == '\0') { /* last line */
		if (con->error) {
			send_error(con, con->error);
		} else {
			switch (con->cmd) {
			case M_OPTIONS:
				if (con->session_parsed && !con->session)
					send_error(con, 404);
				else
					send_option(con);
				break;
			case M_DESCRIBE:
			{
				int only = -1;
				
				if (parse_url(con, 0) < 0) {
					send_error(con, 400);
					break;
				} 
				if (con->p.set & ~(1UL << PARAM_STREAMID)) {
					send_describe2(con, con->url);
					break;
				}
				if (con->p.set & (1UL << PARAM_STREAMID)) 
					only = con->p.param[PARAM_STREAMID];
				res = send_describe(con, only);
				if (res < 0)
					send_error(con, -res);
				break;
			}
			
			case M_SETUP:
				proc_setup(con);
				break;

			case M_PLAY:
				if (!con->session || !con->session->stream) {
					send_error(con, 400);
					break;
				}
				if (parse_url(con, 0) < 0) {
					send_error(con, 400);
					break;
				} 
				if (con->x_octonet_parsed) {
					con->session->port_vec = con->x_ports;
					printf("port_vec = %02x\n", con->session->port_vec);
				}
				res = play_session(con);
				if (res < 0) 
					send_error(con, -res);
				else
					send_play(con);
				break;
			case M_TEARDOWN:
				if (parse_url(con, 0) < 0) {
					send_error(con, 400);
					break;
				} 
				if (!con->session) {
					send_error(con, 404);
					break;
				}
				send_teardown(con);
				release_session(con->session);
				break;
			default:
				send_error(con, 501);
				break;
			}
		}
		con->cmd = M_NONE;
		con->transport_parsed = 0;
		con->session_parsed = 0;
		con->error = 0;
		con->ln = -1;
		return 0;
	} 
	
	if (!strncasecmp(line, "CSeq:", 5)) {
		con->seq = strtoul(line + 5, NULL, 10);
		//printf("CSeq = %d\n", con->seq);
	} else if (!strncasecmp(line, "Transport:", 10)) {
		res = parse_transport(con, line + 10);
		if (res < 0) {
			con->error = -res;
		} else
			con->transport_parsed = 1;
	} else if (!strncasecmp(line, "Session:", 8)) {
		uint32_t sid = strtoul(line + 8, NULL, 10);
		
		con->session = get_session(con->os, sid);
		if (con->session)
			session_timeout(con->session);
		con->state = 2;
		con->session_parsed = 1;
	} else if (!strncasecmp(line, "User-Agent:", 11)) {
		char *p = line + 11;
		
		while (*p == ' ')
			p++;
		if (!strncasecmp(p, "libvlc", 6))
			con->trans.flags |= TRANS_NO_RTP_TO;
	} else if (!strncasecmp(line, "x_octonet:", 10)) {
		char *p = line + 10;
		
		while (*p == ' ')
			p++;
		parse_x_octonet(con, p);
		con->x_octonet_parsed = 1;
	} else 
		;//con->error = 551;
	return 0;
}

static const char *intop(int af, struct sockaddr *sa, char *dst, socklen_t size, uint8_t *ip)
{
       void *adr;

       if (af == AF_INET) 
		adr = &((struct sockaddr_in *) sa)->sin_addr;
       else if (af == AF_INET6)
	       adr = &((struct sockaddr_in6 *) sa)->sin6_addr;
       else 
	       return NULL;

       memcpy(ip, adr,  af == AF_INET ? 4 : 16);
       return inet_ntop(af, adr, dst, size);
}

static void get_ips(struct oscon *con)
{
	con->slen = sizeof(con->sadr);
	con->clen = sizeof(con->cadr);
	getsockname(con->sock, (struct sockaddr *) &con->sadr, &con->slen);
	intop(con->trans.family, &con->sadr, con->sadr_ip, INET6_ADDRSTRLEN, con->trans.sip);
	getpeername(con->sock, (struct sockaddr *) &con->cadr, &con->clen);
	intop(con->trans.family, &con->cadr, con->cadr_ip, INET6_ADDRSTRLEN, con->trans.cip);
}

static int same_ip(struct sockaddr *sa, struct sockaddr *sa2)
{
	unsigned short af = sa->sa_family;
	
	if (af != sa2->sa_family)
		return -1;
	if (af == AF_INET) 
		return memcmp(&((struct sockaddr_in *) sa)->sin_addr,
			      &((struct sockaddr_in *) sa2)->sin_addr, 4);
	else if (af == AF_INET6)
		return memcmp(&((struct sockaddr_in6 *) sa)->sin6_addr,
			      &((struct sockaddr_in6 *) sa2)->sin6_addr, 16);
	else
		return -1;
}

static void init_con(struct oscon *con)
{
	int one = 1, idle = 20, intvl = 20, cnt = 3;

	get_ips(con);
	if (!same_ip(&con->sadr, &con->cadr))
		memcpy(con->trans.cmac, con->os->mac, 6);
	else
		get_mac(con->os->ifname, &con->cadr, con->trans.cmac);
	memcpy(con->trans.smac, con->os->mac, 6);
	con->bufp = 0;
	con->ln = 0;
	con->session = 0;
	setsockopt(con->sock, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
#if 1
	setsockopt(con->sock, SOL_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
	setsockopt(con->sock, SOL_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
	setsockopt(con->sock, SOL_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
#else
	setsockopt(con->sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
	setsockopt(con->sock, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
	setsockopt(con->sock, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
	
#endif
}


static void con_check(struct octoserve *os)
{
	int i;
	time_t t = mtime(NULL);

	for (i = 0; i < MAX_CONNECT; i++) {
		if (os->con[i].state == 3 &&
		    t > os->con[i].timeout)
			release_con(&os->con[i]);
	}
}

int con_loop(struct oscon *con)
{
	uint8_t buf[1024];
	int len, i, res;

	len = recv(con->sock, buf, 1024, 0);
	if (len == 0) 
		goto release;
	if (len < 0) {
		if (errno != EAGAIN) 
			goto release;
		return 0;
	}
	if (debug & DEBUG_RTSP)
		dump(buf, len);
	//printf("received %d bytes\n", len);
	for (i = 0; i < len; i++) {
		// FIXME send URI too long
		if (con->bufp >= 8192) {
			goto release;
		}
		con->buf[con->bufp++] = buf[i];
		if (buf[i] == '\n') {
			if (con->bufp < 2 || 
			    con->buf[con->bufp - 2] != '\r') {
				goto release;
			}
			con->buf[con->bufp - 2] = 0;
			if (con->buf[0])
				dbgprintf(DEBUG_SYS, "%d:%d:%d: %s\n", 
					  con->nr, con->ln, 
					  con->bufp, con->buf);
			res = proc_line(con);
			con->ln++;
			con->bufp = 0;
		}
	}
	return 0;
release:
	dbgprintf(DEBUG_SYS, "release\n");
	release_con(con);
	return -1;
}

void handle_con(struct oscon *con)
{
	while (con_loop(con) >= 0);
}

int set_nonblock(int fd)
{
	int fl = fcntl(fd, F_GETFL);
	
	if (fl < 0)
		return fl;
	return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

int set_block(int fd)
{
	int fl = fcntl(fd, F_GETFL);
	
	if (fl < 0)
		return fl;
	return fcntl(fd, F_SETFL, fl & ~O_NONBLOCK);
}

void check_frontends(struct octoserve *os)
{
	int i;
	struct dvbfe *fe;
	uint16_t sig, snr;
	fe_status_t stat;

	for (i = 0; i < os->dvbfe_num; i++) {
		fe = &os->dvbfe[i];
		if (fe->state != 1)
			continue;
		if (ioctl(fe->fd, FE_READ_STATUS, &stat) < 0)
			continue;
		if (ioctl(fe->fd, FE_READ_SIGNAL_STRENGTH, &sig) < 0)
			continue;
		if (ioctl(fe->fd, FE_READ_SNR, &snr) < 0)
			continue;
		fe->stat = stat;
		fe->lock = (stat == 0x1f) ? 1 : 0;
		fe->level = sig >> 8;
		fe->quality = snr >> 12;
		dbgprintf(DEBUG_DVB, "fe%d: stat=%02x str=%04x snr=%04x\n", fe->nr, stat, sig, snr);
	}
}

static int check_streams(struct octoserve *os)
{
	int i;
	struct osstrm *str;
	
	for (i = 0; i < MAX_STREAM; i++) {
		str = &os->stream[i];
		if (!str->state)
			continue;
		set_rtcp_msgs(str);
	}
	return 0;
}


static int alloc_igmp_socket(struct octoserve *os)
{
	struct ip_mreq imr;
	struct sockaddr_in sadr;
	uint8_t one = 1;
	
	os->igmp_sock = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP); 

	set_nonblock(os->igmp_sock);
	get_ifa(os->ifname, AF_INET, (struct sockaddr *) &sadr);
	imr.imr_interface.s_addr = sadr.sin_addr.s_addr;
	imr.imr_multiaddr.s_addr = inet_addr("224.0.0.22");
	setsockopt(os->igmp_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		   &imr, sizeof(imr));
	setsockopt(os->igmp_sock, IPPROTO_IP, IP_MULTICAST_TTL, 
		   &one, sizeof(one));
	setsockopt(os->igmp_sock, IPPROTO_IP, IP_ROUTER_ALERT, 
		   &one, sizeof(one));
	mtime(&os->igmp_time);
	/* first query after 125-94=31 seconds */
	os->igmp_mode = 0;
	os->igmp_time -= 94;
	os->igmp_robust = 1;
	return 0;
}

static int alloc_igmp_raw_socket(struct octoserve *os)
{
	struct ifreq ifr;
	int s, n, i;
	struct sockaddr cadr;
	socklen_t len = sizeof(cadr);
	uint8_t buf[4096];
	struct sock_filter bpf_igmp[] = {
		{ 0x28, 0, 0, 0x0000000c },
		{ 0x15, 0, 3, 0x00000800 },
		{ 0x30, 0, 0, 0x00000017 },
		{ 0x15, 0, 1, 0x00000002 },
		{ 0x6, 0, 0, 0x0000ffff },
		{ 0x6, 0, 0, 0x00000000 },
	};
	struct sock_fprog sfprog;
	
	sfprog.len = ARRAY_SIZE(bpf_igmp);
	sfprog.filter = bpf_igmp;
	
	s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));
	set_nonblock(s);

	memset (&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, os->ifname, IFNAMSIZ);
	ioctl(s, SIOCGIFFLAGS, &ifr);
	ifr.ifr_flags |= IFF_PROMISC;
	ioctl(s, SIOCSIFFLAGS, &ifr);

	n  = setsockopt(s, SOL_SOCKET, SO_ATTACH_FILTER, &sfprog, sizeof(sfprog));
	if (n < 0) {
		printf("could not set filter\n");
	}
	
#if 0
	printf("raw = %d\n", s);
	for (i = 0; i < 5; i++) {
		n = recvfrom(s, buf, sizeof(buf), 0, NULL, NULL);
		printf("raw = %d, n = %d, errno = %d\n", s, n, errno);
		dump(buf, n);
	}
	close (s);
#endif
	os->igmp_rsock = s;
	return 0;
}

static void os_serve(struct octoserve *os)
{
	struct sigaction sa;
	fd_set fds;
	int mfd, i;
	struct timeval timeout;
	time_t t, u;

	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	sa.sa_handler = sigchld_handler; 
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	if (sigaction(SIGCHLD, &sa, NULL) == -1) 
		die("Error in sigaction");

	os->rtsp_sock = streamsock("554", AF_INET, &os->rtsp_sadr);
	sockname(&os->rtsp_sadr, os->rtsp_ip);
	dbgprintf(DEBUG_SYS, "rtsp_ip: %s, %d\n", os->rtsp_ip, os->rtsp_sock);
	if (listen(os->rtsp_sock, 20) < 0) {
		dbgprintf(DEBUG_SYS, "listen error");
		return;
	}
	set_nonblock(os->rtsp_sock);
	
	alloc_igmp_raw_socket(os);
	alloc_igmp_socket(os);

#if 0
	{
		struct ipv6_mreq imr;

		memset(&imr, 0, sizeof(imr));
		imr.ipv6mr_interface = if_nametoindex(os->ifname);
		inet_pton(AF_INET6, "FF02::16", &imr.ipv6mr_multiaddr);
		setsockopt(os->mld_sock, IPPROTO_IPV6, IPV6_JOIN_GROUP,
			   &imr, sizeof(imr));
	}
#endif
	mtime(&t);
	while (!os->exit) {
		int csock, ncon;
		struct sockaddr cadr;
		socklen_t clen = sizeof(cadr);
		uint8_t buf[2048];
		int num, n;

		mtime(&u);
		if (u > t) {
			t = u;
			check_session_timeouts(os);
			//check_frontends(os);
			check_streams(os);
			check_igmp(os);
		}

		timeout.tv_sec = 0;
		timeout.tv_usec = 500000;

		FD_ZERO(&fds);
		mfd = 0;
		add_fd(os->rtsp_sock, &mfd, &fds);
		//add_fd(os->igmp_sock, &mfd, &fds);
		add_fd(os->igmp_rsock, &mfd, &fds);
		//add_fd(os->mld_sock, &mfd, &fds);
		for (i = 0, ncon = 0; i < MAX_CONNECT; i++) {
			if (os->con[i].state) {
				ncon++;
				add_fd(os->con[i].sock, &mfd, &fds);
			}
		}
		//printf("serve_loop: %d cons, mfd=%d\n", ncon, mfd);
		num = select(mfd + 1, &fds, NULL, NULL, &timeout);
		if (num < 0)
			break;
		if (FD_ISSET(os->igmp_sock, &fds)) {
			n = recvfrom(os->igmp_sock, buf, sizeof(buf), 0, &cadr, &clen);
			if (n > 0)
				proc_igmp(os, buf, n, NULL);
		}
		if (FD_ISSET(os->igmp_rsock, &fds)) {
			n = recvfrom(os->igmp_rsock, buf, sizeof(buf), 0, &cadr, &clen);
			if (n > 14)
				proc_igmp(os, buf + 14, n - 14, buf);
		}
		if (FD_ISSET(os->mld_sock, &fds)) {
			n = recvfrom(os->mld_sock, buf, sizeof(buf), 0, &cadr, &clen);
			printf("n = %d\n", n);
			if (n > 0)
				;//dump(buf, n);
		}
#ifndef MULTI_THREADED
		for (i = 0; i < MAX_CONNECT; i++) 
			if (os->con[i].state && 
			    FD_ISSET(os->con[i].sock, &fds))
				con_loop(&os->con[i]);
#endif
		con_check(os);

		
		if (FD_ISSET(os->rtsp_sock, &fds)) {
			struct oscon *con;

			csock = accept(os->rtsp_sock, &cadr, &clen);
			if (csock < 0) {
				dbgprintf(DEBUG_SYS, "Could not accept\n");
				continue;
			}
			con = alloc_con(os);
			if (!con) {
				dbgprintf(DEBUG_SYS, "Could not alloc new connection\n");
				continue;
			}
			con->sock = csock;
			con->trans.family = AF_INET;
			init_con(con);
			dbgprintf(DEBUG_SYS, "sock %d  con  %d on %s\n", 
				  con->sock, con->nr, con->cadr_ip);
#ifdef MULTI_THREADED
			pthread_create(&con->pt, NULL, (void *) handle_con, con); 
			set_block(csock);
#else
			set_nonblock(csock);
#endif
		}
	}
	killall_sessions(os);
}

static struct octoserve *os_init(char *ifname, int nossdp, int nodms, int nodvbt, int noswitch)
{
	struct octoserve *os;
	struct os_ssdp *ss;
	struct ifreq ifr;
	int s;
	pthread_mutexattr_t mta;

	os = malloc(sizeof(*os));
	if (!os)
		return NULL;
	dbgprintf(DEBUG_SYS, "allocated octoserve struct, %d bytes\n", sizeof(*os));
	memset(os, 0, sizeof(struct octoserve));

	pthread_mutexattr_init(&mta);
	pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&os->lock, &mta);

	os->ifname = ifname;
	os->sessionid = random();

	if (get_ifa(ifname, AF_INET, &os->ssdp.sadr) < 0) {
		perror("no such interface:");
		free(os);
		return NULL;
	}
	get_ifa(ifname, AF_INET6, &os->ssdp.sadr6);
	sockname(&os->ssdp.sadr, os->ssdp.ip);
	sockname(&os->ssdp.sadr6, os->ssdp.ip6);

	strcpy(ifr.ifr_name, os->ifname);
	s = socket(AF_INET, SOCK_DGRAM, 0);
	ioctl(s, SIOCGIFHWADDR, &ifr);
	close(s);
	memcpy(os->mac, ifr.ifr_hwaddr.sa_data, 6);
	dbgprintf(DEBUG_NET, "MAC %s=%02x:%02x:%02x:%02x:%02x:%02x\n", 
		  os->ifname,
		  os->mac[0], os->mac[1], os->mac[2], 
		  os->mac[3], os->mac[4], os->mac[5]);
	if (os->has_switch)
		switch_get_port(os->mac);

	init_dvb(os, nodvbt, noswitch);

	ss = &os->ssdp;
	if (init_ssdp(os, &os->ssdp, debug, nossdp, nodms) < 0) {
		release_dvb(os);
		free(os);
		return NULL;
	}
#if 0
	if (init_httpos, debug) < 0) {
		release_dvb(os);
		free(os);
		return NULL;
	}
#endif
	return os;
}

struct octoserve *os;

void term_action(int sig)
{
	printf("EXIT\n");
	os->exit = 1;
}

static int set_termaction(void)
{
	struct sigaction term;

	memset(&term, 0, sizeof(term));
	term.sa_handler = term_action;
	sigemptyset(&term.sa_mask);
	term.sa_flags = 0;
	sigaction(SIGINT, &term, NULL);

	memset(&term, 0, sizeof(term));
	term.sa_handler = term_action;
	sigemptyset(&term.sa_mask);
	term.sa_flags = 0;
	sigaction(SIGTERM, &term, NULL);
}

static int fexists(char *fn)
{
	struct stat b;

	return (!stat(fn, &b));
}

static void awrite(char *fn, char *txt)
{
	FILE *f = fopen(fn, "w");

	if (f)
		fprintf(f, "%s", txt);
}

int main(int argc, char **argv)
{
	int nodms = 0, nossdp = 0, nodvbt = 0, vlan = 0, noswitch = 0;
		
	printf("Octoserve " OCTOSERVE_VERSION
	       ", Copyright (C) 2012-15 Digital Devices GmbH\n"); 
	debug = 0;
	flags = 0;
	while (1) {
                int option_index = 0;
		int c;
                static struct option long_options[] = {
			{"debug", required_argument, 0, 'd'},
			{"flags", required_argument, 0, 'f'},
			{"nossdp", no_argument, 0, 'n'},
			{"nodms", no_argument, 0, 'm'},
			{"nodvbt", no_argument, 0, 't'},
			{"noswitch", no_argument, 0, 's'},
			{"conform", no_argument, 0, 'c'},
			{"help", no_argument , 0, 'h'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, 
				"d:f:nmtsch",
				long_options, &option_index);
		if (c==-1)
 			break;

		switch (c) {
		case 'd':
			debug = strtoul(optarg, NULL, 16);
			break;
		case 'f':
			flags = strtoul(optarg, NULL, 16);
			break;
		case 'n':
			nossdp = 1;
			break;
		case 'm':
			nodms = 1;
			break;
		case 't':
			nodvbt = 1;
			break;
		case 's':
			noswitch = 1;
			break;
		case 'c':
			conform = 1;
			break;
		case 'h':
		default:
			break;
			
		}
	}
	if (optind < argc) {
		printf("Warning: unused arguments\n");
	}
	if (fexists("/config/nodms.enabled"))
		nodms = 1;
	if (fexists("/config/noswitch.enabled"))
		noswitch = 1;
	if (fexists("/config/nodvbt.enabled"))
		nodvbt = 1;
	if (fexists("/config/vlan.enabled")) {
		awrite("/sys/class/ddbridge/ddbridge0/vlan", "1");
		vlan = 1;
	} else
		awrite("/sys/class/ddbridge/ddbridge0/vlan", "0");
	printf("nodms = %d, nodvbt = %d, vlan = %d\n", nodms, nodvbt, vlan);
	
	os = os_init("eth0", nossdp, nodms, nodvbt, noswitch);
	if (!os)
		return -1;
	set_termaction();

	os->has_switch = switch_test();
	if (os->has_switch)
		printf("Switch detected\n");
	else
		printf("No switch detected\n");
	
	os_serve(os);
	if (!nossdp)
		pthread_join(os->ssdp.pt, NULL);
	release_dvb(os);
	free(os);
}
