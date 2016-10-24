/*  
    (C) 2012-13 Digital Devices GmbH. 

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

int streamsock(const char *port, int family, struct sockaddr *sadr)
{
	int one=1, sock;
	struct addrinfo *ais, *ai, hints = {
		.ai_flags = AI_PASSIVE, 
		.ai_family = family,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0, .ai_addrlen = 0,
		.ai_addr = NULL, .ai_canonname = NULL, .ai_next = NULL,
	};
	if (getaddrinfo(NULL, port, &hints, &ais) < 0)
		return -1;
	for (ai = ais; ai; ai = ai->ai_next)  {
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock == -1)
			continue;
		if (!setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) &&
		    !bind(sock, ai->ai_addr, ai->ai_addrlen)) {
			*sadr = *ai->ai_addr;
			break;
		}
		close(sock);
		sock = -1;
	}
	freeaddrinfo(ais);
	return sock;
}

void sockname(struct sockaddr *sadr, char *name)
{
	void *adr;
	char *unknown = "unknown";
	
	strcpy(name, unknown);
	if (sadr->sa_family == AF_INET) 
		adr = &((struct sockaddr_in *) sadr)->sin_addr;
	else if (sadr->sa_family == AF_INET6) 
		adr = &((struct sockaddr_in6 *) sadr)->sin6_addr;
	else 
		return;
	inet_ntop(sadr->sa_family, adr, name, INET6_ADDRSTRLEN);
	
	printf("sockname: %s\n", name);
}

int get_ifa(const char *ifname, int iffam, struct sockaddr *sadr)
{
	struct ifaddrs *ifaddrs, *ifa;

	if (getifaddrs(&ifaddrs) == -1)
		return -1;
	for (ifa = ifaddrs; ifa; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr)
			continue;
		if (ifa->ifa_addr->sa_family != iffam)
			continue;
		if (strcmp(ifname, ifa->ifa_name))
			continue;
		*sadr = *ifa->ifa_addr;
#if 1
		{
			void *adr;
			char buf[INET6_ADDRSTRLEN];
			
			if (ifa->ifa_addr->sa_family == AF_INET) 
				adr = &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
			else 
				adr = &((struct sockaddr_in6 *) ifa->ifa_addr)->sin6_addr;
			inet_ntop(ifa->ifa_addr->sa_family, adr, buf, sizeof(buf));
			printf("get_ifa %s:%s\n", ifname, buf);
		}
#endif
		freeifaddrs(ifaddrs);
		return 0;
	}
	freeifaddrs(ifaddrs);
	return -1;
}


void sadr2str(const struct sockaddr *sadr, char *s, size_t len)
{
	switch(sadr->sa_family) {
        case AF_INET:
		inet_ntop(AF_INET,
			  &(((struct sockaddr_in *)sadr)->sin_addr), s, len);
		break;

        case AF_INET6:
		inet_ntop(AF_INET6,
			  &(((struct sockaddr_in6 *)sadr)->sin6_addr), s, len);
		break;
	}
}

int sendlen(int sock, char *buf, int len)
{
	int done, todo; 

	for (todo = len; todo; todo -= done, buf += done)
		if ((done = send(sock, buf, todo, 0)) < 0) {
			printf("sendlen error\n");
			return done;
		}
	return len;
} 

int sendstring(int sock, char *fmt, ...)
{
	int len;
	uint8_t buf[2048];
	va_list args;

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	if (len <= 0 || len >= sizeof(buf))
		return;
	sendlen(sock, buf, len);
	va_end(args);
}

time_t mtime(time_t *t)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts))
		return 0;
	if (t)
		*t = ts.tv_sec;
	return ts.tv_sec;
}




struct nlreq {
	struct nlmsghdr n;
	struct rtmsg r;
	char buf[1024];
};

int nlrequest(int fd, struct nlmsghdr *n, unsigned char *gw)
{
	char buf[1024];
	int rtl, len = 0, c;
	struct nlmsghdr *nlp;
	struct sockaddr_nl snl;
	struct iovec iov = { (void *) n, n->nlmsg_len };
	struct msghdr msg = { (void *) &snl, sizeof(snl),
			      &iov, 1, NULL, 0, 0 };
	char *p	= (char *) buf;

	memset(&snl, 0, sizeof(snl));
	snl.nl_family = AF_NETLINK;
	sendmsg(fd, &msg, 0);
	while (1) {
		iov.iov_base = p;
		iov.iov_len = sizeof(buf) - len;
		c = recvmsg(fd, &msg, 0);
		if (c < 0)
			return -1;
		if (c == 0)
			break;
		nlp = (struct nlmsghdr *)p;
		if (nlp->nlmsg_type == NLMSG_DONE)
			break;
		if (nlp->nlmsg_type == NLMSG_ERROR)
			break;
		p += c;
		len += c;
		break;
	}
	for (nlp = (struct nlmsghdr *) buf; NLMSG_OK(nlp, len); nlp = NLMSG_NEXT(nlp, len)) {
		struct rtmsg *rtp = (struct rtmsg *) NLMSG_DATA(nlp);
		struct rtattr *rtap = (struct rtattr *) RTM_RTA(rtp);
		
		for (rtl = RTM_PAYLOAD(nlp); RTA_OK(rtap, rtl); rtap = RTA_NEXT(rtap, rtl)) {
			switch (rtap->rta_type) {
			case RTA_GATEWAY:
				memcpy(gw, RTA_DATA(rtap), 24);
				break;
			default:
				break;
			}
		}
	}
}

int get_route(unsigned char *ip)
{
	struct sockaddr_nl snl;
	struct nlreq req;
	struct nlmsghdr *n = &req.n;
	struct rtattr *rta;
	int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	
	memset(&snl, 0, sizeof(snl));
	snl.nl_family = AF_NETLINK;
	snl.nl_pid = getpid();
	bind(fd, (struct sockaddr *) &snl, sizeof(snl));

	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.n.nlmsg_type = RTM_GETROUTE;
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.r.rtm_family = AF_INET;
	//req.r.rtm_flags |= RTM_F_LOOKUP_TABLE;
	req.r.rtm_table = 0;
	req.r.rtm_dst_len = 32;

	rta = (struct rtattr*)(((char*)n) + NLMSG_ALIGN(n->nlmsg_len));
	rta->rta_type = RTA_DST;
	rta->rta_len = RTA_LENGTH(4);
	memcpy(RTA_DATA(rta), ip, 4);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_LENGTH(4);

	nlrequest(fd, n, ip);
	printf("IP/GW = %u.%u.%u.%u\n", ip[0], ip[1], ip[2], ip[3]);
	close(fd);
}

