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


// file: .../c++-lib/inc/tcl-if.h
//
// $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c++-lib/inc/tcl-if.h,v 1.1.1.1 2001/05/18 23:14:06 mb Exp $
// $Log: tcl-if.h,v $
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
// Revision 1.1  1999/02/25 05:21:48  mb
// Added snacc c++ library
//
// Revision 1.5  1997/01/01 23:27:22  rj
// `typename' appears to be a reserved word in gcc 2.7, so prefix it with `_'
//
// Revision 1.4  1995/09/07  18:50:34  rj
// duplicate code merged into a new function SnaccTcl::gettypedesc().
//
// Revision 1.3  1995/08/17  15:06:43  rj
// snacced.[hC] renamed to tcl-if.[hC].
// class SnaccEd renamed to SnaccTcl.
//
// Revision 1.2  1995/07/27  09:53:25  rj
// comment leader fixed
//
// Revision 1.1  1995/07/27  09:52:12  rj
// new file: tcl interface used by snacced.

#ifdef DEBUG
#include <assert.h>
#endif

class SnaccTcl
{
  Tcl_Interp		*interp;
  Tcl_HashTable		modules,
			types,
			files;

  Tcl_HashEntry		*create();
  const AsnTypeDesc	*gettypedesc (const char *cmdname, const char *type_name);

public:
			SnaccTcl (Tcl_Interp *);
			~SnaccTcl();

  int			create (int argc, char **argv);
  int			openfile (int argc, char **argv);
  int			finfo (int argc, char **argv);
  int			read (int argc, char **argv);
  int			write (int argc, char **argv);
  int			closefile (int argc, char **argv);

  int			modulesinfo (int argc, char **argv);
  int			typesinfo (int argc, char **argv);
  int			typeinfo (int argc, char **argv);
  int			info (int argc, char **argv);

  int			getval (int argc, char **argv);
  int			setval (int argc, char **argv);
  int			unsetval (int argc, char **argv);

  int			test (int argc, char **argv);

#ifdef DEBUG
  void			ckip (Tcl_Interp *i)	{ assert (i == interp); }
#endif
};

class ASN1File
{
  const AsnTypeDesc	*type;
  AsnType		*pdu;

  char			*fn;
  int			fd;
  off_t			filesize;

public:
			ASN1File (const AsnTypeDesc *);
			ASN1File (const AsnTypeDesc *, const char *fn, int fd);
  virtual		~ASN1File();

  bool			bad();

			operator AsnType * ()	{ return pdu; }

  int			finfo (Tcl_Interp *);

  int			read (Tcl_Interp *, const char *fn=NULL);
  int			write (Tcl_Interp *, const char *fn=NULL);
};
