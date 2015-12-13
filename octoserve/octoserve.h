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

#ifndef _OCTOSERVE_H_
#define _OCTOSERVE_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <pthread.h>
#include <ifaddrs.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <netdb.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <netinet/tcp.h>

#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/video.h>
#include <linux/dvb/net.h>
#include <linux/mii.h>
#include <linux/sockios.h>
#include <linux/filter.h>
#include <time.h>

#include <libdvben50221/en50221_stdcam.h>

#include "version.h"
#include "ns.h"

#define DEBUG_RTSP     1
#define DEBUG_SSDP     2
#define DEBUG_NET      4
#define DEBUG_SYS      8
#define DEBUG_DVB     16
#define DEBUG_IGMP    32
#define DEBUG_SWITCH  64

#if 0
#define dbgprintf(_mask_, ...) \
	 do { if (debug & _mask_) fprintf(stderr, __VA_ARGS__); } while (0) 
#else
#define dbgprintf(_mask_, ...) \
         do { if (debug & _mask_) { fprintf(stderr, "[%5u] ", mtime(NULL)); \
			            fprintf(stderr, __VA_ARGS__); } } while (0) 
#endif

#define SYS_DVBC2 19

struct ddb_reg {
	__u32 reg;
	__u32 val;
};

struct ddb_id {
	__u16 vendor;
	__u16 device;
	__u16 subvendor;
	__u16 subdevice;
	__u32 hw;
	__u32 regmap;
};

struct ddb_mdio {
	__u8   adr;
	__u8   reg;
	__u16  val;
};

struct ddb_mem {
	__u32  off;
	__u8  *buf;
	__u32  len;
};

#define DDB_MAGIC 'd'
#define IOCTL_DDB_FLASHIO    _IOWR(DDB_MAGIC, 0x00, struct ddb_flashio)
#define IOCTL_DDB_GPIO_IN    _IOWR(DDB_MAGIC, 0x01, struct ddb_gpio)
#define IOCTL_DDB_GPIO_OUT   _IOWR(DDB_MAGIC, 0x02, struct ddb_gpio)
#define IOCTL_DDB_ID         _IOR(DDB_MAGIC, 0x03, struct ddb_id)
#define IOCTL_DDB_READ_REG   _IOWR(DDB_MAGIC, 0x04, struct ddb_reg)
#define IOCTL_DDB_WRITE_REG  _IOW(DDB_MAGIC, 0x05, struct ddb_reg)
#define IOCTL_DDB_READ_MEM   _IOWR(DDB_MAGIC, 0x06, struct ddb_mem)
#define IOCTL_DDB_WRITE_MEM  _IOR(DDB_MAGIC, 0x07, struct ddb_mem)
#define IOCTL_DDB_READ_MDIO  _IOWR(DDB_MAGIC, 0x08, struct ddb_mdio)
#define IOCTL_DDB_WRITE_MDIO _IOR(DDB_MAGIC, 0x09, struct ddb_mdio)

struct rtsp_error {
	int number;
	char *name;
};

#define PARAM_STREAMID   0
#define PARAM_FE         1
#define PARAM_SRC        3
#define PARAM_FEC        4
#define PARAM_FREQ       5
#define PARAM_SR         6
#define PARAM_POL        7
#define PARAM_RO         8

#define PARAM_MSYS       9

#define PARAM_MTYPE     10
#define PARAM_PLTS      11
#define PARAM_BW        12
#define PARAM_BW_HZ     13
#define PARAM_TMODE     14
#define PARAM_GI        15
#define PARAM_PLP       16
#define PARAM_ISI       16
#define PARAM_T2ID      17
#define PARAM_SM        18
#define PARAM_C2TFT     19
#define PARAM_DS        20
#define PARAM_SPECINV   21


#define PARAM_CI        27
#define PARAM_PMT       28
#define PARAM_PID       29
#define PARAM_APID      30
#define PARAM_DPID      31

#define MAX_PMT         16

struct dvb_params {
	uint32_t param[32];
	uint32_t set;
	uint8_t  pid[1024];
	uint8_t  dpid[1024];
	uint32_t pmt[MAX_PMT];
};

#define MAX_SOURCE 4

struct dvbfe {
	struct octoserve *os;
	struct osstrm *stream;
	pthread_t pt;

	int fd;
	int dmx;
	uint32_t type;
	int anum;
	int fnum;
	int state;
	int nr;
	int do_switch;
	int has_switch;
	uint32_t input[4];

	uint32_t lof1[MAX_SOURCE];
	uint32_t lof2[MAX_SOURCE];
	uint32_t lofs[MAX_SOURCE];
	uint32_t prev_delay[MAX_SOURCE];

	int scif_type;
	int scif_slot;
	uint32_t scif_freq;
	
	fe_status_t stat;
	uint32_t level;
	uint32_t lock;
	uint32_t quality;
	int first;

	uint32_t tune;
	uint32_t param[32];

	uint32_t n_tune;
	uint32_t n_param[32];

	pthread_mutex_t mutex;
};

struct dvbca {
	struct octoserve *os;
	struct osstrm *stream;
	int fd;
	uint32_t type;
	int anum;
	int fnum;
	int state;
	int nr;
	int input;

	pthread_t pt;
	pthread_t poll_pt;

	pthread_mutex_t mutex;

	struct en50221_transport_layer *tl;
	struct en50221_session_layer *sl;
	struct en50221_stdcam *stdcam;
	int resource_ready;
	int sentpmt;
	int moveca;
	int ca_pmt_version[MAX_PMT];
	int data_pmt_version;

	int setpmt;
	uint32_t pmt[MAX_PMT];
	uint32_t pmt_new[MAX_PMT];
	uint32_t pmt_old[MAX_PMT];

	int mmi_state;
	uint8_t mmi_buf[16];
	int mmi_bufp;
	int sock;
};

#define MAX_DVB_FE 16
#define MAX_DVB_CA 4
#define MAX_CONNECT 32
#define MAX_SESSION 12
#define MAX_STREAM  12

struct ostrans {
	int cport;
	int cport2;
	int sport;
	int sport2;
	int mcast;
	int family;
	int ttl;
	int rtp;

	uint8_t sip[16];
	uint8_t smac[6];
	uint8_t cip[16];
	uint8_t cmac[6];

	uint8_t mcip[16];
	uint8_t mcmac[6];

	uint8_t ssrc[4];
	uint32_t flags;
#define TRANS_NO_RTP_TO  1
#define TRANS_ALT_DEST   2
}; 

struct osmcc {
	LIST_ENTRY(osmcc) mcc;

	uint8_t ip[4];
	int32_t port_vec;
	uint8_t mac[6];
	uint32_t tag;
	uint32_t gtag;
};

LIST_HEAD(osmcchead, osmcc);

struct osstrm {
	struct octoserve *os;
	struct ossess *session; /* owner */
	struct dvbfe *fe;
	struct dvbca *ca;
	int nr;
	int state;
	
	int sport;
	int sport2;
};

struct ossess {
	struct octoserve *os;
	struct osstrm *stream;
	uint32_t id;
	int state;
	int playing;
	int nr;
	
	uint32_t timeout_len;
	time_t timeout;
 
	int nsfd;
	struct ostrans trans;
	struct dvb_params p;

	struct osmcchead mccs;
	uint32_t mcc_tag;
	time_t mcc_time;
	uint32_t mcc_state;
	uint32_t mcc_port_vec;
	uint32_t mcc_port_vec_set;
	uint32_t port_vec;
	uint32_t port_vec_set;
};

enum {
	M_NONE = 0,
	M_OPTIONS = 1,
	M_DESCRIBE = 2,
	M_SETUP = 3,
	M_PLAY = 4,
	M_TEARDOWN = 5,
	M_ERROR = 16,
	M_UNKNOWN = 17,
};

struct oscon {
	struct octoserve *os;
	struct ossess *session;
	int state;
	pthread_t pt;
	int sock;
	int nr;

	time_t timeout;

	struct ostrans trans;

	struct sockaddr cadr;
	char cadr_ip[INET6_ADDRSTRLEN];
	socklen_t clen;

	struct sockaddr sadr;
	char sadr_ip[INET6_ADDRSTRLEN];
	socklen_t slen;

	int seq;
	int transport_parsed;
	int session_parsed;
	int x_octonet_parsed;
	uint32_t x_ports;
	int error;

	int cmd;
	uint8_t  buf[8192];
	uint32_t bufp;
	int ln;
       
	struct dvb_params p;

	uint8_t url[8192];
};

#define UPNP_DEV_MAX 10

struct upnp_dev {
	char *nt;
	char *loc;
	char *uuid;
	char *server;
};

struct os_ssdp {
	struct octoserve *os;
	pthread_t pt;

	int sock;
	struct sockaddr_in sadr;
	char ip[INET6_ADDRSTRLEN];

	int sock6;
	struct sockaddr_in6 sadr6;
	char ip6[INET6_ADDRSTRLEN];

	uint8_t uuid[16];
	char uuid_str[42];
	char uuid_str2[42];

	struct sockaddr cadr;
	uint16_t csport;
	int csock;
	int v6;

	int alive;
	int bootid;
	int configid;
	uint8_t devid;

	char *server;
	time_t annt;

	int setup;
	time_t setupt;

	int http_sock;
	struct sockaddr http_sadr;

	int http_sock6;
	struct sockaddr http_sadr6;

	struct upnp_dev dev[UPNP_DEV_MAX];
	int dev_num;
};

struct os_http {
	struct octoserve *os;
	pthread_t pt;

	int sock;
	struct sockaddr sadr;
	char ip[INET6_ADDRSTRLEN];

	int sock6;
	struct sockaddr sadr6;
	char ip6[INET6_ADDRSTRLEN];

	struct sockaddr cadr;
	int csock;
};

struct octoserve {
	struct os_ssdp ssdp;
	struct os_http http;
	
	int exit;
	char *ifname;
	uint8_t mac[6];

	int scif_type;
	int has_feswitch;
	int do_feswitch;
	int strict_satip;

	int dvbfe_num;
	struct dvbfe dvbfe[MAX_DVB_FE];
	int dvbca_num;
	struct dvbca dvbca[MAX_DVB_CA];
	uint32_t dvbs2num;
	uint32_t dvbtnum;
	uint32_t dvbt2num;
	uint32_t dvbcnum;
	uint32_t dvbc2num;

	struct oscon  con[MAX_CONNECT];
	struct ossess session[MAX_SESSION];
	struct osstrm stream[MAX_STREAM];
	pthread_mutex_t lock;
	pthread_mutex_t uni_lock;
	pthread_t ci_pt;

	int rtsp_sock;
	struct sockaddr rtsp_sadr;
	char rtsp_ip[INET6_ADDRSTRLEN];

	int rtsp_sock6;
	struct sockaddr rtsp_sadr6;
	char rtsp_ip6[INET6_ADDRSTRLEN];
	
	uint32_t sessionid;

	int igmp_sock;
	int igmp_rsock;
	time_t igmp_time;
	uint32_t igmp_mode;
	uint32_t igmp_tag; 
	uint32_t igmp_timeout; 
	uint32_t igmp_robust; 

	int mld_sock;
	int has_switch;
	int strict;
}; 

int streamsock(const char *port, int family, struct sockaddr *sadr);
void sockname(struct sockaddr *sadr, char *name);
int get_ifa(const char *ifname, int iffam, struct sockaddr *sadr);
int init_ssdp(struct octoserve *os, struct os_ssdp *ss, uint32_t d, int nossdp, int nodms);
int dvb_tune(struct dvbfe *fe, struct dvb_params *p);
int init_dvb(struct octoserve *os, int nodvbt, int noswitch);
int release_dvb(struct octoserve *os);
void parse_config(struct octoserve *os, char *sec,
		  void (*cb)(struct octoserve *, char *, char *));
void handle_http(struct os_ssdp *ss);
void sadr2str(const struct sockaddr *sadr, char *s, size_t len);
void dump(const uint8_t *b, int l);

void proc_igmp(struct octoserve *os, uint8_t *b, int l, uint8_t *mh);
void mc_join(struct octoserve *os, uint8_t *ip, uint8_t *mac, uint8_t *group);
void mc_leave(struct octoserve *os, uint8_t *ip, uint8_t *group);
void mc_check(struct ossess *sess, int update);
void mc_del(struct ossess *sess);

void send_igmp_query(struct octoserve *os, uint8_t *group, uint8_t timeout);
void check_igmp(struct octoserve *os);

int switch_test(void);
int swich_get_port(uint8_t mac[6]);

int sectest(void);
void handle_fe(struct dvbfe *fe);

int set_pmt(struct dvbca *ca, uint32_t *pmt);

int set_nonblock(int fd);
int sendlen(int sock, char *buf, int len);
int sendstring(int sock, char *fmt, ...);

time_t mtime(time_t *t);

#endif
