/* $RCSfile: INTERN.h,v $$Revision: 1.1.1.2 $$Date: 2000/03/31 05:13:03 $
 *
 *    Copyright (c) 1991-1997, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: INTERN.h,v $
 * Revision 1.1.1.2  2000/03/31 05:13:03  wsanchez
 * Import of perl 5.6.0
 *
 */

#undef EXT
#define EXT

#undef INIT
#define INIT(x) = x

#define DOINIT
