/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/os2/os2_serial.c,v 1.4 2002/05/31 18:46:02 dawes Exp $ */
/*
 * (c) Copyright 1999 by Holger Veit
 *			<Holger.Veit@gmd.de>
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
 * HOLGER VEIT  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 * Except as contained in this notice, the name of Holger Veit shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Holger Veit.
 *
 */
/* $XConsortium$ */

#define I_NEED_OS2_H
#include "X.h"
#include "Xmd.h"
#include "input.h"
#include "scrnintstr.h"

#include "compiler.h"

#define INCL_DOSDEVIOCTL
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

static int _set_baudrate(HFILE fd,int baud) 
{
	USHORT br = baud;
	ULONG plen;
	return DosDevIOCtl(fd,IOCTL_ASYNC,ASYNC_SETBAUDRATE,
			   (PULONG)&br,sizeof(br),&plen,NULL,0,NULL);
}

#pragma pack(1)
typedef struct _glinectl {
	UCHAR databits;
	UCHAR parity;
	UCHAR stopbits;
	UCHAR sendbrk;
} GLINECTL;
typedef struct _slinectl {
	UCHAR databits;
	UCHAR parity;
	UCHAR stopbits;
} SLINECTL;

#pragma pack()

static int _get_linectrl(HFILE fd,GLINECTL* linectrl)
{
	ULONG dlen;
	return DosDevIOCtl(fd,IOCTL_ASYNC,ASYNC_GETLINECTRL,
		NULL,0,NULL,linectrl,sizeof(GLINECTL),&dlen);
}

static int _set_linectl(HFILE fd,GLINECTL* linectl)
{
	ULONG plen;
	return DosDevIOCtl(fd,IOCTL_ASYNC,ASYNC_SETLINECTRL,
		(PULONG)&linectl,sizeof(SLINECTL),&plen,NULL,0,NULL);
}

static int _get_dcb(HFILE fd,DCBINFO* dcb) {

	ULONG dlen;
	return DosDevIOCtl(fd,IOCTL_ASYNC,ASYNC_GETDCBINFO,
		NULL,0,NULL,(PULONG)dcb,sizeof(DCBINFO),&dlen);
}

static int _set_dcb(HFILE fd,DCBINFO* dcb) 
{
	ULONG plen;
	return DosDevIOCtl(fd,IOCTL_ASYNC, ASYNC_SETDCBINFO,
			   (PULONG)dcb,sizeof(DCBINFO),&plen,NULL,0,NULL);
}

#pragma pack(1)
typedef struct comsize {
		USHORT nqueued;
		USHORT qsize;
} COMSIZE;
#pragma pack()

static int _get_nread(HFILE fd,ULONG* nread) 
{
	ULONG dlen;
	COMSIZE sz;
	APIRET rc = DosDevIOCtl(fd,IOCTL_ASYNC,ASYNC_GETINQUECOUNT,
			 NULL, 0, NULL, sz,sizeof(COMSIZE),&dlen);
	*nread = sz.nqueued;
	return rc ? -1 : 0;
}

int xf86OpenSerial (pointer options)
{
	APIRET rc;
	HFILE fd, i;
	ULONG action;
	GLINECTL linectl;

	char* dev = xf86FindOptionValue (options, "Device");
	xf86MarkOptionUsedByName (options, "Device");
	if (!dev) {
		xf86Msg (X_ERROR, "xf86OpenSerial: No Device specified.\n");
		return -1;
	}

	rc = DosOpen(dev, &fd, &action, 0, FILE_NORMAL, FILE_OPEN,
		     OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYNONE, NULL);
	if (rc) {
		xf86Msg (X_ERROR,
			 "xf86OpenSerial: Cannot open device %s, rc=%d.\n",
			 dev, rc);
		return -1;
	}

	/* check whether it is an async device */
	if (_get_linectrl(fd,&linectl)) {
		xf86Msg (X_WARNING,
			 "xf86OpenSerial: Specified device %s is not a tty\n",
			 dev);
		DosClose(fd);
		return -1;
	}

	/* set up default port parameters */
	_set_baudrate(fd, 9600);

	linectl.databits = 8;
	linectl.parity = 0;
	linectl.stopbits = 0;
	_set_linectl(fd, &linectl);

	if (xf86SetSerial (fd, options) == -1) {
		DosClose(fd);
		return -1;
	}

	return fd;
}

int xf86SetSerial (int fd, pointer options)
{
	APIRET rc;
	USHORT baud;
	ULONG plen,dlen;
	char *s;

	GLINECTL linectl;
	DCBINFO dcb;

	if ((s = xf86FindOptionValue (options, "BaudRate"))) {
		xf86MarkOptionUsedByName (options, "BaudRate");
		if ((rc = _set_baudrate(fd, atoi(s)))) {
			xf86Msg (X_ERROR,"Set Baudrate: %s, rc=%d\n", s, rc);
			return -1;
		}
	}

	/* get line parameters */
	if (DosDevIOCtl((HFILE)fd,IOCTL_ASYNC, ASYNC_GETLINECTRL,
			NULL,0,NULL,
			(PULONG)&linectl,sizeof(GLINECTL),&dlen)) return -1;

	if ((s = xf86FindOptionValue (options, "StopBits"))) {
		xf86MarkOptionUsedByName (options, "StopBits");
		switch (atoi (s)) {
		case 1:	linectl.stopbits = 0;
			break;
		case 2:	linectl.stopbits = 2;
			break;
		default: xf86Msg (X_ERROR,
				 "Invalid Option StopBits value: %s\n", s);
			return -1;
		}
	}

	if ((s = xf86FindOptionValue (options, "DataBits"))) {
		int db;
		xf86MarkOptionUsedByName (options, "DataBits");
		switch (db = atoi (s)) {
		case 5: case 6: case 7: case 8:
			linectl.databits = db;
			break;
		default: xf86Msg (X_ERROR,
				 "Invalid Option DataBits value: %s\n", s);
			return -1;
		}
	}

	if ((s = xf86FindOptionValue (options, "Parity"))) {
		xf86MarkOptionUsedByName (options, "Parity");
		if (xf86NameCmp (s, "Odd") == 0)
			linectl.parity = 1; /* odd */
		else if (xf86NameCmp (s, "Even") == 0)
			linectl.parity = 2; /* even */
		else if (xf86NameCmp (s, "None") == 0)
			linectl.parity = 0; /* none */
		else {
			xf86Msg (X_ERROR,
				 "Invalid Option Parity value: %s\n", s);
			return -1;
		}
	}

	/* set line parameters */
	if (_set_linectl(fd,&linectl)) return -1;

	if (xf86FindOptionValue (options, "Vmin"))
		xf86Msg (X_ERROR, "Vmin unsupported on this OS\n");

	if (xf86FindOptionValue (options, "Vtime"))
		xf86Msg (X_ERROR, "Vtime unsupported on this OS\n");

	/* get device parameters */
	if (_get_dcb(fd,&dcb)) return -1;

	if ((s = xf86FindOptionValue (options, "FlowControl"))) {
		xf86MarkOptionUsedByName (options, "FlowControl");
		if (xf86NameCmp (s, "XonXoff") == 0)
			dcb.fbFlowReplace |= 0x03;
		else if (xf86NameCmp (s, "None") == 0)
			dcb.fbFlowReplace &= ~0x03;
		else {
			xf86Msg (X_ERROR,
				 "Invalid Option FlowControl value: %s\n", s);
			return -1;
		}
	}

	if ((s = xf86FindOptionValue (options, "ClearDTR"))) {
		dcb.fbCtlHndShake &= ~0x03; /* DTR=0 */
		xf86MarkOptionUsedByName (options, "ClearDTR");
	}

	if ((s = xf86FindOptionValue (options, "ClearRTS"))) {
		dcb.fbFlowReplace &= ~0xc0; /* RTS=0 */
		xf86MarkOptionUsedByName (options, "ClearRTS");
	}

	/* set device parameters */
	return _set_dcb(fd,&dcb) ? -1 : 0;
}

int xf86ReadSerial (int fd, void *buf, int count)
{
	ULONG nread,nq;
	APIRET rc;

	/* emulate non-blocking read */
	if (_get_nread((HFILE)fd,&nq)) return -1;
	if (nq==0) return 0;
	if (nq < count) count = nq;

	rc = DosRead((HFILE)fd,(PVOID)buf,(ULONG)count,&nread);
	return rc ? -1 : (int)nread;
}

int xf86WriteSerial (int fd, const void *buf, int count)
{
	ULONG nwrite;
	APIRET rc = DosWrite((HFILE)fd,(PVOID)buf,(ULONG)count,&nwrite);
	return rc ? -1 : (int)nwrite;
}

int xf86CloseSerial (int fd)
{
	APIRET rc = DosClose((HFILE)fd);
	return rc ? -1 : 0;
}

int xf86WaitForInput (int fd, int timeout)
{
	APIRET rc;
	ULONG dlen,nq;

	do {
		if (_get_nread((HFILE)fd,&nq)) return -1;
		if (nq) return 1;

		DosSleep(10);
		timeout -= 10000; /* 10000 usec */
	} while (timeout > 0);

	return 0;
}

int xf86SerialSendBreak (int fd, int duration)
{
	USHORT data;
	ULONG dlen;
	APIRET rc;
	rc = DosDevIOCtl((HFILE)fd,IOCTL_ASYNC,ASYNC_SETBREAKON,
			NULL, 0, NULL,
			&data, sizeof(data), &dlen);
	if (rc)
		return -1;
	DosSleep(500);

	rc = DosDevIOCtl((HFILE)fd,IOCTL_ASYNC,ASYNC_SETBREAKOFF,
			NULL, 0, NULL,
			&data, sizeof(data), &dlen);
	return rc ? -1 : 0;
}

int xf86FlushInput(int fd)
{
	APIRET rc;
	UCHAR buf;
	ULONG nread,nq;

	if (_get_nread((HFILE)fd,&nq)) return -1;

	/* eat all chars in queue */
	while (nq) {
		rc = DosRead((HFILE)fd,&buf,1,&nread);
		if (rc) return -1;
		nq--;
	}
	return 0;
}

static struct states {
        int xf;
        int os;
} modemStates[] = {
        { XF86_M_DTR, 0x01 },
        { XF86_M_RTS, 0x02 },
        { XF86_M_CTS, 0x10 },
        { XF86_M_DSR, 0x20 },
        { XF86_M_RNG, 0x40 },
        { XF86_M_CAR, 0x80 },
};

static int numStates = sizeof(modemStates) / sizeof(modemStates[0]);

static int
xf2osState(int state)
{
        int i;
        int ret = 0;

        for (i = 0; i < numStates; i++)
                if (state & modemStates[i].xf)
                        ret |= modemStates[i].os;
        return ret;
}

static int
os2xfState(int state)
{
        int i;
        int ret = 0;

        for (i = 0; i < numStates; i++)
                if (state & modemStates[i].os)
                        ret |= modemStates[i].xf;
        return ret;
}

static int
getOsStateMask(void)
{
        int i;
        int ret = 0;
        for (i = 0; i < numStates; i++)
                ret |= modemStates[i].os;
        return ret;
}

static int osStateMask = 0;

static 
int _get_modem_state(int fd,ULONG* state) 
{
	ULONG state1,len;

	if (DosDevIOCtl((HFILE)fd,IOCTL_ASYNC,ASYNC_GETMODEMOUTPUT,
		NULL,0,NULL, state, sizeof(BYTE), &len) != 0 ||
	    DosDevIOCtl((HFILE)fd,IOCTL_ASYNC,ASYNC_GETMODEMINPUT,
		NULL,0,NULL, &state1, sizeof(BYTE), &len) != 0)
		return -1;
	*state |= state1;
	*state &= 0xff;
	return 0;	
}

static 
int _set_modem_state(int fd,ULONG state,ULONG mask) 
{
	int len;
	struct {
		BYTE onmask;
		BYTE offmask;
	} modemctrl;
	modemctrl.onmask = state;
	modemctrl.offmask = mask;

	if (DosDevIOCtl((HFILE)fd,IOCTL_ASYNC,ASYNC_SETMODEMCTRL,
		NULL,0,NULL, (PULONG)&modemctrl, sizeof(modemctrl), &len) != 0)
		return -1;
	else
		return 0;
}

int
xf86SetSerialModemState(int fd, int state)
{
        ULONG s;

        if (fd < 0)
                return -1;

        /* Don't try to set parameters for non-tty devices. */
        if (!isatty(fd))
                return 0;

        if (!osStateMask)
                osStateMask = getOsStateMask();

        state = xf2osState(state);

	if (_get_modem_state(fd,&s) != 0)
		return -1;

        s &= ~osStateMask;
        s |= state;

	return _set_modem_state(fd,s,0x03);
}

int
xf86GetSerialModemState(int fd)
{
        ULONG s;

        if (fd < 0)
                return -1;

        /* Don't try to set parameters for non-tty devices. */
        if (!isatty(fd))
                return 0;

	if (_get_modem_state(fd,&s) != 0)
		return -1;

        return os2xfState(s);
}

int
xf86SerialModemSetBits(int fd, int bits)
{
        int ret;
        int s;

        if (fd < 0)
                return -1;

        /* Don't try to set parameters for non-tty devices. */
        if (!isatty(fd))
                return 0;

        s = xf2osState(bits);
	return _set_modem_state(fd,s,0x03);
}

int
xf86SerialModemClearBits(int fd, int bits)
{
        int ret;
        int s;

        if (fd < 0)
                return -1;

        /* Don't try to set parameters for non-tty devices. */
        if (!isatty(fd))
                return 0;

        s = xf2osState(bits);
	return _set_modem_state(fd, 0, ~s & 0xff);
}

int
xf86SetSerialSpeed (int fd, int speed)
{
	if (fd < 0)
		return -1;

        /* Don't try to set parameters for non-tty devices. */
        if (!isatty(fd))
                return 0;

	return _set_baudrate(fd,speed);
}
