/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/* -----------------------------------------------------------------------------
 *
 *  Theory of operation :
 *
 *  performs EAP Radius proxy operations.
 *
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
  Includes
----------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <ppp/ppp_defs.h>

#include "../../Helpers/pppd/eap.h"
#include "../../Helpers/pppd/eap_plugin.h"

#include "radius.h"

#include "radlib.h"
#include "radlib_vs.h"

/* -----------------------------------------------------------------------------
 Forward declarations
----------------------------------------------------------------------------- */

static int radius_eap_init __P((struct EAP_Input *eap_in, void **context));
static int radius_eap_dispose __P((void *context));
static int radius_eap_process __P((void *context, EAP_Input *eap_in, EAP_Output *eap_out));
static int radius_eap_attribute __P((void *context, EAP_Attribute *eap_attr));

static void makePacket(struct EAP_Output *eap_out, u_int8_t code, u_int8_t id, u_int8_t *data, u_int16_t datalen, u_int16_t action);

/* -----------------------------------------------------------------------------
 Definitions
----------------------------------------------------------------------------- */

static u_char eap_mppe_send_key[MPPE_MAX_KEY_LEN];
static u_char eap_mppe_recv_key[MPPE_MAX_KEY_LEN];
static int eap_mppe_keys_set = 0;

struct rad_handle *rad_handle = 0;
static u_int8_t current_id = 0;
static u_int16_t mtu = 0;
#define MAX_RETRANSMITS 10
static int retransmits = 0;
static unsigned char last_state_attr[256];
static int last_state_attr_len = 0;
static char hostname[256];
static struct in_addr nas_ip_address_val;

void (*log_debug) __P((char *, ...)) = 0;
void (*log_error) __P((char *, ...)) = 0;

static eap_ext *eap_handle = NULL;		// EAP proxy handler

static u_int8_t output_buffer[1500];
static u_int16_t output_buffer_len = 0;

/* -----------------------------------------------------------------------------
install eap handler
----------------------------------------------------------------------------- */
int radius_eap_install()
{

    eap_handle = (eap_ext *)malloc(sizeof(eap_ext));
    if (eap_handle == 0)
        return -1;
    
	bzero(eap_handle, sizeof(eap_ext));
	
    eap_handle->type = 0;			// catch all
	eap_handle->name = "EAP-Radius";
	eap_handle->init = radius_eap_init;
	eap_handle->dispose = radius_eap_dispose;
	eap_handle->process = radius_eap_process;
	eap_handle->attribute = radius_eap_attribute;

    if (EapExtAdd(eap_handle))
        return -1;
    
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int 
radius_eap_init (struct EAP_Input *eap_in, void **context)
{
    int err, i;
	
    log_debug = eap_in->log_debug;
    log_error = eap_in->log_error;

	if (eap_in->mode != 1) {
        (*log_error)("Radius : Can't open Radius handler context.\n");
		goto fail;
	}
	
	mtu = eap_in->mtu;
	current_id = eap_in->initial_id;
	last_state_attr_len = 0;
	eap_mppe_keys_set = 0;

	if (gethostname(hostname, sizeof(hostname)) < 0 )
		strcpy(hostname, "Apple");
	hostname[sizeof(hostname) - 1] = 0; // gethostname() does not always terminate the name

	nas_ip_address_val.s_addr = 0;
	if (nas_ip_address)
		ascii2addr(AF_INET, nas_ip_address, &nas_ip_address_val);

    rad_handle = rad_auth_open();
    if (rad_handle == NULL) {
        (*log_error)("Radius : Can't open Radius handler context.\n");
		goto fail;
	}
     
	for (i = 0; i < nb_auth_servers; i++) {
		struct auth_server *server = auth_servers[i];
		
		if (server->proto & RADIUS_USE_EAP) {
			err = rad_add_server(rad_handle, server->address, server->port, server->secret, server->timeout, server->retries);
			if (err != 0) {
				(*log_error)("Radius : Can't use server '%s'\n", server->address);
				if (i == 0)
					goto fail;
			}
		}
	}

	return EAP_NO_ERROR;

fail:
	if (rad_handle) {
		rad_close(rad_handle);
		rad_handle = 0;
	}
	
	return EAP_ERROR_GENERIC;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int 
radius_eap_dispose (void *context)
{

	if (rad_handle) {
		rad_close(rad_handle);
		rad_handle = 0;
	}

	return EAP_NO_ERROR;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int 
radius_eap_process (void *context, struct EAP_Input *eap_in, struct EAP_Output *eap_out)
{
    struct EAP_Packet *pkt_in;
	int	err = 0, attr_type, len, n;
	unsigned char *attr_value;
	size_t attr_len;
	unsigned char *p;
	u_int32_t attr_vendor;
    char auth[MD4_SIGNATURE_SIZE + 1];
	
	// by default, ignore the message
	eap_out->action = EAP_ACTION_NONE;

	switch (eap_in->notification) {
		case EAP_NOTIFICATION_START:
		case EAP_NOTIFICATION_PACKET:

			rad_create_request(rad_handle, RAD_ACCESS_REQUEST);

			rad_put_int(rad_handle, RAD_FRAMED_MTU, mtu);
			rad_put_int(rad_handle, RAD_SERVICE_TYPE, RAD_FRAMED);
			rad_put_int(rad_handle, RAD_FRAMED_PROTOCOL, RAD_PPP);
			rad_put_int(rad_handle, RAD_NAS_PORT_TYPE, nas_port_type);
			if (tunnel_type)
				rad_put_int(rad_handle, RAD_TUNNEL_TYPE, tunnel_type);
			
			if (nas_ip_address)
				rad_put_int(rad_handle, RAD_NAS_IP_ADDRESS, ntohl(nas_ip_address_val.s_addr));

			if (nas_identifier)
				rad_put_string(rad_handle, RAD_NAS_IDENTIFIER, nas_identifier);

			/* if there is no nas_ip_address or no nas_identifier, use a hostname() as nas_identifier */
			if (nas_identifier == NULL && nas_ip_address == NULL)
				rad_put_string(rad_handle, RAD_NAS_IDENTIFIER, hostname);
	
			/* if username is not present, Windows will reject the access request */
			rad_put_attr(rad_handle, RAD_USER_NAME, eap_in->identity, strlen(eap_in->identity));

			if (eap_in->notification == EAP_NOTIFICATION_START) {
				/* build a Identity Response and add the EAP Message attribute */
				output_buffer_len = strlen(eap_in->identity) + (EAP_HEADERLEN + 1);
				output_buffer[0] = EAP_RESPONSE;
				output_buffer[1] = current_id - 1;
				output_buffer[2] = output_buffer_len >> 8;
				output_buffer[3] = output_buffer_len & 0xFF;
				output_buffer[4] = EAP_TYPE_IDENTITY;
				strlcpy(&output_buffer[5], eap_in->identity, sizeof(output_buffer) - (EAP_HEADERLEN + 1));
				rad_put_attr(rad_handle, RAD_EAP_MESSAGE, output_buffer, output_buffer_len);
			}
			else {
				/* add state sttribute if present */
				if (last_state_attr_len) {
					rad_put_attr(rad_handle, RAD_STATE, last_state_attr, last_state_attr_len);
				}
				
				/* forward the EAP packet received asseveral EAP Message attributes */
				pkt_in = (struct EAP_Packet *)eap_in->data;
				
				if (pkt_in->code != EAP_RESPONSE) {
					(*log_error)("Radius: Didn't receive an EAP response packet. (received %d)\n", pkt_in->code);
					makePacket(eap_out, EAP_FAILURE, current_id++, 0, 0, EAP_ACTION_SEND_AND_DONE);
					break;
				}

				p = (unsigned char *)pkt_in;
				len = ntohs(pkt_in->len);
				while (len > 0) {
					n = len > RAD_MAX_ATTR_LEN ? RAD_MAX_ATTR_LEN : len;
					rad_put_attr(rad_handle, RAD_EAP_MESSAGE, p, n);
					p += n;
					len -= n;
				}
			}
			
			err = rad_send_request(rad_handle);
			
			switch (err) {
				case RAD_ACCESS_ACCEPT: 
					/* authentication succeeded, retrieve keys attributes if present */
					eap_mppe_keys_set = 0;
					while ((attr_type = rad_get_attr(rad_handle, (const void **)&attr_value, &attr_len)) > 0 ) {

						switch (attr_type) {
						
							case RAD_VENDOR_SPECIFIC: 
							
								attr_type = rad_get_vendor_attr(&attr_vendor, (const void **)&attr_value,  &attr_len);
								switch (attr_type) {
									case RAD_MICROSOFT_MS_MPPE_SEND_KEY:
										len = rad_request_authenticator(rad_handle, auth, sizeof(auth));
										
										if(len != -1)
										{
											radius_decryptmppekey(eap_mppe_send_key, attr_value, attr_len, (u_char*)rad_server_secret(rad_handle), auth, len);
											eap_mppe_keys_set = 1;
										}
										else
											error("Radius: rad-eap-mppe-send-key:  could not get authenticator!\n");
										break;
									case RAD_MICROSOFT_MS_MPPE_RECV_KEY:
										len = rad_request_authenticator(rad_handle, auth, sizeof(auth));
										
										if(len != -1)
										{										
											radius_decryptmppekey(eap_mppe_recv_key, attr_value, attr_len, (u_char*)rad_server_secret(rad_handle), auth, len);
											eap_mppe_keys_set = 1;
										}
										else
											error("Radius: rad-eap-mppe-recv-key:  could not get authenticator!\n");											
										break;
								}
								break;
						}
					}
					
					makePacket(eap_out, EAP_SUCCESS, current_id++, 0, 0, EAP_ACTION_SEND_AND_DONE);
					break;

				case RAD_ACCESS_REJECT: 
					/* pretty clear, the server don't like us ... */
					makePacket(eap_out, EAP_FAILURE, current_id++, 0, 0, EAP_ACTION_SEND_AND_DONE);
					break;
					
				case RAD_ACCESS_CHALLENGE: 
					/* concatenate all EAP message attributes, and build an EAP packet */
					output_buffer_len = 0;
					last_state_attr_len = 0;
					while ((attr_type = rad_get_attr(rad_handle, (const void **)&attr_value, &attr_len)) > 0 ) {

						switch (attr_type) {
						
							case RAD_EAP_MESSAGE: 
								if (output_buffer_len == 0)
									current_id = attr_value[1];
								if ((output_buffer_len + attr_len) <= sizeof(output_buffer)) {
									bcopy(attr_value, output_buffer + output_buffer_len, attr_len);
									output_buffer_len += attr_len;
								}
								break;
								
							case RAD_STATE: 
								/* memorize server state for next access request */
								last_state_attr_len = attr_len;
								bcopy(attr_value, last_state_attr, attr_len);
								break;
						}
					}
					
					if (!output_buffer_len) {
						(*log_error)("Radius : Incorrect Access Challenge received\n");
						makePacket(eap_out, EAP_FAILURE, current_id++, 0, 0, EAP_ACTION_SEND_AND_DONE);
						break;
					}

					retransmits = 0;
					eap_out->action = EAP_ACTION_SEND_WITH_TIMEOUT;
					eap_out->data = output_buffer;
					eap_out->data_len = output_buffer_len;
					break;
					
				default: 
					(*log_error)("Radius : Authentication error %d. %s.\n", err, rad_strerror(rad_handle));
					makePacket(eap_out, EAP_FAILURE, current_id++, 0, 0, EAP_ACTION_SEND_AND_DONE);
					break;
			}

			break;
			
		case EAP_NOTIFICATION_TIMEOUT:
			/* retransmit same EAP packet on timeout */
			if (++retransmits >= MAX_RETRANSMITS) {
				makePacket(eap_out, EAP_FAILURE, current_id++, 0, 0, EAP_ACTION_SEND_AND_DONE);
				break;
			}
			
			eap_out->action = EAP_ACTION_SEND_WITH_TIMEOUT;
			eap_out->data = output_buffer;
			eap_out->data_len = output_buffer_len;
			break;

	}
	
	return EAP_NO_ERROR;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int 
radius_eap_attribute (void *context, struct EAP_Attribute *eap_attr)
{

	void *data = NULL;
	int len = 0;
	
	eap_attr->data = 0;		

    switch (eap_attr->type) {
	
        case EAP_ATTRIBUTE_MPPE_SEND_KEY:
            if (eap_mppe_keys_set) {
				data = eap_mppe_send_key;
				len = sizeof(eap_mppe_send_key);
			}
            break;
        case EAP_ATTRIBUTE_MPPE_RECV_KEY:
            if (eap_mppe_keys_set) {
				data = eap_mppe_recv_key;
				len = sizeof(eap_mppe_recv_key);
			}
            break;
    }

	if (data == NULL)
		return -1;
		
	eap_attr->data = data;
	eap_attr->data_len = len;
    return 0;
}

/* ------------------------------------------------------------------------------------
------------------------------------------------------------------------------------ */ 
static void 
makePacket(struct EAP_Output *eap_out, u_int8_t code, u_int8_t id, u_int8_t *data, u_int16_t datalen, u_int16_t action) 
{

	int i;
	
	switch (code) {
		case EAP_FAILURE:
		case EAP_SUCCESS:
			output_buffer_len = EAP_HEADERLEN;
			break;

		case EAP_REQUEST:
		case EAP_RESPONSE:
			output_buffer_len = datalen + EAP_HEADERLEN;
			break;
	}

	if (output_buffer_len > (mtu - PPP_HDRLEN)) {
		// need to handle fragmentation
		eap_out->action = EAP_ACTION_NONE;
		return;
	}
	
	i = 0;
	output_buffer[i++] = code;
	output_buffer[i++] = id;
	output_buffer[i++] = output_buffer_len >> 8;
	output_buffer[i++] = output_buffer_len & 0xFF;
	
	if (output_buffer_len > EAP_HEADERLEN) {
		bcopy(data, &output_buffer[EAP_HEADERLEN], datalen);
	}

	eap_out->action = action;
	eap_out->data = output_buffer;
	eap_out->data_len = output_buffer_len;
}

