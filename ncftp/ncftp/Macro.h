/* Macro.h */

#ifndef _macro_h_
#define _macro_h_

typedef struct MacroNode *MacroNodePtr;
typedef struct MacroNode {
	char			*name;
	LineList		macroData;
	MacroNodePtr 	prev, next;
} MacroNode;

#define kMacroFileName			"macros"
#define kMacroStartToken		"macro "
#define kMacroEndToken			"end"

/* Prototypes. */
void DisposeMacro(MacroNodePtr macro);
MacroNodePtr CollectMacro(char *macroName, char *endKeyword, FILE *fp);
void AttachMacro(MacroNodePtr macro);
int ExecuteMacro(MacroNodePtr macro, int argc, char **argv);
MacroNodePtr FindMacro(char *macroName);
int ShowMacro(MacroNodePtr macro);
void DumpMacro(char *macName);
void ReadMacroFile(void);
int RunPrefixedMacro(char *pfx, char *sfx);
char *MacroGetLine(char *macline, FILE *fp);

#endif	/* _macro_h_ */

/* eof macro.h */
