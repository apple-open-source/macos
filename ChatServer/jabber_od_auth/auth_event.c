/*
 *
 * Copyright (c) 2010, Apple, Inc. All rights reserved.
 *
 * @APPLE_BSD_LICENSE_HEADER_START@
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * @APPLE_BSD_LICENSE_HEADER_END@
 * 
*/

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include "auth_event.h"

#include <CoreDaemon/CoreDaemon.h>

void auth_event_data_init(auth_event_data_t *data, char *client_ip, unsigned int client_port, char *mech)
{
    if (client_ip == NULL || mech == NULL) {
        *data = NULL;
        return;
    }

    *data = (auth_event_data_t) calloc(1, sizeof(struct auth_event_data_st));
    auth_event_data_t d = *data;
    d->client_ip = strdup(client_ip);
    d->client_port = client_port;
    d->mech = strdup(mech);
    d->username = NULL;
    d->status = eUnknownEvent;

    return;
}

void auth_event_data_dispose(auth_event_data_t *data)
{
    if (data == NULL)
        return;

    auth_event_data_t d = *data;
    if (d->username != NULL) free(d->username);
    if (d->mech != NULL) free(d->mech);
    if (d->client_ip != NULL) free(d->client_ip);
    free(d);

    return;
}

void auth_event_log_simple(char *username, char *client_ip, unsigned int client_port, char *mech, int status)
{
    // First, log the event
    char log_buf[1024];

    if (status == eAuthFailure)
        snprintf(log_buf, sizeof(log_buf), "Authentication failed, mech: %s client IP: %s client port: %d username: %s",
                (mech == NULL) ? "?" : mech,
                (client_ip == NULL) ? "?" : client_ip,
                client_port,
                (username == NULL) ? "?" : username);
    else if (status == eAuthSuccess)
        snprintf(log_buf, sizeof(log_buf), "Authentication succeeded, mech: %s client IP: %s client port: %d username: %s",
                (mech == NULL) ? "?" : mech,
                (client_ip == NULL) ? "?" : client_ip,
                client_port,
                (username == NULL) ? "?" : username);
    else
        return;

    syslog(LOG_NOTICE, "%s", log_buf);
    
    // Now send an event for emond/Adaptive Firewall handling
    send_server_event(status, client_ip);
    return;
}

void auth_event_log(auth_event_data_t data)
{
    // First, log the event
    char log_buf[1024];

    if (data->status == eAuthFailure)
        snprintf(log_buf, sizeof(log_buf), "Authentication failed, mech: %s client IP: %s client port: %d username: %s",
                (data->mech == NULL) ? "?" : data->mech,
                (data->client_ip == NULL) ? "?" : data->client_ip,
                data->client_port,
                (data->username == NULL) ? "?" : data->username);
    else if (data->status == eAuthSuccess)
        snprintf(log_buf, sizeof(log_buf), "Authentication succeeded, mech: %s client IP: %s client port: %d username: %s",
                (data->mech == NULL) ? "?" : data->mech,
                (data->client_ip == NULL) ? "?" : data->client_ip,
                data->client_port,
                (data->username == NULL) ? "?" : data->username);
    else
        return;

    syslog(LOG_NOTICE, "%s", log_buf);

    // Now send an event for emond/Adaptive Firewall handling
    send_server_event(data->status, data->client_ip);
    return;
}

char g_client_addr[16] = "";
XSEventPortRef	gEventPort = NULL;

/* send server events
 *	event code 1: authentication failure
 *  event code 2: authentication success
 */

void send_server_event ( const eEventCode in_event_code, const char *in_addr )
{
	CFTypeRef keys[2];
	CFTypeRef values[2];
	CFStringRef cfstr_addr = NULL;
	CFStringRef cfstr_event = NULL;

    if (in_addr == NULL)
        return;

	if ( !strlen(g_client_addr) || (strcmp(g_client_addr, in_addr) != 0) ) {
		strlcpy(g_client_addr, in_addr, sizeof g_client_addr);
	}

	/* create a port to the event server */
	if ( gEventPort == NULL )
		gEventPort = XSEventPortCreate(nil);

	keys[0] = CFSTR("eventType");
	keys[1] = CFSTR("host_address");

	/* set event code string */
	switch ( in_event_code ) {
		case eAuthFailure:
			cfstr_event = CFStringCreateWithCString(NULL, "auth.failure", kCFStringEncodingMacRoman);
			break;
		case eAuthSuccess:
			cfstr_event = CFStringCreateWithCString(NULL, "auth.success", kCFStringEncodingMacRoman);
			break;
		default:
			syslog(LOG_WARNING, "Warning: unknown sever event: %d", in_event_code);
			return;
	}

	cfstr_addr = CFStringCreateWithCString(NULL, in_addr, kCFStringEncodingMacRoman);

	values[0] = cfstr_event;
	values[1] = cfstr_addr;

     CFDictionaryRef dict_event = CFDictionaryCreate(NULL, keys, values, 
                                               sizeof(keys) / sizeof(keys[0]), 
                                               &kCFTypeDictionaryKeyCallBacks, 
                                               &kCFTypeDictionaryValueCallBacks); 
	
	/* send the event */
	(void)XSEventPortPostEvent(gEventPort, cfstr_event, dict_event);

	CFRelease(cfstr_addr);
	CFRelease(cfstr_event);
	CFRelease(dict_event);
} /* send_server_event */


void close_server_event_port ( void )
{
	if ( gEventPort != NULL )
		XSEventPortDelete(gEventPort);
} /* close_server_event_port */