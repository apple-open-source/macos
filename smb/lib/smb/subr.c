/*
 * Copyright (c) 2000, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2007 Apple Inc. All rights reserved.
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: subr.c,v 1.20 2006/04/12 04:55:30 lindak Exp $
 */
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <asl.h>
#ifndef USE_ASL_LOGGING
#include <syslog.h>
#endif // USE_ASL_LOGGING

#include <locale.h>

#include <netsmb/netbios.h>
#include <netsmb/smb_lib.h>
#include <netsmb/nb_lib.h>
#include <cflib.h>
#include <sys/smb_iconv.h>

#include <fs/smbfs/smbfs.h>
#include <load_smbfs.h>
#include "smbfs_load_kext.h"

/*
 * Generic routine to log information or errors
 */
void smb_log_info(const char *fmt, int error, int log_level,...) 
{
	va_list ap;
	char *fstr = NULL;
	
	va_start(ap, log_level);
	if (vasprintf(&fstr, fmt, ap) == -1)
		asl_log(NULL, NULL, log_level, fmt, ap);
	va_end(ap);
	/* Some day when asl_log works correctly we can turn this back on. */
#ifdef USE_ASL_LOGGING
	if (error == -1)
		asl_log(NULL, NULL, log_level, "%s: syserr = %s\n", (fstr) ? fstr : "", strerror(errno));
	else if (error)
		asl_log(NULL, NULL, log_level, "%s: syserr = %s\n", (fstr) ? fstr : "", strerror(error));
	else
		asl_log(NULL, NULL, log_level, "%s\n", (fstr) ? fstr : "");
#else // USE_ASL_LOGGING
	if (log_level == ASL_LEVEL_ERR)
		log_level = LOG_ERR;
	else
		log_level = LOG_DEBUG;
	
	if (error == -1)
		syslog(log_level, "%s: syserr = %s\n", (fstr) ? fstr : "", strerror(errno));
	else if (error)
		syslog(log_level, "%s: syserr = %s\n", (fstr) ? fstr : "", strerror(error));
	else
		syslog(log_level, "%s\n", (fstr) ? fstr : "");
#endif // USE_ASL_LOGGING
	if (fstr)
		free(fstr);
}

/*
 * first read ~/.smbrc, next try to merge SMB_CFG_FILE - 
 */
struct rcfile * smb_open_rcfile(int NoUserPreferences)
{
	struct rcfile * smb_rc = NULL;
	char *home = NULL, *fn;
	int error;
	int fnlen;

	if (! NoUserPreferences)
		home = getenv("HOME");
		
	if (home) {
		fnlen = strlen(home) + strlen(SMB_CFG_LOCAL_FILE) + 1;
		fn = malloc(fnlen);
		snprintf(fn, fnlen, "%s%s", home, SMB_CFG_LOCAL_FILE);		
		error = rc_open(fn, "r", &smb_rc);
			/* Used for debugging bad configuration files. */
		if (error && (error != ENOENT) )
			smb_log_info("%s: Can't open %s: %s ", error, ASL_LEVEL_DEBUG, __FUNCTION__, fn, strerror(errno));
		free(fn);
	}
	fn = SMB_CFG_FILE;
	error = rc_merge(fn, &smb_rc);
	/* Used for debugging bad configuration files. */
	if (error && (error != ENOENT) )
		smb_log_info("%s: Can't open %s: %s ", error, ASL_LEVEL_DEBUG, __FUNCTION__, fn, strerror(errno));
	
	fn = SMB_GCFG_FILE;
	error = rc_merge(fn, &smb_rc);
	/* Used for debugging bad configuration files. */
	if (error && (error != ENOENT) )
		smb_log_info("%s: Can't open %s: %s ", error, ASL_LEVEL_DEBUG, __FUNCTION__, fn, strerror(errno));
	
	return smb_rc;
}

void
smb_simplecrypt(char *dst, const char *src)
{
	int ch, pos;

	*dst++ = '$';
	*dst++ = '$';
	*dst++ = '1';
	pos = 27;
	while (*src) {
		ch = *src++;
		if (isascii(ch))
		    ch = (isupper(ch) ? ('A' + (ch - 'A' + 13) % 26) :
			  islower(ch) ? ('a' + (ch - 'a' + 13) % 26) : ch);
		ch ^= pos;
		pos += 13;
		sprintf(dst, "%02x", ch);
		dst += 2;
	}
	*dst = 0;
}

int
smb_simpledecrypt(char *dst, const char *src)
{
	char *ep, hexval[3];
	int len, ch, pos;

	if (strncmp(src, "$$1", 3) != 0)
		return EINVAL;
	src += 3;
	len = strlen(src);
	if (len & 1)
		return EINVAL;
	len /= 2;
	hexval[2] = 0;
	pos = 27;
	while (len--) {
		hexval[0] = *src++;
		hexval[1] = *src++;
		ch = strtoul(hexval, &ep, 16);
		if (*ep != 0)
			return EINVAL;
		ch ^= pos;
		pos += 13;
		if (isascii(ch))
		    ch = (isupper(ch) ? ('A' + (ch - 'A' + 13) % 26) :
			  islower(ch) ? ('a' + (ch - 'a' + 13) % 26) : ch);
		*dst++ = ch;
	}
	*dst = 0;
	return 0;
}

/*
 * Check to make sure all the required code pages are load. Remember if we load by hand
 * some of our code pages may not be load. Call load_kext to load them.
 */
static int need_codepages()
{
	struct iconv_cspair_info csi[6];
	size_t olen = sizeof(csi);
	int ii;
	
	bzero(&csi, sizeof(csi));
	if (sysctlbyname("net.smb.fs.iconv.cslist", &csi, &olen, NULL, 0) == -1) {
		if (errno != ENOMEM) {
			smb_log_info("%s: sysctlbyname failed", -1, ASL_LEVEL_DEBUG, __FUNCTION__);
			return TRUE;
		}
	}
	for (ii=0; ii < 6; ii++) {
		if ((csi[ii].cs_id == 0) || ( csi[ii].cs_version != ICONV_CSPAIR_INFO_VER))
			return TRUE;
	}
	
	return FALSE;
}

/*
 * Currently we only support code page CP437, if we decide to support others they will need to be loaded by
 * the load_smbfs application. The codepage value will hold the name of said code page. 
 */
int smb_load_library(char * codepage)
{
	struct vfsconf vfc;
	int error = 0;
	static mach_port_t mp = MACH_PORT_NULL;
	
	setlocale(LC_CTYPE, "");
	/*
	 * They are not changing code pages, we are already loaded, and we were not loaded by hand
	 * then just get out nothing else to do here. We need to make sure our default code pages
	 * our load even if the kext is already load. This can happen when we are debugging our kext.
	 */
	if ((codepage == NULL) && (getvfsbyname(SMBFS_VFSNAME, &vfc) == 0) && (!need_codepages())) {
			return 0;	/* Already loaded and they are not changing the encoding */		
	}
	
	if (!mp) {
		error = bootstrap_look_up(bootstrap_port, SMBFS_LOAD_KEXT_BOOTSTRAP_NAME, &mp);
		if (error != KERN_SUCCESS) {
			smb_log_info("%s: bootstrap_look_up: %s", 0, ASL_LEVEL_DEBUG, __FUNCTION__, bootstrap_strerror(error));
			return error;
		}
	}
	
	if (codepage == NULL)
		codepage = SMBFS_DEFAULT_CODE_PAGE;
	
	error = load_kext(mp, SMBFS_VFSNAME, codepage);
	if (error != KERN_SUCCESS)
		smb_log_info("%s: load_kext: %s", 0, ASL_LEVEL_DEBUG, __FUNCTION__, bootstrap_strerror(error));
	
	return error;
}
