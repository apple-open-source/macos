/*
 * Copyright (c) 2001 - 2010 Apple Inc. All rights reserved.
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

#include <sys/errno.h>
#include <sys/socket.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <sysexits.h>
#include <arpa/inet.h>
#include <CoreFoundation/CoreFoundation.h>

#include <SMBClient/smbclient.h>
#include <SMBClient/netbios.h>

#include "common.h"

static const char *nb_name_type_string(uint8_t nbType, uint16_t nbFlags)
{
	
	switch (nbType) {
		case kNetBIOSWorkstationService:
			if (nbFlags & NBNS_GROUP_NAME) {
				return "Domain Name";
			} else {
				return "Workstation Service";
			}
			break;
		case kNetBIOSMSBrowseService:
			if (nbFlags & NBNS_GROUP_NAME) {
				return "Master Browser";
			} else {
				return "Messenger Service";
			}
			break;
		case kNetBIOSMessengerService:
			return "Messenger Service";
			break;
		case kNetBIOSRASServerService:
			return "RAS Server Service";
			break;
		case kNetBIOSDomainMasterBrowser:
			return "Domain Master Browser";
			break;
		case kNetBIOSDomainControllers:
			return "Domain Controllers";
			break;
		case kNetBIOSMasterBrowser:
			return "Master Browser";
			break;
		case kNetBIOSBrowserServiceElections:
			return "Browser Service Elections";
			break;
		case kNetBIOSNetDDEService:
			return "NetDDE Service";
			break;
		case kNetBIOSFileServerService:
			return "File/Print Server Service";
			break;
		case kNetBIOSRASClientService:
			return "RAS Client Service";
			break;
		case kNetBIOSMSExchangeInterchange:
			return "Microsoft Exchange Interchange";
			break;
		case kNetBIOSMicrosoftExchangeStore:
			return "Microsoft Exchange Store";
			break;
		case kNetBIOSMicrosoftExchangeDirectory:
			return "Microsoft Exchange Directory";
			break;
		case kNetBIOSModemSharingServerService:
			return "Modem Sharing Server Service";
			break;
		case kNetBIOSModemSharingClientService:
			return "Modem Sharing Client Service";
			break;
		case kNetBIOSSMSClientsRemoteControl:
			return "SMS Clients Remote Control";
			break;
		case kNetBIOSSMSAdminRemoteControlTool:
			return "SMS Administrators Remote Control Tool";
			break;
		case kNetBIOSSMSClientsRemoteChat:
			return "SMS Clients Remote Chat";
			break;
		case kNetBIOSDECPathworks:
			return "DEC Pathworks";
			break;
		case kNetBIOSMicrosoftExchangeIMC:
			return "Microsoft Exchange MTA";
			break;
		case kNetBIOSMicrosoftExchangeMTA:
			return "Microsoft Exchange IMC";
			break;
		case kNetBIOSNetworkMonitorAgent:
			return "Network Monitor Agent";
			break;
		case kNetBIOSNetworkMonitorApplication:
			return "Network Monitor Application";
			break;
		default:
			return "";
			break;
	}	
}

/*
 * Convert the NetBIOS name to a UTF8 display name.
 */
static char * 
getDisplayName(const char *name, Boolean escapeNames)
{	
	char *displayName = SMBConvertFromCodePageToUTF8(name);
	CFStringRef nameRef = NULL;
	CFStringRef newNameRef = NULL;

	if (!escapeNames) {
		return displayName;
	}
	
	/* Get a CFString */
	if (displayName) {
		nameRef = CFStringCreateWithCString(kCFAllocatorDefault, displayName, kCFStringEncodingUTF8);
	}
	
	/* Escape out the CFString */
	if (nameRef) {
		newNameRef = CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault, nameRef, 
															 CFSTR(" "), CFSTR(""), 
															 kCFStringEncodingUTF8);
	}
	/* Now create an esacpe out c style string */
	if (newNameRef) {
		int maxlen = (int)CFStringGetLength(newNameRef)+1;
		
		free(displayName);
		displayName = malloc(maxlen);
		if (displayName) {
			CFStringGetCString(newNameRef, displayName, maxlen, kCFStringEncodingUTF8);
		}
	}

	if (nameRef) {
		CFRelease(nameRef);
	}
	if (newNameRef) {
		CFRelease(newNameRef);
	}
	return displayName;
}

int
cmd_status(int argc, char *argv[])
{
	uint32_t ii;
	int opt;
	char * displayName;
	char *hostname;
	struct NodeStatusInfo *listInfo, *startInfo;
	uint32_t count = 0;
	Boolean displayAllNames= FALSE;
	Boolean escapeNames= FALSE;

	if (argc < 2) {
		status_usage();
	}
	
	while ((opt = getopt(argc, argv, "ae")) != EOF) {
		switch(opt) {
			case 'a':
				displayAllNames = TRUE;
				break;
			case 'e':
				escapeNames = TRUE;
				break;
		    default:
				status_usage();
				/*NOTREACHED*/
		}
	}
	
	if (optind >= argc) {
		status_usage();
	}
	
	hostname = argv[argc - 1];
	startInfo = listInfo = SMBGetNodeStatus(hostname, &count);
	if (startInfo == NULL) {
		if (errno == EHOSTUNREACH) {
			err(EX_NOHOST, "unable to get status for hostname %s", hostname);
		} else {
			err(EX_NOHOST, "unable to resolve hostname %s", hostname);
		}
	}

	for (ii=0; ii < count; ii++, listInfo++) {				
		struct sockaddr_in *in4 = NULL;
		struct sockaddr_in6 *in6 = NULL;
		char addrStr[INET6_ADDRSTRLEN+1];
		
		/* Get an address we can display */
		if (listInfo->node_storage.ss_family == AF_INET) {
			in4 = (struct sockaddr_in *)&listInfo->node_storage;
			inet_ntop(listInfo->node_storage.ss_family, &in4->sin_addr, addrStr, sizeof(addrStr));
		} else if (listInfo->node_storage.ss_family == AF_INET6) {
			in6 = (struct sockaddr_in6 *)&listInfo->node_storage;
			inet_ntop(listInfo->node_storage.ss_family, &in6->sin6_addr, addrStr, sizeof(addrStr));
		} else {
			strcpy(addrStr, "unknown address family");
		} 
		/* Didn't get any names on this address go to the next one */
		if (listInfo->node_errno) {
			fprintf(stdout, "%s: unable to get status from %s using address %s\n", 
					strerror(listInfo->node_errno), hostname, addrStr);
			continue;
		}

		fprintf(stdout, "Using IP address of %s: %s\n", hostname, addrStr);
		/* They want us to display all the names. */
		if (displayAllNames) {
			struct NBResourceRecord *nbrr = listInfo->node_nbrrArray;
			
			if (nbrr) {
				fprintf(stdout, "%-32s %-7s %-6s  Description\n", "NetBIOS Name","Number","Type");
				for (ii=0; ii < listInfo->node_nbrrArrayCnt; ii++) {
					uint8_t nbNum;

					nbNum = nbrr->rrName[NetBIOS_NAME_LEN-1];
					nbrr->rrName[NetBIOS_NAME_LEN-1] = 0;
					displayName = getDisplayName(nbrr->rrName, escapeNames);
					if (displayName) {
						fprintf(stdout, "%-32s 0x%.2x    %-6s  [%s]\n", displayName, 
								nbNum, (nbrr->nbFlags & NBNS_GROUP_NAME) ? "GROUP" : "UNIQUE", 
								nb_name_type_string(nbNum, nbrr->nbFlags));
						free(displayName);
					} else {
						fprintf(stdout, "%-32s 0x%.2x    %-6s  [%s]\n", nbrr->rrName, 
								nbNum, (nbrr->nbFlags & NBNS_GROUP_NAME) ? "GROUP" : "UNIQUE", 
								nb_name_type_string(nbNum, nbrr->nbFlags));
					}
					nbrr++;
				}
			}
		} else {
			/* Display the file server and workgroup name */
			if (listInfo->node_workgroupname[0]) {
				displayName = getDisplayName(listInfo->node_workgroupname, escapeNames);
				if (displayName) {
					fprintf(stdout, "Workgroup: %s\n", displayName);
					free(displayName);
				} else 
					fprintf(stdout, "Workgroup: %s\n", listInfo->node_workgroupname);
			}
			if (listInfo->node_servername[0]) {
				displayName = getDisplayName(listInfo->node_servername, escapeNames);
				if (displayName) {
					fprintf(stdout, "Server: %s\n", displayName);
					free(displayName);
				} else 
					fprintf(stdout, "Server: %s\n", listInfo->node_servername);
			}
		}
		fprintf(stdout, "\n");
		/* We have nbrrArray so free it */
		if (listInfo->node_nbrrArray) {
			free(listInfo->node_nbrrArray);
			listInfo->node_nbrrArray = NULL;
		}
	}

	if (startInfo) {
		free(startInfo);
	}
	return 0;
}


void
status_usage(void)
{
	fprintf(stderr, "usage: smbutil status  [ -ae ] hostname\n");
	exit(1);
}
