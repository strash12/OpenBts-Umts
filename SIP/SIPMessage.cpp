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

#include <ortp/ortp.h>
#include <osipparser2/sdp_message.h>
#include <osipparser2/osip_md5.h>

#include "SIPInterface.h"
#include "SIPUtility.h"
#include "SIPMessage.h"

using namespace std;
using namespace SIP;


#define DEBUG 1
#define MAX_VIA 10


osip_message_t * SIP::sip_register( const char * sip_username, short timeout, short wlocal_port, const char * local_ip, const char * proxy_ip, const char * from_tag, const char * via_branch, const char * call_id, int cseq, string *RAND, const char *IMSI, const char *SRES) {

 	char local_port[10];
	sprintf(local_port,"%i",wlocal_port);	
	
	// Message URI
	osip_message_t * request;
	osip_message_init(&request);
	// FIXME -- Should use the "force_update" function.
	request->message_property = 2; // buffer is not synchronized with object
	request->sip_method = strdup("REGISTER");
	osip_message_set_version(request, strdup("SIP/2.0"));	
	osip_uri_init(&request->req_uri);
	osip_uri_set_host(request->req_uri, strdup(proxy_ip));

	
	// VIA
	osip_via_t * via;
	osip_via_init(&via);
	via_set_version(via, strdup("2.0"));
	via_set_protocol(via, strdup("UDP"));
	via_set_host(via, strdup(local_ip));
	via_set_port(via, strdup(local_port));

	// VIA BRANCH
	osip_via_set_branch(via, strdup(via_branch));

	// MAX FORWARDS
	osip_message_set_max_forwards(request, strdup(gConfig.getStr("SIP.MaxForwards").c_str()));

	char  * via_str;
	osip_via_to_str(via, &via_str);
	osip_message_set_via(request, via_str);
	// set via
	osip_list_add(&request->vias, via, -1);


	// FROM
	osip_from_init(&request->from);
	osip_from_set_displayname(request->from, strdup(sip_username));

	// FROM TAG
	osip_from_set_tag(request->from, strdup(from_tag));
	osip_uri_init(&request->from->url);
	osip_uri_set_host(request->from->url, strdup(proxy_ip));
	osip_uri_set_username(request->from->url, strdup(sip_username));

	// TO
	osip_to_init(&request->to);
	osip_to_set_displayname(request->to, strdup(sip_username));
	osip_uri_init(&request->to->url);
	osip_uri_set_host(request->to->url, strdup(proxy_ip));
	osip_uri_set_username(request->to->url, strdup(sip_username));
	
	// CALL ID
	osip_call_id_init(&request->call_id);
	osip_call_id_set_host(request->call_id, strdup(local_ip));
	osip_call_id_set_number(request->call_id, strdup(call_id));

	// CSEQ
	osip_cseq_init(&request->cseq);
	osip_cseq_set_method(request->cseq, strdup("REGISTER"));
	char temp_buf[14];
	sprintf(temp_buf,"%i",cseq);
	osip_cseq_set_number(request->cseq, strdup(temp_buf));	

	// CONTACT
	osip_contact_t * con;
	osip_to_init(&con);

	// CONTACT URI
	osip_uri_init(&con->url);
	osip_uri_set_host(con->url, strdup(local_ip));
	osip_uri_set_port(con->url, strdup(local_port));
	osip_uri_set_username(con->url, strdup(sip_username));
	char numbuf[10];
	sprintf(numbuf,"%d",timeout);
	osip_contact_param_add(con, strdup("expires"), strdup(numbuf) );

	// add contact
	osip_list_add(&request->contacts, con, -1);

	if (SRES) {
		// add authentication
		osip_authorization_t *auth;
		osip_authorization_init(&auth);
		osip_authorization_set_auth_type(auth, osip_strdup("Digest"));
		osip_authorization_set_nonce(auth, osip_strdup(RAND->c_str()));
		osip_authorization_set_uri(auth, osip_strdup(IMSI));
		osip_authorization_set_response(auth, osip_strdup(SRES));
		osip_list_add(&request->authorizations, auth, -1);
	}

	return request;	
}



osip_message_t * SIP::sip_message( const char * dialed_number, const char * sip_username, short wlocal_port, const char * local_ip, const char * proxy_ip, const char * from_tag, const char * via_branch, const char * call_id, int cseq, const char* message, const char* content_type) {

	char local_port[10];
	sprintf(local_port, "%i", wlocal_port);

	osip_message_t * request;
	osip_message_init(&request);
	// FIXME -- Should use the "force_update" function.
	request->message_property = 2;

	// METHOD
	request->sip_method = strdup("MESSAGE");
	osip_message_set_version(request, strdup("SIP/2.0"));	

	// REQ.URI
	osip_uri_init(&request->req_uri);
	osip_uri_set_host(request->req_uri, strdup(proxy_ip));
	osip_uri_set_username(request->req_uri, strdup(dialed_number));
	
	// VIA
	osip_via_t * via;
	osip_via_init(&via);
	via_set_version(via, strdup("2.0"));
	via_set_protocol(via, strdup("UDP"));
	via_set_host(via, strdup(local_ip));
	via_set_port(via, strdup(local_port));
	osip_via_set_branch(via, strdup(via_branch));

	// MAX FORWARDS
	osip_message_set_max_forwards(request, strdup(gConfig.getStr("SIP.MaxForwards").c_str()));

	// add via
	osip_list_add(&request->vias, via, -1);

	// FROM
	osip_from_init(&request->from);
	osip_from_set_displayname(request->from, strdup(sip_username));
	osip_uri_init(&request->from->url);
	osip_uri_set_host(request->from->url, strdup(proxy_ip));
	osip_uri_set_username(request->from->url, strdup(sip_username));
	// FROM TAG
	osip_from_set_tag(request->from, strdup(from_tag));

	// TO
	osip_to_init(&request->to);
	osip_to_set_displayname(request->to, strdup(dialed_number));
	osip_uri_init(&request->to->url);
	osip_uri_set_host(request->to->url, strdup(proxy_ip));
	osip_uri_set_username(request->to->url, strdup(dialed_number));

	// CALL ID
	osip_call_id_init(&request->call_id);
	osip_call_id_set_host(request->call_id, strdup(local_ip));
	osip_call_id_set_number(request->call_id, strdup(call_id));

	// CSEQ
	osip_cseq_init(&request->cseq);
	osip_cseq_set_method(request->cseq, strdup("MESSAGE"));
	char temp_buf[21];
	sprintf(temp_buf,"%i",cseq);
	osip_cseq_set_number(request->cseq, strdup(temp_buf));	

	// Content-Type
	if (content_type)
	{
		// Explicit value provided
		osip_message_set_content_type(request, strdup(content_type));
	} else {
		// Default to text/plain
		osip_message_set_content_type(request, strdup("text/plain"));
	}

	// Content-Length
	sprintf(temp_buf,"%u",strlen(message));
	osip_message_set_content_length(request, strdup(temp_buf));

	// Payload.
	osip_message_set_body(request,message,strlen(message));

	return request;	
}


osip_message_t * SIP::sip_invite5031(short rtp_port, const char * sip_username, short wlocal_port, const char * local_ip, const char* proxy_ip, const char * from_tag, const char * via_branch, const char * call_id, int cseq, unsigned codec)
{
	char local_port[10];
	sprintf(local_port, "%i", wlocal_port);

	osip_message_t * request;
	osip_message_init(&request);
	// FIXME -- Should use the "force_update" function.
	request->message_property = 2;
	request->sip_method = strdup("INVITE");
	osip_message_set_version(request, strdup("SIP/2.0"));	
	osip_uri_init(&request->req_uri);
	osip_uri_set_scheme(request->req_uri, strdup("sip"));
	osip_uri_set_username(request->req_uri, strdup("sos"));
	osip_uri_set_host(request->req_uri, strdup(proxy_ip));
	
	// VIA
	osip_via_t * via;
	osip_via_init(&via);
	via_set_version(via, strdup("2.0"));
	via_set_protocol(via, strdup("UDP"));
	via_set_host(via, strdup(local_ip));
	via_set_port(via, strdup(local_port));

	// VIA BRANCH
	osip_via_set_branch(via, strdup(via_branch));

	// MAX FORWARDS
	osip_message_set_max_forwards(request, strdup(gConfig.getStr("SIP.MaxForwards").c_str()));

	// add via
	osip_list_add(&request->vias, via, -1);

	// FROM
	osip_from_init(&request->from);
	osip_from_set_displayname(request->from, strdup(sip_username));

	// FROM TAG
	osip_from_set_tag(request->from, strdup(from_tag));

	osip_uri_init(&request->from->url);
	osip_uri_set_host(request->from->url, strdup(local_ip));
	osip_uri_set_username(request->from->url, strdup(sip_username));

	// TO
	osip_to_init(&request->to);
	osip_to_set_displayname(request->to, strdup(""));
	osip_uri_init(&request->to->url);
	osip_uri_set_host(request->to->url, strdup(gConfig.getStr("Control.Emergency.Destination.Host").c_str()));
	osip_uri_set_username(request->to->url, strdup(gConfig.getStr("Control.Emergency.Destination.User").c_str()));

	// If response, we need a to tag.
	//osip_uri_param_t * to_tag_param;
	//osip_from_get_tag(rsp->to, &to_tag_param);

	// CALL ID
	osip_call_id_init(&request->call_id);
	osip_call_id_set_host(request->call_id, strdup(local_ip));
	osip_call_id_set_number(request->call_id, strdup(call_id));

	// CSEQ
	osip_cseq_init(&request->cseq);
	osip_cseq_set_method(request->cseq, strdup("INVITE"));
	char temp_buf[14];
	sprintf(temp_buf,"%i",cseq);
	osip_cseq_set_number(request->cseq, strdup(temp_buf));	

	// CONTACT
	osip_contact_t * con;
	osip_to_init(&con);

	// CONTACT URI
	osip_uri_init(&con->url);
	osip_uri_set_host(con->url, strdup(local_ip));
	osip_uri_set_port(con->url, strdup(local_port));
	osip_uri_set_username(con->url, strdup(sip_username));
	osip_contact_param_add(con, strdup("expires"), strdup("3600") );

	// add contact
	osip_list_add(&request->contacts, con, -1);

	sdp_message_t * sdp;
	sdp_message_init(&sdp);
	sdp_message_v_version_set(sdp, strdup("0"));
	sdp_message_o_origin_set(sdp, strdup(sip_username), strdup("0"),
        strdup("0"), strdup("IN"), strdup("IP4"), strdup(local_ip));

	sdp_message_s_name_set(sdp, strdup("Talk Time"));
	sdp_message_t_time_descr_add(sdp, strdup("0"), strdup("0") );

	sprintf(temp_buf,"%i",rtp_port);
	sdp_message_m_media_add(sdp, strdup("audio"), 
		strdup(temp_buf), NULL, strdup("RTP/AVP"));
	sdp_message_c_connection_add
        (sdp, 0, strdup("IN"), strdup("IP4"), strdup(local_ip),NULL, NULL);

	// FIXME -- This should also be inside the switch?
	sdp_message_m_payload_add(sdp,0,strdup("3"));
	switch (codec) {
		case RTPuLaw:
			sdp_message_a_attribute_add(sdp,0,strdup("rtpmap"),strdup("0 PCMU/8000"));
			break;
		case RTPGSM610:
			sdp_message_a_attribute_add(sdp,0,strdup("rtpmap"),strdup("3 GSM/8000"));
			break;
		default: assert(0);
	};

	/*
	 * We construct a sdp_message_t, turn it into a string, and then treat it
	 * like an osip_body_t.  This works, and perhaps is how it is supposed to
	 * be done, but in any case we're going to have to do the extra processing
	 * to turn it into a string first.
	 */
	char * sdp_str;
	sdp_message_to_str(sdp, &sdp_str);
	osip_message_set_body(request, sdp_str, strlen(sdp_str));
	osip_free(sdp_str);
	osip_message_set_content_type(request, strdup("application/sdp"));

	return request;	
}


osip_message_t * SIP::sip_invite( const char * dialed_number, short rtp_port, const char * sip_username, short wlocal_port, const char * local_ip, const char * proxy_ip, const char * from_tag, const char * via_branch, const char * call_id, int cseq, unsigned codec) {

	char local_port[10];
	sprintf(local_port, "%i", wlocal_port);

	osip_message_t * request;
	osip_message_init(&request);
	// FIXME -- Should use the "force_update" function.
	request->message_property = 2;
	request->sip_method = strdup("INVITE");
	osip_message_set_version(request, strdup("SIP/2.0"));	
	osip_uri_init(&request->req_uri);
	osip_uri_set_host(request->req_uri, strdup(proxy_ip));
	osip_uri_set_username(request->req_uri, strdup(dialed_number));
	
	// VIA
	osip_via_t * via;
	osip_via_init(&via);
	via_set_version(via, strdup("2.0"));
	via_set_protocol(via, strdup("UDP"));
	via_set_host(via, strdup(local_ip));
	via_set_port(via, strdup(local_port));

	// VIA BRANCH
	osip_via_set_branch(via, strdup(via_branch));

	// MAX FORWARDS
	osip_message_set_max_forwards(request, strdup(gConfig.getStr("SIP.MaxForwards").c_str()));

	// add via
	osip_list_add(&request->vias, via, -1);

	// FROM
	osip_from_init(&request->from);
	osip_from_set_displayname(request->from, strdup(sip_username));

	// FROM TAG
	osip_from_set_tag(request->from, strdup(from_tag));

	osip_uri_init(&request->from->url);
	osip_uri_set_host(request->from->url, strdup(proxy_ip));
	osip_uri_set_username(request->from->url, strdup(sip_username));

	// TO
	osip_to_init(&request->to);
	osip_to_set_displayname(request->to, strdup(""));
	osip_uri_init(&request->to->url);
	osip_uri_set_host(request->to->url, strdup(proxy_ip));
	osip_uri_set_username(request->to->url, strdup(dialed_number));

	// If response, we need a to tag.
	//osip_uri_param_t * to_tag_param;
	//osip_from_get_tag(rsp->to, &to_tag_param);

	// CALL ID
	osip_call_id_init(&request->call_id);
	osip_call_id_set_host(request->call_id, strdup(local_ip));
	osip_call_id_set_number(request->call_id, strdup(call_id));

	// CSEQ
	osip_cseq_init(&request->cseq);
	osip_cseq_set_method(request->cseq, strdup("INVITE"));
	char temp_buf[14];
	sprintf(temp_buf,"%i",cseq);
	osip_cseq_set_number(request->cseq, strdup(temp_buf));	

	// CONTACT
	osip_contact_t * con;
	osip_to_init(&con);

	// CONTACT URI
	osip_uri_init(&con->url);
	osip_uri_set_host(con->url, strdup(local_ip));
	osip_uri_set_port(con->url, strdup(local_port));
	osip_uri_set_username(con->url, strdup(sip_username));
	osip_contact_param_add(con, strdup("expires"), strdup("3600") );

	// add contact
	osip_list_add(&request->contacts, con, -1);

	sdp_message_t * sdp;
	sdp_message_init(&sdp);
	sdp_message_v_version_set(sdp, strdup("0"));
	sdp_message_o_origin_set(sdp, strdup(sip_username), strdup("0"),
        strdup("0"), strdup("IN"), strdup("IP4"), strdup(local_ip));

	sdp_message_s_name_set(sdp, strdup("Talk Time"));
	sdp_message_t_time_descr_add(sdp, strdup("0"), strdup("0") );

	sprintf(temp_buf,"%i",rtp_port);
	sdp_message_m_media_add(sdp, strdup("audio"), 
		strdup(temp_buf), NULL, strdup("RTP/AVP"));
	sdp_message_c_connection_add
        (sdp, 0, strdup("IN"), strdup("IP4"), strdup(local_ip),NULL, NULL);

	// FIXME -- This should also be inside the switch?
	sdp_message_m_payload_add(sdp,0,strdup("3"));
	switch (codec) {
		case RTPuLaw:
			sdp_message_a_attribute_add(sdp,0,strdup("rtpmap"),strdup("0 PCMU/8000"));
			break;
		case RTPGSM610:
			sdp_message_a_attribute_add(sdp,0,strdup("rtpmap"),strdup("3 GSM/8000"));
			break;
		default: assert(0);
	};

	/*
	 * We construct a sdp_message_t, turn it into a string, and then treat it
	 * like an osip_body_t.  This works, and perhaps is how it is supposed to
	 * be done, but in any case we're going to have to do the extra processing
	 * to turn it into a string first.
	 */
	char * sdp_str;
	sdp_message_to_str(sdp, &sdp_str);
	osip_message_set_body(request, sdp_str, strlen(sdp_str));
	osip_free(sdp_str);
	osip_message_set_content_type(request, strdup("application/sdp"));

	return request;	
}


// Take the authorization produced by an earlier invite message.

osip_message_t * SIP::sip_ack(const char * req_uri, const char * dialed_number, const char * sip_username, short wlocal_port, const char * local_ip, const char * proxy_ip, const osip_from_t *from_header, const osip_to_t* to_header, const char * via_branch, const osip_call_id_t* call_id_header, int cseq) {

	char local_port[20];
	sprintf(local_port, "%i", wlocal_port);

	osip_message_t * ack;
	osip_message_init(&ack);
	// FIXME -- Should use the "force_update" function.
	ack->message_property = 2;
	ack->sip_method = strdup("ACK");
	osip_message_set_version(ack, strdup("SIP/2.0"));	

	osip_uri_init(&ack->req_uri);

	// If we are Acking a BYE message then need to 
	// set the req_uri to the owner address thats taken from the 200 Okay.
	if( req_uri == NULL ) {
		osip_uri_set_host(ack->req_uri, strdup(proxy_ip));
	} else {
		osip_uri_set_host(ack->req_uri, strdup(req_uri));
	}

	osip_uri_set_username(ack->req_uri, strdup(dialed_number));

	// Via
	osip_via_t *via;
	osip_via_init(&via);
	via_set_version(via, strdup("2.0"));
	via_set_protocol(via, strdup("UDP"));
	via_set_host(via, strdup(local_ip));
	via_set_port(via, strdup(local_port));

	// VIA BRANCH
	osip_via_set_branch(via, strdup(via_branch));

	// MAX FORWARDS
	osip_message_set_max_forwards(ack, strdup(gConfig.getStr("SIP.MaxForwards").c_str()));

	// add via
	osip_list_add(&ack->vias, via, -1);

	osip_from_init(&ack->from);
	osip_from_set_displayname(ack->from, strdup(sip_username));
	osip_uri_init(&ack->from->url);
	osip_uri_set_host(ack->from->url, strdup(proxy_ip));
	osip_uri_set_username(ack->from->url, strdup(sip_username));

	// from/to headers
	osip_from_clone(from_header, &ack->from);
	osip_to_clone(to_header, &ack->to);

	// call id
	osip_call_id_clone(call_id_header, &ack->call_id);

	osip_cseq_init(&ack->cseq);
	osip_cseq_set_method(ack->cseq, strdup("ACK"));
	char temp_buf[14];
	sprintf(temp_buf, "%i", cseq);
	osip_cseq_set_number(ack->cseq, strdup(temp_buf));	

	return ack;
}


osip_message_t * SIP::sip_bye(const char * req_uri, const char * dialed_number, const char * sip_username, short wlocal_port, const char * local_ip, const char * proxy_ip, short wproxy_port, const osip_from_t* from_header, const osip_to_t* to_header, const char * via_branch, const osip_call_id_t* call_id_header, int cseq) {

	// FIXME -- We really need some NULL-value error checking in here.

	char local_port[10];
	sprintf(local_port,"%i",wlocal_port);

	char proxy_port[10];
	sprintf(proxy_port,"%i",wproxy_port);

	osip_message_t * bye;
	osip_message_init(&bye);
	// FIXME -- Should use the "force_update" function.
	bye->message_property = 2;
	bye->sip_method = strdup("BYE");
	osip_message_set_version(bye, strdup("SIP/2.0"));	

	//char o_addr[30];
	//get_owner_ip(okay, o_addr);

	osip_uri_init(&bye->req_uri);
	osip_uri_set_host(bye->req_uri, strdup(req_uri));
	osip_uri_set_username(bye->req_uri, strdup(dialed_number));

	osip_via_t * via;
	osip_via_init(&via);
	via_set_version(via, strdup("2.0"));
	via_set_protocol(via, strdup("UDP"));
	via_set_host(via, strdup(local_ip));
	via_set_port(via, strdup(local_port));

	// via branch + max forwards
	osip_via_set_branch(via, strdup(via_branch));
	osip_message_set_max_forwards(bye, strdup(gConfig.getStr("SIP.MaxForwards").c_str()));

	// add via
	osip_list_add(&bye->vias, via, -1);

	// from/to header
	osip_from_clone(from_header, &bye->from);
	osip_to_clone(to_header, &bye->to);

	// Call Id Header	
	osip_call_id_clone(call_id_header, &bye->call_id);

	// Cseq Number
	osip_cseq_init(&bye->cseq);
	osip_cseq_set_method(bye->cseq, strdup("BYE"));
	char temp_buf[12];
	sprintf(temp_buf,"%i",cseq);
	osip_cseq_set_number(bye->cseq, strdup(temp_buf));	

	// Contact
	osip_contact_t * contact;
	osip_contact_init(&contact);
	osip_contact_set_displayname(contact, strdup(sip_username) );	
	osip_uri_init(&contact->url);
	osip_uri_set_host(contact->url, strdup(local_ip));
	osip_uri_set_username(contact->url, strdup(sip_username));
	osip_uri_set_port(contact->url, strdup(local_port));

	// add contact
	osip_list_add(&bye->contacts, contact, -1);

	return bye;
}


osip_message_t * SIP::sip_okay( osip_message_t * inv, const char * sip_username, const char * local_ip, short wlocal_port, short rtp_port, unsigned audio_codec)
{

	// Check for consistency.
	if(inv==NULL){ return NULL;}

	char local_port[10];
	sprintf(local_port, "%i", wlocal_port);
	// k used for error conditions on various osip operations.
	
	osip_message_t * okay;
	osip_message_init(&okay);
	// FIXME -- Should use the "force_update" function.
	okay->message_property = 2;

	// Set Header stuff.
	okay->status_code = 200;	
	okay->reason_phrase = strdup("OK");
	osip_message_set_version(okay, strdup("SIP/2.0"));
	osip_uri_init(&okay->req_uri);

	// Get Record Route.
	// FIXME -- Should use _clone() routines.
	osip_record_route_t * rr;
	char * rr_str;
	osip_message_get_record_route(inv, 0, &rr);
	osip_record_route_to_str(rr, &rr_str);
	osip_message_set_record_route(okay, rr_str);
	osip_free(rr_str);


	// SIP Okay needs to repeat the Via tags from the INVITE Message.
	osip_via_t * via;
	char * via_str;
	osip_message_get_via(inv, 0, &via);
	osip_via_to_str(via, &via_str);
	osip_message_set_via(okay, via_str);
	osip_free(via_str);

	// from/to header
	osip_from_clone(inv->from, &okay->from);
	osip_to_clone(inv->to, &okay->to);

	// CONTACT URI
	osip_contact_t * con;
	osip_to_init(&con);
	osip_uri_init(&con->url);
	osip_uri_set_host(con->url, strdup(local_ip));
	osip_uri_set_port(con->url, strdup(local_port));
	osip_uri_set_username(con->url, strdup(sip_username));
	osip_contact_param_add(con, strdup("expires"), strdup("3600") );

	// add contact
	osip_list_add(&okay->contacts, con, -1);

	// Get Call-ID.
	osip_call_id_clone(inv->call_id, &okay->call_id);

	// Get Cseq.
	osip_cseq_t * cseq;
	char * cseq_str;
	cseq = osip_message_get_cseq(inv);
	osip_cseq_to_str(cseq ,&cseq_str);
	osip_message_set_cseq(okay, cseq_str);	
	osip_free(cseq_str);

	// Session Description Protocol.	
	sdp_message_t * sdp;
	sdp_message_init(&sdp);
	sdp_message_v_version_set(sdp, strdup("0"));
	sdp_message_o_origin_set(sdp, strdup(sip_username), strdup("0"),
        strdup("0"), strdup("IN"), strdup("IP4"), strdup(local_ip));

	sdp_message_s_name_set(sdp, strdup("Talk Time"));
	sdp_message_t_time_descr_add(sdp, strdup("0"), strdup("0") );
	char temp_buf[10];
	sprintf(temp_buf,"%i", rtp_port);
	sdp_message_m_media_add(sdp, strdup("audio"), 
		strdup(temp_buf), NULL, strdup("RTP/AVP"));
	sdp_message_c_connection_add
        (sdp, 0, strdup("IN"), strdup("IP4"), strdup(local_ip),NULL, NULL);

	// FIXME -- This should also be inside the switch?
	sdp_message_m_payload_add(sdp,0,strdup("3"));
	switch (audio_codec) {
		case RTPuLaw:
			sdp_message_a_attribute_add(sdp,0,strdup("rtpmap"),strdup("0 PCMU/8000"));
			break;
		case RTPGSM610:
			sdp_message_a_attribute_add(sdp,0,strdup("rtpmap"),strdup("3 GSM/8000"));
			break;
		default: assert(0);
	};

	char * sdp_str;
	sdp_message_to_str(sdp, &sdp_str);
	osip_message_set_body(okay, sdp_str, strlen(sdp_str));
	osip_free(sdp_str);

	osip_message_set_content_type(okay, strdup("application/sdp"));

	return okay;
}


osip_message_t * SIP::sip_b_okay( osip_message_t * bye  )
{
	// Check for consistency.
	if(bye==NULL){ return NULL;}

	// k used for error conditions on various osip operations.
	
	osip_message_t * okay;
	osip_message_init(&okay);
	// FIXME -- Should use the "force_update" function.
	okay->message_property = 2;

	// Set Header stuff.
	okay->status_code = 200;	
	okay->reason_phrase = strdup("OK");
	osip_message_set_version(okay, strdup("SIP/2.0"));
	osip_uri_init(&okay->req_uri);

	// SIP Okay needs to repeat the Via tags from the BYE Message.
	osip_via_t * via;
	char * via_str;
	osip_message_get_via(bye, 0, &via);
	osip_via_to_str(via, &via_str);
	osip_message_set_via(okay, via_str);	
	osip_free(via_str);

	// from/to header
	osip_from_clone(bye->from, &okay->from);
	osip_to_clone(bye->to, &okay->to);

	// Get Call-ID.
	osip_call_id_clone(bye->call_id, &okay->call_id);

	// Get Cseq.
	osip_cseq_t * cseq;
	char * cseq_str;
	cseq = osip_message_get_cseq(bye);
	osip_cseq_to_str(cseq ,&cseq_str);
	osip_message_set_cseq(okay, cseq_str);	
	osip_free(cseq_str);

	return okay;
}


osip_message_t * SIP::sip_trying( osip_message_t * invite, const char * sip_username, const char * local_ip )
{
	osip_message_t * trying;
	osip_message_init(&trying);
	// FIXME -- Should use the "force_update" function.
	trying->message_property = 2;

	// Set Header stuff.
	trying->status_code = 100;	
	trying->reason_phrase = strdup("Trying");
	osip_message_set_version(trying, strdup("SIP/2.0"));
	osip_uri_init(&invite->req_uri);	// FIXME? -- Invite rather than trying?

	// Get Record Route.
	osip_via_t * via;
	char * via_str;
	osip_message_get_via(invite, 0, &via);
	osip_via_to_str(via, &via_str);
	osip_message_set_via(trying, via_str);	
	osip_free(via_str);
	
	// from/to header
	osip_from_clone(invite->from, &trying->from);
	osip_to_clone(invite->to, &trying->to);

	// Get Call-ID.
	osip_call_id_clone(invite->call_id, &trying->call_id);

	// Get Cseq.
	osip_cseq_t * cseq;
	char * cseq_str;
	cseq = osip_message_get_cseq(invite);
	osip_cseq_to_str(cseq ,&cseq_str);
	osip_message_set_cseq(trying, cseq_str);	
	osip_free(cseq_str);

	// CONTACT URI
	osip_contact_t * con;
	osip_to_init(&con);
	osip_uri_init(&con->url);
	osip_uri_set_host(con->url, strdup(local_ip));
	//osip_uri_set_port(con->url, strdup(local_port));	// FIXME ??
	osip_uri_set_username(con->url, strdup(sip_username));

	// add contact
	osip_list_add(&trying->contacts, con, -1);

	return trying;
}


osip_message_t * SIP::sip_ringing( osip_message_t * invite, const char * sip_username, const char *local_ip)
{
	osip_message_t * ringing;
	osip_message_init(&ringing);
	// FIXME -- Should use the "force_update" function.
	ringing->message_property = 2;

	// Set Header stuff.
	ringing->status_code = 180;	
	ringing->reason_phrase = strdup("Ringing");
	osip_message_set_version(ringing, strdup("SIP/2.0"));
	//osip_uri_init(&invite->req_uri);

	// Get Record Route.
	osip_via_t * via;
	char * via_str;
	osip_message_get_via(invite, 0, &via);
	osip_via_to_str(via, &via_str);
	osip_message_set_via(ringing, via_str);	
	osip_free(via_str);
	
	// from/to header
	osip_from_clone(invite->from, &ringing->from);
	osip_to_clone(invite->to, &ringing->to);

	// Get Call-ID.
	osip_call_id_clone(invite->call_id, &ringing->call_id);

	// Get Cseq.
	osip_cseq_t * cseq;
	char * cseq_str;
	cseq = osip_message_get_cseq(invite);
	osip_cseq_to_str(cseq ,&cseq_str);
	osip_message_set_cseq(ringing, cseq_str);	
	osip_free(cseq_str);

	// CONTACT URI
	osip_contact_t * con;
	osip_to_init(&con);
	osip_uri_init(&con->url);
	osip_uri_set_host(con->url, strdup(local_ip));
	osip_uri_set_username(con->url, strdup(sip_username));

	// add contact
	osip_list_add(&ringing->contacts, con, -1);

	return ringing;
}


osip_message_t * SIP::sip_okay_SMS( osip_message_t * inv, const char * sip_username, const char * local_ip, short wlocal_port)
{

	// Check for consistency.
	if(inv==NULL){ return NULL;}

	char local_port[20];
	sprintf(local_port, "%i", wlocal_port);

	osip_message_t * okay;
	osip_message_init(&okay);
	// FIXME -- Should use the "force_update" function.
	okay->message_property = 2;

	// FIXME -- Do we really need all of this string conversion?

	// Set Header stuff.
	okay->status_code = 200;	
	okay->reason_phrase = strdup("OK");
	osip_message_set_version(okay, strdup("SIP/2.0"));
	osip_uri_init(&okay->req_uri);

	// Get Record Route.
	osip_record_route_t * rr;
	char * rr_str;
	osip_message_get_record_route(inv, 0, &rr);
	osip_record_route_to_str(rr, &rr_str);
	osip_message_set_record_route(okay, rr_str);
	osip_free(rr_str);

	// SIP Okay needs to repeat the Via tags from the INVITE Message.
//	osip_via_clone(inv->via, &okay->via);
	osip_via_t * via;
	char * via_str;
	osip_message_get_via(inv, 1, &via);
	osip_via_to_str(via, &via_str);
	osip_message_set_via(okay, via_str);	
	osip_free(via_str);

	// from/to header
	osip_from_clone(inv->from, &okay->from);
	osip_to_clone(inv->to, &okay->to);

	// Get Call-ID.
	osip_call_id_clone(inv->call_id, &okay->call_id);

	// Get Cseq.
	osip_cseq_t * cseq;
	char * cseq_str;
	cseq = osip_message_get_cseq(inv);
	osip_cseq_to_str(cseq ,&cseq_str);
	osip_message_set_cseq(okay, cseq_str);	
	osip_free(cseq_str);

	return okay;
}


osip_message_t * SIP::sip_info(unsigned info, const char *dialed_number, short rtp_port, const char * sip_username, short wlocal_port, const char * local_ip, const char * proxy_ip, const char * from_tag, const char * via_branch, const osip_call_id_t *call_id_header, int cseq) {

	char local_port[10];
	sprintf(local_port, "%i", wlocal_port);

	osip_message_t * request;
	osip_message_init(&request);
	// FIXME -- Should use the "force_update" function.
	request->message_property = 2;
	request->sip_method = strdup("INFO");
	osip_message_set_version(request, strdup("SIP/2.0"));	
	osip_uri_init(&request->req_uri);
	osip_uri_set_host(request->req_uri, strdup(proxy_ip));
	osip_uri_set_username(request->req_uri, strdup(dialed_number));
	
	// VIA
	osip_via_t * via;
	osip_via_init(&via);
	via_set_version(via, strdup("2.0"));
	via_set_protocol(via, strdup("UDP"));
	via_set_host(via, strdup(local_ip));
	via_set_port(via, strdup(local_port));

	// VIA BRANCH
	osip_via_set_branch(via, strdup(via_branch));

	// add via
	osip_list_add(&request->vias, via, -1);

	// FROM
	osip_from_init(&request->from);
	osip_from_set_displayname(request->from, strdup(sip_username));

	// FROM TAG
	osip_from_set_tag(request->from, strdup(from_tag));

	osip_uri_init(&request->from->url);
	osip_uri_set_host(request->from->url, strdup(proxy_ip));
	osip_uri_set_username(request->from->url, strdup(sip_username));

	// TO
	osip_to_init(&request->to);
	osip_to_set_displayname(request->to, strdup(""));
	osip_uri_init(&request->to->url);
	osip_uri_set_host(request->to->url, strdup(proxy_ip));
	osip_uri_set_username(request->to->url, strdup(dialed_number));

	// CALL ID
	osip_call_id_clone(call_id_header, &request->call_id);

	// CSEQ
	osip_cseq_init(&request->cseq);
	osip_cseq_set_method(request->cseq, strdup("INFO"));
	char temp_buf[21];
	sprintf(temp_buf,"%i",cseq);
	osip_cseq_set_number(request->cseq, strdup(temp_buf));	

	osip_message_set_content_type(request, strdup("application/dtmf-relay"));
	char message[31];
	// FIXME -- This duration should probably come from a config file.
	switch (info) {
		case 11:
			snprintf(message,sizeof(message),"Signal=*\nDuration=200");
			break;
		case 12:
			snprintf(message,sizeof(message),"Signal=#\nDuration=200");
			break;
		default:
			snprintf(message,sizeof(message),"Signal=%i\nDuration=200",info);
	}
	sprintf(temp_buf,"%lu",strlen(message));
	osip_message_set_content_length(request, strdup(temp_buf));

	// Payload.
	osip_message_set_body(request,message,strlen(message));

	return request;	
}



// vim: ts=4 sw=4
