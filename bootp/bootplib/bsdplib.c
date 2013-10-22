/*
 * Copyright (c) 2003-2013 Apple Inc. All rights reserved.
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

#include "dhcp.h"
#include "dhcp_options.h"
#include "dhcplib.h"
#include "bsdp.h"
#include "bsdplib.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void
bsdp_print_packet(struct dhcp * pkt, int length, int options_only)
{
    dhcpo_err_str_t	err;
    int 	i;
    dhcpol_t	options;
    dhcpol_t	vendor_options;

    dhcpol_init(&options);
    dhcpol_init(&vendor_options);
    if (options_only == 0) {
	dhcp_packet_print(pkt, length);
    }
    if (dhcpol_parse_packet(&options, pkt, length, &err) == FALSE) {
	fprintf(stderr, "packet did not parse, %s\n", err.str);
	return;
    }
    if (dhcpol_parse_vendor(&vendor_options, &options, &err) == FALSE) {
	fprintf(stderr, "vendor options did not parse, %s\n", err.str);
	goto done;
    }
    printf("BSDP Options count is %d\n", dhcpol_count(&vendor_options));
    for (i = 0; i < dhcpol_count(&vendor_options); i++) {
	u_int8_t	code;
	u_int8_t *	opt = dhcpol_element(&vendor_options, i);
	u_int8_t	len;

	code = opt[TAG_OFFSET];
	len = opt[LEN_OFFSET];
	printf("%s: ", bsdptag_name(code));
	if (code == bsdptag_message_type_e) {
	    printf("%s (", bsdp_msgtype_names(opt[OPTION_OFFSET]));
	    dhcptype_print(bsdptag_type(code), opt + OPTION_OFFSET, len);
	    printf(")\n");
	}
	else {
	    dhcptype_print(bsdptag_type(code), opt + OPTION_OFFSET, len);
	    printf("\n");
	}
    }
 done:
    dhcpol_free(&options);
    dhcpol_free(&vendor_options);
    return;
}
