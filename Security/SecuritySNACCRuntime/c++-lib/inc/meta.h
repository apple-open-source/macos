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


// file: .../c++-lib/inc/meta.h
//
// $Header: /cvs/root/Security/SecuritySNACCRuntime/c++-lib/inc/Attic/meta.h,v 1.1.1.1 2001/05/18 23:14:06 mb Exp $
// $Log: meta.h,v $
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.3  2001/05/05 00:59:18  rmurphy
// Adding darwin license headers
//
// Revision 1.2  2000/06/15 18:48:25  dmitch
// Snacc-generated source files, now part of CVS tree to allow for cross-platform build of snaccRuntime.
//
// Revision 1.1.1.1  2000/03/09 01:00:05  rmurphy
// Base Fortissimo Tree
//
// Revision 1.1  1999/02/25 05:21:47  mb
// Added snacc c++ library
//
// Revision 1.6  1997/02/28 13:39:43  wan
// Modifications collected for new version 1.3: Bug fixes, tk4.2.
//
// Revision 1.5  1995/09/07 18:50:04  rj
// long int replaced by newly introduced AsnIntType.
// it shall provide a 32 bit integer type on all platforms.
//
// Revision 1.4  1995/08/17  15:23:47  rj
// introducing an AsnEnumTypeDesc class with its own TclGetDesc2 function that returns the value names but omits the numeric values.
// utility function AsnSe_TypeDesc::mandatmemberr added.
//

#include <unistd.h>
#include <stdlib.h>

struct AsnNameDesc
{
  const char			*const name;
  const AsnIntType		value;
};

struct AsnTypeDesc;

struct AsnMemberDesc // description of CHOICE member; base class for AsnSe_MemberDesc
{
  const char			*const name;
  const AsnTypeDesc		*const desc;

				AsnMemberDesc (const char *, const AsnTypeDesc *);
				AsnMemberDesc();

#if TCL
  virtual int			TclGetDesc (Tcl_DString *) const;
  virtual int			TclGetDesc2 (Tcl_DString *) const;
#endif
};

struct AsnSe_MemberDesc: AsnMemberDesc	// _ == t/quence; description of SET or SEQUENCE member
{
  bool				optional;

				AsnSe_MemberDesc (const char *, const AsnTypeDesc *, bool);
				AsnSe_MemberDesc();

#if TCL
  int				TclGetDesc2 (Tcl_DString *) const;
#endif
};

typedef AsnMemberDesc		AsnChoiceMemberDesc;
typedef AsnSe_MemberDesc	AsnSetMemberDesc;
typedef AsnSe_MemberDesc	AsnSequenceMemberDesc;

struct AsnModuleDesc;

class AsnType;

struct AsnTypeDesc
{
  const AsnModuleDesc		*module;
  const char			*const name;	// NULL for basic types
  const bool			pdu;
  const enum Type	// NOTE: keep this enum in sync with the typenames[]
  {
    VOID,
    ALIAS,

    INTEGER,
    REAL,
    NUL_, // sic! (can't fight the ubiquitous NULL #define)
    BOOLEAN,
    ENUMERATED,
    BIT_STRING,
    OCTET_STRING,
    OBJECT_IDENTIFIER,

    SET,
    SEQUENCE,
    SET_OF,
    SEQUENCE_OF,
    CHOICE,
    ANY
  }				type;

  AsnType			*(*create)();

  static const char		*const typenames[];

				AsnTypeDesc (const AsnModuleDesc *, const char *, bool ispdu, Type, AsnType *(*create)());

  virtual const AsnModuleDesc	*getmodule() const;
  virtual const char		*getname() const;
  virtual bool			ispdu() const;
  virtual Type			gettype() const;
  virtual const AsnNameDesc	*getnames() const;
  //virtual const AsnMemberDesc	*getmembers() const;

#if TCL
  virtual int			TclGetDesc (Tcl_DString *) const;
  virtual int			TclGetDesc2 (Tcl_DString *) const;
#endif
};

struct AsnNamesTypeDesc: AsnTypeDesc
{
  const AsnNameDesc		*const names;

				AsnNamesTypeDesc (const AsnModuleDesc *, const char *, bool ispdu, Type, AsnType *(*create)(), const AsnNameDesc *);

  const AsnNameDesc		*getnames() const;

#if TCL
  int				TclGetDesc (Tcl_DString *) const;
  // for BIT STRING and INTEGER, ENUMERATED has its own:
  int				TclGetDesc2 (Tcl_DString *) const;
#endif
};

struct AsnEnumTypeDesc: AsnNamesTypeDesc
{
				AsnEnumTypeDesc (const AsnModuleDesc *, const char *, bool ispdu, Type, AsnType *(*create)(), const AsnNameDesc *);

#if TCL
  int				TclGetDesc2 (Tcl_DString *) const;
#endif
};

struct AsnMembersTypeDesc: AsnTypeDesc
{
				AsnMembersTypeDesc (const AsnModuleDesc *, const char *, bool ispdu, Type, AsnType *(*create)());

#if TCL
  int				TclGetDesc (Tcl_DString *) const;
#endif
};

struct AsnChoiceTypeDesc: AsnMembersTypeDesc
{
  const AsnChoiceMemberDesc	*const members;

				AsnChoiceTypeDesc (const AsnModuleDesc *, const char *, bool ispdu, Type, AsnType *(*create)(), const AsnChoiceMemberDesc *);

  int				choicebyname (const char *name) const;
  const char			*choicebyvalue (int value) const;

#if TCL
  int				TclGetDesc2 (Tcl_DString *) const;
#endif
};

struct AsnSe_TypeDesc: AsnMembersTypeDesc
{
  const AsnSe_MemberDesc	*const members;

				AsnSe_TypeDesc (const AsnModuleDesc *, const char *, bool ispdu, Type, AsnType *(*create)(), const AsnSe_MemberDesc *);

#if TCL
  int				mandatmemberr (Tcl_Interp *interp, const char *membername) const;
  int				TclGetDesc2 (Tcl_DString *) const;
#endif
};

struct AsnListTypeDesc: AsnTypeDesc
{
  const AsnTypeDesc		*const base;

				AsnListTypeDesc (const AsnModuleDesc *, const char *, bool ispdu, Type, AsnType *(*create)(), const AsnTypeDesc *);

#if TCL
  int				TclGetDesc (Tcl_DString *) const;
#endif
};

struct AsnAliasTypeDesc: AsnTypeDesc
{
  const AsnTypeDesc		*const alias;

				AsnAliasTypeDesc (const AsnModuleDesc *, const char *, bool ispdu, Type, AsnType *(*create)(), const AsnTypeDesc *);

  const AsnModuleDesc		*getmodule() const;
  const char			*getname() const;
  bool				ispdu() const;
  Type				gettype() const;

  const AsnNameDesc		*getnames() const;
  //const AsnMemberDesc		*getmembers() const;

#if TCL
  int				TclGetDesc (Tcl_DString *) const;
#endif
};

typedef AsnTypeDesc		AsnRealTypeDesc;
typedef AsnTypeDesc		AsnNullTypeDesc;
typedef AsnTypeDesc		AsnBoolTypeDesc;
typedef AsnNamesTypeDesc	AsnIntTypeDesc;
typedef AsnNamesTypeDesc	AsnBitsTypeDesc;
typedef AsnTypeDesc		AsnOctsTypeDesc;
typedef AsnTypeDesc		AsnOidTypeDesc;
typedef AsnSe_TypeDesc		AsnSetTypeDesc;
typedef AsnSe_TypeDesc		AsnSequenceTypeDesc;

struct AsnModuleDesc
{
  const char			*const name;
  const AsnTypeDesc		**const types;
};

extern const AsnModuleDesc	*asnModuleDescs[];

#if TCL

//\[sep]----------------------------------------------------------------------------------------------------------------------------
// designed to be used with Tcl_SplitList(): argument list that automagically frees itself when it goes out of scope:

struct Args
{
  int				c;
  char				**v;

				Args();
  virtual			~Args();
};

//\[sep]----------------------------------------------------------------------------------------------------------------------------
// file that automagically closes itself when it goes out of scope:

struct TmpFD
{
  int	fd;

	TmpFD()			{ fd = -1; }
	TmpFD (int _fd)		{ fd = _fd; }
	~TmpFD()		{ if (fd > 0) ::close (fd); }

	int operator = (int _fd){ return fd = _fd; }
//	operator int()		{ return fd; }
};

//\[sep]----------------------------------------------------------------------------------------------------------------------------
// hack to cope with Tcl's inability to handle binary strings:

extern int	debinify (Tcl_Interp *interp, const char *in, size_t len);
extern int	binify (Tcl_Interp *interp, const char *str, char *buf, size_t *len);

//\[sep]----------------------------------------------------------------------------------------------------------------------------
#endif /* TCL */
