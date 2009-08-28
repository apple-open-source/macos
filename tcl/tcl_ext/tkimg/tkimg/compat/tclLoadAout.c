/*
 * tclLoadAout.c --
 *
 *	This procedure provides a version of dlopen() that
 *	provides pseudo-static linking using version-7 compatible
 *	a.out files described in either sys/exec.h or sys/a.out.h.
 *
 * Copyright (c) 1995, by General Electric Company. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * This work was supported in part by the ARPA Manufacturing Automation
 * and Design Engineering (MADE) Initiative through ARPA contract
 * F33615-94-C-4400.
 *
 * SCCS: @(#) tclLoadAout.c 1.7 96/02/15 11:58:53
 */

#include "tcl.h"
#include "compat/dlfcn.h"
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_EXEC_AOUT_H
#   include <sys/exec_aout.h>
#endif

/*
 * Some systems describe the a.out header in sys/exec.h, and some in
 * a.out.h.
 */

#ifdef USE_SYS_EXEC_H
#include <sys/exec.h>
#endif
#ifdef USE_A_OUT_H
#include <a.out.h>
#endif
#ifdef USE_SYS_EXEC_AOUT_H
#include <sys/exec_aout.h>
#define a_magic a_midmag
#endif

EXTERN char *tclExecutableName;

#define UCHAR(c) ((unsigned char) (c))

/*
 * TCL_LOADSHIM is the amount by which to shim the break when loading
 */

#ifndef TCL_LOADSHIM
#define TCL_LOADSHIM 0x4000L
#endif

/*
 * TCL_LOADALIGN must be a power of 2, and is the alignment to which
 * to force the origin of load modules
 */

#ifndef TCL_LOADALIGN
#define TCL_LOADALIGN 0x4000L
#endif

/*
 * TCL_LOADMAX is the maximum size of a load module, and is used as
 * a sanity check when loading
 */

#ifndef TCL_LOADMAX
#define TCL_LOADMAX 2000000L
#endif

/*
 * Kernel calls that appear to be missing from the system .h files:
 */

extern char * brk _ANSI_ARGS_((char *));
extern char * sbrk _ANSI_ARGS_((size_t));

/*
 * The static variable SymbolTableFile contains the file name where the
 * result of the last link was stored.  The file is kept because doing so
 * allows one load module to use the symbols defined in another.
 */

static char * SymbolTableFile = NULL;

/*
 * Prototypes for procedures referenced only in this file:
 */

static int FindLibraries _ANSI_ARGS_((const char *fileName, Tcl_DString *buf));
static void UnlinkSymbolTable _ANSI_ARGS_((void));
static void Seterror _ANSI_ARGS_((char *message));
static char *errorMessage = NULL;


/*
 *----------------------------------------------------------------------
 *
 * dlopen --
 *
 *	Dynamically loads a binary code file into memory.
 *
 * Results:
 *	A handle which can be used in later calls to dlsym(),
 *	or NULL when the attempt fails.
 *
 * Side effects:
 *	New code suddenly appears in memory.
 *
 *
 * Bugs:
 *	This function does not attempt to handle the case where the
 *	BSS segment is not executable.  It will therefore fail on
 *	Encore Multimax, Pyramid 90x, and similar machines.  The
 *	reason is that the mprotect() kernel call, which would
 *	otherwise be employed to mark the newly-loaded text segment
 *	executable, results in a system crash on BSD/386.
 *
 *	In an effort to make it fast, this function eschews the
 *	technique of linking the load module once, reading its header
 *	to determine its size, allocating memory for it, and linking
 *	it again.  Instead, it `shims out' memory allocation by
 *	placing the module TCL_LOADSHIM bytes beyond the break,
 *	and assuming that any malloc() calls required to run the
 *	linker will not advance the break beyond that point.  If
 *	the break is advanced beyonnd that point, the load will
 *	fail with an `inconsistent memory allocation' error.
 *	It perhaps ought to retry the link, but the failure has
 *	not been observed in two years of daily use of this function.
 *----------------------------------------------------------------------
 */

VOID *
dlopen(path, flags)
    const char *path;
    int flags;
{
  char * inputSymbolTable;	/* Name of the file containing the
				 * symbol table from the last link. */
  Tcl_DString linkCommandBuf;	/* Command to do the run-time relocation
				 * of the module.*/
  char * linkCommand;
  char relocatedFileName [L_tmpnam];
				/* Name of the file holding the relocated */
				/* text of the module */
  int relocatedFd = -1;		/* File descriptor of the file holding
				 * relocated text */
  struct exec relocatedHead;	/* Header of the relocated text */
  unsigned long relocatedSize;	/* Size of the relocated text */
  char * startAddress;		/* Starting address of the module */
  int status;			/* Status return from Tcl_ calls */
  char *p, *q;
  const char *r, *pkgGuess;
  Tcl_Interp *interp = NULL;
  Tcl_DString fullPath;

  errno = 0;
  if (errorMessage) {
    ckfree(errorMessage);
    errorMessage = NULL;
  }

  /* Find the file that contains the symbols for the run-time link. */

  if (SymbolTableFile != NULL) {
    inputSymbolTable = SymbolTableFile;
  } else if (tclExecutableName == NULL) {
    Seterror("can't find the tclsh executable");
    goto error;
  } else {
    inputSymbolTable = tclExecutableName;
  }

  /* Construct the `ld' command that builds the relocated module */

  interp = Tcl_CreateInterp();

  Tcl_DStringInit (&fullPath);
  if (Tcl_GetPathType(path) == TCL_PATH_RELATIVE) {
	p = getenv("LD_LIBRARY_PATH");
	while (p) {
	    if ((q = strchr(p,':')) == NULL) {
		q = p; while(*q) q++;
	    }
	    if (p == q) break;
	    Tcl_DStringAppend(&fullPath, p, q-p);
	    Tcl_DStringAppend(&fullPath, "/", 1);
	    Tcl_DStringAppend(&fullPath, path, -1);
	    if (access(Tcl_DStringValue(&fullPath), F_OK) != -1) {
		break;
	    }
	    Tcl_DStringSetLength(&fullPath, 0);
	    p = q; if (*p) p++;
	}
  }
  if (*Tcl_DStringValue(&fullPath) == 0) {
    Tcl_DStringAppend(&fullPath, path, -1);
  }
  tmpnam (relocatedFileName);
  Tcl_DStringInit (&linkCommandBuf);
  Tcl_DStringAppend (&linkCommandBuf, "exec ld -o ", -1);
  Tcl_DStringAppend (&linkCommandBuf, relocatedFileName, -1);
#if defined(__mips) || defined(mips)
  Tcl_DStringAppend (&linkCommandBuf, " -G 0 ", -1);
#endif
  Tcl_DStringAppend (&linkCommandBuf, " -u TclLoadDictionary_", -1);
  if (pkgGuess = strrchr(path,'/')) {
    pkgGuess++;
  } else {
    pkgGuess = path;
  }
  if (!strncmp(pkgGuess,"lib",3)) {
    pkgGuess+=3;
  }
  for (r = pkgGuess; (*r) && (*r != '.'); r++) {
    /* Empty loop body. */
  }
  if ((r>pkgGuess+3) && !strncmp(r-3,"_G0.",4)) {
    r-=3;
  }
  while ((r-- > pkgGuess) && isdigit(UCHAR(*r))) {
    /* Empty loop body. */
  }
  r++;
  Tcl_DStringAppend(&linkCommandBuf,(char *) pkgGuess, r-pkgGuess);

  p = Tcl_DStringValue(&linkCommandBuf);
  p += strlen(p) - (r-pkgGuess);

  if (islower(UCHAR(*p))) {
    *p = (char) toupper(UCHAR(*p));
  }
  while (*(p++)) {
    if (isupper(UCHAR(*p))) {
	*p = (char) tolower(UCHAR(*p));
    }
  }
  Tcl_DStringAppend (&linkCommandBuf, " -A ", -1);
  Tcl_DStringAppend (&linkCommandBuf, inputSymbolTable, -1);
  Tcl_DStringAppend (&linkCommandBuf, " -N -T XXXXXXXX ", -1);
  Tcl_DStringAppend (&linkCommandBuf, Tcl_DStringValue(&fullPath), -1);
  p = getenv("LD_LIBRARY_PATH");
  while (p) {
    if ((q = strchr(p,':')) == NULL) {
	q = p; while(*q) q++;
    }
    if (p == q) break;
    Tcl_DStringAppend(&linkCommandBuf, " -L", 3);
    Tcl_DStringAppend(&linkCommandBuf, p, q-p);
    p = q; if (*p) p++;
  }
  Tcl_DStringAppend (&linkCommandBuf, " ", -1);
  if (FindLibraries (Tcl_DStringValue(&fullPath), &linkCommandBuf) != TCL_OK) {
    Tcl_DStringFree (&linkCommandBuf);
    Tcl_DStringFree (&fullPath);
    goto error;
  }
  Tcl_DStringFree (&fullPath);
  linkCommand = Tcl_DStringValue (&linkCommandBuf);

  /* Determine the starting address, and plug it into the command */

  startAddress = (char *) (((unsigned long) sbrk (0)
			    + TCL_LOADSHIM + TCL_LOADALIGN - 1)
			   & (- TCL_LOADALIGN));
  p = strstr (linkCommand, "-T") + 3;
  sprintf (p, "%08lx", (long) startAddress);
  p [8] = ' ';

  /* Run the linker */

  status = Tcl_Eval (interp, linkCommand);
  Tcl_DStringFree (&linkCommandBuf);
  if (status != 0) {
    Seterror(interp->result);
    errno = 0;
    goto error;
  }

  /* Open the linker's result file and read the header */

  relocatedFd = open (relocatedFileName, O_RDONLY);
  if (relocatedFd < 0) {
    goto ioError;
  }
  status= read (relocatedFd, (char *) & relocatedHead, sizeof relocatedHead);
  if (status < sizeof relocatedHead) {
    goto ioError;
  }

  /* Check the magic number */

  if (relocatedHead.a_magic != OMAGIC) {
    Seterror("bad magic number in intermediate file");
    goto failure;
  }

  /* Make sure that memory allocation is still consistent */

  if ((unsigned long) sbrk (0) > (unsigned long) startAddress) {
    Seterror("can't load, memory allocation is inconsistent");
    goto failure;
  }

  /* Make sure that the relocated module's size is reasonable */

  relocatedSize = relocatedHead.a_text + relocatedHead.a_data
    + relocatedHead.a_bss;
  if (relocatedSize > TCL_LOADMAX) {
    Seterror("module too big to load");
    goto failure;
  }

  /* Advance the break to protect the loaded module */

  (void) brk (startAddress + relocatedSize);

  /* Seek to the start of the module's text */

#if defined(__mips) || defined(mips)
  status = lseek (relocatedFd,
		  N_TXTOFF (relocatedHead.ex_f, relocatedHead.ex_o),
		  SEEK_SET);
#else
  status = lseek (relocatedFd, N_TXTOFF (relocatedHead), SEEK_SET);
#endif
  if (status < 0) {
    goto ioError;
  }

  /* Read in the module's text and data */

  relocatedSize = relocatedHead.a_text + relocatedHead.a_data;
  if (read (relocatedFd, startAddress, relocatedSize) < relocatedSize) {
    brk (startAddress);
  ioError:
    Seterror("error on intermediate file: ");
  failure:
    (void) unlink (relocatedFileName);
    goto error;
  }

  /* Close the intermediate file. */

  (void) close (relocatedFd);

  /* Arrange things so that intermediate symbol tables eventually get
   * deleted. If the flag RTLD_GLOBAL is not set, just keep the
   * old file. */

  if (flags & RTLD_GLOBAL) {
    if (SymbolTableFile != NULL) {
      UnlinkSymbolTable ();
    } else {
      atexit (UnlinkSymbolTable);
    }
    SymbolTableFile = ckalloc (strlen (relocatedFileName) + 1);
    strcpy (SymbolTableFile, relocatedFileName);
  } else {
    (void) unlink (relocatedFileName);
  }
  return (VOID *) startAddress;

error:
  if (relocatedFd>=0) {
	close (relocatedFd);
  }
  if (interp) {
    Tcl_DeleteInterp(interp);
  }
  return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * dlsym --
 *
 *	This function returns the address of a
 *	symbol, give the handle returned by dlopen().
 *
 * Results:
 *	Returns the address of the symbol in the dll.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

VOID *dlsym(handle, symbol)
    VOID *handle;
    const char *symbol;
{
    if ((handle != NULL) && (symbol != NULL)) {
	return ((VOID * (*) _ANSI_ARGS_((const char *))) handle) (symbol);
    } else {
	return (VOID *) NULL;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * dlerror --
 *
 *	This function returns a string describing the error which
 *	occurred in dlopen().
 *
 * Results:
 *	Returns an error message.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
dlerror()
{
    char *err, *msg;

    if (errorMessage && errno) {
	err = Tcl_ErrnoMsg(errno);
	msg = ckalloc(strlen(errorMessage)+strlen(err)+1);
	strcpy(msg, errorMessage);
	strcat(msg, err);
	ckfree(errorMessage);
	errorMessage = msg;
    }
    return errorMessage;
}


/*
 *----------------------------------------------------------------------
 *
 * dlclose --
 *
 *	Just a dummy function, only for compatibility. There is no
 *	way to remove dll's from memory.
 *
 * Results:
 *	Always returns 0 (= O.K.)
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

int
dlclose(handle)
    VOID *handle;
{
    return 0;
}

static void
Seterror(message)
    char *message;
{
    if (errorMessage) {
	ckfree(errorMessage);
    }
    errorMessage = ckalloc(strlen(message)+1);
    strcpy(errorMessage, message);
    return;
}


/*
 *------------------------------------------------------------------------
 *
 * FindLibraries --
 *
 *	Find the libraries needed to link a load module at run time.
 *
 * Results:
 *	A standard Tcl completion code.  If an error occurs,
 *	an error message is left in interp->result.  The -l and -L flags
 *	are concatenated onto the dynamic string `buf'.
 *
 *------------------------------------------------------------------------
 */

static int
FindLibraries (fileName, buf)
     const char * fileName;	/* Name of the load module */
     Tcl_DString * buf;		/* Buffer where the -l an -L flags */
{
  FILE * f;			/* The load module */
  int c = EOF;			/* Byte from the load module */
  char * p;

  /* Open the load module */

  if ((f = fopen (fileName, "rb")) == NULL) {
    Seterror("");
    return TCL_ERROR;
  }

  /* Search for the library list in the load module */

  p = "@LIBS: ";
  while (*p != '\0' && (c = getc (f)) != EOF) {
    if (c == *p) {
      ++p;
    }
    else {
      p = "@LIBS: ";
      if (c == *p) {
	++p;
      }
    }
  }

  /* No library list -- assume no dependancies */

  if (c == EOF) {
    (void) fclose (f);
    return TCL_OK;
  }

  /* Accumulate the library list */

  while ((c = getc (f)) != '\0' && c != EOF) {
    char cc = c;
    Tcl_DStringAppend (buf, &cc, 1);
  }
  (void) fclose (f);

  if (c == EOF) {
    Seterror("Library directory ends prematurely");
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * UnlinkSymbolTable --
 *
 *	Remove the symbol table file from the last dynamic link.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The symbol table file from the last dynamic link is removed.
 *	This function is called when (a) a new symbol table is present
 *	because another dynamic link is complete, or (b) the process
 *	is exiting.
 *------------------------------------------------------------------------
 */

static void
UnlinkSymbolTable ()
{
  (void) unlink (SymbolTableFile);
  ckfree (SymbolTableFile);
  SymbolTableFile = NULL;
}
