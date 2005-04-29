/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/lnx_mouse.c,v 1.3 2004/02/13 23:58:48 dawes Exp $ */

/*
 * Copyright 1999 by The XFree86 Project, Inc.
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

#include "X.h"
#include "xf86.h"
#include "xf86Xinput.h"
#include "xf86OSmouse.h"
#include "xf86_OSlib.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static int
SupportedInterfaces(void)
{
    return MSE_SERIAL | MSE_BUS | MSE_PS2 | MSE_XPS2 | MSE_AUTO;
}

static const char *
DefaultProtocol(void)
{
    return "Auto";
}

#define DEFAULT_MOUSE_DEV		"/dev/mouse"
#define DEFAULT_PS2_DEV			"/dev/psaux"
#define DEFAULT_GPM_DATA_DEV		"/dev/gpmdata"
#define DEFAULT_GPM_CTL_DEV		"/dev/gpmdata"

static const char *mouseDevs[] = {
	DEFAULT_MOUSE_DEV,
	DEFAULT_PS2_DEV,
	DEFAULT_GPM_DATA_DEV,
	NULL
};

typedef enum {
	MOUSE_PROTO_UNKNOWN = 0,
	MOUSE_PROTO_SERIAL,
	MOUSE_PROTO_PS2,
	MOUSE_PROTO_MSC,
	MOUSE_PROTO_GPM
} protocolTypes;

static struct {
	protocolTypes proto;
	const char *name;
} devproto[] = {
	{ MOUSE_PROTO_UNKNOWN,	NULL },
	{ MOUSE_PROTO_PS2,	"PS/2" },
	{ MOUSE_PROTO_MSC,	"MouseSystems" },
	{ MOUSE_PROTO_GPM,	"GPM" }
};

static const char *
FindDevice(InputInfoPtr pInfo, const char *protocol, int flags)
{
    int fd = -1;
    const char **pdev;

    for (pdev = mouseDevs; *pdev; pdev++) {
	SYSCALL (fd = open(*pdev, O_RDWR | O_NONBLOCK | O_EXCL));
	if (fd == -1) {
#ifdef DEBUG
	    ErrorF("Cannot open %s (%s)\n", *pdev, strerror(errno));
#endif
	} else
	    break;
    }

    if (*pdev) {
	close(fd);
	/* Set the Device option. */
	pInfo->conf_idev->commonOptions =
	    xf86AddNewOption(pInfo->conf_idev->commonOptions, "Device", *pdev);
	xf86Msg(X_INFO, "%s: Setting Device option to \"%s\"\n",
		pInfo->name, *pdev);
    }

    return *pdev;
}

static const char *
GuessProtocol(InputInfoPtr pInfo, int flags)
{
    int fd = -1;
    const char *dev;
    char *realdev;
    struct stat sbuf;
    int i;
    int proto = MOUSE_PROTO_UNKNOWN;

    dev = xf86SetStrOption(pInfo->conf_idev->commonOptions, "Device", NULL);
    if (!dev) {
#ifdef DEBUG
	ErrorF("xf86SetStrOption failed to return the device name\n");
#endif
	return NULL;
    }
    /* Look at the device name to guess the protocol. */
    realdev = NULL;
    if (strcmp(dev, DEFAULT_MOUSE_DEV) == 0) {
	if (lstat(dev, &sbuf) != 0) {
#ifdef DEBUG
	    ErrorF("lstat failed for %s (%s)\n", dev, strerror(errno));
#endif
	    return NULL;
	}
	if (S_ISLNK(sbuf.st_mode)) {
	    realdev = xnfalloc(PATH_MAX + 1);
	    i = readlink(dev, realdev, PATH_MAX);
	    if (i <= 0) {
#ifdef DEBUG
		ErrorF("readlink failed for %s (%s)\n", dev, strerror(errno));
#endif
		xfree(realdev);
		return NULL;
	    }
	    realdev[i] = '\0';
	}
    }
    if (!realdev)
	realdev = xnfstrdup(dev);
    else {
	/* If realdev doesn't contain a '/' then prepend "/dev/" */
	if (!strchr(realdev, '/')) {
	    char *tmp = xnfalloc(strlen(realdev) + 5 + 1);
	    sprintf(tmp, "/dev/%s", realdev);
	    xfree(realdev);
	    realdev = tmp;
	}
    }

    if (strcmp(realdev, DEFAULT_PS2_DEV) == 0)
	proto = MOUSE_PROTO_PS2;
    else if (strcmp(realdev, DEFAULT_GPM_DATA_DEV) == 0)
	proto = MOUSE_PROTO_MSC;
    else if (strcmp(realdev, DEFAULT_GPM_CTL_DEV) == 0)
	proto = MOUSE_PROTO_GPM;
    xfree(realdev);
    /*
     * If the protocol can't be guessed from the device name,
     * try to characterise it.
     */
    if (proto == MOUSE_PROTO_UNKNOWN) {
	SYSCALL (fd = open(dev, O_RDWR | O_NONBLOCK | O_EXCL));
	if (isatty(fd)) {
	    /* Serial PnP has already failed, so give up. */
	} else {
	    if (fstat(fd, &sbuf) != 0) {
#ifdef DEBUG
		ErrorF("fstat failed for %s (%s)\n", dev, strerror(errno));
#endif
		close(fd);
		return NULL;
	    }
	    if (S_ISFIFO(sbuf.st_mode)) {
		/* Assume GPM data in MSC format. */
		proto = MOUSE_PROTO_MSC;
	    } else {
		/* Default to PS/2 */
		proto = MOUSE_PROTO_PS2;
	    }
	}
	close(fd);
    }
    if (proto == MOUSE_PROTO_UNKNOWN) {
	xf86Msg(X_ERROR, "%s: GuessProtocol: Cannot find mouse protocol.\n",
		pInfo->name);
	return NULL;
    } else {
	for (i = 0; i < sizeof(devproto)/sizeof(devproto[0]); i++) {
	    if (devproto[i].proto == proto) {
		xf86Msg(X_INFO,
			"%s: GuessProtocol: "
			"setting mouse protocol to \"%s\"\n", 
			pInfo->name, devproto[i].name);
		return devproto[i].name;
	    }
	}
    }
    return NULL;
}

OSMouseInfoPtr
xf86OSMouseInit(int flags)
{
    OSMouseInfoPtr p;

    p = xcalloc(sizeof(OSMouseInfoRec), 1);
    if (!p)
	return NULL;
    p->SupportedInterfaces = SupportedInterfaces;
    p->DefaultProtocol = DefaultProtocol;
    p->FindDevice = FindDevice;
    p->GuessProtocol = GuessProtocol;
    return p;
}

