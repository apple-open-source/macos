/* LGets.c */

#include "Sys.h"
#include "Util.h"
#include "LGets.h"

#ifdef HAVE_LIBREADLINE
#	include <readline/readline.h>
#	ifdef HAVE_READLINE_HISTORY_H
		/* Required for version 2.0 of readline. */
#		include <readline/history.h>
#	endif
#endif	/* HAVE_LIBREADLINE */

#ifdef HAVE_LIBGETLINE
#	include <getline.h>
#endif	/* HAVE_LIBGETLINE */

extern int gIsFromTTY, gIsToTTY;

char *StdioGets(char *promptStr, char *lineStr, size_t size)
{
	char *cp, *nl;

	if (gIsFromTTY) {
		/* It's okay to print a prompt if we are redirecting stdout,
		 * as long as stdin is still a tty.  Otherwise, don't print
		 * a prompt at all if stdin is redirected.
		 */
		(void) fputs(promptStr, stdout);
	}
	(void) fflush(stdout);	/* for svr4 */
	cp = fgets(lineStr, (int)(size - 2), stdin);
	if (cp != NULL) {
		nl = cp + strlen(cp) - 1;
		if (*nl == '\n')
			*nl = '\0';
	}
	return cp;
}	/* StdioGets */




#ifdef HAVE_LIBREADLINE
char *ReadlineGets(char *promptStr, char *lineStr, size_t size)
{
	char *dynamic;
	
	dynamic = readline(promptStr);
	if (dynamic == NULL)
		return NULL;
	if (dynamic[0] != '\0') {
		(void) Strncpy(lineStr, dynamic, size);
    	add_history(lineStr);
 	}
   	free(dynamic);
   	return (lineStr);
}	/* ReadlineGets */
#endif	/* HAVE_LIBREADLINE */





#ifdef HAVE_LIBGETLINE
char *GetlineGets(char *promptStr, char *lineStr, size_t size)
{
	char *cp, *nl;

	if ((cp = getline(promptStr)) != NULL) {
		if (*cp == '\0')	/* You hit ^D. */
			return NULL;
		cp = Strncpy(lineStr, cp, size);
		nl = cp + strlen(cp) - 1;
		if (*nl == '\n')
			*nl = '\0';
		if (*cp != '\0') {	/* Don't add blank lines to history buffer. */
			gl_histadd(cp);
		}
	}
	return (cp);
}	/* GetlineGets */
#endif	/* HAVE_LIBGETLINE */




/* Given a prompt string, a destination string, and its size, return feedback
 * from the user in the destination string, with any trailing newlines
 * stripped.  Returns NULL if EOF encountered.
 */
char *LineModeGets(char *promptStr, char *lineStr, size_t size)
{
	char *cp;
	longstring pLines;
	string p2;

	lineStr[0] = 0;	/* Clear it, in case of an error later. */
	if (gIsFromTTY && gIsToTTY) {
		/* Don't worry about a cmdline/history editor or prompts
		 * if you redirected a file at me.
		 */

		/* The prompt string may actually be several lines if the user put a
		 * newline in it with the @N option.  In this case we only want to print
		 * the very last line, so the command-line editors won't screw up.  So
		 * now we print all the lines except the last line.
		 */
		cp = strrchr(promptStr, '\n');
		if (cp != NULL) {
			STRNCPY(p2, cp + 1);
			STRNCPY(pLines, promptStr);
			cp = pLines + (int)(cp - promptStr);
			*cp = '\0';
			promptStr = p2;
			(void) fputs(pLines, stdout);
		}

#ifdef HAVE_LIBREADLINE
		return (ReadlineGets(promptStr, lineStr, size));
#endif	/* HAVE_LIBREADLINE */
#ifdef HAVE_LIBGETLINE
		return (GetlineGets(promptStr, lineStr, size));
#endif	/* HAVE_LIBGETLINE */
	}

	return (StdioGets(promptStr, lineStr, size));
}	/* LineModeGets */
