// handler.inl --
// $Id: handler.inl 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html

/** @file
 * Inlined members of the handler classes
 */

/////////////////////////////////////////////////////////////////////////////
// c4_Handler

d4_inline c4_Handler::c4_Handler (const c4_Property& prop_)
  : _property (prop_)
{
}

d4_inline c4_Handler::~c4_Handler ()
{
}

d4_inline void c4_Handler::Define(int, const t4_byte**)
{
}

d4_inline void c4_Handler::FlipBytes()
{
}

d4_inline const c4_Property& c4_Handler::Property() const
{
  return _property;
}

d4_inline int c4_Handler::PropId() const
{
  return _property.GetId();
}

d4_inline c4_Column* c4_Handler::GetNthMemoCol(int, bool alloc_)
{
  return 0;
}

d4_inline bool c4_Handler::IsPersistent() const
{
  return false;
}

d4_inline void c4_Handler::Unmapped()
{
}

d4_inline bool c4_Handler::HasSubview(int)
{
  return false;
}

/////////////////////////////////////////////////////////////////////////////
// c4_HandlerSeq

d4_inline int c4_HandlerSeq::NumRows() const
{
  d4_assert(_numRows >= 0);

  return _numRows;
}
  
d4_inline int c4_HandlerSeq::NumHandlers() const
{
  return _handlers.GetSize();
}

d4_inline c4_Handler& c4_HandlerSeq::NthHandler(int index_) const
{
  d4_assert(_handlers.GetAt(index_) != 0);
  
  return *(c4_Handler*) _handlers.GetAt(index_);
}

d4_inline const c4_Sequence* c4_HandlerSeq::HandlerContext(int) const
{
  return this;
}

d4_inline c4_HandlerSeq& c4_HandlerSeq::Parent() const
{
  return *_parent;
}

/////////////////////////////////////////////////////////////////////////////
