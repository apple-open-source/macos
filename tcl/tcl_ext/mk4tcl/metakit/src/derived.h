// derived.h --
// $Id: derived.h,v 1.6 2003/11/23 01:42:51 wcvs Exp $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit/

/** @file
 * Encapsulation of derived view classes
 */

#ifndef __DERIVED_H__
#define __DERIVED_H__

/////////////////////////////////////////////////////////////////////////////
// Declarations in this file

  class c4_Cursor;          // not defined here
  class c4_Sequence;          // not defined here

  extern c4_Sequence* f4_CreateFilter(c4_Sequence&, c4_Cursor, c4_Cursor);
  extern c4_Sequence* f4_CreateSort(c4_Sequence&, c4_Sequence* =0);
  extern c4_Sequence* f4_CreateProject(c4_Sequence&, c4_Sequence&,
                          bool, c4_Sequence* =0);

/////////////////////////////////////////////////////////////////////////////

#endif
