%{

/*
 * rcfile_l.l -- lexer for the run control file
 *
 * For license terms, see the file COPYING in this directory.
 */
#include <string.h>

#include "config.h"
#include "fetchmail.h"
#include "rcfile_y.h"

int prc_lineno = 1;
%}

/* this doesn't work with Linux lex, see the INSTALL file */
%o 7000
%a 4000
%p 3000

%s NAME PREAUTH

%%

\"[^\"]*\"	{
			char buf[MSGBUFSIZE];

			yytext[strlen(yytext)-1] = '\0';
			escapes(yytext+1, buf);
			yylval.sval = (char *) xstrdup(buf);
                        BEGIN(0);
			return STRING;
		}
\'[^\']*\'	{
			char buf[MSGBUFSIZE];

			yytext[strlen(yytext)-1] = '\0';
			escapes(yytext+1, buf);
			yylval.sval = (char *) xstrdup(buf);
                        BEGIN(0);
			return STRING;
		}

"*"		{ BEGIN(0); return WILDCARD; }

<NAME>[^=;:, \t\r\n]+	{
			char buf[MSGBUFSIZE];

			escapes(yytext, buf);
			yylval.sval = (char *) xstrdup(buf);
                        BEGIN(0);
			return STRING;
		}

set		{ return SET; }
logfile		{ return LOGFILE; }
idfile		{ return IDFILE; }
daemon		{ return DAEMON; }
syslog		{ return SYSLOG; }
invisible	{ return INVISIBLE; }
postmaster	{ return POSTMASTER; }
bouncemail	{ return BOUNCEMAIL; }
warnings	{ return WARNINGS; }

defaults 	{ return DEFAULTS; }
server 		{ return POLL; }
poll		{ return POLL; }
skip		{ return SKIP; }
via		{ return VIA; }
aka		{ return AKA; }
local(domains)	{ return LOCALDOMAINS; }
proto(col)? 	{ return PROTOCOL; }
service		{ return SERVICE; }
port		{ return PORT; }
interval	{ return INTERVAL; }
preauth(enticate)?	{ BEGIN(PREAUTH); return PREAUTHENTICATE; }
kerberos(_v)?4	{ BEGIN(0); return KERBEROS4; }
kerberos(_v)?5	{ BEGIN(0); return KERBEROS5; }
kerberos	{ BEGIN(0); return KERBEROS; }
<PREAUTH>password { BEGIN(0); return PASSWORD; }
timeout		{ return TIMEOUT;}
envelope	{ return ENVELOPE; }
qvirtual	{ return QVIRTUAL; }

user(name)?	{BEGIN(NAME); return USERNAME; }
pass(word)?	{BEGIN(NAME); return PASSWORD; }
folder(s)? 	{ return FOLDER; }
smtp(host)?	{ return SMTPHOST; }
smtpaddress	{ return SMTPADDRESS; }
antispam	{ return SPAMRESPONSE; }
mda		{ return MDA; }
bsmtp		{ return BSMTP; }
lmtp		{ return LMTP; }
pre(connect)?	{ return PRECONNECT; }
post(connect)?	{ return POSTCONNECT; }
netsec		{ return NETSEC; }
interface	{ return INTERFACE; }
monitor		{ return MONITOR; }
plugin		{ return PLUGIN; }
plugout		{ return PLUGIN; }
batchlimit	{ return BATCHLIMIT; }
fetchlimit	{ return FETCHLIMIT; }
expunge		{ return EXPUNGE; }
properties	{ return PROPERTIES; }

is		{ BEGIN(NAME); return IS; }
here		{ return HERE; }
there		{ return THERE; }
to		{ BEGIN(NAME); return TO; }
=		{ BEGIN(NAME); return MAP; }

nobouncemail	|
nouidl		|
nocheckalias	|
nodns		|
noenvelope	|
nokeep		|
noflush		|
nofetchall	|
norewrite	|
noforcecr	|
nostripcr	|
nopass8(bits)?	|
nodropstatus	|
nomimedec(ode)?	{
                   yyless(2);
                   return NO;
                }

no		{return NO;}

keep		{ return KEEP; }
flush		{ return FLUSH; }
fetchall	{ return FETCHALL; }
rewrite		{ return REWRITE; }
forcecr		{ return FORCECR; }
stripcr		{ return STRIPCR; }
pass8(bits)?	{ return PASS8BITS; }
dropstatus	{ return DROPSTATUS; }
mimedec(ode)?   { return MIMEDECODE; }
dns		{ return DNS; }
uidl		{ return UIDL; }
checkalias	{ return CHECKALIAS; }

limit		{ return LIMIT; }

with		{/* EMPTY */}
and		{/* EMPTY */}
has		{/* EMPTY */}
wants		{/* EMPTY */}
options		{/* EMPTY */}
[;:,]		{/* EMPTY */}

(auto)|(AUTO)	{ yylval.proto = P_AUTO;  return PROTO; }
(pop2)|(POP2)	{ yylval.proto = P_POP2;  return PROTO; }
(sdps)|(SDPS)   { return SDPS; }
(pop3)|(POP3)	{ yylval.proto = P_POP3;  return PROTO; }
(imap-k4)|(IMAP-K4)   { yylval.proto = P_IMAP_K4;  return PROTO; }
(imap-gss)|(IMAP-GSS) { yylval.proto = P_IMAP_GSS;  return PROTO; }
(imap-crammd5)|(IMAP-CRAMMD5) { yylval.proto = P_IMAP_CRAM_MD5;  return PROTO; }
(imap-login)|(IMAP-LOGIN) { yylval.proto = P_IMAP_LOGIN;  return PROTO; }
(imap)|(IMAP)	{ yylval.proto = P_IMAP;  return PROTO; }
(apop)|(APOP)   { yylval.proto = P_APOP;  return PROTO; }
(etrn)|(ETRN)   { yylval.proto = P_ETRN;  return PROTO; }
(kpop)|(KPOP)	{ return KPOP; }

(#.*)?\\?\n	{ prc_lineno++;	}   /* newline is ignored */

[0-9]+		{ yylval.number = atoi(yytext); return NUMBER; }

[^=;:, \t\r\n]+	{
			char buf[MSGBUFSIZE];

			escapes(yytext, buf);
			yylval.sval = (char *) xstrdup(buf);
			return STRING;
		}

[ \t\r]+	;	/* whitespace */

%%

void escapes(cp, tp)
/* process standard C-style escape sequences in a string */
const char	*cp;	/* source string with escapes */
char		*tp;	/* target buffer for digested string */
{
    while (*cp)
    {
	int	cval = 0;

	if (*cp == '\\' && strchr("0123456789xX", cp[1]))
	{
	    char *dp;
	    const char *hex = "00112233445566778899aAbBcCdDeEfF";
	    int dcount = 0;

	    if (*++cp == 'x' || *cp == 'X')
		for (++cp; (dp = strchr(hex, *cp)) && (dcount++ < 2); cp++)
		    cval = (cval * 16) + (dp - hex) / 2;
	    else if (*cp == '0')
		while (strchr("01234567",*cp) != (char*)NULL && (dcount++ < 3))
		    cval = (cval * 8) + (*cp++ - '0');
	    else
		while ((strchr("0123456789",*cp)!=(char*)NULL)&&(dcount++ < 3))
		    cval = (cval * 10) + (*cp++ - '0');
	}
	else if (*cp == '\\')		/* C-style character escapes */
	{
	    switch (*++cp)
	    {
	    case '\\': cval = '\\'; break;
	    case 'n': cval = '\n'; break;
	    case 't': cval = '\t'; break;
	    case 'b': cval = '\b'; break;
	    case 'r': cval = '\r'; break;
	    default: cval = *cp;
	    }
	    cp++;
	}
	else
	    cval = *cp++;
	*tp++ = cval;
    }
    *tp = '\0';
}

