/**
 * @file msgparser.c
 * rdp2tcp commands parser
 */
/*
 * This file is part of rdp2tcp
 *
 * Copyright (C) 2010-2011, Nicolas Collignon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "print.h"
#include "iobuf.h"
#include "msgparser.h"

#include <stdio.h>
#ifndef _WIN32
#include <arpa/inet.h>
#else
#include <windows.h>
#endif

extern int debug_level;
extern const cmdhandler_t cmd_handlers[];

/*
static char const hexdigit[] = "0123456789abcdef";

static void dumpData(void *data, unsigned length) {
    unsigned const limit = 98;
    char hex[2];
    unsigned l = length>limit ? limit/2 : length;
    for (unsigned i=0; i<l; ++i) {
	hex[0] = hexdigit[(((char *)data)[i]>>4)&0xf];
	hex[1] = hexdigit[((char *)data)[i]&0xf];
        printf("%.2s", hex);
    }
    if (length>limit) {
        printf("...");
        for (unsigned i=length-l; i<length; ++i) {
	    hex[0] = hexdigit[(((char *)data)[i]>>4)&0xf];
	    hex[1] = hexdigit[((char *)data)[i]&0xf];
            printf("%.2s", hex);
	}
    }
    puts("");
}
*/

/**
 * parse rdp2tcp commands and call specific handlers
 * @param[in] ibuf input buffer
 * @return 0 on success, -1 on error
 */
int commands_parse(iobuf_t *ibuf)
{
	unsigned char cmd, *data;
	unsigned int off, msg_len, avail;
	int rc = 0;
	static const unsigned char r2t_min_size[R2TCMD_MAX] = {
		3, // R2TCMD_CONN
		2, // R2TCMD_CLOSE
		2, // R2TCMD_DATA
		1, // R2TCMD_PING
		3, // R2TCMD_BIND
		2  // R2TCMD_RCONN
	};

	assert(valid_iobuf(ibuf) && (iobuf_datalen(ibuf)>0));

#ifdef DEBUG
	if (debug_level > 0) iobuf_dump(ibuf);
#endif

	off   = 0;
	data  = iobuf_dataptr(ibuf);
	avail = iobuf_datalen(ibuf);
	debug(1, "commands_parse(avail=%u)", avail);
	//printf(">%8u ", (unsigned)avail);
	//dumpData(data, avail);

	// for each command
	while (off + 5 < avail) {

		msg_len = ntohl(*(unsigned int*)(data+off));
		if (!msg_len || (msg_len > RDP2TCP_MAX_MSGLEN))
			return error("invalid channel msg size 0x%08x", msg_len);

		if (off+msg_len+4 > avail)
			break;

		off += 4;

		cmd = data[off];
		unsigned char *msg = data+off;
		off += msg_len;
		if (cmd >= R2TCMD_MAX) {
			rc = error("invalid command id 0x%02x", cmd);
			break;
		}

		if (msg_len < (unsigned int)r2t_min_size[cmd]) {
			rc = error("command 0x%02x too short 0x%08x < 0x%08x", 
					cmd, msg_len, (unsigned int)r2t_min_size[cmd]);
			break;
		}

		if (!cmd_handlers[cmd]) {
			rc = error("command 0x%02x not supported", cmd);
			break;
		}

		// call specific command handler
		if (cmd_handlers[cmd]((const r2tmsg_t*)msg, msg_len)) {
			rc = -1;
			break;
		}
	}

	if (off > 0)
		iobuf_consume(ibuf, off);

	return rc;
}

// R2TERR_xxx error strings
const char *r2t_errors[R2TERR_MAX] = {
	"",
	"generic error",
	"bad message",
	"connection refused",
	"forbidden",
	"address not available",
	"failed to resolve hostname",
	"executable not found"
};

