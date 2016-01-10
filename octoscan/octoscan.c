#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <signal.h>
#include <dirent.h>
#include <pthread.h>
#include <ifaddrs.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <netdb.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <time.h>
#include <ctype.h>
#include "list.h"

#include <getopt.h>

time_t mtime(time_t *t)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts))
		return 0;
	if (t)
		*t = ts.tv_sec;
	return ts.tv_sec;
}

static int done = 0;
static int use_nit = 0;

char *pol2str[] = {"v", "h", "r", "l"};
char *msys2str [] = {"undef", "dvbc", "dvbcb", "dvbt", "dss", "dvbs", "dvbs2", "dvbh",
		      "isdbt", "isdbs", "isdbc", "atsc", "atscmh", "dtmb", "cmmb", "dab",
		      "dvbt2", "turbo", "dvbcc", "dvbc2"};
char *mtype2str [] = {"qpsk", "16qam", "32qam",
		      "64qam", "128qam", "256qam",
		      "autoqam", "8vsb", "16vsb", "8psk",
		      "16apsk", "32apsk", "dqpsk", "4qamnr", NULL};
char *pilot2str [] = {"on", "off", "auto", NULL};
char *roll2str [] = {"0.35", "0.20", "0.25", NULL};
char *fec2str [] = {"none", "12", "23", "34", "56", "78", "89", "35", "45", "910", "25", NULL};
char *bw2str [] = {"8", "7", "6", "auto", "5", "10", "1.712", NULL };
char *tmode2str [] = { "2k", "8k", "auto", "4k", "1k", "16k", "32k", "c1", "c3780", NULL};
char *gi2str [] = { "132", "116", "18", "14", "auto", "1128", "19128", "19256", "pn420", "pn595", "pn945", NULL};
char *num2str [] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", NULL};

struct sfilter {
	struct pid_info *pidi;
	struct list_head link;
	struct list_head tslink;

	uint8_t  tid;
	uint16_t  ext;

	//int (*cb) (struct sfilter *sf);

	uint8_t vnr;
	unsigned int todo_set : 1;
	unsigned int done : 1;
	unsigned int use_ext : 2;
	unsigned int vnr_set : 1;
	uint32_t todo[8];

	time_t   timeout;
	uint32_t timeout_len;
};

struct pid_info {
	struct list_head link;
	struct ts_info *tsi;
	struct list_head sfilters;

	uint16_t pid;
	int      add_ext;
	int      done;

        uint8_t  used;
        uint8_t  cc;
	uint16_t bufp;
	uint16_t len;
	uint8_t *buf;
};

struct satipcon {
	char *host;
	char *port;
	char tune[256];
	char sid[64];
	uint32_t strid;
	int seq;
	int sock;
	int usock;
	int nsport;
};

struct ts_info {
	struct list_head pids;
	struct list_head sfilters;

	struct scantp *stp;
	uint16_t tsid;
	time_t timeout;
	int done;

        struct pid_info pidi[0x2000];
};

#define MAX_ANUM 32

struct service {
	struct list_head link;
	struct tp_info *tpi;

	char name[80];
	char pname[80];

	unsigned int got_pmt : 1;
	unsigned int got_sdt : 1;
	unsigned int is_enc  : 1;

	uint16_t sid;
	uint16_t tsid;

	uint16_t pmt;
	uint16_t pcr;
	uint16_t vpid;
	uint16_t apid[MAX_ANUM];
	uint16_t sub;
	uint16_t ttx;
	uint8_t  anum;
};

struct tp_info {
	struct list_head link;
	struct list_head services;

	int type;
	uint32_t src;

	uint16_t nid;
	uint16_t onid;
	uint16_t tsid;

	uint32_t pos;
	uint32_t east;
	uint16_t id;
	uint32_t msys;
	uint32_t freq;
	uint32_t freq_frac;
	uint32_t pol;
	uint32_t sr;
	uint32_t ro;
	uint32_t mod;
	uint32_t bw;
	uint32_t fec;
	uint32_t isi;
};

struct scantp {
	struct scanip *sip;
	struct tp_info *tpi;
	time_t timeout;

	struct list_head sfilters;
	struct ts_info tsi;
	struct satipcon scon;
};

struct scanip {
	char *host;

	struct list_head tps;
	struct list_head tps_done;
	struct scantp stp;
	int done;
};


static struct service *get_service(struct tp_info *tpi, uint16_t sid)
{
	struct service *s;

	list_for_each_entry(s, &tpi->services, link) {
		if (s->sid == sid)
			return s;
	}
	s = calloc(1, sizeof(struct service));
	s->sid = sid;
	snprintf(s->name, sizeof(s->name), "Service %d", sid);
	snprintf(s->pname, sizeof(s->name), "none");
	list_add(&s->link, &tpi->services);
	return s;
}

/****************************************************************************/
/****************************************************************************/


void dump(const uint8_t *b, int l)
{
	int i, j;

	for (j = 0; j < l; j += 16, b += 16) {
		for (i = 0; i < 16; i++)
			if (i + j < l)
				fprintf(stderr, "%02x ", b[i]);
			else
				fprintf(stderr, "   ");
		fprintf(stderr, " | ");
		for (i = 0; i < 16; i++)
			if (i + j < l)
				fputc((b[i] > 31 && b[i] < 127) ? b[i] : '.', stderr);
		fprintf(stderr, "\n");
	}
}

int sendlen(int sock, char *buf, int len)
{
	int done, todo;

	for (todo = len; todo; todo -= done, buf += done)
		if ((done = send(sock, buf, todo, 0)) < 0)
			return done;
	return len;
}

static int udpsock(struct sockaddr *sadr, char *port)
{
	int one=1, sock;
	struct addrinfo *ais, *ai, hints = {
		.ai_flags = AI_PASSIVE,
		.ai_family = AF_INET,
		.ai_socktype = SOCK_DGRAM,
		.ai_protocol = 0,
		.ai_addrlen = 0,
		.ai_addr = sadr,
		.ai_canonname = NULL,
		.ai_next = NULL,
	};

	if (getaddrinfo(NULL, port, &hints, &ais) < 0)
		return -1;
	for (ai = ais; ai; ai = ai->ai_next)  {
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock == -1)
			continue;
		if (!setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) &&
		    !bind(sock, ai->ai_addr, ai->ai_addrlen))
			break;
		close(sock);
		sock = -1;
	}
	freeaddrinfo(ais);
	return sock;
}

static int streamsock(const char *name, const char *port, struct sockaddr *sadr)
{
	int one=1, sock;
	struct addrinfo *ais, *ai, hints = {
		.ai_flags = 0,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0, .ai_addrlen = 0,
		.ai_addr = NULL, .ai_canonname = NULL, .ai_next = NULL,
	};

	if (getaddrinfo(name, port, &hints, &ais) < 0)
		return -1;
	for (ai = ais; ai; ai = ai->ai_next)  {
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock == -1)
			continue;
		if (!connect(sock, ai->ai_addr, ai->ai_addrlen)) {
			*sadr = *ai->ai_addr;
			break;
		}
		close(sock);
		sock = -1;
	}
	freeaddrinfo(ais);
	return sock;
}

static const char *sockname(struct sockaddr *sadr, char *name)
{
	void *adr;

	if (sadr->sa_family == AF_INET)
		adr = &((struct sockaddr_in *) sadr)->sin_addr;
	else
		adr = &((struct sockaddr_in6 *) sadr)->sin6_addr;
	return inet_ntop(sadr->sa_family, adr, name, INET6_ADDRSTRLEN);
}

static void send_setup(int s, char *host, char *port, char *tune,
		       int *seq, uint16_t cport, int mc)
{
	uint8_t buf[256], opt[256] = { 0 };
	int len;

	if (mc)
		len = snprintf(buf, sizeof(buf),
			       "SETUP rtsp://%s:%s/?%s RTSP/1.0\r\n"
			       "CSeq: %d\r\n"
			       "Transport: RTP/AVP;multicast;port=%d-%d;ttl=3\r\n"
			       "\r\n",
			       host, port, tune,
			       *seq , cport, cport + 1);
	else
		len = snprintf(buf, sizeof(buf),
			       "SETUP rtsp://%s:%s/?%s RTSP/1.0\r\n"
			       "CSeq: %d\r\n"
			       "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n"
			       "\r\n",
			       host, port, tune,
			       *seq , cport, cport + 1);
	(*seq)++;
	if (len > 0 && len < sizeof(buf)) {
		sendlen(s, buf, len);
		//printf("Send: %s\n", buf);
	}
}

static void send_play(int s, char *host, char *port, uint32_t strid,
		      int *seq, char *sid)
{
	uint8_t buf[1024];
	int len;

	len = snprintf(buf, sizeof(buf),
		       "PLAY rtsp://%s:%s/stream=%u RTSP/1.0\r\n"
		       "CSeq: %d\r\n"
		       "Session: %s\r\n"
		       "\r\n",
		       host, port, strid,
		       *seq , sid);
	(*seq)++;
	if (len > 0 && len < sizeof(buf)) {
		sendlen(s, buf, len);
		//printf("Send: %s\n", buf);
	}
}

static void send_teardown(int s, char *host, char *port, uint32_t strid,
			  int *seq, char *sid)
{
	uint8_t buf[1024];
	int len;

	len = snprintf(buf, sizeof(buf),
		       "TEARDOWN rtsp://%s:%s/stream=%u RTSP/1.0\r\n"
		       "CSeq: %d\r\n"
		       "Session: %s\r\n"
		       "\r\n",
		       host, port, strid, *seq , sid);
	(*seq)++;
	if (len > 0 && len < sizeof(buf)) {
		sendlen(s, buf, len);
		//printf("Send: %s\n", buf);
	}
}

static int get_url(char *url, char **a)
{
	struct sockaddr_in sa;
	char sname[INET_ADDRSTRLEN];
	int s;
	struct sockaddr sadr;
	char host[1024], port[1024], dport[] = "554";
	char *u, *e;

	if (strncasecmp(url, "rtsp://", 7))
		return  -1;
	e = url + 7;
	for (u = e; *u && *u != ':' && *u != '/'; u++);
	if (u == e || !*u)
		return -1;
	memcpy(host, e, u - e);
	host[u - e] = '\0';
	if (*u == ':') {
		e = u + 1;
		for (u++; *u && *u != '/'; u++);
		if (!*u)
			return -1;
		memcpy(port, e, u - e);
		port[u - e] = '\0';
	} else 	if (*u == '/') {
		strcpy(port, dport);
	}
	*a = u + 1;
	//printf("host %s, port %s\n", host, port);
	s = streamsock(host, port, &sadr);
	if (s < 0)
		return -1;
	if (!sockname(&sadr, sname))
		return -1;
	//printf("%s\n", sname);
	return s;
}

static void getarg(char *b, char **a, char **ae)
{
	while (isspace(*b))
		b++;
	*a = b;
	while (*b && *b != ';')
		b++;
	*ae = b;
	**ae = 0;
}

static int check_ok(int s, char *sid, uint32_t *strid)
{
	char b[4096], *a, *ae;
	int n, bl = 0, bs = sizeof(b);
	char *l, *e;
	uint32_t sport, sport2;

	while (1) {
		n = recv(s, b + bl, bs - bl, 0);
		if (n <= 0)
			return 0;
		if (n + bl > bs)
			return -1;
		bl += n;
		if (bl >=4 &&
		    b[bl - 4] == '\r' && b[bl - 3] == '\n' &&
		    b[bl - 2] == '\r' && b[bl - 1] == '\n')
			break;
	}
	b[bl-2] = 0;
	if (strncasecmp(b, "RTSP/1.0 200 OK\r\n", 17))
		return -1;
	//dump(b, bl);
	for (l = b + 17; *l; l = e + 1) {
		for (e = l; *e && *e != '\r' && *e != '\n'; e++);
		if (e == l)
			continue;
		*e = 0;
		//printf("%s\n", l);
		if (!strncasecmp(l, "Session:", 8)) {
			getarg(l + 8, &a, &ae);
			*ae = 0;
			//printf("session = %s\n", a);
			strcpy(sid, a);
		} else if (!strncasecmp(l, "Transport:", 10)) {
			char *k;

			for (k = l + 10; k < e; k = ae + 1) {
				getarg(k, &a, &ae);
				if (!strncasecmp(a, "server_port=", 12)) {
					a += 12;
					sport = strtoul(a, &a, 10);
					if (*a != '-')
						return -1;
					a++;
					sport2 = strtoul(a, &a, 10);
					//printf("sports = %d-%d\n", sport, sport2);
				}
			}
		} else if (!strncasecmp(l, "com.ses.streamID:", 17)) {
			*strid = strtoul(l + 17, NULL, 10);
			//printf("stream id = %d\n", *strid);
		}

	}
	return 0;
}

/****************************************************************************/
void add_fd(int fd, int *mfd, fd_set *fds)
{
	FD_SET(fd, fds);
	if (fd > *mfd)
		*mfd = fd;
}

static inline uint16_t seclen(const uint8_t *buf)
{
        return 3+((buf[1]&0x0f)<<8)+buf[2];
}

static inline uint16_t tspid(const uint8_t *buf)
{
        return ((buf[1]&0x1f)<<8)+buf[2];
}

static inline int tspayload(const uint8_t *tsp)
{
        if (!(tsp[3] & 0x10))
                return 0;
        if (tsp[3] & 0x20) {
		if (tsp[4] > 183)
			return 0;
		else
			return 183 - tsp[4];
		}
        return 184;
}


static inline int
tspaystart(const uint8_t *tsp)
{
        if (!(tsp[3]&0x10))
                return 188;
        if (tsp[3]&0x20) {
		if (tsp[4]>=184)
			return 188;
		else
			return tsp[4]+5;
		}
        return 4;
}
/****************************************************************************/

static uint32_t dvb_crc_table[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4};

uint32_t dvb_crc32(uint8_t *data, int len)
{
	int i;
	uint32_t crc=0xffffffff;

	for (i = 0; i < len; i++)
                crc = (crc << 8) ^ dvb_crc_table[((crc >> 24) ^ *data++) & 0xff];
	return crc;
}

static void pid_info_init(struct pid_info *pidi, uint16_t pid, struct ts_info *tsi)
{
	memset(pidi, 0, sizeof(struct pid_info));
	INIT_LIST_HEAD(&pidi->sfilters);
	pidi->pid=pid;
	pidi->tsi=tsi;
}

static void pid_info_release(struct pid_info *pidi)
{
	struct sfilter *p, *n;

	list_for_each_entry_safe(p, n, &pidi->sfilters, link) {
		list_del(&p->link);
		free(p);
	}
	if (pidi->buf)
		free(pidi->buf);
}

static inline void pid_info_reset(struct pid_info *pidi)
{
	pidi->bufp = pidi->len = 0;
}

static int update_pids(struct ts_info *tsi);

static int del_sfilter(struct sfilter *sf)
{
	list_del(&sf->link);
	free(sf);
	return 0;
}

int cmp_tp(struct tp_info *a, struct tp_info *b)
{
	if (a->msys != b->msys)
		return 0;
	if (a->src != b->src)
		return 0;
	if (a->freq != b->freq) {
		if (a->freq != b->freq + 1 && a->freq != b->freq - 1)
			return 0;
	}
	//~ if (a->mod != b->mod )
		//~ return 0;
	if (a->pol != b->pol )
		return 0;
	return 1;
}

int add_tp(struct scanip *sip, struct tp_info *tpi_new)
{
	struct tp_info *tpi, *p;

	list_for_each_entry(p, &sip->tps, link) {
		if (cmp_tp(p, tpi_new))
			return -1;
	}
	list_for_each_entry(p, &sip->tps_done, link) {
		if (cmp_tp(p, tpi_new))
			return -1;
	}
	tpi = malloc(sizeof(struct tp_info));
	if (!tpi)
		return -1;
	memcpy(tpi, tpi_new, sizeof(struct tp_info));
	//printf("added tp freq = %u\n", tpi->freq);
	list_add_tail(&tpi->link, &sip->tps);
	INIT_LIST_HEAD(&tpi->services);
	return 0;
}

static int add_pid(struct ts_info *tsi, uint16_t pid, int add_ext)
{
	struct pid_info *pidi = &tsi->pidi[pid];

	if (!tsi->pidi[pid].used) {
		pid_info_init(pidi, pid, tsi);
		pidi->used = 1;
		update_pids(tsi);
		list_add_tail(&pidi->link, &tsi->pids);
	}
	pidi->add_ext = add_ext;
	return 0;
}

static int add_sfilter(struct ts_info *tsi, uint16_t pid, uint8_t tid, uint16_t ext,
		       int use_ext, uint32_t timeout)
{
	struct pid_info *pidi;
	struct sfilter *sf;

	add_pid(tsi, pid, use_ext ? 1 : 0);
	pidi = &tsi->pidi[pid];

	list_for_each_entry(sf, &pidi->sfilters, link) {
			if (sf->tid == tid && sf->ext == ext)
				return -1;
	}
	sf = calloc(1, sizeof(struct sfilter));
	if (!sf)
		return -1;
	sf->pidi = pidi;
	sf->tid = tid;
	sf->ext = ext;
	sf->vnr = 0xff;
	sf->timeout_len = timeout;
	sf->timeout = mtime(NULL) + sf->timeout_len;
	list_add_tail(&sf->link, &pidi->sfilters);
	list_add_tail(&sf->tslink, &pidi->tsi->sfilters);
	//printf("add_sfilter PID=%u TID=%u EXT=%u\n", pidi->pid, tid, ext);
	return 0;
}

void ts_info_init(struct ts_info *tsi)
{
	int i;

	INIT_LIST_HEAD(&tsi->pids);
	INIT_LIST_HEAD(&tsi->sfilters);
	for (i=0; i<0x2000; i++)
		pid_info_init(&tsi->pidi[i], i, tsi);
}

void ts_info_release(struct ts_info *tsi)
{
	int i;

	for (i=0; i<0x2000; i++)
		pid_info_release(&tsi->pidi[i]);
}

static uint32_t getbcd(uint8_t *p, int l)
{
	int i;
	uint32_t val = 0, t;

	for (i = 0; i < l / 2; i++) {
		t = (p[i] >> 4) * 10 + (p[i] & 0x0f);
		val = val * 100 + t;
	}
	if (l & 1)
		val = val * 10 + (p[i] >> 4);
	return val;
}

static uint16_t get16(uint8_t *p)
{
	return (p[0] << 8) | p[1];
}

static uint16_t get12(uint8_t *p)
{
	return ((p[0] & 0x0f) << 8) | p[1];
}

static uint16_t get_pid(uint8_t *pid)
{
        uint16_t pp;

        pp = (pid[0] & 0x1f) << 8;
        pp |= pid[1] &0xff;
        return pp;
}

static int get_desc(struct pid_info *p, uint8_t *buf, int length)
{
 	int dlength;
 	int c=0;
 	uint16_t casys;
 	uint16_t capid;

 	while (c < length) {
 		dlength = buf[c+1];

 		switch(buf[c]){
 		case 0x02:
 			break;
 		case 0x03:
 			break;
 		case 0x09:
 			if (!dlength)
				break;
 			casys =(buf[c+2]<<8)|buf[c+3];
 			capid = get_pid(buf+c+4);
			break;
		default:
			break;
		}
		c += dlength + 2;
	}
	return length;
}

static int update_pids(struct ts_info *tsi)
{
	struct satipcon *scon = &tsi->stp->scon;
	uint8_t buf[1024];
	char pids[512];
	int len, len2, plen;
	uint32_t pid;

	//printf("tune=%s\n", &scon->tune[0]);
	for (pid = 0, plen = 0; pid < 8192; pid++) {
		if (tsi->pidi[pid].used) {
			len2 = snprintf(pids + plen, sizeof(pids) - plen,
					",%u", pid);
			if (len2 < 0)
				return -1;
			if (plen + len2 >= 300)
				break;
			plen += len2;
		}
	}
	pids[0] = '=';
	if (!plen)
		snprintf(pids, sizeof(pids), "=none");

	//printf("pids%s\n",pids);

	len = snprintf(buf, sizeof(buf),
		       "PLAY rtsp://%s:%s/stream=%u?%s&pids%s RTSP/1.0\r\n"
		       "CSeq: %d\r\n"
		       "Session: %s\r\n"
		       "\r\n",
		       scon->host, scon->port, scon->strid, &scon->tune[0], pids,
		       scon->seq , scon->sid);
	scon->seq++;
	if (len > 0 && len < sizeof(buf)) {
		sendlen(scon->sock, buf, len);
		//printf("Send: %s\n", buf);
	}
}

static int hasdesc(uint8_t stag, uint8_t *b, int dll)
{
	int i;

	for (i = 0; i < dll; i += b[i + 1] + 2) {
		if (stag == b[i])
			return 1;
	}
	return 0;
}


static int pmt_cb(struct sfilter *sf)
{
	struct pid_info *p = sf->pidi;
	uint8_t *buf=p->buf;
	int slen, ilen, eslen, c;
	uint16_t epid, pnr;
	struct service *s;

	slen = get12(buf + 1) + 3;
	pnr = get16(buf + 3);
	if (pnr != sf->ext)
		return 0;
	//fprintf(stderr, "PMT %04x:  PNR %04x\n", p->pid, pnr);
	//fprintf(stderr, " snr %02x lsnr %02x", buf[6], buf[7]);
	//dump(buf, p->len);
	c = 12;
	if ((ilen = get12(buf + 10)))
		c += get_desc(p, buf + c, ilen);
	if (c != 12 + ilen)
 		return 0;
	s = get_service(p->tsi->stp->tpi, pnr);
	s->pcr = get_pid(buf + 8);
	s->anum = 0;
	s->pmt = p->pid;
   s->is_enc = 0;
	while (c < slen - 4) {
		eslen = get12(buf + c + 3);
		epid = get_pid(buf + c + 1);
		//fprintf(stderr, " TYPE %02x PID %04x len %u\n", buf[c], epid, eslen, eslen);
		//dump(buf + c, eslen + 5);
		switch (buf[c]) {
		case 0x01: // MPEG1
		case 0x02: // MPEG2
		case 0x10: // MPEG4
		case 0x1b: // H264
		case 0x24: // HEVC
		case 0x42: // CAVS
		case 0xea: // VC1
		case 0xd1: // DIRAC
			s->vpid = epid;
			if (hasdesc(0x09, buf + c + 5, eslen))
				s->is_enc = 1;
			break;
		case 0x03: // MPEG1
		case 0x04: // MPEG2
		case 0x0F: // AAC
		case 0x11: // AAC_LATM
		case 0x81: // AC3
		case 0x82: // DTS
		case 0x83: // TRUEHD
			if (s->anum < MAX_ANUM)
				s->apid[s->anum++] = epid;
			//fprintf(stderr, "  APID %04x", epid);
			if (hasdesc(0x09, buf + c + 5, eslen))
				s->is_enc = 1;
			break;
		case 0x06:
			if (hasdesc(0x56, buf + c + 5, eslen))
				s->ttx = epid;
			else if (hasdesc(0x59, buf + c + 5, eslen))
			        s->sub = epid;
			else if (hasdesc(0x6a, buf + c + 5, eslen)) {
				if (s->anum < MAX_ANUM)
					s->apid[s->anum++] = epid;
			}
			if (hasdesc(0x09, buf + c + 5, eslen))
				s->is_enc = 1;
			break;
		case 0x05: // PRIVATE
			break;
		default:
			break;
		}
		c += 5;
		c += eslen;
		//if (eslen)
		//	c+=get_desc(p, buf+c, eslen);
	}
	s->got_pmt = 1;
	//fprintf(stderr, "\n");
	return 0;

}

static int nit_cb(struct sfilter *sf)
{
	struct pid_info *p = sf->pidi;
	struct scanip *sip = p->tsi->stp->sip;

	uint8_t *buf = p->buf;
	int slen, tsp, c;
	uint16_t pid, pnr, nid;
	uint16_t ndl, tsll, tdl;
	uint16_t tsid, onid;
	struct tp_info t;

	slen = get12(buf + 1) + 3;
	nid = get16(buf + 3);
	ndl = get12(buf + 8);
	tsp = 10 + ndl;
	for (c = 10; c < tsp; c++) {

	}
	tsll = get12(buf + tsp);
	//fprintf(stderr, "NIT(%02x): len %u nid %u snr %02x lsnr %02x", buf[0], slen, nid, buf[6], buf[7]);
	//fprintf(stderr, " ndl %02x  tsll %02x\n", ndl, tsll);

	for (c = tsp + 2; c < slen; c += tdl) {
		//dump(buf + c + 6, tdl);
      memset(&t, 0, sizeof(struct tp_info));
		t.tsid = get16(buf + c);
		t.onid = get16(buf + c + 2);
		t.nid = nid;
		tdl = get12(buf + c + 4);
		//fprintf(stderr, " tsid %02x onid %02x tdl %02x\n", tsid, onid, tdl);
		c += 6;
		switch (buf[c]) {
		case 0x43:
			t.freq = getbcd(buf + c + 2, 8) / 100;
			t.freq_frac = 0;
			t.pos = getbcd(buf + c + 6, 4);
			t.sr = getbcd(buf + c + 9, 7) / 10;
			t.east = (buf[c + 8] & 0x80) >> 7;
			t.pol = 1 ^ ((buf[c + 8] & 0x60) >> 5); // H V L R
			t.ro = (buf[c + 8] & 0x18) >> 3;  // 35 25 20
			t.type = t.msys = ((buf[c + 8] & 0x04) >> 2) ? 6 : 5;
			t.mod = buf[c + 8] & 0x03; // auto qpsk 8psk 16-qam
			t.fec = buf[c + 12] & 0x0f; // undef 1/2 2/3 3/4 5/6 7/8 8/9 3/5 4/5 9/10
			//fprintf(stderr, " freq = %u  pos = %u  sr = %u  fec = %u  \n", freq, pos, sr, fec);
			//fprintf(stderr, "freq=%u&pol=%s&msys=%s&sr=%u\n",
			//t.freq, pol2str[t.pol&3], t.type == 6 ? "dvbs2" : "dvbs", t.sr);
			add_tp(sip, &t);
			break;
		case 0x44:
			{
				uint32_t freq = getbcd(buf + c + 2, 8);
				t.freq =  freq / 10000;
				t.freq_frac =  freq % 10000;
			}
			t.sr = getbcd(buf + c + 9, 7) / 10;
			t.mod = buf[c + 8]; // undef 16 32 64 128 256
			t.msys = 1;
			t.type = 1;
			//fprintf(stderr, " freq = %u  pos = %u  sr = %u  fec = %u  \n", freq, pos, sr, fec);
			//fprintf(stderr, "freq=%u&msys=dvbc&mtype=%s\n", t.freq, mtype2str[t.mod]);

			if( t.freq >= 50 && t.freq <= 1000 && t.sr >= 1000 && t.sr <= 7100 && t.mod >= 1 && t.mod <= 5 )
				add_tp(sip, &t);
			else {
				fprintf(stderr, " *************************  freq = %u  sr = %u  mod = %u  \n", t.freq, t.sr, t.mod);
				fprintf(stderr, " *************************  buffer start:\n" );
				dump(buf, 16);
				fprintf(stderr, " *************************  buffer position c = %d\n", c);
				dump(buf + c, 16);
			}
			break;

		}
	}

	return 0;
}


static int pat_cb(struct sfilter *sf)
{
	struct pid_info *p = sf->pidi;
	uint8_t *buf = p->buf;
	int slen, ilen, eslen, c;
	uint16_t pid, pnr;
	uint8_t snr, lsnr;

	slen = (((buf[1]&0x03)<<8)|buf[2])+3;
	sf->ext = ((buf[3]<<8)|buf[4]);
	p->tsi->tsid = sf->ext;

	//fprintf(stderr, "PAT:  TSID %04x sn %u lsn %u todo %08x vnr %u", p->tsi->tsid, snr, lsnr, sf->todo, sf->vnr);
	c = 8;
	for (c = 8; c < slen - 4; c +=4) {
		pnr = (buf[c] << 8) | buf[c + 1];
		pid = get_pid(buf + c + 2);
		//fprintf(stderr, " PNR %04x PID %04x", pnr, pid);
		if (pnr) {
			add_sfilter(p->tsi, pid, 0x02, pnr, 2, 5);
			add_sfilter(p->tsi, 0x11, 0x42, pnr, 2, 5);
		} else {
			add_sfilter(p->tsi, pid, 0x40, 0, 1, 15);
			add_sfilter(p->tsi, pid, 0x41, 0, 1, 15);
		}
	}
	//fprintf(stderr, "\n");
	return 0;
}

#define UTF8_CC_START 0xc2
#define SB_CC_RESERVED_80 0x80
#define SB_CC_RESERVED_81 0x81
#define SB_CC_RESERVED_82 0x82
#define SB_CC_RESERVED_83 0x83
#define SB_CC_RESERVED_84 0x84
#define SB_CC_RESERVED_85 0x85
#define CHARACTER_EMPHASIS_ON 0x86
#define CHARACTER_EMPHASIS_OFF 0x87
#define SB_CC_RESERVED_88 0x88
#define SB_CC_RESERVED_89 0x89
#define CHARACTER_CR_LF 0x8a
#define SB_CC_USER_8B 0x8b
#define SB_CC_USER_9F 0x9f

void en300468_parse_string_to_utf8(char *dest, uint8_t *src,
		const unsigned int len)
{
	int utf8 = (src[0] == 0x15) ? 1 : 0;
	int skip = (src[0] < 0x20) ? 1 : 0;
	if( src[0] == 0x10 ) skip += 2;
	uint16_t utf8_cc;
	int dest_pos = 0;
	int emphasis = 0;
	int i;

	for (i = skip; i < len; i++) {
		switch(*(src + i)) {
			case SB_CC_RESERVED_80 ... SB_CC_RESERVED_85:
			case SB_CC_RESERVED_88 ... SB_CC_RESERVED_89:
			case SB_CC_USER_8B ... SB_CC_USER_9F:
			case CHARACTER_CR_LF:
				dest[dest_pos++] = '\n';
				continue;
			case CHARACTER_EMPHASIS_ON:
				emphasis = 1;
				continue;
			case CHARACTER_EMPHASIS_OFF:
				emphasis = 0;
				continue;
			case UTF8_CC_START:
				if (utf8 == 1) {
					utf8_cc = *(src + i) << 8;
					utf8_cc += *(src + i + 1);

					switch(utf8_cc) {
						case ((UTF8_CC_START << 8) | CHARACTER_EMPHASIS_ON):
							emphasis = 1;
							i++;
							continue;
						case ((UTF8_CC_START << 8) | CHARACTER_EMPHASIS_OFF):
							emphasis = 0;
							i++;
							continue;
						default:
							break;
					}
				}
			default: {
				if (*(src + i) < 128)
					dest[dest_pos++] = *(src + i);
				else {
					dest[dest_pos++] = 0xc2 + (*(src + i) > 0xbf);
					dest[dest_pos++] = (*(src + i) & 0x3f) | 0x80;
				}
				break;
			}
		}
	}
	dest[dest_pos] = '\0';
}

static void sscopy(char *b, char *a, int len)
{
	while (len--) {
		if (*a > 0x20 && (*a < 0x80 || *a > 0x9f))
			*b++ = *a;
		a++;
	}
	*b = 0;
}


static int sdt_cb(struct sfilter *sf)
{
	struct pid_info *p = sf->pidi;
	uint8_t *buf=p->buf, tag;
	int c, dll, dl, d, doff;
	uint16_t onid, sid, tsid;
	struct service *s;

	tsid = get16(buf + 3);
	onid = get16(buf + 8);
	for (c = 11; c < p->len - 4; c += dll + 5) {
		int spnl, snl;

		sid = get16(buf + c);
		dll = get12(buf + c + 3);

		s = get_service(p->tsi->stp->tpi, sid);
		//printf("sid = %04x, dll = %u\n", sid, dll);
		for (d = 0; d < dll; d += dl + 2) {
			doff = c + d + 5;
			tag = buf[doff];
			dl = buf[doff + 1];
			//printf("desc %02x: %u\n", tag, dl);
			if (tag == 0x48) {
				spnl = buf[doff + 3];
				snl = buf[doff + 4 + spnl];
            s->pname[79] = 0x00;
            s->name[79] = 0x00;
				en300468_parse_string_to_utf8(s->pname, buf + doff + 4, spnl);
            if( s->pname[79] != 0 )
               printf("********************************************* PNAME OVERFLOW %d spnl = %d",spnl);
				en300468_parse_string_to_utf8(s->name, buf + doff + 5 + spnl, snl);
            if( s->name[79] != 0 )
               printf("********************************************* SNAME OVERFLOW %d snl = %d",snl);
				s->got_sdt = 1;
			}
		}
	}

	return 0;
}

static int all_zero_8(uint32_t *p)
{
	return (p[0] | p[1] | p[2] | p[3] | p[4] | p[5] | p[6] | p[7]) ? 0 : 1;
}

static int proc_sec(struct pid_info *p)
{
	uint8_t *buf=p->buf;
	uint8_t snr, vnr, lsnr, tid;
	struct sfilter *sf, *sfn;
	uint16_t ext;
	int i, res;

	tid = buf[0];
	ext = ((buf[3] << 8) | buf[4]);
	vnr = (buf[5] & 0x3f) >> 1;
	snr = buf[6];
	lsnr = buf[7];

	list_for_each_entry_safe(sf, sfn, &p->sfilters, link) {
		if (tid != sf->tid)
			continue;
		if (p->add_ext) {
			if (sf->use_ext == 2) {
				if (ext != sf->ext)
					continue;
			} else {
				sf->ext = ext;
				sf->use_ext = 2;
			}
		}
		if (!sf->vnr_set) {
			sf->vnr = vnr;
			sf->vnr_set = 1;
		}
		if (sf->vnr != vnr) {
			printf("TID %u ext %u\n", tid, ext);
			printf("VNR change %u->%u\n", sf->vnr, vnr);

			sf->todo_set = 0;
			sf->vnr = vnr;
			//sf->done = 0;
		}
		if (sf->done)
			break;
		if (!sf->todo_set) {
			for (i = 0; i <= lsnr; i++)
				sf->todo[i >> 5] |= (1UL << (i & 31));
			sf->todo_set = 1;
		}
		switch (tid) {
		case 0x00:
			res = pat_cb(sf);
			break;
		case 0x02:
			res = pmt_cb(sf);
			break;
		case 0x40:
		case 0x41:
			if( use_nit )
            res = nit_cb(sf);
			break;
		case 0x42:
		case 0x46:
			res = sdt_cb(sf);
			break;
		default:
			break;
		}
		if (res == 0) {
			sf->todo[snr >> 5] &= ~(1UL << (snr & 31));
			if (all_zero_8(sf->todo)) {
				sf->done = 1;
				list_del(&sf->tslink);
			} else
				sf->timeout = mtime(NULL) + sf->timeout_len;
			break;
		}
	}
	if (&sf->link == &p->sfilters)
		return -1;
	return 0;
}



static int pid_info_proc_section(struct pid_info *p)
{
	struct sfilter *sf, *sfn;
	uint8_t *buf = p->buf;
	uint8_t tid;
	uint16_t ext;
	int res;

        if (p->bufp != p->len) {
		if (p->len && p->bufp > p->len)
			goto exit;
		return 0;
	}
	if (buf[1] & 0x80) {
		if (dvb_crc32(buf, p->len)) {
			fprintf(stderr, "CRC error pid %04x!\n", p->pid);
			goto exit;
		}
	}

	//fprintf(stderr, "PID %04x SEC[%d]: %02x\n", (int) p->pid, p->len, (int)p->buf[0]);
	if (p->len < 8)
		return 0;
	if (!(buf[5] & 1))
		return 0;


	tid = buf[0];
	ext = ((buf[3] << 8) | buf[4]);

	res = proc_sec(p);

	if (res && p->add_ext) {
		if (tid == 0x42 || tid == 0x02) {
			printf("section not matched");
			printf("adding %02x:%04x\n", tid, ext);
			//add_sfilter(p->tsi, p->pid, tid, ext, 1, 5);
			//proc_sec(p);
		}
	}
exit:
	pid_info_reset(p);
	return 0;
}

/****************************************************************************/
/****************************************************************************/

static inline void write_secbuf(struct pid_info *p, uint8_t *tsp, int n)
{
	memcpy(p->buf+p->bufp, tsp, n);
	p->bufp += n;
}

static inline int validcc(struct pid_info *p, uint8_t *tsp)
{
	uint8_t newcc;
	int valid;

	newcc = tsp[3] & 0x0f;
        valid = (((p->cc + 1) & 0x0f) == newcc) ? 1 : 0;
	if (p->cc == 0xff)
		valid=1;
        p->cc = newcc;
	if (!valid) {
		fprintf(stderr, "CC error PID %04x!\n", p->pid);
		pid_info_reset(p);
	}
	return valid;
}

static inline int pid_info_build_section(struct pid_info *p, uint8_t *tsp)
{
        int pusoff, todo = tspayload(tsp), i = 188 - todo;

        if (!todo)
                return -1;
	pusoff = (tsp[1] & 0x40) ? tsp[i++] : todo;
	if (pusoff + i > 188)
		goto error;
	if (validcc(p, tsp) && pusoff && p->bufp) {
	        int rlen = pusoff;
		if (p->len) {
			if (p->bufp + rlen > p->len)
				rlen = p->len - p->bufp;
		} else
			if (p->bufp + rlen > 4096)
				rlen = 4096 - p->bufp;
		write_secbuf(p, tsp + i, rlen);
		if (!p->len && p->bufp >= 3 && (p->len = seclen(p->buf)) > 4096)
			pid_info_reset(p);
		else
			pid_info_proc_section(p);
	}
	i += pusoff;
	while ((todo = 188 - i) > 0 && tsp[i] != 0xff) {
	        pid_info_reset(p);
		if (todo < 3)
			fprintf(stderr, "sec start <3 \n");
		if (todo < 3 || (p->len = seclen(tsp+i)) > todo) {
			if (p->len > 4096)
				goto error;
			write_secbuf(p, tsp+i, todo);
			i+=todo;
		} else {
			write_secbuf(p, tsp+i, p->len);
			i+=p->len;
			pid_info_proc_section(p);
		}
	}
	return 0;

error:
	fprintf(stderr, "error\n");
	pid_info_reset(p);
	return -1;
}

/****************************************************************************/

void proc_tsp(struct ts_info *tsi, uint8_t *tsp)
{
        uint16_t pid = 0x1fff & ((tsp[1] << 8) | tsp[2]);
	struct pid_info *pidi = &tsi->pidi[pid];

	if (!pidi->used)
		return;

	if (!pidi->buf) {
		pidi->buf = malloc(4096);
		if (!pidi->buf)
			return;
		pidi->cc = 0xff;
	}
	pid_info_build_section(pidi, tsp);
}

void proc_tsps(struct ts_info *tsi, uint8_t *tsp, uint32_t len)
{
	time_t mt = mtime(NULL);
	struct sfilter *sf, *sfn;

	list_for_each_entry_safe(sf, sfn, &tsi->sfilters, tslink) {
		if (sf->done || mt > sf->timeout) {
			sf->done = 1;
			list_del(&sf->tslink);
		}
	}
	if (list_empty(&tsi->sfilters))
		tsi->done = 1;
#if 0
	if (list_empty(&tsi->sfilters)) {
		if (mt > tsi->timeout)
			tsi->done = 1;
	} else
		tsi->timeout = mt + 1;
#endif
	while (len >= 188) {
		proc_tsp(tsi, tsp);
		tsp += 188;
		len -= 188;
	}
}

/****************************************************************************/
/****************************************************************************/

static void dump_tp(struct tp_info *tpi)
{
	struct service *s;
	int i;

	list_for_each_entry(s, &tpi->services, link) {
		if (s->got_pmt && (s->vpid != 0 || s->anum>0)) {
			printf("BEGIN\n");
			printf(" PNAME:%s\n",s->pname);
			printf(" SNAME:%s\n",s->name);
			printf(" SID:%d\n",s->sid);
			printf(" PIDS:%d",s->pmt);
			uint16_t pcr = s->pcr;
			if (s->pmt == pcr) pcr = 0;
			if (s->vpid != 0 ) {
				printf(",%d",s->vpid);
				if (s->vpid == pcr) pcr = 0;
			}
			for (i= 0; i < s->anum; i+=1) {
				if (s->apid[i] != 0 ) {
					printf(",%d",s->apid[i]);
					if (s->apid[i] == pcr) pcr = 0;
				}
			}
			if (s->sub != 0 ) {
				printf(",%d",s->sub);
				if (s->sub == pcr) pcr = 0;
			}
			if (s->ttx != 0 ) {
				printf(",%d",s->ttx);
				if (s->ttx == pcr) pcr = 0;
			}
			if (pcr != 0) {
				printf(",%d",pcr);
			}
			printf("\n");
			if (s->anum > 0 && s->apid[0] != 0) {
				printf(" APIDS:%d",s->apid[0]);
				for (i= 1; i < s->anum; i+=1) {
					if (s->apid[i] != 0 ) {
						printf(",%d",s->apid[i]);
					}
				}
			}
			printf("\n");
			if ( s->vpid == 0 )
				printf(" RADIO:1\n");
			if ( s->is_enc )
				printf(" ENC:1\n");
			printf("END\n");
			fflush(stdout);
		}
		//~ if (!s->got_pmt)
			//~ printf("NO PMT: ");
		//~ printf("%s:%s sid=%04x pmt=%04x pcr=%04x vpid=%04x apid=%04x\n",
		       //~ s->pname, s->name, s->sid, s->pmt, s->pcr, s->vpid, s->apid[0]);
	}
}


static int scan_tp(struct scantp *stp)
{
	struct scanip *sip= stp->sip;
	struct satipcon *scon = &stp->scon;
	fd_set fds;
	struct timeval timeout;
	int mfd, num, n;
	time_t t, u;
	char buf[2048];
	struct sockaddr sadr;
	char *a;
	int rbuf = 1024 * 1024;

	scon->seq = 0;
	scon->usock = udpsock(&sadr, "0");
	if (scon->usock < 0) {
		printf("Could not get UDP socket\n");
		return -1;
	}
	//setsockopt(usock, SOL_SOCKET, SO_RCVBUF, &rbuf, sizeof(rbuf));
	scon->nsport = 0;//strtoul(sport, NULL, 10);
	if (scon->nsport == 0) {
		struct sockaddr_in sin;
		socklen_t len = sizeof(sin);
		getsockname(scon->usock, (struct sockaddr*) &sin, &len);
		scon->nsport = ntohs(sin.sin_port);
	}
	//printf("Socket port = %u\n", scon->nsport);
	//printf("host = %s, port = %s\n", scon->host, scon->port);

	scon->sock = streamsock(scon->host, scon->port, &sadr);
	if (scon->sock < 0)
		return scon->sock;

	send_setup(scon->sock, scon->host, scon->port, scon->tune, &scon->seq, scon->nsport, 0);
	if (check_ok(scon->sock, scon->sid, &scon->strid) < 0)
		return 0;
	update_pids(&stp->tsi);
	if (check_ok(scon->sock, scon->sid, &scon->strid) < 0)
		return 0;

	add_sfilter(&stp->tsi, 0x00, 0x00, 0, 0, 5);
	add_sfilter(&stp->tsi, 0x11, 0x42, 0, 1, 5);
	//add_sfilter(p->tsi, 0x11, 0x46, 0, 1, 15);

	stp->timeout = mtime(NULL) + 10;
	while (!done && !stp->tsi.done && mtime(NULL) < stp->timeout) {
		mfd = 0;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		FD_ZERO(&fds);
		add_fd(scon->sock, &mfd, &fds);
		add_fd(scon->usock, &mfd, &fds);
		num = select(mfd + 1, &fds, NULL, NULL, &timeout);
		if (num < 0)
			break;
		if (FD_ISSET(scon->sock, &fds)) {
			n = recv(scon->sock, buf, sizeof(buf), 0);
			//dump(buf, n);
		}
		if (FD_ISSET(scon->usock, &fds)) {
			n = recvfrom(scon->usock, buf, sizeof(buf), 0, 0, 0);
			if (n > 12) {
				proc_tsps(&sip->stp.tsi, buf + 12, n - 12);
				// stp->timeout = mtime(NULL) + 10;
				stp->timeout += 60;
			}
		}
	}
	dump_tp(stp->tpi);
	a = 0;
	send_teardown(scon->sock, scon->host, scon->port, scon->strid, &scon->seq, scon->sid);
	close(scon->sock);
}


void tpstring(struct tp_info *tpi, char *s, int slen)
{
	int len;

   switch (tpi->msys) {
   case 1:
      if( tpi->freq_frac )
         len = snprintf(s, slen,
                   "freq=%u.%04u&msys=dvbc&sr=%u&mtype=%s",
                   tpi->freq, tpi->freq_frac, tpi->sr, mtype2str[tpi->mod]);
      else
         len = snprintf(s, slen,
                   "freq=%u&msys=dvbc&sr=%u&mtype=%s",
                   tpi->freq, tpi->sr, mtype2str[tpi->mod]);
      break;
   case 5:
   case 6:
      len = snprintf(s, slen,
                "src=%u&freq=%u&pol=%s&msys=%s&sr=%u",
                tpi->src,tpi->freq, pol2str[tpi->pol&3],
                msys2str[tpi->msys], tpi->sr);
      break;
   }
}


static int scanip(struct scanip *sip)
{
	struct scantp *stp;
	struct ts_info *tsi;
	struct tp_info *tpi;

	while (!done && !list_empty(&sip->tps)) {
		stp = &sip->stp;
		memset(stp, 0, sizeof(struct scantp));

		tsi = &stp->tsi;
		ts_info_init(tsi);
		stp->sip = sip;
		stp->scon.port = "554";
		stp->scon.host = sip->host;
		tsi->stp = stp;

		tpi = list_first_entry(&sip->tps, struct tp_info, link);
		tpstring(tpi, &stp->scon.tune[0], sizeof(stp->scon.tune));
		printf("\nTUNE:%s\n", stp->scon.tune);
      fflush(stdout);
		stp->tpi = tpi;
   		scan_tp(stp);
		ts_info_release(tsi);
		list_del(&tpi->link);
		list_add(&tpi->link, &sip->tps_done);
	}
}

void term_action(int sig, siginfo_t *si, void *d)
{
	done = 1;
}


void scanip_init(struct scanip *sip, char *host)
{
	INIT_LIST_HEAD(&sip->tps);
	INIT_LIST_HEAD(&sip->tps_done);
	sip->done = 0;
	sip->host = host;
}

void scanip_release(struct scanip *sip)
{
	struct tp_info *p, *n;

	list_for_each_entry_safe(p, n, &sip->tps, link) {
		list_del(&p->link);
		free(p);
	}
	list_for_each_entry_safe(p, n, &sip->tps_done, link) {
		list_del(&p->link);
		free(p);
	}
}

void scan_cable(struct scanip *sip)
{
	struct tp_info tpi = {
		.freq = 130,
		.msys = 1,
		.mod = 5,
	};
	uint32_t f, m;

	for (f = 114; f < 800; f += 8)
		for (m = 5; m < 6; m += 2) {
			tpi.freq = f;
			tpi.mod = m;
			add_tp(sip, &tpi);
		}
}

void usage() {
	printf("Octoscan"
	       ", Copyright (C) 2016 Digital Devices GmbH\n\n");
	printf("octoscan [options] <server ip>\n");
	printf("    <server ip> address of SAT>IP server\n");
	printf("\n");
	printf("  options:\n");
	printf("    --use_nit, -n\n");
	printf("       Use network information table\n");
	printf("       if not specified only a single transponder is scanned\n");
	printf("    --freq=<frequency>, -f <frequency>\n");
	printf("       frequency in MHz  (required)\n");
	printf("    --sr=<symbolrate>, -s <symbolrate>\n");
	printf("       symbolrate in kSymbols (required for DVB-S/S2 and DVB-C)\n");
	printf("           DVB-S/S2 example: --sr=27500\n");
	printf("           DVB-C example:    --sr=6900\n");
	printf("    --pol=<polarisation>, -p <polarisation>\n");
	printf("       polarisation = v,h,r,l (required for DVB-S/S2)\n");
	printf("           example: --pol=v\n");
	printf("    --msys=<modulation system>, -m <modulation system>\n");
	printf("       system = dvbs,dvbs2,dvbc (required)\n");
	printf("           example: --msys=dvbs\n");
	printf("    --mtype=<modulation type>, -t <modulation type>\n");
	printf("       modulation type = 16qam,32qam,64qam,128qam,256qam (required for DVB-C)\n");
	printf("    --help, -?\n");
	printf("\n");
	printf("  Notes on NIT scanning:\n");
	printf("    NIT scanning is currently not reliable for DVB-S/S2 (to be fixed).\n");
	printf("    With some cable providers or inhouse retransmission systems\n");
	printf("    it may be not usable, i.e. due to wrong frequencies in the NIT.\n");
	printf("\n");
	printf("  Notes on hardware depencies:\n");
	printf("    Depending on hardware configuration the scan will succeed even if\n");
	printf("    some required parameters are wrong. This will result in a channel list\n");
	printf("    which is usable only on the same hardware configuration.\n");
	printf("\n");
	printf("  Example: NIT based scan which should work on Unitymedia in Germany\n");
	printf("    octoscan --use_nit --freq=138 --msys=dvbc --sr=6900 --mtype=256qam 10.0.4.24\n");
}

int main(int argc, char **argv)
{
	struct sigaction term;
	struct scanip sip;
	struct tp_info tpi;
	int i;

#if 0
	struct tp_info tpi1 = {
		.freq = 11836,
		.pol = 1,
		.msys = 5,
		.sr = 27500,
	};
	struct tp_info tpi2 = {
		.freq = 11914,
		.pol = 1,
		.msys = 6,
		.sr = 27500,
	};

	struct tp_info tpi3 = {
		.freq = 138,
		.msys = 1,
		.mod = 5,
		.sr = 6900,
	};
	struct tp_info tpi4 = {
		.freq = 410,
		.msys = 1,
		.mod = 5,
		.sr = 6900,
	};
#endif

	if(argc < 2) {
		usage();
		exit(0);
	};

	memset(&tpi, 0, sizeof(struct tp_info));

	while (1) {
		int option_index = 0;
		int c;
		static struct option long_options[] = {
			{"use_nit", no_argument, 0, 'n'},
			{"freq", required_argument, 0, 'f'},
			{"sr", required_argument, 0, 's'},
			{"pol", required_argument, 0, 'p'},
			{"msys", required_argument, 0, 'm'},
			{"mtype", required_argument, 0, 't'},
			{"help", no_argument , 0, '?'},
			{0, 0, 0, 0}
		};
		c = getopt_long(argc, argv,
				"nf:s:p:m:t:?",
				long_options, &option_index);
		if (c==-1)
			break;

		switch (c) {
		case 'n':
			use_nit = 1;
			break;

		case 'f':
			tpi.freq = strtoul(optarg, NULL, 10);
			break;

		case 's':
			tpi.sr = strtoul(optarg, NULL, 10);
			break;

		case 'S':
			tpi.src = strtoul(optarg, NULL, 10);
			break;

		case 'p':
			i = 0;
			while( i < 4 ) {
				if( strcmp(optarg,pol2str[i]) == 0 ) {
					tpi.msys = i;
					break;
				}
				i += 1;
			}
			break;

		case 'm':
			i = 0;
			while( msys2str[i] ) {
				if( strcmp(optarg,msys2str[i]) == 0 ) {
					tpi.msys = i;
					break;
				}
				i += 1;
			}
			break;

 		case 't':
			i = 0;
			while( mtype2str[i] ) {
				if( strcmp(optarg,mtype2str[i]) == 0 ) {
					tpi.mod = i;
					break;
				}
				i += 1;
			}
			break;

		case '?':
			usage();
			exit(0);
		default:
			break;

		}
	}

	if( optind != argc - 1 ) {
		printf("wrong number of arguments\n\n");
		usage();
		exit(-1);
	}

	memset(&term, 0, sizeof(term));
	term.sa_sigaction = term_action;
	sigemptyset(&term.sa_mask);
	term.sa_flags = 0;

	sigaction(SIGINT, &term, NULL);

	scanip_init(&sip, argv[optind]);
	add_tp(&sip, &tpi);
	//scan_cable(&sip);
	scanip(&sip);
	scanip_release(&sip);
}

