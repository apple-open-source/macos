/*
 * conf.c -- dump fetchmail configuration as Python dictionary initializer
 *
 * For license terms, see the file COPYING in this directory.
 */

#include "config.h"
#include "tunable.h"

#include <stdio.h>
#include <ctype.h>
#if defined(STDC_HEADERS)
#include <stdlib.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#include <string.h>
#include <pwd.h>
#include <errno.h>

#include "fetchmail.h"

/* Python prettyprinting functions */

static int indent_level;

static void indent(char ic)
/* indent current line */
{
    int	i;

    if (ic == ')' || ic == ']' || ic == '}')
	indent_level--;

    /*
     * The guard here is a kluge.  It depends on the fact that in the
     * particular structure we're dumping, opening [s are always
     * initializers for dictionary members and thus will be preceded
     * by a member name.
     */
    if (ic != '[')
    {
	for (i = 0; i < indent_level / 2; i++)
	    putc('\t', stdout);
	if (indent_level % 2)
	    fputs("    ", stdout);
    }

    if (ic)
    {
	putc(ic, stdout);
	putc('\n', stdout);
    }

    if (ic == '(' || ic == '[' || ic == '{')
	indent_level++;
}


static void stringdump(const char *name, const char *member)
/* dump a string member with current indent */
{
    indent('\0');
    fprintf(stdout, "\"%s\":", name);
    if (member)
	fprintf(stdout, "\"%s\"", visbuf(member));
    else
	fputs("None", stdout);
    fputs(",\n", stdout);
}

static void numdump(const char *name, const int num)
/* dump a numeric quantity at current indent */
{
    indent('\0');
    fprintf(stdout, "'%s':%d,\n", name, NUM_VALUE_OUT(num));
}

static void booldump(const char *name, const int onoff)
/* dump a boolean quantity at current indent */
{
    indent('\0');
    if (onoff)
	fprintf(stdout, "'%s':TRUE,\n", name);
    else
	fprintf(stdout, "'%s':FALSE,\n", name);
}

static void listdump(const char *name, struct idlist *list)
/* dump a string list member with current indent */
{
    indent('\0');
    fprintf(stdout, "\"%s\":", name);

    if (!list)
	fputs("[],\n", stdout);
    else
    {
	struct idlist *idp;

	fputs("[", stdout);
	for (idp = list; idp; idp = idp->next)
	    if (idp->id)
	    {
		fprintf(stdout, "\"%s\"", visbuf(idp->id));
		if (idp->next)
		    fputs(", ", stdout);
	    }
	fputs("],\n", stdout);
    }
}

/*
 * Note: this function dumps the entire configuration,
 * after merging of the defaults record (if any).  It
 * is intended to produce output parseable by a configuration
 * front end, not anything especially comfortable for humans.
 */

void dump_config(struct runctl *runp, struct query *querylist)
/* dump the in-core configuration in recompilable form */
{
    struct query *ctl;
    struct idlist *idp;
    const char *features;

    indent_level = 0;

    /*
     * These had better match the values fetchmailconf is expecting!
     * (We don't want to import them from Tkinter because the user
     * might not have it installed.)
     */
    fputs("TRUE=1; FALSE=0\n\n", stdout);

    /*
     * We need this in order to know whether `interface' and `monitor'
     * are valid options or not.
     */
#if defined(linux)
    fputs("os_type = 'linux'\n", stdout);
#elif defined(__FreeBSD__)
    fputs("os_type = 'freebsd'\n", stdout);
#else
    fputs("os_type = 'generic'\n", stdout);
#endif

    /* 
     * This should be approximately in sync with the -V option dumping 
     * in fetchmail.c.
     */
    features = "feature_options = ("
#ifdef POP2_ENABLE
    "'pop2',"
#endif /* POP2_ENABLE */
#ifdef POP3_ENABLE
    "'pop3',"
#endif /* POP3_ENABLE */
#ifdef IMAP_ENABLE
    "'imap',"
#endif /* IMAP_ENABLE */
#ifdef GSSAPI
    "'gssapi',"
#endif /* GSSAPI */
#if defined(KERBEROS_V4)
    "'kerberos',"
#endif /* defined(IMAP4) */
#ifdef RPA_ENABLE
    "'rpa',"
#endif /* RPA_ENABLE */
#ifdef SDPS_ENABLE
    "'sdps',"
#endif /* SDPS_ENABLE */
#ifdef ETRN_ENABLE
    "'etrn',"
#endif /* ETRN_ENABLE */
#ifdef ODMR_ENABLE
    "'odmr',"
#endif /* ODMR_ENABLE */
#ifdef SSL_ENABLE
    "'ssl',"
#endif /* SSL_ENABLE */
#ifdef OPIE_ENABLE
    "'opie',"
#endif /* OPIE_ENABLE */
#ifdef HAVE_SOCKS
    "'socks',"
#endif /* HAVE_SOCKS */
    ")\n";
    fputs(features, stdout);

    fputs("# Start of configuration initializer\n", stdout);
    fputs("fetchmailrc = ", stdout);
    indent('{');

    numdump("poll_interval", runp->poll_interval);
    stringdump("logfile", runp->logfile);
    stringdump("idfile", runp->idfile);
    stringdump("postmaster", runp->postmaster);
    booldump("bouncemail", runp->bouncemail);
    booldump("spambounce", runp->spambounce);
    booldump("softbounce", runp->softbounce);
    stringdump("properties", runp->properties);
    booldump("invisible", runp->invisible);
    booldump("showdots", runp->showdots);
    booldump("syslog", runp->use_syslog);

    if (!querylist)
    {
	fputs("    'servers': []\n", stdout);
	goto alldone;
    }

    indent(0);
    fputs("# List of server entries begins here\n", stdout);
    indent(0);
    fputs("'servers': ", stdout);
    indent('[');

    for (ctl = querylist; ctl; ctl = ctl->next)
    {
	/*
	 * First, the server stuff.
	 */
	if (!ctl->server.lead_server)
	{
	    flag	using_kpop;

	    /*
	     * Every time we see a leading server entry after the first one,
	     * it implicitly ends the both (a) the list of user structures
	     * associated with the previous entry, and (b) that previous entry.
	     */
	    if (ctl > querylist)
	    {
		indent(']');
		indent('}');
		indent('\0'); 
		putc(',', stdout);
		putc('\n', stdout);
	    }

	    indent(0);
	    fprintf(stdout,"# Entry for site `%s' begins:\n",ctl->server.pollname);
	    indent('{');

	    using_kpop =
		(ctl->server.protocol == P_POP3 &&
		 ctl->server.service && !strcmp(ctl->server.service, KPOP_PORT ) &&
		 ctl->server.authenticate == A_KERBEROS_V4);

	    stringdump("pollname", ctl->server.pollname); 
	    booldump("active", !ctl->server.skip); 
	    stringdump("via", ctl->server.via); 
	    stringdump("protocol", 
		       using_kpop ? "KPOP" : showproto(ctl->server.protocol));
	    stringdump("service",  ctl->server.service);
	    numdump("timeout",  ctl->server.timeout);
	    numdump("interval", ctl->server.interval);

	    if (ctl->server.envelope == STRING_DISABLED)
		stringdump("envelope", NULL); 
	    else if (ctl->server.envelope == NULL)
		stringdump("envelope", "Received"); 		
	    else
		stringdump("envelope", ctl->server.envelope); 
	    numdump("envskip", ctl->server.envskip);
	    stringdump("qvirtual", ctl->server.qvirtual);
 
	    if (ctl->server.authenticate == A_ANY)
		stringdump("auth", "any");
	    else if (ctl->server.authenticate == A_PASSWORD)
		stringdump("auth", "password");
	    else if (ctl->server.authenticate == A_OTP)
		stringdump("auth", "otp");
	    else if (ctl->server.authenticate == A_NTLM)
		stringdump("auth", "ntlm");
	    else if (ctl->server.authenticate == A_CRAM_MD5)
		stringdump("auth", "cram-md5");
	    else if (ctl->server.authenticate == A_GSSAPI)
		stringdump("auth", "gssapi");
	    else if (ctl->server.authenticate == A_KERBEROS_V4)
		stringdump("auth", "kerberos_v4");
	    else if (ctl->server.authenticate == A_KERBEROS_V5)
		stringdump("auth", "kerberos_v5");
	    else if (ctl->server.authenticate == A_SSH)
		stringdump("auth", "ssh");
	    else if (ctl->server.authenticate == A_OTP)
		stringdump("auth", "otp");
	    else if (ctl->server.authenticate == A_MSN)
		stringdump("auth", "msn");

#ifdef HAVE_RES_SEARCH
	    booldump("dns", ctl->server.dns);
#endif /* HAVE_RES_SEARCH */
	    booldump("uidl", ctl->server.uidl);

	    listdump("aka", ctl->server.akalist);
	    listdump("localdomains", ctl->server.localdomains);

#ifdef CAN_MONITOR
	    stringdump("interface", ctl->server.interface);
	    stringdump("monitor", ctl->server.monitor);
#endif

	    stringdump("plugin", ctl->server.plugin);
	    stringdump("plugout", ctl->server.plugout);
	    stringdump("principal", ctl->server.principal);
	    if (ctl->server.esmtp_name)
	        stringdump("esmtpname",ctl->server.esmtp_name);
	    if (ctl->server.esmtp_password)
	        stringdump("esmtppassword",ctl->server.esmtp_password);
	    booldump("tracepolls", ctl->server.tracepolls);
	    indent(0);
	    switch(ctl->server.badheader) {
		/* this is a hack - we map this to a boolean option for
		 * fetchmailconf purposes */
		case BHREJECT: puts("'badheader': FALSE,"); break;
		case BHACCEPT: puts("'badheader': TRUE,"); break;
	    }

	    indent(0);
	    fputs("'users': ", stdout);
	    indent('[');
	}

	indent('{');

	stringdump("remote", ctl->remotename);
	stringdump("password", ctl->password);

	indent('\0');
	fprintf(stdout, "'localnames':[");
	for (idp = ctl->localnames; idp; idp = idp->next)
	{
	    char namebuf[USERNAMELEN + 1];

	    strlcpy(namebuf, visbuf(idp->id), sizeof(namebuf));
	    if (idp->val.id2)
		fprintf(stdout, "(\"%s\", %s)", namebuf, visbuf(idp->val.id2));
	    else
		fprintf(stdout, "\"%s\"", namebuf);
	    if (idp->next)
		fputs(", ", stdout);
	}
	if (ctl->wildcard)
	    fputs(", '*'", stdout);
	fputs("],\n", stdout);

	booldump("fetchall", ctl->fetchall);
	booldump("keep", ctl->keep);
	booldump("flush", ctl->flush);
	booldump("limitflush", ctl->limitflush);
	booldump("rewrite", ctl->rewrite);
	booldump("stripcr", ctl->stripcr); 
	booldump("forcecr", ctl->forcecr);
	booldump("pass8bits", ctl->pass8bits);
	booldump("dropstatus", ctl->dropstatus);
	booldump("dropdelivered", ctl->dropdelivered);
	booldump("mimedecode", ctl->mimedecode);
	booldump("idle", ctl->idle);

	stringdump("mda", ctl->mda);
	stringdump("bsmtp", ctl->bsmtp);
	indent('\0');
	if (ctl->listener == LMTP_MODE)
	    fputs("'lmtp':TRUE,\n", stdout);
	else
	    fputs("'lmtp':FALSE,\n", stdout);
	    
	stringdump("preconnect", ctl->preconnect);
	stringdump("postconnect", ctl->postconnect);
	numdump("limit", ctl->limit);
	numdump("warnings", ctl->warnings);
	numdump("fetchlimit", ctl->fetchlimit);
	numdump("fetchsizelimit", ctl->fetchsizelimit);
	numdump("fastuidl", ctl->fastuidl);
	numdump("batchlimit", ctl->batchlimit);
#ifdef SSL_ENABLE
	booldump("ssl", ctl->use_ssl);
	stringdump("sslkey", ctl->sslkey);
	stringdump("sslcert", ctl->sslcert);
	stringdump("sslproto", ctl->sslproto);
	booldump("sslcertck", ctl->sslcertck);
	stringdump("sslcertpath", ctl->sslcertpath);
	stringdump("sslcommonname", ctl->sslcommonname);
	stringdump("sslfingerprint", ctl->sslfingerprint);
#endif /* SSL_ENABLE */
	numdump("expunge", ctl->expunge);
	stringdump("properties", ctl->properties);
	listdump("smtphunt", ctl->smtphunt);
	listdump("fetchdomains", ctl->domainlist);
	stringdump("smtpaddress", ctl->smtpaddress);
	stringdump("smtpname", ctl->smtpname);

	indent('\0');
	fprintf(stdout, "'antispam':'");
	for (idp = ctl->antispam; idp; idp = idp->next)
	{
	    fprintf(stdout, "%d", idp->val.status.num);
	    if (idp->next)
		fputs(" ", stdout);
	}
	fputs("',\n", stdout);
	listdump("mailboxes", ctl->mailboxes);

	indent('}');
	indent('\0'); 
	fputc(',', stdout);
    }

    /* end last span of user entries and last server entry */
    indent(']');
    indent('}');

    /* end array of servers */
    indent(']');

 alldone:
    /* end top-level dictionary */
    indent('}');
    fputs("# End of initializer\n", stdout);
}

/* conf.c ends here */
