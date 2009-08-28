// field.inl --
// $Id: field.inl 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

/** @file
 * Inlined members of the field class
 */

d4_inline bool c4_Field::IsRepeating() const
{
  return _type == 'V';
}

d4_inline int c4_Field::NumSubFields() const
{
  return _indirect->_subFields.GetSize();
}

d4_inline c4_Field& c4_Field::SubField(int index_) const
{
  return *(c4_Field*) _indirect->_subFields.GetAt(index_);
}

d4_inline const c4_String& c4_Field::Name() const
{
  return _name;
}
  
d4_inline char c4_Field::OrigType() const
{
  return _type;
}
  
d4_inline char c4_Field::Type() const
{
  return _type == 'M' ? 'B' : _type;
}
