/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2003, 2005 by Apple Computer, Inc.
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
 * Browse functions for zeroconfiguration feature.
 * Currently these functions are invoked exclusively by a daemon on the local
 * machine, distccschedd.
 **/


#if defined(DARWIN)


#include <dns_sd.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "config.h"
#include "distcc.h"
#include "exitcode.h"
#include "trace.h"
#include "zeroconf_browse.h"
#include "zeroconf_util.h"


#define ADD_SERVICE_COUNT      16
#define MAX_INTERFACES         16
#define SERVICE_EMPTY_STRING   ""
#define SERVICE_TRAILING_SPACE " "
#define ZC_BROWSE_FLAGS        0
#define ZC_RESOLVE_FLAGS       0


typedef struct serviceEntry {
    char         *description;
    char         *domain;
    char         *fullName;
    char         *hostTarget;
    uint32_t      interfaceIndex[MAX_INTERFACES];
    char         *name;
    uint16_t      port;
    int           refCount;
    char         *regType;
    DNSServiceRef resolveRef;
    int           resolveRefSockFD;
    uint16_t      txtLen;
    char         *txtRecord;
} ServiceEntry;

typedef struct serviceEntryTable {
    size_t         capacity;
    ServiceEntry **services;
} ServiceEntryTable;


static const size_t SERVICE_ENTRY_SIZE     = sizeof(ServiceEntry);
static const size_t SERVICE_ENTRY_PTR_SIZE = sizeof(ServiceEntry *);


static int               zcBrowseFD;
static DNSServiceRef     zcBrowseRef     = NULL;
static int               zcMoreComing    = FALSE;
static char             *zcResolvedSvcs  = NULL;
static int               zcSelectFDCount = 0;
static fd_set            zcSelectFDs; 
static ServiceEntryTable zcServices      = { 0, NULL };


// Bonjour browsing and resolution utility functions


/**
 * Frees all memory allocated for <code>aServiceEntry</code>, including
 * the entry itself.  Terminate any existing resolve attempt.
 **/
static void dcc_zc_free_service_entry(ServiceEntry **aServiceEntry)
{
    // Free the members of aServiceEntry, as required.

    if ( (*aServiceEntry)->description != NULL ) {
        free((*aServiceEntry)->description);
    }

    if ( (*aServiceEntry)->domain != NULL ) {
        free((*aServiceEntry)->domain);
    }

    if ( (*aServiceEntry)->fullName != NULL ) {
        free((*aServiceEntry)->fullName);
    }

    if ( (*aServiceEntry)->hostTarget != NULL ) {
        free((*aServiceEntry)->hostTarget);
    }

    if ( (*aServiceEntry)->name != NULL ) {
        free((*aServiceEntry)->name);
    }

    if ( (*aServiceEntry)->regType != NULL ) {
        free((*aServiceEntry)->regType);
    }

    if ( (*aServiceEntry)->resolveRef != NULL ) {
        dcc_zc_remove_from_select_set((*aServiceEntry)->resolveRefSockFD);
        DNSServiceRefDeallocate((*aServiceEntry)->resolveRef);
    }

    if ( (*aServiceEntry)->txtRecord != NULL ) {
        free((*aServiceEntry)->txtRecord);
    }

    // Free aServiceEntry, and clear out the caller's pointer to it.

    free(*aServiceEntry);
    *aServiceEntry = NULL;
}


// File descriptor functions


/**
 * Determines the highest file descriptor in the <code>dcc_zc_select_set</code>.
 * Only invoked when the select set is changed.
 **/
static void dcc_zc_determine_select_count(void)
{
    int i;

    for ( i = 0; i < FD_SETSIZE; i++ ) {
        if ( FD_ISSET(i, &zcSelectFDs) ) {
            zcSelectFDCount = i;
        }
    }

    zcSelectFDCount++;
}


/**
 * Add <code>anFD</code> to the set of file descriptors to which to listen for
 * incoming messages.
 **/
void dcc_zc_add_to_select_set(int anFD)
{
    if ( anFD > 0 && anFD < FD_SETSIZE ) {
        if ( FD_ISSET(anFD, &zcSelectFDs) ) {
            rs_trace("Already included file descriptor: %d", anFD);
        } else {
            FD_SET(anFD, &zcSelectFDs);
            dcc_zc_determine_select_count();
        }
    } else {
        rs_trace("Unable to include invalid file descriptor: %d", anFD);
    }
}


/**
 * Remove <code>anFD</code> from the set of file descriptors to which to listen
 * for incoming messages.
 **/
void dcc_zc_remove_from_select_set(int anFD)
{
    if ( anFD > 0 && anFD < FD_SETSIZE ) {
        if ( FD_ISSET(anFD, &zcSelectFDs) ) {
            FD_CLR(anFD, &zcSelectFDs);
            dcc_zc_determine_select_count();
        } else {
            rs_trace("Already excluded file descriptor: %d", anFD);
        }
    } else {
        rs_trace("Unable to exclude invalid file descriptor: %d", anFD);
    }
}


/**
 * Returns the highest file descriptor in the <code>dcc_zc_select_set</code>.
 **/
int dcc_zc_select_count(void)
{
    return zcSelectFDCount;
}


/**
 * Copies into <code>setCopy</code> the set of file descriptors to which to
 * listen for incoming messages.
 **/
void dcc_zc_select_set(fd_set *setCopy)
{
    FD_COPY(&zcSelectFDs, setCopy);
}


/**
 * Processes browse messages if the browse socket is among those ready to read
 * (i.e. in <code>availableFDs</code>).  Returns either <code>fdCount</code>
 * or <code>fdCount - 1</code> to indicate the remaining number of active
 * file descriptors in <code>availableFDs</code>.
 **/
int dcc_zc_process_browse_messages(fd_set *availableFDs, int fdCount)
{
    if ( FD_ISSET(zcBrowseFD, availableFDs) ) {
        DNSServiceErrorType exitVal = DNSServiceProcessResult(zcBrowseRef);

        if ( exitVal != kDNSServiceErr_NoError ) {
            rs_log_error("Failure processing zeroconfiguration browse messages: %d", exitVal);
        }

        return ( fdCount - 1 );
    } else {
        return fdCount;
    }
}


/**
 * Processes resolve messages if any of the resolve sockets is among those
 * ready to read (i.e. in <code>availableFDs</code>).  Returns either
 * <code>fdCount</code> or the remaining number of active file descriptors in
 * <code>availableFDs</code>.
 **/
int dcc_zc_process_resolve_messages(fd_set *availableFDs, int fdCount)
{
    size_t         capacity   = zcServices.capacity;
    size_t         i;
    int            newFDCount = fdCount;
    ServiceEntry **services   = zcServices.services;

    for ( i = 0; i < capacity && newFDCount > 0; i++ ) {
        ServiceEntry *service = services[i];

        if ( service != NULL ) {
            int resolveFD = service->resolveRefSockFD;

            if ( resolveFD > 0 && FD_ISSET(resolveFD, availableFDs) ) {
                DNSServiceErrorType exitVal = DNSServiceProcessResult(service->resolveRef);

                if ( exitVal != kDNSServiceErr_NoError ) {
                    rs_log_error("Failure processing zeroconfiguration resolve messages: %d", exitVal);
                }

                newFDCount--;
            }
        }
    }

    return newFDCount;
}


/**
 * Indicates whether client (distcc) requests for a canonical list of
 * zeroconfiguration-enabled compile servers (distccd) should be accepted.
 * Returns zero if a batch of zeroconfiguration changes is still being
 * processed.
 **/
int dcc_zc_should_process_client_requests(void)
{
    return ( ! zcMoreComing );
}


// Service list generation and access


/**
 * Returns the canonical list of zeroconfiguration-enabled compile servers
 * (distccd) or <code>"localhost"</code> if none are currently available.
 * Callers should not modify the returned string.
 **/
char *dcc_zc_resolved_services_list(void)
{
    if ( zcResolvedSvcs == NULL ) {
        return (char *) "localhost";
    } else {
        return zcResolvedSvcs;
    }
}


/**
 * Rebuilds the list returned by <code>dcc_zc_resolved_services_list</code>.
 * Only invoked when zeroconfiguration changes (either a service removed or an
 * added service resolved) are encountered.
 **/
static void dcc_zc_build_resolved_services_list(void)
{
    size_t         capacity   = zcServices.capacity;
    size_t         i;
    size_t         listLength = 0;
    char          *oldList    = zcResolvedSvcs;
    ServiceEntry **services   = zcServices.services;

    for ( i = 0; i < capacity; i++ ) {
        ServiceEntry *service = services[i];

        if ( service != NULL && service->description != NULL ) {
            listLength += strlen(service->description) + 1;
        }
    }

    if ( listLength > 0 ) {
        char *serviceList = malloc(listLength);

        if ( serviceList != NULL ) {
            for ( i = 0; i < capacity; i++ ) {
                ServiceEntry *service = services[i];

                if ( service != NULL && service->description != NULL ) {
                    strcat(serviceList, service->description);
                    strcat(serviceList, " ");
                }
            }

            serviceList[listLength-1] = '\0';

            zcResolvedSvcs = serviceList;

            rs_trace("Resolved distcc services are: %s", zcResolvedSvcs);
        }
    } else {
        zcResolvedSvcs = NULL;
    }

    if ( oldList != NULL ) {
        free(oldList);
    }
}


// Bonjour resolution


/**
 * Store resolution information to an existing service in the list.
 * Called by <code>dcc_zc_resolve_reply</code>.
 * We assume the following:
 * <ul>
 * <li>the service table has been basically initialized</li>
 * <li>mDNS service resolution will not give us duplicate information
 * (one resolution per service per interface per host)</li>
 * <li>we do not have to compare regTypes</li>
 * <li>no two entries will have the same triad of name, domain and
 *     interfaceIndex</li>
 * <li>all entries are NULL (unused or deleted) or valid</li>
 * <li>there will be no contention between adding, removing and resolving
 *     services</li>
 * </ul>
 **/
static void dcc_zc_service_resolved(const DNSServiceRef ref,
                                          uint32_t      anInterfaceIndex,
                                    const char         *aFullName,
                                    const char         *aHostTarget,
                                          uint16_t      aPort,
                                          uint16_t      aTxtLen,
                                    const char         *aTxtRecord)
{
    size_t         capacity    = zcServices.capacity;
    size_t         i;
    ServiceEntry  *service     = NULL;
    ServiceEntry **serviceList = zcServices.services;

    aPort = ntohs(aPort);

    // Find the matching entry.

    for ( i = 0; i < capacity; i++ ) {
        service = serviceList[i];

        if ( service != NULL && service->resolveRef == ref ) {
            break;
        }
    }

    if ( i == capacity ) {
        rs_log_error("Cannot find resolved service: \"%s\"", aFullName);
    } else {
        // Store values that the resolve provides.

        service->port   = aPort;
        service->txtLen = aTxtLen;

        // description is hostTarget:port
        service->description = malloc(strlen(aHostTarget) + 1 + 5 + 1);
        service->fullName    = malloc(strlen(aFullName)           + 1);
        service->hostTarget  = malloc(strlen(aHostTarget)         + 1);
        service->txtRecord   = malloc(strlen(aTxtRecord)          + 1);

        if ( service->description == NULL || service->fullName    == NULL ||
             service->hostTarget  == NULL || service->txtRecord   == NULL ) {
            rs_log_error("Unable to allocate space for data for resolved service \"%s\" on interface %d", aFullName, anInterfaceIndex);
            dcc_zc_free_service_entry(&serviceList[i]);
        } else {
            sprintf(service->description, "%s:%u", aHostTarget, aPort);
            strcpy(service->fullName,   aFullName);
            strcpy(service->hostTarget, aHostTarget);
            strcpy(service->txtRecord,  aTxtRecord);
        }
    }
}


/**  
 * Remove resolution information for an existing service in the list.
 * Called by <code>dcc_zc_resolve_reply</code>.
 * We assume the following:
 * <ul>
 * <li>the service table has been basically initialized</li>
 * <li>we do not have to compare regTypes</li>
 * <li>no two entries will have the same triad of name, domain and
 *     interfaceIndex</li>
 * <li>all entries are NULL (unused or deleted) or valid</li>
 * <li>there will be no contention between adding, removing and resolving
 *     services</li>
 * </ul>
 **/
static void dcc_zc_service_unresolved(const DNSServiceRef ref,
                                            uint32_t      anInterfaceIndex,
                                      const char         *aFullName,
                                      const char         * UNUSED(aHostTarget),
                                            uint16_t      UNUSED(aPort),
                                            uint16_t      UNUSED(aTxtLen),
                                      const char         * UNUSED(aTxtRecord))
{
    size_t         capacity    = zcServices.capacity;
    size_t         i;
    ServiceEntry **serviceList = zcServices.services;

    // Find the matching entry.

    for ( i = 0; i < capacity; i++ ) {
        ServiceEntry *service = serviceList[i];

        if ( service != NULL && service->resolveRef == ref ) {
            if ( service->resolveRef != NULL ) {
                dcc_zc_remove_from_select_set(service->resolveRefSockFD);
                DNSServiceRefDeallocate(service->resolveRef);
                service->resolveRef       = NULL;
                service->resolveRefSockFD = 0;
                rs_trace("Stopped resolving service \"%s\" on interface %d",
                         aFullName, anInterfaceIndex);
            }
            break;
        }
    }

    if ( i == capacity ) {
        rs_log_error("Cannot find service \"%s\" on interface %d to stop resolution", aFullName, anInterfaceIndex);
    }
}


/**
 * Handle resolution messages from the mDNSResponder for a particular service.
 * Invoke either <code>dcc_zc_service_resolved</code> or
 * <code>dcc_zc_service_unresolved</code> if a service is added or incompatible,
 * respectively.  Log on error.
 **/
static void dcc_zc_resolve_reply(const DNSServiceRef       ref,
                                       DNSServiceFlags     UNUSED(flags),
                                       uint32_t            anInterfaceIndex,
                                       DNSServiceErrorType errorCode,
                                 const char               *aFullName,
                                 const char               *aHostTarget,
                                       uint16_t            aPort,
                                       uint16_t            aTxtLen,
                                 const char               *aTxtRecord,
                                       void               *context)
{
    // context contains the client's TXT record
    int matchedProfile = ( strncmp((char *) context, aTxtRecord,
                           strlen((char *) context)) == 0 );

    if ( errorCode ) {
        rs_log_error("Encountered zeroconfiguration resolution error: %d",
                     errorCode);
    }

    if ( ! matchedProfile ) {
        rs_log_info("Distributed compile profile did not match for \"%s\"",
                    aFullName);
        dcc_zc_service_unresolved(ref, anInterfaceIndex, aFullName, aHostTarget,
                                  aPort, aTxtLen, aTxtRecord);
    } else {
        dcc_zc_service_resolved(ref, anInterfaceIndex, aFullName, aHostTarget,
                                aPort, aTxtLen, aTxtRecord);

        dcc_zc_build_resolved_services_list();
    }
}


// Bonjour browsing


/**
 * Increase the size of the service entry list by <code>ADD_SERVICE_COUNT</code>
 * (currently <code>16</code>).
 * Zero-fill the additional space.
 * Return <code>-1</code> if the size cannot be increased.
 * Note that the service entry list may be <code>realloc</code>ed, so be careful
 * if making local copies of the pointer to the service entry list.
 **/
static size_t dcc_zc_expand_service_table(void)
{
    size_t currentCapacity = zcServices.capacity;
    size_t j;
    size_t newCapacity     = currentCapacity + ADD_SERVICE_COUNT;

    rs_trace("Increasing service list capacity to %lu", newCapacity);

    zcServices.services = realloc(zcServices.services,
                                  newCapacity * SERVICE_ENTRY_PTR_SIZE);

    if ( zcServices.services == NULL ) {
        rs_log_error("Unable to increase the service list capacity");
        newCapacity = -1;
    } else {
        // Zero-fill the new space.

        for ( j = currentCapacity; j < newCapacity; j++ ) {
            (zcServices.services)[j] = NULL;
        }

        // Note the new capacity.
        zcServices.capacity = newCapacity;
    }

    return newCapacity;
}


/**  
 * Add a service to the list of known distcc daemons.
 * Attempt to resolve a service.
 * Called by <code>dcc_zc_browse_reply</code>.
 * We assume the following:
 * <ul>
 * <li>the service table has been basically initialized</li>
 * <li>mDNS service browsing will not give us duplicate information</li>
 * <li>we do not have to compare regTypes</li>
 * <li>no two entries will have the same triad of name, domain and
 *     interfaceIndex</li>
 * <li>all entries are NULL (unused or deleted) or valid</li>
 * <li>there will be no contention between adding, removing and resolving
 *     services</li>
 * </ul>
 **/
static void dcc_zc_add_service(const char *aName, const char *aRegType,
                               const char *aDomain,
                               const uint32_t anInterfaceIndex,
                               void *clientTxtRecord)
{
    size_t         capacity    = zcServices.capacity;
    char          *fullName    = dcc_zc_full_name(aName, aRegType, aDomain);
    size_t         i;
    ServiceEntry **serviceList = zcServices.services;

    // See if the service has already been added (possible for a multihomed
    // servicei).  If so, don't clobber any existing data; just increment the
    // ref count, and keep track of the interfaceIndex.

    for ( i = 0; i < capacity; i++ ) {
        ServiceEntry *service = serviceList[i];

        if ( service != NULL && strcmp(service->name, aName) == 0 ) {
            rs_log_info("Duplicate registration of service \"%s\" on interface %d", fullName, anInterfaceIndex);

            if ( service->refCount == MAX_INTERFACES - 1 ) {
                rs_log_info("Too many duplicates to record; ignoring");
            } else {
                int refCount;

                rs_log_info("Recorded duplicate");
                refCount = service->refCount++;
                (service->interfaceIndex)[refCount] = anInterfaceIndex;
            }

            free(fullName);
            return;
        }
    }

    // Find the first available slot.
    // NULL services indicate deleted or empty entries in the table.

    for ( i = 0; i < capacity; i++ ) {
        ServiceEntry *service = serviceList[i];

        if ( service == NULL ) {
            break;
        }
    }

    if ( i == capacity ) {
        capacity = dcc_zc_expand_service_table();

        // zcServices.services may have been realloc'ed somewhere else.
        serviceList = zcServices.services;
    }

    // Add the new service to an empty space (at index i).

    if ( i < capacity ) {
        ServiceEntry *newService = malloc(SERVICE_ENTRY_SIZE);

        if ( newService == NULL ) {
            rs_log_error("Unable to allocate space for new service \"%s\" on interface %d", fullName, anInterfaceIndex);
        } else {
            DNSServiceErrorType exitValue;
            int                 failedToInitialize = FALSE;

            // Initialize values that the resolve will provide.

            newService->description = NULL;
            newService->fullName    = NULL;
            newService->hostTarget  = NULL;
            newService->port        = 0;
            newService->txtLen      = 0;
            newService->txtRecord   = NULL;

            // Store the values known before the resolve.

            newService->refCount            = 0;
            (newService->interfaceIndex)[0] = anInterfaceIndex;
            newService->name                = malloc(strlen(aName)    + 1);
            newService->regType             = malloc(strlen(aRegType) + 1);
            newService->domain              = malloc(strlen(aDomain)  + 1);

            // Kick off the resolve.

            exitValue = DNSServiceResolve(&(newService->resolveRef),
                                          ZC_RESOLVE_FLAGS, anInterfaceIndex,
                                          aName, aRegType, aDomain,
                  (DNSServiceResolveReply)dcc_zc_resolve_reply,
                                          clientTxtRecord);

            if ( exitValue != kDNSServiceErr_NoError ||
                 newService->resolveRef == NULL ) {
                rs_log_error("Failed to resolve new service...");
                failedToInitialize = TRUE;
            } else {
                newService->resolveRefSockFD = DNSServiceRefSockFD(newService->resolveRef);

                if ( newService->resolveRefSockFD < 0 ||
                     newService->resolveRefSockFD > FD_SETSIZE ) {
                    rs_log_error("Failed to determine resolve socket...");
                    failedToInitialize = TRUE;
                } else {
                    dcc_zc_add_to_select_set(newService->resolveRefSockFD);
                }
            }

            if ( newService->name == NULL || newService->regType == NULL ||
                 newService->domain == NULL ) {
                rs_log_error("Unable to allocate space for data for new service...");
            } else {
                strcpy(newService->name,    aName);
                strcpy(newService->regType, aRegType);
                strcpy(newService->domain,  aDomain);
            }

            if ( failedToInitialize ) {
                rs_log_error("...unable to add service \"%s\" on interface %d",
                             fullName, anInterfaceIndex);
                dcc_zc_free_service_entry(&newService);
            }
        }

        if ( newService != NULL ) {
            serviceList[i] = newService;
            rs_log_info("Added service \"%s\" on interface %d", fullName,
                        anInterfaceIndex);
        }
    } else {
        rs_log_error("Unable to add service \"%s\" on interface %d; cannot resize list", fullName, anInterfaceIndex);
    }

    free(fullName);
}


/**
 * Remove a service from the list of known distcc daemons.
 * Terminate any existing resolve attempt.
 * Called by <code>dcc_zc_browse_reply</code>.
 * We assume the following:
 * <ul>
 * <li>the service table has been basically initialized</li>
 * <li>mDNS service browsing will not remove unadded services</li>
 * <li>we do not have to compare regTypes</li>
 * <li>no two entries will have the same triad of name, domain and
 *     interfaceIndex</li>
 * <li>all entries are NULL (unused or deleted) or valid</li>
 * <li>there will be no contention between adding, removing and resolving
 *     services</li>
 * </ul>
 **/
static void dcc_zc_remove_service(const char *aName, const char *aRegType,
                                  const char *aDomain,
                                  const uint32_t anInterfaceIndex,
                                  void *clientTxtRecord)
{
    size_t         capacity    = zcServices.capacity;
    char          *fullName    = dcc_zc_full_name(aName, aRegType, aDomain);
    size_t         i;
    ServiceEntry **serviceList = zcServices.services;

    // Find the service specified by the parameters, and remove it.
    // NULL services indicate deleted entries in the table.

    for ( i = 0; i < capacity; i++ ) {
        ServiceEntry *service = serviceList[i];

        if ( service != NULL && strcmp(aName, service->name) == 0 ) {
            if ( service->refCount > 0 ) {
                uint32_t *interfaceIndex = service->interfaceIndex;
                int       k;
                int       tmpRefCount    = service->refCount;

                for ( k = 0; k <= tmpRefCount; k++ ) {
                     if ( interfaceIndex[k] == anInterfaceIndex ) {
                         break;
                     }
                }

                if ( k > tmpRefCount ) {
                    rs_log_error("Cannot remove duplicate service \"%s\"; interface %d unregistered", fullName, anInterfaceIndex);
                } else {
                    if ( k < tmpRefCount ) {
                        int j;

                        for ( j = k + 1; j <= tmpRefCount; j++ ) {
                            interfaceIndex[j-1] = interfaceIndex[j];
                        }
                    }

                    // note: tmpRefCount invalid after this
                    service->refCount--;

                    // The first service is always resolved.
                    // May need to resolve again if we lose the first service.
                    if ( k == 0 ) {
                        DNSServiceErrorType exitValue;
                        int                 failedToResolve = FALSE;

                        DNSServiceRefDeallocate(service->resolveRef);

                        exitValue = DNSServiceResolve(&(service->resolveRef),
                                                      ZC_RESOLVE_FLAGS,
                                                      interfaceIndex[0],
                                                      aName, aRegType, aDomain,
                              (DNSServiceResolveReply)dcc_zc_resolve_reply,
                                                      clientTxtRecord);

                        if ( exitValue != kDNSServiceErr_NoError ||
                             service->resolveRef == NULL ) {
                            rs_log_error("Failed to resolve...");
                            failedToResolve = TRUE;
                        } else {
                            service->resolveRefSockFD = DNSServiceRefSockFD(service->resolveRef);
                            if ( service->resolveRefSockFD < 0 ||
                                 service->resolveRefSockFD > FD_SETSIZE ) {
                                rs_log_error("Failed to determine resolve socket...");
                                failedToResolve = TRUE;
                            }
                        }

                        if ( failedToResolve ) {
                            dcc_zc_free_service_entry(&(serviceList[i]));
                            rs_log_error("...removed service \"%s\" entirely.",
                                         fullName);
                        }
                    }

                    rs_log_info("Removed duplicate service \"%s\" on interface %d", fullName, anInterfaceIndex);
                }
            } else {
                dcc_zc_free_service_entry(&(serviceList[i]));
                rs_log_info("Removed service \"%s\" on interface %d", fullName,
                            anInterfaceIndex);
            }

            break;
        }
    }

    if ( i == capacity ) {
        rs_log_error("Cannot remove \"%s\" for interface %d; service not found",
                     fullName, anInterfaceIndex);
    }

    free(fullName);
}


/**
 * Cleanup after error during or completion of browsing for zeroconfiguration.
 **/
static void dcc_zc_browse_cleanup(void)
{
    size_t i;

    if ( zcBrowseRef != NULL ) {
        DNSServiceRefDeallocate(zcBrowseRef);
        zcBrowseRef = NULL;
    }

    if ( zcServices.services != NULL ) {
        for ( i = 0; i < zcServices.capacity; i++ ) {
            if ( zcServices.services[i] != NULL ) {
                dcc_zc_free_service_entry(&(zcServices.services[i]));
            }
        }

        free(zcServices.services);
        zcServices.services = NULL;
        zcServices.capacity = 0;
    }

    zcSelectFDCount = 0;
    FD_ZERO(&zcSelectFDs);
}


/**
 * Handle browse messages from the mDNSResponder.
 * Invoke either <code>dcc_zc_add_service</code> or
 * <code>dcc_zc_remove_service</code> if a service is added or removed,
 * respectively.  Log on error.
 **/
static void dcc_zc_browse_reply(const DNSServiceRef       UNUSED(ref),
                                const DNSServiceFlags     flags,
                                const uint32_t            anInterfaceIndex,
                                const DNSServiceErrorType errorCode,
                                const char               *aName,
                                const char               *aRegType,
                                const char               *aDomain,  
                                      void               *context)
{
    if ( errorCode ) {
        rs_log_error("Encountered zeroconfiguration browse error: %d",
                     errorCode);
    }

    if ( flags & kDNSServiceFlagsMoreComing ) {
        zcMoreComing = TRUE;
    } else {
        zcMoreComing = FALSE;
    }

    if ( flags & kDNSServiceFlagsAdd ) {
        dcc_zc_add_service(aName, aRegType, aDomain, anInterfaceIndex,
                           context);
    } else {
        dcc_zc_remove_service(aName, aRegType, aDomain, anInterfaceIndex,
                              context);

        dcc_zc_build_resolved_services_list();
    }
}


/**
 * Collect distcc compile server (distccd)registrations from mDNS.
 * Initializes the service entry list for registrations.
 * Maintains a file descriptor set that contains file descriptors for the
 * results of browse and resolve operations.
 * <code>txtRecord</code> is used to determine whether remote compile servers
 * are compatible with the local machine; remote servers that are not 100%
 * compatible are ignored by the list daemon (distccschedd).
 * Compatibility is determined by OS version and gcc 3.3 version (necessary
 * for remote use of files that are locally preprocessed using the PCH header).
 **/
void dcc_browse_for_zeroconfig(char *txtRecord)
{
    int failedToInitialize = FALSE;

    // Initialize the file descriptor set for the select() loop.

    FD_ZERO(&zcSelectFDs);

    // Initialize service entry table.

    zcServices.capacity = ADD_SERVICE_COUNT;
    zcServices.services = calloc(SERVICE_ENTRY_PTR_SIZE, ADD_SERVICE_COUNT);

    if ( zcServices.services == NULL ) {
        rs_log_error("Unable to allocate memory for service list");
        failedToInitialize = TRUE;
    } else {
        DNSServiceErrorType exitValue = DNSServiceBrowse(&zcBrowseRef,
                                                         ZC_BROWSE_FLAGS,
                                                         ZC_ALL_INTERFACES,
                                                         ZC_REG_TYPE,
                                                         ZC_DOMAIN,
                                  (DNSServiceBrowseReply)dcc_zc_browse_reply,
                                                 (void *)txtRecord);

        if ( exitValue == kDNSServiceErr_NoError && zcBrowseRef != NULL ) {
            zcBrowseFD = DNSServiceRefSockFD(zcBrowseRef);

            if ( zcBrowseFD < 0 || zcBrowseFD > FD_SETSIZE ) {
                rs_log_error("Failed to determine browse socket");
                failedToInitialize = TRUE;
            } else {
                dcc_zc_add_to_select_set(zcBrowseFD);
            }
        } else {
            rs_log_error("Failed to browse for registered distcc daemons");
            failedToInitialize = TRUE;
        }
    }

    if ( txtRecord == NULL ) {
        rs_log_error("Unable to generate distributed build profile");
        failedToInitialize = TRUE;
    }

    if ( failedToInitialize ) {
        rs_fatal("Failed to initialize properly... terminating!");
        dcc_zc_browse_cleanup();
        exit(EXIT_DISTCC_FAILED);
    }
}


#endif // DARWIN
