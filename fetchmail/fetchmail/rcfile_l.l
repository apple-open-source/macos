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

#ifdef LEXDEBUG
#define SETSTATE(n)	do {BEGIN(n); if (yydebug) fprintf(stderr, "Entering lexer state %d\n", n);} while (0)
#else
#define SETSTATE(n)	BEGIN(n)
#endif /* LEXDEBUG */
%}

/* this doesn't work with Linux lex, see the INSTALL file */
%o 7000
%a 4000
%p 3000
%option nounput

%s NAME AUTH

%%

\"[^\"]*\"	{
			char buf[MSGBUFSIZE];

			yytext[strlen(yytext)-1] = '\0';
			escapes(yytext+1, buf);
			yylval.sval = (char *) xstrdup(buf);
                        SETSTATE(0);
			return STRING;
		}
\'[^\']*\'	{
			char buf[MSGBUFSIZE];

			yytext[strlen(yytext)-1] = '\0';
			escapes(yytext+1, buf);
			yylval.sval = (char *) xstrdup(buf);
                        SETSTATE(0);
			return STRING;
		}

"*"		{ BEGIN(0); return WILDCARD; }

<NAME>[^=;:, \t\r\n]+	{
			char buf[MSGBUFSIZE];

			escapes(yytext, buf);
			yylval.sval = (char *) xstrdup(buf);
                        SETSTATE(0);
			return STRING;
		}

set		{ return SET; }
logfile		{ return LOGFILE; }
idfile		{ return IDFILE; }
daemon		{ return DAEMON; }
syslog		{ return SYSLOG; }
invisible	{ return INVISIBLE; }
showdots	{ return SHOWDOTS; }
postmaster	{ return POSTMASTER; }
bouncemail	{ return BOUNCEMAIL; }
spambounce	{ return SPAMBOUNCE; }
warnings	{ return WARNINGS; }
tracepolls	{ return TRACEPOLLS; }

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
preauth(enticate)?	{ SETSTATE(AUTH); return AUTHENTICATE; }
auth(enticate)?	{ SETSTATE(AUTH); return AUTHENTICATE; }
any		{ SETSTATE(0); yylval.proto = A_ANY; return AUTHTYPE;}
gssapi		{ SETSTATE(0); yylval.proto = A_GSSAPI; return AUTHTYPE;}
kerberos(_v)?4	{ SETSTATE(0); yylval.proto = A_KERBEROS_V4; return AUTHTYPE;}
kerberos(_v)?5	{ SETSTATE(0); yylval.proto = A_KERBEROS_V5; return AUTHTYPE;}
kerberos	{ SETSTATE(0); yylval.proto = A_KERBEROS_V4; return AUTHTYPE;}
ssh		{ SETSTATE(0); yylval.proto = A_SSH; return AUTHTYPE;}
(otp|opie)	{ SETSTATE(0); yylval.proto = A_OTP; return AUTHTYPE;}
cram(-md5)?	{ SETSTATE(0); yylval.proto = A_CRAM_MD5; return AUTHTYPE;}
ntlm		{ SETSTATE(0); yylval.proto = A_NTLM; return AUTHTYPE;}
<AUTH>password	{ SETSTATE(0); yylval.proto = A_PASSWORD; return AUTHTYPE;}
timeout		{ return TIMEOUT;}
envelope	{ return ENVELOPE; }
qvirtual	{ return QVIRTUAL; }
principal	{ return PRINCIPAL; }
esmtpname	{ return ESMTPNAME; }
esmtppassword	{ return ESMTPPASSWORD; }


user(name)?	{SETSTATE(NAME); return USERNAME; }
<INITIAL,NAME>pass(word)?	{SETSTATE(NAME); return PASSWORD; }
folder(s)? 	{ return FOLDER; }
smtp(host)?	{ return SMTPHOST; }
fetchdomains	{ return FETCHDOMAINS; }
smtpaddress	{ return SMTPADDRESS; }
smtpname	{ return SMTPNAME; }
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
plugout		{ return PLUGOUT; }
batchlimit	{ return BATCHLIMIT; }
fetchlimit	{ return FETCHLIMIT; }
expunge		{ return EXPUNGE; }
properties	{ return PROPERTIES; }

is		{ SETSTATE(NAME); return IS; }
here		{ return HERE; }
there		{ return THERE; }
to		{ SETSTATE(NAME); return TO; }
=		{ SETSTATE(NAME); return MAP; }

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
nodropdelivered	|
nomimedec(ode)? |
nospambounce	|
noidle	        {
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
dropdelivered   { return DROPDELIVERED; }
mimedec(ode)?   { return MIMEDECODE; }
idle		{ return IDLE; }
dns		{ return DNS; }
uidl		{ return UIDL; }
ssl		{ return SSL; }
sslkey		{ return SSLKEY; }
sslcert		{ return SSLCERT; }
sslproto	{ return SSLPROTO; }
sslcertck	{ return SSLCERTCK; }
sslcertpath	{ return SSLCERTPATH; }
sslfingerprint	{ return SSLFINGERPRINT; }
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
(imap)|(IMAP)	{ yylval.proto = P_IMAP;  return PROTO; }
(apop)|(APOP)   { yylval.proto = P_APOP;  return PROTO; }
(etrn)|(ETRN)   { yylval.proto = P_ETRN;  return PROTO; }
(odmr)|(ODMR)   { yylval.proto = P_ODMR;  return PROTO; }
(kpop)|(KPOP)	{ return KPOP; }

(#.*)?\\?\n	{ prc_lineno++;	}   /* newline is ignored */

-?[0-9]+/[^a-zA-Z]	{ yylval.number = atoi(yytext); return NUMBER; }

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

