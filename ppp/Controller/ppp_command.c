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
includes
----------------------------------------------------------------------------- */

#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <syslog.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_time.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "ppp_msg.h"
#include "ppp_privmsg.h"
#include "../Family/ppp_defs.h"
#include "../Family/if_ppp.h"
#include "../Family/if_ppplink.h"
#include "../Helpers/pppd/pppd.h"

#include "ppp_manager.h"
#include "ppp_client.h"
#include "ppp_command.h"
#include "ppp_option.h"


/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Forward Declarations
----------------------------------------------------------------------------- */

u_int32_t ppp_translate_error(u_int16_t subtype, u_int32_t native_ppp_error, u_int32_t native_dev_error);

/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

extern TAILQ_HEAD(, ppp) 	ppp_head;
extern double	 		gTimeScaleSeconds;

/* -----------------------------------------------------------------------------
find the ppp structure corresponding to the message
----------------------------------------------------------------------------- */
struct ppp *ppp_find(struct msg *msg)
{

    if (msg->hdr.m_flags & USE_SERVICEID)
        return ppp_findbysid(msg->data, msg->hdr.m_link);
    else
        return ppp_findbyref(msg->hdr.m_link);

    return 0;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */
static CFDataRef ppp_serialize(CFPropertyListRef obj, void **data, u_int32_t *dataLen)
{
    CFDataRef           	xml;
    
    xml = CFPropertyListCreateXMLData(NULL, obj);
    if (xml) {
        *data = (void*)CFDataGetBytePtr(xml);
        *dataLen = CFDataGetLength(xml);
    }
    return xml;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */
static CFPropertyListRef ppp_unserialize(void *data, u_int32_t dataLen)
{
    CFDataRef           	xml;
    CFStringRef         	xmlError;
    CFPropertyListRef	ref = 0;

    xml = CFDataCreate(NULL, data, dataLen);
    if (xml) {
        ref = CFPropertyListCreateFromXMLData(NULL,
                xml,  kCFPropertyListImmutable, &xmlError);
        CFRelease(xml);
    }

    return ref;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_status (struct client *client, struct msg *msg, void **reply)
{
    struct ppp_status 		*stat;
    struct ifpppstatsreq 	rq;
    struct ppp			*ppp = ppp_find(msg);
    int		 		s;
    u_int32_t			retrytime, curtime;

    PRINTF(("PPP_STATUS\n"));

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        msg->hdr.m_len = 0;
        return 0;
    }

    *reply = CFAllocatorAllocate(NULL, sizeof(struct ppp_status), 0);
    if (*reply == 0) {
        msg->hdr.m_result = ENOMEM;
        msg->hdr.m_len = 0;
        return 0;
    }
    stat = (struct ppp_status *)*reply;

    bzero (stat, sizeof (struct ppp_status));
    switch (ppp->phase) {
        case PPP_STATERESERVED:
        case PPP_HOLDOFF:
            stat->status = PPP_IDLE;		// Dial on demand does not exist in the api
            break;
        default:
            stat->status = ppp->phase;
    }

    switch (stat->status) {
        case PPP_RUNNING:
        case PPP_ONHOLD:

            s = socket(AF_INET, SOCK_DGRAM, 0);
            if (s < 0) {
                msg->hdr.m_result = errno;
                msg->hdr.m_len = 0;
            	CFAllocatorDeallocate(NULL, *reply);
                return 0;
            }
    
            bzero (&rq, sizeof (rq));
    
            sprintf(rq.ifr_name, "%s%d", ppp->name, ppp->ifunit);
            if (ioctl(s, SIOCGPPPSTATS, &rq) < 0) {
                close(s);
                msg->hdr.m_result = errno;
                msg->hdr.m_len = 0;
            	CFAllocatorDeallocate(NULL, *reply);
                return 0;
            }
    
            close(s);

            curtime = mach_absolute_time() * gTimeScaleSeconds;
            if (ppp->conntime)
		stat->s.run.timeElapsed = curtime - ppp->conntime;
            if (!ppp->disconntime)	// no limit...
     	       stat->s.run.timeRemaining = 0xFFFFFFFF;
            else
      	      stat->s.run.timeRemaining = (ppp->disconntime > curtime) ? ppp->disconntime - curtime : 0;

            stat->s.run.outBytes = rq.stats.p.ppp_obytes;
            stat->s.run.inBytes = rq.stats.p.ppp_ibytes;
            stat->s.run.inPackets = rq.stats.p.ppp_ipackets;
            stat->s.run.outPackets = rq.stats.p.ppp_opackets;
            stat->s.run.inErrors = rq.stats.p.ppp_ierrors;
            stat->s.run.outErrors = rq.stats.p.ppp_ierrors;
            break;
            
        case PPP_WAITONBUSY:
        
            stat->s.busy.timeRemaining = 0;
            retrytime = 0;
            getNumberFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPRetryConnectTime, &retrytime);
            if (retrytime) {
                curtime = mach_absolute_time() * gTimeScaleSeconds;
                stat->s.busy.timeRemaining = (curtime < retrytime) ? retrytime - curtime : 0;
            }
            break;
         
        default:
            stat->s.disc.lastDiscCause = ppp_translate_error(ppp->subtype, ppp->laststatus, ppp->lastdevstatus);
    }

    msg->hdr.m_result = 0;
    msg->hdr.m_len = sizeof(struct ppp_status);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void AddNumber(CFMutableDictionaryRef dict, CFStringRef property, u_int32_t nunmber) 
{
   CFNumberRef num;
    num = CFNumberCreate(NULL, kCFNumberSInt32Type, &nunmber);
    if (num) {
        CFDictionaryAddValue(dict, property, num);
        CFRelease(num); 
    } 
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void AddString(CFMutableDictionaryRef dict, CFStringRef property, char *string) 
{
    CFStringRef str;
    str = CFStringCreateWithCString(NULL, string, kCFStringEncodingUTF8);
    if (str) { 
        CFDictionaryAddValue(dict, property, str);
        CFRelease(str); 
    }
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void AddNumberFromState(struct ppp *ppp, CFStringRef entity, CFStringRef property, CFMutableDictionaryRef dict) 
{
    u_int32_t 	lval;
    
    if (getNumberFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, entity, property, &lval)) 
        AddNumber(dict, property, lval);
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void AddStringFromState(struct ppp *ppp, CFStringRef entity, CFStringRef property, CFMutableDictionaryRef dict) 
{
    CFStringRef	string;
    
    if (string = copyCFStringFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, entity, property)) {
        CFDictionaryAddValue(dict, property, string);
        CFRelease(string);
    }
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_extendedstatus (struct client *client, struct msg *msg, void **reply)
{
    struct ppp			*ppp = ppp_find(msg);
    CFMutableDictionaryRef	statusdict = 0, dict = 0;
    CFDataRef			dataref = 0;
    void			*dataptr = 0;
    u_int32_t			datalen = 0;
    
   PRINTF(("PPP_EXTENDEDSTATUS\n"));

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        msg->hdr.m_len = 0;
        return 0;
    }

    if ((statusdict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
        goto fail;

    /* create and add PPP dictionary */
    if ((dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
        goto fail;
    
    AddNumber(dict, kSCPropNetPPPStatus, ppp->phase);
    
    if (ppp->phase != PPP_IDLE)
        AddStringFromState(ppp, kSCEntNetPPP, kSCPropNetPPPCommRemoteAddress, dict);

    switch (ppp->phase) {
        case PPP_RUNNING:
        case PPP_ONHOLD:

            AddNumber(dict, kSCPropNetPPPConnectTime, ppp->conntime);
            AddNumber(dict, kSCPropNetPPPDisconnectTime, ppp->disconntime);
            AddNumberFromState(ppp, kSCEntNetPPP, kSCPropNetPPPLCPCompressionPField, dict);
            AddNumberFromState(ppp, kSCEntNetPPP, kSCPropNetPPPLCPCompressionACField, dict);
            AddNumberFromState(ppp, kSCEntNetPPP, kSCPropNetPPPLCPMRU, dict);
            AddNumberFromState(ppp, kSCEntNetPPP, kSCPropNetPPPLCPMTU, dict);
            AddNumberFromState(ppp, kSCEntNetPPP, kSCPropNetPPPLCPReceiveACCM, dict);
            AddNumberFromState(ppp, kSCEntNetPPP, kSCPropNetPPPLCPTransmitACCM, dict);
            AddNumberFromState(ppp, kSCEntNetPPP, kSCPropNetPPPIPCPCompressionVJ, dict);
            break;
            
        case PPP_WAITONBUSY:
            AddNumberFromState(ppp, kSCEntNetPPP, kSCPropNetPPPRetryConnectTime, dict);
            break;
         
        case PPP_STATERESERVED:
            break;
            
        default:
            AddNumber(dict, kSCPropNetPPPLastCause, ppp->laststatus);
            AddNumber(dict, kSCPropNetPPPDeviceLastCause, ppp->lastdevstatus);
    }

    CFDictionaryAddValue(statusdict, kSCEntNetPPP, dict);
    CFRelease(dict);

    /* create and add Modem dictionary */
    if (ppp->subtype == PPP_TYPE_SERIAL
        && (ppp->phase == PPP_RUNNING || ppp->phase == PPP_ONHOLD)) {
        if ((dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
            goto fail;

        AddNumberFromState(ppp, kSCEntNetModem, kSCPropNetModemConnectSpeed, dict);
            
        CFDictionaryAddValue(statusdict, kSCEntNetModem, dict);
        CFRelease(dict);
    }

    /* create and add IPv4 dictionary */
    if (ppp->phase == PPP_RUNNING || ppp->phase == PPP_ONHOLD) {
        dict = (CFMutableDictionaryRef)copyEntity(kSCDynamicStoreDomainState, ppp->serviceID, kSCEntNetIPv4);
        if (dict) {
            CFDictionaryAddValue(statusdict, kSCEntNetIPv4, dict);
            CFRelease(dict);
        }
    }
    
    /* We are done, now serialize it */
    if ((dataref = ppp_serialize(statusdict, &dataptr, &datalen)) == 0)
        goto fail;
    
    *reply = CFAllocatorAllocate(NULL, datalen, 0);
    if (*reply == 0)
        goto fail;

    bcopy(dataptr, *reply, datalen);    

    CFRelease(statusdict);
    CFRelease(dataref);
    msg->hdr.m_result = 0;
    msg->hdr.m_len = datalen;
    return 0;

fail:
    if (statusdict)
        CFRelease(statusdict);
    if (dict)
        CFRelease(dict);
    if (dataref)
        CFRelease(dataref);
    msg->hdr.m_result = ENOMEM;
    msg->hdr.m_len = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_connect (struct client *client, struct msg *msg, void **reply)
{
    void 		*data = (struct ppp_status *)&msg->data[MSG_DATAOFF(msg)];
    struct ppp		*ppp = ppp_find(msg);
    CFDictionaryRef	opts = 0;

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        msg->hdr.m_len = 0;
        return 0;
    }

    PRINTF(("PPP_CONNECT\n"));
        
    if (msg->hdr.m_len == 0) {
        // first find current the appropriate set of options
        opts = client_findoptset(client, ppp->serviceID);
    }
    else {
        opts = (CFDictionaryRef)ppp_unserialize(data, msg->hdr.m_len);
        if (opts == 0 || CFGetTypeID(opts) != CFDictionaryGetTypeID()) {
            msg->hdr.m_result = ENOMEM;
            msg->hdr.m_len = 0;
            if (opts) 
                CFRelease(opts);
            return 0;
        }
    }
    
    msg->hdr.m_result = ppp_doconnect(ppp, opts, 0, 
        (msg->hdr.m_flags & CONNECT_ARBITRATED_FLAG) ? client : 0,  
        (msg->hdr.m_flags & CONNECT_AUTOCLOSE_FLAG) ? 1 : 0);
    if (opts && msg->hdr.m_len) 
        CFRelease(opts);
    msg->hdr.m_len = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_disconnect (struct client *client, struct msg *msg, void **reply)
{
    struct ppp		*ppp = ppp_find(msg);
    
    PRINTF(("PPP_DISCONNECT\n"));

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        msg->hdr.m_len = 0;
        return 0;
    }
    
    ppp_dodisconnect(ppp, 15, (msg->hdr.m_flags & DISCONNECT_ARBITRATED_FLAG) ? client : 0); /* 1 */

    msg->hdr.m_result = 0;
    msg->hdr.m_len = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_suspend (struct client *client, struct msg *msg, void **reply)
{
    struct ppp		*ppp = ppp_find(msg);
    
    PRINTF(("PPP_SUSPEND\n"));

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        msg->hdr.m_len = 0;
        return 0;
    }
    
    ppp_dosuspend(ppp); 

    msg->hdr.m_result = 0;
    msg->hdr.m_len = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_resume (struct client *client, struct msg *msg, void **reply)
{
    struct ppp		*ppp = ppp_find(msg);
    
    PRINTF(("PPP_RESUME\n"));

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        msg->hdr.m_len = 0;
        return 0;
    }
    
    ppp_doresume(ppp);

    msg->hdr.m_result = 0;
    msg->hdr.m_len = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_getconnectdata (struct client *client, struct msg *msg, void **reply)
{
    struct ppp			*ppp = ppp_find(msg);
    CFDataRef			dataref = 0;
    void			*dataptr = 0;
    u_int32_t			datalen = 0;
    CFDictionaryRef		opts;
    CFMutableDictionaryRef	mdict, mdict1;
    CFDictionaryRef	dict;

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        msg->hdr.m_len = 0;
        return 0;
    }

    PRINTF(("PPP_GETCONNECTDATA\n"));
        
    /* return saved data */
    opts = ppp->needconnectopts ? ppp->needconnectopts : ppp->connectopts;

    if (opts == 0) {
        // no data
        msg->hdr.m_len = 0;
        return 0;
    }
    
    /* special code to remove secret information */

    mdict = CFDictionaryCreateMutableCopy(0, 0, opts);
    if (mdict == 0) {
        // no data
        msg->hdr.m_len = 0;
        return 0;
    }
    
    dict = CFDictionaryGetValue(mdict, kSCEntNetPPP);
    if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID())) {
        mdict1 = CFDictionaryCreateMutableCopy(0, 0, dict);
        if (mdict1) {
            CFDictionaryRemoveValue(mdict1, kSCPropNetPPPAuthPassword);
            CFDictionarySetValue(mdict, kSCEntNetPPP, mdict1);
            CFRelease(mdict1);
        }
    }

    dict = CFDictionaryGetValue(mdict, kSCEntNetL2TP);
    if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID())) {
        mdict1 = CFDictionaryCreateMutableCopy(0, 0, dict);
        if (mdict1) {
            CFDictionaryRemoveValue(mdict1, SCSTR("IPSecSharedSecret"));
            CFDictionarySetValue(mdict, kSCEntNetL2TP, mdict1);
            CFRelease(mdict1);
        }
    }
    
    if ((dataref = ppp_serialize(mdict, &dataptr, &datalen)) == 0) {
        msg->hdr.m_result = ENOMEM;
        msg->hdr.m_len = 0;
        return 0;
    }
    
    *reply = CFAllocatorAllocate(NULL, datalen, 0);
    if (*reply == 0) {
        msg->hdr.m_result = ENOMEM;
        msg->hdr.m_len = 0;
    }
    else {
        bcopy(dataptr, *reply, datalen);
        msg->hdr.m_result = 0;
        msg->hdr.m_len = datalen;
    }

    CFRelease(mdict);
    CFRelease(dataref);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_enable_event (struct client *client, struct msg *msg, void **reply)
{
    u_int32_t	notification = 1; // type of notification, event or status
    
    PRINTF(("PPP_ENABLE_EVENT\n"));

    if (msg->hdr.m_len == 4) {
        notification = *(u_int32_t *)&msg->data[MSG_DATAOFF(msg)];
        if (notification > 3) {
            msg->hdr.m_result = EINVAL;
            msg->hdr.m_len = 0;
            return 0;
        }
    }

    msg->hdr.m_result = 0;
    client->notify |= notification;
    client->notify_link = 0;
    client->notify_useserviceid = ((msg->hdr.m_flags & USE_SERVICEID) == USE_SERVICEID);
    if (client->notify_useserviceid && msg->hdr.m_link) {        
        if (client->notify_serviceid = malloc(msg->hdr.m_link + 1)) {
            strncpy(client->notify_serviceid, msg->data, msg->hdr.m_link);
            client->notify_serviceid[msg->hdr.m_link] = 0;
        }
        else 
            msg->hdr.m_result = ENOMEM;
    }
    else 
        client->notify_link = msg->hdr.m_link;

    msg->hdr.m_len = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_disable_event (struct client *client, struct msg *msg, void **reply)
{
    u_int32_t	notification = 1; // type of notification, event or status
    
    PRINTF(("PPP_DISABLE_EVENT\n"));

    if (msg->hdr.m_len == 4) {
        notification = *(u_int32_t *)&msg->data[MSG_DATAOFF(msg)];
        if (notification > 3) {
            msg->hdr.m_result = EINVAL;
            msg->hdr.m_len = 0;
            return 0;
        }
    }

    client->notify &= ~notification;    
    if (client->notify == 0) {
        client->notify_link = 0;    
        client->notify_useserviceid = 0;    
        if (client->notify_serviceid) {
            free(client->notify_serviceid);
            client->notify_serviceid = 0;
        }
    }
    msg->hdr.m_result = 0;
    msg->hdr.m_len = 0;
    return 0;
}

 
/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_version (struct client *client, struct msg *msg, void **reply)
{
    PRINTF(("PPP_DISABLE_EVENT\n"));

    *reply = CFAllocatorAllocate(NULL, sizeof(u_int32_t), 0);
    if (*reply == 0) {
        msg->hdr.m_result = ENOMEM;
        msg->hdr.m_len = 0;
    }
    else {
        msg->hdr.m_result = 0;
        msg->hdr.m_len = sizeof(u_int32_t);
        *(u_int32_t*)*reply = CURRENT_VERSION;
    }

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_getnblinks (struct client *client, struct msg *msg, void **reply)
{
    u_long		nb = 0;
    struct ppp		*ppp;
    u_short		subtype = msg->hdr.m_link >> 16;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if ((subtype == 0xFFFF)
            || ( subtype == ppp->subtype)) {
            nb++;
        }
    }

    *reply = CFAllocatorAllocate(NULL, sizeof(u_int32_t), 0);
    if (*reply == 0) {
        msg->hdr.m_result = ENOMEM;
        msg->hdr.m_len = 0;
    }
    else {
        msg->hdr.m_result = 0;
        msg->hdr.m_len = sizeof(u_int32_t);
        *(u_int32_t*)*reply = nb;
    }
    return 0;

}

/* -----------------------------------------------------------------------------
index is a global index across all the link types (or within the family)
index if between 0 and nblinks
----------------------------------------------------------------------------- */
u_long ppp_getlinkbyindex (struct client *client, struct msg *msg, void **reply)
{
    u_long		nb = 0, len = 0, err = ENODEV, index;
    struct ppp		*ppp;
    u_short		subtype = msg->hdr.m_link >> 16;

    index = *(u_long *)&msg->data[0];

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if ((subtype == 0xFFFF)
            || (subtype == ppp->subtype)) {
            if (nb == index) {
                *reply = CFAllocatorAllocate(NULL, sizeof(u_int32_t), 0);
                if (*reply == 0)
                    err = ENOMEM;
                else {
                    err = 0;
                    len = sizeof(u_int32_t);
                    *(u_int32_t*)*reply = ppp_makeref(ppp);
                }
                break;
            }
            nb++;
        }
    }
    
    msg->hdr.m_result = err;
    msg->hdr.m_len = len;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_getlinkbyserviceid (struct client *client, struct msg *msg, void **reply)
{
    u_long		len = 0, err = ENODEV;
    struct ppp		*ppp;
    CFStringRef		ref;

    msg->data[msg->hdr.m_len] = 0;
    ref = CFStringCreateWithCString(NULL, msg->data, kCFStringEncodingUTF8);
    if (ref) {
	ppp = ppp_findbyserviceID(ref);
        if (ppp) {
            *reply = CFAllocatorAllocate(NULL, sizeof(u_int32_t), 0);
            if (*reply == 0)
                err = ENOMEM;
            else {
                err = 0;
                len = sizeof(u_int32_t);
                *(u_int32_t*)*reply = ppp_makeref(ppp);
            }
        }
        CFRelease(ref);
    }
    else 
        err = ENOMEM;
    
    msg->hdr.m_result = err;
    msg->hdr.m_len = len;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_getlinkbyifname (struct client *client, struct msg *msg, void **reply)
{
    u_long		len = 0, err = ENODEV;
    struct ppp		*ppp;
    u_char      	ifname[IFNAMSIZ] = { 0 };
    u_int16_t 		ifunit = 0, i = 0;

    while ((i < msg->hdr.m_len) && (i < sizeof(ifname)) 
        && ((msg->data[i] < '0') || (msg->data[i] > '9'))) {
        ifname[i] = msg->data[i];
        i++;
    }
    ifname[i] = 0;
    while ((i < msg->hdr.m_len) 
        && (msg->data[i] >= '0') && (msg->data[i] <= '9')) {
        ifunit = (ifunit * 10) + (msg->data[i] - '0');
        i++;
    }

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if (!strcmp(ppp->name, ifname)
            && (ppp->ifunit == ifunit)) {
            
            if (msg->hdr.m_flags & USE_SERVICEID) {
                *reply = CFAllocatorAllocate(NULL, strlen(ppp->sid), 0);
                if (*reply == 0)
                    err = ENOMEM;
                else {
                    err = 0;
                    len = strlen(ppp->sid);
                    bcopy(ppp->sid, *reply, len);
                }
            }
            else {
                *reply = CFAllocatorAllocate(NULL, sizeof(u_int32_t), 0);
                if (*reply == 0)
                    err = ENOMEM;
                else {
                    err = 0;
                    len = sizeof(u_int32_t);
                    *(u_int32_t*)*reply = ppp_makeref(ppp);
                }
            }
            break;
        }
    }

    msg->hdr.m_result = err;
    msg->hdr.m_len = len;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_event(struct client *client, struct msg *msg)
{
    u_int32_t		event = *(u_int32_t *)&msg->data[0];
    u_int32_t		error = *(u_int32_t *)&msg->data[4];
    u_char 		*serviceid = &msg->data[8];
    CFStringRef		ref;
    struct ppp		*ppp;
    CFDictionaryRef	service = NULL;

    serviceid[msg->hdr.m_len - 8] = 0;	// need to zeroterminate the serviceid
    msg->hdr.m_len = 0xFFFFFFFF; // no reply
    //printf("ppp_event, event = 0x%x, cause = 0x%x, serviceid = '%s'\n", event, error, serviceid);

    ref = CFStringCreateWithCString(NULL, serviceid, kCFStringEncodingUTF8);
    if (ref) {
        ppp = ppp_findbyserviceID(ref);
        if (ppp) {
        
           // update status information first
            service = copyEntity(kSCDynamicStoreDomainState, ref, kSCEntNetPPP);
            if (service) {
                ppp_updatestate(ppp, service);
                CFRelease(service);
            }
        
            if (event == PPP_EVT_DISCONNECTED) {
                //if (error == EXIT_USER_REQUEST)
                //    return;	// PPP API generates PPP_EVT_DISCONNECTED only for unrequested disconnections
                error = ppp_translate_error(ppp->subtype, error, 0);
            }
            else 
                error = 0;
            client_notify(ppp->sid, ppp_makeref(ppp), event, error, 1);
        }
        CFRelease(ref);
    }
}

/* -----------------------------------------------------------------------------
translate a pppd native cause into a PPP API cause
----------------------------------------------------------------------------- */
u_int32_t ppp_translate_error(u_int16_t subtype, u_int32_t native_ppp_error, u_int32_t native_dev_error)
{
    u_int32_t	error = PPP_ERR_GEN_ERROR; 
    
    switch (native_ppp_error) {
        case EXIT_USER_REQUEST:
            error = 0;
            break;
        case EXIT_CONNECT_FAILED:
            error = PPP_ERR_GEN_ERROR;
            break;
        case EXIT_TERMINAL_FAILED:
            error = PPP_ERR_TERMSCRIPTFAILED;
            break;
        case EXIT_NEGOTIATION_FAILED:
            error = PPP_ERR_LCPFAILED;
            break;
        case EXIT_AUTH_TOPEER_FAILED:
            error = PPP_ERR_AUTHFAILED;
            break;
        case EXIT_IDLE_TIMEOUT:
            error = PPP_ERR_IDLETIMEOUT;
            break;
        case EXIT_CONNECT_TIME:
            error = PPP_ERR_SESSIONTIMEOUT;
            break;
        case EXIT_LOOPBACK:
            error = PPP_ERR_LOOPBACK;
            break;
        case EXIT_PEER_DEAD:
            error = PPP_ERR_PEERDEAD;
            break;
        case EXIT_OK:
            error = PPP_ERR_DISCBYPEER;
            break;
        case EXIT_HANGUP:
            error = PPP_ERR_DISCBYDEVICE;
            break;
    }
    
    // override with a more specific error
    if (native_dev_error) {
        switch (subtype) {
            case PPP_TYPE_SERIAL:
                switch (native_dev_error) {
                    case EXIT_PPPSERIAL_NOCARRIER:
                        error = PPP_ERR_MOD_NOCARRIER;
                        break;
                    case EXIT_PPPSERIAL_NONUMBER:
                        error = PPP_ERR_MOD_NONUMBER;
                        break;
                    case EXIT_PPPSERIAL_BADSCRIPT:
                        error = PPP_ERR_MOD_BADSCRIPT;
                        break;
                    case EXIT_PPPSERIAL_BUSY:
                        error = PPP_ERR_MOD_BUSY;
                        break;
                    case EXIT_PPPSERIAL_NODIALTONE:
                        error = PPP_ERR_MOD_NODIALTONE;
                        break;
                    case EXIT_PPPSERIAL_ERROR:
                        error = PPP_ERR_MOD_ERROR;
                        break;
                    case EXIT_PPPSERIAL_NOANSWER:
                        error = PPP_ERR_MOD_NOANSWER;
                        break;
                    case EXIT_PPPSERIAL_HANGUP:
                        error = PPP_ERR_MOD_HANGUP;
                        break;
                    default :
                        error = PPP_ERR_CONNSCRIPTFAILED;
                }
                break;
    
            case PPP_TYPE_PPPoE:
                // need to handle PPPoE specific error codes
                break;
        }
    }
    
    return error;
}
