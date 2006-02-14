/*	Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
*
* @APPLE_LICENSE_HEADER_START@
* 
* "Portions Copyright (c) 2002 Apple Computer, Inc.  All Rights
* Reserved.  This file contains Original Code and/or Modifications of
* Original Code as defined in and that are subject to the Apple Public
* Source License Version 1.0 (the 'License').  You may not use this file
* except in compliance with the License.  Please obtain a copy of the
* License at http://www.apple.com/publicsource and read it before using
* this file.
* 
* The Original Code and all software distributed under the License are
* distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
* EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
* INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
* License for the specific language governing rights and limitations
* under the License."
* 
* @APPLE_LICENSE_HEADER_END@
*
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <ifaddrs.h>

/* From netinet6/in6_var.h */
#ifndef SIOCPROTOATTACH_IN6
#define SIOCPROTOATTACH_IN6 _IOWR('i', 110, struct in6_aliasreq)    /* attach proto to interface */
#endif
#ifndef SIOCPROTODETACH_IN6
#define SIOCPROTODETACH_IN6 _IOWR('i', 111, struct in6_ifreq)    /* detach proto from interface */
#endif
#ifndef SIOCLL_START
#define SIOCLL_START _IOWR('i', 130, struct in6_aliasreq)    /* start aquiring linklocal on interface */
#endif
#ifndef SIOCLL_STOP
#define SIOCLL_STOP _IOWR('i', 131, struct in6_ifreq)    /* deconfigure linklocal from interface */
#endif

/* options */
#define IPv6_STARTUP		1
#define IPv6_SHUTDOWN		2
#define IPv6_STARTUP_ALL	3
#define IPv6_SHUTDOWN_ALL	4

const char *if_exceptions[] = {"lo0", "gif0", "faith0", "stf0"};

extern char	*optarg;

void do_usage(void);
int do_protoattach(int s, char *name);
int do_protodetach(int s, char *name);
void do_protoattach_all(int s);
void do_protodetach_all(int s);


int
main(int argc, char **argv)
{
	int 	s,
			ch,
			option = 0,
			err;
	char	*interface = NULL;
					
	if ((ch = getopt(argc, argv, "u:d:ax")) != -1) {
		switch (ch) {
			case 'u':
				/* option -u: start up proto */
				option = IPv6_STARTUP;
				interface = optarg;
				break;
			case 'd':
				/* option -d: shut down proto */
				option = IPv6_SHUTDOWN;
				interface = optarg;
				break;
			case 'a':
				/* option -a: start up proto on all interfaces */
				option = IPv6_STARTUP_ALL;
				break;
			case 'x':
				/* option -x: shut down proto on all interfaces */
				option = IPv6_SHUTDOWN_ALL;
				break;
			default:
				break;
		}
	}

	if (!option) {
		do_usage();
		return 0;
	}
	
	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		err = s;
		printf("%s: Error %d creating socket.\n", argv[0], err);
		return 0;
	}
	
	switch (option) {
		case IPv6_STARTUP:
			err = do_protoattach(s, interface);
			if (err < 0)
				printf("%s: Error %d encountered attaching interface %s.\n", argv[0], err, interface);
				
			break;
		case IPv6_SHUTDOWN:
			err = do_protodetach(s, interface);
			if (err < 0)
				printf("%s: Error %d encountered detaching interface %s.\n", argv[0], err, interface);
			
			break;
		case IPv6_STARTUP_ALL:
			do_protoattach_all(s);
			
			break;
		case IPv6_SHUTDOWN_ALL:
			do_protodetach_all(s);
			
			break;
		default:
			break;
	}
	
	close(s);
		
	return 0;
}

void
do_usage(void)
{
	printf("Usage: \n\
	Start up IPv6 on ALL interfaces:	-a\n\
	Shut down IPv6 on ALL interfaces:	-x\n\
	Start up IPv6 on given interface:	-u [interface]\n\
	Shut down IPv6 on given interface:	-d [interface].\n");
}

int
do_protoattach(int s, char *name)
{
    struct in6_aliasreq		ifr;
    int		err;
	
    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifra_name, name, sizeof(ifr.ifra_name));
    
    if ((err = ioctl(s, SIOCPROTOATTACH_IN6, &ifr)) != 0)
        return (err);
    
    return (ioctl(s, SIOCLL_START, &ifr));
}

int
do_protodetach(int s, char *name)
{
    struct in6_ifreq	ifr;
    int		err;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

    if ((err = ioctl(s, SIOCLL_STOP, &ifr)) != 0)
        return (err);
    
    return (ioctl(s, SIOCPROTODETACH_IN6, &ifr));
}

void
do_protoattach_all(int s)
{
	struct	ifaddrs *ifaddrs, *ifa;
    
	if (getifaddrs(&ifaddrs)) {
        printf("ip6: getifaddrs returned error (%s)", strerror(errno));
    }
			
	for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
		/* skip over invalid interfaces */
		if ((strcmp(ifa->ifa_name, if_exceptions[0])))
			if (strcmp(ifa->ifa_name, if_exceptions[1]))
				if (strcmp(ifa->ifa_name, if_exceptions[2]))
					if (strcmp(ifa->ifa_name, if_exceptions[3])) {
						/* this is a valid interface */
						if (do_protoattach(s, ifa->ifa_name)) {
                            printf("ip6: error attaching %s\n", ifa->ifa_name);
                        }
						
						while (ifa->ifa_next != NULL && 
								!(strcmp(ifa->ifa_name, ifa->ifa_next->ifa_name))) {
							/* skip multiple entries for same interface */
							ifa = ifa->ifa_next;
						}
					}
	}
	
	freeifaddrs(ifaddrs);
	
	return;
}

void
do_protodetach_all(int s)
{
	struct	ifaddrs *ifaddrs, *ifa;
	
	if (getifaddrs(&ifaddrs)) {
        printf("ip6: getifaddrs returned error (%s)", strerror(errno));
    }
			
	for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
		/* skip over invalid interfaces */
		if ((strcmp(ifa->ifa_name, if_exceptions[0])))
			if (strcmp(ifa->ifa_name, if_exceptions[1]))
				if (strcmp(ifa->ifa_name, if_exceptions[2]))
					if (strcmp(ifa->ifa_name, if_exceptions[3])) {
						/* this is a valid interface */
						if (do_protodetach(s, ifa->ifa_name)) {
                            printf("ip6: error detaching %s\n", ifa->ifa_name);
                        }
						
						while (ifa->ifa_next != NULL && 
								!(strcmp(ifa->ifa_name, ifa->ifa_next->ifa_name))) {
							/* skip multiple entries for same interface */
							ifa = ifa->ifa_next;
						}
					}
	}
	
	freeifaddrs(ifaddrs);
	
	return;
}
