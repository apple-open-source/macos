/* $XFree86: xc/lib/XTrap/XEPrInfo.c,v 1.1 2001/11/02 23:29:27 dawes Exp $ */
/*****************************************************************************
Copyright 1987, 1988, 1989, 1990, 1991, 1994 by Digital Equipment Corp., 
Maynard, MA

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*****************************************************************************/
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/extensions/xtraplib.h>
#include <X11/extensions/xtraplibp.h>

#ifndef TRUE
# define TRUE 1L
#endif
#ifndef FALSE
# define FALSE 0L
#endif

void XEPrintRelease( FILE *ofp, XETrapGetAvailRep *pavail)
{

    fprintf(ofp,"\tRelease:   %d.%d-%d\n", XETrapGetAvailRelease(pavail),
        XETrapGetAvailVersion(pavail), XETrapGetAvailRevision(pavail));
}
void XEPrintTkRelease( FILE *ofp, XETC *tc)
{
    fprintf(ofp,"\tRelease:   %d.%d-%d\n", XEGetRelease(tc), XEGetVersion(tc),
        XEGetRevision(tc));
}

void XEPrintPlatform( FILE *ofp, XETrapGetAvailRep *pavail)
{
    fprintf(ofp,"\tPlatform:  %s (0x%02x)\n",
        XEPlatformIDToString(XETrapGetAvailPFIdent(pavail)), 
        (int)XETrapGetAvailPFIdent(pavail));
}

void XEPrintAvailFlags( FILE *ofp, XETrapGetAvailRep *pavail)
{

    CARD8 f[4L];

    XETrapGetAvailFlags(pavail,f);
    fprintf(ofp,"\tFlags: ");
    if (XETrapGetAvailFlagTimestamp(pavail)) fputs("Timestamps ", ofp);
    if (XETrapGetAvailFlagCmd(pavail)) fputs("CmdKey ", ofp);
    if (XETrapGetAvailFlagCmdKeyMod(pavail)) fputs("CmdKeyMod ", ofp); 
    if (XETrapGetAvailFlagRequest(pavail)) fputs("Requests ", ofp);
    if (XETrapGetAvailFlagEvent(pavail)) fputs("Events ", ofp);
    if (XETrapGetAvailFlagMaxPacket(pavail)) fputs("MaxPkt ", ofp);
    if (XETrapGetAvailFlagStatistics(pavail)) fputs("Statistics ", ofp);
    if (XETrapGetAvailFlagWinXY(pavail)) fputs("WinXY ", ofp);
    if (XETrapGetAvailFlagCursor(pavail)) fputs("Cursor ", ofp);
    if (XETrapGetAvailFlagXInput(pavail)) fputs("XInput ", ofp);
    if (XETrapGetAvailFlagVecEvt(pavail)) fputs("Vect_Evnts ", ofp);
    if (XETrapGetAvailFlagColorReplies(pavail)) fputs("ColorRep ", ofp);
    if (XETrapGetAvailFlagGrabServer(pavail)) fputs("GrabServer ", ofp);
    fprintf(ofp," (0x%02x%02x%02x%02x)\n", f[0], f[1], f[2], f[3]);
}

void XEPrintAvailPktSz( FILE *ofp, XETrapGetAvailRep *pavail)
{

    fprintf(ofp,"\tMax Packet Size: %d\n", XETrapGetAvailMaxPktSize(pavail));
}
void XEPrintStateFlags( FILE *ofp, XETrapGetCurRep *pcur)
{

    CARD8   f[2];
    XETrapGetCurSFlags(pcur, f);
    fputs("\tFlags: ",ofp); 
    if (BitIsTrue(f,XETrapTrapActive)) fputs("I/O Active ", ofp);
    fprintf(ofp," (0x%02x%02x)\n", f[0], f[1]);    
}

void XEPrintMajOpcode( FILE *ofp, XETrapGetAvailRep *pavail)
{

    fprintf(ofp,"\tMajor Opcode:  %d\n", (int)XETrapGetAvailOpCode(pavail));
}
void XEPrintCurXY( FILE *ofp, XETrapGetAvailRep *pavail)
{

    fprintf(ofp,"\tCurrent (x,y):  (%d,%d)\n", XETrapGetCurX(pavail), 
        XETrapGetCurY(pavail));
}

void XEPrintTkFlags( FILE *ofp, XETC *tc)
{

    CARD8   f[2];
    XETrapGetTCLFlags(tc, f);
    fputs("\tFlags: ",ofp); 
    if (XETrapGetTCFlagDeltaTimes(tc)) fputs("Delta Times ", ofp);
    if (XETrapGetTCFlagTrapActive(tc)) fputs("Trap Active ", ofp);
    fprintf(ofp," (0x%02x%02x)\n", f[0], f[1]);    
}

void XEPrintLastTime( FILE *ofp, XETC *tc)
{

    fprintf(ofp,"\tLast Relative Time:  %d\n", (int)XETrapGetTCTime(tc));
}

void XEPrintCfgFlags( FILE *ofp, XETrapGetCurRep *pcur)
{

    CARD8 f[4L];

    XETrapGetCurCFlags(pcur,data,f);
    fprintf(ofp,"\tFlags: ");
    if (XETrapGetCurFlagTimestamp(pcur,data)) fputs("Timestamps ", ofp);
    if (XETrapGetCurFlagCmd(pcur,data)) fputs("CmdKey ", ofp);
    if (XETrapGetCurFlagCmdKeyMod(pcur,data)) fputs("CmdKeyMod ", ofp);
    if (XETrapGetCurFlagRequest(pcur,data)) fputs("Requests ", ofp);
    if (XETrapGetCurFlagEvent(pcur,data)) fputs("Events ", ofp);
    if (XETrapGetCurFlagMaxPacket(pcur,data)) fputs("MaxPkt ", ofp);
    if (XETrapGetCurFlagStatistics(pcur,data)) fputs("Statistics ", ofp);
    if (XETrapGetCurFlagWinXY(pcur,data)) fputs("WinXY ", ofp);
    if (XETrapGetCurFlagCursor(pcur,data)) fputs("Cursor ", ofp);
    if (XETrapGetCurFlagXInput(pcur,data)) fputs("XInput ", ofp);
    if (XETrapGetCurFlagColorReplies(pcur,data)) fputs("ColorReplies ", ofp);
    if (XETrapGetCurFlagGrabServer(pcur,data)) fputs("GrabServer ", ofp);
    fprintf(ofp," (0x%02x%02x%02x%02x)\n", f[0], f[1], f[2], f[3]);
}

void XEPrintRequests( FILE *ofp, XETrapGetCurRep *pcur)
{

    long i;
    fprintf(ofp,"\tX Requests:  ");
    for (i=0L; i<=XETrapMaxRequest-1; i++)
    {   /* Not using the macro cause we're doing things
         * a byte at a time rather than a bit.
         */
        fprintf(ofp,"%02x ", pcur->config.flags.req[i]);
        if ((i+1L)%4L == 0L)
        { 
            fprintf(ofp,"  "); 
        }
        if ((i+1L)%16L == 0L)
        { 
            fprintf(ofp,"\n\t\t     "); 
        }
    }
    fprintf(ofp,"\n");
}

void XEPrintEvents( FILE *ofp, XETrapGetCurRep *pcur)
{

    int i;
    fprintf(ofp,"\tX Events:  ");
    for (i=0L; i<XETrapMaxEvent; i++)
    {   /* Not using the macro cause we're doing things
         * a byte at a time rather than a bit.
         */
        fprintf(ofp,"%02x ", pcur->config.flags.event[i]);
        if ((i+1L)%4L == 0L)
        { 
            fprintf(ofp,"  "); 
        }
        if ((i+1L)%16L == 0L)
        { 
            fprintf(ofp,"\n\t\t     "); 
        }
    }
    fprintf(ofp,"\n");
}

void XEPrintCurPktSz( FILE *ofp, XETrapGetCurRep *pcur)
{

    fprintf(ofp,"\tMax Packet Size: %d\n", XETrapGetCurMaxPktSize(pcur));
}

void XEPrintCmdKey( FILE *ofp, XETrapGetCurRep *pcur)
{

    fprintf(ofp,"\tcmd_key: 0x%02x\n", XETrapGetCurCmdKey(pcur));
}

void XEPrintEvtStats( FILE *ofp, XETrapGetStatsRep *pstats, XETC *tc)
{

    int i;
    fprintf(ofp,"\tX Events:\n");
    for (i=0; i<XETrapCoreEvents; i++)
    {   
        if (XETrapGetStatsEvt(pstats,i))
        {
            fprintf(ofp,"\t   %-20s :  %d\n", XEEventIDToString(i,tc),
                (int)XETrapGetStatsEvt(pstats,i));
        }
    }
    fprintf(ofp,"\n");
}

void XEPrintReqStats( FILE *ofp, XETrapGetStatsRep *pstats, XETC *tc)
{

    int i;
    fprintf(ofp,"\tX Requests:\n");
    for (i=0L; i<256L; i++)
    {   
        if (XETrapGetStatsReq(pstats,i))
        {
            fprintf(ofp,"\t   %-20s :  %d\n", XERequestIDToString(i,tc),
                (int)XETrapGetStatsReq(pstats,i));
        }
    }
    fprintf(ofp,"\n");
}


void XEPrintAvail( FILE *ofp, XETrapGetAvailRep *pavail)
{

    fprintf(ofp,"Available Information:\n");
    XEPrintRelease(ofp, pavail);
    XEPrintPlatform(ofp, pavail);
    XEPrintMajOpcode(ofp, pavail);
    XEPrintAvailFlags(ofp, pavail);
    XEPrintAvailPktSz(ofp, pavail);
    XEPrintCurXY(ofp, pavail);
    return;
}

void XEPrintTkState( FILE *ofp, XETC *tc)
{

    fprintf(ofp,"Toolkit State:\n");
    XEPrintTkFlags(ofp, tc);
    XEPrintLastTime(ofp, tc);
    XEPrintTkRelease(ofp, tc);
}

void XEPrintCurrent( FILE *ofp, XETrapGetCurRep *pcur)
{

    fprintf(ofp,"Current State:\n");
    XEPrintStateFlags(ofp, pcur);
    fprintf(ofp,"Current Config:\n");
    XEPrintCfgFlags(ofp, pcur);
    XEPrintRequests(ofp, pcur);
    XEPrintEvents(ofp, pcur);
    XEPrintCurPktSz(ofp, pcur);
    XEPrintCmdKey(ofp, pcur);
}

void XEPrintStatistics( FILE *ofp, XETrapGetStatsRep *pstats, XETC *tc)
{

    fprintf(ofp,"Statistics:\n");
    XEPrintEvtStats(ofp, pstats, tc);
    XEPrintReqStats(ofp, pstats, tc);
}

