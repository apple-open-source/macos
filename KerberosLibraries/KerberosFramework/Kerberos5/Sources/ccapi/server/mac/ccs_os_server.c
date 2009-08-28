/*
 * $Header$
 *
 * Copyright 2006 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "ccs_common.h"

#include <syslog.h>
#include <DiskArbitration/DiskArbitration.h>
#include <DiskArbitration/DiskArbitrationPrivate.h>
#include <dispatch/dispatch.h>
#include <bsm/libbsm.h>

#include "k5_mig_server.h"
#include "ccs_os_server.h"

/* ------------------------------------------------------------------------ */

static char *
cf2cstring(CFStringRef inString)
{
    char *string = NULL;
    CFIndex length;
    
    string = (char *) CFStringGetCStringPtr (inString, kCFStringEncodingUTF8);
    if (string)
	return strdup(string);

    length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(inString), 
					       kCFStringEncodingUTF8) + 1;
    string = malloc (length);
    if (string == NULL)
	return NULL;
    if (!CFStringGetCString (inString, string, length, kCFStringEncodingUTF8)) {
	free (string);
	return NULL;
    }
    return string;
}


static void
callback(DADiskRef disk, void *context)
{
    CFStringRef path, ident;
    CFURLRef url;
    CFDictionaryRef dict;
    size_t len;
    char *str, *str2;

    dict = DADiskCopyDescription(disk);
    if (dict == NULL)
	return;

    url = (CFURLRef)CFDictionaryGetValue(dict, kDADiskDescriptionVolumePathKey);
    if (url == NULL)
	goto out;

    path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    if (path == NULL)
	goto out;

    str = cf2cstring(path);
    CFRelease(path);
    if (str == NULL)
	goto out;

    /* remove trailing / */
    len = strlen(str);
    if (len > 0 && str[len - 1] == '/')
	len--;

    asprintf(&str2, "fs:%.*s", (int)len, str);
    free(str);

    ident = CFStringCreateWithCString(NULL, str2, kCFStringEncodingUTF8);
    free(str2);
    if (ident) {
	KRBCredFindByLabelAndRelease(ident);
	CFRelease(ident);
    }
 out:
    CFRelease(dict);
}

static void
listen_unmount(void)
{
    DASessionRef session = DASessionCreate(kCFAllocatorDefault);
    dispatch_queue_t queue;

    queue = dispatch_queue_create("com.apple.CCacheServer.diskwatch", NULL);
    DASessionSetDispatchQueue(session, queue);
    DARegisterDiskDisappearedCallback(session, NULL, callback, NULL);
}

/* ------------------------------------------------------------------------ */

int32_t k5_ipc_server_add_client (mach_port_t in_client_port)
{
    return cci_check_error (ccs_server_add_client (in_client_port));
}

/* ------------------------------------------------------------------------ */

int32_t k5_ipc_server_remove_client (mach_port_t in_client_port)
{
    return cci_check_error (ccs_server_remove_client (in_client_port));
}


/* ------------------------------------------------------------------------ */

kern_return_t k5_ipc_server_handle_request (mach_port_t    in_connection_port,
					    audit_token_t  remote_creds,
                                            mach_port_t    in_reply_port,
                                            k5_ipc_stream  in_request_stream)
{
    au_asid_t mAuditSessionId;

    audit_token_to_au32(remote_creds, NULL, NULL, NULL, NULL, NULL,
			NULL, &mAuditSessionId, NULL);

    return cci_check_error (ccs_server_handle_request (in_connection_port, 
						       mAuditSessionId,
                                                       in_reply_port, 
                                                       in_request_stream));
}

#pragma mark -

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_server_initialize (int argc, const char *argv[])
{
    openlog (argv[0], LOG_CONS | LOG_PID, LOG_AUTH);
    syslog (LOG_INFO, "Starting up.");   
    
#define CCS_TIMEOUT (600ULL * NSEC_PER_SEC)

    dispatch_source_timer_create(DISPATCH_TIMER_INTERVAL,
				 CCS_TIMEOUT, NSEC_PER_SEC, NULL, 
				 dispatch_get_main_queue(),
        ^(dispatch_event_t event) {
	    cc_int32 err = 0;
	    int valid = 0;

	    err = ccs_server_valid(&valid);
	    if (!err && !valid && ccs_server_isrefed() == 0) {
		syslog(LOG_NOTICE, "No valid tickets, timing out");   
		exit(0);
	    }
        }
    );

    listen_unmount();
    
    return cci_check_error (ccNoError);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_server_cleanup (int argc, const char *argv[])
{
    cc_int32 err = 0;
    
    syslog (LOG_NOTICE, "Exiting.");
    
    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_server_listen_loop (int argc, const char *argv[])
{
    k5_ipc_server_listen();
    dispatch_main();
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_server_send_reply (ccs_pipe_t   in_reply_pipe,
                                   k5_ipc_stream in_reply_stream)
{
    return cci_check_error (k5_ipc_server_send_reply (in_reply_pipe, 
                                                      in_reply_stream));
}
