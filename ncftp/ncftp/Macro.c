/* Macro.c
 *
 * Purpose:  Let the user create and execute macros.
 */

#include "Sys.h"

#include <ctype.h>

#include "Util.h"
#include "Macro.h"
#include "Cmds.h"
#include "Cmdline.h"
#include "MakeArgv.h"

/* First and last nodes in the macro list. */
MacroNodePtr gFirstMacro = NULL;
MacroNodePtr gLastMacro = NULL;

/* Number of macros in the macro list. */
int gNumGlobalMacros = 0;

/* We need this routine in case the user wants to redefine an existing
 * macro.
 */
void DisposeMacro(MacroNodePtr macro)
{
	MacroNodePtr prevMac, nextMac;

	if (macro == NULL)
		return;

	/* Dispose of the linked-list of data lines first. */
	DisposeLineListContents(&macro->macroData);

	/* Repair the macro list. */
	prevMac = macro->prev;
	nextMac = macro->next;
	
	if (prevMac == NULL)
		gFirstMacro = nextMac;
	if (nextMac != NULL)
		nextMac->prev = prevMac;
	
	if (nextMac == NULL)
		gLastMacro = prevMac;
	if (prevMac != NULL)
		prevMac->next = nextMac;
	
	free(macro->name);
	free(macro);
	--gNumGlobalMacros;
}	/* DisposeMacro */




char *MacroGetLine(char *macline, FILE *fp)
{
	char *cp;
	string str;
	
	*macline = '\0';
	while ((fgets(str, ((int) sizeof(str)) - 1, fp)) != NULL) {
		cp = strrchr(str, '#');
		if (cp != NULL)
			*cp = '\0';
		cp = str + strlen(str) - 1;
		/* Strip trailing space. */
		while (cp >= str && isspace(*cp))
			*cp-- = '\0';

		/* Strip leading space. */
		for (cp = str; isspace(*cp); ) ++cp;

		if (*cp != '\0') {
			Strncpy(macline, cp, sizeof(string));
			return (macline);
		}
	}
	return (NULL);
}	/* MacroGetLine */





/* Reads in the macro line by line, until we reach a special keyword
 * that designates the end.
 */
MacroNodePtr CollectMacro(char *macroName, char *endKeyword, FILE *fp)
{
	string macline;
	MacroNodePtr macro;
	char *name;

	/* If we already have a macro by that name, we'll dispose of this
	 * macro and replace it.
	 */
	DisposeMacro(FindMacro(macroName));

	if ((name = StrDup(macroName)) == NULL)
		return NULL;

	macro = (MacroNodePtr) calloc((size_t)1, sizeof(MacroNode));
	if (macro == NULL)
		return NULL;

	macro->name = name;
	InitLineList(&macro->macroData);

	while (1) {
		if ((MacroGetLine(macline, fp)) == NULL)
			return NULL;	/* incomplete macro. */
		
		/* See if we have found the terminating keyword. */
		if (STREQ(endKeyword, macline))
			break;

		AddLine(&macro->macroData, macline);
	}
	
	return (macro);
}	/* CollectMacro */



/* Adds a macro to the macro list.  */
void AttachMacro(MacroNodePtr macro)
{
	if (macro != NULL) {
		macro->next = NULL;
		if (gFirstMacro == NULL) {
			macro->prev = NULL;
			gFirstMacro = gLastMacro = macro;
		} else {
			gLastMacro->next = macro;
			macro->prev = gLastMacro;
			gLastMacro = macro;
		}
		++gNumGlobalMacros;
	}
}	/* AttachMacro */




void ReadMacroFile(void)
{
	FILE *fp;
	string str;
	longstring fName;
	char *cp;
	MacroNodePtr macp;
	size_t len;

	(void) OurDirectoryPath(fName, sizeof(fName), kMacroFileName);
	fp = fopen(fName, "r");
	if (fp == NULL)	/* It's okay not to have one.  Most folks won't. */
		return;
	
	len = strlen(kMacroStartToken);
	while (1) {
		if ((MacroGetLine(str, fp)) == NULL)
			break;
		
		/* See if we have found a starting keyword. */
		if (STRNEQ(kMacroStartToken, str, len)) {
			cp = str + 6;
			if (*cp != '\0') {
				macp = CollectMacro(cp, kMacroEndToken, fp);
				AttachMacro(macp);
			}
		}
	}
	
	DebugMsg("Read %d macros from %s.\n", gNumGlobalMacros, fName);
	(void) fclose(fp);
}	/* ReadMacroFile */




/* Runs each line in a macro. */
int ExecuteMacro(MacroNodePtr macro, int argc, char **argv)
{
	LinePtr mlp;
	int err, i;
	char *line2;

	if (macro == NULL)
		return -1;

	line2 = (char *) malloc((size_t) 1024);
	if (line2 == NULL) {
		Error(kDontPerror,
			"Not enough memory to run macro \"%s.\"\n", macro->name);
		return -1;
	}

	DebugMsg("Macro: %s\n", macro->name);
	for (i=1; i<argc; i++)
		DebugMsg("    %s\n", argv[i]);

	err = kNoErr;
	for (mlp = macro->macroData.first; mlp != NULL; mlp = mlp->next) {
		/* Insert arguments passed to the macro. */
		(void) Strncpy(line2, mlp->line, (size_t)1024);
		ExpandDollarVariables(line2, (size_t)1024, argc, argv);

		DebugMsg("%s: %s\n", macro->name, line2);
		
		if ((err = ExecCommandLine(line2)) != 0)
			break;
	}
	free(line2);
	return err;
}	/* ExecuteMacro */




int ShowMacro(MacroNodePtr macro)
{
	LinePtr mlp;

	if (macro == NULL)
		return -1;

	PrintF("macro %s\n", macro->name);
	for (mlp = macro->macroData.first; mlp != NULL; mlp = mlp->next) {
		PrintF("    %s\n", mlp->line);
	}
	PrintF("end\n\n"); 
	return 0;
}	/* ShowMacro */




void DumpMacro(char *macName)
{
	MacroNodePtr mnp;

	if (macName == NULL) {
		/* Dump 'em all. */
		for (mnp = gFirstMacro; mnp != NULL; mnp = mnp->next)
			(void) ShowMacro(mnp);
	} else {
		mnp = FindMacro(macName);
		(void) ShowMacro(mnp);
	}
}	/* DumpMacro */




/* This routine can be used before the CAName list has been constructed
 * (see cmdtab.c).  We use this to make sure we don't have any duplicate
 * macro names when we try to add a new macro.  You can also use this
 * if you only want to look for a macro, but don't want to bother with
 * the possibility of GetCommandOrMacro() returning a command, or an
 * ambiguous name error.
 */
MacroNodePtr FindMacro(char *macroName)
{
	MacroNodePtr mnp;

	for (mnp = gFirstMacro; mnp != NULL; mnp = mnp->next) {
		if (STREQ(mnp->name, macroName))
			break;
	}
	return mnp;
}	/* FindMacro */




int RunPrefixedMacro(char *pfx, char *sfx)
{
	string macName;
	MacroNodePtr mnp;
	
	STRNCPY(macName, ".");
	STRNCAT(macName, pfx);
	STRNCAT(macName, sfx);
	mnp = FindMacro(macName);
	return ExecuteMacro(mnp, 0, NULL);
}	/* RunPrefixedMacro */

/* eof Macro.c */
