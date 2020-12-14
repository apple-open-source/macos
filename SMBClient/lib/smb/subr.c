/*
 * Copyright (c) 2000, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2011 Apple Inc. All rights reserved.
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
#include <IOKit/kext/KextManager.h>

#include <netsmb/netbios.h>
#include <netsmb/smb_lib.h>
#include <netsmb/nb_lib.h>

#include <smbfs/smbfs.h>

#define CFENVFORMATSTRING "__CF_USER_TEXT_ENCODING=0x%X:0:0"

void smb_ctx_hexdump(const char *func, const char *s, unsigned char *buf, size_t inlen)
{
	char printstr[512];
	size_t maxlen;
	char *strPtr;
    int32_t addr, len = (int32_t)inlen;
    int32_t i;
	
	os_log_error(OS_LOG_DEFAULT, "%s: %s %p length %ld",  func, s, buf, inlen);
	if (buf == NULL) {
		return;
	}
    addr = 0;
    while( addr < len )
    {
		strPtr = printstr;
		maxlen = sizeof(printstr);
        strPtr += snprintf(strPtr, maxlen, "%6.6x - " , addr );
		maxlen -= (strPtr - printstr);
        for( i=0; i<16; i++ )
        {
            if( addr+i < len )
				strPtr += snprintf(strPtr, maxlen, "%2.2x ", buf[addr+i]);
            else
 				strPtr += snprintf(strPtr, maxlen, "   ");
			maxlen -= (strPtr - printstr);
       }
		strPtr += snprintf(strPtr, maxlen, " \"");
		maxlen -= (strPtr - printstr);
        for( i=0; i<16; i++ )
        {
            if( addr+i < len )
            {
                if(( buf[addr+i] > 0x19 ) && ( buf[addr+i] < 0x7e ) )
					strPtr += snprintf(strPtr, maxlen, "%c", buf[addr+i] );
                else
					strPtr += snprintf(strPtr, maxlen, ".");
				maxlen -= (strPtr - printstr);
            }
        }
		os_log_error(OS_LOG_DEFAULT, "%s", printstr);
        addr += 16;
    }
	os_log_error(OS_LOG_DEFAULT, " ");
}

/*
 * Load our kext
 */
int smb_load_library(void)
{
	struct vfsconf vfc;
	kern_return_t status;
	
	setlocale(LC_CTYPE, "");
	if (getvfsbyname(SMBFS_VFSNAME, &vfc) != 0) {
		/* Need to load the kext */
		status = KextManagerLoadKextWithIdentifier(CFSTR("com.apple.filesystems.smbfs") ,NULL);
        if (status != KERN_SUCCESS) {
			os_log_error(OS_LOG_DEFAULT, "Loading com.apple.filesystems.smbfs status = 0x%x", 
						 status);
			return EIO;
        }
	}
	return 0;
}
