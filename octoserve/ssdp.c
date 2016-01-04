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

extern struct rtsp_error rtsp_errors;

extern uint32_t debug;

static void ssdp_reset_setup(struct os_ssdp *ss)
{
	ss->setup = 0;
	mtime(&ss->setupt);
	ss->setupt += 5;
	mtime(&ss->annt);
}

static int read_id(char *type, uint32_t *id)
{
	int fd, len;
	char name[80];

	len = snprintf(name, 80, "/config/%s.id", type);
	if (len < 0 || len == 80) 
		return -1;
	fd = open(name, O_RDONLY);
	if (fd < 0)
		return -1;
	printf("read id from %s\n", name);
	len = read(fd, name, 80);
	name[len] = 0;
	*id = strtol(name, NULL, 10);
	printf("%s.id = %d\n", type, *id);
	return 0;
}

static int write_id(char *type, uint32_t id)
{
	int fd, len;
	char name[80];

	len = snprintf(name, 80, "/config/%s.id", type);
	if (len < 0 || len == 80) 
		return -1;
	fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 00644);
	if (fd < 0)
		return -1;
	printf("write id = %d to %s\n", id, name);
	len = snprintf(name, 80, "%d", id);
	write(fd, name, len);
	close(fd);
	return 0;
}

static int ssdp_msg(struct os_ssdp *ss, char *buf, int n, int notify, int nr, int v6, int send_id)
{
	int len;
	uint8_t *u = ss->uuid;
	char uuid[37];
	char *header = notify ? 
		"NOTIFY * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\n" :
		"HTTP/1.1 200 OK\r\n";
	char id[32];

	if (send_id) {
		len = snprintf(id, sizeof(id), 
			       "DEVICEID.SES.COM: %d",
			       ss->devid);
		if (len < 0  || len >= sizeof(id)) 
			return -1;
	} else
		id[0] = 0;

	if (ss->alive) {
		len = snprintf(buf, n, 
			       "%s"
			       "CACHE-CONTROL: max-age=1800\r\n"
			       "LOCATION: http://%s%s\r\n"
			       "NT: %s\r\n"
			       "NTS: ssdp:%s\r\n"
			       "SERVER: %s\r\n"
			       "USN: %s%s%s\r\n"
			       "BOOTID.UPNP.ORG: %d\r\n"
			       "CONFIGID.UPNP.ORG: %d\r\n"
			       "%s\r\n"
			       "\r\n",
			       header,
			       v6 ? ss->ip6 : ss->ip,
			       ss->dev[nr].loc,
			       ss->dev[nr].nt[0] ? ss->dev[nr].nt : ss->dev[nr].uuid,
			       "alive",
			       ss->dev[nr].server,
			       ss->dev[nr].uuid, ss->dev[nr].nt[0] ? "::" : "", ss->dev[nr].nt, 
			       ss->bootid, ss->configid,
			       id
			);
	} else {
		dbgprintf(DEBUG_SSDP, "BYEBYE\n");
				
		len = snprintf(buf, n, 
			       "%s"
			       "NT: %s\r\n"
			       "NTS: ssdp:byebye\r\n"
			       "USN: %s%s%s\r\n"
			       "BOOTID.UPNP.ORG: %d\r\n"
			       "CONFIGID.UPNP.ORG: %d\r\n"
			       "\r\n",
			       header,
			       ss->dev[nr].nt[0] ? ss->dev[nr].nt : ss->dev[nr].uuid,
			       ss->dev[nr].uuid, ss->dev[nr].nt[0] ? "::" : "", ss->dev[nr].nt, 
			       ss->bootid, ss->configid
			);

	}
	return len;
}

static int sendto_port(int sock, const void *buf, size_t len,
		       struct sockaddr *cadr, uint16_t port)
{
	struct sockaddr_in adr;
	int r;

	/*adr = *((struct sockaddr_in *) cadr); */
	/* struct assignment seem to be broken in arm gcc 4.5.4?!? */
	memcpy(&adr, cadr, sizeof(adr));
	adr.sin_port = htons(port);
	r = sendto(sock, buf, len, 0, (struct sockaddr *) &adr, sizeof(adr));
	//dbgprintf(DEBUG_SSDP, "sent to port %u, family %u, ret=%d errno=%d\n", port, adr.sin_family, r, errno);
	return r;
}

static int send_ssdp_msg(struct os_ssdp *ss, int nr, int notify)
{
	uint8_t buf[2048];
	int len;
	
	len = ssdp_msg(ss, buf, sizeof(buf), notify, nr, 0, nr < 3);
	
	if (len < 0  || len >= sizeof(buf))
		return -1;
	if (notify) {
		struct sockaddr_in insadr;

		memset(&insadr, 0, sizeof(insadr));
		insadr.sin_family = AF_INET;
		insadr.sin_port = htons(1900);
		insadr.sin_addr.s_addr = inet_addr("239.255.255.250");
		return sendto(ss->sock, buf, len, 0, (struct sockaddr *) &insadr, sizeof(insadr));
	} else 
		return sendto(ss->sock, buf, len, 0, &ss->cadr, sizeof(struct sockaddr));
}

static int ssdp_announce(struct octoserve *os, int alive, int notify)
{
	struct os_ssdp *ss = &os->ssdp;
	uint8_t buf[2048];
	int len, nr, res;
	int s = os->ssdp.sock;
	uint8_t *u = ss->uuid;
	time_t t;

	if (notify) {
		mtime(&t);
		if (!ss->setup)
			if (t >= ss->setupt) {
				ss->setup = 1;
				dbgprintf(DEBUG_SSDP, "END SSDP SETUP\n");
			}
		if (ss->alive == alive && t  < ss->annt)
			return 0;
		ss->alive = alive;
		ss->annt = t + 900;  //FIXME: +/- random
	}
	for (nr = 0; nr < ss->dev_num; nr++) {
		send_ssdp_msg(ss, nr, notify);
	}
}

static int ssdp_defend(struct os_ssdp *ss)
{
	char buf[2048], host[64];
	int len;
	int s = ss->v6 ? ss->sock6 : ss->sock;

	sockname(&ss->cadr, host);
	dbgprintf(DEBUG_SSDP, "defend against HOST:%s\n", host);

	len = snprintf(buf, sizeof(buf), 
		       "M-SEARCH * HTTP/1.1\r\n"
		       "HOST: %s:1900\r\n"
		       "MAN: \"ssdp:discover\"\r\n"
		       "ST: urn:ses-com:device:SatIPServer:1\r\n"
		       "USER-AGENT: %s\r\n"
		       "DEVICEID.SES.COM: %d\r\n"
		       "\r\n",
		       ss->v6 ? ss->ip6 : ss->ip,
		       ss->server,
		       ss->devid);
	if (len < 0  || len >= sizeof(buf)) 
		return -1;
	//return sendto(s, buf, len, 0, &ss->cadr, sizeof(ss->cadr));
	return sendto_port(ss->sock, buf, len, &ss->cadr, ss->csport);
}

static int send_reply_msearch(struct os_ssdp *ss, int mc, int send_id, int nr)
{
	char buf[2048], id[32];
	int len, res;
	int v6 = ss->v6;
	int s = v6 ? ss->sock6 : ss->sock;

	dbgprintf(DEBUG_SSDP, "send msearch reply nr=%d\n", nr);		
	if (send_id) {
		len = snprintf(id, sizeof(id), 
			       "DEVICEID.SES.COM: %d\r\n",
			       ss->devid);
		if (len < 0  || len >= sizeof(id)) 
			return -1;
	} else
		id[0] = 0;
	len = snprintf(buf, sizeof(buf), 
		       "HTTP/1.1 200 OK\r\n"
		       "CACHE-CONTROL: max-age=1800\r\n"
		       "EXT:\r\n"
		       "LOCATION: http://%s%s\r\n"
		       "SERVER: %s\r\n"
		       "ST: %s\r\n"
		       "USN: %s%s%s\r\n"
		       "BOOTID.UPNP.ORG: %d\r\n"
		       "CONFIGID.UPNP.ORG: %d\r\n"
		       "%s"
		       "\r\n",
		       ss->v6 ? ss->ip6 : ss->ip,
		       ss->dev[nr].loc,
		       ss->dev[nr].server,
		       ss->dev[nr].nt[0] ? ss->dev[nr].nt : ss->dev[nr].uuid,
		       ss->dev[nr].uuid, ss->dev[nr].nt[0] ? "::" : "", ss->dev[nr].nt, 
		       ss->bootid, ss->configid, id
		);
	if (len < 0  || len >= sizeof(buf)) 
		return -1;
	dbgprintf(DEBUG_SSDP, "sending:\n%s", buf);		
	if (mc) {
		struct sockaddr_in insadr;

		memset(&insadr, 0, sizeof(insadr));
		insadr.sin_family = AF_INET;
		insadr.sin_port = htons(1900);
		insadr.sin_addr.s_addr = inet_addr("239.255.255.250");
		res = sendto(s, buf, len, 0, (struct sockaddr *) &insadr, sizeof(insadr));
	} else 
	        res = sendto(s, buf, len, 0, &ss->cadr, sizeof(struct sockaddr));
	//res = sendto_port(s, buf, len, &ss->cadr, ss->csport);
	return res;
}

static int ssdp_msearch(struct os_ssdp *ss, int mc, char *st)
{
	int nr, len = strlen(st), type = 0;
	uint8_t *u = ss->uuid;

	if (!len)
		return 0;
	if (!strncasecmp(st, "ssdp:all", 8)) 
		type = 1;
	else if (!strncasecmp(st, "uuid:", 5))
		type = 2;
	dbgprintf(DEBUG_SSDP, "msearch mc=%u, st=%s (type=%u)\n", mc, st, type);
	
	for (nr = 0; nr < ss->dev_num; nr++) {
		if ((type == 1) || 
		    ((type == 2) && !ss->dev[nr].nt[0] &&
		     !strncasecmp(ss->dev[nr].uuid, st, len)) ||
		    ((type == 0) && !strncasecmp(ss->dev[nr].nt, st, len)))
			send_reply_msearch(ss, mc, nr < 3, nr);
	}
	return 0;
}

static void handle_ssdp(struct octoserve *os, char *m, int ml)
{
	struct os_ssdp *ss = &os->ssdp;
	int ln, ll, tl, as, al;
	char *l, *le, *me = m + ml, *st = NULL;
	int type = 0;
	int htype = 0;
	int devid = -1;
	int mx = -1;
	uint8_t uu[16];
	char host[64];
	char str[INET_ADDRSTRLEN];	
	int sport = 1900, hport = 1900;

	dbgprintf(DEBUG_SSDP, 
		  "\n********************************************************************************\n");
	inet_ntop(AF_INET, &((struct sockaddr_in *) &ss->cadr)->sin_addr, str, INET_ADDRSTRLEN);
	dbgprintf(DEBUG_SSDP, "\nSSDP: %s\n", str);

	memset(uu, 0, 16);

	for (l = m, ln = -1; l < me; l += ll + 1) {
		for (le = l; *le != '\r' && *le != '\n' && le < me; le++);
		ll = le - l;
		if (!ll) 
			continue;
		*le = 0;
		ln++;
		dbgprintf(DEBUG_SSDP, "%02d: %s\n", ln, l);
		if (ln == 0) {
			if (!strncasecmp(l, "M-SEARCH * HTTP/1.1", ll)) {
				type = 1;
				continue;
			}
			if (!strncasecmp(l, "NOTIFY * HTTP/1.1", ll)) {
				type = 2;
				continue;
			}
			if (!strncasecmp(l, "HTTP/1.1 200 OK", ll)) {
				type = 3;
				continue;
			}
			dbgprintf(DEBUG_SSDP, "unknown header\n");
			return;
		}
		for (tl = 0; tl < ll && l[tl] != ':'; tl++);
		if (tl == ll)
			continue;
		for (as = tl + 1; as < ll && isspace(l[as]); as++);
		al = ll - as;

		if (tl == 4 && !strncasecmp(l, "HOST", 4)) {
			char *h, *p;
			
			if (!strncmp(l + as, "239.255.255.250:1900", al)) 
				htype = 1;
			else {
				htype = 2;
				if (mx < 0)
					mx = 1;
			}
			h = host;
			p = l + as;
			while (*p && *p != ':')
				*(h++) = *(p++);
			*h = 0;
			p++;
			hport = atoi(p);
		}
		if (tl == 2 && !strncasecmp(l, "MX", tl)) {
			mx = atoi(l + as);
			continue;
		}
		if (tl == 2 && !strncasecmp(l, "ST", tl)) {
			st = l + as;
			continue;
		}
		if (tl == 16 && !memcmp(l, "DEVICEID.SES.COM", tl)) {
			devid = atoi(l + as);
			//dbgprintf(DEBUG_SSDP, "sat>ip device id %d\n", devid); 
			continue;
		}
		if (tl = 19 && !memcmp(l, "SEARCHPORT.UPNP.ORG", tl)) {
			sport = atoi(l + as);
			dbgprintf(DEBUG_SSDP, "searchport %d\n", sport); 
			continue;
		}
		if (tl == 3 && !memcmp(l, "USN", tl) && !memcmp(l+5, "uuid:", 5)) {
			int n;
			
			//printf("USN: %s\n", l+5);
			sscanf(l+10, "%2hhx%2hhx%2hhx%2hhx-%2hhx%2hhx-"
			       "%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%n",
			       &uu[0], &uu[1], &uu[2], &uu[3],
			       &uu[4], &uu[5], &uu[6], &uu[7],
			       &uu[8], &uu[9], &uu[10], &uu[11],
			       &uu[12], &uu[13], &uu[14], &uu[15], &n);
			/* ignore message from myself */
			if (!memcmp(uu, ss->uuid, 16))
				dbgprintf(DEBUG_SSDP, "that's me!\n");
			continue;
		}
	}
	ss->csport = sport;
	dbgprintf(DEBUG_SSDP, "host=%s, hport=%u, type=%d, htype=%d mx=%d\n",
		  host, hport, type, htype, mx); 
	/* M-SEARCH */
	if (ss->setup) {
		if (type == 1 && mx > 0 && st != NULL) {
			//ssdp_msearch(ss, htype == 2 ? 0 : 1, st);
			ssdp_msearch(ss, 0, st);
		}
		/* NOTIFY */
		if (type == 2 && devid == ss->devid && memcmp(uu, ss->uuid, 16))
			ssdp_defend(ss);
	} else {
		if (type == 1 && htype == 2 && ss->devid == devid) {
			send_reply_msearch(ss, 0, 1, 2);
			ss->devid++;
			write_id("device", ss->devid);
			dbgprintf(DEBUG_SSDP, "new device id %d \n", ss->devid);
			ssdp_reset_setup(ss);
		}
	}
}


#define SSDP_MCAST "239.255.255.250"
#define SSDP_MCAST_LL "FF02::C"
#define SSDP_MCAST_SL "FF05::C"

static int ssdp_mc_add(int s, const char *ifname, int add, int v6)
{
	if (v6)	{
		struct ipv6_mreq imr;
		unsigned int ifindex = if_nametoindex(ifname);

		memset(&imr, 0, sizeof(imr));
		inet_pton(AF_INET6, SSDP_MCAST_LL, &imr.ipv6mr_multiaddr);
		imr.ipv6mr_interface = ifindex;
		if (setsockopt(s, IPPROTO_IPV6, add ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP,
			      &imr, sizeof(imr)) < 0)
			return -1;
		inet_pton(AF_INET6, SSDP_MCAST_SL, &imr.ipv6mr_multiaddr);
		if (setsockopt(s, IPPROTO_IPV6, add ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP,
			      &imr, sizeof(imr)) < 0)
			return -1;
	} else {
		struct ip_mreq imr;
		struct sockaddr_in sadr;
		unsigned char ttl = 2, one = 1, zero = 0;

		get_ifa(ifname, AF_INET, (struct sockaddr *) &sadr);
		imr.imr_multiaddr.s_addr = inet_addr(SSDP_MCAST);
		imr.imr_interface.s_addr = sadr.sin_addr.s_addr;

		if (setsockopt(s, IPPROTO_IP, add ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP,
			       &imr, sizeof(imr)) < 0)
			return -1;
		setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
		//setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, &zero, sizeof(zero));
	}
	printf("mc_add OK\n");
	return 0;
}

static int ssdpsock(const char *ifnam, int family, struct sockaddr *sadr)
{
	int one=1, sock;
	struct addrinfo *ais, *ai, hints = {
		.ai_flags = AI_PASSIVE, 
		.ai_family = family,
		.ai_socktype = SOCK_DGRAM,
		.ai_protocol = 0,
		.ai_addrlen = 0,
		.ai_addr = sadr,
		.ai_canonname = NULL,
		.ai_next = NULL,
	};
	
	if (getaddrinfo(NULL, "1900", &hints, &ais) < 0)
		return -1;
	for (ai = ais; ai; ai = ai->ai_next)  {
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock == -1)
			continue;
		//dump(ai->ai_addr, sizeof(*ai->ai_addr));
		if (!setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) &&
		    !bind(sock, ai->ai_addr, ai->ai_addrlen))
			break;
		close(sock);
	}
	freeaddrinfo(ais);
	ssdp_mc_add(sock, ifnam, 1, family == AF_INET6 ? 1 : 0);
	return sock;
}

static void add_fd(int fd, int *mfd, fd_set *fds)
{
	FD_SET(fd, fds);
	if (fd > *mfd)
		*mfd = fd;
}

static void ssdp_loop(struct octoserve *os)
{
	struct os_ssdp *ss = &os->ssdp;
	int num;
	int mfd;
	fd_set fds;
	struct timeval timeout;
	char buf[1501];

	ss->http_sock = streamsock("8888", AF_INET, &ss->http_sadr);
	if (listen(ss->http_sock, 10) < 0) {
		printf("listen error");
		return;
	}
	ssdp_reset_setup(ss);
	
	while (!os->exit) {
		ssdp_announce(os, 1, 1);

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		FD_ZERO(&fds);
		mfd = 0;
		add_fd(ss->sock, &mfd, &fds);
		//add_fd(ss->sock6, &mfd, &fds);
		add_fd(ss->http_sock, &mfd, &fds);

		num = select(mfd + 1, &fds, NULL, NULL, &timeout);
		if (num < 0)
			break;
		if (FD_ISSET(ss->sock, &fds)) {
			socklen_t len;
			int n;

			len = sizeof(ss->cadr);
			n = recvfrom(ss->sock, buf, sizeof(buf) - 1, 0, &ss->cadr, &len);
			ss->v6 = 0;
			if (n > 0) 
				handle_ssdp(os, buf, n);
		}
		if (FD_ISSET(ss->http_sock, &fds)) {
			socklen_t len;
			pthread_t pt;
			
			//ss->csock = accept4(ss->http_sock, &ss->cadr, &len, SOCK_NONBLOCK);
			ss->csock = accept(ss->http_sock, &ss->cadr, &len);
			handle_http(ss);
		}
	}
	ssdp_announce(os, 0, 1);
}

static void ssdp_search(int s)
{
	struct sockaddr_in insadr;
	char ms[] = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\n"
		"MAN: \"ssdp:discover\"\r\nMX: 5\r\nST: ssdp:all\r\n\r\n";

	memset(&insadr, 0, sizeof(insadr));
	insadr.sin_family = AF_INET;
	insadr.sin_port = htons(1900);
	insadr.sin_addr.s_addr = inet_addr("239.255.255.250");
	sendto(s, &ms[0], sizeof(ms) - 1, 0, 
	       (struct sockaddr *) &insadr, sizeof(insadr));
}

void ssdp_thread(struct octoserve *os)
{
	//ssdp_search(s);
	ssdp_loop(os);
}

static uint64_t get_us(void)
{ 
	struct timeval tv;
	gettimeofday(&tv, 0);
	return ((uint64_t) tv.tv_sec * 1000000ULL + (uint64_t) tv.tv_usec);
}

static void create_uuid(uint8_t *u, uint8_t *mac)
{
	uint8_t uu[10] = { 0xdd, 0x84, 0x8d, 0x00, 0x26, 
			   0xec, 0x11, 0xeb, 0x80, 0x00 };
	
	memcpy(u, uu, 10);
	memcpy(u + 10, mac, 6);
}

static void create_uuid_old(uint8_t *u, uint8_t *mac)
{
	uint64_t us, ns;
	int i; 

	memcpy(u + 10, mac, 6);

	us = get_us();
	ns = us * 1000ULL + 122192928000000000ULL;

	u[0] = ns >> 24;
	u[1] = ns >> 16;
	u[2] = ns >> 8;
	u[3] = ns;
	u[4] = ns >> 40;
	u[5] = ns >> 32;
	u[6] = ns >> 56;
	u[7] = ns >> 48;

	u[8] = 0;
	u[9] = 0;

	u[6] = (u[6] & 0x0f) | 0x10;
	u[8] = (u[8] & 0x3f) | 0x80;
}

static int add_dev(struct os_ssdp *ss, char *nt, char *loc, char *uuid, char *server)
{
	int n = ss->dev_num;
	
	if (n == UPNP_DEV_MAX)
		return -1;
	ss->dev[n].nt = nt;
	ss->dev[n].loc = loc;
	ss->dev[n].uuid = uuid;
	ss->dev[n].server = server;
	ss->dev_num++;
}


int init_ssdp(struct octoserve *os, struct os_ssdp *ss, uint32_t d, int nossdp, int nodms)
{
	time_t t;
	uint8_t *u, mac[6];
	struct ifreq ifr;
	uint32_t id;
	int stat;
	char *dms = "Linux/3.17.7 DLNADOC/1.50 UPnP/1.0 OctopusNet-DMS/1.0";

	debug = d;
	ss->os = os;
	ss->server = "Linux/3.17.7 UPnP/1.1 OctopusNet/" OCTOSERVE_VERSION;
	stat = read_id("device", &id);
	if (stat < 0) {
		ss->devid = 5;
		write_id("device", ss->devid);
	} else
		ss->devid = id;
	stat = read_id("boot", &id);
	if (stat < 0)
		ss->bootid = 0;
	else
		ss->bootid = id + 1;
	write_id("boot", ss->bootid);

	dbgprintf(DEBUG_SSDP, "DEVID = %u\n", ss->devid);

	
	ss->configid = 1;

	get_ifa(os->ifname, AF_INET, (struct sockaddr *) &os->ssdp.sadr);
	get_ifa(os->ifname, AF_INET6, (struct sockaddr *) &os->ssdp.sadr6);
	sockname((struct sockaddr *) &os->ssdp.sadr, os->ssdp.ip);
	sockname((struct sockaddr *) &os->ssdp.sadr6, os->ssdp.ip6);

	if (!nossdp) {
		ss->sock = ssdpsock(os->ifname, AF_INET, (struct sockaddr *) &os->ssdp.sadr);
		if (ss->sock < 0)
			return -1;
		
		ss->sock6 = ssdpsock(os->ifname, AF_INET6, (struct sockaddr *) &os->ssdp.sadr);
		if (ss->sock6 < 0)
			return -1;
		
		strcpy(ifr.ifr_name, os->ifname);
		ioctl(ss->sock, SIOCGIFHWADDR, &ifr);
		memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);

		create_uuid(ss->uuid, mac);
		u = ss->uuid;

		snprintf(ss->uuid_str, 42, 
			 "uuid:%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			 u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], 
			 u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
		
		snprintf(ss->uuid_str2, 42, 
			 "uuid:f0287290-e1e1-11e2-9a21-%02x%02x%02x%02x%02x%02x",
			 u[10], u[11], u[12], u[13], u[14], u[15]);
		
		add_dev(ss, "upnp:rootdevice", "/octoserve/octonet.xml", ss->uuid_str, ss->server);
		add_dev(ss, "", "/octoserve/octonet.xml", ss->uuid_str, ss->server);
		add_dev(ss, "urn:ses-com:device:SatIPServer:1", "/octoserve/octonet.xml", 
			ss->uuid_str, ss->server);
		
		if (!nodms) {
			add_dev(ss, "upnp:rootdevice", ":8080/dms.xml", ss->uuid_str2, dms);
			add_dev(ss, "", ":8080/dms.xml", ss->uuid_str2, dms);
			add_dev(ss, "urn:schemas-upnp-org:device:MediaServer:1", ":8080/dms.xml",
				ss->uuid_str2, dms);
			add_dev(ss, "urn:schemas-upnp-org:service:ConnectionManager:1",
				":8080/dms.xml", ss->uuid_str2, dms);
			add_dev(ss, "urn:schemas-upnp-org:service:ContentDirectory:1",
				":8080/dms.xml", ss->uuid_str2, dms);
			add_dev(ss, "urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1",
				":8080/dms.xml", ss->uuid_str2, dms);
		}
		pthread_create(&os->ssdp.pt, NULL, (void *) ssdp_thread, os);
	}
	return 0;
}
