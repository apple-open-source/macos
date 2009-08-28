// field.cpp --
// $Id: field.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

/** @file
 * Implementation of the field structure tree
 */

#include "header.h"
#include "field.h"

#include <stdlib.h>   // strtol

#if !q4_INLINE
#include "field.inl"
#endif 

/////////////////////////////////////////////////////////////////////////////
// Implemented in this file

class c4_Field;

/////////////////////////////////////////////////////////////////////////////
// c4_Field

c4_Field::c4_Field(const char * &description_, c4_Field *parent_): _type(0) {
    _indirect = this;

    size_t n = strcspn(description_, ",[]");
    const char *p = strchr(description_, ':');

    if (p != 0 && p < description_ + n) {
        _name = c4_String(description_, p - description_);
        _type = p[1] &~0x20; // force to upper case
    } else {
        _name = c4_String(description_, n);
        _type = 'S';
    }

    description_ += n;

    if (*description_ == '[') {
        ++description_;
        _type = 'V';

        if (*description_ == '^') {
            ++description_;
            _indirect = parent_;
            d4_assert(*description_ == ']');
        }

        if (*description_ == ']')
          ++description_;
        else
        do {
            // 2004-01-20 ignore duplicate property names
            // (since there is no good way to report errors at this point)
            c4_Field *sf = d4_new c4_Field(description_, this);
            for (int i = 0; i < NumSubFields(); ++i)
            if (SubField(i).Name().CompareNoCase(sf->Name()) == 0) {
                delete sf;
                sf = 0;
                break;
            }
            if (sf != 0)
              _subFields.Add(sf);
        } while (*description_++ == ',');
    }
}

c4_Field::~c4_Field() {
  if (_indirect == this) {
    //better? for (int i = NumSubFields(); --i >= 0 ;)
    for (int i = 0; i < NumSubFields(); ++i) {
      c4_Field *sf = &SubField(i);
      if (sf != this)
      // careful with recursive subfields
        delete sf;
    }
  }
}

c4_String c4_Field::Description(bool anonymous_)const {
  c4_String s = anonymous_ ? "?" : (const char*)Name();

  if (Type() == 'V')
    s += "[" + DescribeSubFields(anonymous_) + "]";
  else {
    s += ":";
    s += (c4_String)Type();
  }

  return s;
}

c4_String c4_Field::DescribeSubFields(bool)const {
  d4_assert(Type() == 'V');

  if (_indirect != this)
    return "^";

  c4_String s;
  char c = 0;

  for (int i = 0; i < NumSubFields(); ++i) {
    if (c != 0)
      s += (c4_String)c;
    s += SubField(i).Description();
    c = ',';
  }

  return s;
}

/////////////////////////////////////////////////////////////////////////////
