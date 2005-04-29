/* $XFree86: xc/programs/Xserver/hw/xfree86/parser/write.c,v 1.19 2004/02/13 23:58:50 dawes Exp $ */
/* 
 * 
 * Copyright (c) 1997  Metro Link Incorporated
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * Except as contained in this notice, the name of the Metro Link shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Metro Link.
 * 
 */
/*
 * Copyright (c) 1997-2003 by The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/* View/edit this file with tab stops set to 4 */

#include "xf86Parser.h"
#include "xf86tokens.h"
#include "Configint.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#if ((defined(sun) && !defined(SVR4)) || defined(macII)) && !defined(__GLIBC__)
#ifndef strerror
extern char *sys_errlist[];
extern int sys_nerr;
#define strerror(n) \
	(((n) >= 0 && (n) < sys_nerr) ? sys_errlist[n] : "unknown error")
#endif
#endif

#if defined(SVR4) || defined(__linux__) || defined(CSRG_BASED)
#define HAS_SAVED_IDS_AND_SETEUID
#endif
#if defined(__UNIXOS2__) || defined(WIN32)
#define HAS_NO_UIDS
#endif

#ifdef HAS_NO_UIDS
#define doWriteConfigFile xf86writeConfigFile
#define Local /**/
#else
#define Local static
#endif

Local int
doWriteConfigFile (const char *filename, XF86ConfigPtr cptr)
{
	FILE *cf;

	if ((cf = fopen (filename, "w")) == NULL)
	{
		return 0;
	}

	if (cptr->conf_comment)
		fprintf (cf, "%s\n", cptr->conf_comment);

	xf86printLayoutSection (cf, cptr->conf_layout_lst);

	fprintf (cf, "Section \"Files\"\n");
	xf86printFileSection (cf, cptr->conf_files);
	fprintf (cf, "EndSection\n\n");

	fprintf (cf, "Section \"Module\"\n");
	xf86printModuleSection (cf, cptr->conf_modules);
	fprintf (cf, "EndSection\n\n");

	xf86printVendorSection (cf, cptr->conf_vendor_lst);

	xf86printServerFlagsSection (cf, cptr->conf_flags);

	xf86printInputSection (cf, cptr->conf_input_lst);

	xf86printVideoAdaptorSection (cf, cptr->conf_videoadaptor_lst);

	xf86printModesSection (cf, cptr->conf_modes_lst);

	xf86printMonitorSection (cf, cptr->conf_monitor_lst);

	xf86printDeviceSection (cf, cptr->conf_device_lst);

	xf86printScreenSection (cf, cptr->conf_screen_lst);

	xf86printDRISection (cf, cptr->conf_dri);

	fclose(cf);
	return 1;
}

#ifndef HAS_NO_UIDS

int
xf86writeConfigFile (const char *filename, XF86ConfigPtr cptr)
{
	int ret;

#if !defined(HAS_SAVED_IDS_AND_SETEUID)
	int pid, p;
	int status;
	void (*csig)(int);
#else
	int ruid, euid;
#endif

	if (getuid() != geteuid())
	{

#if !defined(HAS_SAVED_IDS_AND_SETEUID)
		/* Need to fork to change ruid without loosing euid */
#ifdef SIGCHLD
		csig = signal(SIGCHLD, SIG_DFL);
#endif
		switch ((pid = fork()))
		{
		case -1:
			ErrorF("xf86writeConfigFile(): fork failed (%s)\n",
					strerror(errno));
			return 0;
		case 0: /* child */
			setuid(getuid());
			ret = doWriteConfigFile(filename, cptr);
			exit(ret);
			break;
		default: /* parent */
			do
			{
				p = waitpid(pid, &status, 0);
			} while (p == -1 && errno == EINTR);
		}
#ifdef SIGCHLD
		signal(SIGCHLD, csig);
#endif
		if (p != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0)
			return 1;	/* success */
		else
			return 0;

#else /* HAS_SAVED_IDS_AND_SETEUID */

		ruid = getuid();
		euid = geteuid();

		if (seteuid(ruid) == -1)
		{
			ErrorF("xf86writeConfigFile(): seteuid(%d) failed (%s)\n",
					ruid, strerror(errno));
			return 0;
		}
		ret = doWriteConfigFile(filename, cptr);

		if (seteuid(euid) == -1)
		{
			ErrorF("xf86writeConfigFile(): seteuid(%d) failed (%s)\n",
					euid, strerror(errno));
		}
		return ret;

#endif /* HAS_SAVED_IDS_AND_SETEUID */

	}
	else
	{
		return doWriteConfigFile(filename, cptr);
	}
}

#endif /* !HAS_NO_UIDS */
