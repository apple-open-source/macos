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
 * 03/12/02 (anders) - add support for :Q (quotation) and :E (escaping)
 *                    modifiers when expanding variables.
 */

/*
 * file_parse() - split a file name into dir/base/suffix/member
 */

void
file_parse( file, f )
char		*file;
FILENAME	*f;
{
#ifdef APPLE_EXTENSIONS
	char *    file_base = file;
#endif
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

# ifndef unix
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

# ifndef unix
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

	while( (q = memchr( q, '.', end - q )) != NULL )
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
#ifdef APPLE_EXTENSIONS
	if ( DEBUG_VARSET ) {
	    fprintf(stderr, "[parsed %.*s as (G:%.*s R:%.*s D:%.*s B:%.*s A:%.*s S:%.*s M:%.*s)]\n", (file-file_base), file_base, f->f_grist.len, f->f_grist.ptr, f->f_root.len, f->f_root.ptr, f->f_dir.len, f->f_dir.ptr, f->f_base.len, f->f_base.ptr, f->f_archive.len, f->f_archive.ptr, f->f_suffix.len, f->f_suffix.ptr, f->f_member.len, f->f_member.ptr);
	}
#endif
}



#ifdef APPLE_EXTENSIONS

static unsigned append_quoted_characters (char * buffer, const char * characters, unsigned length, unsigned quoting_style, unsigned * needs_quoting_ptr)
    /** Private function that appends the 'length' characters starting at 'characters' to the buffer starting at 'buffer', escaping them according to the quoting style 'quoting_style' if they contain any characters that require such escaping.  If 'needs_quoting_ptr' is not NULL, this function also sets it to 1 if any characters requiring quoting were encountered (wether or not they needed to be backslash-escaped) or to 0 if no such characters were encountered.  This function returns the total number of characters appended to the buffer -- if any backslash-escaping was done, this number will be greater than 'length'.  This method follows 'sh' quoting rules. */
{
    if (quoting_style == NO_QUOTING) {
	// No quoting is requested, so we just copy the characters -- this is the fast-path case.
	memcpy(buffer, characters, length);
	if (needs_quoting_ptr != NULL) *needs_quoting_ptr = 0;
	return length;
    }
    else {
	// Otherwise we process the string character by character, and backslash-escape characters as we go.  We also keep track of whether any characters caused us to need quoting (regardless of whether or not they were also escaped).
	register char *   bufp = buffer;
	unsigned          needs_quoting = 0;
	unsigned          i;

	if (quoting_style == SINGLE_QUOTING) {
	    // From 'sh' man page:  Enclosing characters in single quotes preserves the literal meaning of all the characters (except single quotes, making it impossible to put single-quotes in a single-quoted string).
	    for (i = 0; i < length; i++) {
		register char ch = characters[i];
		if (ch == '\'') {
		    // We do a special trick that involves ending the already-started single-quoting, putting in a backslash followed by a single quote, and then restarting single-quoting.
		    *bufp++ = '\'';
		    *bufp++ = '\\';
		    *bufp++ = '\'';
		    *bufp++ = '\'';
		    needs_quoting = 1;
		}
		else if (!isalnum(ch)  &&  ch != '_'  &&  ch != '+'  &&  ch != '-'  &&  ch != '.'  &&  ch != '/'  &&  ch != ','  &&  ch != '~') {
		    *bufp++ = ch;
		    needs_quoting = 1;
		}
		else {
		    *bufp++ = ch;
		}
	    }
	}
	else if (quoting_style == DOUBLE_QUOTING) {
	    // From 'sh' man page:  Enclosing characters within double quotes preserves the literal meaning of all characters except dollarsign ($), backquote (`), and backslash (\).  The backslash inside double quotes is historically weird, and serves to quote only the following characters: $  `  "  \  <newline>.  Otherwise it remains literal.
	    for (i = 0; i < length; i++) {
		register char ch = characters[i];
		if (ch == '$'  ||  ch == '`'  ||  ch == '\"'  ||  ch == '\\') {
		    *bufp++ = '\\';
		    needs_quoting = 1;
		}
		else if (!isalnum(ch)  &&  ch != '_'  &&  ch != '+'  &&  ch != '-'  &&  ch != '.'  &&  ch != '/'  &&  ch != ','  &&  ch != '~') {
		    needs_quoting = 1;
		}
		*bufp++ = ch;
	    }
	}
	else if (quoting_style == BACKSLASH_QUOTING) {
	    // From 'sh' man page:  A backslash preserves the literal meaning of the following character, with the exception of <newline>.  A backslash preceding a <newline> is treated as a line continuation.
	    for (i = 0; i < length; i++) {
		register char ch = characters[i];
		if (!isalnum(ch)  &&  ch != '_'  &&  ch != '+'  &&  ch != '-'  &&  ch != '.'  &&  ch != '/'  &&  ch != ','  &&  ch != '~') {
		    *bufp++ = '\\';
		    needs_quoting = 1;
		}
		*bufp++ = ch;
	    }
	}
	else {
	    memcpy(bufp, characters, length);
	    bufp += length;
	}
	if (needs_quoting_ptr != NULL) *needs_quoting_ptr = needs_quoting;
	return (bufp - buffer);
    }
}

#endif


/*
 * file_build() - build a filename given dir/base/suffix/member
 */

void
file_build( f, file, binding )
FILENAME	*f;
char		*file;
int		binding;
{
#ifdef APPLE_EXTENSIONS
	char *    file_base = file;
	unsigned  any_substring_needs_quotes = 0;
	unsigned  this_substring_needs_quotes = 0;
#endif

	/* Start with the grist.  If the current grist isn't */
	/* surrounded by <>'s, add them. */
	if( f->f_grist.len )
	{
	    if( f->f_grist.ptr[0] != '<' ) *file++ = '<';
#ifdef APPLE_EXTENSIONS
	    file += append_quoted_characters(file, f->f_grist.ptr, f->f_grist.len, f->quoting_style, &this_substring_needs_quotes);
	    any_substring_needs_quotes |= this_substring_needs_quotes;
#else
	    memcpy( file, f->f_grist.ptr, f->f_grist.len );
	    file += f->f_grist.len;
#endif
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
#ifdef APPLE_EXTENSIONS
	    file += append_quoted_characters(file, f->f_root.ptr, f->f_root.len, f->quoting_style, &this_substring_needs_quotes);
	    any_substring_needs_quotes |= this_substring_needs_quotes;
#else
	    memcpy( file, f->f_root.ptr, f->f_root.len );
	    file += f->f_root.len;
#endif
	    *file++ = DELIM;
	}

	if( f->f_dir.len )
	{
#ifdef APPLE_EXTENSIONS
	    file += append_quoted_characters(file, f->f_dir.ptr, f->f_dir.len, f->quoting_style, &this_substring_needs_quotes);
	    any_substring_needs_quotes |= this_substring_needs_quotes;
#else
	    memcpy( file, f->f_dir.ptr, f->f_dir.len );
	    file += f->f_dir.len;
#endif
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
#ifdef APPLE_EXTENSIONS
	    file += append_quoted_characters(file, f->f_base.ptr, f->f_base.len, f->quoting_style, &this_substring_needs_quotes);
	    any_substring_needs_quotes |= this_substring_needs_quotes;
#else
	    memcpy( file, f->f_base.ptr, f->f_base.len );
	    file += f->f_base.len;
#endif
	}

        if( !f->f_base.len && f->f_archive.len ) /* Just the archive */
        {
#ifdef APPLE_EXTENSIONS
	    file += append_quoted_characters(file, f->f_archive.ptr, f->f_archive.len, f->quoting_style, &this_substring_needs_quotes);
	    any_substring_needs_quotes |= this_substring_needs_quotes;
#else
            memcpy( file, f->f_archive.ptr, f->f_archive.len );
            file += f->f_archive.len;
#endif
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
#ifdef APPLE_EXTENSIONS
	    file += append_quoted_characters(file, f->f_archive.ptr, f->f_archive.len, f->quoting_style, &this_substring_needs_quotes);
	    any_substring_needs_quotes |= this_substring_needs_quotes;
#else
            memcpy( file, f->f_archive.ptr, f->f_archive.len );
            file += f->f_archive.len;
#endif
        }

	if( f->f_suffix.len )
	{
#ifdef APPLE_EXTENSIONS
	    file += append_quoted_characters(file, f->f_suffix.ptr, f->f_suffix.len, f->quoting_style, &this_substring_needs_quotes);
	    any_substring_needs_quotes |= this_substring_needs_quotes;
#else
	    memcpy( file, f->f_suffix.ptr, f->f_suffix.len );
	    file += f->f_suffix.len;
#endif
	}

	if( f->f_member.len )
	{
	    *file++ = '(';
#ifdef APPLE_EXTENSIONS
	    file += append_quoted_characters(file, f->f_member.ptr, f->f_member.len, f->quoting_style, &this_substring_needs_quotes);
	    any_substring_needs_quotes |= this_substring_needs_quotes;
#else
	    memcpy( file, f->f_member.ptr, f->f_member.len );
	    file += f->f_member.len;
#endif
	    *file++ = ')';
	}
#ifdef APPLE_EXTENSIONS
	if (any_substring_needs_quotes  &&  (f->quoting_style == DOUBLE_QUOTING  ||  f->quoting_style == SINGLE_QUOTING)) {
	    memmove(file_base+1, file_base, (file++ - file_base));
	    file_base[0] = (f->quoting_style == DOUBLE_QUOTING) ? '\"' : '\'';
	    *file++ = (f->quoting_style == DOUBLE_QUOTING) ? '\"' : '\'';
	}
	if ( DEBUG_VARSET ) {
	    fprintf(stderr, "[quoted (G:%.*s R:%.*s D:%.*s B:%.*s A:%.*s S:%.*s M:%.*s)%s as %.*s]\n", f->f_grist.len, f->f_grist.ptr, f->f_root.len, f->f_root.ptr, f->f_dir.len, f->f_dir.ptr, f->f_base.len, f->f_base.ptr, f->f_archive.len, f->f_archive.ptr, f->f_suffix.len, f->f_suffix.ptr, f->f_member.len, f->f_member.ptr, (f->quoting_style == DOUBLE_QUOTING ? ":Q" : (f->quoting_style == SINGLE_QUOTING ? ":q" : (f->quoting_style == BACKSLASH_QUOTING ? ":E" : ""))), (file-file_base), file_base);
	}
#endif
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
