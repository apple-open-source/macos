/* $XConsortium: search.c,v 1.21 94/04/17 20:43:58 rws Exp $ */
/*

Copyright (c) 1987, 1988  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/
/* $XFree86: xc/programs/xman/search.c,v 1.5 2001/01/27 17:24:27 herrb Exp $ */


#include "globals.h"
#include "vendor.h"

/* Map <CR> and control-M to goto begining of file. */

#define SEARCHARGS 10

FILE * DoManualSearch(ManpageGlobals *man_globals, char * string);
static int BEntrySearch(char * string, char ** first, int number);

/*	Function Name: MakeSearchWidget
 *	Description: This Function Creates the Search Widget.
 *	Arguments: man_globals - the pseudo globas for this manpage.
 *                 w - the widgets parent
 *	Returns: the search widget.
 */

void
MakeSearchWidget(ManpageGlobals * man_globals, Widget parent)
{
  Widget dialog, command, text, cancel;
  Arg arglist[2];
  Cardinal num_args = 0;

  XtSetArg(arglist[0], XtNtransientFor, parent); 
  man_globals->search_widget = XtCreatePopupShell(SEARCHNAME, 
						  transientShellWidgetClass, 
						  parent,
						  arglist, 1);

  if (resources.clear_search_string) {
    XtSetArg(arglist[0], XtNvalue, ""); num_args++;  
  }

  dialog = XtCreateManagedWidget(DIALOG, dialogWidgetClass,
				 man_globals->search_widget, 
				 arglist, num_args);

  if ( (text = XtNameToWidget(dialog, "value")) == (Widget) NULL)
    PopupWarning(NULL, "Could not find text widget in MakeSearchWidget.");
  else
    XtSetKeyboardFocus(dialog, text);

  XawDialogAddButton(dialog, MANUALSEARCH, NULL, NULL);
  XawDialogAddButton(dialog, APROPOSSEARCH, NULL, NULL);
  XawDialogAddButton(dialog, CANCEL, NULL, NULL);

/*
 * This is a bit gross, but it get the cancel button underneath the 
 * others, and forms them up to the right size..
 */

  if ( ((command = XtNameToWidget(dialog, MANUALSEARCH)) == (Widget) NULL) ||
       ((cancel = XtNameToWidget(dialog, CANCEL)) == (Widget) NULL) )
    PopupWarning(NULL,
		 "Could not find manual search widget in MakeSearchWidget.");
  else {
    static char * half_size[] = {
      MANUALSEARCH, APROPOSSEARCH, NULL
    };
    static char * full_size[] = {
      "label", "value", CANCEL, NULL
    };

    num_args = 0;
    XtSetArg(arglist[num_args], XtNfromVert, command); num_args++;
    XtSetArg(arglist[num_args], XtNfromHoriz, NULL); num_args++;
    XtSetValues(cancel, arglist, num_args);
    FormUpWidgets(dialog, full_size, half_size);
  }

}

/*      Function Name: SearchString
 *      Description: Returns the search string.
 *      Arguments: man_globals - the globals.
 *      Returns: the search string.
 */

static char *
SearchString(
ManpageGlobals * man_globals)
{
  Widget dialog;

  dialog = XtNameToWidget(man_globals->search_widget, DIALOG);
  if (dialog != NULL) 
    return(XawDialogGetValueString(dialog));

  PopupWarning(man_globals,
	      "Could not get the search string, no search will be preformed.");
  return(NULL);
}

  
/*	Function Name: DoSearch
 *	Description: This function performs a search for a man page or apropos
 *                   search upon search string.
 *	Arguments: man_globals - the pseudo globas for this manpage.
 *                 type - the type of search.
 *	Returns: none.
 */

#define LOOKLINES 6

/* 
 * Manual searches look through the list of manual pages for the right one
 * with a binary search.
 *
 * Apropos searches still exec man -k.
 *
 * If nothing is found then I send a warning message to the user, and do
 * nothing.
 */

FILE *
DoSearch(ManpageGlobals * man_globals, int type)
{
  char cmdbuf[BUFSIZ],*mantmp, *manpath;
  char tmp[BUFSIZ],path[BUFSIZ];
  char string_buf[BUFSIZ], cmp_str[BUFSIZ], error_buf[BUFSIZ];
  char * search_string = SearchString(man_globals);
  FILE * file;
#ifdef HAS_MKSTEMP
  int fd;
#endif
  int count;
  Boolean flag;

  if (search_string == NULL) return(NULL);

  /* If the string is empty or starts with a space then do not search */

  if ( streq(search_string,"") ) {
    PopupWarning(man_globals, "Search string is empty.");
    return(NULL);
  }

  if (search_string[0] == ' ') {
    PopupWarning(man_globals, "First character cannot be a space.");
    return(NULL);
  }

  if (type == APROPOS) {
    char label[BUFSIZ];

    strcpy(tmp, MANTEMP);		/* get a temp file. */
#ifdef HAS_MKSTEMP
    fd = mkstemp(tmp);
    if (fd < 0) {
      PopupWarning(man_globals, "Cant create temp file");
      return NULL;
    }
#else
    (void)mktemp(tmp);
#endif
    mantmp = tmp;

    manpath=getenv("MANPATH");
    if (manpath == NULL || streq(manpath,"") ) {
#ifdef MANCONF
      if (!ReadManConfig(path))
#endif
      {
	strcpy(path,SYSMANPATH);
#ifdef LOCALMANPATH
	strcat(path,":");
	strcat(path,LOCALMANPATH);
#endif
      }
    } else {
      strcpy(path,manpath);
    }

    sprintf(label,"Results of apropos search on: %s", search_string);

#ifdef NO_MANPATH_SUPPORT	/* not quite correct, but the best I can do. */
    sprintf(cmdbuf, APROPOS_FORMAT, search_string, mantmp);
#else
    sprintf(cmdbuf, APROPOS_FORMAT, path, search_string, mantmp);
#endif

    if(system(cmdbuf) != 0) {	/* execute search. */
      sprintf(error_buf,"Something went wrong trying to run %s\n",cmdbuf);
      PopupWarning(man_globals, error_buf);
    }

#ifdef HAS_MKSTEMP
    if ((file = fdopen(fd, "r")) == NULL) 
#else
    if((file = fopen(mantmp,"r")) == NULL)
#endif
      PrintError("lost temp file? out of temp space?");

/* 
 * Since we keep the FD open we can unlink the file safely, this
 * will keep extra files out of /tmp. 
 */

    unlink(mantmp);

    sprintf(string_buf,"%s: nothing appropriate", search_string);

    /*
     * Check first LOOKLINES lines for "nothing appropriate".
     */
  
    count = 0;
    flag = FALSE;
    while ( (fgets(cmp_str, BUFSIZ, file) != NULL) && (count < LOOKLINES) ) {
      if ( cmp_str[strlen(cmp_str) - 1] == '\n') /* strip off the '\n' */
	  cmp_str[strlen(cmp_str) - 1] = '\0';

      if (streq(cmp_str, string_buf)) {
	flag = TRUE;
	break;
      }
      count++;
    }

    /*
     * If the file is less than this number of lines then assume that there is
     * nothing apropriate found. This does not confuse the apropos filter.
     */

    if (flag) {
      fclose(file);
      file = NULL;
      ChangeLabel(man_globals->label,string_buf);
      return(NULL);
    }
  
    strcpy(man_globals->manpage_title,label);
    ChangeLabel(man_globals->label,label);
    fseek(file, 0L, 0);		/* reset file to point at top. */
  }
  else {			/* MANUAL SEACH */
    file = DoManualSearch(man_globals, search_string);
    if (file == NULL) {
      sprintf(string_buf,"No manual entry for %s.", search_string);
      ChangeLabel(man_globals->label, string_buf);
      if (man_globals->label == NULL)
	PopupWarning(man_globals, string_buf);
      return(NULL);
    }
  }

  if (resources.clear_search_string) {
    Arg arglist[1];
    Widget dialog;

    dialog = XtNameToWidget(man_globals->search_widget, DIALOG);
    if (dialog == NULL) 
      PopupWarning(man_globals, "Could not clear the search string.");

    XtSetArg(arglist[0], XtNvalue, "");
    XtSetValues(dialog, arglist, (Cardinal) 1);
  }

  return(file);
}

/*	Function Name: DoManualSearch
 *	Description: performs a manual search.
 *	Arguments: man_globals - the manual page specific globals.
 *	Returns: the filename of the man page.
 */

#define NO_ENTRY -100

FILE * 
DoManualSearch(ManpageGlobals *man_globals, char * string)
{
  int e_num = NO_ENTRY;
  int i;

/* search current section first. */
  
  i = man_globals->current_directory;
  e_num = BEntrySearch(string, manual[i].entries, manual[i].nentries);

/* search other sections. */

  if (e_num == NO_ENTRY) {
    i = 0;			/* At the exit of the loop i needs to
				   be the one we used. */
    while ( TRUE ) {
      if (i == man_globals->current_directory)
	if (++i >= sections) return(NULL);
      e_num = BEntrySearch(string, manual[i].entries, manual[i].nentries);
      if (e_num != NO_ENTRY) break;
      if (++i >= sections) return(NULL);
    }

/*
 * Manual page found in some other section, unhighlight the current one.
 */
    if ( man_globals->manpagewidgets.box != NULL)
      XawListUnhighlight(
	man_globals->manpagewidgets.box[man_globals->current_directory]);
  }
  else {
    /* 
     * Highlight the element we are searching for if it is in the directory
     * listing currently being shown.
     */
    if ( man_globals->manpagewidgets.box != NULL)
      XawListHighlight(man_globals->manpagewidgets.box[i], e_num);
  }
  return(FindManualFile(man_globals, i, e_num));
}

/*	Function Name: BEntrySearch
 *	Description: binary search through entries.
 *	Arguments: string - the string to match.
 *                 first - the first entry in the list.
 *                 number - the number of entries.
 *	Returns: a pointer to the entry found.
 */

static int
BEntrySearch(
char * string,
char ** first,
int number)
{
  int check, cmp, len_cmp, global_number;
  char *head, *tail;
  
  global_number = 0;
  while (TRUE) {

    if (number == 0) {
      return(NO_ENTRY);		/* didn't find it. */
    }

    check = number/2;

    head = rindex(first[ global_number + check ], '/');
    if (head == NULL) 
      PrintError("index failure in BEntrySearch");
    head++;

    tail = rindex(head, '.');
    if (tail == NULL) 
      /* not an error, some systems (e.g. sgi) have only a .z suffix */
      tail = head + strlen(head);

    cmp = strncmp(string, head, (tail - head));
    len_cmp = strlen(string) - (int) (tail - head);

    if ( cmp == 0 && len_cmp == 0) {
      return(global_number + check);
    }
    else if ( cmp < 0 || ((cmp == 0) && (len_cmp < 0)) ) 
      number = check;
    else /* cmp > 0 || ((cmp == 0) && (len_cmp > 0)) */ {
      global_number += (check + 1);
      number -= ( check + 1 );
    }
  }
}
