/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2003 by Apple Computer, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


/**
 * @file
 *
 * Registration functions for the server process (distccd) for zeroconfiguration
 * feature.
 **/


#if defined(DARWIN)


#include <arpa/inet.h>
#include <dns_sd.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "config.h"
#include "distcc.h"
#include "dopt.h"
#include "trace.h"
#include "zeroconf_reg.h"
#include "zeroconf_util.h"


#define ZC_CONTEXT             NULL
#define ZC_HOST                NULL
#define ZC_NAME                NULL
#define ZC_REGISTER_FLAGS      0


static DNSServiceRef zcRegRef    = NULL;
static pthread_t     zcRegThread = { 0 };


// Rendezvous registration


/**
 * Cleanup if an error occurs during daemon registration for zeroconfiguration.
 **/
static void dcc_zc_reg_cleanup(void)
{
    if ( zcRegRef != NULL ) {
        DNSServiceRefDeallocate(zcRegRef);
        zcRegRef = NULL;
    }
}


/**
 * Handle registration messages from the mDNSResponder.
 * Invoke <code>dcc_zc_reg_cleanup</code> and terminate the thread upon error.
 * Otherwise, log <code>name</code> to the console.
 **/
static void dcc_zc_reg_reply(const DNSServiceRef       UNUSED(ref),
                             const DNSServiceFlags     UNUSED(flags),
                             const DNSServiceErrorType errorCode,
                             const char               *aName,
                             const char               *aRegType,
                             const char               *aDomain,
                                   void               * UNUSED(aContext))
{
    if ( errorCode ) {
        rs_log_error("Aborting zeroconfiguration registration due to error: %d",
                     errorCode);
        dcc_zc_reg_cleanup();
        pthread_exit(NULL);
    } else {
        char *fullName = dcc_zc_full_name(aName, aRegType, aDomain);
        rs_log_info("Registered and active as \"%s\"", fullName);
        free(fullName);
    }
}


/**
 * Invoke <code>DNSServiceRegister</code>.
 * Invoke <code>DNSServiceDiscoveryProcessResult</code> until an error is
 * encountered.
 * Invoke <code>dcc_zc_reg_cleanup</code> and terminate the thread upon error.
 **/
static void dcc_zc_actually_register(void *aTxtRecord)
{
    size_t txtLen    = ( aTxtRecord == NULL ) ? 0
                                             : strlen((char *)aTxtRecord) + 1;

    rs_log_info("Registering on port %d for all interfaces", arg_port);
    rs_log_info("Distributed compile profile is:\n%s", (char *)aTxtRecord);

    DNSServiceErrorType exitValue = DNSServiceRegister(&zcRegRef,
                                                       ZC_REGISTER_FLAGS,
                                                       ZC_ALL_INTERFACES,
                                                       ZC_NAME,
                                                       ZC_REG_TYPE,
                                                       ZC_DOMAIN,
                                                       ZC_HOST,
                                                       htons(arg_port),
                                                       txtLen,
                                                       aTxtRecord,
                              (DNSServiceRegisterReply)dcc_zc_reg_reply,
                                                       ZC_CONTEXT);

    free(aTxtRecord);

    if ( exitValue == kDNSServiceErr_NoError ) {
        // Continue processing incoming messages.
        // This might not terminate until the process terminates.
        while ( TRUE ) {
            exitValue = DNSServiceProcessResult(zcRegRef);

            if ( exitValue != kDNSServiceErr_NoError ) {
                rs_log_error("Unable to handle zeroconfiguration replies: %d",
                             exitValue);
                break;
            }
        }
    } else {
        rs_log_error("Unable to register for zeroconfiguration: %d", exitValue);
    }

    // Doubtful we'll ever get here.

    dcc_zc_reg_cleanup();
    pthread_exit(NULL);
}


/**
 * Register a distcc daemon with mDNS to obviate manual configuration for
 * distcc clients.
 * Spawn a separate thread using <code>dcc_zc_actually_register</code> to
 * handle replies from the mDNSResponder.
 * This thread uses no shared data structures and terminates itself (not
 * the entire process) upon error.
 * Passes <code>txtRecord</code> to <code>dcc_zc_actually_register</code>,
 * which frees <code>txtRecord</code> after it has been used.
 **/
void dcc_register_for_zeroconfig(const char *aTxtRecord)
{
    if ( pthread_create(&zcRegThread, NULL,
                        (void *(*)(void *))dcc_zc_actually_register,
                        (void *)aTxtRecord) ) {
        rs_log_error("Unable to create thread for zeroconfiguration");
    }
}


#endif // DARWIN
