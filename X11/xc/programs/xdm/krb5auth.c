/* $Xorg: krb5auth.c,v 1.4 2001/02/09 02:05:40 xorgcvs Exp $ */
/*

Copyright 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/xdm/krb5auth.c,v 1.4 2001/12/14 20:01:22 dawes Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Stephen Gildea, The Open Group
 */

/*
 * krb5auth
 *
 * generate Kerberos Version 5 authorization records
 */

#include "dm.h"
#include "dm_error.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <krb5/krb5.h>
#include <krb5/kdb.h>			/* for TGTNAME */

/*ARGSUSED*/
void
Krb5InitAuth (unsigned short name_len, char *name)
{
    krb5_init_ets();		/* initialize error_message() tables */
}

/*
 * Returns malloc'ed string that is the credentials cache name.
 * name should be freed by caller.
 */
char *
Krb5CCacheName(char *dname)
{
    char *name;
    char *tmpdir;

    tmpdir = getenv("TMPDIR");
    if (!tmpdir)
	tmpdir = "/tmp";
    name = malloc(strlen(tmpdir) + strlen(dname) + 20);
    if (!name)
	return NULL;
    sprintf(name, "FILE:%s/K5C", tmpdir);
    CleanUpFileName(dname, name+strlen(name), strlen(dname)+1);
    return name;
}

krb5_error_code
Krb5DisplayCCache(char *dname, krb5_ccache *ccache_return)
{
    krb5_error_code code;
    char *name;

    name = Krb5CCacheName(dname);
    if (!name)
	return ENOMEM;
    Debug("resolving Kerberos cache %s\n", name);
    code = krb5_cc_resolve(name, ccache_return);
    free(name);
    return code;
}

Xauth *
Krb5GetAuthFor(unsigned short namelen, char *name, char *dname)
{
    Xauth   *new;
    char *filename;
    struct stat statbuf;

    new = (Xauth *) malloc (sizeof *new);
    if (!new)
	return (Xauth *) 0;
    new->family = FamilyWild;
    new->address_length = 0;
    new->address = 0;
    new->number_length = 0;
    new->number = 0;

    if (dname)
    {
	filename = Krb5CCacheName(dname);
	new->data = (char *) malloc (3 + strlen(filename) + 1);
	if (!new->data)
	{
	    free (filename);
	    free ((char *) new);
	    return (Xauth *) 0;
	}
	strcpy(new->data, "UU:");
	strcat(new->data, filename);
	free (filename);
	new->data_length = strlen(new->data);
    }
    else
    {
	new->data = NULL;
	new->data_length = 0;
    }

    new->name = (char *) malloc (namelen);
    if (!new->name)
    {
	free ((char *) new->data);
	free ((char *) new);
	return (Xauth *) 0;
    }
    memmove( new->name, name, namelen);
    new->name_length = namelen;
    return new;
}


Xauth *
Krb5GetAuth (unsigned short namelen, char *name)
{
    return Krb5GetAuthFor(namelen, name, NULL);
}

int preauth_search_list[] = {
	0,			
	KRB5_PADATA_ENC_TIMESTAMP,
	-1
	};

/*
 * Krb5Init - lifted from kinit.c
 * Get TGT.
 * Returns 0 if successful, 1 if not.
 */
int
Krb5Init(
    char *name,
    char *passwd,
    struct display *d)		/* k5_ccache filled in if successful */
{
    krb5_ccache ccache;
    krb5_error_code code;
    krb5_principal me;
    krb5_creds my_creds;
    krb5_principal server;
    krb5_address **my_addresses;
    krb5_timestamp now;
    int	i;

    if (code = Krb5DisplayCCache(d->name, &ccache)) {
	LogError("%s while getting Krb5 ccache for \"%s\"\n",
		 error_message(code), d->name);
	return 1;
    }

    if (code = krb5_parse_name (name, &me)) {
	 LogError("%s while parsing Krb5 name \"%s\"\n",
		  error_message(code), name);
	 return 1;
    }

    code = krb5_cc_initialize (ccache, me);
    if (code != 0) {
	LogError("%s while initializing Krb5 cache \"%s\"\n",
		 error_message(code), krb5_cc_default_name());
	return 1;
    }

    memset((char *)&my_creds, 0, sizeof(my_creds));

    my_creds.client = me;

    if (code = krb5_build_principal_ext(&server,
					krb5_princ_realm(me)->length,
					krb5_princ_realm(me)->data,
					6, "krbtgt",
					krb5_princ_realm(me)->length,
					krb5_princ_realm(me)->data,
					0)) {
	LogError("%s while building Krb5 TGT server name\n",
		 error_message(code));
	return 1;
    }

    my_creds.server = server;

    code = krb5_os_localaddr(&my_addresses);
    if (code != 0) {
	LogError("%s while getting my address for Krb5\n",
		 error_message(code));
	return 1;
    }
    if (code = krb5_timeofday(&now)) {
	LogError("%s while getting time of day for Krb5\n",
		 error_message(code));
	return 1;
    }
    my_creds.times.starttime = 0;	/* start timer when request
					   gets to KDC */
    my_creds.times.endtime = now + 60*60*8; /* 8 hours */
    my_creds.times.renew_till = 0;

    for (i = 0; preauth_search_list[i] >= 0; i++) {
	code = krb5_get_in_tkt_with_password(0, my_addresses,
					     preauth_search_list[i],
					     ETYPE_DES_CBC_CRC,
					     KEYTYPE_DES,
					     passwd,
					     ccache,
					     &my_creds, 0);
	if (code != KRB5KDC_PREAUTH_FAILED &&
	    code != KRB5KRB_ERR_GENERIC)
	    break;
    }

    krb5_free_principal(server);
    krb5_free_addresses(my_addresses);

    if (code) {
	char *my_name = NULL;
	int code2 = krb5_unparse_name(me, &my_name);
	if (code == KRB5KRB_AP_ERR_BAD_INTEGRITY) {
	    LogError ("password incorrect for Krb5 principal \"%s\"\n",
		      code2 ? name : my_name);
	}
	else
	    LogError("%s while getting initial Krb5 credentials for \"%s\"\n",
		     error_message(code), code2 ? name : my_name);
	if (my_name)
	    free (my_name);
	return 1;
    }
    krb5_cc_close(ccache);
    return 0;
}
