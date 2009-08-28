// store.inl --
// $Id: store.inl 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

/** @file
 * Inlined members of the storage management classes
 */

/////////////////////////////////////////////////////////////////////////////
// c4_Notifier

d4_inline c4_Notifier::c4_Notifier (c4_Sequence* origin_)
  : _origin (origin_), _chain (0), _next (0),
    _type (kNone), _index (0), _propId (0), _count (0), 
    _cursor (0), _bytes (0)
{
  d4_assert(_origin != 0);
}

/////////////////////////////////////////////////////////////////////////////
