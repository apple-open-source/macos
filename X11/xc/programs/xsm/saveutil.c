/* $Xorg: saveutil.c,v 1.5 2001/02/09 02:06:01 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xsm/saveutil.c,v 1.6 2001/12/14 20:02:27 dawes Exp $ */

#include "xsm.h"
#include "log.h"
#include "saveutil.h"

char 		 session_save_file[PATH_MAX];

extern Widget manualRestartCommands;


void
set_session_save_file_name(char *session_name)
{
    char *p;

    p = (char *) getenv ("SM_SAVE_DIR");
    if (!p)
    {
	p = (char *) getenv ("HOME");
	if (!p)
	    p = ".";
    }

    strcpy (session_save_file, p);
    strcat (session_save_file, "/.XSM-");
    strcat (session_save_file, session_name);
}



int
ReadSave(char *session_name, char **sm_id)
{
    char		*buf;
    int			buflen;
    char		*p;
    PendingClient	*c = NULL;
    Prop		*prop = NULL;
    PropValue		*val;
    FILE		*f;
    int			state, i;
    int			version_number;

    f = fopen(session_save_file, "r");
    if(!f) {
	if (verbose)
	    printf("No session save file.\n");
	*sm_id = NULL;
	return 0;
    }
    if (verbose)
	printf("Reading session save file...\n");

    buf = NULL;
    buflen = 0;

    /* Read version # */
    getnextline(&buf, &buflen, f);
    if((p = strchr(buf, '\n'))) *p = '\0';
    version_number = atoi (buf);
    if (version_number > SAVEFILE_VERSION)
    {
	if (verbose)
	    printf("Unsupported version number of session save file.\n");
	*sm_id = NULL;
	if (buf)
	    free (buf);
	return 0;
    }

    /* Read SM's id */
    getnextline(&buf, &buflen, f);
    if((p = strchr(buf, '\n'))) *p = '\0';
    *sm_id = XtNewString(buf);

    /* Read number of clients running in the last session */
    if (version_number >= 2)
    {
	getnextline(&buf, &buflen, f);
	if((p = strchr(buf, '\n'))) *p = '\0';
	num_clients_in_last_session = atoi (buf);
    }

    state = 0;
    while(getnextline(&buf, &buflen, f)) {
	if((p = strchr(buf, '\n'))) *p = '\0';
	for(p = buf; *p && isspace(*p); p++) /* LOOP */;
	if(*p == '#') continue;

	if(!*p)
	{
	    if (version_number >= 3 &&
		ListCount (PendingList) == num_clients_in_last_session)
	    {
		state = 5;
		break;
	    }
	    else
	    {
		state = 0;
		continue;
	    }
	}

	if(!isspace(buf[0])) {
	    switch(state) {
		case 0:
		    c = (PendingClient *)XtMalloc(sizeof *c);
		    if(!c) nomem();

		    c->clientId = XtNewString(p);
		    c->clientHostname = NULL;  /* set in next state */

		    c->props = ListInit();
		    if(!c->props) nomem();

		    if(!ListAddLast(PendingList, (char *)c)) nomem();

		    state = 1;
		    break;

		case 1:
		    c->clientHostname = XtNewString(p);
                    state = 2;
                    break;

		case 2:
		case 4:
		    prop = (Prop *)XtMalloc(sizeof *prop);
		    if(!prop) nomem();

		    prop->name = XtNewString(p);
		    prop->values = ListInit();
		    if(!prop->values) nomem();

		    prop->type = NULL;

		    if(!ListAddLast(c->props, (char *)prop)) nomem();

		    state = 3;
		    break;

		case 3:
		    prop->type = XtNewString(p);
		    state = 4;
		    break;

		default:
		    fprintf(stderr, "state %d\n", state);
		    fprintf(stderr,
			    "Corrupt save file line ignored:\n%s\n", buf);
		    continue;
	    }
	} else {
	    if (state != 4) {
		fprintf(stderr, "Corrupt save file line ignored:\n%s\n", buf);
		continue;
	    }
	    val = (PropValue *)XtMalloc(sizeof *val);
	    if(!val) nomem();

	    if (strcmp (prop->type, SmCARD8) == 0)
	    {
		val->length = 1;
		val->value = (XtPointer) XtMalloc (1);
		*((char *)(val->value)) = atoi (p);
	    }
	    else
	    {
		val->length = strlen(p);
		val->value = XtNewString(p);
	    }

	    if(!ListAddLast(prop->values, (char *)val)) nomem(); 
	}
    }

    /* Read commands for non-session aware clients */

    if (state == 5)
    {
	String strbuf;
	int bufsize = 0;

	getnextline(&buf, &buflen, f);
	if((p = strchr(buf, '\n'))) *p = '\0';
	non_session_aware_count = atoi (buf);

	if (non_session_aware_count > 0)
	{
	    non_session_aware_clients = (char **) malloc (
	        non_session_aware_count * sizeof (char *));

	    for (i = 0; i < non_session_aware_count; i++)
	    {
		getnextline(&buf, &buflen, f);
		if((p = strchr(buf, '\n'))) *p = '\0';
		non_session_aware_clients[i] = (char *) malloc (
		    strlen (buf) + 2);
		strcpy (non_session_aware_clients[i], buf);
		bufsize += (strlen (buf) + 1);
	    }

	    strbuf = (String) malloc (bufsize + 1);
	    strbuf[0] = '\0';

	    for (i = 0; i < non_session_aware_count; i++)
	    {
		strcat (strbuf, non_session_aware_clients[i]);
		strcat (strbuf, "\n");
	    }

	    XtVaSetValues (manualRestartCommands,
	        XtNstring, strbuf,
	        NULL);

	    free ((char *) strbuf);
	}
    }

    fclose(f);

    if (buf)
	free (buf);

    return 1;
}



static void
SaveClient(FILE *f, ClientRec *client)
{
    List *pl;

    fprintf (f, "%s\n", client->clientId);
    fprintf (f, "%s\n", client->clientHostname);

    for (pl = ListFirst (client->props); pl; pl = ListNext (pl))
    {
	Prop *pprop = (Prop *) pl->thing;
	List *pj, *vl;
	PropValue *pval;

	fprintf (f, "%s\n", pprop->name);
	fprintf (f, "%s\n", pprop->type);

	if (strcmp (pprop->type, SmCARD8) == 0)
	{
	    char *card8;
	    int value;

	    vl = ListFirst (pprop->values);
	    pval = (PropValue *) vl->thing;

	    card8 = pval->value;
	    value = *card8;
	    fprintf(f, "\t%d\n", value);
	}
	else
	{
	    for (pj = ListFirst (pprop->values); pj; pj = ListNext (pj))
	    {
		pval = (PropValue *) pj->thing;
		fprintf (f, "\t%s\n", (char *)pval->value);
	    }
	}
    }

    fprintf (f, "\n");
}



void
WriteSave(char *sm_id)
{
    ClientRec *client;
    FILE *f;
    List *cl;
    String commands;
    char *p, *c;
    int count;

    f = fopen (session_save_file, "w");

    if (!f)
    {
	char msg[256];

	sprintf (msg, "%s: Error creating session save file %s", 
	    Argv[0], session_save_file);
	add_log_text (msg);
	perror (msg);
    }
    else
    {
	fprintf (f, "%d\n", SAVEFILE_VERSION);
	fprintf (f, "%s\n", sm_id);

	count = 0;
	for (cl = ListFirst (RunningList); cl; cl = ListNext (cl))
	{
	    client = (ClientRec *) cl->thing;

	    if (client->restartHint != SmRestartNever)
		count++;
	}
	count += ListCount (RestartAnywayList);

	fprintf (f, "%d\n", count);
	if (count == 0)
	    fprintf (f, "\n");

	for (cl = ListFirst (RunningList); cl; cl = ListNext (cl))
	{
	    client = (ClientRec *) cl->thing;

	    if (client->restartHint == SmRestartNever)
		continue;

	    SaveClient (f, client);
	}

	for (cl = ListFirst (RestartAnywayList); cl; cl = ListNext (cl))
	{
	    client = (ClientRec *) cl->thing;

	    SaveClient (f, client);
	}


	/* Save the non-session aware clients */

	XtVaGetValues (manualRestartCommands,
	    XtNstring, &commands,
	    NULL);

	p = c = commands;
	count = 0;

	while (*p)
	{
	    if (*p == '\n')
	    {
		if (p != c)
		    count++;
		c = p + 1;
	    }
	    p++;
	}
	if (p != c)
	    count++;

	fprintf (f, "%d\n", count);

	p = c = commands;

	while (*p)
	{
	    if (*p == '\n')
	    {
		if (p != c)
		{
		    *p = '\0';
		    fprintf (f, "%s\n", c);
		    *p = '\n';
		}
		c = p + 1;
	    }
	    p++;
	}

	if (p != c)
	    fprintf (f, "%s\n", c);

	fclose (f);
    }
}



Status
DeleteSession(char *session_name)
{
    char	*buf;
    int		buflen;
    char	*p, *dir;
    FILE	*f;
    int		state;
    int		foundDiscard;
    char	filename[256];
    int		version_number;

    dir = (char *) getenv ("SM_SAVE_DIR");
    if (!dir)
    {
	dir = (char *) getenv ("HOME");
	if (!dir)
	    dir = ".";
    }

    sprintf (filename, "%s/.XSM-%s", dir, session_name);

    f = fopen(filename, "r");
    if(!f) {
	return (0);
    }

    buf = NULL;
    buflen = 0;

    /* Read version # */
    getnextline(&buf, &buflen, f);
    if((p = strchr(buf, '\n'))) *p = '\0';
    version_number = atoi (buf);
    if (version_number > SAVEFILE_VERSION)
    {
	if (verbose)
	    printf("Can't delete session save file - incompatible version.\n");
	if (buf)
	    free (buf);
	return (0);
    }

    /* Skip SM's id */
    getnextline(&buf, &buflen, f);

    /* Skip number of clients running in the last session */
    if (version_number >= 2)
	getnextline(&buf, &buflen, f);

    state = 0;
    foundDiscard = 0;
    while(getnextline(&buf, &buflen, f)) {
	if((p = strchr(buf, '\n'))) *p = '\0';
	for(p = buf; *p && isspace(*p); p++) /* LOOP */;
	if(*p == '#') continue;

	if(!*p) {
	    state = 0;
	    foundDiscard = 0;
	    continue;
	}

	if(!isspace(buf[0])) {
	    switch(state) {
		case 0:
		    state = 1;
		    break;

		case 1:
                    state = 2;
                    break;

		case 2:
		case 4:
		    if (strcmp (p, SmDiscardCommand) == 0)
			foundDiscard = 1;
		    state = 3;
		    break;

		case 3:
		    state = 4;
		    break;

		default:
		    continue;
	    }
	} else {
	    if (state != 4) {
		continue;
	    }
	    if (foundDiscard)
	    {
		execute_system_command (p);	/* Discard Command */
		foundDiscard = 0;
	    }
	}
    }

    fclose(f);

    if (buf)
	free (buf);

    return ((unlink (filename) == -1) ? 0 : 1);
}



Bool
getnextline(char **pbuf, int *plen, FILE *f)
{
	int c;
	int i;

	i = 0;
	while(1) {
	    if(i+2 > *plen) {
		if(*plen) *plen *= 2;
		else *plen = BUFSIZ;
		if(*pbuf) *pbuf = (char *) realloc(*pbuf, *plen + 1);
		else *pbuf = (char *) malloc(*plen + 1);
	    }
	    c = getc(f);
	    if(c == EOF) break;
	    (*pbuf)[i++] = c;
	    if(c == '\n') break;
	}
	(*pbuf)[i] = '\0';
	return i;
}
