/*
 * Copyright (c) 2017-2018 Apple Inc. All rights reserved.
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

/*
 * Copyright (c) 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <stdlib.h>
#include <unistd.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_6lowpan_var.h>
#include <net/route.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include "ifconfig.h"

#include <sys/cdefs.h>


static boolean_t is_sixlowpan_inited = FALSE;
static struct sixlowpanreq params;

static int
get6lowpan(int s, struct ifreq *ifr, struct sixlowpanreq *req)
{
	bzero((char *)req, sizeof(*req));
	ifr->ifr_data = (caddr_t)req;
	return ioctl(s, SIOCGIF6LOWPAN, (caddr_t)ifr);
}

static void
sixlowpan_status(int s)
{
	struct sixlowpanreq req;

	if (get6lowpan(s, &ifr, &req) != -1)
		printf("\t6lowpan: parent interface: %s\n",
			req.parent[0] == '\0' ?
			"<none>" : req.parent);
}


static void
set6lowpandev(const char *val, int d, int s, const struct afswtch *afp)
{
	struct sixlowpanreq req;

	strlcpy(params.parent, val, sizeof(params.parent));
	is_sixlowpan_inited = TRUE;
	fprintf(stderr, "val %s\n", val);

	strlcpy(req.parent, val, sizeof(req.parent));
	ifr.ifr_data = (caddr_t) &req;
	if (ioctl(s, SIOCSIF6LOWPAN, (caddr_t)&ifr) == -1)
		err(1, "SIOCSIF6LOWPAN");
}

static void
unset6lowpandev(const char *val, int d, int s, const struct afswtch *afp)
{
	struct sixlowpanreq req;

	bzero((char *)&req, sizeof(req));
	ifr.ifr_data = (caddr_t)&req;

	if (ioctl(s, SIOCGIF6LOWPAN, (caddr_t)&ifr) == -1)
		err(1, "SIOCGIF6LOWPAN");

	bzero((char *)&req, sizeof(req));
	if (ioctl(s, SIOCSIF6LOWPAN, (caddr_t)&ifr) == -1)
		err(1, "SIOCSIF6LOWPAN");
}

static void
sixlowpan_create(const char *val, int d, int s, const struct afswtch *afp)
{
	strlcpy(params.parent, val, sizeof(params.parent));
    is_sixlowpan_inited = TRUE;
}

static void
sixlowpan_clone_cb(int s, void *arg)
{
	if (is_sixlowpan_inited == TRUE && params.parent[0] == '\0')
		errx(1, "6lowpandev must be specified");
}

static struct cmd sixlowpan_cmds[] = {
	DEF_CLONE_CMD_ARG("6lowpandev", sixlowpan_create),
	DEF_CMD_OPTARG("6lowpansetdev", set6lowpandev),
	DEF_CMD_OPTARG("6lowpanunsetdev", unset6lowpandev),
};
static struct afswtch af_6lowpan = {
	.af_name	= "af_6lowpan",
	.af_af		= AF_UNSPEC,
	.af_other_status = sixlowpan_status,
};

static __constructor void
sixlowpan_ctor(void)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	int i;

	for (i = 0; i < N(sixlowpan_cmds);  i++)
		cmd_register(&sixlowpan_cmds[i]);
	af_register(&af_6lowpan);
	callback_register(sixlowpan_clone_cb, NULL);
#undef N
}
