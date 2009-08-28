/*
 * fips186prf.c    An implementation of the FIPS-186-2 SHA1-based PRF.
 *
 * The development of the EAP/SIM support was funded by Internet Foundation
 * Austria (http://www.nic.at/ipa).
 *
 * This code was written from scratch by Michael Richardson, and it is
 * dual licensed under both GPL and BSD.
 *
 * Version:     $Id: fips186prf.c,v 1.12 2008/06/05 12:17:33 aland Exp $
 *
 * GPL notice:
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * BSD notice:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Copyright 2003  Michael Richardson <mcr@sandelman.ottawa.on.ca>
 * Copyright 2006  The FreeRADIUS server project
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <stdint.h>
#include "fr_sha1.h"
#include "fips186prf.h"

/*
 * we do it in 8-bit chunks, because we have to keep the numbers
 * in network byte order (i.e. MSB)
 *
 * make it a structure so that we can do structure assignments.
 */
typedef struct onesixty {
	uint8_t p[20];
} onesixty;

static void onesixty_add_mod(onesixty *sum, onesixty *a, onesixty *b)
{
	uint32_t s;
	int i, carry;

	carry = 0;
	for(i=19; i>=0; i--) {
/*	for(i=0; i<20; i++) {  */
		s = a->p[i] + b->p[i] + carry;
		sum->p[i] = s & 0xff;
		carry = s >> 8;
	}
}

/*
 * run the FIPS-186-2 PRF on the given Master Key (160 bits)
 * in order to derive 1280 bits (160 bytes) of keying data from
 * it.
 *
 * Given that in EAP-SIM, this is coming from a 64-bit Kc it seems
 * like an awful lot of "randomness" to pull out.. (MCR)
 *
 */

__private_extern__
void fips186_2prf(uint8_t mk[20], uint8_t finalkey[160])
{
	fr_SHA1_CTX context;
	int j;
	onesixty xval, xkey, w_0, w_1, sum, one;
	uint8_t *f;
	uint8_t zeros[64];

	/*
	 * let XKEY := MK,
	 *
	 * Step 3: For j = 0 to 3 do
         *   a. XVAL = XKEY
         *   b. w_0 = SHA1(XVAL)
         *   c. XKEY = (1 + XKEY + w_0) mod 2^160
         *   d. XVAL = XKEY
         *   e. w_1 = SHA1(XVAL)
         *   f. XKEY = (1 + XKEY + w_1) mod 2^160
         * 3.3 x_j = w_0|w_1
	 *
	 */
	memcpy(&xkey, mk, sizeof(xkey));

	/* make the value 1 */
	memset(&one,  0, sizeof(one));
	one.p[19]=1;

	f=finalkey;

	for(j=0; j<4; j++) {
		/*   a. XVAL = XKEY  */
		xval = xkey;

		/*   b. w_0 = SHA1(XVAL)  */
		fr_SHA1Init(&context);

		memset(zeros, 0, sizeof(zeros));
		memcpy(zeros, xval.p, 20);
		fr_SHA1Transform(&context, zeros);
		fr_SHA1FinalNoLen(w_0.p, &context);

		/*   c. XKEY = (1 + XKEY + w_0) mod 2^160 */
		onesixty_add_mod(&sum,  &xkey, &w_0);
		onesixty_add_mod(&xkey, &sum,  &one);

		/*   d. XVAL = XKEY  */
		xval = xkey;

		/*   e. w_1 = SHA1(XVAL)  */
		fr_SHA1Init(&context);

		memset(zeros, 0, sizeof(zeros));
		memcpy(zeros, xval.p, 20);
		fr_SHA1Transform(&context, zeros);
		fr_SHA1FinalNoLen(w_1.p, &context);

		/*   f. XKEY = (1 + XKEY + w_1) mod 2^160 */
		onesixty_add_mod(&sum,  &xkey, &w_1);
		onesixty_add_mod(&xkey, &sum,  &one);

		/* now store it away */
		memcpy(f, &w_0, 20);
		f += 20;

		memcpy(f, &w_1, 20);
		f += 20;
	}
}

#ifdef TEST_FIPS186PRF
/*
  from RFC4186: A.5.  EAP-Request/SIM/Challenge

   Next, the MK is calculated as specified in Section 7*.

   MK = e576d5ca 332e9930 018bf1ba ee2763c7 95b3c712

   And the other keys are derived using the PRNG:

         K_encr = 536e5ebc 4465582a a6a8ec99 86ebb620
         K_aut =  25af1942 efcbf4bc 72b39434 21f2a974
         MSK =    39d45aea f4e30601 983e972b 6cfd46d1
                  c3637733 65690d09 cd44976b 525f47d3
                  a60a985e 955c53b0 90b2e4b7 3719196a
                  40254296 8fd14a88 8f46b9a7 886e4488
         EMSK =   5949eab0 fff69d52 315c6c63 4fd14a7f
                  0d52023d 56f79698 fa6596ab eed4f93f
                  bb48eb53 4d985414 ceed0d9a 8ed33c38
                  7c9dfdab 92ffbdf2 40fcecf6 5a2c93b9
 */
uint8_t	mk[20] = { 0xe5, 0x76, 0xd5, 0xca, 
		    0x33, 0x2e, 0x99, 0x30,
		    0x01, 0x8b, 0xf1, 0xba,
		    0xee, 0x27, 0x63, 0xc7,
		    0x95, 0xb3, 0xc7, 0x12 };

uint8_t final_output[160] = {
    0x53, 0x6e, 0x5e, 0xbc, 0x44, 0x65, 0x58, 0x2a, 0xa6, 0xa8, 0xec, 0x99,  0x86, 0xeb, 0xb6, 0x20,
    0x25, 0xaf, 0x19, 0x42, 0xef, 0xcb, 0xf4, 0xbc, 0x72, 0xb3, 0x94, 0x34,  0x21, 0xf2, 0xa9, 0x74,
    0x39, 0xd4, 0x5a, 0xea, 0xf4, 0xe3, 0x06, 0x01, 0x98, 0x3e, 0x97, 0x2b,  0x6c, 0xfd, 0x46, 0xd1,
    0xc3, 0x63, 0x77, 0x33, 0x65, 0x69, 0x0d, 0x09, 0xcd, 0x44, 0x97, 0x6b,  0x52, 0x5f, 0x47, 0xd3,
    0xa6, 0x0a, 0x98, 0x5e, 0x95, 0x5c, 0x53, 0xb0, 0x90, 0xb2, 0xe4, 0xb7,  0x37, 0x19, 0x19, 0x6a,
    0x40, 0x25, 0x42, 0x96, 0x8f, 0xd1, 0x4a, 0x88, 0x8f, 0x46, 0xb9, 0xa7,  0x88, 0x6e, 0x44, 0x88,
    0x59, 0x49, 0xea, 0xb0, 0xff, 0xf6, 0x9d, 0x52, 0x31, 0x5c, 0x6c, 0x63,  0x4f, 0xd1, 0x4a, 0x7f,
    0x0d, 0x52, 0x02, 0x3d, 0x56, 0xf7, 0x96, 0x98, 0xfa, 0x65, 0x96, 0xab,  0xee, 0xd4, 0xf9, 0x3f,
    0xbb, 0x48, 0xeb, 0x53, 0x4d, 0x98, 0x54, 0x14, 0xce, 0xed, 0x0d, 0x9a,  0x8e, 0xd3, 0x3c, 0x38,
    0x7c, 0x9d, 0xfd, 0xab, 0x92, 0xff, 0xbd, 0xf2, 0x40, 0xfc, 0xec, 0xf6,  0x5a, 0x2c, 0x93, 0xb9,
};

int
main(int argc, char *argv[])
{
	uint8_t finalkey[160];
	int i, j, k;

	fips186_2prf(mk, finalkey);

	printf("Input was: ");
	j=0;
	for (i = 0; i < 20; i++) {
		if(j==4) {
			printf(" ");
			j=0;
		}
		j++;

		printf("%02x", mk[i]);
	}

	printf("\nOutput was: ");
	j=0; k=0;
	for (i = 0; i < 160; i++) {
		if(k==16) {
			printf("\n            ");
			k=0;
			j=0;
		}
		if(j==4) {
			printf(" ");
			j=0;
		}
		k++;
		j++;

		printf("%02x", finalkey[i]);
	}
	printf("\n");

	if (bcmp(finalkey, final_output, sizeof(finalkey)) != 0) {
	    fprintf(stderr, "Calculation FAILED\n");
	}
	else {
	    printf("Calculation is correct!\n");
	}
	exit(0);
	return (0);
}
#endif /* TEST_FIPS186PRF */
