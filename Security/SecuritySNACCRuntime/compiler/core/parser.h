/*
 * compiler/core/parser.h
 *
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/compiler/core/parser.h,v 1.1 2001/06/20 21:27:58 dmitch Exp $
 * $Log: parser.h,v $
 * Revision 1.1  2001/06/20 21:27:58  dmitch
 * Adding missing snacc compiler files.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:52  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/24 13:33:19  rj
 * typo fixed: Pasrser -> Parser
 *
 * Revision 1.2  1994/10/08  03:48:52  rj
 * since i was still irritated by cpp standing for c++ and not the C preprocessor, i renamed them to cxx (which is one known suffix for C++ source files). since the standard #define is __cplusplus, cplusplus would have been the more obvious choice, but it is a little too long.
 *
 * Revision 1.1  1994/08/28  09:49:31  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

extern int	smallErrG;   /* can continue processing but don't produce code - see more errs */
extern int	yydebug;     /* set to 1 to enable debugging */

int InitAsn1Parser PROTO ((Module *mod, char *fileName, FILE *fPtr));

int  yyparse();
