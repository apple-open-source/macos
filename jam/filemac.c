/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

# include "jam.h"
# include "filesys.h"

# ifdef macintosh

# include <sys/types.h>
# include <stat.h>
# include <dirent.h>

/*
 * filemac.c - manipulate file names and scan directories on macintosh
 *
 * External routines:
 *
 *	file_dirscan() - scan a directory for files
 *	file_time() - get timestamp of file, if not done by file_dirscan()
 *	file_archscan() - scan an archive for files
 *
 * File_dirscan() and file_archscan() call back a caller provided function
 * for each file found.  A flag to this callback function lets file_dirscan()
 * and file_archscan() indicate that a timestamp is being provided with the
 * file.   If file_dirscan() or file_archscan() do not provide the file's
 * timestamp, interested parties may later call file_time().
 *
 * 04/08/94 (seiwald) - Coherent/386 support added.
 * 12/19/94 (mikem) - solaris string table insanity support
 * 02/14/95 (seiwald) - parse and build /xxx properly
 * 05/03/96 (seiwald) - split into pathunix.c
 * 11/21/96 (peterk) - BEOS does not have Unix-style archives
 */

/*
 * file_dirscan() - scan a directory for files
 */

void
file_dirscan( dir, func )
char	*dir;
void	(*func)();
{
	FILENAME f;
	DIR *d;
	struct dirent *dirent;
	char filename[ MAXJPATH ];
	struct stat statbuf;

	/* First enter directory itself */

	memset( (char *)&f, '\0', sizeof( f ) );

	f.f_dir.ptr = dir;
	f.f_dir.len = strlen(dir);

	if( DEBUG_BINDSCAN )
	    printf( "scan directory %s\n", dir );
		
	/* Special case ":" - enter it */

	if( f.f_dir.len == 1 && f.f_dir.ptr[0] == ':' )
	    (*func)( dir, 0 /* not stat()'ed */, (time_t)0 );

	/* Now enter contents of directory */

	if( !( d = opendir( dir ) ) )
	    return;

	while( dirent = readdir( d ) )
	{
	    f.f_base.ptr = dirent->d_name;
	    f.f_base.len = strlen( f.f_base.ptr );

	    file_build( &f, filename, 0 );

	    (*func)( filename, 0 /* not stat()'ed */, (time_t)0 );
	}

	closedir( d );
}

/*
 * file_time() - get timestamp of file, if not done by file_dirscan()
 */

int
file_time( filename, time )
char	*filename;
time_t	*time;
{
	struct stat statbuf;

	if( stat( filename, &statbuf ) < 0 )
	    return -1;

	*time = statbuf.st_mtime;
	
	return 0;
}

/*
 * file_archscan() - scan an archive for files
 */

void
file_archscan( archive, func )
char *archive;
void (*func)();
{
}


# endif /* macintosh */

