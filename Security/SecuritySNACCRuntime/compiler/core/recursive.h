/*
 * compiler/core/recursive.h - find and mark the recursive types in a module
 *
 * MS 92
 *
 * Copyright (C) 1992 Michael Sample and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/compiler/core/Attic/recursive.h,v 1.1 2001/06/20 21:27:59 dmitch Exp $
 * $Log: recursive.h,v $
 * Revision 1.1  2001/06/20 21:27:59  dmitch
 * Adding missing snacc compiler files.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:52  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.2  1994/10/08 03:48:57  rj
 * since i was still irritated by cpp standing for c++ and not the C preprocessor, i renamed them to cxx (which is one known suffix for C++ source files). since the standard #define is __cplusplus, cplusplus would have been the more obvious choice, but it is a little too long.
 *
 * Revision 1.1  1994/08/28  09:49:36  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

void MarkRecursiveTypes PROTO ((Module *m));
