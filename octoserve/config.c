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

void parse_config(struct octoserve *os, char *sec,
		  void (*cb)(struct octoserve *, char *, char *) )
{
	char line[256], csec[80], par[80], val[80], *p;
	FILE *f;

	if ((f = fopen ("/config/octoserve.conf", "r")) == NULL)
		return;
	
	while ((p = fgets(line, sizeof(line), f))) {
		if (*p == '\r' || *p == '\n' || *p == '#')
			continue;
		if (*p == '[') {
			if ((p = strtok(line + 1, "]")) == NULL)
				continue;
			strncpy(csec, p, sizeof(csec));
			//printf("current section %s\n", csec);
			if (!strcmp(sec, csec) && cb)
				cb(os, NULL, NULL);
			continue;
		}
		if (!(p = strtok(line, "=")))
			continue;
		strncpy(par, p, sizeof(par));
		if (!(p = strtok(NULL, "=")))
			continue;
		strncpy (val, p, sizeof(val));
		//printf("%s=%s\n", par, val);
		if (!strcmp(sec, csec) && cb)
			cb(os, par, val);
	}
	if (!strcmp(sec, csec) && cb)
		cb(os, NULL, NULL);
	fclose(f);
}
