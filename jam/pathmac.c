/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

# include "jam.h"
# include "filesys.h"

# ifdef macintosh

# define DELIM ':'

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

	/* Look for dir: */

	p = strrchr( file, DELIM );

	if( p )
	{
	    f->f_dir.ptr = file;
	    f->f_dir.len = p - file;
	    
	    /* Dir of : is : */
#if 0
	    if( !f->f_dir.len )
	    	++f->f_dir.len;
# else
	    f->f_dir.len++;
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
 
# define DIR_EMPTY	0	/* "" */
# define DIR_DOT	1	/* : */
# define DIR_DOTDOT	2	/* :: */
# define DIR_ABS	3	/* dira:dirb: */
# define DIR_REL	4	/* :dira:dirb: */

# define G_DIR		0	/* take dir */
# define G_ROOT		1	/* take root */
# define G_CAT		2	/* prepend root to dir */
# define G_DTDR		3	/* :: of rel dir */
# define G_DDDD		4	/* make it ::: (../..) */
# define G_MT		5	/* leave it empty */

char grid[5][5] = {
/* 		EMPTY	DOT	DOTDOT	ABS	REL */
/* EMPTY */   {	G_MT,	G_DIR,	G_DIR,	G_DIR,	G_DIR },
/* DOT */     {	G_ROOT,	G_DIR,	G_DIR,	G_DIR,	G_DIR },
/* DOTDOT */  {	G_ROOT,	G_ROOT,	G_DDDD,	G_DIR,	G_DTDR },
/* ABS */     {	G_ROOT,	G_ROOT, G_ROOT,	G_DIR,	G_CAT },
/* REL */     {	G_ROOT,	G_ROOT,	G_ROOT,	G_DIR,	G_CAT }
} ;

static int
file_flags( ptr, len )
char	*ptr;
int	len;
{
	if( !len )
	    return DIR_EMPTY;
	if( len == 1 && ptr[0] == DELIM )
	    return DIR_DOT;
	if( len == 2 && ptr[0] == DELIM && ptr[1] == DELIM )
	    return DIR_DOTDOT;
	if( ptr[0] == DELIM )
	    return DIR_REL;
	return DIR_ABS;
}

void
file_build( f, file, binding )
FILENAME	*f;
char		*file;
int		binding;
{
	char *ofile = file;
	int dflag, rflag, act;
	
	if( DEBUG_SEARCH )
	{
	printf("build file: ");
	if( f->f_root.len )
		printf( "root = '%.*s' ", f->f_root.len, f->f_root.ptr );
	if( f->f_dir.len )
		printf( "dir = '%.*s' ", f->f_dir.len, f->f_dir.ptr );
	if( f->f_base.len )
		printf( "base = '%.*s' ", f->f_base.len, f->f_base.ptr );
	}
	
	/* Start with the grist.  If the current grist isn't */
	/* surrounded by <>'s, add them. */

	if( f->f_grist.len )
	{
	    if( f->f_grist.ptr[0] != '<' ) *file++ = '<';
	    memcpy( file, f->f_grist.ptr, f->f_grist.len );
	    file += f->f_grist.len;
	    if( file[-1] != '>' ) *file++ = '>';
	}
	
	/* Combine root & directory, according to the grid. */
	
	dflag = file_flags( f->f_dir.ptr, f->f_dir.len );
	rflag = file_flags( f->f_root.ptr, f->f_root.len );
	
	switch( act = grid[ rflag ][ dflag ] )
	{
	case G_DTDR:
		/* :: of rel dir */
		*file++ = DELIM;
		/* fall through */
		
	case G_DIR: 	
		/* take dir */
		memcpy( file, f->f_dir.ptr, f->f_dir.len );
	    	file += f->f_dir.len;
		break;
		
	case G_ROOT:	
		/* take root */
		memcpy( file, f->f_root.ptr, f->f_root.len );
	    	file += f->f_root.len;
		break;
	    
	case G_CAT:	
		/* prepend root to dir */
		memcpy( file, f->f_root.ptr, f->f_root.len );
	    	file += f->f_root.len;
		if( file[-1] == DELIM ) --file;
		memcpy( file, f->f_dir.ptr, f->f_dir.len );
	    	file += f->f_dir.len;
		break;
	
	case G_DDDD:	
		/* make it ::: (../..) */
		strcpy( file, ":::" );
		file += 3;
		break;
	}

	/* Put : between dir and file (if none already) */
	
	if( act != G_MT && 
	    file[-1] != DELIM && 
	    ( f->f_base.len || f->f_suffix.len ) )
	{
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
	
	if( DEBUG_SEARCH )
		printf(" -> '%s'\n", ofile);
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
	f->f_member.ptr = "";

	f->f_base.len = 
	f->f_suffix.len = 
	f->f_member.len = 0;
}

# endif /* macintosh */
