/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

# include "jam.h"
# include "filesys.h"

# if defined( unix ) || defined( NT ) || defined( __OS2__ )

# ifdef unix
# define DELIM '/'
# else
# define DELIM '\\'
# endif

/*
 * pathunix.c - manipulate file names on UNIX, NT, OS2
 *
 * External routines:
 *
 *	file_parse() - split a file name into dir/base/suffix/member
 *	file_build() - build a filename given dir/base/suffix/member
 *	file_parent() - make a FILENAME point to its parent dir
 *
 * File_parse() and file_build() just manipuate a string and a structure;
 * they do not make system calls.
 *
 * 04/08/94 (seiwald) - Coherent/386 support added.
 * 12/26/93 (seiwald) - handle dir/.suffix properly in file_build()
 * 12/19/94 (mikem) - solaris string table insanity support
 * 12/21/94 (wingerd) Use backslashes for pathnames - the NT way.
 * 02/14/95 (seiwald) - parse and build /xxx properly
 * 02/23/95 (wingerd) Compilers on NT can handle "/" in pathnames, so we
 *                    should expect hdr searches to come up with strings
 *                    like "thing/thing.h". So we need to test for "/" as
 *                    well as "\" when parsing pathnames.
 * 03/16/95 (seiwald) - fixed accursed typo on line 69.
 * 05/03/96 (seiwald) - split from filent.c, fileunix.c
 * 12/20/96 (seiwald) - when looking for the rightmost . in a file name,
 *		      don't include the archive member name.
 * 12/09/98 (mgobbi) - add support for :A modifier when expanding variables.
 */

/*
 * file_parse() - split a file name into dir/base/suffix/member
 */

void
file_parse( file, f )
char		*file;
FILENAME	*f;
{
	char *p, *q;
	char *end;
	
	memset( (char *)f, 0, sizeof( *f ) );

	/* Look for <grist> */

	if( file[0] == '<' && ( p = strchr( file, '>' ) ) )
	{
	    f->f_grist.ptr = file;
	    f->f_grist.len = p - file;
	    file = p + 1;
	}

	/* Look for dir/ */

	p = strrchr( file, '/' );

# ifndef UNIX
	/* On NT, look for dir\ as well */
	{
	    char *p1 = strrchr( file, '\\' );
	    p = p1 > p ? p1 : p;
	}
# endif

	if( p )
	{
	    f->f_dir.ptr = file;
	    f->f_dir.len = p - file;
	
	    /* Special case for / - dirname is /, not "" */

	    if( !f->f_dir.len )
		f->f_dir.len = 1;

# ifndef UNIX
	    /* Special case for D:/ - dirname is D:/, not "D:" */

	    if( f->f_dir.len == 2 && file[1] == ':' )
		f->f_dir.len = 3;
# endif

	    file = p + 1;
	}

	end = file + strlen( file );

	/* Look for (member) */

	if( ( p = strchr( file, '(' ) ) && end[-1] == ')' )
	{
	    f->f_member.ptr = p + 1;
	    f->f_member.len = end - p - 2;
	    end = p;
	} 

	/* Look for .suffix */
	/* This would be memrchr() */

	p = 0;
	q = file;

	while( q = memchr( q, '.', end - q ) )
	    p = q++;

	if( p )
	{
	    f->f_suffix.ptr = p;
	    f->f_suffix.len = end - p;
	    end = p;
	}

	/* Leaves base */

	f->f_base.ptr = file;
	f->f_base.len = end - file;

        /* and archive */

        if (strncmp (file, "lib", 3) == 0) {
            f->f_archive.ptr = file+3;
            f->f_archive.len = end - file - 3;
        } else {
            f->f_archive.ptr = file;
            f->f_archive.len = end - file;
        }
}

/*
 * file_build() - build a filename given dir/base/suffix/member
 */

void
file_build( f, file, binding )
FILENAME	*f;
char		*file;
int		binding;
{
	/* Start with the grist.  If the current grist isn't */
	/* surrounded by <>'s, add them. */

	if( f->f_grist.len )
	{
	    if( f->f_grist.ptr[0] != '<' ) *file++ = '<';
	    memcpy( file, f->f_grist.ptr, f->f_grist.len );
	    file += f->f_grist.len;
	    if( file[-1] != '>' ) *file++ = '>';
	}

	/* Don't prepend root if it's . or directory is rooted */

# ifdef unix

	if( f->f_root.len 
	    && !( f->f_root.len == 1 && f->f_root.ptr[0] == '.' )
	    && !( f->f_dir.len && f->f_dir.ptr[0] == '/' ) )

# else /* unix */

	if( f->f_root.len 
	    && !( f->f_root.len == 1 && f->f_root.ptr[0] == '.' )
	    && !( f->f_dir.len && f->f_dir.ptr[0] == '/' )
	    && !( f->f_dir.len && f->f_dir.ptr[0] == '\\' )
	    && !( f->f_dir.len && f->f_dir.ptr[1] == ':' ) )

# endif /* unix */

	{
	    memcpy( file, f->f_root.ptr, f->f_root.len );
	    file += f->f_root.len;
	    *file++ = DELIM;
	}

	if( f->f_dir.len )
	{
	    memcpy( file, f->f_dir.ptr, f->f_dir.len );
	    file += f->f_dir.len;
	}

	/* UNIX: Put / between dir and file */
	/* NT:   Put \ between dir and file */

	if( f->f_dir.len && ( f->f_base.len || f->f_suffix.len ) )
	{
	    /* UNIX: Special case for dir \ : don't add another \ */
	    /* NT:   Special case for dir / : don't add another / */

# ifndef UNIX
	    if( !( f->f_dir.len == 3 && f->f_dir.ptr[1] == ':' ) )
# endif
		if( !( f->f_dir.len == 1 && f->f_dir.ptr[0] == DELIM ) )
		    *file++ = DELIM;
	}

	if( f->f_base.len && !f->f_archive.len ) /* Just the base */
	{
	    memcpy( file, f->f_base.ptr, f->f_base.len );
	    file += f->f_base.len;
	}

        if( !f->f_base.len && f->f_archive.len ) /* Just the archive */
        {
            memcpy( file, f->f_archive.ptr, f->f_archive.len );
            file += f->f_archive.len;
        }

        if( f->f_base.len && f->f_archive.len ) /* Both base and archive */
        {
            /* slap down the "lib" from the base, if present */
            if( strncmp (f->f_base.ptr, "lib", 3) == 0 )
            {
                memcpy( file, f->f_base.ptr, 3 );
                file += 3;
            }

            /* add the archive name */
            memcpy( file, f->f_archive.ptr, f->f_archive.len );
            file += f->f_archive.len;
        }

	if( f->f_suffix.len )
	{
	    memcpy( file, f->f_suffix.ptr, f->f_suffix.len );
	    file += f->f_suffix.len;
	}

	if( f->f_member.len )
	{
	    *file++ = '(';
	    memcpy( file, f->f_member.ptr, f->f_member.len );
	    file += f->f_member.len;
	    *file++ = ')';
	}
	*file = 0;
}

/*
 *	file_parent() - make a FILENAME point to its parent dir
 */

void
file_parent( f )
FILENAME *f;
{
	/* just set everything else to nothing */

	f->f_base.ptr =
	f->f_suffix.ptr =
	f->f_member.ptr =
	f->f_archive.ptr = "";

	f->f_base.len = 
	f->f_suffix.len = 
	f->f_member.len =
	f->f_archive.len = 0;
}

# endif /* unix, NT, OS/2 */
