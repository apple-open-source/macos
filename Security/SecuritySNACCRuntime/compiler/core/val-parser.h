/*
 * compiler/core/val_parser.h
 *
 * Value  *ParseValue (Type *t, char *valueNotation, int vnlen);
 *         given a string with txt ASN.1 value notation, the length of
 *         the string and the ASN.1 type the value notion defines a value
 *         for, return a Value that contains the internal version
 *
 *
 * Copyright (C) 1992 Michael Sample and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/compiler/core/val-parser.h,v 1.1 2001/06/20 21:27:59 dmitch Exp $
 * $Log: val-parser.h,v $
 * Revision 1.1  2001/06/20 21:27:59  dmitch
 * Adding missing snacc compiler files.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:53  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 19:41:48  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/08/31  21:47:26  rj
 * adjust the function declaration to the function definition. this went undetected because the .c file didn't include its .h file.
 *
 * Revision 1.1  1994/08/28  09:49:45  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */


int	ParseValues PROTO ((ModuleList *mods, Module *m));
