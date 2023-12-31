/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008 Free Software Foundation, Inc.
 * Copyright 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <signal.h>
#include <stdlib.h>

#include <ortp/ortp.h>
#include <osipparser2/osip_md5.h>
#include <osipparser2/sdp_message.h>

#include "SIPInterface.h"
#include "SIPUtility.h"


using namespace SIP;
using namespace std;



#define DEBUG 1





bool SIP::get_owner_ip( osip_message_t * msg, char * o_addr )
{
	osip_body_t * sdp_body = (osip_body_t*)osip_list_get(&msg->bodies, 0);
	if (!sdp_body) return false;
	char * sdp_str = sdp_body->body;
	if (!sdp_str) return false;

	sdp_message_t * sdp;
	sdp_message_init(&sdp);
	sdp_message_parse(sdp, sdp_str);
	strcpy(o_addr, sdp->o_addr);
	return true;
}

bool SIP::get_rtp_params(const osip_message_t * msg, char * port, char * ip_addr )
{
	osip_body_t * sdp_body = (osip_body_t*)osip_list_get(&msg->bodies, 0);
	if (!sdp_body) return false;
	char * sdp_str = sdp_body->body;
	if (!sdp_str) return false;

	sdp_message_t * sdp;
	sdp_message_init(&sdp);
	sdp_message_parse(sdp, sdp_str);

	strcpy(port,sdp_message_m_port_get(sdp,0));
	strcpy(ip_addr, sdp->c_connection->c_addr);
	return true;
}

void SIP::make_tag(char * tag)
{
	uint64_t r1 = random();
	uint64_t r2 = random();
	uint64_t val = (r1<<32) + r2;
	
	// map [0->26] to [a-z] 
	int k;
	for (k=0; k<16; k++) {
		tag[k] = val%26+'a';
		val = val >> 4;
	}
	tag[k] = '\0';
}

void SIP::make_branch( char * branch )
{
	uint64_t r1 = random();
	uint64_t r2 = random();
	uint64_t val = (r1<<32) + r2;
	sprintf(branch,"z9hG4bKobts28%llx", val);
}

// vim: ts=4 sw=4
