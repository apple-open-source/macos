// field.h --
// $Id: field.h 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html

/** @file
 * Core class to represent fields
 */

#ifndef __FIELD_H__
#define __FIELD_H__

#ifndef __K4CONF_H__
#error Please include "k4conf.h" before this header file
#endif 

/////////////////////////////////////////////////////////////////////////////

class c4_Field {
    c4_PtrArray _subFields;
    c4_String _name;
    char _type;
    c4_Field *_indirect;

  public:
    /* Construction / destruction */
    c4_Field(const char * &, c4_Field * = 0);
    //: Constructs a new field.
    ~c4_Field();

    /* Repeating and compound fields */
    int NumSubFields()const;
    //: Returns the number of subfields.
    c4_Field &SubField(int)const;
    //: Returns the description of each subfield.
    bool IsRepeating()const;
    //: Returns true if this field contains subtables.

    /* Field name and description */
    const c4_String &Name()const;
    //: Returns name of this field.
    char Type()const;
    //: Returns the type description of this field, if any.
    char OrigType()const;
    //: Similar, but report types which were originall 'M' as well.
    c4_String Description(bool anonymous_ = false)const;
    //: Describes the structure, omit names if anonymous.
    c4_String DescribeSubFields(bool anonymous_ = false)const;
    //: Describes just the subfields, omit names if anonymous.

  private:
    c4_Field(const c4_Field &); // not implemented
    void operator = (const c4_Field &); // not implemented
};

/////////////////////////////////////////////////////////////////////////////

#if q4_INLINE
#include "field.inl"
#endif 

/////////////////////////////////////////////////////////////////////////////

#endif
