/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright 1993 NeXT Computer, Inc.
 * All rights reserved.
 */

/*
 * Standalone Interface Generator
 *
 * Input file format: a series of lines of the form:
 *
 *     #<text>
 * or
 *     <function declaration> <arg list> ;
 *
 * e.g.:
 *	void *malloc(int len) len;
 *
 * Lines starting with '#' are passed through unchanged.
 * Lines starting with '/' are ignored.
 *
 *
 */
 
#import <stdio.h>
#import <ctype.h>
#import <string.h>
#import <stdlib.h>
#import <sys/param.h>

#define MAXLINE 65535
typedef enum {NO=0, YES=1} BOOL;
#define strdup(str) ((char *)strcpy(malloc(strlen(str)+1),str))
extern int getopt();

enum {
    none,
    external,
    internal,
    table,
    defs
} which;
char *osuffix[] = {
    "none",
    "_external.h",
    "_internal.h",
    "_table.c",
    "_defs.h"
};

FILE *ofile;
int lineNumber, outputNumber;
char *moduleName;
char *moduleNameCaps;

char *stringToLower(char *string)
{
    char *new = strdup(string);
    char *p = new;
    
    while (*p)
	*p = tolower(*p++);
    return new;
}

char *stringToUpper(char *string)
{
    char *new = strdup(string);
    char *p = new;
    
    while (*p)
	*p = toupper(*p++);
    return new;
}

inline BOOL
isIdentifier(char c)
{
    if (isalnum(c) || c == '_')
	return YES;
    else
	return NO;
}

void
outputLine(char *decl, char *function, char *args, char *arglist)
{
    if (which == table) {
	static int struct_started;
	if (struct_started == 0) {
	    fprintf(ofile, "unsigned long (*%s_functions[])() = {\n",
		    moduleName);
	    struct_started = 1;
	}
	fprintf(ofile, "(unsigned long (*)())_%s,\t\t/* %d */\n", function, outputNumber);
    }
    
    if (which == defs) {
	fprintf(ofile, "#define %s _%s\n",function,function);
    }
    
    if (which == internal) {
	fprintf(ofile, "extern %s _%s(%s);\n", decl, function, args);
    }
    
    if (which == external) {
	fprintf(ofile, "#define %s_%s_FN %d\n",
	    moduleNameCaps, function, outputNumber);
	fprintf(ofile, 
	    "static inline %s %s ( %s ) {\n", decl, function, args);
	fprintf(ofile,
	    "\treturn (%s)(*%s_FN[%d])(%s);\n",
	    decl, moduleNameCaps, outputNumber, arglist);
	fprintf(ofile, "}\n");
    }
    outputNumber++;
}

void
parseLine(char *line)
{
    char *paren, *parenEnd;
    char *ident, *identEnd;
    char *arglist, *arglistEnd;
    char *function;
    
    paren = strchr(line, '(');
    if (paren == NULL)
	goto syntax_error;
    for (identEnd = paren - 1; !isIdentifier(*identEnd); identEnd--)
	continue;
    for (ident = identEnd; isIdentifier(*ident); ident--)
	continue;
    ident++;
    *++identEnd = '\0';
    paren++;
    parenEnd = strchr(paren, ')');
    if (parenEnd == NULL)
	goto syntax_error;
    *parenEnd = '\0';

    arglist = parenEnd + 1;
    while (isspace(*arglist))
	arglist++;
    arglistEnd = strchr(arglist, ';');
    if (arglistEnd == NULL)
	goto syntax_error;
    *arglistEnd = '\0';
    
    function = strdup(ident);
    *ident = '\0';
    outputLine(line, function, paren, arglist);
    free(function);
    return;

syntax_error:
    fprintf(stderr, "Syntax error at line %d\n",lineNumber);
    return;
}

int
getLineThru(FILE *file, char *linebuf, char stop, int len)
{
    char *p = linebuf;
    int c, gotten;

    gotten = 0;
    while (((c = fgetc(file)) != EOF) && len) {
	*p++ = c;
	len--;
	gotten++;
	if (c == '\n') lineNumber++;
	if (c == stop)
	    break;
    }
    *p = '\0';
    return gotten;
}

int
peekf(FILE *file)
{
    int c = fgetc(file);
    ungetc(c, file);
    return c;
}

void
skipWhitespace(FILE *file)
{
    int c;
    
    while ((c = fgetc(file)) != EOF && isspace(c))
	if (c == '\n') ++lineNumber;
    ungetc(c, file);
}

void
parseFile(FILE *file)
{
    char *line, c;
    int len, lineNumber;
    
    line = malloc(MAXLINE+1);
    lineNumber = 1;
    
    skipWhitespace(file);
    
    while (!feof(file)) {
	c = peekf(file);
	if (c == '#' || c == '/') {
	    len = getLineThru(file, line, '\n', MAXLINE);
	    if (c == '#')
		fprintf(ofile, line);
	} else {
	    len = getLineThru(file, line, ';', MAXLINE);
	    parseLine(line);
	}
	skipWhitespace(file);
    }
    free(line);
}

int
main(int argc, char **argv)
{
    extern char *optarg; 
    extern int optind;
    FILE *file;
    int c, errflag = 0;
    char ofilename[MAXPATHLEN];
    char *ifile, *odir = ".";

    while ((c = getopt(argc, argv, "d:n:")) != EOF)
	switch (c) {
	case 'd':
	    odir = optarg;
	    break;
	case 'n':
	    moduleName = optarg;
	    moduleNameCaps = stringToUpper(moduleName);
	    break;
	default:
	    errflag++;
	    break;
	}

    ofile = stdout;
    if ((ifile = argv[optind]) != NULL) {
	file = fopen(argv[optind], "r");
	if (file == NULL) {
	    perror("open");
	    exit(1);
	}
    } else {
	fprintf(stderr,"No input file specified\n");
	exit(2);
    }

    if (moduleName == NULL) {
	char *newName, *dot;
	int len;
	newName = strchr(ifile, '/');
	if (newName == NULL)
	    newName = ifile;
	dot = strchr(newName, '.');
	if (dot == NULL)
	    dot = &newName[strlen(newName)];
	len = dot - newName;
	moduleName = (char *)malloc(len + 1);
	strncpy(moduleName, newName, len);
	moduleName[len] = '\0';
	moduleNameCaps = stringToUpper(moduleName);
    }
    
    for (which = external; which <= defs; which++) {
	rewind(file);
	lineNumber = 1;
	outputNumber = 0;
	sprintf(ofilename, "%s/%s%s", odir, moduleName, osuffix[which]);
	ofile = fopen((const char *)ofilename, "w");
	if (ofile == NULL) {
	    fprintf(stderr,"error opening output file %s\n",ofilename);
	    exit(3);
	}

	if (which == table) {
	    fprintf(ofile, "#define %s_TABLE 1\n", moduleNameCaps);
	    fprintf(ofile, "#import \"%s_internal.h\"\n",moduleName);
	}
    
	if (which == internal) {
	    fprintf(ofile, "#define %s_INTERNAL 1\n", moduleNameCaps);
	}
	
	if (which == defs) {
	    fprintf(ofile, "#define %s_DEFS 1\n", moduleNameCaps);
	}
	
	if (which == external) {
	    fprintf(ofile, "#import \"memory.h\"\n");
	    fprintf(ofile, "#define %s_EXTERNAL 1\n", moduleNameCaps);
	    fprintf(ofile, 
	    "#define %s_FN (*(unsigned long (***)())%s_TABLE_POINTER)\n\n",
		moduleNameCaps, moduleNameCaps);
	}
	parseFile(file);
	
	if (which == table) {
	    fprintf(ofile, "};\n\n");
	}
    }
    return 0;
}
