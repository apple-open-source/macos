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


// file: .../c++-lib/src/tcl-if.C
//
// $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c++-lib/c++/tcl-if.cpp,v 1.1.1.1 2001/05/18 23:14:06 mb Exp $
// $Log: tcl-if.cpp,v $
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.2  2001/05/05 00:59:17  rmurphy
// Adding darwin license headers
//
// Revision 1.1  2000/06/15 18:44:58  dmitch
// These snacc-generated source files are now checked in to allow cross-platform build.
//
// Revision 1.2  2000/06/08 20:05:37  dmitch
// Mods for X port. These files are actually machine generated and probably don't need to be in CVS....
//
// Revision 1.1.1.1  2000/03/09 01:00:06  rmurphy
// Base Fortissimo Tree
//
// Revision 1.2  1999/02/26 00:23:41  mb
// Fixed for Mac OS 8
//
// Revision 1.1  1999/02/25 05:21:57  mb
// Added snacc c++ library
//
// Revision 1.6  1997/02/28 13:39:47  wan
// Modifications collected for new version 1.3: Bug fixes, tk4.2.
//
// Revision 1.5  1997/01/01 23:24:35  rj
// `typename' appears to be a reserved word in gcc 2.7, so prefix it with `_'
//
// Revision 1.4  1995/09/07  18:57:13  rj
// duplicate code merged into a new function SnaccTcl::gettypedesc().
//
// Revision 1.3  1995/08/17  15:09:09  rj
// snacced.[hC] renamed to tcl-if.[hC].
// class SnaccEd renamed to SnaccTcl.
// set Tcl's errorCode variable.
//
// Revision 1.2  1995/07/27  09:53:38  rj
// comment leader fixed
//
// Revision 1.1  1995/07/27  09:52:22  rj
// new file: tcl interface used by snacced.

#if !defined(macintosh) && !defined(__APPLE__)
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <strstream.h>
#include <fstream.h>
#include <string.h>

#include "asn-incl.h"

#if TCL

#ifdef _AIX32
extern "C" int strncasecmp (const char* s1, const char* s2, size_t number);
extern "C" int strcasecmp (const char* s1, const char* s2);
#endif

#include "tcl-if.h"
#include "init.h"

//\[banner "utility functions"]-----------------------------------------------------------------------------------------------------
static bool strniabbr (const char *pattern, const char *test, size_t min)
{
  register      len;

  if (strlen (pattern)<min)
    fprintf (stderr, "strniabbr(): strlen (pattern) < min\n");
  if ((len = strlen (test))<min)
    return false;
  return !strncasecmp (pattern, test, len);
}

//\[banner "ctor & dtor"]-----------------------------------------------------------------------------------------------------------
ASN1File::ASN1File (const AsnTypeDesc *typedesc)
{
  type = typedesc;
  pdu = type->create();
  fn = NULL;
  fd = -1;
  filesize = 0;
}

ASN1File::ASN1File (const AsnTypeDesc *typedesc, const char *_fn, int _fd)
{
  type = typedesc;
  pdu = type->create();

  int fnlen = strlen (_fn) + 1;
  fn = new char [fnlen];
  memcpy (fn, _fn, fnlen);

  fd = _fd;
}

ASN1File::~ASN1File()
{
  delete pdu;
  delete fn;
  if (fd >= 0)
    close (fd);
}

bool ASN1File::bad()
{
  return fd < 0;
}

int ASN1File::finfo (Tcl_Interp *interp)
{
  Tcl_AppendElement (interp, fn ? fn : "");
  char *acc = "bad";
  if (!bad())
  {
    int flags;
    if ((flags = fcntl (fd, F_GETFL)) != -1)
      switch (flags & O_ACCMODE)
      {
	case O_RDONLY:
	  acc = "ro";
	  break;
	case O_WRONLY:
	  acc = "wo";
	  break;
	case O_RDWR:
	  acc = "rw";
	  break;
      }
  }
  Tcl_AppendElement (interp, acc);

  return TCL_OK;
}

int ASN1File::read (Tcl_Interp *interp, const char *rfn)
{
  int rfd;
  TmpFD tmpfd;

  delete pdu;
  pdu = type->create();

  if (rfn)
  {
    if ((rfd = open (rfn, O_RDONLY)) < 0)
    {
      Tcl_AppendResult (interp, "can't open \"", rfn, "\": ", Tcl_PosixError (interp), NULL);
      return TCL_ERROR;
    }
    tmpfd = rfd;
  }
  else if (fd < 0)
  {
    Tcl_AppendResult (interp, "can't read, file is not open", NULL);
    Tcl_SetErrorCode (interp, "SNACC", "MUSTOPEN", NULL);
    return TCL_ERROR;
  }
  else
  {
    rfn = fn;
    lseek (rfd = fd, 0l, SEEK_SET);
  }

  struct stat statbuf;
  if (fstat (rfd, &statbuf))
  {
    Tcl_AppendResult (interp, "can't fstat \"", rfn, "\": ", Tcl_PosixError (interp), NULL);
    return TCL_ERROR;
  }

  filesize = statbuf.st_size;

  char* buf = new char[filesize];
  if (::read (rfd, buf, filesize) != filesize)
  {
    Tcl_AppendResult (interp, "can't read \"", rfn, "\": ", Tcl_PosixError (interp), NULL);
    delete buf;
    return TCL_ERROR;
  }

  AsnBuf inputBuf;
  inputBuf.InstallData (buf, filesize);

  size_t decodedLen = 0;
  jmp_buf env;
  int eval;
  if (eval = setjmp (env))
  {
    char eno[80];
    sprintf (eno, "%d", eval);
    Tcl_AppendResult (interp, "can't decode (error ", eno, ")", NULL);
    Tcl_SetErrorCode (interp, "SNACC", "DECODE", eno, NULL);
    delete buf;
    return TCL_ERROR;
  }
  pdu->BDec (inputBuf, decodedLen, env);
  if (inputBuf.ReadError())
  {
    Tcl_AppendResult (interp, "can't decode, out of data", NULL);
    Tcl_SetErrorCode (interp, "SNACC", "DECODE", "EOBUF", NULL);
    delete buf;
    return TCL_ERROR;
  }

#if DEBUG
cout << "DECODED:" << endl << *pdu << endl;
#endif

  if (decodedLen != filesize)
    sprintf (interp->result, "decoded %d of %d bytes", decodedLen, filesize);

  delete buf;
  return TCL_OK;
}

int ASN1File::write (Tcl_Interp *interp, const char *wfn)
{
  int wfd;
  TmpFD tmpfd;

  if (wfn)
  {
    if ((wfd = open (wfn, O_CREAT|O_TRUNC|O_WRONLY, 0666)) < 0)
    {
      Tcl_AppendResult (interp, "can't open \"", wfn, "\": ", Tcl_PosixError (interp), NULL);
      return TCL_ERROR;
    }
    tmpfd = wfd;
  }
  else if (fd < 0)
  {
    Tcl_AppendResult (interp, "can't write, file is not open", NULL);
    Tcl_SetErrorCode (interp, "SNACC", "MUSTOPEN", NULL);
    return TCL_ERROR;
  }
  else
  {
    wfn = fn;
    int flags;
    if ((flags = fcntl (fd, F_GETFL)) == -1)
    {
      Tcl_AppendResult (interp, "can't fcntl \"", wfn, "\": ", Tcl_PosixError (interp), NULL);
      return TCL_ERROR;
    }
    else
    {
      if ((flags & O_ACCMODE) == O_RDONLY)
      {
	Tcl_AppendResult (interp, "can't write, file is read only", NULL);
	Tcl_SetErrorCode (interp, "SNACC", "WRITE", "RDONLY", NULL);
	return TCL_ERROR;
      }
    }
    lseek (wfd = fd, 0l, SEEK_SET);
  }

  size_t size = filesize ? filesize : 10240;
  char *buf;
  AsnBuf outputBuf;
  size_t encodedLen;
  for (;;)
  {
    size <<= 1;
    buf = new char[size];
    outputBuf.Init (buf, size);
    outputBuf.ResetInWriteRvsMode();
    encodedLen = pdu->BEnc (outputBuf);
    if (!outputBuf.WriteError())
      break;
    delete buf;
  }

  outputBuf.ResetInReadMode();
  size_t hunklen = 8192;
  char* hunk = new char[hunklen];
  for (size_t written=0; written<encodedLen; written+=hunklen)
  {
    if (encodedLen-written < hunklen)
      hunklen = encodedLen - written;
    outputBuf.CopyOut (hunk, hunklen);
    if (::write (wfd, hunk, hunklen) != hunklen)
    {
      Tcl_AppendResult (interp, "write error on \"", wfn, "\": ", Tcl_PosixError (interp), NULL);
      delete hunk; // may affect errno
      delete buf; // may affect errno
      return TCL_ERROR;
    }
  }

  delete hunk;
  delete buf;

  filesize = encodedLen;
  if (!wfn)
    ftruncate (wfd, filesize);

  return TCL_OK;
}

//\[banner "import & export"]-------------------------------------------------------------------------------------------------------
int import (Tcl_Interp *interp, int argc, char **argv)
{
  if (argc != 2)
  {
    strcpy (interp->result, "wrong # args: should be \"snacc import filename\"");
    return TCL_ERROR;
  }

  const char *fn = argv[1];
  int fd;
  if ((fd = open (fn, O_RDONLY)) < 0)
  {
    Tcl_AppendResult (interp, "can't open \"", fn, "\": ", Tcl_PosixError (interp), NULL);
    return TCL_ERROR;
  }
  TmpFD tmpfd (fd);

  struct stat statbuf;
  if (fstat (fd, &statbuf))
  {
    Tcl_AppendResult (interp, "can't fstat \"", fn, "\"'s fd: ", Tcl_PosixError (interp), NULL);
    return TCL_ERROR;
  }

  off_t filesize = statbuf.st_size;

  char* ibuf = new char[filesize];
  if (::read (fd, ibuf, filesize) != filesize)
  {
    Tcl_AppendResult (interp, "read error on \"", fn, "\": ", Tcl_PosixError (interp), NULL);
    delete ibuf;
    return TCL_ERROR;
  }

  int result = debinify (interp, ibuf, filesize);
  delete ibuf;
  return result;
}

int export (Tcl_Interp *interp, int argc, char **argv)
{
  if (argc != 3)
  {
    strcpy (interp->result, "wrong # args: should be \"snacc export str filename\"");
    return TCL_ERROR;
  }

  const char *str = argv[1], *fn = argv[2];
  char* obuf = new char[strlen (str)]; // the binary buffer is as most as long as the escaped Tcl string.
  size_t olen;
  if (binify (interp, str, obuf, &olen) != TCL_OK)
  {
    delete obuf;
    return TCL_ERROR;
  }

  int fd;
  if ((fd = open (fn, O_CREAT|O_TRUNC|O_WRONLY, 0666)) < 0)
  {
    Tcl_AppendResult (interp, "can't open \"", fn, "\": ", Tcl_PosixError (interp), NULL);
    delete obuf;
    return TCL_ERROR;
  }
  TmpFD tmpfd (fd);

  if (::write (fd, obuf, olen) != olen)
  {
    Tcl_AppendResult (interp, "write error on \"", fn, "\": ", Tcl_PosixError (interp), NULL);
    delete obuf;
    return TCL_ERROR;
  }

  delete obuf;
  return TCL_OK;
}

//\[banner "ctor & dtor"]-----------------------------------------------------------------------------------------------------------
SnaccTcl::SnaccTcl (Tcl_Interp *i)
{
  interp = i;

  Tcl_InitHashTable (&modules, TCL_STRING_KEYS);
  Tcl_InitHashTable (&types, TCL_STRING_KEYS);

  const AsnModuleDesc **moddesc;
  for (moddesc=asnModuleDescs; *moddesc; moddesc++)
  {
    int created;
    Tcl_HashEntry *entry = Tcl_CreateHashEntry (&modules, (char*)(*moddesc)->name, &created);
    assert (created);
    Tcl_SetHashValue (entry, *moddesc);

    const AsnTypeDesc **typedesc;
    for (typedesc=(*moddesc)->types; *typedesc; typedesc++)
    {
      char buf[1024];
      sprintf (buf, "%s %s", (*moddesc)->name, (*typedesc)->name);
      char *_typename = strdup (buf);
      int created;
      Tcl_HashEntry *entry = Tcl_CreateHashEntry (&types, _typename, &created);
      if (!created)
      {
	cerr << "fatal error: duplicate type " << _typename << endl;
	exit (1);
      }
      Tcl_SetHashValue (entry, *typedesc);
    }
  }

  Tcl_InitHashTable (&files, TCL_STRING_KEYS);
}

SnaccTcl::~SnaccTcl()
{
  Tcl_DeleteHashTable (&files);
}

//\[banner "utility functions"]-----------------------------------------------------------------------------------------------------
const AsnTypeDesc *SnaccTcl::gettypedesc (const char *cmdname, const char *_typename)
{
  Tcl_HashEntry *typedescentry;
  if (typedescentry = Tcl_FindHashEntry (&types, (char*)_typename))
    return (const AsnTypeDesc *)Tcl_GetHashValue (typedescentry);
  else
  {
    Tcl_SetErrorCode (interp, "SNACC", "ILLTYPE", NULL);
    Tcl_AppendResult (interp, "snacc ", cmdname, ": no type \"", _typename, "\"", NULL);
    return NULL;
  }
}

//\[banner "data manipulation functions"]-------------------------------------------------------------------------------------------
Tcl_HashEntry *SnaccTcl::create()
{
  static unsigned int id;
  int created;
  Tcl_HashEntry *entry;
  do
  {
    sprintf (interp->result, "file%u", id++);
    entry = Tcl_CreateHashEntry (&files, interp->result, &created);
  }
  while (!created);
  return entry;
}

int SnaccTcl::create (int argc, char **argv)
{
  if (argc != 2)
  {
    strcpy (interp->result, "wrong # args: should be \"snacc create {module type}\"");
    return TCL_ERROR;
  }

  const char	*_typename = argv[1];
  const AsnTypeDesc *typedesc;
  if (!(typedesc = gettypedesc ("type", _typename)))
    return TCL_ERROR;

  Tcl_HashEntry *entry = create();
  ASN1File *file = new ASN1File (typedesc);
  Tcl_SetHashValue (entry, file);

  return TCL_OK;
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
// snacc open {module type} filename ?flags? ?permissions?

int SnaccTcl::openfile (int argc, char **argv)
{
  if (argc < 3 || argc > 5)
  {
    strcpy (interp->result, "wrong # args: should be \"snacc open {module type} filename ?flags? ?permissions?\"");
    return TCL_ERROR;
  }

  const char	*_typename = argv[1];
  const char	*filename = argv[2];
  bool		rw_spec = false;
  int		oflags = 0, omode = 0666, fd = -1;

  switch (argc)
  {
    case 5:
      if (Tcl_GetInt (interp, argv[4], &omode))
	return TCL_ERROR;
      // \(da fall thru
    case 4:
      {
	Args flags;
	if (Tcl_SplitList (interp, argv[3], &flags.c, &flags.v) != TCL_OK)
	  return TCL_ERROR;

	for (int i=0; i<flags.c; i++)
	{
	  if (strniabbr ("truncate", flags.v[i], 1))
	    oflags |= O_TRUNC;
	  else if (strniabbr ("create", flags.v[i], 1))
	    oflags |= O_CREAT;
	  else if (!strcasecmp ("ro", flags.v[i]))
	  {
	    oflags |= O_RDONLY;
	    rw_spec = true;
	  }
	  else if (!strcasecmp ("rw", flags.v[i]))
	  {
	    oflags |= O_RDWR;
	    rw_spec = true;
	  }
	  else
	  {
	    Tcl_AppendResult (interp, "snacc open: illegal argument \"", flags.v[i], "\" in flags", NULL);
	    return TCL_ERROR;
	  }
	}
      }
      break;
  }

  const AsnTypeDesc *typedesc;
  if (!(typedesc = gettypedesc ("open", _typename)))
    return TCL_ERROR;

  if (rw_spec)
    fd = open (filename, oflags, omode);
  else
    if ((fd = open (filename, oflags | O_RDWR, omode)) < 0)
      fd = open (filename, oflags | O_RDONLY, omode);

  if (fd < 0)
  {
    Tcl_AppendResult (interp, "can't open \"", filename, "\": ", Tcl_PosixError (interp), NULL);
    return TCL_ERROR;
  }

  ASN1File *file = new ASN1File (typedesc, filename, fd);
  if (file->bad())
  {
    delete file;
    Tcl_AppendResult (interp, "internal error on \"", filename, "\": bad status", NULL);
    Tcl_SetErrorCode (interp, "SNACC", "OPEN", "BAD", NULL);
    return TCL_ERROR;
  }

  Tcl_HashEntry *entry = create();
  Tcl_SetHashValue (entry, file);

  return file->read (interp);
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
int SnaccTcl::finfo (int argc, char **argv)
{
  if (argc != 2)
  {
    strcpy (interp->result, "wrong # args: should be \"snacc finfo file\"");
    return TCL_ERROR;
  }

  Tcl_HashEntry *entry = Tcl_FindHashEntry (&files, argv[1]);
  if (!entry)
  {
    Tcl_AppendResult (interp, "no file named \"", argv[1], "\"", NULL);
    return TCL_ERROR;
  }

  ASN1File *file = (ASN1File *)Tcl_GetHashValue (entry);

  return file->finfo (interp);
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
// snacc read file ?{module type} filename?

int SnaccTcl::read (int argc, char **argv)
{
  const char	*_typename, *filename;

  switch (argc)
  {
    case 2: // reread from old fd
      _typename = filename = NULL;
      break;
    case 4:
      _typename = argv[2];
      filename = argv[3];
      break;
    default:
      strcpy (interp->result, "wrong # args: should be \"snacc read file ?{module type} filename?\"");
      return TCL_ERROR;
  }

  Tcl_HashEntry *entry = Tcl_FindHashEntry (&files, argv[1]);
  if (!entry)
  {
    Tcl_AppendResult (interp, "no file named \"", argv[1], "\"", NULL);
    return TCL_ERROR;
  }

  ASN1File *file = (ASN1File *)Tcl_GetHashValue (entry);

  if (_typename)
  {
    const AsnTypeDesc *typedesc;
    if (!(typedesc = gettypedesc ("read", _typename)))
      return TCL_ERROR;

    delete file;
    file = new ASN1File (typedesc);
    Tcl_SetHashValue (entry, file);
  }

  return file->read (interp, filename);
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
int SnaccTcl::write (int argc, char **argv)
{
  if (argc < 2 || argc > 3)
  {
    strcpy (interp->result, "wrong # args: should be \"snacc write file ?filename?\"");
    return TCL_ERROR;
  }

  Tcl_HashEntry *entry = Tcl_FindHashEntry (&files, argv[1]);
  if (!entry)
  {
    Tcl_AppendResult (interp, "no file named \"", argv[1], "\"", NULL);
    return TCL_ERROR;
  }

  ASN1File *file = (ASN1File *)Tcl_GetHashValue (entry);

  return file->write (interp, argv[2]);
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
int SnaccTcl::closefile (int argc, char **argv)
{
  if (argc != 2)
  {
    strcpy (interp->result, "wrong # args: should be \"snacc close file\"");
    return TCL_ERROR;
  }

  Tcl_HashEntry *entry = Tcl_FindHashEntry (&files, argv[1]);
  if (!entry)
  {
    Tcl_AppendResult (interp, "no file named \"", argv[1], "\"", NULL);
    return TCL_ERROR;
  }

  ASN1File *file = (ASN1File *)Tcl_GetHashValue (entry);
  delete file;

  Tcl_DeleteHashEntry (entry);

  return TCL_OK;
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
int SnaccTcl::modulesinfo (int argc, char **argv)
{
  if (argc != 1)
  {
    strcpy (interp->result, "wrong # args: should be \"snacc modules\"");
    return TCL_ERROR;
  }

  Tcl_HashEntry *moduleentry;
  Tcl_HashSearch hi;
  for (moduleentry=Tcl_FirstHashEntry (&modules, &hi); moduleentry; moduleentry=Tcl_NextHashEntry (&hi))
    Tcl_AppendElement (interp, Tcl_GetHashKey (&modules, moduleentry));

  return TCL_OK;
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
int SnaccTcl::typesinfo (int argc, char **argv)
{
  switch (argc)
  {
    case 1:
      Tcl_HashEntry *typeentry;
      Tcl_HashSearch hi;
      for (typeentry=Tcl_FirstHashEntry (&types, &hi); typeentry; typeentry=Tcl_NextHashEntry (&hi))
	Tcl_AppendElement (interp, Tcl_GetHashKey (&types, typeentry));
      return TCL_OK;
    case 2:
      Tcl_HashEntry *moduleentry;
      if (moduleentry = Tcl_FindHashEntry (&modules, argv[1]))
      {
	const AsnModuleDesc *moddesc = (const AsnModuleDesc *)Tcl_GetHashValue (moduleentry);
	const AsnTypeDesc **typedesc;
	for (typedesc=moddesc->types; *typedesc; typedesc++)
	  Tcl_AppendElement (interp, (char*)(*typedesc)->name);
	return TCL_OK;
      }
      else
      {
	Tcl_AppendResult (interp, "snacc types: no module \"", argv[1], "\"", NULL);
	return TCL_ERROR;
      }
    default:
      strcpy (interp->result, "wrong # args: should be \"snacc types ?module?\"");
      return TCL_ERROR;
  }
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
int SnaccTcl::typeinfo (int argc, char **argv)
{
  if (argc != 2)
  {
    strcpy (interp->result, "wrong # args: should be \"snacc type {module type}\"");
    return TCL_ERROR;
  }

  const char	*_typename = argv[1];
  const AsnTypeDesc *typedesc;
  if (!(typedesc = gettypedesc ("type", _typename)))
    return TCL_ERROR;

  Tcl_DString desc;
  Tcl_DStringInit (&desc);
  int rc = typedesc->TclGetDesc (&desc);
  Tcl_DStringResult (interp, &desc);
  return rc;
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
int SnaccTcl::info (int argc, char **argv)
{
  if (argc != 2)
  {
    strcpy (interp->result, "wrong # args: should be \"snacc info path\"");
    return TCL_ERROR;
  }

  Args path;
  if (Tcl_SplitList (interp, argv[1], &path.c, &path.v) != TCL_OK)
    return TCL_ERROR;

  if (path.c < 1)
  {
    strcpy (interp->result, "snacc info: wrong # args in path");
    return TCL_ERROR;
  }

  Tcl_HashEntry *entry = Tcl_FindHashEntry (&files, path.v[0]);
  if (!entry)
  {
    Tcl_AppendResult (interp, "snacc info: no file named \"", path.v[0], "\"", NULL);
    return TCL_ERROR;
  }

  ASN1File *file = (ASN1File *)Tcl_GetHashValue (entry);

  AsnType *var = (AsnType *)*file;
  for (int i=1; i<path.c; i++)
    if (!(var = var->_getref (path.v[i])))
    {
      Tcl_AppendResult (interp, "snacc info: illegal component \"", path.v[i], "\" in path", NULL);
      return TCL_ERROR;
    }

  Tcl_DString desc;
  Tcl_DStringInit (&desc);
  int rc;
  if ((rc = var->_getdesc()->AsnTypeDesc::TclGetDesc (&desc)) == TCL_OK)
    rc = var->TclGetDesc (&desc);
  Tcl_DStringResult (interp, &desc);
  return rc;
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
int SnaccTcl::getval (int argc, char **argv)
{
  if (argc != 2)
  {
    strcpy (interp->result, "wrong # args: should be \"snacc get path\"");
    return TCL_ERROR;
  }

  Args path;
  if (Tcl_SplitList (interp, argv[1], &path.c, &path.v) != TCL_OK)
    return TCL_ERROR;

  if (path.c < 1)
  {
    strcpy (interp->result, "snacc get: wrong # args in path");
    return TCL_ERROR;
  }

  Tcl_HashEntry *entry = Tcl_FindHashEntry (&files, path.v[0]);
  if (!entry)
  {
    Tcl_AppendResult (interp, "snacc get: no file named \"", path.v[0], "\"", NULL);
    return TCL_ERROR;
  }

  ASN1File *file = (ASN1File *)Tcl_GetHashValue (entry);

  AsnType *var = (AsnType *)*file;
  for (int i=1; i<path.c; i++)
    if (!(var = var->_getref (path.v[i])))
    {
      Tcl_AppendResult (interp, "snacc get: illegal component \"", path.v[i], "\" in path", NULL);
      return TCL_ERROR;
    }

  return var->TclGetVal (interp);
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
int SnaccTcl::test (int argc, char **argv)
{
  if (argc != 2)
  {
    strcpy (interp->result, "wrong # args: should be \"snacc get path\"");
    return TCL_ERROR;
  }

  Args path;
  if (Tcl_SplitList (interp, argv[1], &path.c, &path.v) != TCL_OK)
    return TCL_ERROR;

  if (path.c < 1)
  {
    strcpy (interp->result, "snacc get: wrong # args in path");
    return TCL_ERROR;
  }

  Tcl_HashEntry *entry = Tcl_FindHashEntry (&files, path.v[0]);
  if (!entry)
  {
    Tcl_AppendResult (interp, "snacc get: no file named \"", path.v[0], "\"", NULL);
    return TCL_ERROR;
  }

  ASN1File *file = (ASN1File *)Tcl_GetHashValue (entry);

  AsnType *var = (AsnType *)*file;
  for (int i=1; i<path.c; i++)
    if (!(var = var->_getref (path.v[i])))
    {
      Tcl_AppendResult (interp, "snacc test: illegal component \"", path.v[i], "\" in path", NULL);
      return TCL_ERROR;
    }

cout << *var;
  strstream s;
  s << *var;
  s.put ('\0');
  cout << strlen(s.str()) << endl;
  cout << s.str() << endl;

  return TCL_OK;
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
int SnaccTcl::setval (int argc, char **argv)
{
  if (argc != 3)
  {
    strcpy (interp->result, "wrong # args: should be \"snacc set path value\"");
    return TCL_ERROR;
  }

  Args path;
  if (Tcl_SplitList (interp, argv[1], &path.c, &path.v) != TCL_OK)
    return TCL_ERROR;

  if (path.c < 1)
  {
    strcpy (interp->result, "snacc set: wrong # args in path");
    return TCL_ERROR;
  }

  Tcl_HashEntry *entry = Tcl_FindHashEntry (&files, path.v[0]);
  if (!entry)
  {
    Tcl_AppendResult (interp, "snacc set: no file named \"", path.v[0], "\"", NULL);
    return TCL_ERROR;
  }

  ASN1File *file = (ASN1File *)Tcl_GetHashValue (entry);

  AsnType *var = (AsnType *)*file;
  for (int i=1; i<path.c; i++)
    if (!(var = var->_getref (path.v[i], true)))
    {
      Tcl_AppendResult (interp, "snacc set: illegal component \"", path.v[i], "\" in path", NULL);
      return TCL_ERROR;
    }

  return var->TclSetVal (interp, argv[2]);
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
int SnaccTcl::unsetval (int argc, char **argv)
{
  if (argc != 2)
  {
    strcpy (interp->result, "wrong # args: should be \"snacc unset path\"");
    return TCL_ERROR;
  }

  Args path;
  if (Tcl_SplitList (interp, argv[1], &path.c, &path.v) != TCL_OK)
    return TCL_ERROR;

  if (path.c == 1)
  {
    strcpy (interp->result, "snacc unset: sorry, but you are not allowed to unset the file itself");
    return TCL_ERROR;
  }
  else if (path.c < 1)
  {
    strcpy (interp->result, "snacc unset: wrong # args in path");
    return TCL_ERROR;
  }

  Tcl_HashEntry *entry = Tcl_FindHashEntry (&files, path.v[0]);
  if (!entry)
  {
    Tcl_AppendResult (interp, "snacc unset: no file named \"", path.v[0], "\"", NULL);
    return TCL_ERROR;
  }

  ASN1File *file = (ASN1File *)Tcl_GetHashValue (entry);

  AsnType *var = (AsnType *)*file;
  for (int i=1; i<path.c-1; i++)
  {
    if (!(var = var->_getref (path.v[i])))
    {
      Tcl_AppendResult (interp, "snacc unset: illegal component \"", path.v[i], "\" in path", NULL);
      return TCL_ERROR;
    }
  }

  return var->TclUnsetVal (interp, path.v[path.c-1]);
}

//\[sep]----------------------------------------------------------------------------------------------------------------------------
int Snacc_Cmd (ClientData cd, Tcl_Interp *interp, int argc, char **argv)
{
  SnaccTcl *ed = (SnaccTcl *)cd;

#ifdef DEBUG
  ed->ckip (interp);
#endif

  if (argc < 2)
  {
    strcpy (interp->result, "wrong # args: should be \"snacc option arg ?arg ...?\"");
    return TCL_ERROR;
  }
  --argc;
  argv++;

  switch (**argv)
  {
    case 'c':
      if (!strcmp (*argv, "close"))
	return ed->closefile (argc, argv);
      else if (!strcmp (*argv, "create"))
	return ed->create (argc, argv);
      break;
    case 'e':
      if (!strcmp (*argv, "export"))
	return export (interp, argc, argv);
      break;
    case 'f':
      if (!strcmp (*argv, "finfo"))
	return ed->finfo (argc, argv);
      break;
    case 'g':
      if (!strcmp (*argv, "get"))
	return ed->getval (argc, argv);
      break;
    case 'i':
      if (!strcmp (*argv, "import"))
	return import (interp, argc, argv);
      else if (!strcmp (*argv, "info"))
	return ed->info (argc, argv);
      break;
    case 'm':
      if (!strcmp (*argv, "modules"))
	return ed->modulesinfo (argc, argv);
      break;
    case 'o':
      if (!strcmp (*argv, "open"))
	return ed->openfile (argc, argv);
      break;
    case 'r':
      if (!strcmp (*argv, "read"))
	return ed->read (argc, argv);
      break;
    case 's':
      if (!strcmp (*argv, "set"))
	return ed->setval (argc, argv);
      break;
    case 't':
      if (!strcmp (*argv, "test"))
	return ed->test (argc, argv);
      else if (!strcmp (*argv, "type"))
	return ed->typeinfo (argc, argv);
      else if (!strcmp (*argv, "types"))
	return ed->typesinfo (argc, argv);
      break;
    case 'u':
      if (!strcmp (*argv, "unset"))
	return ed->unsetval (argc, argv);
      break;
    case 'w':
      if (!strcmp (*argv, "write"))
	return ed->write (argc, argv);
      break;
  }
  sprintf (interp->result, "bad command option %s: should be close, create, export, finfo, get, import, info, modules, open, read, set, type, types, unset or write", *argv);

  return TCL_ERROR;
}

//\[banner "check for proper initialization & finalization"]------------------------------------------------------------------------

struct check
{
  int	i, j;

	check (int);

  bool	bad();
};

static int cki;

check::check (int v)
{
  i = v;
  j = ~i;
}

#define CK	42

bool check::bad()
{
  return i != CK || j != ~CK;
}

check	check (CK);

//\[banner "initialization & finalization"]-----------------------------------------------------------------------------------------
void Snacc_Exit (ClientData data)
{
  delete (SnaccTcl *)data;
}

// prohibit function name mangling to enable tkAppInit.c:Tcl_AppInit() to call this function:
extern "C" int Snacc_Init (Tcl_Interp *interp)
{
  if (check.bad())
  {
    static const char emsg[] = "linkage error, constructors of static variables didn't get called!\n";
    write (2, emsg, sizeof emsg);
    exit (1);
  }

  SnaccTcl *data = new SnaccTcl (interp);
  Tcl_CreateCommand (interp, "snacc", Snacc_Cmd, (ClientData)data, Snacc_Exit);
  return TCL_OK;
}

#endif // TCL
