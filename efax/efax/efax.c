#define Copyright         "Copyright 1999 Ed Casas"

#define Version		  "efax v 0.9a-001114"

/*
    Copyright (C) 1999  Ed Casas

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Please contact the author if you wish to use efax or efix in
    ways not covered by the GNU GPL.

    You may contact the author by e-mail at: edc@cce.com.

*/

const char *Usage =
  "Usage:\n"
  "  %s [ option ]... [ -t num [ file... ] ]\n"
  "Options:\n"
  "  -a str  use command ATstr to answer\n"
  "  -c cap  set modem and receive capabilites to cap\n"
  "  -d dev  use modem on device dev\n"
  "  -e cmd  exec \"/bin/sh -c cmd\" for voice calls\n"
  "  -f fnt  use (PBM) font file fnt for headers\n"
  "  -g cmd  exec \"/bin/sh -c cmd\" for data calls\n"
  "  -h hdr  use page header hdr (use %%d's for current page/total pages)\n"
  "  -i str  send modem command ATstr at start\n"
  "  -j str  send modem command ATstr after set fax mode\n"
  "  -k str  send modem command ATstr when done\n"
  "  -l id   set local identification to id\n"
  "  -o opt  use protocol option opt:\n"
  "      0     use class 2.0 instead of class 2 modem commands\n"
  "      1     use class 1 modem commands\n"
  "      2     use class 2 modem commands\n"
  "      a     if first [data mode] answer attempt fails retry as fax\n"
  "      e     ignore errors in modem initialization commands\n"
  "      f     use virtual flow control\n"
  "      h     use hardware flow control\n"
  "      l     halve lock file polling interval\n"
  "      n     ignore page retransmission requests\n"
  "      r     do not reverse received bit order for Class 2 modems\n"  
  "      x     use XON instead of DC2 to trigger reception\n"
  "      z     add 100 ms to pause before each modem comand (cumulative)\n"
  "  -q ne   ask for retransmission if more than ne errors per page\n"
  "  -r pat  save received pages into files pat.001, pat.002, ... \n"
  "  -s      share (unlock) modem device while waiting for call\n"
  "  -v lvl  print messages of type in string lvl (ewinchamr)\n"
  "  -w      don't answer phone, wait for OK or CONNECT instead\n"
  "  -x fil  use uucp-style lock file fil\n"
  "Commands:\n"
  "  -t      dial num and send fax image files file... \n"
  ;

#include <ctype.h>		/* ANSI C */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/signal.h>

#include "efaxio.h"		/* EFAX */
#include "efaxlib.h"
#include "efaxmsg.h"
#include "efaxos.h"

/* constants... */

			    /* delays and timeouts (t/o), in deciseconds */
#define T1 350		    /* T.30 T1 - waiting for DIS/DCS before Phase B */
#define T2 60		    /* T.30 T2 - waiting for frame in Phase B */
#define T3S 30		    /* T.30 response timeout (not T3) */
#define T4 30		    /* T.30 T4 - between [re]transmissions of DIS */

#define TCFSECS 1.5	    /* TCF duration (seconds, nominally 1.5) */

#define TMOD 55             /* T.30 pause between v.21&v.29, 75-20 ms */
#define MODDLY "5"	    /* same as above, a string */

#define TO_A     1200	    /* dial/answer (Phase A) - modem may t/o first */
#define TO_ABRT  20	    /* max delay after sending abort sequence */
#define TO_CHAR  51	    /* per data character (max FILL length) */
#define TO_DATAF 80	    /* software adaptive answer data connect t/o */
#define TO_DRAIN_H 136	    /* minimum HDLC buffer drain time (4k/300cps) */
#define TO_DRAIN_D 300	    /* minimum data buffer drain time */
#define TO_FT    31	    /* max delay after +F[TR][MH] command */
#define TO_RTCMD 20	    /* return to command mode after DLE-ETX (rx) */

#define TO_C2B    450	    /* Class 2 DIS to CONNECT:(DCS+TCF+CFR)xretries */
#define TO_C2X    20	    /* Class 2 wait for XON: 2/5 of 5s timeout */
#define TO_C2PP   200	    /* Class 2 wait for ppr: (ppm+ppr)x3retries + 2 */
#define TO_C2R    600	    /* Class 2 receive: (TCF+FTT)x11 retrains + 5 */
#define TO_C2EOR  160	    /* Class 2 end of data rx (4 retrans x 4 s) */

#define ANSCMD  "A"	    /* default modem command to answer calls */
#define DCSLEN 3	    /* length of FIF for DCS commands sent */
#define DEFDISLEN 3	    /* length of DIS initially transmitted */
#define DEFCAP 1,3,0,2,0,0,0,0	/* default local capabilities */
#define DEFID "                    " /* default local ID */
#define DEFPAT "%m%d%H%M%S" /* default received file name pattern */
#define HDRSHFT 54	    /* shift header right 6.7mm into image area */
#define HDRSPCE 20	    /* number of scan lines inserted before image */
#define HDRSTRT  4	    /* scan line where header is placed on image */
#define HDRCHRH 24 	    /* header character height (pels, at 196lpi) */
#define HDRCHRW 12	    /* header character width (pels) */
#define IDLEN 20	    /* length of T.30 ID strings, must be 20 */
#define MAXDIS 8	    /* maximum DIS frames sent without response (T1) */
#define MAXERRPRT 32	    /* maximum number of reception errors to report */
#define MAXFIFLEN 125	    /* max FIF len = MAXFRLEN - (adx+ctl+FCF) - FCS */
#define MAXFRLEN 130        /* max frame length = 3.45s x 300 bps / 8 */
#define MAXGETTY 512        /* maximum length of exec'ed (-g, -e) commands */
#define MAXICMD  100        /* maximum # of modem setup/reset commands */
#define MAXLKFILE 16	    /* maximum number of lock files */
#define MAXNULLS 2	    /* maximum consecutive received nulls saved */
#define MAXPGERR 10         /* maximum received errors allowed per page */
#define MAXTRAIN 2	    /* maximum training retries at lowest speed */
#define MAXRETRY 3	    /* maximum retries of unacknowledged commands */
#define NCAP 8              /* number of fields in a capability string */
#define NTXRETRY 2	    /* maximum re-sends per page */

typedef int cap [ NCAP ] ;		/* remote/local capabilities */

int sighup = 0 ;		/* received a SIGHUP (should exit when we go idle) */
int waiting = 0 ;		/* blocked waiting for activity (okay to exit on SIGHUP) */
int manual_answer = 0 ;		/* Manual answer flag (set by client connection) */
int answer_wait = 0 ;		/* blocked waiting for the first fax frame (inclusive of RING messages) */
char *faxfile = FAXFILE ;	/* modem to use ("/dev/<*>.modem") */
int modem_found = 0;		/* modem found */

                                        /* capability fields... */
enum  captype {	         VR, BR, WD, LN, DF, EC, BF, ST } ;
int capmax [ NCAP ] = {   1,  7,  2,  2,  3,  2,  1,  7 } ;
					/* & maximum values */

                                        /* vertical resolution, dpi */
int vresolution [ 2 ] = { 98, 196 } ;

					/* characters per second for br */
int cps [ 8 ] = { 300, 600, 900, 1200, 1500, 1800, 900, 1200 } ;

					/* next br = fallback [ br ] */
                    /* 0, 1, 2, 3, 4, 5, 6, 7 */
int fallback [ 8 ] = {-1, 0, 1, 2, 7, 4, 3, 6 } ;

					/* negotiation speed index */
                         /* 0, 1, 2, 3, 4, 5, 6, 7 */
int brindex [ 8 ] = { 0, 1, 2, 3, 4, 7, 5, 6 } ;

					/* minimum scan time in ms  */
int mst [ 8 ] = { 0 , 5, 10, 10, 20, 20, 40, 40 } ;

					/* page width in pixels */
int pagewidth [ 5 ] = { 1728, 2048, 2432, 1216, 864 } ;

/* Table to convert between T.30 DIS/DCS/DTC FIF and Class 2-like
   capability codes. Uses br=6, 7 for V.17 at 7200, 9600. */

typedef struct t30tabstruct
{ 
  char *name ; 
  uchar byte, shift, mask ; 
  uchar safeval ;
  uchar captodis[8], distocap[16], captodcs[8], dcstocap[16] ; 
} t30tabst ;

#define X 0xff				/* invalid values */

t30tabst t30tab [ NCAP ] = {
  { "vr", 1, 1, 0x01, 0, { 0, 1 } , { 0, 1 } , { 0, 1 } , { 0, 1 } },	 
  { "br", 1, 2, 0x0f, 0,
      { 0, 4, 12, 12, 13, 13 } ,
      { 0, X, X, X, 1, X, X, X, 3, X, X, X, 3, 5, 3, X } ,
      { 0, 4, 12, 8, 5, 1 } ,
      { 0, 5, 5, X, 1, 4, 4, X, 3, 7, X, X, 2, 6, X, X } } ,
  { "wd", 2, 6, 0x03, 0, { 0, 2, 1 } , { 0, 2, 1, 2 } ,
      { 0, 2, 1 } , { 0, 2, 1, 2 } },
  { "ln", 2, 4, 0x03, 0, { 0, 2, 1 } , { 0, 2, 1, X } ,
      { 0, 2, 1 } , { 0, 2, 1, X } },
  { "df", 1, 0, 0x01, 0, { 0, 1 } , { 0, 1 } , { 0, 1 } , { 0, 1 } },
  { "ec", 3, 4, 0x03, 0, { 0, 2, 2 } , { 0, X, 2, X } , 
      { 0, 3, 2 } , { 0, 0, 2, 1 } }, 
  { "bf", 5, 5, 0x01, 0, { 0, 1 } , { 0, 1 } , { 0, 1 } , { 0, 1 } },
  // (2003-01-15, ggs) The dcstocap table for the st field was incorrect
  //   dcstocap[3] should be X and dcstocap[4] should be 1 (5ms).
  { "st", 2, 1, 0x07, 7,
      { 7, 4, 3, 2, 6, 0, 5, 1 } , { 5, 7, 3, 2, 1, 6, 4, 0 } ,
      { 7, 4, X, 2, X, 0, X, 1 } , { 5, 7, 3, X, 1, X, X, 0 } } 
} ;

					/* values of capability fields */
char *capvaluestr [ NCAP ] [8] = {
  { " 98lpi", "196lpi" } , 
  { " 2400bps", " 4800bps", " 7200bps", " 9600bps", "  12kbps", "14.4kbps",
    "7200V.17", "9600V.17" } ,
  { "8.5\"/215mm", " 10\"/255mm", " 12\"/303mm",
    "  6\"/151mm", "4.2\"/107mm" } ,
  { "11\"/A4", "14\"/B4", " any  " } ,
  { "1D" , "2D" }, { "   -   ", "ECM-256", "ECM-64 " }, { " - ", "BFT" },
  { "0ms", "5ms", "10/5ms", "10ms", "20/10ms", "20ms", "40/20ms", "40ms" }
} ;

/* T.30 control frames */

enum frametype {	    
 DIS=0x01, CSI,	NSF=0x04,
 CFR=0x21, FTT,
 MCF=0x31, RTN, RTP, PIN, PIP,
 DCS=0x41, TSI,	NSS=0x44,
 CRP=0x58, DCN=0x5f,
 EOM=0x71, MPS, EOP=0x74, PRI_EOM=0x79, PRI_MPS, PRI_EOP=0x7c,
 DTC=0x81, CIG, NSC=0x84
 } ;

enum commanddtype { RCV=0, SND=1, DTA=0, TRN=1 } ;

/* Class 1 commands to [receive=0/transmit=1] [data=0/training=1] for
   [baud rate=BR]. */

char *c1cmd [ 2 ]  [ 2 ] [ 8 ] = { 
{ { "+FRM=24", "+FRM=48", "+FRM=72", "+FRM=96", "+FRM=122", "+FRM=146" ,
    "+FRM=74", "+FRM=98" } ,
  { "+FRM=24", "+FRM=48", "+FRM=72", "+FRM=96", "+FRM=121", "+FRM=145" ,
    "+FRM=73", "+FRM=97" } } ,
{ { "+FTM=24", "+FTM=48", "+FTM=72", "+FTM=96", "+FTM=122", "+FTM=146" ,
    "+FTM=74", "+FTM=98", } ,
  { "+FTM=24", "+FTM=48", "+FTM=72", "+FTM=96", "+FTM=121", "+FTM=145" ,
    "+FTM=73", "+FTM=97" } }
} ;

struct c2msgstruct
{ 
  int min, max ;
  char *msg ;
} c2msg [] = {
  {   0,   9, "Call Placement and Termination:" },
  {   0,   0, "  Normal and proper end of connection" },
  {   1,   1, "  Ring Detect without successful handshake" },
  {   2,   2, "  Call aborted, from +FK[S] or CAN" },
  {   3,   3, "  No Loop Current" },
  {   4,   4, "  Ringback Detected, no answer" },
  {   5,   5, "  Ringback Detected, answer without CED" },

  {  10,  19, "Transmit Phase A & Miscellaneous Errors:" },
  {  10,  10, "  Unspecified Phase A error" },
  {  11,  11, "  No Answer (T.30 T1 timeout)" },
  {  20,  39, "Transmit Phase B Hangup Codes:" },
  {  20,  20, "  Unspecified Transmit Phase B error" },
  {  21,  21, "  Remote cannot receive or send" },
  {  22,  22, "  COMREC error in transmit Phase B" },
  {  23,  23, "  COMREC invalid command received" },
  {  24,  24, "  RSPREC error" },
  {  25,  25, "  DCS sent three times without response" },
  {  26,  26, "  DIS/DTC received 3 times; DCS not recognized" },
  {  27,  27, "  Failure to train at 2400 bps or +FMINSP value" },
  {  28,  28, "  RSPREC invalid response received" },
  {  40,  49, "Transmit Phase C Hangup Codes:" },
  {  40,  40, "  Unspecified Transmit Phase C error" },
  {  41,  41, "  Unspecified image format error"  },
  {  42,  42, "  Image conversion error" },
  {  43,  43, "  DTE to DCE data underflow" },
  {  44,  44, "  Unrecognized transparent data command" },
  {  45,  45, "  Image error, line length wrong" },
  {  46,  46, "  Image error, page length wrong" },
  {  47,  47, "  Image error, wrong compression code" },
  {  50,  69, "Transmit Phase D Hangup Codes:" },
  {  50,  50, "  Unspecified Transmit Phase D error" },
  {  51,  51, "  RSPREC error" },
  {  52,  52, "  No response to MPS repeated 3 times" },
  {  53,  53, "  Invalid response to MPS" },
  {  54,  54, "  No response to EOP repeated 3 times" },
  {  55,  55, "  Invalid response to EOP" },
  {  56,  56, "  No response to EOM repeated 3 times" },
  {  57,  57, "  Invalid response to EOM" },
  {  58,  58, "  Unable to continue after PIN or PIP" },

  {  70,  89, "Receive Phase B Hangup Codes:" },
  {  70,  70, "  Unspecified Receive Phase B error" },
  {  71,  71, "  RSPREC error" },
  {  72,  72, "  COMREC error" },
  {  73,  73, "  T.30 T2 timeout, expected page not received" },
  {  74,  74, "  T.30 T1 timeout, after EOM received" },
  {  90,  99, "Receive Phase C Hangup Codes:" },
  {  90,  90, "  Unspecified Receive Phase C error" },
  {  91,  91, "  Missing EOL after 5 seconds" },
  {  92,  92, "  Unused code" },
  {  93,  93, "  DCE to DTE buffer overflow" },
  {  94,  94, "  Bad CRC or frame (ECM or BFT modes)" },
  { 100, 119, "Receive Phase D Hangup Codes:" },
  { 100, 100, "  Unspecified Receive Phase D errors" },
  { 101, 101, "  RSPREC invalid response received" },
  { 102, 102, "  COMREC invalid response received" },
  { 103, 103, "  Unable to continue after PIN or PIP" },
  { 120, 255, "Reserved Codes" },
  {  -1,  -1, "" }
} ;

/* meaning of efax return codes */

char *errormsg [] = {
  "success",
  "number busy or modem in use",
  "unrecoverable error",
  "invalid modem response",
  "no response from modem",
  "terminated by signal",
  "terminated by sleep",
  "internal error" } ;

/* Functions... */
static void signal_handler(int sig);
void onsig(int sig);

/* Return name of frame of type 'fr'. */

char *frname ( int fr )
{
  static struct framenamestruct {  int code ;  char *name ; } 
  framenames [] = {

    {NSC,"NSC - poller features"}, /* these 3 frames must be first */
    {CIG,"CIG - poller ID"}, 
    {DTC,"DTC - poller capabilities"},
    {NSF,"NSF - answering features"},
    {CSI,"CSI - answering ID"},
    {DIS,"DIS - answering capabilities"},
    {NSS,"NSS - caller features"},
    {TSI,"TSI - caller ID"},
    {DCS,"DCS - session format"},

    {CFR,"CFR - channel OK"},
    {FTT,"FTT - channel not OK"},

    {MPS,"MPS - not done"},
    {EOM,"EOM - not done, new format"},
    {EOP,"EOP - done"},

    {PRI_MPS,"PRI-MPS - not done, call operator"},
    {PRI_EOM,"PRI-EOM - not done, new format, call operator"},
    {PRI_EOP,"PRI-EOP - done, call operator"},

    {MCF,"MCF - page OK"},
    {RTP,"RTP - page OK, check channel"},
    {PIP,"PIP - page OK, call operator"},
    {RTN,"RTN - page not OK, check channel"},
    {PIN,"PIN - page not OK, call operator"},

    {CRP,"CRP - repeat command"},
    {DCN,"DCN - disconnect"},
    {0,0} },
  *p ;
  
  for ( p=framenames ; p->code ; p++ )
    if ( fr == p->code || ( fr & 0x7f ) == p->code) break ;
  return p->code ? p->name : "UNKNOWN" ;
}

/* Range-check capability. */

int checkcap ( cap c )
{
  int err=0, i ;

  for ( i=0 ; i<NCAP ; i++ )
    if ( c[i] > capmax[i] || c[i] < 0 ) {
      err = msg ( "E3%s = %d out of range, set to 0", t30tab[i].name, c[i] ) ;
      c[i]=0 ;
    }
  return err ;
}


/* Print cap[ability] c using text values and prefix s. */

void printcap ( char *s , cap c )
{
  int i ;
  msg ( "N-+ %s" , s ) ;
  checkcap ( c ) ;
  for ( i=0 ; i<NCAP ; i++ ) 
    msg ( "N-+  %s" , capvaluestr [ i ] [ c[i] ] ) ;
  msg ( "N-" ) ;
}


/* Convert capability string to cap struct. Returns 0 or 2 on errors. */

int str2cap ( char *s, cap c )
{
  int err=0, n ;

  n = sscanf ( s, "%d,%d,%d,%d,%d,%d,%d,%d", 
	      c+0, c+1, c+2, c+3,  c+4, c+5, c+6, c+7 ) ;

  if ( n < NCAP ) msg ( "Wmissing value(s) in \"%s\"", s ) ;

  checkcap ( c ) ;

  return err ;
}


/* Convert a cap[ability] 'c' to a DIS/DCS/DTC FIF 'fif' of 'len'
   bytes.  Converts into DIS format if 'isdis' is true, else into
   DCS/DTC format. */

void mkdis ( cap c, uchar *fif, int len, int isdis, int t4tx ) 
{
  int i, k ;
  t30tabst *p ;

  len = len > DCSLEN ? DCSLEN : len ;

  fif[0] = 0 ;
  fif[1] = ( isdis && t4tx ? 0x80 : 0 ) | 0x40 ;
  for ( i=2 ; i<len-1 ; i++ ) fif[i] = 0x01 ;       /* add extension bits */
  fif[i] = 0 ;

  checkcap ( c ) ;

  for ( i=0 ; i<NCAP ; i++ ) {
    p = t30tab + i ;
    if ( ( k = ( isdis ? p->captodis : p->captodcs ) [ c [ i ] ] ) == X )
      msg ( "E3mkdis: can't happen (invalid %s)", p->name ), k=0 ;
    if ( p->byte < len ) fif [ p->byte ] |= k << p->shift ;
  }
}


/* Return length of DIS/DTC FIF (counts extension bits). */

int dislen ( uchar *fif )
{
  int n ;
  for ( n=3 ; fif [ n-1 ] & 0x01 && n < MAXFIFLEN ; n++ ) ;
  return n ;
}


/* Convert received DIS/DCS/DTC FIF to cap. Returns 0 or 3 if bad DIS/DCS
   field. */

int mkcap ( uchar *fif, cap c, int dis ) 
{
  int err=0, i, j, k, len ;
  t30tabst *p ;

  len = dislen ( fif ) ;

  for ( i=0 ; i<NCAP ; i++ ) {
    p=t30tab+i ;
    if ( p->byte >= len ) {
      c [ i ] = 0 ;
    } else {
      j = ( fif [ p->byte ] >> p->shift ) & p->mask ;
      k = ( dis ? p->distocap : p->dcstocap ) [ j ] ;
      if ( k == X ) {
	c [ i ] = p->safeval ;
	err = msg("E3mkcap: bad %s field (%d) set to %d", 
		  p->name, j, c [ i ] ) ;
      } else { 
	c [ i ] = k ;
      }
    }
  }
  return err ;
}


/* Compute compatible local/remote capabilities. Used by the
   sending station only and only for Class 1. Returns 0 if OK or
   3 if no compatible settings possible. */

int mincap ( cap local, cap remote, cap session )
{
  int err=0, i ;
  int msttab[2][8] = { { 0,1,3,3,5,5,7,7 } , { 0,1,1,3,3,5,5,7 } } ;

  printcap ( "local  ", local ) ;
  printcap ( "remote ", remote ) ;

  for ( i=0 ; i<NCAP && i!=ST && i !=BR ; i++ )
    session[i] = remote[i] < local[i] ? remote[i] : local[i] ;

  session[BR] = brindex[ remote[BR] ] < brindex[ local[BR] ] ?
    remote[BR] : local[BR] ;

  session[ST] = msttab [ session[VR] ] [ remote[ST] ] ;

  printcap ( "session", session ) ;

  if ( local[WD] != session[WD] || local[LN] > session[LN] || 
      local[DF] != session[DF] ) 
    err = msg ("W3incompatible local and remote capabilities" ) ;

  return err ;
}


/* Skip to start of first/next page (or to start of previous page
   if dp is 0).  If ppm in not null, it is then set to EOP if
   there are no pages following this one, MPS if the next page
   has the same format as `local' (assumed to be the format of
   the previous page), EOM if the page has a different format.
   If local is non-NULL its format fields are set according to
   the format of the new page.  Currently only considers the
   file's y-resolution.

   This function is called before send_data() and obtains the ppm
   for that page.  It can be called again with dp=0 if a PIN or
   RTN is received to restart the page.  Returns 0 or 2 on
   errors. */

int rdpage ( IFILE *f, int dp, int *ppm, cap local, int *changed )
{
  int err=0, m=EOP, yres, fVR, nVR  ;

  if ( nextipage ( f, dp ) )
    err = msg ( "E2 can't happen (rdpage: can't go to %s page)", 
	       dp ? "next" : "same" ) ;
  
  if ( ! err ) {

    yres = f->page->yres ;
    fVR = ( yres > (196+98)/2 ) ? 1 : 0 ;

    if ( local && yres ) {
      if ( local [ VR ] != fVR ) {
	local [ VR ] = fVR ;
	if ( changed ) *changed = 1 ;
      } else {
	if ( changed ) *changed = 0 ;
      }
    }

    if ( lastpage ( f ) ) {
      m = EOP ;
    } else {
      PAGE *p = f->page + 1 ;
      nVR = ( p->yres > (196+98)/2 ) ? 1 : 0  ;
      m = ( nVR != fVR ) ? EOM : MPS ;
    }

  }

  if ( ppm ) {
    *ppm = err ? EOP : m ;
  }

  return err ;
}


/* Terminate previous page if page number is non-zero and start
   next output page if page number is non-negative. If page is -1
   removes the most recently opened file. Returns 0 if OK, 2 on
   errors. */

int wrpage ( OFILE *f, int page )
{
  int err=0 ;

  err = nextopage ( f, page ) ;

  if ( ! err && page == -1 ) {
    if ( remove ( f->cfname ) ) {
      err = msg ( "ES2can't delete file %s:", f->cfname ) ; 
    } else {
      msg ( "Fremoved %s", f->cfname ) ; 
    }
  }
  
  return err ;
}


/* Send data for one page.  Figures out required padding and 196->98 lpi
   decimation based on local and session capabilitites, substitutes page
   numbers in header string and enables serial port flow control.  Inserts
   the page header before the input file data.  Converts each scan line to
   T.4 codes and adds padding (FILL) and EOL codes before writing out.
   Sends RTC when done.  Sends DLE-ETX and returns serial port to command
   mode when done. Returns 0 if OK, non-0 on errors. */

int send_data ( TFILE *mf, IFILE *f, int page, int pages,
	       cap local, cap session, char *header, faxfont *font )
{
  int done=0, err=0, noise=0, nr=0, lastnr=0, line, pixels ;
  int i, decimate, pwidth, minlen, dcecps, inheader, skip=0 ;
  uchar buf [ MAXCODES + 2*EOLBITS/8 + 1 ], *p ;
  short runs [ MAXRUNS ], lastruns [ MAXRUNS ] ;
  char headerbuf [ MAXLINELEN ] ;
  ENCODER e ;

  newENCODER ( &e ) ;

  dcecps = cps[session[BR]] ;
  minlen = ( (long)dcecps * mst[session[ST]] - 1500 + 500 ) / 1000 ;
  pwidth = pagewidth [ session [ WD ] ] ;
  decimate = local[VR] > session[VR] ;

  msg ( "T padding to %d bytes/scan line.%s", minlen+1, 
       decimate ? " reducing 196->98 lpi." : "" ) ;

  if ( vfc ) 
    msg ( "T limiting output to %d bps for %d byte modem buffer", 
	 dcecps*8, MAXDCEBUF + MINWRITE  ) ;

  if ( ckfmt ( header, 6 ) )
    msg ( "W too many %%d escapes in header format string \"%s\"", header ) ;
  else
    snprintf ( headerbuf, sizeof(headerbuf), header, page, pages, page, pages, page, pages ) ;
  msg ("I header:[%s]", headerbuf ) ;
      
  done = err = ttymode ( mf, SEND ) ; 

  msg ( "Isending page %d of %d", page, f->totalpages ) ;

  mf->start = time(0) ;
  mf->mstart = proc_ms() ;
  mf->bytes = mf->pad = mf->lines = 0 ;

  /* start T.4 data with some FILL and an EOL */

  p = buf ;
  for ( i=0 ; i<32 ; i++ ) {
    p = putcode ( &e, 0, 8, p ) ;
  }
  p = putcode ( &e, EOLCODE, EOLBITS, p ) ;

  if ( ! f || ! f->f ) 
    err = msg ( "E2can't happen(send_data)" ) ; 

  mf->lines=0 ;
  for ( line=0 ; ! done && ! err ; line++ ) {

    if ( line < HDRSPCE ) {	/* insert blank lines at the top */
      runs[0] = pwidth ;
      pixels = pwidth ;
      nr = 1 ;
    } else {
      if ( ( nr = readline ( f, runs, &pixels ) ) < 0 ) {
	done = 1 ;
	continue ;
      }
    }
				/* generate and OR in header pixels */
    if ( line >= HDRSTRT && line < HDRSTRT + HDRCHRH ) {
      int hnr ;
      short hruns [ MAXRUNS ] ;
      hnr = texttorun ( (uchar*) headerbuf, font, line-HDRSTRT, 
		       HDRCHRW, HDRCHRH, HDRSHFT,
		       hruns, 0 ) ;
      nr = runor ( runs, nr, hruns, hnr, 0, &pixels ) ;
    }
    
    inheader = line < HDRSTRT + HDRCHRH ;

    if ( decimate || ( inheader && local[VR] == 0 ) ) {
      if ( ++skip & 1 ) {	/* save the first of every 2 lines */
   	memcpy ( lastruns, runs, nr * sizeof(short) ) ;
   	lastnr = nr ;
   	continue ;		/* get next line */
      } else {			/* OR previous line into current line */
   	nr = runor ( runs, nr, lastruns, lastnr, 0, &pixels ) ;
      }
    }

    if ( nr > 0 ) {
      if ( pixels ) {
				/* make line the right width */
	if ( pixels != pwidth ) nr = xpad ( runs, nr, pwidth - pixels ) ;
				/* convert to MH coding */
	p = runtocode ( &e, runs, nr, p ) ;
				/* zero pad to minimum scan time */
	while ( p - buf < minlen ) { 
	  p = putcode ( &e, 0, 8, p ) ;
	  mf->pad ++ ;
	}
				/* add EOL */
	p = putcode ( &e, EOLCODE, EOLBITS, p ) ;
	sendbuf ( mf, buf, p - buf, dcecps ) ;
	mf->bytes += p - buf ;
	mf->lines++ ;
      } else {
	/* probably read an EOL as part of RTC */
      }
      if ( tdata ( mf, 0 ) ) noise = 1 ;
      p = buf ;
    }
    
    /* (2002-12-02, ggs) Add notification of percentage completion. */
#if defined(__APPLE__)
    if (mf->lines % 100 == 0)
    {
      CFNumberRef value;
      long percentage = decimate ?
			((float)mf->lines * 2 / (float)f->page->h) * 100 :			
			((float)mf->lines / (float)f->page->h) * 100;

      value = CFNumberCreate(kCFAllocatorDefault,
			kCFNumberLongType, &percentage);

      notify(CFSTR("percentage"), value);

      CFRelease(value);
    }
#endif
  }

  for ( i=0 ; i < RTCEOL ; i++ )
    p = putcode ( &e, EOLCODE, EOLBITS, p ) ;
  p = putcode ( &e, 0, 0, p ) ;
  sendbuf ( mf, buf, p - buf, dcecps ) ;
  mf->bytes += p - buf ;
  
  if ( noise ) msg ("W- characters received while sending" ) ;

  return err ;
}


int end_data ( TFILE *mf, cap session, int ppm, int *good )
{
  int err=0, c ;
  char *p ;
  long dt, draintime ;

  if ( ! ppm ) p = DLE_ETX ;
  else if ( ppm == MPS ) p = "\020," ; 
  else if ( ppm == EOM ) p = "\020;" ; 
  else if ( ppm == EOP ) p = "\020." ; 
  else {
    p = "" ;
    err = msg ( "E2 can't happen (end_data)" ) ;
  }

  tput ( mf, p, 2 ) ;

  dt = time(0) - mf->start ;
				/* time to drain buffers + 100% + 4s */
  draintime = ( 2 * ( mf->bytes / cps[ session[BR] ] + 1 - dt ) + 4 ) * 10 ;
  draintime = draintime < TO_DRAIN_D ? TO_DRAIN_D : draintime ;

  c = ckcmd ( mf, 0, 0, (int) draintime, OK ) ;

  if ( good ) *good = ( c == OK ) ? 1 : 0 ;

  dt = time(0) - mf->start ;

  msg ( "Ifinished page" ) ;

  msg ( "Isent %d+%d lines, %d+%d bytes, %d s  %d bps" , 
       HDRSPCE, mf->lines-HDRSPCE, 
       mf->bytes-mf->pad, mf->pad, (int) dt, (mf->bytes*8)/dt ) ;

  if ( mf->bytes / (dt+1) > cps[session[BR]] )
    msg ( "E flow control did not work" ) ;

  if ( ! err ) err = ttymode ( mf, COMMAND ) ;

  return err ;
}


/* Read one scan line from fax device. If pointer pels is not
   null it is used to save pixel count.  Returns number of runs
   stored, EOF on RTC, or -2 on EOF, DLE-ETX or other error. */

int readfaxruns ( TFILE *f, DECODER *d, short *runs, int *pels )
{
  int err=0, c=EOF, x, n ;
  dtab *tab, *t ;
  short shift ;
  short *p, *maxp, *q, len=0 ;
  uchar rd_state ;

  maxp = ( p = runs ) + MAXRUNS ;

  x = d->x ; shift = d->shift ; tab = d->tab ; /* restore decoder state */
  rd_state = f->rd_state ;

  do {
    do {
      while ( shift < 0 ) { 
	c = tgetd ( f, TO_CHAR ) ;

	if (( rd_state & rd_allowed[c] ))
	{
	   if (( rd_state & rd_nexts[c] ))
	     rd_state <<= 1;
	}
	else
	  rd_state = RD_BEGIN ;

	if ( rd_state == RD_END )
	  msg ( "W+ modem response in data" ) ; 

	if ( c < 0 )  {
	  x = ( x << 15 ) | 1 ; shift += 15 ;  /* EOL pad at EOF */
	} else {
	  x = ( x <<  8 ) | c ; shift +=  8 ; 
	}
      }
      t = tab + ( ( x >> shift ) & 0x1ff ) ;
      tab = t->next ;
      shift -= t->bits ;
    } while ( ! t->code ) ;
    if ( p < maxp ) *p++ = t->code ;
  } while ( t->code != -1 ) ;

  d->x = x ; d->shift = shift ; d->tab = tab ; /* save state */
  f->rd_state = rd_state ;

  if ( p >= maxp ) msg ( "Wrun length buffer overflow" ) ;

  /* combine make-up and terminating codes and remove +1 offset
     in run lengths */

  n = p - runs - 1 ;
  for ( p = q = runs ; n-- > 0 ; )
    if ( *p > 64 && n-- > 0 ) {
      len += *q++ = p[0] + p[1] - 2 ;
      p+=2 ;
    } else {
      len += *q++ = *p++ - 1 ;
    }
  n = q - runs ;
  
  /* check for RTC and errors */

  if ( len )
    d->eolcnt = 0 ;
  else
    if ( ++(d->eolcnt) >= RTCEOL ) err = EOF ;

  if ( c < 0 ) err = - 2 ;

  if ( pels ) *pels = len ;
  
  return err ? err : n ;
}


/* Receive data. Reads scan lines from modem and writes to output
   file.  Checks for errors by comparing received line width and
   session line width.  Check that the output file is still OK
   and if not, send one CANcel character and wait for protocol to
   complete. Returns 0 if OK, 1 on DLE-ETX without RTC, or 2 if
   there was a file write error. */

int receive_data ( TFILE *mf, OFILE *f, cap session, int *nerr, int *nline )
{
  int err=0, line, lines, nr, len ;
  int pwidth = pagewidth [ session [ WD ] ] ;
  short runs [ MAXRUNS ] ;
  DECODER d ;

  if ( ! f || ! f->f ) {
    msg ( "E2 can't happen (writeline)" ) ;
    return 2;
  } 
  
  newDECODER ( &d ) ;

  lines=0 ; 
  for ( line=0 ; ( nr = readfaxruns ( mf, &d, runs, &len ) ) >= 0 ; line++ ) {
    if ( nr > 0 && len > 0 && line ) { /* skip first line+EOL and RTC */
      if ( len != pwidth ) { 
	(*nerr)++ ;
	if ( *nerr <= MAXERRPRT ) msg ("R-+ (%d:%d)", line, len ) ;
	nr = xpad ( runs, nr, pwidth - len ) ;
      } 
      writeline ( f, runs, nr, 1 ) ;
      lines++ ;
    }
    if ( ferror ( f->f ) ) {
      err = msg ("ES2file write:") ;
      tput ( mf, CAN_STR, 1 ) ;
      msg ("Wdata reception CANcelled") ;
    } 
  }
  
  if ( *nerr ) {
    if ( *nerr > MAXERRPRT ) msg ("R-+ ....." ) ;
    msg ("R-  : reception errors" ) ;
    msg ("W- %d reception errors", *nerr ) ;
  }

  if ( nr == EOF ) { 
    while ( tgetd ( mf, TO_CHAR ) >= 0 ) ; /* got RTC, wait for DLE-ETX */
  } else {
    err = 1 ;			/* DLE-ETX without RTC - should try again */
  }

  msg ( "I- received %d lines, %d errors, eolcnt %d", lines, *nerr, d.eolcnt ) ;

  if (nline) *nline = lines;

  return err ;
}


/* Send training check sequence of n zeroes.  Returns 0 or 2 on error. */

int puttrain ( TFILE *f, char *s, int n  )
{
  int i, m, err=0 ;
  uchar buf [ MINWRITE ] = { 0 } ;

#ifdef ADDTCFRTC
  ENCODER e ;
  uchar *p ;
#endif

  ckcmd ( f, &err, s, TO_FT, CONNECT ) ;
  
  if ( ! err ) {
    ttymode ( f, SEND ) ;

    /* send n bytes of zeros */

    for ( i=0 ; i < n ; i += m ) {
      m = n-i < MINWRITE ? n-i : MINWRITE ;
      sendbuf ( f, buf, m, 0 ) ;
    }

#ifdef ADDTCFRTC
    /* append RTC in case modem is looking for it */

    newENCODER ( &e ) ;

    p = buf ;
    for ( i=0 ; i < RTCEOL ; i++ )
      p = putcode ( &e, EOLCODE, EOLBITS, p ) ;
    p = putcode ( &e, 0, 0, p ) ;
    sendbuf ( f, buf, p - buf, 0 ) ;
#endif

    tput ( f, DLE_ETX, 2 ) ; 


    ckcmd ( f, &err, 0, TO_DRAIN_D, OK ) ;
    msg ( "I- sent TCF - channel check of %d bytes", n ) ;

    ttymode ( f, COMMAND ) ;
  }
  
  return err ;
}


/* Checks for an error-free run of at least n bytes in the
   received training check sequence. Sets good if it's not null,
   the run was long enough and there were no errors. Returns 0 or
   3 on other errors.  */

int gettrain ( TFILE *f, char *s, int n, int *good ) 
{ 
  int err=0, c, i=0, maxrunl=0, runl=0 ;
  
  // (2003-01-29, ggs) Modify to support proper operation when +FAR=1
  //   which is necessary to support receive at 9600bps from Brother
  //   branded fax machines (and probably others using the same chipset)
  //   If we get the PLUS result, we eat the rogue frame then re-issue
  //   the +FRM command.
  c = cmd ( f, s, T3S ) ; // CONNECT or +FRH:3
  
  if (c == PLUS) {
  	c = cmd ( f, 0, T2 ) ; 	// CONNECT
  	while ( ( c = tgetd ( f, T3S ) ) >= 0 )
  		;					// rogue data
  	c = cmd ( f, 0, T2 ) ; 	// OK
    c = cmd ( f, s, T2 ); 	// +FRM=X
  }
  
  if ( c == CONNECT ) {

    for ( i=0 ; ( c = tgetd ( f, T3S ) ) >= 0 ; i++ )
      if ( c ) {
   	if ( runl > maxrunl ) maxrunl = runl ;
   	runl = 0 ;
      } else {
   	runl ++ ;
      }
    
    if ( c == EOF )
      err = msg ( "E3timed out during training check data" ) ;
    else
      ckcmd ( f, &err, 0, TO_RTCMD, NO ) ;
    
  }
  else if (c != OK)
  {
    tput ( f, CAN_STR, 1 ) ;
    ckcmd ( f, 0, 0, 1, OK ) ;
    err = 1 ;
  }

  if ( runl > maxrunl ) maxrunl = runl ;
     
  if ( good ) *good = !err && maxrunl > n ;

  if ( !err ) {
    msg ( "I- received TCF - channel check (%sOK: run of %d in %d)", 
   	 maxrunl > n ? "" : "not ", maxrunl, i ) ;
  }

  return err ;
}


/* Log a sent/received HDLC frame.  Display of these messages is delayed to
   avoid possible timing problems. */

void logfr ( char *s , char *nm , uchar *p , int n )
{
  int i=0 ;
  msg ( n > 10 ? "H- %s %d bytes:" : "H-+ %s %d bytes:" , s, n ) ;
  for ( i=0 ; i<n ; i++ ) {
    msg ( "H-+  %02x" , p[i] & 0xff ) ;
    if ( ( i&0xf ) == 0xf && i != n-1 ) msg ( "H-" ) ;
  }
  msg ( "H-" ) ;
  msg ( "I- %s %s", s, nm ) ;
}


/* Send HDLC control frame of type type.  Extra bits can be OR'ed
   into the frame type (FCF) to indicate that this frame follows
   a previous one (no +FTH required) and/or that more frames will
   follow.  Sets up flag, address, and fax control field bytes in
   `buf'.  Sends these plus `len` additional bytes.  Terminates
   with DLE-ETX and checks response.  Returns 0 if OK, 2 or 3 on
   error. */

#define MORE_FR  0x100 
#define SUB_FR   0x200 

int nframes = 0 ;		/* counts frames sent/received */

int putframe ( int type, uchar *buf, int len, TFILE *f, int t )
{
  int err=0 ;

  buf [ 0 ] = 0xff ;
  buf [ 1 ] = type & MORE_FR ? 0xc0 : 0xc8 ;
  buf [ 2 ] = type & 0xff ;

  if ( nframes++ && ! ( type & SUB_FR ) )
    ckcmd ( f, &err, "+FTH=3" , TO_FT, CONNECT ) ;
  
  if ( ! err ) {

    if ( ! buf[len+2] ) {
      msg ( "Wlast byte of frame is NULL" ) ;
    }

    /* ttymode ( f, SEND ) ; */
    sendbuf ( f, buf, len+3, 0 ) ;
    tput ( f, DLE_ETX, 2 ) ; 
    /* ttymode ( f, COMMAND ) ; */

    logfr ( "sent", frname ( buf [ 2 ] ), buf, len+3 ) ;

    ckcmd ( f, &err, 0, TO_DRAIN_H, ( type & MORE_FR ) ? CONNECT : OK ) ;
  }

  return err ;
}


/* Reverse bit and byte order of ID strings as per T.30 5.3.6.2.4-6 */

void revcpy ( uchar *from , uchar *to )
{
  int i, j ;
  for ( i=0, j=IDLEN-1 ; i<IDLEN ; i++, j-- ) 
    to [ i ] = normalbits [ from [ j ] & 0xff ] ;
}


/* Check for missing initial 0xFF in HDLC frame and insert it if
   missing.  Ugly fix for a still-hidden bug.  Also do bit
   inversion if required. */

int fixframe ( uchar *buf, int n, TFILE *f )
{
  int i;

  if ( *buf == 0xc0 || *buf == 0xc8 ) {
    for ( i=n; i >= 1 ; i-- ) 
      buf[i]=buf[i-1] ;
    buf[i] = 0xff ;
    msg ("W HDLC frame missing initial 0xff" ) ;
    n++ ;
  }

  if ( buf[1] == 0x03 || buf[1] == 0x13 ) {
    for ( i=0 ; i < n ; i++ ) 
      buf[i]=normalbits[buf[i]] ;
    msg ("W bit-reversed HDLC frame, reversing bit order" ) ;
    f->ibitorder = f->ibitorder == normalbits ? reversebits : normalbits ;
  }

  return n ;
}


/* Read HDLC frame data.  Returns 0 if OK, 1 on frame error, 3 on
   timeout, invalid response or too-long frame. */

int receive_frame_data ( TFILE *f, uchar *buf, int n, int *len )
{
  int err=0, c, i ;

  for ( i=0 ; ( c = tgetd ( f, T3S ) ) >= 0  ; i++ )
    if ( i < n ) buf[ i ] = c ;
  
  if ( c == EOF ) {

    err = msg ( "E3timed out reading frame data" ) ;

  } else {
    
    switch ( cmd ( f, 0, TO_RTCMD ) ) {
    case OK:
    case CONNECT:
      break ;
    case ERROR:
    case NO:
      err = msg ( "W1frame error" ) ;
      break ;
    case EOF:
      err = msg ( "E3no response after frame data" ) ;
      break ;
    default:
      err = msg ( "E3wrong response after frame data" ) ;
      break ;
    }

  }

  if ( i > n )  {
    err = msg ( "E3frame too long (%d, > %d max bytes)", i, n ) ;
    i = n ;
  }

  if ( len ) *len = i ;

  return err ;
}


/* Get a Class 1 command or response frame.  An attempt to match
   and combine T.30 "Response Received?" and "Command Received?"
   protocol flowcharts.

   When receiving commands returns after first correct
   non-optional frame or after the time given by getcmd has
   elapsed.  This is instead of looping through main flowchart.

   When receiving responses returns on the first detected
   non-optional frame, after timeout T4, or on errors.

   Returns immediately if gets a +FCERROR response so can retry
   as data carrier.  Returns DCN as a valid frame instead of
   hanging up.

   Returns the command/response received, or EOF on timeout or
   error.
 
*/

int getfr ( TFILE *mf, uchar *buf, int getcmd )
{
  int err=0, frame=0, frlen, c, t ;
  char remoteid [ IDLEN + 1 ] ;
  time_t start ;
  uchar *fif=buf+3 ;
  
  start = 10*time(0) ;
  
  t = getcmd ? ( getcmd > 1 ? getcmd : T2 ) : T4 ;

 Enter:

  err = 0 ;

  if ( nframes++ ) {
    c = cmd ( mf, "+FRH=3", t ) ;
  } else {
    c = CONNECT ;		/* implied by ATA or ATD */
  }
  
  switch ( c ) {
  case EOF:			/* time out */
    tput ( mf, CAN_STR, 1 ) ;
    ckcmd ( mf, 0, 0, TO_ABRT, OK ) ;
    err = 1 ;
    break ;
  case NO:			/* S7 time out */
    err = 1 ;
    break ;
  case MODULATION:		/* data carrier (or DHS) */
    return -msg ( "W-2 wrong carrier" ) ;
    break ;
  case CONNECT:			/* frame */
    break ;
  case OK:
    break;
  default:			/* shouldn't happen */
    err = msg ( "E3wrong response to receive-frame command" ) ;
    break ;
  }
  
  if ( ! err ) 
  {
    err = receive_frame_data ( mf, buf, MAXFRLEN, &frlen ) ;
  
  if ( ! err && frlen < 3 ) 
    err = msg ( "E3received short frame (%d bytes)", frlen ) ;

  logfr ( "received", frname ( buf [ 2 ] ), buf, frlen ) ;
  }

  if ( ! err ) {

    frlen = fixframe ( buf, frlen, mf ) ;
    frame = buf [ 2 ] & 0x7f ;

    switch ( frame ) {
    case CRP:
      err = 1 ;
    case NSF:
    case NSC:
    case NSS:
      goto Enter ;
    case CIG:
    case CSI:
    case TSI:
      revcpy ( fif , (uchar*) remoteid ) ;
      msg ( "I- remote ID -> %*.*s", IDLEN, IDLEN, remoteid ) ;
#if defined(__APPLE__)
      {
	CFStringRef temp;
	CFMutableStringRef value;

	temp = CFStringCreateWithFormat(kCFAllocatorDefault,
			NULL, CFSTR("%*.*s"), IDLEN, IDLEN, remoteid);

	value = CFStringCreateMutableCopy(kCFAllocatorDefault,
			CFStringGetLength(temp), temp);
	CFRelease(temp);
	CFStringTrimWhitespace(value);

	notify(CFSTR("remoteid"), value);

	CFRelease(value);
      }
#endif
      goto Enter ;
    }

  }
  
  if ( err && getcmd && ( t -= 10*time(0) - start ) > 0 ) 
    goto Enter ;

  return err ? EOF : frame ;
}


/* Class 1 send/receive.  

  The logic in this function is a mess because it's meant to
  mirror the flowchart in ITU-T recommendation T.30 which is the
  protocol specification.

  */

int c1sndrcv ( 
	      TFILE *mf, cap local, char *localid, 
	      OFILE *outf, IFILE *inf, 
	      int pages, char *header, faxfont *font, 
	      int maxpgerr, int noretry, int calling )
{ 
  int err=0, rxpage=0, page=1, t, disbit, good=0, frame, last, nerr, nlines ;
  int rxdislen, ppm, try=0, pagetry=0, retry=0, remtx=0, remrx=0 ;
  int writepending=0, dp=0 ;
  cap remote = { DEFCAP }, session = { DEFCAP } ;
  char *fname=0 ;
  uchar buf [ MAXFRLEN ], *fif=buf+3 ;

  if ( ! calling ) goto RX ;

  /* Class 1 Transmitter: */

 T:  /* Transmitter Phase B - wait for DIS or DTC */

#if defined(__APPLE__)
  notify(CFSTR("sendnegotiate"), NULL);
#endif

  pagetry = 0 ;
  
  frame = getfr ( mf, buf, T1 ) ;
  
  if ( frame <= 0 ) {
    err = msg ( "E3no answer from remote fax" ) ;
    goto B ;
  }
  
  if ( frame != DIS && frame != DTC ) {
    msg ( "W2 can't open page" ) ;
    goto C ;
  }

  disbit = ( frame == DIS ) ? 0x80 : 0x00 ;
  try = 0 ;

 A:				/* decide to send or receive after DIS/DTC */

  if ( frame == DIS || frame == DTC ) {
    rxdislen = dislen ( fif ) ;
    mkcap ( fif, remote, 1 ) ;
    remtx = fif[1] & 0x80 ;
    remrx = fif[1] & 0x40 ;
  }

  msg ( "N remote has %sdocument(s) to send, and can %sreceive",
       remtx ? "" : "no ", remrx ? "" : "not " ) ;

  if ( pages > 0 ) {
    if ( ! remrx ) msg ( "W remote cannot receive, trying anyways" ) ; 
    goto D ;
  } else {
    if ( ! remtx ) msg ( "W remote has nothing to send, trying anyways" )  ; 
    goto R ;
  }

 D:				/* send DCS */

  if ( rdpage ( inf, dp, &ppm, local, 0 ) ) {
    err = msg ( "E2can't open page" ) ;
    goto B ;
  }

 D_2:

  mincap ( local, remote, session ) ;

  revcpy ( (uchar*) localid, fif ) ;
  if ( ! err ) 
    err = putframe ( TSI | MORE_FR | disbit, buf, IDLEN, mf, -1 ) ;  

  mkdis ( session, fif, DCSLEN, 0, pages ) ;
  if ( ! err ) 
    err = putframe ( DCS | SUB_FR | disbit, buf, DCSLEN, mf, -1 ) ;

#ifdef USEFTS
  if ( cmd ( mf, "+FTS=" MODDLY, T3S ) != OK )
#endif
    msleep ( TMOD ) ;		/* if +FTS not supported */

  if ( ! err ) 
    err = puttrain ( mf, c1cmd[SND][TRN][session[BR]], 
		    TCFSECS*cps [ session[BR] ] ) ;
  try++ ;

  if ( ! err ) {
    cmd ( mf, "+FRS=1", T3S ) ; /* wait for TCF carrier to drop */
    frame = getfr ( mf, buf, 0 ) ;
  }

  if ( err || frame < 0 ) {
    if ( try >= 3 ) {
      goto C_timeout ;
    } else { 
      goto D_2 ;
    }
  }
  
  switch ( frame ) {

  case DIS:
  case DTC:
    if ( try >= 3 ) goto C_timeout ;
    else goto A ;

  case FTT:
    msg ( "I channel not usable at %d bps", 8*cps[session[BR]] ) ;
    remote[BR] = fallback[session[BR]] ;
    if ( remote[BR] >= 0 ) goto D_2 ;
    else { err = msg ( "E2 channel not usable at lowest speed" ) ; goto C ; }

  case CFR:
    goto I_2 ;

  default:
    err = msg ( "E3 invalid response to DCS (0x%02x)", frame ) ;
    goto C ;
  }    

 I:				/* send a page */

  if ( rdpage ( inf, dp, &ppm, local, 0 ) ) {
    err = msg ( "E2can't open page" ) ;
    goto B ;
  }

 I_2:

  ckcmd ( mf, &err, c1cmd [SND][DTA][session[BR]], TO_FT, CONNECT ) ;
  if ( !err ) {
#if defined(__APPLE__)
      notify(CFSTR("sending"), NULL);
#endif
    msleep ( 1000 ) ;
    err = send_data ( mf, inf, page, pages, local, session, header, font ) ;
  }

  pagetry++ ;

  if ( !err )
    err = end_data ( mf, session, 0, 0 ) ;
  
#ifdef USEFTS
  if ( cmd ( mf, "+FTS=" MODDLY, T3S ) != OK )
#endif
    msleep ( TMOD ) ;		/* if +FTS not supported */

				/* fix ppm if on last page of stdin */
  if ( lastpage ( inf ) ) ppm = EOP ;

  try = 0 ;
 sendppm:
  if ( !err ) err = putframe ( ppm | disbit, buf, 0, mf, -1 ) ;
  try++ ;
  
  frame = getfr ( mf, buf, 0 ) ;
  if ( frame < 0 ) {
    if ( try >= 3 ) {
      goto C_timeout ;
    } else { 
      goto sendppm ;
    }
  }

  fname = inf->page->fname ;

  switch ( noretry ? MCF : frame ) { /* common retry logic */
  case MCF:
  case RTP:
  case PIP:
    fname = inf->page->fname ;
    if ( fname ) msg ( "Isent -> %s", fname ) ;
    pagetry=0 ;
#if defined(__APPLE__)
    {
      msg ( "Isent page %d", page ) ;

      CFNumberRef value;
      value = CFNumberCreate(kCFAllocatorDefault,
			kCFNumberIntType, &page);

      notify(CFSTR("sentpage"), value);

      CFRelease(value);
    }
#endif
    page++ ;
    dp = 1 ;
    break ;
  case PIN:
  case RTN:
    dp = 0 ;
    if ((retry = pagetry < NTXRETRY) == 0)
    {
      /* Step down in speed and reset the page try counter */
      if ((remote[BR] = fallback[session[BR]]) < 0)
      {
        err = msg ( "E2 channel not usable at lowest speed" ) ; 
        goto C ;
      }
      pagetry = 0;
    }
    break ;
  default:  
    err = msg ( "E3invalid post-page response (0x%02x)", frame ) ;
    goto C ;
  }
  
  switch ( ppm ) {
    
  case MPS:
    switch ( frame ) {
    case PIN: goto E ;
    case PIP: goto E ;
    case MCF: goto I ;
    case RTP: goto D ;
    case RTN: goto D ;
    }

  case EOP:
    switch ( frame ) {
    case PIN: goto E ;
    case PIP: goto E ;
    case MCF: 
    case RTP: 
      nextipage ( inf, 1 ) ;	/* skip ahead to mark all files done */
      goto C ;
    case RTN: goto D ;
    }
    
  case EOM:
    switch ( frame ) {
    case PIN: goto E ;
    case PIP: goto E ;
    case MCF: 
    case RTP: 
      cmd ( mf, "+FRS=20", T3S ) ; /* wait for ppr carrier to drop */
      goto T ;
    case RTN: goto D ;
    }
    
  }  

 E:				/* ignore PIN and PIP */
  msg ( "W interrupt request ignored" ) ;
  try=0 ;
  goto A ;

  /* Class 1 Receiver */

 RX:

 R:  /* Receiver Phase B */

  if ( ! err ) err = wrpage ( outf, rxpage ) ;

  disbit=0x00 ;

  for ( t=0 ; !err && t<T1 ; t+=T2+10 ) {

    revcpy ( (uchar*) localid, fif ) ;
    if ( !err ) 
      err = putframe ( CSI | disbit | MORE_FR, buf, IDLEN, mf, -1 ) ;
    
    mkdis ( local, fif, DEFDISLEN, 1, pages ) ;
    if ( !err ) 
      err = putframe ( DIS | disbit | SUB_FR, buf, DEFDISLEN, mf, -1 ) ;

    frame = getfr ( mf, buf, 0 ) ;

    if ( frame > 0 ) {
      disbit = ( frame == DIS ) ? 0x80 : 0x00 ;
      goto F_2 ;
    }
  }
  if ( err ) goto C ;
  else goto C_timeout ;
  

 F:  /* get a command */

  last = frame ;
  frame = getfr ( mf, buf, 1 ) ;

  if ( writepending ) {		/* do postponed file close/open */
    writepending=0 ;
    err = wrpage ( outf, rxpage ) ;
    if ( err ) goto C ;
  }

  if ( frame < 0 ) {
    if ( frame == -2 ) goto getdata ; /* data carrier detected */
    if ( last == EOM ) goto R ; 
    else { err = msg ("E3 timed out waiting for command" ) ; goto B ; }
  }
  
 F_2:

#if defined(__APPLE__)
  notify(CFSTR("recvnegotiate"), NULL);
#endif

  switch ( frame ) {

  case DTC:
    goto D ;

  case DIS:
    try=0 ;
    goto A ;
    
  case DCS: 
    mkcap ( fif, session, 0 ) ;
    printcap ( "session", session ) ;

    gettrain ( mf, c1cmd [RCV][TRN][session[BR]], cps[session[BR]], &good ) ;

    if ( putframe ( ( good ? CFR : FTT ) | disbit, buf, 0, mf, -1 ) ||
	! good ) goto F ;

  getdata:
    
    outf->w=pagewidth[session[WD]];
    outf->h=0;
    outf->xres=204.0;
    outf->yres=vresolution[session[VR]];
    nlines = 0 ;
    nerr = 0 ;
    
  re_getdata:

    if ( cmd ( mf, c1cmd [RCV][DTA][session[BR]], TO_FT ) != CONNECT ) 
      goto F ;			/* +FCERROR -> DCS resent */
    
#if defined(__APPLE__)
      notify(CFSTR("receiving"), NULL);
#endif

    switch ( receive_data ( mf, outf, session, &nerr, &nlines) ) {
    case 0:
      good = nerr < maxpgerr ;
      msg ( "I-received -> %s", outf->cfname ) ;
      writepending=1 ;		/* ppm follows immediately, don't write yet */
      rxpage++ ;
#if defined(__APPLE__)
      {
	CFNumberRef value;

	value = CFNumberCreate(kCFAllocatorDefault,
				kCFNumberIntType, &rxpage);
	notify(CFSTR("recdpage"), value);

	CFRelease(value);
      }
#endif
      break ;
    case 1:
      /* no RTC */
      if (nlines >= 10 && nerr < nlines / 2)
      {
        /* we received at least 10 scan lines and less than half of them had errors */
        msg ("W3 Missing RTC (%d good lines / %d bad lines)", nlines, nerr) ;
        good = nerr < maxpgerr ;
        msg ( "I-received -> %s", outf->cfname ) ;
        writepending=1 ;		/* ppm follows immediately, don't write yet */
        rxpage++ ;
#if defined(__APPLE__)
      {
	CFNumberRef value;

	value = CFNumberCreate(kCFAllocatorDefault,
				kCFNumberIntType, &rxpage);
	notify(CFSTR("recdpage"), value);

	CFRelease(value);
      }
#endif
      }
      else
      {
        /* re-issue +FRM command */
        msg ("W3 Missing RTC, re-getting data (%d good lines / %d bad lines)", nlines, nerr) ;
        goto re_getdata ;
      }
      break ;

    default:
      good = 0 ;
      break ;
    }
    ckcmd ( mf, 0, 0, TO_RTCMD, NO ) ;
    goto F ;

    /* III: */

  case PRI_EOM:
  case PRI_MPS:
  case PRI_EOP:
    frame &=0xf7 ;		/* ignore PRocedure Interrupt bit */
  case MPS:
  case EOP:
  case EOM:
    putframe ( ( good ? MCF : RTN ) | disbit, buf, 0, mf, -1 ) ;
    if ( good && frame == MPS ) goto getdata ;
    else goto F ;
    
  case DCN:
    goto B ;
    
  default:
    err = msg ( "E3 unrecognized command" ) ;
    goto B ;

  }

 C_timeout:
  err = msg ( "E3 no command/response from remote" ) ;

 C:
  putframe ( DCN, buf, 0, mf, -1 ) ;

 B:
  ckcmd ( mf, 0, "H", TO_RESET, OK ) ;	/* hang up */

  if ( rxpage > 0 ) 
    wrpage ( outf, -1 ) ;	/* remove last file */

  return err ;
}


/* Check for hangup message.  Assumes hsc is initialized to a
   negative value.  Returns 0 if no hangup message, 1 if there
   was one.  If perr is not null, sets it to 2 if the hsc was
   non-zero (error). */

int gethsc ( int *hsc, int *perr )
{
  int err=0, i ;
  if ( sresponse ( "+FHNG:", hsc ) || sresponse ( "+FHS:", hsc ) ) {
    if ( hsc && *hsc > 0 ) {
      err = msg ( "E2abnormal termination (code %d)", *hsc ) ;
      for ( i=0 ; c2msg[i].min >= 0 ; i++ ) {
	if ( *hsc >= c2msg[i].min && *hsc <= c2msg[i].max ) {
	  msg ( "E %s", c2msg[i].msg ) ;
	}
      }
      if ( perr && ! *perr ) {
	*perr = 2 ;
      }
    } else {
      err = 1 ;
    }
  }
  return err ;
}


/* Print remote ID and store DCS values in session as per
   responses since last command. */

void getc2dcs ( cap session )
{
  char *p ;
  if ( ( p = sresponse ( "+FTI:",  0 ) ) != 0 ||  
       ( p =  sresponse ( "+FTSI:", 0 ) ) != 0 ) {
    msg ( "I- remote ID -> %s", p ) ;
#if defined(__APPLE__)
    {
      CFStringRef temp;
      CFMutableStringRef value;

      temp = CFStringCreateWithCString(kCFAllocatorDefault,
			p, kCFStringEncodingUTF8);

      value = CFStringCreateMutableCopy(kCFAllocatorDefault,
			CFStringGetLength(temp), temp);
      CFRelease(temp);

      CFStringTrimWhitespace(value);
      CFStringTrim(value, CFSTR("\""));
      CFStringTrimWhitespace(value);

      notify(CFSTR("recvbegin"), value);

      CFRelease(value);
    }
#endif
  }
  if ( ( p = sresponse ( "+FCS:", 0 ) ) != 0 || 
      ( p = sresponse ( "+FDCS:", 0 ) ) != 0 ) {
    str2cap ( p, session ) ;
    printcap ( "session", session ) ;
  }
}  

/* Wait for a starting character XON or DC2.  Display & ignore
   any other characters received. */

void getstartc ( TFILE *mf )
{
  int c, noise ;
  
  for ( noise=0 ; ( c = tgetc ( mf, TO_C2X ) ) != XON && c != DC2 ; noise++ ) {
    if ( c == EOF ) {
      msg ( "Wno XON/DC2 received after CONNECT") ;
      break ;
    } else { 
      msg ( "W-+%s", cname ( c ) ) ; 
      noise++ ; 
    }
  }
  
  if ( noise )
    msg ( "W  : %d characters received while waiting to send", noise ) ;
}  


/* Class 2 send and receive.  

   If calling, polls if no files to send, otherwise sends.  If
   not calling sends documents if files to send, else receives.

   When sending, issues +FDIS to change session parameters if
   file format changes, then sends +FDT followed by data and a
   post-page message determined by format of next page, if any.
   Retransmits each page up to NTXRETRY times.

   When receiving extracts file format from responses to +FDR or
   ATA and saves them in the file. Receives data to a file and
   sets page transfer status if too many errors.

   Returns 0 if OK or 2 on errors.  */


int c2sndrcv (
	      TFILE *mf, cap local, char *localid, 
	      OFILE *outf, IFILE *inf, 
	      int pages, char *header, faxfont *font, 
	      int maxpgerr, int noretry, int calling )
{
  int err=0, done=0, page, pagetry, nerr, c, dp=0 ;
  int ppm=0, good=0, hsc, changed ;
  int remtx=0 ;
  char *fname=0 ;
  cap session = { 0,0,0,0, 0,0,0,0 } ;
  char buf [ CMDBUFSIZE ] ;

  hsc=-1 ;			/* will be set >= 0 on hangup */

  if ( sresponse ( "+FPO", 0 ) ) {
    remtx = 1 ;
    msg ( "N remote has document(s) to send." ) ;
  }

  if ( calling ) {
    if ( pages ) goto send ;
    else goto poll ;
  } else {
    if ( pages ) goto pollserver ;
    else goto receive ;
  }

  /* Class 2 Send */

 pollserver:

  /* with +FLP[L]=1 the modem should accept +FDT. */

 send:
  
  page=1 ;
  
  pagetry=0 ;
  while ( ! err && ! done ) {

    err = rdpage ( inf, dp, &ppm, local, &changed ) ;

    if ( ! err && changed ) {
      snprintf ( buf, sizeof(buf), c20 ? "+FIS=%d,%d,%d,%d" : "+FDIS=%d,%d,%d,%d", 
	       local[0], local[1], local[2], local[3] ) ;
      ckcmd ( mf, 0, buf, TO_FT, OK ) ;
      if ( gethsc ( &hsc, &err ) ) {
	continue ;
      }
    }
    
    ckcmd ( mf, &err, "+FDT", -TO_C2B, CONNECT ) ;
    if ( err || gethsc ( &hsc, &err ) ) { 
      done=1 ; 
      continue ; 
    }

    getc2dcs ( session ) ; 

    if ( ! c20 ) getstartc ( mf ) ;

    send_data ( mf, inf, page, pages, local, session, header, font ) ;
    pagetry++ ;

    if ( c20 ) {
      end_data ( mf, session, ppm, &good ) ;
    } else {
      end_data ( mf, session, 0, 0 ) ;

      gethsc ( &hsc, &err ) ;

      if ( ! err && hsc < 0 ) {
	ckcmd ( mf, &err, ppm == EOP ? "+FET=2" : 
	       ppm == EOM ? "+FET=1" : "+FET=0" , TO_C2PP, OK ) ;
      }

      gethsc ( &hsc, &err ) ;

      if ( ! err && hsc < 0 ) {
	if ( sresponse ( "+FPTS:", &good ) ) {
	  good &= 1 ;		/* odd values mean received OK */
	} else {			/* no +FPTS and +FHNG probably NG */
	  good = gethsc ( 0, 0 ) ? 0 :  
	    msg ( "W1no +FPTS response, assumed received" ) ;
	}
      }

    }
    
    if ( noretry ) good = 1;
    
    if ( good ) {
      fname = inf->page->fname ;
      if ( fname ) msg ( "Isent -> %s", fname ) ;
      pagetry=0 ;
#if defined(__APPLE__)
      {
	CFNumberRef value;

	value = CFNumberCreate(kCFAllocatorDefault,
			kCFNumberIntType, &page);

	notify(CFSTR("sentpage"), value);

	CFRelease(value);
      }
#endif
      page++ ;
      dp = 1 ;
      if ( ppm == EOP ) {
	nextipage ( inf, 1 ) ;	/* skip ahead to mark all files done */
	done = 1 ;
      }
    } else {
      dp = 0 ;
      if ( pagetry >= NTXRETRY )
	err = msg ( "E2too many page send retries" ) ;
    }

    if ( gethsc ( &hsc, &err ) )  done=1 ;

    if ( good && lastpage ( inf ) ) {
      done = 1 ;
    }
  }

  goto done ;

  /* Class 2 Receive */

 poll:

  /* with +FSP[L]=1 and +FPO[LL]: the modem should now accept +FDR. */

 receive:

  getc2dcs ( session ) ;	/* get ATA responses */

  done=0 ;
  for ( page=0 ; ! err && ! done ; page++ ) {

    if ( ! ( err = wrpage ( outf, page ) ) ) {
      c = cmd ( mf, "+FDR", -TO_C2R ) ;

      switch ( c ) {

      case CONNECT:
   	getc2dcs ( session ) ; 

   	outf->w=pagewidth[session[WD]];
   	outf->h=0;
	outf->xres=204.0;
	outf->yres=vresolution[session[VR]];
	nerr = 0 ;
	
	tput ( mf, &startchar, 1 ) ;

	if ( receive_data ( mf, outf, session, &nerr, NULL) == 0 ) {
	  good = nerr < maxpgerr ;
	  msg ( "I-received -> %s", outf->cfname ) ;
#if defined(__APPLE__)
	  {
	    CFNumberRef value;
	    int pageNum = page+1;

	    value = CFNumberCreate(kCFAllocatorDefault,
				kCFNumberIntType, &pageNum);

	    notify(CFSTR("recdpage"), value);

	    CFRelease(value);
	  }
#endif
	} else { 
	  good = 0 ;
	}
	
	ckcmd ( mf, &err, 0, TO_C2EOR, OK ) ;

	if ( err || gethsc ( &hsc, &err ) )  { 
	  wrpage ( outf, page+1 ) ;
	  wrpage ( outf, -1 ) ;
	  done=1 ; 
	  continue ; 
	}
	
	if ( ! good ) {
	  msg ( "Wreception errors" ) ;
	  ckcmd ( mf, 0, c20 ? "+FPS=2" : "+FPTS=2",  T3S, OK ) ;
	  if ( gethsc ( &hsc, &err ) ) continue ;
	}
	break ;

      case OK:
	wrpage ( outf, -1 ) ;	/* no more pages */
	done=1 ;
	if ( gethsc ( &hsc, &err ) ) continue ;
	break ;

      default:
	wrpage ( outf, -1 ) ;	/* oops */
	err = msg ( "E3receive (+FDR) command failed") ;
	break ;
      }
    }
  } 

  
 done:
  if ( hsc < 0 ) ckcmd ( mf, 0, c20 ? "+FKS" : "+FK", TO_RESET, OK ) ;

  return err ;
}


/* Dial the phone number given by string s.  If nowait is true
   adds a ';' to the dial string to avoid waiting for a
   CONNECTion (might allow ersatz polling).  Also resets the
   global "nframes" if appropriate so getfr() and putframe() know
   not to issue +FRH/+FTH. Returns 0 if dialed OK, 1 if busy, 2
   on errors.  */

int dial ( TFILE *f, char *s, int nowait )
{
  int err=0, hsc=-1 ;
  char c, dsbuf [ 128 ], *p ;

  snprintf ( dsbuf, sizeof(dsbuf), nowait ? "D%.126s;" : "D%.127s" , s ) ;
  msg ( "Idialing %s", dsbuf+1 ) ;

#if defined(__APPLE__)
  {
    CFStringRef value;

    value = CFStringCreateWithCString(kCFAllocatorDefault,
			dsbuf+1, kCFStringEncodingUTF8);

    notify(CFSTR("dial"), value);

    CFRelease(value);
  }
#endif
  
  c = cmd ( f, dsbuf, TO_A ) ;

  if ( ( p = sresponse ( "+FCSI:", 0 ) ) != 0 ||
       ( p =  sresponse ( "+FCI:", 0 ) ) != 0 ) {
    msg ( "I- remote ID -> %s", p ) ;
#if defined(__APPLE__)
    {
      CFStringRef value;	

      value = CFStringCreateWithCString(kCFAllocatorDefault,
			p, kCFStringEncodingUTF8);

      notify(CFSTR("sendbegin"), value);

      CFRelease(value);
    }
#endif
  }

  if ( nowait && c == OK ) {
    msg ( "Icalled" ) ;
    nframes = 1 ;
  } else if ( c1 && c == CONNECT ) {
    msg ( "Iconnected" ) ; 
    nframes = 0 ;
  } else if ( !c1 && c == OK ) {
    msg ( "Iconnected" ) ; 
  } else if ( c ==  BUSY ) {
    err = msg ( "W1number is busy" ) ; 
  } else {
    err = msg ( "E2dial command failed" ) ;
  }

  gethsc ( &hsc, err ? 0 : &err ) ;

  return err ;
}


/* Figure out which mode the modem answered in (fax, data, voice
   or none) based on modem class and responses to the previous
   command.  Sets crate (connect rate) for DATAMODE and hsc
   (hangup status code) if detects a class 2 hangup message. */

enum connectmode { NONE, DATAMODE, FAXMODE, VOICEMODE } ; 

enum connectmode ansmode ( int *crate, int *hsc )
{
  enum connectmode mode = NONE ;
  int x=0 ;

  if ( c1 && sresponse ( "CONNECT", &x ) ) {
    mode = x ? DATAMODE : FAXMODE ;
  }

  if ( !c1 && sresponse ( "OK", 0 ) ) {
    mode = FAXMODE ;
  } 

  if ( !c1 && ( sresponse ( "CONNECT", &x ) || sresponse ( "+FDM", 0 ) ) ) {
    mode = DATAMODE ;
  } 

  if ( sresponse ( "DATA", 0 ) || sresponse ( "CONNECT DATA", 0 ) ) {
    mode = DATAMODE ;
    sresponse ( "CONNECT", &x ) ;
  }

  if ( sresponse ( "FAX", 0 ) || sresponse ( "+FCO", 0 ) ) {
    mode = FAXMODE ;
  }

  if ( sresponse ( "VCON", 0 ) ) {
    mode = VOICEMODE ;
  }
  
  if ( gethsc ( hsc, 0 ) ) {
    mode = NONE ;
  }

  if ( DATAMODE && x ) *crate = x ;

  return mode ;
}


/* Answer the phone.  Remove our lock if sharing device with
   outgoing calls.  If waiting for call, wait for modem activity,
   else answer phone.  Figure out what mode we answered in and
   handle call appropriately.  Re-lock if necessary. Exec *getty
   or *vcmd for data or voice calls. */

int answer ( TFILE *f, char **lkfile, 
	    int wait, int share, int softaa, 
	    char *getty, char *vcmd, char *acmd )
{
  int err=0, c ;
  int crate=19200, hsc=-1, i ;
  enum connectmode mode=NONE ;

  if ( ! err && share ) {
    err = ttymode ( f, COMMAND ) ;
    if ( ! err ) 
      err = unlockall ( f, lkfile ) ;
  }
  
  if ( wait ) {
    /* Set flag to indicate we're blocked waiting for activity and
    *  check to see if we received a SIGHUP during modem setup.
    */
    waiting = 1;
    if (sighup)
      err = msg ( "E2exiting on SIGHUP" ) ; 

    /*
     * If the manual answer command came in while we were initializing
     * the modem then switch from wait mode to answer mode.
     */

    if (manual_answer) {
      manual_answer = 0;
      wait = 0;
    }
  }

  if ( ! err && wait ) {
    msg ( "Iwaiting for activity") ;
    answer_wait = 1;

#if defined(__APPLE__)
    notify(CFSTR("recvidle"), NULL);
#endif

    if (tdata ( f, -1 ) == TDATA_SLEEP)
      return ( TDATA_SLEEP ) ;		/* machine is about to sleep... */

    /*
     * If the manual answer command came in while we were waiting 
     * for activity then switch from wait mode to answer mode.
     */

    if (manual_answer) {
      manual_answer = 0;
      wait = 0;
    }

    msg ( "Iactivity detected") ;
    waiting = 0;
  }
  
  if ( ! err && share ) {
    msleep ( 500 ) ;		/* let other programs lock port  */
    err = lockall ( f, lkfile, 1 ) ;
    if ( err )
      err = msg ( "W1can't answer: can't lock device" ) ;
    else
      err = ttymode ( f, COMMAND ) ; /* in case it was changed silently */
  }

  for ( i=0 ; ! err && mode == NONE && ( i==0 || ( i==1 && softaa ) ) ; i++ ) {

#if defined(__APPLE__)
  if (!wait)
    notify(CFSTR("answering"), NULL);
#endif

    c = cmd ( f, wait ? 0 : acmd, ( i==0 && softaa ) ? TO_DATAF : TO_A ) ;

    /*
     * We no longer want tdata() to return early for manual answer commands...
     */

    answer_wait = 0;

    /*
     * If the manual answer command came in while the phone was ringing 
     * then issue the answer command.
     */

    if ( c == EOF && wait && manual_answer ) {
#if defined(__APPLE__)
      notify(CFSTR("answering"), NULL);
#endif
      manual_answer = 0;
      wait = 0;
      c = cmd ( f, acmd, ( i==0 && softaa ) ? TO_DATAF : TO_A ) ;
    }

    if ( c == DATA ) cmd ( f, c1 ? "O" : 0, TO_A ) ; /* +FAE=1 weirdness */

    mode = ansmode ( &crate, &hsc ) ;
    
    switch ( mode ) {
    case DATAMODE :
      msg ( "Idata call answered") ;
      if ( getty && *getty ) {
	char buf [ MAXGETTY ] ;
	if ( ckfmt ( getty, 6 ) ) {
	  err = msg ( "E3 too many %%d escapes in command (%s)", getty ) ;
	} else {
	  snprintf ( buf, sizeof(buf), getty, crate, crate, crate, crate, crate, crate ) ;
	  msg ( "Iexec'ing /bin/sh -c \"%s\"" , buf ) ;
	  execl ( "/bin/sh" , "sh" , "-c" , buf , (void*) 0 ) ; 
	  err = msg ( "ES2exec failed:" ) ;
	}
      } else {
	err = msg ( "E2no getty command defined for data call" ) ;
      }
      break ; 
    case FAXMODE :
      nframes = 0 ;
      msg ( "Ifax call answered") ;
      break ;
    case VOICEMODE :
      msg ( "Ivoice call answered") ;
      if ( vcmd && *vcmd ) {
	char buf [ MAXGETTY ] ;
	if ( ckfmt ( vcmd, 6 ) ) {
	} else {
	  snprintf ( buf, sizeof(buf), vcmd, f->fd, f->fd, f->fd, f->fd, f->fd, f->fd ) ;
	  msg ( "Iexec'ing /bin/sh -c \"%s\"" , buf ) ;
	  execl ( "/bin/sh" , "sh" , "-c" , buf , (void*) 0 ) ; 
	  err = msg ( "ES2exec failed:" ) ;
	}
      } else {
	err = msg ( "E2no voice command defined for voice call" ) ;
      }
      break ; 
    case NONE:
      if ( i==0 && softaa && hsc < 0 && getty && *getty ) {
	int j ;			/* switch to fax for 2nd try */
	for ( j=0 ; j<3 ; j++ ) 
	  if ( cmd ( f, c1 ? "+FCLASS=1" : 
		    ( c20 ? "+FCLASS=2.0" : "+FCLASS=2" ), -TO_RESET ) 
	      == OK ) break ; 
	wait = 0 ;
	acmd = ANSCMD ;
      } else {
	err = msg ( "E3unable to answer call") ;
      }
      break ;
    default:
      err = msg ( "E3can't happen(answer)" ) ;
      break ;
    }
    
  }

  return err  ;
}


/* Initialize modem.  Determine class to use and issue
   class-specific fax initialization commands. If poll is true,
   issues commands to enable polling also.  Returns 0 or 3 if a
   mandatory setup command fails. */

int modem_init ( TFILE *mf, cap c, char *id, 
		 int calling, int poll, int capsset, int *preverse )
{
  int err=0, t=-TO_RESET ;
  char buf [ CMDBUFSIZE ], model [ CMDBUFSIZE ] = "" ;
  char **p, *q, *modelq [2][4] = { { "+FMFR?", "+FMDL?", 0 }, 
				   { "+FMI?", "+FMM?", "+FMR?", 0 } } ;

  err = msg ( "Iinitializing modem" ) ;

#if defined(__APPLE__)
      notify(CFSTR("modeminit"), NULL);
#endif

  /* diasable command echo and get firmware revision */

  cmd ( mf, "E0", t ) ;

  if ( cmd ( mf, "I3", t ) == OK ) {
    getresp ( "", model, CMDBUFSIZE-1 ) ;
    strcat ( model, " " ) ;
  }

  /* if not already chosen, pick safest class; set it */

  if ( ! err && ! c1 && !c2 && ! c20 ) {
    if ( cmd ( mf, "+FCLASS=?", t ) != OK ) {
      err = msg ("E3 modem does not support fax" ) ;
    } else {
      if ( strinresp ( "2.0" ) ) c20 = 1 ;
      else if ( strinresp ( "2" ) ) ;
      else if ( strinresp ( "1" ) ) c1 = 1 ;
      else err = msg ("E3 can't determine fax modem class support" ) ;
      if ( strstr ( model, "Sportster" ) ) { /* USR Sporsters are buggy */
	c1 = 1 ; 
	c2 = c20 = 0 ;
      }
    }
  }

  ckcmd ( mf, &err, c1 ? "+FCLASS=1" : 
       ( c20 ? "+FCLASS=2.0" : "+FCLASS=2" ), t, OK ) ;

  /* get make & model if using Class 2 or 2.0 */

  if ( ! c1 ) {
    for ( p = modelq [ c20 ] ; ! err && *p ; p++ ) {
      if ( cmd ( mf, *p, t ) == OK ) {
	getresp ( "", model, CMDBUFSIZE-1 ) ;
	strcat ( model, " " ) ;
      }
    }
  
    if ( ! c1 && strstr ( model, "Multi-Tech" ) ) {
      *preverse = 0 ;
      msg ("I Multi-Tech bit order set" ) ;
    }
  }

  if ( ! err ) 
    msg ( "I using %sin class %s", model, c1 ? "1" : c20 ? "2.0" : "2" ) ;

  /* get maximum modem speed if not already set (Class 1 only) */

  if ( ! err && c1 && ! capsset ) {
    int i ;
    char *c1spstr [6] = { "24", "48", "72", "96", "121", "145" } ;
    ckcmd ( mf, &err, calling ? "+FTM=?" : "+FRM=?", t, OK ) ;
    for ( i=0 ; i < 6 && strinresp ( c1spstr[i] ) ; i++ ) ;
    c[1]=i?i-1:0;
  }

  /* issue essential commands and set/get modem capabilities (Class 2 only) */

  if ( ! err && ! c1 ) {

    if ( c20 ) {
      ckcmd ( mf, 0, "+FIP", t, OK ) ;
      ckcmd ( mf, 0, "+FNR=1,1,1,0", t, OK ) ;

      ckcmd ( mf, 0, "+FLO=1", t, OK ) ;
      ckcmd ( mf, 0, "+FBO=0", t, OK ) ;
    }

    ckcmd ( mf, &err, "+FCR=1", t, OK ) ;

    if ( ! capsset ) {
      if ( cmd ( mf, c20 ? "+FIS?" : "+FDIS?", -t ) == OK &&
	   ( q = strinresp ( "," ) ) ) {
	str2cap ( q-1, c ) ;
      } else {
	msg ( "W can't get modem capabilities, set to default" ) ;
	capsset = 1 ;
      }
    }

    if ( capsset ) {
      snprintf ( buf, sizeof(buf), c20 ? "+FCC=%d,%d,%d,%d,%d,%d,%d,%d" : 
		"+FDCC=%d,%d,%d,%d,%d,%d,%d,%d", 
		c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7] ) ;
      ckcmd ( mf, 0, buf, -t, OK ) ;
    }
    
    snprintf ( buf, sizeof(buf), c20 ? "+FLI=\"%.*s\"" : "+FLID=\"%.*s\"" , 
	     CMDBUFSIZE-9, id ) ;
    ckcmd ( mf, 0, buf, -t, OK ) ;

    if ( ! err && poll ) {

      ckcmd ( mf, 0, c20 ? "+FSP=1" : "+FSPL=1", -t, OK ) ;

      snprintf ( buf, sizeof(buf), c20 ? "+FPI=\"%.*s\"" : "+FCIG=\"%.*s\"" , 
	       CMDBUFSIZE-9, id ) ;
      ckcmd ( mf, 0, buf, -t, OK ) ;

    }
  }

  // (2003-02-04, ggs) If we're faxing in Class 1, try to turn on adaptive receive.
  //   Ignore the result (in case the modem doesn't support adaptive receive).
  if ( ! err && c1 ) {
    cmd ( mf, "+FAR=1", t );
  }

  return err ;
}


/* the following are global so can terminate properly on signal */

char *icmd[3][ MAXICMD+1 ] = {{0},{0},{0}} ; /* initialization commands */
TFILE faxdev = { -1, onsig, 0,0, {0}, 0, 0, 0 } ;	/* modem */
char *lkfile [ MAXLKFILE+1 ] = {0} ; /* lock file names */
IFILE ifile = { 0 } ;		/* files being sent */
int locked = 0 ;		/* modem locked */


/* print names of files not sent and reset modem before
   exiting. */

int cleanup ( int err )
{
#ifdef __APPLE__
  sysEventMonitorStop();
#endif
				/* log names of files not sent */
  logifnames ( &ifile, "I failed -> %s" ) ;

  if ( ! locked && faxdev.fd >= 0 )
  {
    ttymode ( &faxdev, COMMAND ) ;
    end_session ( &faxdev, icmd[2], lkfile, err != 4 && err != 6 ) ;

#if defined(__APPLE__)
  {
    CFNumberRef value = CFNumberCreate(kCFAllocatorDefault,
				kCFNumberIntType, &err);
    notify(CFSTR("exit"), value);
    CFRelease(value);
  }
#endif
  }
  
  ttyclose ( &faxdev ) ;

  msg ( "Idone, returning %d (%s)", err, 
	errormsg [ err >= 0 && err <= 6 ? err : 7 ] ) ;

  return err ;
}


/* signal handler */
static void signal_handler(int sig)
{
  faxdev.signal = sig;
}

void onsig ( int sig ) 
{ 
  if ( sig == SIGHUP )
    sighup = 1;

  /* If it's a SIGHUP but we're not idle ignore the signal.
   */
  if ( sig == SIGHUP && !waiting )
    msg ( "E ignoring signal %d", sig ) ; 
  else
  {
#if defined(__APPLE__)
    if (!waiting)
    {
      CFNumberRef value;

      value = CFNumberCreate(kCFAllocatorDefault,
			kCFNumberIntType, &sig);

      notify(CFSTR("abort"), value);

      CFRelease(value);
    }
#endif

    msg ( "E terminating on signal %d", sig ) ; 
    exit ( cleanup ( 5 ) ) ;
  }
}


/* Fax send/receive program for Class 1, 2 and 2.0 fax
   modems. Returns 0 on success, 1 if number busy or device
   locked, 2 for errors, 3 for protocol errors, 4 if no modem
   response, 5 on signal. */

int main( int argc, char **argv)
{
  int err=0, doneargs=0, c=0, i ;
  int testing=0, calling=0 ;

  int nicmd[3]={0,0,0}, nlkfile=0, nverb=0 ;

  int softaa=0, share=0, wait=0, reverse=1, ignerr=0, noretry=0, hwfc=0 ;
  int capsset=0 ;
  char *getty = "", *vcmd = "", *acmd=ANSCMD ;

  cap local = { DEFCAP } ;
  char localid  [ IDLEN + 1 ] = DEFID ;

  int maxpgerr = MAXPGERR ;

  time_t now ;
  char *header = 0, headerbuf [ MAXLINELEN ] ; 
  char *fontname = 0 ;
  faxfont font ;

  OFILE ofile ;
  int pages = 0 ;
  char *phnum="", *ansfname = DEFPAT ;
  char fnamepat [ EFAX_PATH_MAX ] ;

  /* Don't buffer stdout and stderr */
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  /* print initial message to both stderr & stdout */
  argv0 = argv[0] ;
  verb[1] = "ewia" ;
  (void) setCopyright( Version " " Copyright ". Compiled "__DATE__ " " __TIME__ "\n" ); 
  argv0 = efaxbasename ( argv0 ) ;
  verb[1] = "" ;

  setlocale ( LC_ALL, "" ) ;

  while ( ! err && ! doneargs &&
	 ( c = nextopt ( argc,argv,
			"a:c:d:e:f:g:h:i:j:k:l:o:p:q:r:st:v:wx:T" ) ) != -1 ) {

    switch (c) {
    case 'a': 
      acmd = nxtoptarg ;
      break ;
    case 'c': 
      err = str2cap ( nxtoptarg, local ) ;
      capsset = 1 ;
      break ;
    case 'l': 
      if ( strlen ( nxtoptarg ) > IDLEN ) 
	msg("Wlocal ID (%s) truncated to %d characters", nxtoptarg, IDLEN ) ;
      if ( strspn ( nxtoptarg, " +0123456789" ) != strlen ( nxtoptarg ) )
	msg("Wlocal ID (%s) has non-standard characters", nxtoptarg ) ;
      snprintf ( localid, sizeof(localid), "%*.*s", IDLEN, IDLEN, nxtoptarg ) ;
      break ;
    case 'i': 
      if ( nicmd[0] < MAXICMD ) icmd[0][ nicmd[0]++ ] = nxtoptarg ;
      else err = msg ( "E2too many '-i' options"); 
      break ;
    case 'j': 
      if ( nicmd[1] < MAXICMD ) icmd[1][ nicmd[1]++ ] = nxtoptarg ;
      else err = msg ( "E2too many '-j' options"); 
      break ;
    case 'k': 
      if ( nicmd[2] < MAXICMD ) icmd[2][ nicmd[2]++ ] = nxtoptarg ;
      else err = msg ( "E2too many '-k' options"); 
      break ;
    case 'h': 
      header = nxtoptarg ; 
      break ;
    case 'f': 
      fontname = nxtoptarg ; 
      break ;
    case 'd': 
      faxfile = nxtoptarg ; 
      break ;
    case 'e': 
      vcmd = nxtoptarg ; 
      break ;
    case 'g': 
      getty = nxtoptarg ; 
      break ;
    case 'o':			/* most protocol options are globals */
      for ( ; *nxtoptarg ; nxtoptarg++ ) 
	switch ( *nxtoptarg ) {
	case '0' : c20 = 1 ; break ;
	case '1' : c1 = 1 ; break ;
	case '2' : c2 = 1 ; break ;
	case 'a' : softaa = 1 ;  break ;
	case 'e' : ignerr = 1 ;  break ;
	case 'f' : vfc = 1 ;  break ;
	case 'h' : hwfc = 1 ;  break ;
	case 'l' : lockpolldelay /= 2 ;  break ;
	case 'n' : noretry = 1 ;  break ;
	case 'r' : reverse = 0 ; break ;
	case 'x' : startchar = XON ; break ;
	case 'z' : cmdpause += T_CMD ; break ;
	 default : msg ( "Wunrecognized protocol option (%c)", *nxtoptarg ) ; 
	}
      break ;
    case 'q':
      if ( sscanf ( nxtoptarg , "%d", &maxpgerr ) != 1 || maxpgerr < 0 )
	err=msg ("E2bad quality (-q) argument (%s)", nxtoptarg ) ;
      break;
    case 'r': 
      ansfname = nxtoptarg ;
      break;
    case 's': 
      share = 1 ; 
      break;
    case 't': 
      calling=1;
      /* fall through */
    case 'p':
      if ( argv [ argc ] ) err = msg ("E2can't happen(unterminated argv)") ;
      if ( ! err ) err = newIFILE ( &ifile, argv + nxtoptind ) ;
      pages = argc - nxtoptind - ( c == 'p' ? 1 : 0 )  ;
      pages = pages < 0 ? 0 : pages ;
      phnum = nxtoptarg ;
      doneargs=1 ; 
      break;
    case 'v': 
      verb[nverb] = nxtoptarg ; 
      nverb=1;
      break ;
    case 'w': 
      wait = 1 ; 
      break ;
    case 'x': 
      if ( nlkfile < MAXLKFILE ) lkfile[ nlkfile++ ] = nxtoptarg ;
      else err = msg ( "E2too many lock files" ) ; 
      break ;
    case 'T':			/* test: begin+end session */
      testing=1;
      doneargs=1 ; 
      break ;
    default : fprintf ( stderr, Usage, argv0 ) ; err = 2 ; break ;
    }
  }

  for ( i=0 ; i<argc ; i++ ) 
    msg ( "Aargv[%d]=%s", i, argv[i]) ; 

  if ( ! nicmd[2] ) icmd[2][nicmd[2]++] = "H" ;	/* default -k command */

  readfont ( fontname, &font ) ;

  if ( ! header ) {
    char tmp [ MAXLINELEN ] ;
    char localid_tmp [ (IDLEN * 2) + 1 ], *src, *dst;

    now = time ( 0 ) ;
    strftime ( tmp, MAXLINELEN, "%c %%s   P. %%%%d", localtime ( &now ) ) ;

    /* Escape any percent signs in localid... */
    src = localid;
    dst = localid_tmp;
    while (*src)
    {
      if ((*dst++ = *src++) == '%')
        *dst++ = '%';
    }
    *dst++ = '\0';

    snprintf ( header = headerbuf, sizeof(headerbuf), tmp, localid_tmp ) ;
  }

#ifdef __APPLE__
  if ( ! err ) sysEventMonitorStart();
#endif

SLEEP_RETRY:

  if ( ! err ) {
    err = begin_session ( &faxdev, faxfile, 
			 !c1 && !c20 && reverse, /* Class 2 rx bit reversal */
			 hwfc, lkfile, COMMAND, signal_handler, wait) ;
    if ( ! err ) err = setup ( &faxdev, icmd[0], ignerr ) ;

    if ( ! err ) err = modem_init ( &faxdev, local, localid, 
				    calling, calling && !pages, capsset,
				    &reverse ) ;
    if ( ! err ) err = setup ( &faxdev, icmd[1], ignerr ) ;
    if ( err == 1 ) locked = 1 ;
  }

  if ( ! err && ! testing ) {

    if ( calling ) {
      err = dial ( &faxdev, phnum, 0 ) ;
    } else {
      err = answer ( &faxdev, lkfile, wait, share, softaa, 
		    getty, vcmd, acmd ) ;
      if ( err == 1 ) locked = 1 ;

    }

    if ( ! err ) {
    now = time(0) ;		/* do it here so use reception time */
    strftime ( fnamepat, EFAX_PATH_MAX, ansfname, localtime ( &now ) ) ;
    strncat ( fnamepat, ".%03d", EFAX_PATH_MAX - strlen ( fnamepat ) ) ;
    newOFILE ( &ofile, O_TIFF_FAX, fnamepat, 0, 0, 0, 0 ) ;
    
      if ( c1 ) {
	err = c1sndrcv ( &faxdev, local, localid,
			&ofile, &ifile, pages, header, &font,
			maxpgerr, noretry, calling ) ;
      } else {
	err = c2sndrcv ( &faxdev, local, localid,
			&ofile, &ifile, pages, header, &font,
			maxpgerr, noretry, calling ) ;
      }

#if defined(__APPLE__)
      notify(CFSTR("disconnecting"), NULL);
#endif

    }
  }
#ifdef __APPLE__
      if ( err == TDATA_SLEEP )
      {
       /*
	* The system is going to sleep; reset the modem and close the port
	* before waiting for a wake notification.
	*/
	end_session ( &faxdev, icmd[2], lkfile, err != 4 && err != 6 ) ;
	ttyclose(&faxdev);
        err = 0;

	IOAllowPowerChange(sysevent.powerKernelPort, sysevent.powerNotificationID);

	msg("Isleeping...");

	waiting = 1;
	tdata ( &faxdev, -1 ) ;

	msg ( "Iwoke from sleep") ;
	goto SLEEP_RETRY;
      }
#endif

  return cleanup ( err ) ;
}
