/*
** Copyright (C) 1998-2000 Greg Stein. All Rights Reserved.
**
** By using this file, you agree to the terms and conditions set forth in
** the LICENSE.html file which can be found at the top level of the mod_dav
** distribution or at http://www.webdav.org/mod_dav/license-1.html.
**
** Contact information:
**   Greg Stein, PO Box 760, Palo Alto, CA, 94302
**   gstein@lyra.org, http://www.webdav.org/mod_dav/
*/

/*
** Quick little utility to update propdb version codes to the latest
** version. This is handy when you know your propdb doesn't contain
** whatever problem caused the version bump.
**
** USAGE:
**   fixvers file1.pag file2.pag file3.pag ...
**
** TYPICAL USAGE:
**   find . -name "*.pag" -o -name ".*.pag" -print | xargs ./fixvers
*/


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>		/* for O_RDONLY, O_WRONLY */
#include "sdbm/sdbm.h"

/* --------------------------------------------------------------------
**
** This stuff must match the stuff in dav_props.c
**
** BEGIN:
*/

#define DAV_GDBM_NS_KEY		"METADATA"
#define DAV_GDBM_NS_KEY_LEN	8

#define DAV_DBVSN_MAJOR		4

/*
** END
** --------------------------------------------------------------------
*/

static const char *tweakname(const char *fname)
{
    int len = strlen(fname) - 4;
    char *result;

    if ( memcmp(&fname[len], ".pag", 4) != 0 )
    {
	printf("%s: ERROR: filename must end in \".pag\"\n", fname);
	return NULL;
    }

    result = malloc(len + 1);
    memcpy(result, fname, len);
    result[len] = '\0';
    return result;
}

static void fixfile(const char *fname)
{
    const char *msg;
    DBM *file;
    datum key;
    datum value;
    int rv;
    datum newval;

    file = sdbm_open((char *) fname, O_RDWR, 0);
    if ( file == NULL )
    {
	msg = "%s: ERROR: could not open\n";
	goto error;
    }

    key.dptr = DAV_GDBM_NS_KEY;
    key.dsize = DAV_GDBM_NS_KEY_LEN;
    value = sdbm_fetch(file, key);
    if ( value.dptr == NULL )
    {
	msg = "%s: ERROR: missing metadata (not a mod_dav propdb?)\n";
	goto error;
    }

    if (*value.dptr == DAV_DBVSN_MAJOR)
    {
	msg = "%s: skipped: already contains proper version number\n";
	goto error;
    }

    newval.dptr = malloc(value.dsize);
    newval.dsize = value.dsize;
    memcpy(newval.dptr, value.dptr, newval.dsize);
    *newval.dptr = DAV_DBVSN_MAJOR;
    rv = sdbm_store(file, key, newval, DBM_REPLACE);
    free(newval.dptr);
    if ( rv == -1 )
    {
	msg = "%s: ERROR: could not save metadata back to propdb\n";
	goto error;
    }

    msg = "%s: updated version number\n";

  error:
    sdbm_close(file);
    
    printf(msg, fname);
}

int main(int argc, const char **argv)
{
    int i;

    for (i = 1; i < argc; ++i)
    {
	const char *fname = tweakname(argv[i]);

	if ( fname != NULL )
	{
	    fixfile(fname);
	    free((void *)fname);
	}
    }

    return 0;
}
