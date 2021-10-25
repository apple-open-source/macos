/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <err.h>
#include <sysexits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pktmetadatafilter.h"

int main(int argc,  char * const argv[])
{
    int ch;
    char *input_str = NULL;
    node_t *pkt_meta_data_expression = NULL;
    int verbose = 0;

    while ((ch = getopt(argc, argv, "Q:v")) != -1) {
        switch (ch) {
            case 'Q':
                if (input_str != NULL) {
                    errx(EX_USAGE, "-Q used twice");
                }
                input_str = strdup(optarg);
                if (input_str == NULL) {
                    errx(EX_OSERR, "calloc() failed");
                }
                break;
            case 'v':
                verbose = 1;
                break;
        }
    }
    set_parse_verbose(verbose);

    pkt_meta_data_expression = parse_expression(input_str);
    if (pkt_meta_data_expression == NULL)
        errx(EX_SOFTWARE, "invalid expression \"%s\"", input_str);

    free(input_str);

    return 0;
}
