/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "DNSServiceDiscovery.h"
#include "DNSServiceDiscoveryDefines.h"

#include <stdlib.h>
#include <stdio.h>
#include <servers/bootstrap.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <pthread.h>

#include <netinet/in.h>

extern struct mig_subsystem internal_DNSServiceDiscoveryReply_subsystem;

extern boolean_t DNSServiceDiscoveryReply_server(
        mach_msg_header_t *InHeadP,
        mach_msg_header_t *OutHeadP);

extern
kern_return_t DNSServiceBrowserCreate_rpc
(
    mach_port_t server,
    mach_port_t client,
    DNSCString regtype,
    DNSCString domain
);

extern
kern_return_t DNSServiceDomainEnumerationCreate_rpc
(
    mach_port_t server,
    mach_port_t client,
    int registrationDomains
);

extern
kern_return_t DNSServiceRegistrationCreate_rpc
(
    mach_port_t server,
    mach_port_t client,
    DNSCString name,
    DNSCString regtype,
    DNSCString domain,
    IPPort port,
    DNSCString txtRecord
);

extern
kern_return_t DNSServiceResolverResolve_rpc
(
    mach_port_t server,
    mach_port_t client,
    DNSCString name,
    DNSCString regtype,
    DNSCString domain
);

extern
kern_return_t DNSServiceRegistrationAddRecord_rpc
(
    mach_port_t server,
    mach_port_t client,
    int type,
    record_data_t data,
    mach_msg_type_number_t record_dataCnt,
    uint32_t ttl,
    natural_t *reference
);

extern
int DNSServiceRegistrationUpdateRecord_rpc
(
    mach_port_t server,
    mach_port_t client,
    natural_t reference,
    record_data_t data,
    mach_msg_type_number_t record_dataCnt,
    uint32_t ttl
);

extern
kern_return_t DNSServiceRegistrationRemoveRecord_rpc
(
    mach_port_t server,
    mach_port_t client,
    natural_t reference
);

struct a_requests {
    struct a_requests		*next;
    mach_port_t				client_port;
    union {
        DNSServiceBrowserReply 				browserCallback;
        DNSServiceDomainEnumerationReply 	enumCallback;
        DNSServiceRegistrationReply 		regCallback;
        DNSServiceResolverReply 			resolveCallback;
    } callout;
    void					*context;
};

static struct a_requests	*a_requests	= NULL;
static pthread_mutex_t		a_requests_lock	= PTHREAD_MUTEX_INITIALIZER;

typedef struct _dns_service_discovery_t {
    mach_port_t	port;
} dns_service_discovery_t;

mach_port_t DNSServiceDiscoveryLookupServer(void)
{
    static mach_port_t sndPort 	= MACH_PORT_NULL;
    kern_return_t   result;

    if (sndPort != MACH_PORT_NULL) {
        return sndPort;
    }

    result = bootstrap_look_up(bootstrap_port, DNS_SERVICE_DISCOVERY_SERVER, &sndPort);
    if (result != KERN_SUCCESS) {
        printf("%s(): {%s:%d} bootstrap_look_up() failed: $%x\n", __FUNCTION__, __FILE__, __LINE__, (int) result);
        sndPort = MACH_PORT_NULL;
    }


    return sndPort;
}

void _increaseQueueLengthOnPort(mach_port_t port)
{
    mach_port_limits_t qlimits;
    kern_return_t result;
    
    qlimits.mpl_qlimit = 16;
    result = mach_port_set_attributes(mach_task_self(), port, MACH_PORT_LIMITS_INFO, (mach_port_info_t)&qlimits, MACH_PORT_LIMITS_INFO_COUNT);

    if (result != KERN_SUCCESS) {
        printf("%s(): {%s:%d} mach_port_set_attributes() failed: $%x %s\n", __FUNCTION__, __FILE__, __LINE__, (int) result, mach_error_string(result));
    }
}

dns_service_discovery_ref DNSServiceBrowserCreate (const char *regtype, const char *domain, DNSServiceBrowserReply callBack,void *context)
{
    mach_port_t serverPort = DNSServiceDiscoveryLookupServer();
    mach_port_t clientPort;
    kern_return_t result;
    dns_service_discovery_ref return_t;
    struct a_requests	*request;
    
    if (!serverPort) {
        return NULL;
    }

    result = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &clientPort);
    if (result != KERN_SUCCESS) {
        printf("Mach port receive creation failed, %s\n", mach_error_string(result));
        return NULL;
    }
    result = mach_port_insert_right(mach_task_self(), clientPort, clientPort, MACH_MSG_TYPE_MAKE_SEND);
    if (result != KERN_SUCCESS) {
        printf("Mach port send creation failed, %s\n", mach_error_string(result));
        mach_port_destroy(mach_task_self(), clientPort);
        return NULL;
    }
    _increaseQueueLengthOnPort(clientPort);

    return_t = malloc(sizeof(dns_service_discovery_t));
    return_t->port = clientPort;

    request = malloc(sizeof(struct a_requests));
    request->client_port = clientPort;
    request->context = context;
    request->callout.browserCallback = callBack;

    result = DNSServiceBrowserCreate_rpc(serverPort, clientPort, (char *)regtype, (char *)domain);

    if (result != KERN_SUCCESS) {
        printf("There was an error creating a browser, %s\n", mach_error_string(result));
        free(request);
        return NULL;
    }

    pthread_mutex_lock(&a_requests_lock);
    request->next = a_requests;
    a_requests = request;
    pthread_mutex_unlock(&a_requests_lock);
    
    return return_t;
}

/* Service Enumeration */

dns_service_discovery_ref DNSServiceDomainEnumerationCreate (int registrationDomains, DNSServiceDomainEnumerationReply callBack, void *context)
{
    mach_port_t serverPort = DNSServiceDiscoveryLookupServer();
    mach_port_t clientPort;
    kern_return_t result;
    dns_service_discovery_ref return_t;
    struct a_requests	*request;

    if (!serverPort) {
        return NULL;
    }

    result = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &clientPort);
    if (result != KERN_SUCCESS) {
        printf("Mach port receive creation failed, %s\n", mach_error_string(result));
        return NULL;
    }
    result = mach_port_insert_right(mach_task_self(), clientPort, clientPort, MACH_MSG_TYPE_MAKE_SEND);
    if (result != KERN_SUCCESS) {
        printf("Mach port send creation failed, %s\n", mach_error_string(result));
        mach_port_destroy(mach_task_self(), clientPort);
        return NULL;
    }
    _increaseQueueLengthOnPort(clientPort);

    return_t = malloc(sizeof(dns_service_discovery_t));
    return_t->port = clientPort;

    request = malloc(sizeof(struct a_requests));
    request->client_port = clientPort;
    request->context = context;
    request->callout.enumCallback = callBack;

    result = DNSServiceDomainEnumerationCreate_rpc(serverPort, clientPort, registrationDomains);

    if (result != KERN_SUCCESS) {
        printf("There was an error creating an enumerator, %s\n", mach_error_string(result));
        free(request);
        return NULL;
    }
    
    pthread_mutex_lock(&a_requests_lock);
    request->next = a_requests;
    a_requests = request;
    pthread_mutex_unlock(&a_requests_lock);

    return return_t;
}


/* Service Registration */

dns_service_discovery_ref DNSServiceRegistrationCreate
(const char *name, const char *regtype, const char *domain, uint16_t port, const char *txtRecord, DNSServiceRegistrationReply callBack, void *context)
{
    mach_port_t serverPort = DNSServiceDiscoveryLookupServer();
    mach_port_t clientPort;
    kern_return_t		result;
    dns_service_discovery_ref return_t;
    struct a_requests	*request;
    IPPort IpPort;
	char *portptr = (char *)&port;
	
    if (!serverPort) {
        return NULL;
    }

    if (!txtRecord) {
      txtRecord = "";
    }

    result = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &clientPort);
    if (result != KERN_SUCCESS) {
        printf("Mach port receive creation failed, %s\n", mach_error_string(result));
        return NULL;
    }
    result = mach_port_insert_right(mach_task_self(), clientPort, clientPort, MACH_MSG_TYPE_MAKE_SEND);
    if (result != KERN_SUCCESS) {
        printf("Mach port send creation failed, %s\n", mach_error_string(result));
        mach_port_destroy(mach_task_self(), clientPort);
        return NULL;
    }
    _increaseQueueLengthOnPort(clientPort);

    return_t = malloc(sizeof(dns_service_discovery_t));
    return_t->port = clientPort;

    request = malloc(sizeof(struct a_requests));
    request->client_port = clientPort;
    request->context = context;
    request->callout.regCallback = callBack;

	// older versions of this code passed the port via mach IPC as an int.
	// we continue to pass it as 4 bytes to maintain binary compatibility,
	// but now ensure that the network byte order is preserved by using a struct
	IpPort.bytes[0] = 0;
	IpPort.bytes[1] = 0;
	IpPort.bytes[2] = portptr[0];
	IpPort.bytes[3] = portptr[1];

    result = DNSServiceRegistrationCreate_rpc(serverPort, clientPort, (char *)name, (char *)regtype, (char *)domain, IpPort, (char *)txtRecord);

    if (result != KERN_SUCCESS) {
        printf("There was an error creating a resolve, %s\n", mach_error_string(result));
        free(request);
        return NULL;
    }

    pthread_mutex_lock(&a_requests_lock);
    request->next = a_requests;
    a_requests = request;
    pthread_mutex_unlock(&a_requests_lock);

    return return_t;
}

/* Resolver requests */

dns_service_discovery_ref DNSServiceResolverResolve(const char *name, const char *regtype, const char *domain, DNSServiceResolverReply callBack, void *context)
{
    mach_port_t serverPort = DNSServiceDiscoveryLookupServer();
    mach_port_t clientPort;
    kern_return_t		result;
    dns_service_discovery_ref return_t;
    struct a_requests	*request;

    if (!serverPort) {
        return NULL;
    }

    result = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &clientPort);
    if (result != KERN_SUCCESS) {
        printf("Mach port receive creation failed, %s\n", mach_error_string(result));
        return NULL;
    }
    result = mach_port_insert_right(mach_task_self(), clientPort, clientPort, MACH_MSG_TYPE_MAKE_SEND);
    if (result != KERN_SUCCESS) {
        printf("Mach port send creation failed, %s\n", mach_error_string(result));
        mach_port_destroy(mach_task_self(), clientPort);
        return NULL;
    }
    _increaseQueueLengthOnPort(clientPort);

    return_t = malloc(sizeof(dns_service_discovery_t));
    return_t->port = clientPort;

    request = malloc(sizeof(struct a_requests));
    request->client_port = clientPort;
    request->context = context;
    request->callout.resolveCallback = callBack;

    DNSServiceResolverResolve_rpc(serverPort, clientPort, (char *)name, (char *)regtype, (char *)domain);

    pthread_mutex_lock(&a_requests_lock);
    request->next = a_requests;
    a_requests = request;
    pthread_mutex_unlock(&a_requests_lock);

    return return_t;
}

DNSRecordReference DNSServiceRegistrationAddRecord(dns_service_discovery_ref ref, uint16_t rrtype, uint16_t rdlen, const char *rdata, uint32_t ttl)
{
    mach_port_t serverPort = DNSServiceDiscoveryLookupServer();
    mach_port_t clientPort;
    natural_t reference = 0;
    kern_return_t result = KERN_SUCCESS;

    if (!serverPort) {
        return kDNSServiceDiscoveryUnknownErr;
    }

    clientPort = DNSServiceDiscoveryMachPort(ref);

    if (!clientPort) {
        return kDNSServiceDiscoveryUnknownErr;
    }

    result = DNSServiceRegistrationAddRecord_rpc(serverPort, clientPort, rrtype, (record_data_t)rdata, rdlen, ttl, &reference);

    if (result != KERN_SUCCESS) {
        printf("The result of the registration was not successful.  Error %d, result %s\n", result, mach_error_string(result));
    }
    
    return reference;
}

DNSServiceRegistrationReplyErrorType DNSServiceRegistrationUpdateRecord(dns_service_discovery_ref ref, DNSRecordReference reference, uint16_t rdlen, const char *rdata, uint32_t ttl)
{
    mach_port_t serverPort = DNSServiceDiscoveryLookupServer();
    mach_port_t clientPort;
    kern_return_t result = KERN_SUCCESS;

    if (!serverPort) {
        return kDNSServiceDiscoveryUnknownErr;
    }

    clientPort = DNSServiceDiscoveryMachPort(ref);

    if (!clientPort) {
        return kDNSServiceDiscoveryUnknownErr;
    }

    result = DNSServiceRegistrationUpdateRecord_rpc(serverPort, clientPort, (natural_t)reference, (record_data_t)rdata, rdlen, ttl);
    if (result != KERN_SUCCESS) {
        printf("The result of the registration was not successful.  Error %d, result %s\n", result, mach_error_string(result));
        return result;
    }

    return kDNSServiceDiscoveryNoError;
}


DNSServiceRegistrationReplyErrorType DNSServiceRegistrationRemoveRecord(dns_service_discovery_ref ref, DNSRecordReference reference)
{
    mach_port_t serverPort = DNSServiceDiscoveryLookupServer();
    mach_port_t clientPort;
    kern_return_t result = KERN_SUCCESS;

    if (!serverPort) {
        return kDNSServiceDiscoveryUnknownErr;
    }

    clientPort = DNSServiceDiscoveryMachPort(ref);

    if (!clientPort) {
        return kDNSServiceDiscoveryUnknownErr;
    }

    result = DNSServiceRegistrationRemoveRecord_rpc(serverPort, clientPort, (natural_t)reference);

    if (result != KERN_SUCCESS) {
        printf("The result of the registration was not successful.  Error %d, result %s\n", result, mach_error_string(result));
        return result;
    }

    return kDNSServiceDiscoveryNoError;
}

void DNSServiceDiscovery_handleReply(void *replyMsg)
{
    unsigned long			result = 0xFFFFFFFF;
    mach_msg_header_t *    	msgSendBufPtr;
    mach_msg_header_t *     receivedMessage;
    unsigned        		msgSendBufLength;

    msgSendBufLength = internal_DNSServiceDiscoveryReply_subsystem.maxsize;
    msgSendBufPtr = (mach_msg_header_t *) malloc(msgSendBufLength);


    receivedMessage = ( mach_msg_header_t * ) replyMsg;

    // Call DNSServiceDiscoveryReply_server to change mig-generated message into a
    // genuine mach message. It will then cause the callback to get called.
    result = DNSServiceDiscoveryReply_server ( receivedMessage, msgSendBufPtr );
    ( void ) mach_msg_send ( msgSendBufPtr );
    free(msgSendBufPtr);
}

mach_port_t DNSServiceDiscoveryMachPort(dns_service_discovery_ref dnsServiceDiscovery)
{
    return dnsServiceDiscovery->port;
}

void DNSServiceDiscoveryDeallocate(dns_service_discovery_ref dnsServiceDiscovery)
{
    struct a_requests	*request0, *request;
    mach_port_t reply = dnsServiceDiscovery->port;

    if (dnsServiceDiscovery->port) {
        pthread_mutex_lock(&a_requests_lock);
        request0 = NULL;
        request  = a_requests;
        while (request) {
            if (request->client_port == reply) {
                /* request info found, remove from list */
                if (request0) {
                    request0->next = request->next;
                } else {
                    a_requests = request->next;
                }
                break;
            } else {
                /* not info for this request, skip to next */
                request0 = request;
                request  = request->next;
            }

        }
        pthread_mutex_unlock(&a_requests_lock);

        free(request);
        
        mach_port_destroy(mach_task_self(), dnsServiceDiscovery->port);

        free(dnsServiceDiscovery);
    }
    return;
}

// reply functions, calls the users setup callbacks with function pointers

kern_return_t internal_DNSServiceDomainEnumerationReply_rpc
(
    mach_port_t reply,
    int resultType,
    DNSCString replyDomain,
    DNSServiceDiscoveryReplyFlags flags
)
{
    struct a_requests	*request;
    void *requestContext = NULL;
    DNSServiceDomainEnumerationReply callback = NULL;

    pthread_mutex_lock(&a_requests_lock);
    request  = a_requests;
    while (request) {
        if (request->client_port == reply) {
            break;
        }
        request = request->next;
    }

    if (request != NULL) {
        callback = (*request->callout.enumCallback);
        requestContext = request->context;
    }
    pthread_mutex_unlock(&a_requests_lock);

    if (request != NULL) {
        (callback)(resultType, replyDomain, flags, requestContext);
    }
    
    return KERN_SUCCESS;

}

kern_return_t internal_DNSServiceBrowserReply_rpc
(
    mach_port_t reply,
    int resultType,
    DNSCString replyName,
    DNSCString replyType,
    DNSCString replyDomain,
    DNSServiceDiscoveryReplyFlags flags
)
{
    struct a_requests	*request;
    void *requestContext = NULL;
    DNSServiceBrowserReply callback = NULL;

    pthread_mutex_lock(&a_requests_lock);
    request  = a_requests;
    while (request) {
        if (request->client_port == reply) {
            break;
        }
        request = request->next;
    }
    if (request != NULL) {
        callback = (*request->callout.browserCallback);
        requestContext = request->context;
    }

    pthread_mutex_unlock(&a_requests_lock);

    if (request != NULL) {
        (callback)(resultType, replyName, replyType, replyDomain, flags, requestContext);
    }

    return KERN_SUCCESS;
}


kern_return_t internal_DNSServiceRegistrationReply_rpc
(
    mach_port_t reply,
    int resultType
)
{
    struct a_requests	*request;
    void *requestContext = NULL;
    DNSServiceRegistrationReply callback = NULL;

    pthread_mutex_lock(&a_requests_lock);
    request  = a_requests;
    while (request) {
        if (request->client_port == reply) {
            break;
        }
        request = request->next;
    }
    if (request != NULL) {
        callback = (*request->callout.regCallback);
        requestContext = request->context;
    }

    pthread_mutex_unlock(&a_requests_lock);
    if (request != NULL) {
        (callback)(resultType, requestContext);
    }
    return KERN_SUCCESS;
}


kern_return_t internal_DNSServiceResolverReply_rpc
(
    mach_port_t reply,
    sockaddr_t interface,
    sockaddr_t address,
    DNSCString txtRecord,
    DNSServiceDiscoveryReplyFlags flags
)
{
    struct sockaddr  *interface_storage = NULL;
    struct sockaddr  *address_storage = NULL;
    struct a_requests	*request;
    void *requestContext = NULL;
    DNSServiceResolverReply callback = NULL;

    if (interface) {
        int len = ((struct sockaddr *)interface)->sa_len;
        interface_storage = (struct sockaddr *)malloc(len);
        bcopy(interface, interface_storage,len);
    }

    if (address) {
        int len = ((struct sockaddr *)address)->sa_len;
        address_storage = (struct sockaddr *)malloc(len);
        bcopy(address, address_storage, len);
    }

    pthread_mutex_lock(&a_requests_lock);
    request  = a_requests;
    while (request) {
        if (request->client_port == reply) {
            break;
        }
        request = request->next;
    }

    if (request != NULL) {
        callback = (*request->callout.resolveCallback);
        requestContext = request->context;
    }
    pthread_mutex_unlock(&a_requests_lock);

    if (request != NULL) {
        (callback)(interface_storage, address_storage, txtRecord, flags, requestContext);
    }

    if (interface) {
        free(interface_storage);
    }
    if (address) {
        free(address_storage);
    }
    
    return KERN_SUCCESS;
}
