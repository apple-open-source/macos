/* $Xorg: remote.c,v 1.5 2001/02/09 02:06:01 xorgcvs Exp $ */
/******************************************************************************

Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
******************************************************************************/
/* $XFree86: xc/programs/xsm/remote.c,v 1.5 2001/12/14 20:02:26 dawes Exp $ */

/*
 * We use the rstart protocol to restart clients on remote machines.
 */

#include "xsm.h"
#include "log.h"

#include <X11/ICE/ICEutil.h>

static char 		*format_rstart_env(char *str);

extern IceAuthDataEntry	*authDataEntries;
extern int		numTransports;


void
remote_start(char *restart_protocol, char *restart_machine, char *program, 
	     char **args, char *cwd, char **env,
	     char *non_local_display_env, char *non_local_session_env)
{
    FILE *fp;
    int	 pipefd[2];
    char msg[256];
    int  i;

    if (strcmp (restart_protocol, "rstart-rsh") != 0)
    {
	if (verbose)
	    printf ("Only rstart-rsh remote execution protocol supported.\n");
	return;
    }

    if (!restart_machine)
    {
	if (verbose)
	    printf ("Bad remote machine specified for remote execute.\n");
	return;
    }

    if (verbose)
	printf ("Attempting to restart remote client on %s\n",
	    restart_machine);

    if (pipe (pipefd) < 0)
    {
	sprintf (msg, "%s: pipe() error during remote start of %s",
	    Argv[0], program);
	add_log_text (msg);
	perror (msg);
    }
    else
    {
	switch(fork())
	{
	case -1:

	    sprintf (msg, "%s: fork() error during remote start of %s",
		Argv[0], program);
	    add_log_text (msg);
	    perror (msg);
	    break;

	case 0:		/* kid */

	    close (pipefd[1]);
	    close (0);
	    dup (pipefd[0]);
	    close (pipefd[0]);

	    execlp (RSHCMD, restart_machine, "rstartd", (char *) 0);

	    sprintf (msg,
	        "%s: execlp() of rstartd failed for remote start of %s",
		Argv[0], program);
	    perror (msg);
	    /*
	     * TODO : We would like to send this log information to the
	     * log window in the parent.  This would require using the
	     * pipe between the parent and child.  The child would
	     * set close-on-exec.  If the exec succeeds, the pipe will
	     * be closed.  If it fails, the child can write a message
	     * to the parent.
	     */
	    _exit(255);

	default:		/* parent */

	    close (pipefd[0]);
	    fp = (FILE *) fdopen (pipefd[1], "w");

	    fprintf (fp, "CONTEXT X\n");
	    fprintf (fp, "DIR %s\n", cwd);
	    fprintf (fp, "DETACH\n");

	    if (env)
	    {
		/*
		 * The application saved its environment.
		 */

		for (i = 0; env[i]; i++)
		{
		    /*
		     * rstart requires that any spaces, backslashes, or
		     * non-printable characters inside of a string be
		     * represented by octal escape sequences.
		     */

		    char *temp = format_rstart_env (env[i]);
		    fprintf (fp, "MISC X %s\n", temp);
		    if (temp != env[i])
			XtFree (temp);
		}
	    }
	    else
	    {
		/*
		 * The application did not save its environment.
		 * The default PATH set up by rstart may not contain
		 * the program we want to restart.  We play it safe
		 * and pass xsm's PATH.  This will most likely contain
		 * the path we need.
		 */

		char *path = (char *) getenv ("PATH");

		if (path)
		    fprintf (fp, "MISC X PATH=%s\n", path);
	    }

	    fprintf (fp, "MISC X %s\n", non_local_display_env);
	    fprintf (fp, "MISC X %s\n", non_local_session_env);

	    /*
	     * Pass the authentication data.
	     * Each transport has auth data for ICE and XSMP.
	     * Don't pass local auth data.
	     */

	    for (i = 0; i < numTransports * 2; i++)
	    {
		if (Strstr (authDataEntries[i].network_id, "local/"))
		    continue;

		fprintf (fp, "AUTH ICE %s \"\" %s %s ",
		    authDataEntries[i].protocol_name,
		    authDataEntries[i].network_id,
		    authDataEntries[i].auth_name);
		
		fprintfhex (fp,
		    authDataEntries[i].auth_data_length,
		    authDataEntries[i].auth_data);
		
		fprintf (fp, "\n");
	    }

	    /*
	     * And execute the program
	     */

	    fprintf (fp, "EXEC %s %s", program, program);
	    for (i = 1; args[i]; i++)
		fprintf (fp, " %s", args[i]);
	    fprintf (fp, "\n\n");
	    fclose (fp);
	    break;
	}
    }
}



/*
 * rstart requires that any spaces/backslashes/non-printable characters
 * inside of a string be represented by octal escape sequences.
 */

static char *
format_rstart_env(char *str)
{
    int escape_count = 0, i;
    char *temp = str;

    while (*temp != '\0')
    {
	if (!isgraph (*temp) || *temp == '\\')
	    escape_count++;
	temp++;
    }

    if (escape_count == 0)
	return (str);
    else
    {
	int len = strlen (str) + 1 + (escape_count * 3);
	char *ret = (char *) XtMalloc (len);
	char *ptr = ret;

	temp = str;
	while (*temp != '\0')
	{
	    if (!isgraph (*temp) || *temp == '\\')
	    {
		char octal[3];
		sprintf (octal, "%o", *temp);
		*(ptr++) = '\\';
		for (i = 0; i < (3 - (int) strlen (octal)); i++)
		    *(ptr++) = '0';
		strcpy (ptr, octal);
		ptr += strlen (octal);
	    }
	    else
		*(ptr++) = *temp;

	    temp++;
	}

	*ptr = '\0';
	return (ret);
    }
}
