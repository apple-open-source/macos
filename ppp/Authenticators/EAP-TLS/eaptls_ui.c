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

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <paths.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFBundle.h>
#include <mach/mach.h>
#include <ppp/ppp_defs.h>
#include <SystemConfiguration/SCNetworkConnection.h>

#include "eaptls_ui.h"


/* ------------------------------------------------------------------------------------
 definitions
------------------------------------------------------------------------------------ */ 

/* ------------------------------------------------------------------------------------
 internal functions
------------------------------------------------------------------------------------ */ 
extern uid_t uid;			/* Our real user-id */


/* ------------------------------------------------------------------------------------
 internal variables
------------------------------------------------------------------------------------ */ 

static CFBundleRef	bundleRef = 0;
static int			pid = -1;
static void			(*log_debug) __P((char *, ...)) = 0;
static void			(*log_error) __P((char *, ...)) = 0;

/* ------------------------------------------------------------------------------------
------------------------------------------------------------------------------------ */ 
int eaptls_ui_load(CFBundleRef bundle, void *logdebug, void *logerror)
{
    if (bundle == 0)
        return -1;
        
    bundleRef = bundle;
	CFRetain(bundle);
		
	log_debug = logdebug;
	log_error = logerror;
	
	pid = -1;
    return 0;
}

/* ------------------------------------------------------------------------------------
------------------------------------------------------------------------------------ */ 
void eaptls_ui_dispose()
{

	if (pid != -1) {
		kill(pid, SIGHUP);
		pid = -1;
	}
	
	CFRelease(bundleRef);
	bundleRef = NULL;
}

#define EAPTLSTRUST_PATH	"/System/Library/PrivateFrameworks/EAP8021X.framework/Support/eaptlstrust.app/Contents/MacOS/eaptlstrust"

/* ------------------------------------------------------------------------------------
------------------------------------------------------------------------------------ */ 
static void
eaptls_ui_setup_child(int fdp[2])
{
    int	fd, i;

    /* close open FD's except for the read end of the pipe */
    for (i = getdtablesize() - 1; i >= 0; i--) {
		if (i != fdp[0])
			close(i);
    }
    if (fdp[0] != STDIN_FILENO) {
		dup(fdp[0]);	/* stdin */
		close(fdp[0]);
    }
    fd = open(_PATH_DEVNULL, O_RDWR, 0);/* stdout */
    dup(fd);				/* stderr */
}

/* ------------------------------------------------------------------------------------
------------------------------------------------------------------------------------ */ 
static void
eaptls_ui_setup_parent(int fdp[2], CFDictionaryRef trust_info, CFStringRef caller_label)
{
    size_t					count, write_count;
    CFMutableDictionaryRef 	dict;
    CFDataRef				data;
    
    close(fdp[0]); 	/* close the read end */
    fdp[0] = -1;

	dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(dict, CFSTR("TrustInformation"), trust_info);
	if (caller_label) 
		CFDictionarySetValue(dict, CFSTR("CallerLabel"), caller_label);

    data = CFPropertyListCreateXMLData(NULL, dict);
    count = CFDataGetLength(data);
    /* disable SIGPIPE if it's currently enabled? XXX */
    write_count = write(fdp[1], (void *)CFDataGetBytePtr(data), count);

    /* enable SIGPIPE if it was enabled? XXX */
    CFRelease(data);
	CFRelease(dict);

    if (write_count != count) {
		if (write_count == -1) {
			if (log_error)
				(log_error)("EAP-TLS: dialog setup parent failed to write on pipe, %m\n");
		}
		else {
			if (log_error)
				(log_error)("EAP-TLS: dialog setup parent failed to write on pipe (wrote %d, expected %d)\n", 
								write_count, count);
		}
    }
    close(fdp[1]);	/* close write end to deliver EOF to reader */
    fdp[1] = -1;
}


/* ------------------------------------------------------------------------------------
------------------------------------------------------------------------------------ */ 
static int
eaptls_ui_dialog(CFDictionaryRef trust_info, CFStringRef caller_label)
{
	int fdp[2], err, status, ret = RESPONSE_ERROR;	
	
    fdp[0] = fdp[1] = -1;
    if (pipe(fdp) == -1) {
		if (log_error)
			(log_error)("EAP-TLS: dialog failed to create pipe, %m\n");
		goto fail;
    }
	
	pid = fork();
	if (pid < 0)
		goto fail;

	if (pid == 0) {
		// child

		setuid(uid);
		eaptls_ui_setup_child(fdp);
        err = execle(EAPTLSTRUST_PATH, EAPTLSTRUST_PATH, (char *)0, (char *)0);
        exit(errno);
	}

	// parent

	eaptls_ui_setup_parent(fdp, trust_info, caller_label);
		
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR)
            continue;
		if (log_error)
			(log_error)("EAP-TLS: in parent, child error (%m)\n");
      goto fail;
    }
	
	if (WIFEXITED(status)) {
		switch (WEXITSTATUS(status)) {
			case 0:
				// OK, no error
				ret = RESPONSE_OK;
				break;
			case 1:
				// user cancelled
				ret = RESPONSE_CANCEL;
				break;
			default:
				// error or certificate failure
				ret = RESPONSE_ERROR;				
		}
	}

	return ret;
	
 fail:
	if (fdp[0] != -1)
		close(fdp[0]);
	if (fdp[1] != -1)
		close(fdp[1]);
    return RESPONSE_ERROR;
}

/* ------------------------------------------------------------------------------------
Perform EAP TLS trust evaluation
------------------------------------------------------------------------------------ */ 
int eaptls_ui_trusteval(CFDictionaryRef publishedProperties, void *data_in, int data_in_len,
                    void **data_out, int *data_out_len)
{
    eaptls_ui_ctx *ctx = (eaptls_ui_ctx *)data_in;
	CFStringRef	copy = NULL;

	copy = CFBundleCopyLocalizedString(bundleRef, CFSTR("VPN Authentication"), CFSTR("VPN Authentication"), NULL);
	ctx->response = eaptls_ui_dialog(publishedProperties, copy ? copy : CFSTR("VPN Authentication"));
	if (copy)
		CFRelease(copy);
            
    *data_out = data_in;
    *data_out_len = data_in_len;
    return 0;
}

