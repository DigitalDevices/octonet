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

extern uint32_t debug;

static uint16_t cs16(uint8_t *p, uint32_t len)
{
	uint32_t cs;

	cs = (p[0] << 8) | p[1];
	p += 4;
	len -= 4;
	for (; len > 0; len -= 2, p += 2) 
		cs += (p[0] << 8) | p[1];
	if (len)
		cs += p[0];

	while (cs >> 16)
		cs = (cs & 0xffff) + (cs >> 16);
	return ~cs;
}

void proc_igmp(struct octoserve *os, uint8_t *b, int l, uint8_t *macheader)
{
	int hl = (b[0] & 0xf) * 4, pl, rl, i, j, ns, al;
	uint8_t cmd, *r, *s, type, *p;
	uint16_t n;
	char sd[64], mc[256], *mac = 0;
	uint16_t cs;

	if (macheader)
		mac = macheader + 6;
	if (hl + 1 > l)
		return;
	
	//dump(b, l);
	sprintf(sd, "%03d.%03d.%03d.%03d -> %03d.%03d.%03d.%03d", 
		b[12], b[13], b[14], b[15],
		b[16], b[17], b[18], b[19]);
	
	p = b + hl;
	pl = l - hl;
	if (pl < 4)
		return;
	if (cs16(p, pl) != ((p[2] << 8) | p[3])) {
		dbgprintf(DEBUG_IGMP, "IGMP CS error\n");
#if 0
		if (debug & DEBUG_IGMP)
			dump(b, l);
#endif
		return;
	}

	cmd = b[hl];
	switch (cmd) {
	case 0x11:
	{
		uint32_t a1, a2;
		uint8_t *s;
		
		if (pl < 12)
			return;
		dbgprintf(DEBUG_IGMP, "IGMP: query by %s\n", sd);
		s = (uint8_t *) &(((struct sockaddr_in *) &os->ssdp.sadr)->sin_addr);
		a1 = (b[12] << 24) | (b[13] << 16) | (b[14] << 8) | b[15];
		a2 = (s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3];
		dbgprintf(DEBUG_IGMP, "%08x < %08x ? %s\n", a1, a2, (a1 < a2) ? "yes" : "no");
		if (a1 < a2) {
			/* somebody else with lower IP is already sending queries */
			os->igmp_mode = 3;
			mtime(&os->igmp_time);
			os->igmp_tag++;
			os->igmp_timeout = b[hl + 1] / 10 + 1;
			dbgprintf(DEBUG_IGMP, "IGMP slave, tag = %u, timeout = %u\n",
				  os->igmp_tag, os->igmp_timeout);
		} else
			dbgprintf(DEBUG_IGMP, "IGMP master, tag = %u, timeout = %u\n",
				  os->igmp_tag, os->igmp_timeout);
		break;
	}
	case 0x12:
		if (pl < 8)
			return;
		dbgprintf(DEBUG_IGMP, "IGMPV1: %s : %d.%d.%d.%d\n", sd,
			  b[hl + 4], b[hl + 5], b[hl + 6], b[hl + 7]);
		mc_join(os, b + 12, mac, &b[hl + 4]);
		break;
	case 0x16:
		if (pl < 8)
			return;
		dbgprintf(DEBUG_IGMP, "IGMPV2: %s : %d.%d.%d.%d\n", sd,
			  b[hl + 4], b[hl + 5], b[hl + 6], b[hl + 7]);
		mc_join(os, b + 12, mac, &b[hl + 4]);
		break;
	case 0x17:
		if (pl < 8)
			return;
		dbgprintf(DEBUG_IGMP, "IGMPL: %s : %d.%d.%d.%d\n", sd,
			  b[hl + 4], b[hl + 5], b[hl + 6], b[hl + 7]);
		mc_leave(os, b + 12, &b[hl + 4]);
		break;
	case 0x22:
		if (pl < 8)
			return;
		pl -= 8;
		//dump(b + hl, l - hl);
		n = (b[hl + 6] << 8) | b[hl + 7];
		dbgprintf(DEBUG_IGMP, "IGMPV3: %s, %d records:\n", sd, n);
		for (i = 0, r = &b[hl + 8]; i < n; i++, r += rl, pl -= rl) {
			if (pl < 4)
				return;
			ns = (r[2] << 8) | r[3];
			al = r[1] * 4;
			rl = 8 + ns * 4 + al;
			if (pl < rl)
				return;
			type = r[0];
			dbgprintf(DEBUG_IGMP, "type %d, %d sources, %d aux:\n", type, ns, al);
			if (ns) {
				dbgprintf(DEBUG_IGMP, "cannot handle sources\n");
				continue;
			}
			if (type == 2) {
				dbgprintf(DEBUG_IGMP, "IGMPV3: %s subscribed to %d\n", sd, ns);
				mc_join(os, b + 12, mac, &r[4]);
			}
			if (type == 4) {
				dbgprintf(DEBUG_IGMP, "IGMPV3: %s, change to Exclude %d (Join)\n",
					  sd, ns);
				mc_join(os, b + 12, mac, &r[4]);
			}
			if (type == 3) {
				dbgprintf(DEBUG_IGMP, "IGMPV3: %s, change to Include %d (Leave)\n",
					  sd, ns);
				mc_leave(os, b + 12, &r[4]);
			}
			//printf("MC: %d.%d.%d.%d\n", r[4], r[5], r[6], r[7]);
			for (j = 0, s = &r[8]; j < ns; j++, s += 4) {
				dbgprintf(DEBUG_IGMP, "%d.%d.%d.%d\n", s[0], s[1], s[2], s[3]);
			}
		}
		break;
	}
	//dump(b + hl, l - hl);
}

static void calc_cs(uint8_t *m, uint32_t len)
{
	uint32_t cs  = 0, i;
	
	for (i = 0; i < len - 1; i += 2)
		cs += (m[i] << 8) | m[i + 1];
	if (i < len)
		cs += m[i] << 8;
	cs = (cs >> 16) + (cs & 0xffff);
	cs = (cs >> 16) + (cs & 0xffff);
	cs ^= 0xffff;
	m[2] = cs >> 8;
	m[3] = cs & 0xff;
}

void send_igmp_query(struct octoserve *os, uint8_t *group, uint8_t timeout)
{
	uint8_t msg[] = { 0x11, 0x64, 0x00, 0x00, 
			  0x00, 0x00, 0x00, 0x00,
			  0x02, 0x7d, 0x00, 0x00
	};
	struct sockaddr_in insadr;
	uint8_t zero = 0, ra[4] = { 0x94, 0x04, 0x00, 0x00 };
	
	memset(&insadr, 0, sizeof(insadr));
	insadr.sin_family = AF_INET;
	insadr.sin_port = IPPROTO_IGMP;
	//insadr.sin_addr.s_addr = inet_addr("224.0.0.22");
	insadr.sin_addr.s_addr = inet_addr("224.0.0.1");

	if (group) {
		msg[4] = group[0];
		msg[5] = group[1];
		msg[6] = group[2];
		msg[7] = group[3];
		memcpy(&insadr.sin_addr.s_addr, group, 4);
	}
	if (timeout) 
		msg[1] = timeout;

	calc_cs(msg, sizeof(msg));
	
	setsockopt(os->igmp_sock, IPPROTO_IP, IP_OPTIONS, ra, sizeof(ra));
	sendto(os->igmp_sock, msg, sizeof(msg), 0, (struct sockaddr *) &insadr, sizeof(insadr));
	dbgprintf(DEBUG_IGMP, "Queried group %03u.%03u.%03u.%03u\n",
		  msg[4],  msg[5], msg[6], msg[7]);
}

void check_igmp(struct octoserve *os)
{
	time_t tdiff, t;

	tdiff = mtime(&t) - os->igmp_time;
	switch (os->igmp_mode) {
	case 0:
		if (tdiff > 124) {
			os->igmp_timeout = 11;
			dbgprintf(DEBUG_IGMP,
				  "%u: IGMP master query, tag = %u, timeout = %u\n",
				  t, os->igmp_tag, os->igmp_timeout);
			os->igmp_tag++;
			send_igmp_query(os, 0, 0);
			os->igmp_time = t;
			os->igmp_mode = 1;
			if (os->igmp_robust) {
				os->igmp_robust--;
				os->igmp_time -= 94;
			}
		}
		break;
	case 1:
		if (tdiff > os->igmp_timeout) {
			dbgprintf(DEBUG_IGMP, "%u: IGMP timeout, tag = %u\n", t, os->igmp_tag);
			os->igmp_mode = 0;
		}
		break;
	case 2:
		/* check if query master timed out */
		if (tdiff < 255)
			return;
		/* yes, so we will have to query from now on */
		os->igmp_mode = 0;
		os->igmp_time = t - 94;
		os->igmp_robust = 1;
		break;
	case 3:
		if (tdiff > os->igmp_timeout) {
			dbgprintf(DEBUG_IGMP, "%u: IGMP timeout, tag = %u\n", t, os->igmp_tag);
			os->igmp_mode = 2;
		}
		break;
	}
}
