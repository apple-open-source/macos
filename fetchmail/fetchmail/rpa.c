/***********************************************************************
  module:       rpa.c
  program:      fetchmail
  programmer:   Michael J. Palmer <106177.1156@compuserve.com>
  date:         29 August 1997
  compiler:     GCC 2.7.2
  environment:  RedHat 4.0 Linux 2.0.18
  description:  RPA authorisation code for POP3 client

  The sole entry point is POP3_auth_rpa()
 ***********************************************************************/

#include  "config.h"

#if defined(POP3_ENABLE) && defined(RPA_ENABLE)
#include  <stdio.h>
#include  <unistd.h>
#include  <ctype.h>
#include  <string.h> 

#include  "socket.h"
#include  "fetchmail.h"
#include  "md5.h"
#include  "i18n.h"

#ifdef TESTMODE
extern unsigned char line1[];
extern unsigned char line2[];
extern unsigned char line3[];

extern int linecount;
#endif

#ifndef NO_PROTO
  /* prototypes for internal functions */
  static int  POP3_rpa_resp(unsigned char* argbuf, int socket );
  static void LenAppend(unsigned char** pptr, int len);
  static int  LenSkip(unsigned char** pptr, int rxlen);
  static int  DecBase64(unsigned char* bufp);
  static void EncBase64(unsigned char* bufp, int len);
  static void ToUnicode(unsigned char** pptr, unsigned char delim,
			unsigned char* buf, int* plen, int conv);
  static int  SetRealmService(unsigned char* bufp);
  static void GenChallenge(unsigned char* buf, int len);
  static int  DigestPassphrase(unsigned char* passphrase,
			       unsigned char* rbuf, int unicodeit);
  static void CompUserResp();
  static int  CheckUserAuth();
  static void md5(unsigned char* in, int len, unsigned char* out);
#endif

/* RPA protocol definitions */

#define         EARLYVER        "\x01\x00"  /* Earliest supp version */
#define         LATEVER         "\x03\x00"  /* Latest supp version   */
#define         HDR             0x60        /* ASN.1 SEQUENCE        */
#define         MECH            "\x06\x09\x60\x86\x48\x01\x86\xF8\x73\x01\x01"
#define         FLAGS           "\x00\x01"  /* Mutual authentication */
#define         STRMAX          128         /* Bytes in Unicode      */
#define         Tsl             14          /* Timestamp bytelen     */
#define         Pul             16          /* Passphrase digest len */
#define         Cul             16          /* Usr challenge bytelen */
#define         Rul             16          /* Usr response bytelen  */
#define         Aul             16          /* User auth bytelen     */
#define         Kusl            16          /* Session key bytelen   */

#define         UNIPASS         1           /* 1=Unicode 0=iso8859   */
#define         PS_RPA          42          /* Return code           */

/* RPA authentication items */

unsigned char   Cs[256];                    /* Service challenge     */
int             Csl;                        /* Length of "    "      */
unsigned char   Ts[Tsl+1];                  /* Timestamp incl \0     */
unsigned char   Nu[STRMAX];                 /* Username in Unicode   */
int             Nul;                        /* Length of " in bytes  */
unsigned char   Ns[STRMAX];                 /* Service in Unicode    */
int             Nsl;                        /* Length of " in bytes  */
unsigned char   Nr[STRMAX];                 /* Realm in Unicode      */
int             Nrl;                        /* Length of " in bytes  */
unsigned char   Pu[Pul];                    /* Passphrase after MD5  */
unsigned char   Cu[Cul];                    /* User challenge        */
unsigned char   Ru[Rul];                    /* User response         */
unsigned char   Au[Aul];                    /* User auth from Deity  */
unsigned char   Kusu[Kusl];                 /* Obscured Session key  */
unsigned char   Kus[Kusl];                  /* Session key           */

/*********************************************************************
  function:      POP3_auth_rpa
  description:   send the AUTH RPA commands to the server, and
                 get the server's response. Then progress through the
                 RPA challenge/response protocol until we are
                 (hopefully) granted authorisation.
  arguments:
    userid       user's id@realm e.g. myuserid@csi.com
    passphrase   user's passphrase
                 (upper lower or mixed case as the realm has chosen.
                 spec allows various options :-(   )
    socket       socket to which the server is connected.

  return value:  zero if success, else non-zero.
  calls:         SockPrintf, POP3_rpa_resp, EncBase64, DecBase64,
                 LenAppend, GenChallenge
  globals:       read outlevel.
 *********************************************************************/

int POP3_auth_rpa (unsigned char *userid, unsigned char *passphrase, int socket)
{
    int      ok,rxlen,verh,verl,i,rll;
    unsigned char buf [POPBUFSIZE];
    unsigned char *bufp;
    int      status,aulin,kuslin;
    char* stdec[4] = { N_("Success") ,
		       N_("Restricted user (something wrong with account)") ,
		       N_("Invalid userid or passphrase") ,
		       N_("Deity error") };

    /* Initiate RPA authorisation */

    SockPrintf(socket,"AUTH RPA\r\n");

    if (outlevel >= O_MONITOR)
	report(stdout, "> AUTH RPA\n");

    /* Create unicode user name in Nu.              */
    /* Create MD5 digest of user's passphrase in Pu */

    bufp = userid;
    ToUnicode(&bufp, '@', Nu, &Nul, 1);  /* User (lowercase) */
    DigestPassphrase(passphrase, Pu, UNIPASS);

    /* Get + response from server (RPA ready) */

    if ((ok = POP3_rpa_resp(buf,socket)) != 0)
    {
	if (outlevel > O_SILENT && outlevel < O_MONITOR)
	    report(stdout, "%s\n",buf);

	return(ok);
    }

    /* Assemble Token 1 in buf */

    bufp    = buf;
    *bufp++ = HDR;
    LenAppend(&bufp,      17);
    memcpy(bufp, MECH,    11); bufp += 11;
    memcpy(bufp, EARLYVER, 2); bufp += 2;
    memcpy(bufp, LATEVER,  2); bufp += 2;
    memcpy(bufp, FLAGS,    2); bufp += 2;

    /* Send Token 1, receive Token 2 */

    EncBase64(buf, bufp-buf);
#ifndef TESTMODE
    SockPrintf(socket,"%s\r\n",buf);
#endif
    if (outlevel >= O_MONITOR)
	report(stdout, "> %s\n",buf);
    if ((ok = POP3_rpa_resp(buf,socket)) != 0)
    {
	if (outlevel > O_SILENT && outlevel < O_MONITOR)
	    report(stdout, "%s\n",buf);
	return(ok);
    }
    if ((rxlen = DecBase64(buf)) == 0)
    {
	if (outlevel > O_SILENT)
	    report(stderr, _("RPA token 2: Base64 decode error\n"));
	return(PS_RPA);
    }
    bufp = buf;
    *(buf+rxlen) = 0;  /* Terminates realm list */
    if (LenSkip(&bufp,rxlen) == 0) return(PS_RPA);

    /* Interpret Token 2 */

    verh = *(bufp++); verl = *(bufp++);
    if (outlevel >= O_DEBUG)
	report(stdout, _("Service chose RPA version %d.%d\n"),verh,verl);
    Csl  = *(bufp++);
    memcpy(Cs, bufp, Csl);
    bufp += Csl;
    if (outlevel >= O_DEBUG)
    {
	report(stdout, _("Service challenge (l=%d):\n"),Csl);
	for (i=0; i<Csl; i++)
	    report_build(stdout, "%02X ",Cs[i]);
	report_complete(stdout, "\n");
    }
    memcpy(Ts, bufp, Tsl);
    Ts[Tsl] = 0;
    bufp += Tsl;
    if (outlevel >= O_DEBUG)
	report(stdout, _("Service timestamp %s\n"),Ts);
    rll = *(bufp++) << 8; rll = rll | *(bufp++);
    if ((bufp-buf+rll) != rxlen)
    {
	if (outlevel > O_SILENT)
	    report(stderr, _("RPA token 2 length error\n"));
	return(PS_RPA);
    }
    if (outlevel >= O_DEBUG)
	report(stdout, _("Realm list: %s\n"),bufp);
    if (SetRealmService(bufp) != 0)
    {
	if (outlevel > O_SILENT)
	    report(stderr, _("RPA error in service@realm string\n"));
	return(PS_RPA);
    }

    /* Assemble Token 3 in buf */

    bufp      = buf;
    *(bufp++) = HDR;
    LenAppend(&bufp, 11+2+strlen(userid)+1+Cul+1+Rul );
    memcpy(bufp, MECH, 11); bufp += 11;
    *(bufp++) = 0;
    *(bufp++) = strlen(userid);
    memcpy(bufp,userid,strlen(userid)); bufp += strlen(userid);
    GenChallenge(Cu,Cul);
    *(bufp++) = Cul;
    memcpy(bufp, Cu, Cul);  bufp += Cul;
    CompUserResp();
    *(bufp++) = Rul;
    memcpy(bufp, Ru, Rul);  bufp += Rul;

    /* Send Token 3, receive Token 4 */

    EncBase64(buf,bufp-buf);
#ifndef TESTMODE
    SockPrintf(socket,"%s\r\n",buf);
#endif
    if (outlevel >= O_MONITOR)
	report(stdout, "> %s\n",buf);
    if ((ok = POP3_rpa_resp(buf,socket)) != 0)
    {
	if (outlevel > O_SILENT && outlevel < O_MONITOR)
	    report(stdout, "%s\n",buf);
	return(ok);
    }
    if ((rxlen = DecBase64(buf)) == 0)
    {
	if (outlevel > O_SILENT)
	    report(stderr, _("RPA token 4: Base64 decode error\n"));
	return(PS_RPA);
    }
    bufp = buf;
    if (LenSkip(&bufp,rxlen) == 0) return(PS_RPA);

    /* Interpret Token 4 */

    aulin = *(bufp++);
    if (outlevel >= O_DEBUG)
    {
	report(stdout, _("User authentication (l=%d):\n"),aulin);
	for (i=0; i<aulin; i++)
	    report_build(stdout, "%02X ",bufp[i]);
	report_complete(stdout, "\n");
    }
    if (aulin == Aul) memcpy(Au, bufp, Aul);
    bufp += aulin;
    kuslin = *(bufp++);
    if (kuslin == Kusl) memcpy(Kusu, bufp, Kusl); /* blinded */
    bufp += kuslin;
    if (verh == 3)
    {
	status = *(bufp++);
	if (outlevel >= O_DEBUG)
	    report(stdout, _("RPA status: %02X\n"),status);
    }
    else status = 0;
    if ((bufp - buf) != rxlen)
    {
	if (outlevel > O_SILENT)
	    report(stderr, _("RPA token 4 length error\n"));
	return(PS_RPA);
    }
    if (status != 0)
    {
	if (outlevel > O_SILENT)
	    if (status < 4)
		report(stderr, _("RPA rejects you: %s\n"),_(stdec[status]));
	    else
		report(stderr, _("RPA rejects you, reason unknown\n"));
	return(PS_AUTHFAIL);
    }
    if (Aul != aulin)
    {
	report(stderr, 
	       _("RPA User Authentication length error: %d\n"),aulin);
	return(PS_RPA);
    }
    if (Kusl != kuslin)
    {
	report(stderr, _("RPA Session key length error: %d\n"),kuslin);
	return(PS_RPA);
    }
    if (CheckUserAuth() != 0)
    {
	if (outlevel > O_SILENT)
	    report(stderr, _("RPA _service_ auth fail. Spoof server?\n"));
	return(PS_AUTHFAIL);
    }
    if (outlevel >= O_DEBUG)
    {
	report(stdout, _("Session key established:\n"));
	for (i=0; i<Kusl; i++)
	    report_build(stdout, "%02X ",Kus[i]);
	report_complete(stdout, "\n");
    }

    /* Assemble Token 5 in buf and send (not in ver 2 though)  */
    /* Version 3.0 definitely replies with +OK to this. I have */
    /* no idea what sort of response previous versions gave.   */

    if (verh != 2)
    {
	bufp      = buf;
	*(bufp++) = HDR;
	LenAppend(&bufp, 1 );
	*(bufp++) = 0x42;
	EncBase64(buf,bufp-buf);
#ifndef TESTMODE
	SockPrintf(socket,"%s\r\n",buf);
#endif
	if (outlevel >= O_MONITOR)
	    report(stdout, "> %s\n",buf);
	if ((ok = POP3_rpa_resp(buf,socket)) != 0)
	{
	    if (outlevel > O_SILENT && outlevel < O_MONITOR)
		report(stdout, "%s\n",buf);
	    return(ok);
	}
    }

    if (outlevel > O_SILENT)
	report(stdout, _("RPA authorisation complete\n"));

    return(PS_SUCCESS);
}


/*********************************************************************
  function:      POP3_rpa_resp
  description:   get the server's response to an RPA action.
                 Return received base64 string if successful
  arguments:
    argbuf       buffer to receive the string.
    socket       socket to which the server is connected.

  return value:  zero if okay, else return code.
  calls:         SockGets
  globals:       reads outlevel.
 *********************************************************************/

static int POP3_rpa_resp (argbuf,socket)
unsigned char *argbuf;
int socket;
{
    int ok;
    char buf [POPBUFSIZE];
    char *bufp;
    int sockrc;

    if (outlevel >= O_DEBUG)
	report(stdout,  _("Get response\n"));
#ifndef TESTMODE
    sockrc = gen_recv(socket, buf, sizeof(buf));
#else
    linecount++;
    if (linecount == 1) strcpy(buf,line1);
    if (linecount == 2) strcpy(buf,line2);
    if (linecount == 3) strcpy(buf,line3);
/*  report(stdout, "--> "); fflush(stderr);  */
/*  scanf("%s",&buf)                         */
    sockrc = PS_SUCCESS;
#endif
    if (sockrc == PS_SUCCESS) {
	bufp = buf;
	if ((*buf) == '+')
	{
	    bufp++;
/*      if (*bufp == ' ') bufp++; */
	    if (argbuf != NULL)
		strcpy(argbuf,bufp);
	    ok=0;
	}
	else if (strcmp(buf,"-ERR") == 0)
	    ok = PS_ERROR;
	else ok = PS_PROTOCOL;

    }
    else
	ok = PS_SOCKET;
    if (outlevel >= O_DEBUG)
	report(stdout,  _("Get response return %d [%s]\n"), ok, buf);
    buf[sockrc] = 0;
    return(ok);
}

/*********************************************************************
  function:      LenAppend
  description:   Store token length encoded as per ASN.1 DER rules
                 buffer pointer stepped on appropriately.
                 Copes with numbers up to 32767 at least.
  arguments:
    buf          pointer to buffer to receive result
    len          length value to encode

  return value:  none
  calls:         none
  globals:       none
 *********************************************************************/

static void LenAppend(pptr,len)
unsigned char **pptr;
int  len;
{
    if (len < 0x80)
    {
	**pptr = len; (*pptr)++;
    }
    else if (len < 0x100)
    {
	**pptr = 0x81; (*pptr)++;
	**pptr = len;  (*pptr)++;
    }
    else
    {
	**pptr = 0x82;       (*pptr)++;
	**pptr = len >> 8;   (*pptr)++;
	**pptr = len & 0xFF; (*pptr)++;
    }
}

/*********************************************************************
  function:      LenSkip
  description:   Check token header, length, and mechanism, and
                 skip past these.
  arguments:
    pptr         pointer to buffer pointer
    rxlen        number of bytes after base64 decode

  return value:  0 if error, else token length value
  calls:         none
  globals:       reads outlevel.
 *********************************************************************/

int LenSkip(pptr,rxlen)
unsigned char **pptr;
int rxlen;
{
    int len;
    unsigned char *save;
    save = *pptr;
    if (**pptr != HDR)
    {
	if (outlevel > O_SILENT)
	    report(stderr, _("Hdr not 60\n"));
	return(0);
    }
    (*pptr)++;
    if (((**pptr) & 0x80) == 0 )
    {
	len = **pptr; (*pptr)++;
    }
    else if ((**pptr) == 0x81)
    {
	len = *(*pptr+1); (*pptr) += 2;
    }
    else if ((**pptr) == 0x82)
    {
	len = ((*(*pptr+1)) << 8) | *(*pptr+2);
	(*pptr) += 3;
    }
    else len = 0;
    if (len==0)
    {
	if (outlevel>O_SILENT)
	    report(stderr, _("Token length error\n"));
    }
    else if (((*pptr-save)+len) != rxlen)
    {
	if (outlevel>O_SILENT)
	    report(stderr, _("Token Length %d disagrees with rxlen %d\n"),len,rxlen);
	len = 0;
    }
    else if (memcmp(*pptr,MECH,11))
    {
	if (outlevel > O_SILENT)
	    report(stderr, _("Mechanism field incorrect\n"));
	len = 0;
    }
    else (*pptr) += 11;  /* Skip mechanism field */
    return(len);
}

/*********************************************************************
  function:      DecBase64
  description:   Decode a Base64 string, overwriting the original.
                 Note that result cannot be longer than input.

  arguments:
    bufp         buffer

  return value:  0 if error, else number of bytes in decoded result
  calls:         none
  globals:       reads outlevel.
 *********************************************************************/

static int DecBase64(bufp)
unsigned char *bufp;
{
    unsigned int   new, bits=0, cnt=0, i, part=0;
    unsigned char  ch;
    unsigned char* outp=bufp;
    unsigned char* inp=bufp;
    while((ch=*(inp++)) != 0)
    {
	if ((ch != '=') && (ch != ' ') && (ch != '\n') && (ch != '\r'))
	{
	    if      ((ch>='A') && (ch <= 'Z'))   new = ch - 'A';
	    else if ((ch>='a') && (ch <= 'z'))   new = ch - 'a' + 26;
	    else if ((ch>='0') && (ch <= '9'))   new = ch - '0' + 52;
	    else if ( ch=='+'                )   new = 62;
	    else if ( ch=='/'                )   new = 63;
	    else {
		report(stderr,  _("dec64 error at char %d: %x\n"), inp - bufp, ch);
		return(0);
	    }
	    part=((part & 0x3F)*64) + new;
	    bits += 6;
	    if (bits >= 8)
	    {
		bits -= 8;
		*outp = (part >> bits);
		cnt++; outp++;
	    }
	}
    }
    if (outlevel >= O_MONITOR)
    {
	report(stdout, _("Inbound binary data:\n"));
	for (i=0; i<cnt; i++)
	{
	    report_build(stdout, "%02X ",bufp[i]);
	    if (((i % 16)==15) || (i==(cnt-1)))
		report_complete(stdout, "\n");
	}
    }
    return(cnt);
}

/*********************************************************************
  function:      EncBase64
  description:   Encode into Base64 string, overwriting the original.
                 Note that result CAN be longer than input, the buffer
                 is assumed to be big enough. Result string is
                 terminated with \0.

  arguments:
    bufp         buffer
    len          number of bytes in buffer (>0)

  return value:  none
  calls:         none
  globals:       reads outlevel;
 *********************************************************************/

static void EncBase64(bufp,len)
unsigned char *bufp;
int  len;
{
    unsigned char* outp;
    unsigned char  c1,c2,c3;
    char x[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int  i;

    if (outlevel >= O_MONITOR)
    {
	report(stdout, _("Outbound data:\n"));
	for (i=0; i<len; i++)
	{
	    report_build(stdout, "%02X ",bufp[i]);
	    if (((i % 16)==15) || (i==(len-1)))
		report_complete(stdout, "\n");
	}
    }
    outp = bufp + (((len-1)/3)*4);
    *(outp+4) = 0;
    /* So we can do the update in place, start at the far end! */
    for (i=((len-1)/3)*3; i>=0; i-=3)
    {
	c1 = bufp[i];
	if ((i+1) < len) c2 = bufp[i+1]; else c2=0;
	if ((i+2) < len) c3 = bufp[i+2]; else c3=0;
	*(outp) = x[c1/4];
	*(outp+1) = x[((c1 & 3)*16) + (c2/16)];
	if ((i+1) < len) *(outp+2) = x[((c2 & 0x0F)*4) + (c3/64)];
	else *(outp+2) = '=';
	if ((i+2) < len) *(outp+3) = x[c3 & 0x3F];
	else *(outp+3) = '=';
	outp -= 4;
    }
}

/*********************************************************************
  function:      ToUnicode
  description:   Convert ASCII (or iso-8859-1) byte string into
                 Unicode. Ensure length isn't too long (STRMAX).

  arguments:
    pptr         pointer to input buffer
    delim        delimiter character (in addition to \0)
    buf          buffer where Unicode will go
    plen         pointer to length variable (# bytes output)
    conv         1 to convert to lowercase, 0 leaves alone

  return value:  none
  calls:         none
  globals:       reads outlevel;
 *********************************************************************/

static void ToUnicode(pptr,delim,buf,plen,conv)
unsigned char **pptr; /* input string  */
unsigned char delim;
unsigned char *buf;   /* output buffer */
int  *plen;
int conv;
{
    unsigned char *p;
    int i;
    *plen = 0; p=buf;
    while ( ((**pptr)!=delim) && ((**pptr)!=0) && ((*plen)<STRMAX) )
    {
	*(p++) = 0;
	if (conv)
	    *(p++) = tolower(**pptr);
	else
	    *(p++) = (**pptr);
	(*plen) += 2;
	(*pptr)++;
    }
    if ( ((**pptr)!=delim) && ((**pptr)!=0) && ((*plen)==STRMAX) )
    {
	if (outlevel > O_SILENT)
	    report(stderr, _("RPA String too long\n"));
	*plen = 0;
    }
    if (outlevel >= O_DEBUG)
    {
	report(stdout, _("Unicode:\n"));
	for (i=0; i<(*plen); i++)
	{
	    report_build(stdout, "%02X ",buf[i]);
	    if (((i % 16)==15) || (i==((*plen)-1)))
		report_complete(stdout, "\n");
	}
    }
}

/*********************************************************************
  function:      SetRealmService
  description:   Select a realm from list, and store it.

  arguments:
    bufp         pointer to buffer

  return value:  none
  calls:         none
  globals:       reads outlevel.
                 writes Ns Nsl Nr Nrl
 *********************************************************************/

static int SetRealmService(bufp)
unsigned char* bufp;
{
    /* For the moment we pick the first available realm. It would */
    /* make more sense to verify that the realm which the user    */
    /* has given (as part of id) is in the list, and select it's  */
    /* corresponding service name.                                */
    ToUnicode(&bufp, '@', Ns, &Nsl, 1);  /* Service    */
    bufp++;                              /* Skip the @ */
    ToUnicode(&bufp, ' ', Nr, &Nrl, 1);  /* Realm name */
    if ((Nrl == 0) || (Nsl == 0))
	return(PS_RPA);
    return(0);
}

/*********************************************************************
  function:      GenChallenge
  description:   Generate a random User challenge

  arguments:
    buf          pointer to buffer
    len          length in bytes

  return value:  none
  calls:         none
  globals:       reads outlevel.
                 reads /dev/random
 *********************************************************************/

static void GenChallenge(buf,len)
unsigned char *buf;
int  len;
{
    int  i;
    FILE *devrandom;

    devrandom = fopen("/dev/urandom","rb");
    if (devrandom == NULL && outlevel > O_SILENT)
    {
	report(stdout, _("RPA Failed open of /dev/urandom. This shouldn't\n"));
	report(stdout, _("    prevent you logging in, but means you\n"));
	report(stdout, _("    cannot be sure you are talking to the\n"));
	report(stdout, _("    service that you think you are (replay\n"));
	report(stdout, _("    attacks by a dishonest service are possible.)\n"));
    }

    for(i=0; i<len; i++)
	buf[i] = devrandom ? fgetc(devrandom) : random();

    if (devrandom)
	fclose(devrandom);

    if (outlevel >= O_DEBUG)
    {
	report(stdout, _("User challenge:\n"));
	for (i=0; i<len; i++)
	  {
	  report_build(stdout, "%02X ",buf[i]);
	  if (((i % 16)==15) || (i==(len-1)))
	    report_complete(stdout, "\n");
	  }
    }
}

/*********************************************************************
  function:      DigestPassphrase
  description:   Use MD5 to compute digest (Pu) of Passphrase
                 Don't map to lower case. We assume the user is
                 aware of the case requirement of the realm.
                 (Why oh why have options in the spec?!)
  arguments:
    passphrase   buffer containing string, \0 terminated
    rbuf         buffer into which digest goes

  return value:  0 if ok, else error code
  calls:         md5
  globals:       reads authentication items listed above.
                 writes Pu.
 *********************************************************************/

static int DigestPassphrase(passphrase,rbuf,unicodeit)
unsigned char *passphrase;
unsigned char *rbuf;
int unicodeit;
{
    int   len;
    unsigned char  workarea[STRMAX];
    unsigned char* ptr;

    if (unicodeit)  /* Option in spec. Yuck. */
    {
	ptr = passphrase;
	ToUnicode(&ptr, '\0', workarea, &len, 0); /* No case conv here */
	if (len == 0)
	    return(PS_SYNTAX);
	ptr = workarea;
    }
    else
    {
	ptr = rbuf;
	len = strlen(passphrase);
    }
    md5(ptr,len,rbuf);
    return(0);
}

/*********************************************************************
  function:      CompUserResp
  description:   Use MD5 to compute User Response (Ru) from
                 Pu Z(48) Nu Ns Nr Cu Cs Ts Pu

  arguments:     none

  return value:  none
  calls:         MD5
  globals:       reads authentication items listed above.
                 writes Ru.
 *********************************************************************/

static void CompUserResp()
{
    unsigned char  workarea[Pul+48+STRMAX*5+Tsl+Pul];
    unsigned char* p;
    p = workarea;
    memcpy(p , Pu,  Pul); p += Pul;
    memset(p , '\0', 48); p += 48;
    memcpy(p , Nu,  Nul); p += Nul;
    memcpy(p , Ns,  Nsl); p += Nsl;
    memcpy(p , Nr,  Nrl); p += Nrl;
    memcpy(p , Cu,  Cul); p += Cul;
    memcpy(p , Cs,  Csl); p += Csl;
    memcpy(p , Ts,  Tsl); p += Tsl;
    memcpy(p , Pu,  Pul); p += Pul;
    md5(workarea,p-workarea,Ru);
}

/*********************************************************************
  function:      CheckUserAuth
  description:   Use MD5 to verify Authentication Response to User (Au)
                 using  Pu Z(48) Ns Nu Nr Kusu Cs Cu Ts Kus Pu
                 Also creates unobscured session key Kus from obscured
                 one Kusu

  arguments:     none

  return value:  0 if ok, PS_RPA if mismatch
  calls:         MD5
  globals:       reads authentication items listed above.
                 writes Ru.
 *********************************************************************/

static int CheckUserAuth()
{
    unsigned char  workarea[Pul+48+STRMAX*7+Tsl+Pul];
    unsigned char* p;
    unsigned char  md5ans[16];
    int i;
    /* Create unobscured Kusu */
    p = workarea;
    memcpy(p , Pu,  Pul); p += Pul;
    memset(p , '\0', 48); p += 48;
    memcpy(p , Ns,  Nsl); p += Nsl;
    memcpy(p , Nu,  Nul); p += Nul;
    memcpy(p , Nr,  Nrl); p += Nrl;
    memcpy(p , Cs,  Csl); p += Csl;
    memcpy(p , Cu,  Cul); p += Cul;
    memcpy(p , Ts,  Tsl); p += Tsl;
    memcpy(p , Pu,  Pul); p += Pul;
    md5(workarea,p-workarea,md5ans);
    for (i=0; i<16; i++) Kus[i] = Kusu[i] ^ md5ans[i];
    /* Compute Au from our information */
    p = workarea;
    memcpy(p , Pu,  Pul); p += Pul;
    memset(p , '\0', 48); p += 48;
    memcpy(p , Ns,  Nsl); p += Nsl;
    memcpy(p , Nu,  Nul); p += Nul;
    memcpy(p , Nr,  Nrl); p += Nrl;
    memcpy(p , Kusu,Kusl);p += Kusl;
    memcpy(p , Cs,  Csl); p += Csl;
    memcpy(p , Cu,  Cul); p += Cul;
    memcpy(p , Ts,  Tsl); p += Tsl;
    memcpy(p , Kus, Kusl);p += Kusl;
    memcpy(p , Pu,  Pul); p += Pul;
    md5(workarea,p-workarea,md5ans);
    /* Compare the two */
    for (i=0; i<16; i++)
	if (Au[i] != md5ans[i]) return(PS_RPA);
    return(0);
}

/*********************************************************************
  function:      md5
  description:   Apply MD5
  arguments:
    in           input byte stream
    len          length in bytes
    out          128 bit result buffer
  return value:  none
  calls:         MD5 primitives
  globals:       reads outlevel
 *********************************************************************/

static void md5(in,len,out)
unsigned char*    in;
int      len;
unsigned char*    out;
{
    int      i;
    MD5_CTX  md5context;

    if (outlevel >= O_DEBUG)
    {
	report(stdout, _("MD5 being applied to data block:\n"));
	for (i=0; i<len; i++)
	{
	    report_build(stdout, "%02X ",in[i]);
	    if (((i % 16)==15) || (i==(len-1)))
		report_complete(stdout, "\n");
	}
    }
    MD5Init(   &md5context );
    MD5Update( &md5context, in, len );
    MD5Final(  out, &md5context );
    if (outlevel >= O_DEBUG)
    {
	report(stdout, _("MD5 result is: \n"));
	for (i=0; i<16; i++)
	{
	    report_build(stdout, "%02X ",out[i]);
	}
	report_complete(stdout, "\n");
    }
}
#endif /* POP3_ENABLE && RPA_ENABLE */

/* rpa.c ends here */
