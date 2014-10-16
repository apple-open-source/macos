/*
 * Copyright (c) 2012, 2013 Apple Computer, Inc. All rights reserved.
 */

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>      
#include <mach/boolean.h>

#include "scnc_main.h"
#include "scnc_utils.h"
#include "network_detection.h"
#include "sbslauncher.h"
#include "app_layer.h"

static boolean_t match_pattern(CFStringRef matchstr, CFStringRef matchpattern);
static boolean_t match_dns(CFArrayRef cur_dns_array, CFStringRef match_dns_name);
static boolean_t check_ssid(CFStringRef interface_name, CFArrayRef chk_ssid_array);
static boolean_t check_dns(CFArrayRef cur_dns_array, CFArrayRef chk_dns_array, int checkall);
static boolean_t check_domain(CFStringRef cur_domain, CFArrayRef chk_domain_array);
static boolean_t ondemand_add_action(struct service *serv, CFStringRef action, CFPropertyListRef actionParameters);
static void ondemand_clear_action(struct service *serv);
static void dodisconnect(struct service *serv);
static int start_https_probe(struct service *serv, CFStringRef https_probe_server, CFArrayRef matcharray, CFIndex matcharray_index,
                      CFDictionaryRef primary_dns_dict, CFStringRef primary_interface_name, CFStringRef primary_interface_type);
static void vpn_action( struct service *serv, CFStringRef match_action, CFPropertyListRef actionParameters);
static boolean_t resume_check_network(struct service *serv, CFArrayRef match_array, CFIndex match_array_offset, CFDictionaryRef primary_dns_dict, CFStringRef primary_dns_int, CFStringRef primary_interface_type);
#if TARGET_OS_EMBEDDED
static boolean_t on_cell_network();
#endif

extern TAILQ_HEAD(, service) 	service_head;

enum {
	READ	= 0,	// read end of standard UNIX pipe
	WRITE	= 1		// write end of standard UNIX pipe
};

struct probe {
	struct service *serv;
	CFStringRef url_string;
	CFArrayRef matcharray;
	CFIndex matcharray_index;
	CFDictionaryRef primary_dns_dict;
	CFStringRef primary_interface_name;
	CFStringRef primary_interface_type;
};

struct dns_redirect_context {
	int sbslauncher_sockets[2];
	struct service *serv;
};

static Boolean
service_is_valid (struct service *serv)
{
	Boolean found = FALSE;
	struct service *serv_check = NULL;
	
	// make sure service is still valid
	TAILQ_FOREACH(serv_check, &service_head, next) {
		if (serv_check == serv) {
			found = TRUE;
			break;
		}
	}
	
	return found;
}

#define MAX_SAVED_REDIRECTED_ADDRS 20

static void
dns_redirect_detection_setup_callback(pid_t pid, void *context)
{
	int *fd = (int*)context;
	int	on	= 1;

	fd = ((struct dns_redirect_context*)context)->sbslauncher_sockets;
	if (pid){
		my_close(fd[WRITE]);
		fd[WRITE] = -1;
	} else {
		my_close(fd[READ]);
		fd[READ] = -1;
		ioctl(fd[WRITE], FIONBIO, &on);
	}
}

static void
dns_redirect_detection_callback(pid_t pid, int status, struct rusage *rusage, void *ctx)
{
	struct dns_redirect_context *context = (struct dns_redirect_context*)ctx;
	struct service      *serv = context->serv;
	int exitcode;
	uint8_t buf[(sizeof(struct sockaddr_storage) * MAX_SAVED_REDIRECTED_ADDRS)];

	if (serv == NULL || !service_is_valid(serv)) {
		goto done;
	}

	exitcode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

	Boolean dnsRedirectDetected = (exitcode > 0);

	my_CFRelease(&serv->dnsRedirectedAddresses);

	if (dnsRedirectDetected) {
		struct sockaddr *addr = NULL;
		int i;
		int readlen = 0;
		int validatedBytes = 0;
		
		if ((readlen = read(context->sbslauncher_sockets[READ], buf, sizeof(buf))) > 0) {
			CFMutableDataRef ipv4Data = CFDataCreateMutable(kCFAllocatorDefault, 0);
			CFMutableDataRef ipv6Data = CFDataCreateMutable(kCFAllocatorDefault, 0);
			if (ipv4Data && ipv6Data) {			
				for (i = 0, addr = (void*)buf;
					 (i < MAX_SAVED_REDIRECTED_ADDRS) && (addr->sa_len <= (readlen - validatedBytes));
					 i++) {
					if (addr->sa_family == AF_INET) {
						struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
						if (addr_in->sin_len < sizeof(struct sockaddr_in)) {
							break;
						}
						CFDataAppendBytes(ipv4Data, (uint8_t*)&addr_in->sin_addr, sizeof(struct in_addr));
					} else if (addr->sa_family == AF_INET6) {
						struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;
						if (addr_in6->sin6_len < sizeof(struct sockaddr_in6)) {
							break;
						}
						CFDataAppendBytes(ipv6Data, (uint8_t*)&addr_in6->sin6_addr, sizeof(struct in6_addr));
					}
					
					validatedBytes += addr->sa_len;
					addr = (struct sockaddr *)(((uint8_t*)addr) + addr->sa_len);
				}
				
				CFMutableDictionaryRef addressesDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
				if (addressesDict) {
					if (CFDataGetLength(ipv4Data)) {
						CFMutableDictionaryRef ipv4Dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
						if (ipv4Dict) {
							CFDictionaryAddValue(ipv4Dict, kSCNetworkConnectionNetworkInfoAddresses, ipv4Data);
							CFDictionaryAddValue(addressesDict, kSCNetworkConnectionNetworkInfoIPv4, ipv4Dict);
							my_CFRelease(&ipv4Dict);
						}
					}
					if (CFDataGetLength(ipv6Data)) {
						CFMutableDictionaryRef ipv6Dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
						if (ipv6Dict) {
							CFDictionaryAddValue(ipv6Dict, kSCNetworkConnectionNetworkInfoAddresses, ipv6Data);
							CFDictionaryAddValue(addressesDict, kSCNetworkConnectionNetworkInfoIPv6, ipv6Dict);
							my_CFRelease(&ipv6Dict);
						}
					}
					serv->dnsRedirectedAddresses = addressesDict;
				}
			}
			my_CFRelease(&ipv4Data);
			my_CFRelease(&ipv6Data);
		}
	}

	SCLog(TRUE, LOG_DEBUG, CFSTR("dns_redirect_detection_callback %d %d %d"), status, exitcode, dnsRedirectDetected);
	if (serv->dnsRedirectDetected != dnsRedirectDetected) {
		serv->dnsRedirectDetected = dnsRedirectDetected;
		ondemand_add_service(serv, FALSE);
	}

done:
	my_close(context->sbslauncher_sockets[READ]);
	my_close(context->sbslauncher_sockets[WRITE]);
	free(context);
}

static Boolean
dns_redirect_detection_start(struct service *serv)
{
	struct dns_redirect_context *context;
	char socketstr[10];
	
	context = malloc(sizeof(struct dns_redirect_context));
	if (context == NULL)
		goto fail;
	
	if ((socketpair(AF_LOCAL, SOCK_STREAM, 0, context->sbslauncher_sockets) == -1)){
		SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: socketpair fails, errno = %s"), strerror(errno));
		goto fail;
	}
	snprintf(socketstr, sizeof(socketstr), "%d", context->sbslauncher_sockets[WRITE]);
	
	context->serv = serv;
	
	if (SCNCExecSBSLauncherCommandWithArguments(SBSLAUNCHER_TYPE_DETECT_DNS_REDIRECT, dns_redirect_detection_setup_callback, dns_redirect_detection_callback, (void*)context, socketstr, NULL)) {
		return TRUE;
	}
	
fail:
	if (context != NULL) {
		my_close(context->sbslauncher_sockets[READ]);
		my_close(context->sbslauncher_sockets[WRITE]);
		free(context);
	}
	return FALSE;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static boolean_t
match_pattern(CFStringRef matchstr, CFStringRef matchpattern)
{
	CFRange		range;
	Boolean		ret		= FALSE;
	CFStringRef	s1		= NULL;
	Boolean		s1_created	= FALSE;
    
    if (CFStringHasSuffix(matchpattern, CFSTR("*"))){
		range.location = 0;
		range.length = CFStringGetLength(matchpattern) - 1;
		s1 = CFStringCreateWithSubstring(NULL, matchpattern, range);
		if (s1 == NULL) {
			goto done;
		}
		s1_created = TRUE;
		ret = CFStringHasPrefix(matchstr, s1);
	} else {
		ret = CFEqual(matchstr, matchpattern);
	}
    
done:
	if (s1_created)	CFRelease(s1);
    return ret;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static boolean_t
match_dns(CFArrayRef cur_dns_array, CFStringRef match_dns_name)
{
    CFStringRef cur_dns = NULL;
    boolean_t   match = false;
    int         count = 0, i;
    
    count = CFArrayGetCount(cur_dns_array);
    for (i = 0; i < count; i++) {
        cur_dns = CFArrayGetValueAtIndex(cur_dns_array, i);
        
        if (match_pattern(match_dns_name, cur_dns)){
            match = true;
            break;
        }
    }
    return match;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static boolean_t
check_ssid(CFStringRef interface_name, CFArrayRef chk_ssid_array)
{
	CFDictionaryRef interface_dict = NULL;
	CFStringRef interface_key = NULL;
	CFStringRef match_ssid = NULL;
	boolean_t match = false;
	
	if (!isArray(chk_ssid_array) || !isString(interface_name)) {
		goto done;
	}
	
	interface_key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
															   kSCDynamicStoreDomainState,
															   interface_name,
															   kSCEntNetAirPort);
	
	interface_dict = SCDynamicStoreCopyValue(gDynamicStore, interface_key);
	
	if (!isDictionary(interface_dict)) {
		goto done;
	}
	
	match_ssid = CFDictionaryGetValue(interface_dict, SC_AIRPORT_SSID_STR_KEY);
	if (isString(match_ssid) && CFArrayContainsValue(chk_ssid_array, CFRangeMake(0, CFArrayGetCount(chk_ssid_array)), match_ssid)) {
		match = true;
	}
	
done:
	if (interface_key) {
		CFRelease(interface_key);
	}
	if (interface_dict) {
		CFRelease(interface_dict);
	}
	
	return match;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static boolean_t
check_dns(CFArrayRef cur_dns_array, CFArrayRef chk_dns_array, int checkall)
{
    CFStringRef match_dns_name = NULL;
    int count = 0, i;
    int localDNScount = 0;
    boolean_t   match = false;
    
    count = CFArrayGetCount(chk_dns_array);
    localDNScount = CFArrayGetCount(cur_dns_array);
    
    for (i = 0; i < localDNScount; i++) {
        match_dns_name = CFArrayGetValueAtIndex(cur_dns_array, i);
        if (!isString(match_dns_name))
            break;
        
        /* check domain name */
        if (match_dns(chk_dns_array, match_dns_name)){
            if (!checkall){
                /* we've found a match, get out */
                match = true;
                break;
            }
            continue;
        }else {
            break;
        }
    }
    if ( i >= localDNScount)
        match = true;

    return match;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */

static boolean_t
check_domain(CFStringRef cur_domain, CFArrayRef chk_domain_array)
{
    CFStringRef match_domain_name = NULL;
    int count = 0, i;
    boolean_t   match = false;
    
    count = CFArrayGetCount(chk_domain_array);
        
    for (i = 0; i < count; i++) {
        match_domain_name = CFArrayGetValueAtIndex(chk_domain_array, i);
        if (match_domain_name){
            if (_SC_domainEndsWithDomain(cur_domain, match_domain_name)){
                //   if (CFStringCompare(cur_domain, match_domain_name, 0) == kCFCompareEqualTo) {
                /* we've found a match, get out */
                match = true;
                break;
            }
        }
    }
    return match;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
int  copy_trigger_info(struct service *serv, CFMutableDictionaryRef *ondemand_dict_cp,
                       CFMutableArrayRef *trigger_array_cp, CFMutableDictionaryRef *trigger_dict_cp)
{
    CFStringRef			serviceid = NULL;
	CFDictionaryRef		ondemand_dict = NULL, current_trigger_dict = NULL;
	CFArrayRef			current_triggers_array = NULL;
	int					count = 0, i, found = 0;
    int                 ret = -1;

    *ondemand_dict_cp = NULL;
    *trigger_array_cp = NULL;
    *trigger_dict_cp = NULL;
    
	if (gOndemand_key == NULL)
		goto fail;
	
	ondemand_dict = SCDynamicStoreCopyValue(gDynamicStore, gOndemand_key);
	if (ondemand_dict) {
		current_triggers_array = CFDictionaryGetValue(ondemand_dict, kSCNetworkConnectionOnDemandTriggers);
		if (isArray(current_triggers_array)) {
			found = 0;
			count = CFArrayGetCount(current_triggers_array);
			for (i = 0; i < count; i++) {
				current_trigger_dict = CFArrayGetValueAtIndex(current_triggers_array, i);
				if (!isDictionary(current_trigger_dict))
					continue;
				serviceid = CFDictionaryGetValue(current_trigger_dict, kSCNetworkConnectionOnDemandServiceID);
				if (!isString(serviceid))
					continue;
				if (CFStringCompare(serviceid, serv->serviceID, 0) == kCFCompareEqualTo) {
					found = 1;
                    ret = i;
					break;
				}
			}
		}
	}
    
    if (!found){
        goto fail;
    }

    /* make a copy of current_trigger_dict */
    *trigger_dict_cp = CFDictionaryCreateMutableCopy(0, 0, current_trigger_dict);
    if (*trigger_dict_cp == NULL)
        goto fail;
    
    *ondemand_dict_cp = CFDictionaryCreateMutableCopy(0, 0, ondemand_dict);
    if (*ondemand_dict_cp == NULL)
        goto fail;
    
    *trigger_array_cp = CFArrayCreateMutableCopy(0, 0, current_triggers_array);
    if (*trigger_array_cp == NULL)
        goto fail;
    
    goto done;

fail:
	my_CFRelease(trigger_dict_cp);
	my_CFRelease(ondemand_dict_cp);
	my_CFRelease(trigger_array_cp);

done:
	my_CFRelease(&ondemand_dict);

    return ret;
    
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static boolean_t
ondemand_add_action(struct service *serv, CFStringRef action, CFPropertyListRef actionParameters)
{
	boolean_t changed = !my_CFEqual(serv->ondemandAction, action);
	my_CFRelease(&serv->ondemandAction);
	my_CFRelease(&serv->ondemandActionParameters);
	serv->ondemandAction = my_CFRetain(action);
	if (isA_CFPropertyList(actionParameters)) {
		serv->ondemandActionParameters = my_CFRetain(actionParameters);
	}
	return changed;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static
void ondemand_clear_action(struct service *serv)
{
    my_CFRelease(&serv->ondemandAction);    
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static
void dodisconnect(struct service *serv)
{
	SCLog(TRUE, LOG_DEBUG, CFSTR("SCNC Controller: do disconnect by network detection"));

    /* reset ignore vpn flag */
    scnc_stop(serv, 0, SIGTERM, SCNC_STOP_USER_REQ);
}

static
void ondemand_save_probe_result(struct service *serv, CFStringRef URL, Boolean succeeded)
{
	if (serv->ondemandProbeResults == NULL) {
		serv->ondemandProbeResults = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	}
	
	CFMutableDictionaryRef results = (CFMutableDictionaryRef)serv->ondemandProbeResults;
	
	CFDictionarySetValue(results, URL, succeeded ? kCFBooleanTrue : kCFBooleanFalse);
}

static
Boolean ondemand_probe_already_sent(struct service *serv, CFStringRef URL)
{
	if (serv->ondemandProbeResults != NULL && URL != NULL) {
		return CFDictionaryContainsValue(serv->ondemandProbeResults, URL);
	}
	return FALSE;
}

static
void ondemand_clear_probe_results(struct service *serv)
{
	my_CFRelease(&serv->ondemandProbeResults);
}

static
Boolean doEvaluateConnection(struct service *serv, CFArrayRef domainRules)
{
	Boolean needServerExceptionDNSConfig = FALSE;
	CFMutableDictionaryRef DNSTriggeringDicts = NULL;
	CFIndex domainRulesCount = 0;
	CFIndex i;
	
	if (!isArray(domainRules)) {
		return FALSE;
	}
	
	domainRulesCount = CFArrayGetCount(domainRules);
	if (domainRulesCount == 0) {
		return FALSE;
	}
	
	DNSTriggeringDicts = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
	if (!DNSTriggeringDicts) {
		return FALSE;
	}
	
	for (i = 0; i < domainRulesCount; i++) {
		CFDictionaryRef domainRule = CFArrayGetValueAtIndex(domainRules, i);
		if (!isDictionary(domainRule)) {
			continue;
		}
		
		CFStringRef domainAction = CFDictionaryGetValue(domainRule, kSCPropNetVPNOnDemandRuleActionParametersDomainAction);
		if (!isString(domainAction)) {
			continue;
		}
		
		if (CFStringCompare(domainAction, kSCValNetVPNOnDemandRuleActionParametersDomainActionConnectIfNeeded, 0) == kCFCompareEqualTo) {
			CFArrayRef dnsServers = CFDictionaryGetValue(domainRule, kSCPropNetVPNOnDemandRuleActionParametersRequiredDNSServers);
			CFArrayRef domains = CFDictionaryGetValue(domainRule, kSCPropNetVPNOnDemandRuleActionParametersDomains);
			if (isArray(dnsServers) && CFArrayGetCount(dnsServers) && isArray(domains) && CFArrayGetCount(domains)) {
				CFDictionaryRef dnsDictionary = NULL;
				CFStringRef serviceExt = NULL;
				CFStringRef key = NULL;
				CFMutableArrayRef mutableDomains = NULL;
				CFIndex domainCount = CFArrayGetCount(domains);
				CFIndex domainIndex = 0;
				
				mutableDomains = CFArrayCreateMutableCopy(kCFAllocatorDefault, domainCount, domains);
				if (mutableDomains) {
					/* Sanitize domain array for *.example domains */
					for (domainIndex = 0; domainIndex < domainCount; domainIndex++) {
						CFStringRef domain = CFArrayGetValueAtIndex(mutableDomains, domainIndex);
						if (CFStringHasPrefix(domain, CFSTR("*."))) {
							CFStringRef newDomain = NULL;
							CFRange range;
							range.location = 2;
							range.length = CFStringGetLength(domain) - 2;
							if (range.length > 0) {
								newDomain = CFStringCreateWithSubstring(kCFAllocatorDefault, domain, range);
								if (newDomain) {
									CFArraySetValueAtIndex(mutableDomains, domainIndex, newDomain);
									CFRelease(newDomain);
								}
							}
						}
					}
					
					/* Create temporary service key */
					serviceExt = CFStringCreateWithFormat(kCFAllocatorDefault, 0, CFSTR("%@-TMP%ld"), serv->serviceID, i);
					if (serviceExt) {
						dnsDictionary = create_dns(gDynamicStore, serv->serviceID, dnsServers, NULL, mutableDomains, TRUE);
						key = SCDynamicStoreKeyCreateNetworkServiceEntity(kCFAllocatorDefault, kSCDynamicStoreDomainState, serviceExt, kSCEntNetDNS);
						needServerExceptionDNSConfig = TRUE;
					}
				}
				if (isDictionary(dnsDictionary) && isString(key)) {
					CFDictionaryAddValue(DNSTriggeringDicts, key, dnsDictionary);
				}
				my_CFRelease(&serviceExt);
				my_CFRelease(&mutableDomains);
				my_CFRelease(&key);
				my_CFRelease(&dnsDictionary);
			}
			
			CFStringRef probeURL = CFDictionaryGetValue(domainRule, kSCPropNetVPNOnDemandRuleActionParametersRequiredURLStringProbe);
			if (isString(probeURL) && !ondemand_probe_already_sent(serv, probeURL)) {
				CFStringRef primaryInterface = copy_primary_interface_name(serv->serviceID);
				start_https_probe(serv, probeURL, NULL, 0, NULL, primaryInterface, NULL);
				my_CFRelease(&primaryInterface);
			}
		}
	}
	
	if (needServerExceptionDNSConfig)
	{
		Boolean isHostname = FALSE;
		CFStringRef remoteAddress = scnc_copy_remote_server(serv, &isHostname);
		if (isHostname && isA_CFString(remoteAddress) && CFStringGetLength(remoteAddress) > 0) {
			CFStringRef serviceExt = NULL;
			CFArrayRef domains = CFArrayCreate(kCFAllocatorDefault, (const void **)&remoteAddress, 1, &kCFTypeArrayCallBacks);
			CFStringRef dnsstatekey = CREATEGLOBALSTATE(kSCEntNetDNS);
			CFDictionaryRef globalDNS = dnsstatekey ? SCDynamicStoreCopyValue(gDynamicStore, dnsstatekey) : NULL;
			CFArrayRef dnsServers = globalDNS ? CFDictionaryGetValue(globalDNS, kSCPropNetDNSServerAddresses) : NULL;

			serviceExt = CFStringCreateWithFormat(kCFAllocatorDefault, 0, CFSTR("%@-TMP-SERVER"), serv->serviceID);
			if (serviceExt) {
				CFDictionaryRef dnsDictionary = create_dns(gDynamicStore, serv->serviceID, dnsServers, NULL, domains, TRUE);
				CFStringRef key = SCDynamicStoreKeyCreateNetworkServiceEntity(kCFAllocatorDefault, kSCDynamicStoreDomainState, serviceExt, kSCEntNetDNS);
				if (isDictionary(dnsDictionary) && isString(key)) {
					CFDictionaryAddValue(DNSTriggeringDicts, key, dnsDictionary);
				}
				my_CFRelease(&dnsDictionary);
				my_CFRelease(&key);
			}
			
			my_CFRelease(&domains);
			my_CFRelease(&globalDNS);
			my_CFRelease(&dnsstatekey);
			my_CFRelease(&serviceExt);
		}
		my_CFRelease(&remoteAddress);
	}
	
	if (CFDictionaryGetCount(DNSTriggeringDicts) == 0) {
		my_CFRelease(&DNSTriggeringDicts);
		return FALSE;
	}
	
	/* Add supplemental match domains */
	
	if (serv->ondemandDNSTriggeringDicts && DNSTriggeringDicts && CFEqual(serv->ondemandDNSTriggeringDicts, DNSTriggeringDicts)) {
		/* If equal, we don't need to do anything */
		my_CFRelease(&DNSTriggeringDicts);
	} else {
		/* Otherwise, uninstall the old dictionaries from the dynamic store */
		ondemand_unpublish_dns_triggering_dicts(serv);
		my_CFRelease(&serv->ondemandDNSTriggeringDicts);
		serv->ondemandDNSTriggeringDicts = DNSTriggeringDicts;
	}
	
	return TRUE;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static 
void vpn_action(struct service *serv, CFStringRef match_action, CFPropertyListRef actionParameters)
{
	Boolean checked_dns_redirect = FALSE;
	Boolean set_dns_triggering_dicts = FALSE;
	boolean_t changed;
	
	changed = ondemand_add_action(serv, match_action, actionParameters);
    
	/* what VPN action should take place */
	if (isString(match_action)){
		if (CFStringCompare(match_action, kSCValNetVPNOnDemandRuleActionDisconnect, 0) == kCFCompareEqualTo){
			dodisconnect(serv);
		} else if (CFStringCompare(match_action, kSCValNetVPNOnDemandRuleActionAllow, 0) == kCFCompareEqualTo) {
			/* For allow, if there is a retry list, detect redirecting */
			CFArrayRef onRetryArray = NULL;
			onRetryArray = CFDictionaryGetValue(serv->systemprefs, kSCPropNetVPNOnDemandMatchDomainsOnRetry);
			if (isArray(onRetryArray) && (CFArrayGetCount(onRetryArray) > 0)) {
				checked_dns_redirect = dns_redirect_detection_start(serv);
			}
		} else if (isArray(actionParameters) && CFStringCompare(match_action, kSCValNetVPNOnDemandRuleActionEvaluateConnection, 0) == kCFCompareEqualTo) {
			/* Always detect DNS redirection if evaluating connection */
			checked_dns_redirect = dns_redirect_detection_start(serv);
			
			/* Create DNS triggering dicts */
			set_dns_triggering_dicts = doEvaluateConnection(serv, actionParameters);
		}
		
        /* Do nothing for ignore and connect */
	}
	
	if (!checked_dns_redirect) {
		serv->dnsRedirectDetected = FALSE;
	}
	
	if (!set_dns_triggering_dicts && serv->ondemandDNSTriggeringDicts) {
		ondemand_unpublish_dns_triggering_dicts(serv);
		my_CFRelease(&serv->ondemandDNSTriggeringDicts);
	}

	if (serv->flags & FLAG_SETUP_ONDEMAND) {
		ondemand_add_service(serv, FALSE);
	}

	if (changed) {
		app_layer_handle_network_detection_change(serv->serviceID);
	}
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static void cancel_https_probe(struct probe *probeptr)
{
    my_CFRelease(&probeptr->url_string);
    my_CFRelease(&probeptr->matcharray);
    my_CFRelease(&probeptr->primary_interface_name);
    my_CFRelease(&probeptr->primary_interface_type);
    my_CFRelease(&probeptr->primary_dns_dict);
    free(probeptr);
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static 
void https_probe_callback(pid_t pid, int status, struct rusage *rusage, void *context)
{
    struct probe        *probeptr = (struct probe*)context;
	struct service      *serv;
    CFStringRef         match_action = NULL;
    CFDictionaryRef     match_dict = NULL;
    int exitcode;
    
    if (probeptr == NULL) {
       return;
    }
    
    serv = probeptr->serv;
    if (serv == NULL || !service_is_valid(serv)) {
        /* If our service is no longer valid, bail */
        cancel_https_probe(probeptr);
        return;
    }
    
    exitcode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    ondemand_save_probe_result(serv, probeptr->url_string, (exitcode > 0));

    if (probeptr->matcharray) {
        if (exitcode > 0){
            /* If probe worked, do the action */
            match_dict = CFArrayGetValueAtIndex(probeptr->matcharray, probeptr->matcharray_index);
            if (isDictionary(match_dict)) {
                match_action = CFDictionaryGetValue(match_dict, kSCPropNetVPNOnDemandRuleAction);
                if (isString(match_action)){
                    vpn_action(serv, match_action, CFDictionaryGetValue(match_dict, kSCPropNetVPNOnDemandRuleActionParameters));
                }
            }
        } else {
            /* Try next action */
            resume_check_network(serv, probeptr->matcharray, probeptr->matcharray_index + 1, probeptr->primary_dns_dict, probeptr->primary_interface_name, probeptr->primary_interface_type);
        }
    } else {
        /* If the probe is outside of normal network detection, make sure we publish the result */
        if (serv->flags & FLAG_SETUP_ONDEMAND) {
            ondemand_add_service(serv, FALSE);
        }
    }
    
    /* Release structures */
    cancel_https_probe(probeptr);
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static
struct probe* alloc_new_probe(struct service *serv, CFStringRef url_string, CFArrayRef matcharray, CFIndex matcharray_index,
                              CFDictionaryRef primary_dns_dict, CFStringRef primary_interface_name,
                              CFStringRef primary_interface_type)
{
    struct probe *probeptr = NULL;
    
    /* set up probe ref */
    if (!(probeptr = malloc(sizeof(struct probe))))
        return NULL;
    
    probeptr->serv = serv;
    probeptr->url_string = my_CFRetain(url_string);
    probeptr->matcharray = my_CFRetain(matcharray);
    probeptr->matcharray_index = matcharray_index;
    probeptr->primary_dns_dict = my_CFRetain(primary_dns_dict);
    probeptr->primary_interface_name = my_CFRetain(primary_interface_name);
    probeptr->primary_interface_type = my_CFRetain(primary_interface_type);;

    return probeptr;
}

/* -----------------------------------------------------------------------------
 returns:-
 1 - child was spawned without error
 ----------------------------------------------------------------------------- */
static
int start_https_probe(struct service *serv, CFStringRef https_probe_server, CFArrayRef matcharray, CFIndex matcharray_index,
                      CFDictionaryRef primary_dns_dict, CFStringRef primary_interface_name, CFStringRef primary_interface_type)
{
	struct probe    *probeptr;
	char			https_probe_server_str[255];
	char            interface_str[IFNAMSIZ];
	int				ret = 0;

	if (primary_interface_name == NULL || https_probe_server == NULL){
		goto done;
	}

	if (!CFStringGetCString(https_probe_server, https_probe_server_str, sizeof(https_probe_server_str), kCFStringEncodingMacRoman)) {
		goto done;
	}

	if (!CFStringGetCString(primary_interface_name, interface_str, sizeof(interface_str), kCFStringEncodingMacRoman)) {
		goto done;
	}

	if ((probeptr = alloc_new_probe(serv, https_probe_server, matcharray, matcharray_index, primary_dns_dict, primary_interface_name, primary_interface_type)) == NULL) {
		goto done;
	}

	SCLog(TRUE, LOG_DEBUG, CFSTR("SCNC Controller: launching sbslauncher for %s (via %s)"), https_probe_server_str, interface_str);

	if (SCNCExecSBSLauncherCommandWithArguments(SBSLAUNCHER_TYPE_PROBE_SERVER, NULL, https_probe_callback, (void*)probeptr, https_probe_server_str, interface_str, NULL) == 0) {
		cancel_https_probe(probeptr);
		goto done;
	}

	/* Save the result as FALSE until we hear a reply */
	ondemand_save_probe_result(serv, https_probe_server, FALSE);
	ret = 1;

done:
	return ret;
}

static boolean_t
resume_check_network(struct service *serv, CFArrayRef match_array, CFIndex match_array_offset, CFDictionaryRef primary_dns_dict, CFStringRef primary_interface_name, CFStringRef primary_interface_type)
{
    CFStringRef     match_action = NULL;
    CFDictionaryRef match_dict = NULL;
    CFStringRef		cur_domain = NULL;
    CFArrayRef      cur_dns = NULL;
    CFArrayRef		match_domain_array = NULL;
    CFArrayRef		match_dns_array = NULL;
    CFArrayRef		match_ssid_array = NULL;
    CFStringRef     match_interface = NULL;
    CFStringRef     probe_server = NULL;
    boolean_t       found = false;
    boolean_t       domain_matched = true;
    boolean_t       dns_matched = true;
    boolean_t		ssid_matched = true;
    boolean_t		interface_matched = true;
    boolean_t       probe_sent = false;
    CFIndex         count = 0, i;

    if (!isArray(match_array)) {
        goto done;
    }

    count = CFArrayGetCount(match_array);
    if (match_array_offset >= count) {
        goto done;
    }

    if (primary_dns_dict == NULL || primary_interface_name == NULL || primary_interface_type == NULL) {
        goto done;
    }
    
    cur_domain = CFDictionaryGetValue(primary_dns_dict, kSCPropNetDNSDomainName);
    cur_dns = CFDictionaryGetValue(primary_dns_dict, kSCPropNetDNSServerAddresses);

    /* Traverse array, starting at offset */
    for (i = match_array_offset; i < count; i++) {
        match_dict = (CFDictionaryRef)CFArrayGetValueAtIndex(match_array, i);
        if (!(isDictionary(match_dict)))
            continue;

        /* check interface */
        match_interface = CFDictionaryGetValue(match_dict, kSCPropNetVPNOnDemandRuleInterfaceTypeMatch);
        if (match_interface == NULL
            || (isString(match_interface) && my_CFEqual(match_interface, primary_interface_type))) {
            interface_matched = true;
        } else {
            interface_matched = false;
        }
        
        if (!interface_matched)
            continue;
        
        /* check ssid */
        match_ssid_array = CFDictionaryGetValue(match_dict, kSCPropNetVPNOnDemandRuleSSIDMatch);
        if (isArray(match_ssid_array)) {
            ssid_matched = check_ssid(primary_interface_name, match_ssid_array);
        } else if (match_ssid_array == NULL) {
            ssid_matched = true;
        }

        if (!ssid_matched)
            continue;

        /* check domain name */
        match_domain_array = CFDictionaryGetValue(match_dict, kSCPropNetVPNOnDemandRuleDNSDomainMatch);
        if (isArray(match_domain_array) && isString(cur_domain)){
            domain_matched = check_domain(cur_domain, match_domain_array);
        } else if (match_domain_array == NULL) {
            domain_matched = true;
        } else {
            domain_matched = false;
        }
        
        if ( !domain_matched )          /* domain name does not match, continue to next */
            continue;

        /* check dns */
        match_dns_array = CFDictionaryGetValue(match_dict, kSCPropNetVPNOnDemandRuleDNSServerAddressMatch);
        if (isArray(match_dns_array)){
            dns_matched = check_dns(cur_dns, match_dns_array, 1);
        } else if (match_dns_array == NULL){
            dns_matched = true;
        }
        
        if (!dns_matched)
            continue;

        /* send probe if specified */
        probe_server = CFDictionaryGetValue(match_dict, kSCPropNetVPNOnDemandRuleURLStringProbe);
        if (isString(probe_server) && primary_interface_name){
            if (!start_https_probe(serv, probe_server, match_array, i, primary_dns_dict, primary_interface_name, primary_interface_type)){
                SCLog(TRUE, LOG_DEBUG, CFSTR("SCNC_Controller:check_network -- start_https_probe failed"));
                continue;   /* If we fail the probe, check the next rule. */
            }
            probe_sent = true;
        }
        break;              /* found a match */
    }
    
    // Start of done section:
    SCLog(TRUE, LOG_DEBUG, CFSTR("SCNC Controller:check_network -- interface_matched %d, ssid_matched %d, domain_matched %d, dns_matched %d"), interface_matched, ssid_matched, domain_matched, dns_matched);
    
    if (!probe_sent){
        if (isDictionary(match_dict)){
            found = interface_matched && ssid_matched && domain_matched && dns_matched;
            if (found){
                /* get the action for the found network or default no match fall back action */
                match_action = CFDictionaryGetValue(match_dict, kSCPropNetVPNOnDemandRuleAction);
                SCLog(TRUE, LOG_DEBUG, CFSTR("SCNC Controller:check_network - match_action %@"), match_action);
                vpn_action(serv, match_action, CFDictionaryGetValue(match_dict, kSCPropNetVPNOnDemandRuleActionParameters));
            }
        }else
            SCLog(TRUE, LOG_DEBUG, CFSTR("SCNC Controller:check_network -- no match_dict"));
        
    }
    
done:
    return found;
}

/* -----------------------------------------------------------------------------
 do network detection
 ----------------------------------------------------------------------------- */
boolean_t check_network(struct service *serv)
{
    CFDictionaryRef primary_dns_dict = NULL;
    CFStringRef     primary_interface_name = NULL;
    CFStringRef     primary_interface_type = NULL;
    CFStringRef     primary_interface_service_id = NULL;
    
    Boolean         did_network_detection = FALSE;
    Boolean         result = FALSE;

    /* Always clear actions */
	if (serv->flags & FLAG_SETUP_ONDEMAND) {
		/* clear action from trigger */
		ondemand_clear_action(serv);
	}
	
	/* Always clear probe results */
	ondemand_clear_probe_results(serv);

    /* Make sure there is a primary interface */
    primary_interface_name = copy_primary_interface_name(serv->serviceID);
    if (!isString(primary_interface_name)) {
        goto done;
    }
    
    /* Make sure there is a valid interface type and DNS dictionary */
    primary_interface_service_id = copy_service_id_for_interface(primary_interface_name);
    if (!isString(primary_interface_service_id)) {
        goto done;
    }
    
    primary_interface_type = copy_interface_type(primary_interface_service_id);
    if (!isString(primary_interface_type)) {
        goto done;
    }
    
    primary_dns_dict = copy_dns_dict(primary_interface_service_id);
    if (!isDictionary(primary_dns_dict)) {
        goto done;
    }
    
    /* Now actually check the network, potentially sending out a probe */
    result = resume_check_network(serv, CFDictionaryGetValue(serv->systemprefs, kSCPropNetVPNOnDemandRules), 0, primary_dns_dict, primary_interface_name, primary_interface_type);
    did_network_detection = TRUE;
    
done:
    if (!did_network_detection) {
        /* Notify listeners about the clear */
        if (serv->flags & FLAG_SETUP_ONDEMAND) {
            ondemand_add_service(serv, FALSE);
        }
    }
    
    my_CFRelease(&primary_interface_service_id);
    my_CFRelease(&primary_interface_name);
    my_CFRelease(&primary_interface_type);
    my_CFRelease(&primary_dns_dict);
    return result;
}
