/*
 * Copyright (c) 2026  Apple Inc. All rights reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <Network/Network.h>
#include <Network/Network_Private.h>
#include <os/log.h>

#include <platform_single_signon.h>

/* SRV Lookup keys */
#define kSRVServerHost CFSTR("SRVServerHost")
#define kSRVServerPort CFSTR("SRVServerPort")
#define kSRVWeight CFSTR("SRVWeight")
#define kSRVPriority CFSTR("SRVPriority")
#define kSRVIPAddresses CFSTR("SRVIPAddresses")


/* Prototypes */
CFArrayRef smb_od_dns_srv_lookup(CFStringRef srv_query, CFTimeInterval timeout);
CFStringRef smb_CFStringCreateUpperLowerCase(CFStringRef originalString, int wantLowerCase);
static CFArrayRef create_array_from_set(CFSetRef set);
CFArrayRef smb_od_dns_srv_lookup_parallel_hostnames(CFArrayRef queries,
                                                    CFIndex maxConcurrency,
                                                    CFTimeInterval timeout);


/*
 * smb_od_dns_srv_lookup() is a copy of od_dns_srv_lookup() from opendirectoryd, odcore.c
 * Slightly modified to take in a CFStringRef srv_query
 * Note that since it uses nw_resolver_create_srv_weighted_variant(), the list is already sorted by SRV priority
 * and weight.
 */
CFArrayRef smb_od_dns_srv_lookup(CFStringRef srv_query, CFTimeInterval timeout)
{
    if (srv_query == NULL) {
        os_log_error(OS_LOG_DEFAULT, "%s: srv_query is null", __FUNCTION__);
        return NULL;
    }

    /* Convert CFStringRef to C string */
    CFIndex max_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(srv_query), kCFStringEncodingUTF8) + 1;
    char *query = malloc(max_size);
    if (!query) {
        os_log_error(OS_LOG_DEFAULT, "%s: malloc failed for srv_query", __FUNCTION__);
        return NULL;
    }
    
    if (!CFStringGetCString(srv_query, query, max_size, kCFStringEncodingUTF8)) {
        os_log_error(OS_LOG_DEFAULT, "%s: CFStringGetCString failed for srv_query", __FUNCTION__);
        free(query);
        return NULL;
    }

    __block CFMutableArrayRef result = NULL;
    __block nw_array_t sortedEndpoints = NULL;

    dispatch_queue_t resolverQueue = dispatch_queue_create("com.apple.smb.srvLookup", DISPATCH_QUEUE_SERIAL);
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    nw_parameters_t params = NULL;
    nw_endpoint_t srvEndpoint = NULL;
    nw_path_evaluator_t evaluator = NULL;
    nw_path_t path = NULL;
    nw_resolver_t resolver = NULL;

    params = nw_parameters_create();
    if (params == NULL) {
        os_log_error(OS_LOG_DEFAULT, "%s: SRV lookup for %s failed: failed to create network params", __FUNCTION__, query);
        goto done;
    }

    srvEndpoint = nw_endpoint_create_srv(query);
    if (srvEndpoint == NULL) {
        os_log_error(OS_LOG_DEFAULT, "%s: SRV lookup for %s failed: failed to create network endpoint", __FUNCTION__, query);
        goto done;
    }

    evaluator = nw_path_create_evaluator_for_endpoint(srvEndpoint, params);
    if (evaluator == NULL) {
        os_log_error(OS_LOG_DEFAULT, "%s: SRV lookup for %s failed: failed to create network evaluator", __FUNCTION__, query);
        goto done;
    }

    path = nw_path_evaluator_copy_path(evaluator);
    if (path == NULL) {
        os_log_error(OS_LOG_DEFAULT, "%s: SRV lookup for %s failed: no network path", __FUNCTION__, query);
        goto done;
    }

    if (nw_path_get_status(path) != nw_path_status_satisfied) {
        os_log_error(OS_LOG_DEFAULT, "%s: SRV lookup for %s failed: network path not satisfied", __FUNCTION__, query);
        goto done;
    }

    resolver = nw_resolver_create_with_path(path);
    if (resolver == NULL) {
        os_log_error(OS_LOG_DEFAULT, "%s: SRV lookup for %s failed: failed to create network resolver", __FUNCTION__, query);
        goto done;
    }

    // The SRV lookup flow is a bit tricky in order to prevent a use-after-free.
    // The resolver is asynchronous but this function is synchronous which makes
    // a use-after-free possible if the resolver's update handler was called while
    // this function is cleaning up or even after the results have been returned
    // to the caller.  In order to prevent a use-after-free, the resolver must be
    // completely done before proceeding with any other work.
    //
    // This is how the SRV lookup is structured:
    //   1. The resolver performs the SRV lookup asynchronously.
    //   2. A semaphore waits, with a timeout, for the SRV lookup results.
    //   3. The only safe way to proceed past the semaphore is when the resolver
    //      has been canceled.  That means the only safe place to signal the semaphore
    //      is from the resolver's cancel handler.  There are two ways this can happen:
    //      A) When the resolution completes, the resolver's update handler cancels
    //         the resolver.  The resolver's cancel handler signals the semaphore.
    //      OR
    //      B) When the semaphore times out, the resolver is canceled and another
    //         semaphore wait (no timeout) is issued.  The resolver's cancel handler
    //         signals the semaphore.
    //   4. The SRV results, if any, are converted to the format the caller expects.
    //   5. Memory is freed and results are returned to the caller.

    nw_resolver_set_cancel_handler(resolver, ^(void) {
        // The resolver is completely finished when this block is run, i.e. the
        // resolver update handler won't be called again.  This block is triggered
        // by either the resolver's update handler or by timeout.  In either case,
        // when this block is run it's guaranteed the resolver is done and therefore
        // safe to signal the semaphore.  This is the only place where the semaphore
        // is signaled.
        //
        // This block is run on the same queue that was passed into the update
        // handler.  Since the queue (resolverQueue) is serial this will be the
        // last block run on queue.
        dispatch_semaphore_signal(sema);
    });

    // Setting the update handler starts the resolution.
    nw_resolver_set_update_handler(resolver, resolverQueue, ^(nw_resolver_status_t status, nw_array_t  _Nullable resolvedEndpoints) {

        switch (status) {
            case nw_resolver_status_complete:
                // The lookup is complete.  Sort any resolved endpoints (i.e. SRV
                // records) by weight and priority.
                if (resolvedEndpoints) {
                    if (__builtin_available(macOS 12.4, *)) {
                        sortedEndpoints = nw_resolver_create_srv_weighted_variant(resolvedEndpoints);
                    }
                    else {
                        // Fallback on earlier versions
                        os_log_error(OS_LOG_DEFAULT, "%s: nw_resolver_create_srv_weighted_variant not supported on this OS version",
                                     __FUNCTION__);
                        break;

                    }
                }

                // Cancel the resolver to signal that the SRV lookup is copmlete.
                nw_resolver_cancel(resolver);
                break;

            case nw_resolver_status_invalid:
                // The lookup failed.  Log the error and cancel the resolver to
                // signal the SRV lookup is done.
                os_log_error(OS_LOG_DEFAULT, "%s: SRV lookup for %s failed: network resolver invalid", __FUNCTION__, query);
                nw_resolver_cancel(resolver);
                break;

            case nw_resolver_status_in_progress:
                // Nothing to do.
                break;
        }
    });

    // Wait up to timeout seconds for the SRV query results to come back.  If the results come
    // back within the time limit, the resolver's cancel handler will signal the
    // semaphore.  If the semaphore times out, cancel the resolver and wait for
    // the resolver's cancel handler to signal the semaphore.
    if (dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, timeout * NSEC_PER_SEC)) != 0) {
        // Cancel the resolver.  The cancel handler will be run on the same block
        // that was passed into the update handler.
        nw_resolver_cancel(resolver);

        os_log_error(OS_LOG_DEFAULT, "%s: SRV lookup for %s failed: timed out waiting for results", __FUNCTION__, query);

        // Wait for the resolver's cancel handler to run and signal the semaphore.
        // Once that happens it's safe to cleanup.
        dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
        goto done;
    }

    // The SRV lookup completed.  The resolver's cancel handler has been run.
    // It's safe to examine the data returned by the resolver, if any, and to
    // clean up.

    if (sortedEndpoints == NULL) {
        os_log_error(OS_LOG_DEFAULT, "%s: SRV lookup for %s returned no results", __FUNCTION__, query);
        goto done;
    }

    // Extract the needed data from the sorted endpoints into the array of
    // dictionaries expected by the callers.
    result = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    nw_array_apply(sortedEndpoints, ^bool(__unused size_t index, nw_object_t object) {
        nw_endpoint_t endpoint = (nw_endpoint_t)object;

        CFStringRef cfHostname = NULL;
        CFNumberRef cfPort = NULL;
        CFNumberRef cfWeight = NULL;
        CFNumberRef cfPriority = NULL;

        const char *hostname = nw_endpoint_get_hostname(endpoint);
        if (hostname) {
            cfHostname = CFStringCreateWithCString(kCFAllocatorDefault, hostname, kCFStringEncodingUTF8);

            // Remove the trailing ".", if present
            if (cfHostname != NULL && CFStringHasSuffix(cfHostname, CFSTR("."))) {
                CFStringRef temp = CFStringCreateWithSubstring(kCFAllocatorDefault, cfHostname, CFRangeMake(0,CFStringGetLength(cfHostname)-1));
                CFRelease(cfHostname);
                cfHostname = temp;
            }
        }

        uint16_t port = nw_endpoint_get_port(endpoint);
        cfPort = CFNumberCreate(kCFAllocatorDefault, kCFNumberShortType, &port);

        if (__builtin_available(macOS 12.4, *)) {
            uint16_t weight = nw_endpoint_get_weight(endpoint);
            cfWeight = CFNumberCreate(kCFAllocatorDefault, kCFNumberShortType, &weight);
        } else {
            // Fallback on earlier versions
            os_log_error(OS_LOG_DEFAULT, "%s: nw_endpoint_get_weight not supported on this OS version",
                         __FUNCTION__);
        }

        if (__builtin_available(macOS 12.4, *)) {
            uint16_t priority = nw_endpoint_get_priority(endpoint);
            cfPriority = CFNumberCreate(kCFAllocatorDefault, kCFNumberShortType, &priority);
        } else {
            // Fallback on earlier versions
            os_log_error(OS_LOG_DEFAULT, "%s: nw_endpoint_get_priority not supported on this OS version",
                         __FUNCTION__);
        }

        if (cfHostname != NULL && cfPort != NULL && cfWeight != NULL && cfPriority != NULL) {
            // Create a mutable dictionary with the lookup data and add it to the array.
            // The dictionary is mutable because the callers of this function may add
            // their own data to the dictionary.
            /* os_log(OS_LOG_DEFAULT, "SRV lookup for %s found %@, port %@, weight %@, priority %@", query, cfHostname, cfPort, cfWeight, cfPriority);*/
            CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            if (dict) {
                CFDictionarySetValue(dict, kSRVServerHost, cfHostname);
                CFDictionarySetValue(dict, kSRVServerPort, cfPort);
                CFDictionarySetValue(dict, kSRVWeight, cfWeight);
                CFDictionarySetValue(dict, kSRVPriority, cfPriority);
                
                /* Add dict to results array */
                CFArrayAppendValue(result, dict);
                CFRelease(dict);
            }
        }

        if (cfHostname != NULL) {
            CFRelease(cfHostname);
        }
        if (cfPort != NULL) {
            CFRelease(cfPort);
        }
        if (cfWeight != NULL) {
            CFRelease(cfWeight);
        }
        if (cfPriority != NULL) {
            CFRelease(cfPriority);
        }

        return true;
    });

done:
    if (sortedEndpoints != NULL) {
        nw_release(sortedEndpoints);
    }
    if (resolver != NULL) {
        nw_release(resolver);
    }
    if (path != NULL) {
        nw_release(path);
    }
    if (evaluator != NULL) {
        nw_release(evaluator);
    }
    if (srvEndpoint != NULL) {
        nw_release(srvEndpoint);
    }
    if (params != NULL) {
        nw_release(params);
    }
    if (query != NULL) {
        free(query);
    }
    
    dispatch_release(resolverQueue);
    dispatch_release(sema);

    return result;
}

/* Function to convert CFStringRef to all lowercase or uppercase */
CFStringRef smb_CFStringCreateUpperLowerCase(CFStringRef originalString, int wantLowerCase)
{
    if (originalString == NULL) {
        os_log_error(OS_LOG_DEFAULT, "%s: originalString is null", __FUNCTION__);
        return NULL;
    }
    
    /* Create a mutable copy of the original string */
    CFMutableStringRef mutableString = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, originalString);
    if (mutableString == NULL) {
        os_log_error(OS_LOG_DEFAULT, "%s: CFStringCreateMutableCopy failed", __FUNCTION__);
        return NULL;
    }
    
    if (wantLowerCase == 1) {
        /* Convert to lowercase (pass NULL for current system locale) */
        CFStringLowercase(mutableString, NULL);
    }
    else {
        /* Convert to uppercase (pass NULL for current system locale) */
        CFStringUppercase(mutableString, NULL);
    }
    
    /* Return as an immutable string (caller responsible for releasing) */
    return (CFStringRef)mutableString;
}

/* Helper function to convert CFSetRef to CFArrayRef */
CFArrayRef create_array_from_set(CFSetRef set) {
    CFIndex count = CFSetGetCount(set);
    if (count == 0) {
        return CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks);
    }
    
    // Get values from set
    const void **values = malloc(sizeof(void*) * count);
    if (!values) {
        return CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks);
    }
    
    CFSetGetValues(set, values);
    
    // Create and sort array
    CFMutableArrayRef array = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);

    // Add each string to the mutable array
    for (CFIndex i = 0; i < count; i++) {
        CFArrayAppendValue(array, values[i]);
    }
    
#if 0
    /* If we ever want to sort the array, then here is where it would be done */
    CFArraySortValues(array, CFRangeMake(0, count),
                      smb_string_comparator,
                      (void*)kCFCompareCaseInsensitive);
#endif
    
    // Create immutable copy
    CFArrayRef result = CFArrayCreateCopy(kCFAllocatorDefault, array);

    if (array != NULL) {
        CFRelease(array);
    }
    if (values != NULL) {
        free(values);
    }
    
    return result;
}

CFArrayRef smb_od_dns_srv_lookup_parallel_hostnames(CFArrayRef queries,
                                                    CFIndex maxConcurrency,
                                                    CFTimeInterval timeout) {
    CFIndex queryCount = queries ? CFArrayGetCount(queries) : 0;
    if (queryCount == 0) {
        return CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks);
    }
    
    // Use a mutable array to collect results instead of custom struct
    CFMutableArrayRef allResults = CFArrayCreateMutable(kCFAllocatorDefault, queryCount, &kCFTypeArrayCallBacks);
    CFMutableSetRef uniqueHostnames = CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks);
    
    dispatch_group_t group = dispatch_group_create();
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_queue_t serialQueue = dispatch_queue_create("results", DISPATCH_QUEUE_SERIAL);
    dispatch_semaphore_t semaphore = maxConcurrency > 0 ? dispatch_semaphore_create(maxConcurrency) : NULL;
    
    // Process each query
    for (CFIndex i = 0; i < queryCount; i++) {
        /*
         * Retain query before the block captures it. In a C file, blocks do not
         * automatically retain CFTypeRef objects — they copy the raw pointer only.
         * Without this retain, the string can be freed by the caller before the
         * block executes (e.g. if dispatch_group_wait times out), causing a
         * use-after-free crash (EXC_ARM_PAC_FAIL in _CFTypeGetClass).
         */
        CFStringRef query = (CFStringRef)CFRetain(CFArrayGetValueAtIndex(queries, i));

        if (semaphore) {
            dispatch_retain(semaphore);
            dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
        }

        dispatch_group_async(group, queue, ^{
            CFArrayRef result = smb_od_dns_srv_lookup(query, timeout);

            if (result) {
                // Extract hostnames and add to set
                dispatch_sync(serialQueue, ^{
                    CFArrayAppendValue(allResults, result);

                    CFIndex recordCount = CFArrayGetCount(result);
                    for (CFIndex j = 0; j < recordCount; j++) {
                        CFDictionaryRef record = CFArrayGetValueAtIndex(result, j);
                        CFStringRef hostname = CFDictionaryGetValue(record, kSRVServerHost);
                        if (hostname) {
                            CFSetAddValue(uniqueHostnames, hostname);
                        }
                    }
                });
                /* allResults now holds its own retain on result; release ours */
                CFRelease(result);
            }

            /* Release the retain taken above before dispatching this block */
            CFRelease(query);

            if (semaphore) {
                dispatch_semaphore_signal(semaphore);
                dispatch_release(semaphore);
            }
        });
    }

    /*
     * Wait indefinitely for all blocks to complete. Each smb_od_dns_srv_lookup()
     * call has its own internal 5-second timeout and always completes, so this
     * wait will not hang. A finite timeout here is unsafe: if it expires while
     * blocks are still queued (e.g. during heavy system load at boot), the caller
     * frees the query strings and the still-pending blocks crash with
     * EXC_ARM_PAC_FAIL when they access the freed CFStringRef.
     */
    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
    
    CFArrayRef result = create_array_from_set(uniqueHostnames);

    /*
     * allResults contains the complete results back from smb_od_dns_srv_lookup and includes
     * host, port, priority and weight. If we ever want to sort in a different way, we have
     * the complete results.
     */
    if (allResults != NULL) {
        CFRelease(allResults);
    }
    if (uniqueHostnames != NULL) {
        CFRelease(uniqueHostnames);
    }
    dispatch_release(serialQueue);
    dispatch_release(group);
    if (semaphore) {
        dispatch_release(semaphore);
    }
    
    return result;
}

/*
 * For the case where the client is not bound to Active Directory (AD), it could a Platform Single Sign On (PSSO) setup
 * The server name could be an AD Domain name, in that case we really need to connect to one of the domain controllers.
 * Since we are not bound to AD, we have to use DNS/SRV lookups to return a list of domain controllers for that domain.
 * If this name is not part of the AD domain, then return NULL, else return a CFArray of domain controllers as CFStrings
 */
CFArrayRef smb_PSSO_domain(CFStringRef orig_server_name, uint32_t srv_lookup_timeout)
{
    CFArrayRef DCArrayRef = NULL;
    CFStringRef lowerCaseServerName = smb_CFStringCreateUpperLowerCase(orig_server_name, 1);
    CFMutableArrayRef srv_queries = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    CFStringRef DCSearchName = NULL;
    CFStringRef GCServiceName = NULL;
    CFStringRef local_suffix = CFSTR(".local");

    /*
     * Check to see if orig_server_name ends with ".local" and if it does, then skip doing the SRV searches.
     * smb_od_dns_srv_lookup() will automatically do mDNS searches if the SRV query has ".local" and we do not
     * want any local network resolutions.
     */
    if (CFStringHasSuffix(lowerCaseServerName, local_suffix)) {
        /* server_name ends with ".local", so skip on out */
        os_log_debug(OS_LOG_DEFAULT, "%s: server name ends with .local, so skipping SRV search ", __FUNCTION__);
        goto exit;
    }

    /*
     * To find domain controllers, we will try various SRV searches and if returns results, then we will use
     * that list of domain controllers. If all SRV searches fails, then we assume the server name is not a domain
     * name.
     * SRV search for "_ldap._tcp.<serverName>"
     * SRV search for "_gc._tcp.<serverName>"
     * If AD sites have been configured and we know the site names, then someday add:
     *      SRV search for "_ldap._tcp.ReadWrite._sites.dc._<siteName>.<serverName>"
     *      SRV search for "_ldap._tcp.ReadWrite._sites.gc._<siteName>.<serverName>"
     */

    /*
     * Do parallel search for Domain Controllers for that server name
     * Create SRV string by prepending "_ldap._tcp." to the all lowercase server name
     * Create SRV string by prepending "_gc._tcp." to the all lowercase server name
     */
    DCSearchName = CFStringCreateWithFormat(kCFAllocatorDefault,
                                            NULL,
                                            CFSTR("_ldap._tcp.%@"),
                                            lowerCaseServerName);
    if (DCSearchName == NULL) {
        os_log_error(OS_LOG_DEFAULT, "%s: CFStringCreateWithFormat failed for DCSearchName", __FUNCTION__);
        goto exit;
    }

    GCServiceName = CFStringCreateWithFormat(kCFAllocatorDefault,
                                             NULL,
                                             CFSTR("_gc._tcp.%@"),
                                             lowerCaseServerName);
    if (GCServiceName == NULL) {
        os_log_error(OS_LOG_DEFAULT, "%s: CFStringCreateWithFormat failed for GCServiceName", __FUNCTION__);
        goto exit;
    }

    CFArrayAppendValue(srv_queries, DCSearchName);
    CFArrayAppendValue(srv_queries, GCServiceName);

    /* Do the parallel SRV search with max conconcurrency of 5 */
    DCArrayRef = smb_od_dns_srv_lookup_parallel_hostnames(srv_queries, 5, srv_lookup_timeout);
    if (DCArrayRef == NULL) {
        os_log_error(OS_LOG_DEFAULT, "%s: smb_od_dns_srv_lookup_parallel() found no results for %@", __FUNCTION__, srv_queries);
        goto exit;
    }
    
    /* We found some domain controllers, so just return what we found */
    os_log(OS_LOG_DEFAULT, "%s: SRV lookup successful for %@: %@",
           __FUNCTION__, srv_queries, DCArrayRef);

exit:
    if (lowerCaseServerName != NULL) {
        CFRelease(lowerCaseServerName);
    }
    if (srv_queries != NULL) {
        CFRelease(srv_queries);
    }
    if (DCSearchName != NULL) {
        CFRelease(DCSearchName);
    }
    if (GCServiceName != NULL) {
        CFRelease(GCServiceName);
    }
    
    return DCArrayRef;
}

