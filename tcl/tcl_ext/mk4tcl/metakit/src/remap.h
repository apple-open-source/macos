// remap.h --
// $Id: remap.h 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html

/** @file
 * Encapsulation of the (re)mapping viewers
 */

#ifndef __REMAP_H__
#define __REMAP_H__

/////////////////////////////////////////////////////////////////////////////
// Declarations in this file

class c4_Sequence; // not defined here

extern c4_CustomViewer *f4_CreateReadOnly(c4_Sequence &);
extern c4_CustomViewer *f4_CreateHash(c4_Sequence &, int, c4_Sequence * = 0);
extern c4_CustomViewer *f4_CreateBlocked(c4_Sequence &);
extern c4_CustomViewer *f4_CreateOrdered(c4_Sequence &, int);
extern c4_CustomViewer *f4_CreateIndexed(c4_Sequence &, c4_Sequence &, const
  c4_View &, bool = false);

/////////////////////////////////////////////////////////////////////////////

#endif
