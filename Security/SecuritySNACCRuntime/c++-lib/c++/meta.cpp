/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


// file: .../c++-lib/src/meta.C
//
// $Header: /cvs/root/Security/SecuritySNACCRuntime/c++-lib/c++/Attic/meta.cpp,v 1.1.1.1 2001/05/18 23:14:06 mb Exp $
// $Log: meta.cpp,v $
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.2  2001/05/05 00:59:17  rmurphy
// Adding darwin license headers
//
// Revision 1.1  2000/06/15 18:44:58  dmitch
// These snacc-generated source files are now checked in to allow cross-platform build.
//
// Revision 1.2  2000/06/08 20:05:36  dmitch
// Mods for X port. These files are actually machine generated and probably don't need to be in CVS....
//
// Revision 1.1.1.1  2000/03/09 01:00:06  rmurphy
// Base Fortissimo Tree
//
// Revision 1.1  1999/02/25 05:21:56  mb
// Added snacc c++ library
//
// Revision 1.5  1997/02/28 13:39:47  wan
// Modifications collected for new version 1.3: Bug fixes, tk4.2.
//
// Revision 1.4  1995/08/17 15:23:51  rj
// introducing an AsnEnumTypeDesc class with its own TclGetDesc2 function that returns the value names but omits the numeric values.
// utility function AsnSe_TypeDesc::mandatmemberr added.
//
// Revision 1.3  1995/07/26  19:39:35  rj
// comment leader fixed
//
// Revision 1.2  1995/07/25  22:11:31  rj
// lots of new data types, and new data and function members in old ones.
//
// use memcmpeq that is defined in .../snacc.h to use either memcmp or bcmp.
//
// code extracted from AsnOcts::TclGetVal and AsnOcts::TclSetVal in asn-octs.C into ::debinify and ::binify.
//
// #if TCL ... #endif wrapped into #if META ... #endif
//
// call constructor with additional pdu and create arguments.
//
// changed `_' to `-' in file names.

#include "asn-incl.h"

#if META

AsnMemberDesc::AsnMemberDesc (const char *_name, const AsnTypeDesc *_desc):
  name (_name),
  desc (_desc)
{
}

AsnMemberDesc::AsnMemberDesc():
  name (NULL), desc(NULL)
{
}

int AsnMemberDesc::TclGetDesc (Tcl_DString *desc) const
{
  if (name)
  {
    Tcl_DStringStartSublist (desc);
    Tcl_DStringAppendElement (desc, (char*)name);
    this->desc->AsnTypeDesc::TclGetDesc (desc);
    TclGetDesc2 (desc);
    Tcl_DStringEndSublist (desc);
    return TCL_OK;
  }
  else
    return TCL_BREAK;
}

int AsnMemberDesc::TclGetDesc2 (Tcl_DString *desc) const
{
  return TCL_OK;
}

AsnSe_MemberDesc::AsnSe_MemberDesc (const char *name, const AsnTypeDesc *desc, bool _optional):
  AsnMemberDesc (name, desc),
  optional (_optional)
{
}

AsnSe_MemberDesc::AsnSe_MemberDesc():
  AsnMemberDesc()
{
}

int AsnSe_MemberDesc::TclGetDesc2 (Tcl_DString *desc) const
{
  Tcl_DStringAppendElement (desc, optional ? "optional" : "mandatory");
  return TCL_OK;
}

const char *const AsnTypeDesc::typenames[] =	// NOTE: keep this array in sync with the enum Type
{
  "(void)",
  "(alias)",

  "INTEGER",
  "REAL",
  "NULL",
  "BOOLEAN",
  "ENUMERATED",
  "BIT STRING",
  "OCTET STRING",
  "OBJECT IDENTIFIER",

  "SET",
  "SEQUENCE",
  "SET OF",
  "SEQUENCE OF",
  "CHOICE",
  "ANY",
};

AsnTypeDesc::AsnTypeDesc (const AsnModuleDesc *_module, const char *_name, bool ispdu, Type _type, AsnType *(*_create)()):
  module (_module),
  name (_name),
  pdu (ispdu),
  type (_type),
  create (_create)
{
}

const AsnModuleDesc *AsnTypeDesc::getmodule() const
{
  return module;
}

const char *AsnTypeDesc::getname() const
{
  return name;
}

bool AsnTypeDesc::ispdu() const
{
  return pdu;
}

AsnTypeDesc::Type AsnTypeDesc::gettype() const
{
  return type;
}

const AsnNameDesc *AsnTypeDesc::getnames() const
{
  Asn1Error << typenames[type] << "::getnames() called" << endl;
  abort();
  return NULL;
}

//const AsnMemberDesc *AsnTypeDesc::getmembers() const
//{
  //Asn1Error << typenames[type] << "::getmembers() called" << endl;
  //abort();
//}

//\[banner "names types (int, enum)"]-----------------------------------------------------------------------------------------------
AsnNamesTypeDesc::AsnNamesTypeDesc (const AsnModuleDesc *module, const char *name, bool ispdu, Type type, AsnType *(*create)(), const AsnNameDesc *_names):
  AsnTypeDesc (module, name, ispdu, type, create),
  names (_names)
{
}

const AsnNameDesc *AsnNamesTypeDesc::getnames() const
{
  return names;
}

//\[banner "enum type"]-------------------------------------------------------------------------------------------------------------
AsnEnumTypeDesc::AsnEnumTypeDesc (const AsnModuleDesc *module, const char *name, bool ispdu, Type type, AsnType *(*create)(), const AsnNameDesc *names):
  AsnNamesTypeDesc (module, name, ispdu, type, create, names)
{
}

//\[banner "members types (choice, set, sequence)"]---------------------------------------------------------------------------------
AsnMembersTypeDesc::AsnMembersTypeDesc (const AsnModuleDesc *module, const char *name, bool ispdu, Type type, AsnType *(*create)()):
  AsnTypeDesc (module, name, ispdu, type, create)
{
}

//\[banner "choice type"]-----------------------------------------------------------------------------------------------------------
AsnChoiceTypeDesc::AsnChoiceTypeDesc (const AsnModuleDesc *module, const char *name, bool ispdu, Type type, AsnType *(*create)(), const AsnChoiceMemberDesc *_members):
  AsnMembersTypeDesc (module, name, ispdu, type, create),
  members (_members)
{
}

int AsnChoiceTypeDesc::choicebyname (const char *name) const
{
  for (int m=0; members[m].name; m++)
    if (!strcmp (members[m].name, name))
      return m;

  return -1;
}

const char *AsnChoiceTypeDesc::choicebyvalue (int value) const
{
  return members[value].name;
}

//\[banner "set/sequence type"]-----------------------------------------------------------------------------------------------------
AsnSe_TypeDesc::AsnSe_TypeDesc (const AsnModuleDesc *module, const char *name, bool ispdu, Type type, AsnType *(*create)(), const AsnSe_MemberDesc *_members):
  AsnMembersTypeDesc (module, name, ispdu, type, create),
  members (_members)
{
}

//\[banner "list type"]-------------------------------------------------------------------------------------------------------------
AsnListTypeDesc::AsnListTypeDesc (const AsnModuleDesc *module, const char *name, bool ispdu, Type type, AsnType *(*create)(), const AsnTypeDesc *_base):
  AsnTypeDesc (module, name, ispdu, type, create),
  base (_base)
{
}

//\[banner "alias type"]------------------------------------------------------------------------------------------------------------
AsnAliasTypeDesc::AsnAliasTypeDesc (const AsnModuleDesc *module, const char *name, bool ispdu, Type type, AsnType *(*create)(), const AsnTypeDesc *_alias):
  AsnTypeDesc (module, name, ispdu, type, create),
  alias (_alias)
{
}

const AsnModuleDesc *AsnAliasTypeDesc::getmodule() const
{
  return module;
}

const char *AsnAliasTypeDesc::getname() const
{
  return name;
}

bool AsnAliasTypeDesc::ispdu() const
{
  return pdu;
}

AsnTypeDesc::Type AsnAliasTypeDesc::gettype() const
{
  return alias->gettype();
}

const AsnNameDesc *AsnAliasTypeDesc::getnames() const
{
  return alias->getnames();
}

//const AsnMemberDesc *AsnAliasTypeDesc::getmembers() const
//{
  //return alias->getmembers();
//}

//\[banner "Tcl routines"]----------------------------------------------------------------------------------------------------------
#if TCL

int AsnTypeDesc::TclGetDesc (Tcl_DString *desc) const
{
  Tcl_DStringStartSublist (desc);
  Tcl_DStringAppendElement (desc, getmodule() ? (char*) getmodule()->name : "");
  Tcl_DStringAppendElement (desc, getname() ? (char*) getname() : "");
  Tcl_DStringEndSublist (desc);
  Tcl_DStringAppendElement (desc, ispdu() ? "pdu" : "sub");
  Tcl_DStringAppendElement (desc, (char*) typenames[gettype()]);

  return TCL_OK;
}

int AsnTypeDesc::TclGetDesc2 (Tcl_DString *desc) const
{
  return TCL_OK;
}

int AsnNamesTypeDesc::TclGetDesc (Tcl_DString *desc) const
{
  AsnTypeDesc::TclGetDesc (desc);
  return TclGetDesc2 (desc);
}

// for BIT STRING and INTEGER:
int AsnNamesTypeDesc::TclGetDesc2 (Tcl_DString *desc) const
{
  Tcl_DStringStartSublist (desc);
  const AsnNameDesc *n;
  if (n = names)
    for (; n->name; n++)
    {
      Tcl_DStringStartSublist (desc);
      Tcl_DStringAppendElement (desc, (char*) n->name);
      char buf[32];
      sprintf (buf, "%d", n->value);
      Tcl_DStringAppendElement (desc, buf);
      Tcl_DStringEndSublist (desc);
    }
  Tcl_DStringEndSublist (desc);

  return TCL_OK;
}

int AsnEnumTypeDesc::TclGetDesc2 (Tcl_DString *desc) const
{
  Tcl_DStringStartSublist (desc);
  const AsnNameDesc *n;
  if (n = names)
    for (; n->name; n++)
      Tcl_DStringAppendElement (desc, (char*) n->name);
  Tcl_DStringEndSublist (desc);

  return TCL_OK;
}

int AsnMembersTypeDesc::TclGetDesc (Tcl_DString *desc) const
{
  AsnTypeDesc::TclGetDesc (desc);
  return TclGetDesc2 (desc);
}

int AsnChoiceTypeDesc::TclGetDesc2 (Tcl_DString *desc) const
{
  Tcl_DStringStartSublist (desc);
  const AsnChoiceMemberDesc *m;
  if (m = members)
    for (; m->TclGetDesc (desc) == TCL_OK; m++)
      ;
  Tcl_DStringEndSublist (desc);

  return TCL_OK;
}

int AsnSe_TypeDesc::mandatmemberr (Tcl_Interp *interp, const char *membername) const
{
  sprintf (interp->result, "(in type %s.%s:) member %s is mandatory and can't be deleted", getmodule()->name, getname(), membername);
  Tcl_SetErrorCode (interp, "SNACC", "MANDMEMB", NULL);
  return TCL_ERROR;
}

int AsnSe_TypeDesc::TclGetDesc2 (Tcl_DString *desc) const
{
  Tcl_DStringStartSublist (desc);
  const AsnSe_MemberDesc *m;
  if (m = members)
    for (; m->TclGetDesc (desc) == TCL_OK; m++)
      ;
  Tcl_DStringEndSublist (desc);

  return TCL_OK;
}

int AsnListTypeDesc::TclGetDesc (Tcl_DString *desc) const
{
  AsnTypeDesc::TclGetDesc (desc);
  return base->AsnTypeDesc::TclGetDesc (desc);
}

int AsnAliasTypeDesc::TclGetDesc (Tcl_DString *desc) const
{
  AsnTypeDesc::TclGetDesc (desc);
  return alias->TclGetDesc2 (desc);
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
// designed to be used with Tcl_SplitList(): argument list that automagically frees itself when it goes out of scope:
Args::Args()
{
  v = NULL;
}

Args::~Args()
{
  if (v)
    free (v);
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
// since Tcl cannot handle binary strings, the following hack is needed:

int debinify (Tcl_Interp *interp, const char *bin, size_t len)
{
  char*	str;
  int	i, o;

#ifndef _IBM_ENC_
    str = new char[2*len+1];
#else
    str = (char *) mem_mgr_ptr->Get (2*len+2);
#endif /* _IBM_ENC_ */


  for (o=i=0; i<len; i++)
    switch (bin[i])
    {
      case '\0':
	str[o++] = '\\';
	str[o++] = '0';
	break;
      case '\\':
	str[o++] = '\\';
	// fall thru
      default:
	str[o++] = bin[i];
    }
  str[o] = '\0';

  Tcl_SetResult (interp, str, TCL_VOLATILE);

#ifndef _IBM_ENC_
  delete str;
#else
  mem_mgr_ptr->Put ((void*) str);
#endif /* _IBM_ENC_ */

  return TCL_OK;
}

int binify (Tcl_Interp *interp, const char *str, char *buf, size_t *len)
{
  for (*len=0; *str; )
    if (*str == '\\')
      switch (*++str)
      {
	case '0':
	  buf[(*len)++] = '\0';
	  str++;
	  break;
	case '\\':
	  buf[(*len)++] = *str++;
	  break;
	default:
	  Tcl_AppendResult (interp, "illegal use of '\\' in string value", NULL);
	  Tcl_SetErrorCode (interp, "SNACC", "ILLESC", NULL);
	  return TCL_ERROR;
      }
    else
      buf[(*len)++] = *str++;

  return TCL_OK;
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
#endif // TCL

#endif // META
