/*-
 * Copyright (c) 2013 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#import "acquirecred.h"
#import "krb5.h"
#import "heimcred.h"
#import "gsscred.h"
#import "common.h"
#import "GSSCredXPCHelperClient.h"

void cred_update_expire_time_locked(HeimCredRef cred, time_t t);

static dispatch_queue_t event_work_queue;
static void init_event_queue() {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
	event_work_queue = dispatch_queue_create("com.apple.GSSCred.event_work", DISPATCH_QUEUE_SERIAL);
    });
}

//timer function used to renew renewable credentials
void
renew_func(heim_event_t event, void *ptr)
{
    init_event_queue();
    
    dispatch_sync(event_work_queue, ^{
	krb5_error_code ret;
	NSString *clientName;
	time_t expire;
	
	HeimCredRef cred = (HeimCredRef)ptr;
	if (cred==NULL) {
	    return;
	}
	heim_ipc_event_cancel(cred->renew_event);
	cred->next_acquire_time = 0;
	
	clientName = (__bridge NSString*)CFDictionaryGetValue(cred->attributes, kHEIMAttrClientName);
	os_log_debug(GSSOSLog(), "renew_func: %@", clientName);
	
	ret = [HeimCredGlobalCTX.gssCredHelperClientClass refreshForCred:cred expireTime:&expire];
	
	switch (ret) {
	    case KRB5KRB_AP_ERR_BAD_INTEGRITY:
	    case KRB5KRB_AP_ERR_MODIFIED:
	    case KRB5KDC_ERR_PREAUTH_FAILED:
		/* bad password, drop it like dead */
		os_log_debug(GSSOSLog(), "cache: %@ got auth failed, stop renewing",
			     clientName);
		HEIMDAL_MUTEX_lock(&cred->event_mutex);
		cred->acquire_status = CRED_STATUS_ACQUIRE_STOPPED;
		HEIMDAL_MUTEX_unlock(&cred->event_mutex);
		break;
	    case KRB5KRB_AP_ERR_TKT_EXPIRED:
		os_log_debug(GSSOSLog(), "The ticket has expired, stop renewing: %@",
			     clientName);
		HEIMDAL_MUTEX_lock(&cred->event_mutex);
		cred->acquire_status = CRED_STATUS_ACQUIRE_STOPPED;
		HEIMDAL_MUTEX_unlock(&cred->event_mutex);
		break;
	    case KRB5_CC_NOTFOUND:
		/* this is fatal, the requested cache is not found */
		os_log_debug(GSSOSLog(), "cache not found, stop renewing: %@",
			     clientName);
		HEIMDAL_MUTEX_lock(&cred->event_mutex);
		cred->acquire_status = CRED_STATUS_ACQUIRE_STOPPED;
		HEIMDAL_MUTEX_unlock(&cred->event_mutex);
		break;
	    case 0:
		os_log_debug(GSSOSLog(), "cache: %@ got new tickets (expire in %d seconds)",
			     clientName, (int)(expire - time(NULL)));
		//when the renewal is successful, this cred is replaced with another one.
		//updating these values does nothing
		break;
	    default:
		cred_update_renew_time(cred, true);
		HEIMDAL_MUTEX_lock(&cred->event_mutex);
		cred->acquire_status = CRED_STATUS_ACQUIRE_FAILED;
		HEIMDAL_MUTEX_unlock(&cred->event_mutex);
		break;
	}
    });
}

//event function used to acquire credentials or notify caches when a cred expires
void
expire_func(heim_event_t event, void *ptr)
{
    init_event_queue();
    
    dispatch_sync(event_work_queue, ^{
	krb5_error_code ret;
	NSString *clientName;
	time_t expire;
	
	HeimCredRef cred = (HeimCredRef)ptr;
	if (cred==NULL) {
	    return;
	}
	HEIMDAL_MUTEX_lock(&cred->event_mutex);
	heim_ipc_event_cancel(cred->renew_event);
	cred->next_acquire_time = 0;
	HEIMDAL_MUTEX_unlock(&cred->event_mutex);
	
	if (!isAcquireCred(cred)) {
	    if (cred->mech->notifyCaches!=NULL) {
		cred->mech->notifyCaches();
	    }
	    return;
	}
	
	clientName = (__bridge NSString*)CFDictionaryGetValue(cred->attributes, kHEIMAttrClientName);
	os_log_debug(GSSOSLog(), "expire_func: %@", clientName);
	
	ret = [HeimCredGlobalCTX.gssCredHelperClientClass acquireForCred:cred expireTime:&expire];
	
	switch (ret) {
	    case KRB5KRB_AP_ERR_BAD_INTEGRITY:
	    case KRB5KRB_AP_ERR_MODIFIED:
	    case KRB5KDC_ERR_PREAUTH_FAILED:
	    {
		/* bad password, drop it like dead */
		os_log_debug(GSSOSLog(), "cache: %@ got bad password, stop renewing",
			     clientName);
		cred_update_acquire_status(cred, CRED_STATUS_ACQUIRE_STOPPED);
	    }
		break;
	    case KRB5_CC_NOTFOUND:
	    {
		/* this is fatal, the requested cache is not found */
		os_log_debug(GSSOSLog(), "cache not found, stop renewing: %@",
			     clientName);
		cred_update_acquire_status(cred, CRED_STATUS_ACQUIRE_STOPPED);
	    }
		break;
	    case 0:
	    {
		os_log_debug(GSSOSLog(), "cache: %@ got new tickets (expire in %d seconds)",
			     clientName, (int)(expire - time(NULL)));
		HEIMDAL_MUTEX_lock(&cred->event_mutex);
		cred->expire = expire;
		HEIMDAL_MUTEX_unlock(&cred->event_mutex);
		cred_update_acquire_status(cred, CRED_STATUS_ACQUIRE_SUCCESS);
	    }
		break;
	    default:
		HEIMDAL_MUTEX_lock(&cred->event_mutex);
		cred_update_expire_time_locked(cred, time(NULL) + 300);
		HEIMDAL_MUTEX_unlock(&cred->event_mutex);
		cred_update_acquire_status(cred, CRED_STATUS_ACQUIRE_FAILED);
		break;
	}

	HeimCredGlobalCTX.executeOnRunQueue(^{
	    HeimCredCTX.needFlush = 1;
	    HeimCredGlobalCTX.saveToDiskIfNeeded();
	});

    });
}

void
final_func(void *ptr)
{
}

#define KRB5_CONF_NAME "krb5_ccache_conf_data"
#define KRB5_REALM_NAME "X-CACHECONF:"

void
cred_update_acquire_status(HeimCredRef cred, int status)
{
    os_log_debug(GSSOSLog(), "cred_update_acquire_status: %@", CFBridgingRelease(CFUUIDCreateString(NULL, cred->uuid)));
    
    HEIMDAL_MUTEX_lock(&cred->event_mutex);
    cred->acquire_status = status;
    
    switch (status) {
	case CRED_STATUS_ACQUIRE_START:
	    cred_update_expire_time_locked(cred, time(NULL) + 2);  //wait a few seconds to start to let load and save methods finish.
	    break;

	case CRED_STATUS_ACQUIRE_STOPPED:
	    cred->next_acquire_time = 0;
	    heim_ipc_event_cancel(cred->expire_event);
	    break;

	case CRED_STATUS_ACQUIRE_FAILED:
	    cred_update_expire_time_locked(cred, time(NULL) + 300);
	    break;

	case CRED_STATUS_ACQUIRE_SUCCESS: {
	    time_t next_refresh, now = time(NULL);

	    if (cred->expire > now) {
		next_refresh = cred->expire;
		/* try to acquire credential just before */
		if (cred->expire - now > 300)
		    next_refresh -= 300;
		cred_update_expire_time_locked(cred, next_refresh);
	    } else {
		cred->next_acquire_time = 0;
	    }
	    break;
	}
    }
    HEIMDAL_MUTEX_unlock(&cred->event_mutex);
    
}

//call this in the event mutex
void
cred_update_expire_time_locked(HeimCredRef cred, time_t t)
{
    if (t == 0) {
	t = time(NULL);
    } else if (t < time(NULL)) {
	cred->next_acquire_time = 0;
	os_log_debug(GSSOSLog(), "%@: acquire time is in the past",
		     CFBridgingRelease(CFUUIDCreateString(NULL, cred->uuid)));
	return;
    }
    cred->next_acquire_time = t;
    heim_ipc_event_set_time(cred->expire_event, t);
    os_log_debug(GSSOSLog(), "%@: will try to acquire credentals in %d seconds",
		 CFBridgingRelease(CFUUIDCreateString(NULL, cred->uuid)), (int)(t - time(NULL)));
}

#define KCM_EVENT_QUEUE_INTERVAL 60

void
cred_update_renew_time(HeimCredRef cred, bool is_retry)
{
    time_t renewtime = time(NULL) + HeimCredGlobalCTX.renewInterval;
    
    if (is_retry) {
	renewtime = time(NULL) + 300;
    }
    HEIMDAL_MUTEX_lock(&cred->event_mutex);
    time_t expire = cred->expire;
    
    /* if the ticket is about to expire in less then QUEUE_INTERVAL,
     * don't bother */
    if (time(NULL) + KCM_EVENT_QUEUE_INTERVAL > expire) {
	os_log_debug(GSSOSLog(), "%@: will expire before next attempt",
		     CFBridgingRelease(CFUUIDCreateString(NULL, cred->uuid)));
	HEIMDAL_MUTEX_unlock(&cred->event_mutex);
	return;
    }
    
    if (renewtime > expire - KCM_EVENT_QUEUE_INTERVAL)
	renewtime = expire - KCM_EVENT_QUEUE_INTERVAL;
    
    os_log_debug(GSSOSLog(), "%@: will try to renew credentals in %d seconds",
	    cred, (int)(renewtime - time(NULL)));
    
    heim_ipc_event_set_time(cred->renew_event, renewtime);
    cred->renew_time = renewtime;
    HEIMDAL_MUTEX_unlock(&cred->event_mutex);
}
