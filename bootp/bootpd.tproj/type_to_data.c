/*
 * Copyright (c) 2008-2012 Apple Inc. All rights reserved.
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

/*
 * type_to_data.c
 * - convert from ip address, uint{8,16,32}, and string to xml data
 */

/*
 * Modification History:
 * 
 * March 17, 2008	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFPropertyList.h>

static void
usage(const char * prog)
{
    fprintf(stderr,
	    "usage: %s <type> <value1> [ <value2> ... [ <valueN> ] ... ]\n"
	    "\t where <type> is one of ip, uint8, uint16, uint32, or string\n",
	    prog);
    exit(1);
    return;
}

static void *
data_from_ip(int argc, char * argv[], int * len)
{
    int			i;
    struct in_addr *	ip_list;

    *len = argc * sizeof(*ip_list);
    ip_list = (struct in_addr *)malloc(*len);
    for (i = 0; i < argc; i++) {
	if (inet_aton(argv[i], ip_list + i) == 0) {
	    fprintf(stderr, "invalid IP address '%s'\n",
		    argv[i]);
	    goto failed;
	}
    }
    return (ip_list);
 failed:
    free(ip_list);
    return (NULL);
}

static void *
data_from_uint8(int argc, char * argv[], int * len)
{
    int		i;
    uint8_t * 	list;

    *len = argc * sizeof(*list);
    list = (uint8_t *)malloc(*len);
    for (i = 0; i < argc; i++) {
	list[i] = (uint8_t)strtoul(argv[i], NULL, 0);
    }
    return (list);
}

static void *
data_from_uint16(int argc, char * argv[], int * len)
{
    int		i;
    uint16_t * 	list;

    *len = argc * sizeof(*list);
    list = (uint16_t *)malloc(*len);
    for (i = 0; i < argc; i++) {
	list[i] = htons((uint16_t)strtoul(argv[i], NULL, 0));
    }
    return (list);
}

static void *
data_from_uint32(int argc, char * argv[], int * len)
{
    int		i;
    uint32_t * 	list;

    *len = argc * sizeof(*list);
    list = (uint32_t *)malloc(*len);
    for (i = 0; i < argc; i++) {
	list[i] = htonl((uint32_t)strtoul(argv[i], NULL, 0));
    }
    return (list);
}

int
main(int argc, char * argv[])
{
    const void *	data;
    int			data_len;
    CFDataRef		cfdata;
    const char *	type;
    CFDataRef		xml_data;

    if (argc < 3) {
	usage(argv[0]);
    }
    type = argv[1];
    argv += 2;
    argc -= 2;
    if (strcmp(type, "ip") == 0) {
	data = data_from_ip(argc, argv, &data_len);
    }
    else if (strcmp(type, "uint8") == 0) {
	data = data_from_uint8(argc, argv, &data_len);
    }
    else if (strcmp(type, "uint16") == 0) {
	data = data_from_uint16(argc, argv, &data_len);
    }
    else if (strcmp(type, "uint32") == 0) {
	data = data_from_uint32(argc, argv, &data_len);
    }
    else if (strcmp(type, "string") == 0) {
	data = argv[0];
	data_len = strlen(data);
    }
    else {
	fprintf(stderr, "unrecognized type '%s'\n", type);
	exit(2);
    }
    if (data == NULL) {
	exit(2);
    }
    cfdata = CFDataCreateWithBytesNoCopy(NULL, data, data_len,
					 kCFAllocatorNull);
    xml_data = CFPropertyListCreateXMLData(NULL, cfdata);
    CFRelease(cfdata);
    fwrite(CFDataGetBytePtr(xml_data), CFDataGetLength(xml_data), 1,
	   stdout);
    CFRelease(xml_data);
    exit(0);
    return (0);
}
