/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                    gdb_complete.c                                    |
 |                                                                                      |
 |                            Gdb command completion routines                           |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2001                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*
 
 Most of the code in this file was taken from gdb's own complete.c file to display
 word completions exactly like gdb would display them but using the provided hook
 to handle it here instead of letting gdb do it.  We do this because we need to
 capture the output.  Gdb's internal way is just to write to stdout.
*/

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdarg.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gdb_private_interfaces.h"

#define CTRL(c) 	((c) & 0x1F)
#define RUBOUT 		0x7F
#define ABORT_CHAR 	CTRL('G')
#define CTRL_CHAR(c) 	((c) < 0x020 && (c) >= 0)
#define UNCTRL(c) 	(islower(((c)|0x40)) ? toupper(((c)|0x40)) : (((c)|0x40)))

void (*__word__completion_hook)(int save_input_cursor) = NULL;
void (*__word__completion_query_hook)(GDB_FILE *stream, char *format, ...) = NULL;
int (*__word__completion_read_hook)(void) = NULL;

/*--------------------------------------------------------------------------------------*/

#define rl_outstream NULL
//extern FILE *rl_outstream;
//#define crlf() putc('\n', rl_outstream)

static int _putc(int c, void *stream)
{
    char ch = c;
    gdb_fprintf(stream == stderr ? gdb_stderr : gdb_stdout, "%c", ch);
}

static int _printf(void *stream, const char *format, ...)
{
    char line[1024];
    
    va_list ap;
    va_start(ap, format);
    vsprintf(line, format, ap);
    va_end(ap);
    
    gdb_fprintf(stream == stderr ? gdb_stderr : gdb_stdout, "%s", line);
}

static int _fflush(void *stream)
{
    gdb_fflush(stream == stderr ? gdb_stderr : gdb_stdout);
}

static crlf(void)
{
    _putc('\n', gdb_stdout);
}

#undef putc
#undef printf
#undef fflush
#define putc   _putc
#define printf _printf
#define fflush _fflush

/*--------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*
 | get_y_or_n - user must press "y" or "n". Non-zero return means "y" pressed |
 *----------------------------------------------------------------------------*/

static int get_y_or_n(void)
{
    int c;
    
    for (;;) {
    	if (__word__completion_read_hook)
	    c = __word__completion_read_hook();
	else
	    c = rl_read_key();
	if (c == 'y' || c == 'Y' || c == ' ')
	    return (1);
	if (c == 'n' || c == 'N' || c == RUBOUT)
	    return (0);
	if (c == ABORT_CHAR)
	    _rl_abort_internal();
	rl_ding();
      }
}


/*----------------------------------------------------------------*
 | stat_char - return the character which best describes filename |
 *----------------------------------------------------------------*
 
 The following characters are returned:

     `@' for symbolic links
     `/' for directories
     `*' for executables
     `=' for sockets
     `|' for FIFOs
     `%' for character special devices
     `#' for block special devices
*/

static int stat_char(char *filename)
{
    struct stat finfo;
    int 	character, r;
    
    #if defined(HAVE_LSTAT) && defined(S_ISLNK)
    r = lstat(filename, &finfo);
    #else
    r = stat(filename, &finfo);
    #endif
    
    if (r == -1)
	return (0);
    
    character = 0;
    
    if (S_ISDIR(finfo.st_mode))
	character = '/';
    #if defined(S_ISCHR)
    else if (S_ISCHR(finfo.st_mode))
	character = '%';
    #endif /* S_ISCHR */
    #if defined(S_ISBLK)
    else if (S_ISBLK(finfo.st_mode))
	character = '#';
    #endif /* S_ISBLK */
    #if defined(S_ISLNK)
    else if (S_ISLNK(finfo.st_mode))
	character = '@';
    #endif /* S_ISLNK */
    #if defined(S_ISSOCK)
    else if (S_ISSOCK(finfo.st_mode))
	character = '=';
    #endif /* S_ISSOCK */
    #if defined(S_ISFIFO)
    else if (S_ISFIFO(finfo.st_mode))
	character = '|';
    #endif
    else if (S_ISREG(finfo.st_mode)) {
	if (access(filename, 1) == 0)
	  character = '*';
    }
    
    return (character);
}


/*-----------------------------------------------------------------------------------*
 | printable_part - return portion of pathname to be output when listing completions |
 *-----------------------------------------------------------------------------------*

 Returns the portion of PATHNAME that should be output when listing possible completions.
 If we are hacking filename completion, we are only interested in the basename, the
 portion following the final slash.  Otherwise, we return what we were passed.
*/

static char *printable_part(char *pathname)
{
    char *temp = rl_filename_completion_desired ? strrchr(pathname, '/') : (char *)NULL;
    return (temp ? ++temp : pathname);
}


/*-----------------------------------*
 | print_filename - print a filename |
 *-----------------------------------*

 If to_print != full_pathname, to_print is the basename of the path passed. In this case,
 we try to expand the directory name before checking for the stat character. 

 The function returns the number of characters we output.
*/

static int print_filename(char *to_print, char *full_pathname)
{
    int  printed_len = 0;
    char *s, c, *new_full_pathname;
    int  extension_char, slen, tlen;
   
    #define PUTX(c) 				\
	do { 					\
	  if (CTRL_CHAR(c)) 			\
	    { 					\
	      putc('^', stdout); 		\
	      putc(UNCTRL(c), stdout); 		\
	      printed_len += 2; 		\
	    } 					\
	  else if (c == RUBOUT) 		\
	    { 					\
	      putc('^', stdout); 		\
	      putc('?', stdout); 		\
	      printed_len += 2; 		\
	    } 					\
	  else 					\
	    { 					\
	      putc(c, stdout); 			\
	      printed_len++; 			\
	    }					\
	} while (0)

    
    for (s = to_print; *s; s++)
	PUTX(*s);
    
    if (rl_filename_completion_desired && rl_visible_stats) {
	/* If to_print != full_pathname, to_print is the basename of the
	   path passed.  In this case, we try to expand the directory
	   name before checking for the stat character. */
	if (to_print != full_pathname) {
	    /* Terminate the directory name. */
	    c = to_print[-1];
	    to_print[-1] = '\0';
    
	    s = tilde_expand(full_pathname);
    
	    slen = strlen(s);
	    tlen = strlen(to_print);
	    new_full_pathname = xmalloc(slen + tlen + 2);
	    strcpy(new_full_pathname, s);
	    new_full_pathname[slen] = '/';
	    strcpy(new_full_pathname + slen + 1, to_print);
    
	    extension_char = stat_char(new_full_pathname);
    
	    free(new_full_pathname);
	    to_print[-1] = c;
	} else {
	    s = tilde_expand(full_pathname);
	    extension_char = stat_char(s);
	}
    
	free(s);
	
	if (extension_char) {
	    putc(extension_char, stdout);
	    printed_len++;
	}
    }
    
    return (printed_len);
}


/*---------------------------------------------------------------------------*
 | __cmd_completion_display_hook - word completion list display routine hook |
 *---------------------------------------------------------------------------*

 When the global rl_completion_display_matches_hook is set to point to this routine
 the list of num_matches matches is passed here for display instead of using gdb's
 internal display.  
 
 The matches is the array of strings that matched, num_matches is the number of strings
 in that array, and max_length is the length of the  longest string in that array.
*/

void __cmd_completion_display_hook(char **matches, int num_matches, int max_length)
{
    int  count, limit, printed_len;
    int  len, max, i, j, k, l;
    char *temp, prompt[1024];
    int screenheight, screenwidth;

    if (__word__completion_hook)
    	__word__completion_hook(1);
    else
    	gdb_fprintf(gdb_default_stderr, "\0337");
    
    /* There is always more than one match when the hook is called.  Find out how many	*/
    /* there are, and find the maximum printed length of a single entry. 		*/
  
    for (max = 0, i = 1; matches[i]; i++) {
	temp = printable_part(matches[i]);
	len = strlen(temp);
	if (len > max)
	    max = len;
    }

    len = i - 1;
   
    /* If there are many items, then ask the user if she really wants to see them all. 	*/
    
    if (len >= rl_completion_query_items) {
	if (__word__completion_query_hook)
	    __word__completion_query_hook(gdb_stdout, "Display all %d possibilities? (y or n) ", len);
	else {
	    crlf();
	    printf(stdout, "Display all %d possibilities? (y or n) ", len);
	}
	//if (gdb_current_stderr != gdb_default_stderr)
	//    printf(stdout, "\n");		/* complete the line			*/
	fflush(stdout);
	//gdb_fflush(gdb_default_stderr);
	i = get_y_or_n();
	if (i == 0) {
	    if (__word__completion_hook)
		__word__completion_hook(0);
	    else {
	    	crlf();
    	    	gdb_fprintf(gdb_default_stderr, "\0338");
	    }
	    return;
	}
    }
    
    /* A convenience function for displaying a list of strings in
       columnar format on readline's output stream.  MATCHES is the list
       of strings, in argv format, LEN is the number of strings in MATCHES,
       and MAX is the length of the longest string in MATCHES. */
    
    rl_get_screen_size (&screenheight, &screenwidth);

    /* How many items of MAX length can we fit in the screen window? */
    max += 2;
    limit = screenwidth / max;
    if (limit != 1 && (limit * max == screenwidth))
	limit--;
    
    /* Avoid a possible floating exception.  If max > screenwidth,
       limit will be 0 and a divide-by-zero fault will result. */
    if (limit == 0)
	limit = 1;
    
    /* How many iterations of the printing loop? */
    count = (len + (limit - 1)) / limit;
    
    /* Watch out for special case.  If LEN is less than LIMIT, then
       just do the inner printing loop.  0 < len <= limit  implies  count = 1. */
    
    /* Sort the items if they are not already sorted. */
    if (rl_ignore_completion_duplicates == 0)
	qsort(matches + 1, len, sizeof(char *), _rl_qsort_string_compare);
    
    crlf();
    
    if (_rl_print_completions_horizontally == 0) {
	/* Print the sorted items, up-and-down alphabetically, like ls. */
	for (i = 1; i <= count; i++) {
	    for (j = 0, l = i; j < limit; j++) {
		if (l > len || matches[l] == 0)
		    break;
		else {
		    temp = printable_part(matches[l]);
		    printed_len = print_filename(temp, matches[l]);
    
		    if (j + 1 < limit)
		      for (k = 0; k < max - printed_len; k++)
			putc(' ', rl_outstream);
		}
		l += count;
	    }
	    crlf();
	}
    } else {
	/* Print the sorted items, across alphabetically, like ls -x. */
	for (i = 1; matches[i]; i++) {
	    temp = printable_part(matches[i]);
	    printed_len = print_filename(temp, matches[i]);
	    /* Have we reached the end of this line? */
	    if (matches[i+1]) {
		if (i && (limit > 1) && (i % limit) == 0)
		    crlf();
		else
		    for (k = 0; k < max - printed_len; k++)
			putc(' ', rl_outstream);
	    }
	}
	crlf();
      }
     
    gdb_fflush(gdb_current_stdout);
    
    if (__word__completion_hook)
    	__word__completion_hook(0);
    else
    	gdb_fprintf(gdb_default_stderr, "\0338");
}
