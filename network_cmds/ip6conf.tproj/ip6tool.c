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
#include <sys/socket.h>
/* this is needed to get SIOCPROTOATTACH/SIOCPROTODETACH */
#define KERNEL_PRIVATE
#include <sys/ioctl.h>
#undef KERNEL_PRIVATE
#include <net/if.h>
#include <sys/types.h>
#include <ifaddrs.h>


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
int do_protoattach_all(int s);
int do_protodetach_all(int s);


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
				printf("%s: Error %d encountered attaching to interface %s.\n", argv[0], err, interface);
				
			break;
		case IPv6_SHUTDOWN:
			err = do_protodetach(s, interface);
			if (err < 0)
				printf("%s: Error %d encountered detaching to interface %s.\n", argv[0], err, interface);
			
			break;
		case IPv6_STARTUP_ALL:
			err = do_protoattach_all(s);
			if (err < 0)
				printf("%s: Error %d encountered attaching to interfaces.\n", argv[0], err);
			
			break;
		case IPv6_SHUTDOWN_ALL:
			err = do_protodetach_all(s);
			if (err < 0)
				printf("%s: Error %d encountered detaching to interfaces.\n", argv[0], err);
			
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
    struct ifreq	ifr;
	
    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCPROTOATTACH, &ifr));
}

int
do_protodetach(int s, char *name)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCPROTODETACH, &ifr));
}

int
do_protoattach_all(int s)
{
	struct	ifaddrs *ifaddrs, *ifa;
	int		err;
	
	if ((err = getifaddrs(&ifaddrs)) < 0)
		return err; /* getifaddrs properly sets errno */
			
	for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
		/* skip over invalid interfaces */
		if ((strcmp(ifa->ifa_name, if_exceptions[0])))
			if (strcmp(ifa->ifa_name, if_exceptions[1]))
				if (strcmp(ifa->ifa_name, if_exceptions[2]))
					if (strcmp(ifa->ifa_name, if_exceptions[3])) {
						/* this is a valid interface */
						err = do_protoattach(s, ifa->ifa_name);
						if (err)
							break;
						
						while (ifa->ifa_next != NULL && 
								!(strcmp(ifa->ifa_name, ifa->ifa_next->ifa_name))) {
							/* skip multiple entries for same interface */
							ifa = ifa->ifa_next;
						}
					}
	}
	
	freeifaddrs(ifaddrs);
	
	return err;
}

int
do_protodetach_all(int s)
{
	struct	ifaddrs *ifaddrs, *ifa;
	int		err;
	
	if ((err = getifaddrs(&ifaddrs)) < 0)
		return err; /* getifaddrs properly sets errno */
			
	for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
		/* skip over invalid interfaces */
		if ((strcmp(ifa->ifa_name, if_exceptions[0])))
			if (strcmp(ifa->ifa_name, if_exceptions[1]))
				if (strcmp(ifa->ifa_name, if_exceptions[2]))
					if (strcmp(ifa->ifa_name, if_exceptions[3])) {
						/* this is a valid interface */
						err = do_protodetach(s, ifa->ifa_name);
						if (err)
							break;
						
						while (ifa->ifa_next != NULL && 
								!(strcmp(ifa->ifa_name, ifa->ifa_next->ifa_name))) {
							/* skip multiple entries for same interface */
							ifa = ifa->ifa_next;
						}
					}
	}
	
	freeifaddrs(ifaddrs);
	
	return err;
}