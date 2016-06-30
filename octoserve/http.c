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

char httpxml[] =
	"HTTP/1.0 200 OK\r\nConnection: close\r\n"
	"Content-Length: %d\r\nContent-Type: text/xml\r\nMime-Version: 1.0\r\n"
	"\r\n";

char xmldesc[] = 
	"<\?xml version=\"1.0\"\?>\r\n"
	"<root xmlns=\"urn:schemas-upnp-org:device-1-0\" configId=\"%d\">\r\n"
	"<specVersion>\r\n<major>1</major>\r\n<minor>1</minor>\r\n</specVersion>\r\n"
	"<device>\r\n"
	"<deviceType>urn:ses-com:device:SatIPServer:1</deviceType>\r\n"
	"<friendlyName>%s</friendlyName>\r\n"
	"<manufacturer>Digital Devices GmbH</manufacturer>\r\n"
	"<manufacturerURL>http://www.digitaldevices.de</manufacturerURL>\r\n"
	"<modelDescription>OctopusNet</modelDescription>\r\n"
	"<modelName>OctopusNet</modelName>\r\n"
	"<modelNumber>1.0</modelNumber>\r\n"
	"<modelURL>http://www.digitaldevices.de/OctopusNet.html</modelURL>\r\n"
	"<serialNumber>%d</serialNumber>\r\n"
	"<UDN>%s</UDN>\r\n"
	//"<UPC>Universal Product Code</UPC>\r\n"
	"<iconList>\r\n"

	"<icon>\r\n<mimetype>image/png</mimetype>\r\n"
	"<width>120</width>\r\n<height>120</height>\r\n"
	"<depth>24</depth>\r\n<url>/icons/dd-120.png</url>\r\n"
	"</icon>\r\n"

	"<icon>\r\n<mimetype>image/jpeg</mimetype>\r\n"
	"<width>120</width>\r\n<height>120</height>\r\n"
	"<depth>24</depth>\r\n<url>/icons/dd-120.jpg</url>\r\n"
	"</icon>\r\n"

	"<icon>\r\n<mimetype>image/png</mimetype>\r\n"
	"<width>48</width>\r\n<height>48</height>\r\n"
	"<depth>24</depth>\r\n<url>/icons/dd-48.png</url>\r\n"
	"</icon>\r\n"

	"<icon>\r\n<mimetype>image/jpeg</mimetype>\r\n"
	"<width>48</width>\r\n<height>48</height>\r\n"
	"<depth>24</depth>\r\n<url>/icons/dd-48.jpg</url>\r\n"
	"</icon>\r\n"

	"</iconList>\r\n"
	"<presentationURL>/</presentationURL>\r\n"
	"<satip:X_SATIPCAP xmlns:satip=\"urn:ses-com:satip\">%s</satip:X_SATIPCAP>\r\n"
	"<satip:X_SATIPM3U xmlns:satip=\"urn:ses-com:satip\">/channellist.lua?select=m3u</satip:X_SATIPM3U>\r\n"
	"</device>\r\n"
	"</root>\r\n"
	"\r\n";

char httpfile[] =
	"HTTP/1.1 200 OK\r\nConnection: close\r\n"
	"Content-Length: %d\r\n"
	"Content-Type: %s\r\n"
	"\r\n";

void send_http_error(int sock, int nr)
{
	char buf[2048], *str;
	int len;

	if (nr == 404)
		str = "Not Found";
	if (nr == 405) 
		str = "Method not allowed";
	
	len = snprintf(buf, sizeof(buf), "HTTP/1.1 %d %s\r\nConnection: close\r\n\r\n", nr, str);
	
	if (len <= 0 || len >= sizeof(buf))
		return;
	sendlen(sock, buf, len);
}

static char *mtypes[] = {
	"jpg", "image/jpeg",
	"png", "image/png",
	"html", "text/html",
	NULL
};

void send_http_file(int sock, char *fn)
{
	uint8_t buf[1024];
	int len, len2, fd;
	char fn2[1024] = { 0 }, *d, **m;
		
	strcat(fn2, "/var/satip");
	strcat(fn2, fn);
	d = strrchr(fn, '.');
	if (d) {
		printf("%s\n", d);
		for (d++, m = &mtypes[0]; *m; m += 2) {
			if (!strcmp(*m, d)) {
				m++;
				break;
			}
		}
		d = *m;
	} 
	if (!d)
		d = "binary/octet-stream";
	
	printf("open %s %s\n", fn2, d);
	fd = open(fn2, O_RDONLY);
	if (fd < 0) {
		send_http_error(sock, 404);
		return;
	}
	len = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	len2 = sprintf(buf, httpfile, len, d);
	sendlen(sock, buf, len2);
	sendfile(sock, fd, 0, len);
	close(fd);
}

static int read_boxname(char *name)
{
	int fd, len;

	strcpy(name, "OctopusNet");
	fd = open("/config/boxname", O_RDONLY);
	if (fd < 0)
		return 0;
	printf("read boxname from /etc/boxname\n");
	len = read(fd, name, 79);
	if (len > 0)
		name[len] = 0;
	close(fd);
	return 0;
}

void send_xml(struct os_ssdp *ss)
{
	struct octoserve *os = ss->os;
	uint8_t buf[2048], buf2[1024], cap[1024];
	int len, len2;
	uint8_t *mac = &os->mac[0];
	int serial = (mac[5] | (mac[4] << 8) | (mac[3] << 16)) / 2;
	char boxname[80];

	read_boxname(boxname);
	len = 0;
	if (os->dvbs2num)
		len += sprintf(cap + len, ",DVBS2-%u", os->dvbs2num);
	if (os->dvbtnum)
		len += sprintf(cap + len, ",DVBT-%u", os->dvbtnum);
	if (os->dvbt2num)
		len += sprintf(cap + len, ",DVBT2-%u", os->dvbt2num);
	if (os->dvbcnum)
		len += sprintf(cap + len, ",DVBC-%u", os->dvbcnum);
	if (os->dvbc2num)
		len += sprintf(cap + len, ",DVBC2-%u", os->dvbc2num); 
	len = snprintf(buf, sizeof(buf), xmldesc,
		       ss->configid, boxname,
		       serial, ss->uuid_str, cap + 1);
	if (len <= 0 || len >= sizeof(buf))
		return;
	len2=sprintf(buf2, httpxml, len);
	sendlen(ss->csock, buf2, len2);
	sendlen(ss->csock, buf, len);
	//printf("Send:\n%s", buf2);
	//printf("%s\n", buf);
}

char httptxt[] =
	"HTTP/1.0 200 OK\r\nConnection: close\r\n"
	"Content-Type: text/html\r\n"
	"\r\n";

char httpjava[] =
	"HTTP/1.0 200 OK\r\nConnection: close\r\n"
	"Pragma: no-cache\r\n"
	"Cache-Control: no-cache\r\n"
	"Content-Type: application/x-javascript\r\n\r\n";

char httpjson[] =
	"HTTP/1.0 200 OK\r\nConnection: close\r\n"
	"Pragma: no-cache\r\n"
	"Cache-Control: no-cache\r\n"
	"Content-Type: application/json\r\n\r\n";

#define sendstr(_fd_,...) do {					\
		len = snprintf(buf, sizeof(buf), __VA_ARGS__);	\
		if (len <= 0 || len >= sizeof(buf))		\
			break;					\
		sendlen(_fd_, buf, len);			\
	} while (0);

void send_serverinfo(struct os_ssdp *ss)
{
	struct octoserve *os = ss->os;
	struct dvbfe *fe;
	uint8_t buf[2048];
	int i, j, fd = ss->csock, len;
	uint8_t *mac = &ss->os->mac[0];

	sendlen(fd, httpjava, sizeof(httpjava) - 1);
	sendstr(fd, "Octoserve = new Object();\r\n");
	sendstr(fd, "Octoserve.Version = \"" OCTOSERVE_VERSION "\";\r\n");
	sendstr(fd, "Octoserve.BootID = %u;\r\n", ss->bootid);
	sendstr(fd, "Octoserve.DeviceID = %u;\r\n", ss->devid);
	sendstr(fd, "Octoserve.MAC = \"%02x:%02x:%02x:%02x:%02x:%02x\";\r\n",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	sendstr(fd, "Octoserve.TunerList = new Array();\r\n");
	for (i = 0; i < os->dvbfe_num; i++) {
		struct dvbfe *fe = &os->dvbfe[i];

		if (fe->type) {
			char types[80];
			int pos;

			types[0] = 0;
			if (fe->type & 0xb00ee) {
				strcat(types, "DVB");
				pos = strlen(types);
				if (fe->type & (1 << SYS_DVBS))
					strcat(types, "/S");
				if (fe->type & (1 << SYS_DVBS2))
					strcat(types, "/S2");
				if (fe->type & (1 << SYS_DVBC_ANNEX_A))
					strcat(types, "/C");
				if (fe->type & (1 << SYS_DVBC2))
					strcat(types, "/C2");
				if (fe->type & (1 << SYS_DVBT))
					strcat(types, "/T");
				if (fe->type & (1 << SYS_DVBT2))
					strcat(types, "/T2");
				strcat(types, " ");
				types[pos] = '-';
			}
			if (fe->type & 0x700) {
				strcat(types, "ISDB");
				pos = strlen(types);
				if (fe->type & (1 << SYS_ISDBT))
					strcat(types, "/T");
				if (fe->type & (1 << SYS_ISDBS))
					strcat(types, "/S");
				if (fe->type & (1 << SYS_ISDBC))
					strcat(types, "/C");
				types[pos] = '-';
			}			
			sendstr(fd, "Octoserve.TunerList[%d] = new Object();\r\n", i);
			sendstr(fd, "Octoserve.TunerList[%d].Type = 0;\r\n", i);
			sendstr(fd, "Octoserve.TunerList[%d].Desc = \"%s\";\r\n", i, types);
		} else {
			sendstr(fd, "Octoserve.TunerList[%d] = false;\r\n", i);
		}
	}
}

void send_json_serverinfo(struct os_ssdp *ss)
{
	struct octoserve *os = ss->os;
	struct dvbfe *fe;
	uint8_t buf[2048];
	int i, j, fd = ss->csock, len;
	uint8_t *mac = &ss->os->mac[0];

	sendlen(fd, httpjson, sizeof(httpjson) - 1);
	sendstr(fd, "{\r\n");
	sendstr(fd, "\"Version\":" OCTOSERVE_VERSION ",\r\n");
	sendstr(fd, "\"BootID\":%u,\r\n", ss->bootid);
	sendstr(fd, "\"DeviceID\":%u,\r\n", ss->devid);
	sendstr(fd, "\"MAC\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\r\n",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	sendstr(fd, "TunerList\":[\r\n");
	for (i = 0; i < os->dvbfe_num; i++) {
		struct dvbfe *fe = &os->dvbfe[i];

		if (i)
			sendstr(fd, ",\r\n");
		sendstr(fd, "{\"Input\":\"%u\"", i);
		if (fe->type) {
			char types[80];
			int pos;

			types[0] = 0;
			if (fe->type & 0xb00ee) {
				strcat(types, "DVB");
				pos = strlen(types);
				if (fe->type & (1 << SYS_DVBS))
					strcat(types, "/S");
				if (fe->type & (1 << SYS_DVBS2))
					strcat(types, "/S2");
				if (fe->type & (1 << SYS_DVBC_ANNEX_A))
					strcat(types, "/C");
				if (fe->type & (1 << SYS_DVBC2))
					strcat(types, "/C2");
				if (fe->type & (1 << SYS_DVBT))
					strcat(types, "/T");
				if (fe->type & (1 << SYS_DVBT2))
					strcat(types, "/T2");
				strcat(types, " ");
				types[pos] = '-';
			}
			if (fe->type & 0x700) {
				strcat(types, "ISDB");
				pos = strlen(types);
				if (fe->type & (1 << SYS_ISDBT))
					strcat(types, "/T");
				if (fe->type & (1 << SYS_ISDBS))
					strcat(types, "/S");
				if (fe->type & (1 << SYS_ISDBC))
					strcat(types, "/C");
				types[pos] = '-';
			}			
			sendstr(fd, ",\"Present\":true", i);
			sendstr(fd, ",\"Type\":0");
			sendstr(fd, ",\"Desc\":\"%s\"", types);
		} else {
			sendstr(fd, "\"Present\":false", i);
		}
		sendstr(fd, "}");
	}
	sendstr(fd, "\r\n]\r\n}\r\n");
}

void send_tunerstatus(struct os_ssdp *ss)
{
	struct octoserve *os = ss->os;
	struct dvbfe *fe;
	uint8_t buf[2048];
	int len, i, fd = ss->csock;
	
	sendlen(fd, httpjava, sizeof(httpjava) - 1);
	sendstr(fd, "TunerList = new Array();\r\n");
	for (i = 0; i < os->dvbfe_num; i++) {
		fe = &os->dvbfe[i];
		sendstr(fd, "TunerList[%d] = new Object();\r\n", i);
		sendstr(fd, "TunerList[%d].Active = %s;\r\n", i, fe->state ? "true" : "false");
		if (!fe->state)
			continue;
		sendstr(fd, "TunerList[%d].Lock = %s;\r\n", i, fe->lock ? "true" : "false");
		sendstr(fd, "TunerList[%d].Quality = %u;\r\n", i, fe->quality);
		sendstr(fd, "TunerList[%d].Strength = %u;\r\n", i, fe->level);
	}
}

void send_json_tunerstatus(struct os_ssdp *ss)
{
	struct octoserve *os = ss->os;
	struct dvbfe *fe;
	uint8_t buf[2048];
	int len, i, fd = ss->csock;
	
	sendlen(fd, httpjson, sizeof(httpjson) - 1);
	sendstr(fd, "{\"TunerList\":[\r\n");
	for (i = 0; i < os->dvbfe_num; i++) {
		if (i)
			sendstr(fd, ",\r\n");
		fe = &os->dvbfe[i];
		sendstr(fd, "{\"Input\":\"%u\"", i);
		sendstr(fd, ",\"Status\":\"%s\"", fe->state ? "Active" : "Inactive");
		sendstr(fd, ",\"Lock\":%s", fe->lock ? "true" : "false");
		sendstr(fd, ",\"Strength\":%d", fe->strength);
		sendstr(fd, ",\"SNR\":%d", fe->snr);
		sendstr(fd, ",\"Quality\":%u", fe->quality);
		sendstr(fd, ",\"Level\":%u", fe->level);
		sendstr(fd, "}");
	}
	sendstr(fd, "\r\n]}\r\n");
}

static uint32_t ddreg(int fd, uint32_t reg)
{
	struct ddb_reg ddr = { .reg = reg };
	
	ioctl(fd, IOCTL_DDB_READ_REG, &ddr);
	return ddr.val;
}

static uint32_t ddrmem(int fd, uint8_t *buf, uint32_t off, uint32_t len)
{
	struct ddb_mem ddm = { .off = off, .buf = buf, .len = len };
	
	return ioctl(fd, IOCTL_DDB_READ_MEM, &ddm);
}

static void send_streamstatus(struct os_ssdp *ss)
{
	struct octoserve *os = ss->os;
	struct osstrm *oss;
	uint8_t buf[2048];
	int len, i, fd = ss->csock;
	struct timeval tval;
	struct timespec tp;
	int dd;

	dd = open("/dev/ddbridge/card0", O_RDWR); /* FIXME: replace with ioctls  */
	if (dd < 0) 
		return;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	gettimeofday(&tval, NULL);
	sendlen(fd, httpjava, sizeof(httpjava) - 1);
	sendstr(fd, "TimeStamp = %d;\r\n", (uint32_t) (tp.tv_sec * 1000 + tp.tv_nsec / 1000000));
	sendstr(fd, "StreamList = new Array();\r\n");
	for (i = 0; i < MAX_STREAM; i++) {
		uint32_t ctrl = ddreg(dd, 0x400 + i*0x20);

		sendstr(fd, "StreamList[%d] = new Object();\r\n", i);
		sendstr(fd, "StreamList[%d].Status = \"%s\";\r\n", i, (ctrl & 1) ? "Active" : "Inactive");
		sendstr(fd, "StreamList[%d].Input = %d;\r\n", i, (ctrl >> 8) & 7);
		sendstr(fd, "StreamList[%d].Packets = %d;\r\n", i, ddreg(dd, 0x41c + i*0x20));
		sendstr(fd, "StreamList[%d].Bytes = %d;\r\n", i, ddreg(dd, 0x418 + i*0x20));
		if (ctrl & 1) {
			uint32_t off = 0x2000 + i * 0x200;
			uint8_t mem[64];

			ddrmem(dd, mem, off, sizeof(mem));
			off = 30;
			if (mem[12] == 0x81)
				off += 4;
			sendstr(fd, "StreamList[%d].Client = \"%u.%u.%u.%u\";\r\n", i,
				mem[off], mem[off + 1], mem[off + 2], mem[off + 3]); 
		} else
			sendstr(fd, "StreamList[%d].Client = \"\";\r\n", i); 
	}
	close(dd);
}

static void send_json_streamstatus(struct os_ssdp *ss)
{
	struct octoserve *os = ss->os;
	struct osstrm *oss;
	uint8_t buf[2048];
	int len, i, fd = ss->csock;
	struct timeval tval;
	struct timespec tp;
	int dd;

	dd = open("/dev/ddbridge/card0", O_RDWR); /* FIXME: replace with ioctls  */
	if (dd < 0) 
		return;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	gettimeofday(&tval, NULL);
	sendlen(fd, httpjson, sizeof(httpjson) - 1);
	sendstr(fd, "{\"TimeStamp\":%u,\r\n", (uint32_t) (tp.tv_sec * 1000 + tp.tv_nsec / 1000000));
	sendstr(fd, "\"StreamList\":[\r\n");
	for (i = 0; i < MAX_STREAM; i++) {
		uint32_t ctrl = ddreg(dd, 0x400 + i*0x20);

		if (i)
			sendstr(fd, ",\r\n");
		sendstr(fd, "{\"Status\":\"%s\"", (ctrl & 1) ? "Active" : "Inactive");
		sendstr(fd, ",\"Input\":%u", (ctrl >> 8) & 7);
		sendstr(fd, ",\"Packets\":%u", ddreg(dd, 0x41c + i*0x20));
		sendstr(fd, ",\"Bytes\":%u", ddreg(dd, 0x418 + i*0x20));
		if (ctrl & 1) {
			uint32_t off = 0x2000 + i * 0x200;
			uint8_t mem[64];

			ddrmem(dd, mem, off, sizeof(mem));
			off = 30;
			if (mem[12] == 0x81)
				off += 4;
			sendstr(fd, ",\"Client\":\"%u.%u.%u.%u\"",
				mem[off], mem[off + 1], mem[off + 2], mem[off + 3]); 
		} else
			sendstr(fd, ",\"Client\":\"\"");
		sendstr(fd, "}");
	}
	sendstr(fd, "\r\n]\r\n}\r\n");
	close(dd);
}

static void send_octoserve(struct os_ssdp *ss)
{
	struct octoserve *os = ss->os;
	struct dvbfe *fe;
	uint8_t buf[2048], buf2[1024];
	int len, i;
	
	send(ss->csock, httptxt, sizeof(httptxt), 0);
	for (i = 0; i < os->dvbfe_num; i++) {
		fe = &os->dvbfe[i];
		if (!fe->state)
			continue;
		len = snprintf(buf, sizeof(buf), "tuner%d: level=%d, quality=%d\r\n", 
			       i, fe->level, fe->quality);
		if (len <= 0 || len >= sizeof(buf))
			return;
		send(ss->csock, buf, len, 0);
	}
}

void handle_http(struct os_ssdp *ss)
{
	int len;
	uint8_t buf[2048];
	int p = 0;

	while (1) {
		len = recv(ss->csock, buf + p, 2048 - p, 0);
		if (!len)
			return;
		if (len > 0) {
			p += len;
			if (p > sizeof(buf)) {
				send_http_error(ss->csock, 405);
				break;
			}
			if (buf[p-4] == '\r' && buf[p-3] == '\n' &&
			    buf[p-2] == '\r' && buf[p-1] == '\n') {
				if (strncasecmp("GET ", buf, 4)) {
					send_http_error(ss->csock, 405);
					break;
				}
				dbgprintf(DEBUG_SSDP, "%s\n", buf);
				if (!strncasecmp("GET /octonet.xml", buf, 16)) {
					send_xml(ss);
					break;
				} else if (!strncasecmp("GET /serverinfo.json", buf, 20)) {
					send_json_serverinfo(ss);
					break;
				} else if (!strncasecmp("GET /serverinfo.js", buf, 18)) {
					send_serverinfo(ss);
					break;
				} else if (!strncasecmp("GET /streamstatus.json", buf, 22)) {
					send_json_streamstatus(ss);
					break;
				} else if (!strncasecmp("GET /streamstatus.js", buf, 20)) {
					send_streamstatus(ss);
					break;
				} else if (!strncasecmp("GET /tunerstatus.json", buf, 21)) {
					send_json_tunerstatus(ss);
					break;
				} else if (!strncasecmp("GET /tunerstatus.js", buf, 19)) {
					send_tunerstatus(ss);
					break;
				} else {
					int i = 3, j, fd;

					while (buf[i] == ' ')
						i++;
					j = i;
					while (buf[j] && buf[j] != '\r' && buf[j] != ' ')
						j++;
					buf[j] = 0;
					if (i == j) {
						send_http_error(ss->csock, 404);
						break;
					}
					send_http_file(ss->csock, buf + i);
				}
				break;
			}
		}
	}
	close(ss->csock);
	return;
}

static void http_loop(struct octoserve *os)
{
	struct os_http *http = &os->http;
	int num;
	int mfd;
	fd_set fds;
	struct timeval timeout;

	while (!os->exit) {
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		FD_ZERO(&fds);
		mfd = 0;
		add_fd(http->sock, &mfd, &fds);

		num = select(mfd + 1, &fds, NULL, NULL, &timeout);
		if (num < 0)
			break;
		if (FD_ISSET(http->sock, &fds)) {
			socklen_t len;
			pthread_t pt;
			
			http->csock = accept(http->sock, &http->cadr, &len);
			//handle_http(http);
		}
	}
}

int init_http(struct octoserve *os)
{
	struct os_http *http = &os->http;

	http->os = os;
	http->sock = streamsock("8888", AF_INET, &http->sadr);
	if (listen(http->sock, 10) < 0) {
		printf("http listen error");
		return;
	}
	pthread_create(&os->http.pt, NULL, (void *) handle_http, os);
}
