/*
 * tcldom.c --
 *
 *  Generic interface to DOM Implementation.
 *
 * Copyright (c) 2002 Steve Ball, Zveno Pty Ltd
 *
 * Zveno Pty Ltd makes this software and associated documentation
 * available free of charge for any purpose.  You may make copies
 * of the software but you must include all of this notice on any copy.
 *
 * Zveno Pty Ltd does not warrant that this software is error free
 * or fit for any purpose.  Zveno Pty Ltd disclaims any liability for
 * all claims, expenses, losses, damages and costs any user may incur
 * as a result of using, copying or modifying the software.
 *
 * $Id: tcldom.c,v 1.20 2003/03/09 11:12:41 balls Exp $
 *
 */

#include "tcldom.h"

#define TCL_DOES_STUBS \
    (TCL_MAJOR_VERSION > 8 || TCL_MAJOR_VERSION == 8 && (TCL_MINOR_VERSION > 1 || \
    (TCL_MINOR_VERSION == 1 && TCL_RELEASE_LEVEL == TCL_FINAL_RELEASE)))

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

/*
 * Declarations for externally visible functions.
 */

EXTERN int      Tcldom_Init _ANSI_ARGS_((Tcl_Interp *interp));

static Tcl_HashTable registeredImplementations;
static TclDOM_Implementation *defaultImpl = NULL;

/*
 * Prototypes for procedures defined later in this file:
 */

static int  TclDOMRegisterCmd _ANSI_ARGS_((ClientData dummy,
            Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
static int  TclDOMDOMImplementationCmd _ANSI_ARGS_((ClientData dummy,
            Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
static int  TclDOMDocumentCmd _ANSI_ARGS_((ClientData dummy,
            Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
static int  TclDOMDocumentFragmentCmd _ANSI_ARGS_((ClientData dummy,
            Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
static int  TclDOMNodeCmd _ANSI_ARGS_((ClientData dummy,
            Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
static int  TclDOMElementCmd _ANSI_ARGS_((ClientData dummy,
            Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));

/*
 * Method tables for commands
 */

#ifndef CONST84
#define CONST84 /* Before 8.4 no 'const' required */
#endif

static CONST84 char *RegisterCommandMethods[] = {
  "list",
  "info",
  (char *) NULL
};
enum RegisterCommandMethods {
  TCLDOM_REGISTER_LIST,
  TCLDOM_REGISTER_INFO
};

static CONST84 char *DOMImplementationCommandMethods[] = {
  "hasFeature",
  "createDocument",
  "create",
  "createDocumentType",
  "createNode",
  "destroy",
  "isNode",
  "parse",
  "selectNode",
  "serialize",
  "trim",
  (char *) NULL
};
enum DOMImplementationCommandMethods {
  TCLDOM_IMPL_HASFEATURE,
  TCLDOM_IMPL_CREATEDOCUMENT,
  TCLDOM_IMPL_CREATE,
  TCLDOM_IMPL_CREATEDOCUMENTTYPE,
  TCLDOM_IMPL_CREATENODE,
  TCLDOM_IMPL_DESTROY,
  TCLDOM_IMPL_ISNODE,
  TCLDOM_IMPL_PARSE,
  TCLDOM_IMPL_SELECTNODE,
  TCLDOM_IMPL_SERIALIZE,
  TCLDOM_IMPL_TRIM
};

/*
 *----------------------------------------------------------------------------
 *
 * Tcldom_Init --
 *
 *  Initialisation routine for loadable module
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Creates commands in the interpreter,
 *  loads dom package.
 *
 *----------------------------------------------------------------------------
 */

int
Tcldom_Init (interp)
      Tcl_Interp *interp; /* Interpreter to initialise. */
{
  extern TcldomStubs tcldomStubs;
  Tcl_Obj *objPtr;

#ifdef USE_TCL_STUBS
  if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
    return TCL_ERROR;
  }
#endif

  Tcl_InitHashTable(&registeredImplementations, TCL_ONE_WORD_KEYS);

  Tcl_CreateObjCommand(interp, "dom::register", TclDOMRegisterCmd, NULL, NULL);

  Tcl_CreateObjCommand(interp, "dom::DOMImplementation", TclDOMDOMImplementationCmd, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::document", TclDOMDocumentCmd, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::documentfragment", TclDOMDocumentFragmentCmd, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::node", TclDOMNodeCmd, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::element", TclDOMElementCmd, NULL, NULL);

  Tcl_SetVar(interp, "::dom::strictDOM", "0", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "::dom::maxSpecials", "10", TCL_GLOBAL_ONLY);
  objPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, objPtr, Tcl_NewIntObj(2));
  Tcl_ListObjAppendElement(interp, objPtr, Tcl_NewStringObj("        ", -1));
  Tcl_ListObjAppendElement(interp, objPtr, Tcl_NewStringObj("\t", -1));
  Tcl_SetVar2Ex(interp, "::dom::indentspec", NULL, objPtr, TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "::dom::xmlnsURI", "http://www.w3.org/2000/xmlns/", TCL_GLOBAL_ONLY);

  #if TCL_DOES_STUBS
    {
      extern TcldomStubs tcldomStubs;
      if (Tcl_PkgProvideEx(interp, "dom::generic", TCLDOM_VERSION,
	(ClientData) &tcldomStubs) != TCL_OK) {
        return TCL_ERROR;
      }
    }
  #else
    if (Tcl_PkgProvide(interp, "dom::generic", TCLDOM_VERSION) != TCL_OK) {
      return TCL_ERROR;
    }
  #endif

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOM_Register --
 *
 *  Adds a new DOM implementation.
 *
 * Results:
 *  Standard Tcl return code.
 *
 * Side effects:
 *  New implementation is available for use in dom commands.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOM_Register (interp, impl)
      Tcl_Interp *interp; /* Interpreter to initialise. */
      TclDOM_Implementation *impl; /* Implementation details */
{
  int new;
  Tcl_HashEntry *entryPtr;

  if (impl == NULL) {
    Tcl_SetResult(interp, "invalid implementation", NULL);
    return TCL_ERROR;
  }
  if (impl->type == NULL) {
    Tcl_SetResult(interp, "invalid implementation type", NULL);
    return TCL_ERROR;
  }
  if (impl->name == NULL) {
    Tcl_SetResult(interp, "invalid implementation name", NULL);
    return TCL_ERROR;
  }

  entryPtr = Tcl_CreateHashEntry(&registeredImplementations, (char *)impl->type, &new);
  if (!new) {
    Tcl_AppendResult(interp, "implementation \"", impl->name, "\" already registered", NULL);
    return TCL_ERROR;
  }

  Tcl_SetHashValue(entryPtr, (ClientData) impl);

  /*
   * Set default implementation - last wins
   */

  defaultImpl = impl;

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMRegisterCmd --
 *
 *  Implements dom::register command.
 *
 * Results:
 *  Depends on the method.
 *
 * Side effects:
 *  Depends on the method.
 *
 *----------------------------------------------------------------------------
 */

static int
TclDOMRegisterCmd(dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int method;
  Tcl_HashSearch search;
  Tcl_HashEntry *entry;
  Tcl_Obj *result;
  TclDOM_Implementation *impl;

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "method ?args?");
    return TCL_ERROR;
  }

  if (Tcl_GetIndexFromObj(interp, objv[1], RegisterCommandMethods,
			  "method", 0, &method) != TCL_OK) {
    return TCL_ERROR;
  }

  switch ((enum RegisterCommandMethods) method) {
  case TCLDOM_REGISTER_LIST:
    result = Tcl_NewObj();
    entry = Tcl_FirstHashEntry(&registeredImplementations, &search);
    while (entry) {
      impl = (TclDOM_Implementation *) Tcl_GetHashValue(entry);
      Tcl_ListObjAppendElement(interp, result, impl->name);
      entry = Tcl_NextHashEntry(&search);
    }

    Tcl_SetObjResult(interp, result);

    break;

  case TCLDOM_REGISTER_INFO:
    if (objc != 3) {
      Tcl_WrongNumArgs(interp, 1, objv, "info node");
      return TCL_ERROR;
    }

    entry = Tcl_FindHashEntry(&registeredImplementations, (char *)objv[2]->typePtr);

    if (entry) {
      impl = (TclDOM_Implementation *) Tcl_GetHashValue(entry);
      Tcl_SetObjResult(interp, impl->name);
    } /* else return empty result */

    break;

  default:
    Tcl_SetResult(interp, "unknown method", NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMDOMImplementationCmd --
 *
 *  dom::DOMImplementation command.
 *
 * Results:
 *  Depends on the method.
 *
 * Side effects:
 *  Depends on the method.
 *
 *----------------------------------------------------------------------------
 */

static int
TclDOMDOMImplementationCmd(dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int method;
  Tcl_Obj *namePtr;
  Tcl_Obj *newcmd;
  Tcl_Obj *newobjv[20];

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "method ?args?");
    return TCL_ERROR;
  }

  if (Tcl_GetIndexFromObj(interp, objv[1], DOMImplementationCommandMethods,
			  "method", 0, &method) != TCL_OK) {
    return TCL_ERROR;
  }

  switch ((enum DOMImplementationCommandMethods) method) {
  case TCLDOM_IMPL_HASFEATURE:
    Tcl_SetResult(interp, "use implementation command", NULL);
    return TCL_ERROR;

  case TCLDOM_IMPL_CREATEDOCUMENT:
    Tcl_SetResult(interp, "not yet implemented - use implementation command", NULL);
    break;

  case TCLDOM_IMPL_CREATE:

    if (objc == 2) {

      /* Create document with no document element */

      namePtr = NULL;

    } else if (Tcl_GetStringFromObj(objv[2], NULL)[0] != '-') {

      /* Create document with given document element*/

      /*
       * Need to construct a new argument list to do this properly

      namePtr = objv[2];

      for (i = 2; i < objc; i++) {
	objv[i] = objv[i + 1];
      }
      */

      Tcl_SetResult(interp, "create document first, then add document element", NULL);

    } else {
      Tcl_WrongNumArgs(interp, 1, objv, "create ?doc?");
      return TCL_ERROR;
    }

    if (defaultImpl && defaultImpl->create) {
      if (((defaultImpl->create)(NULL, interp, 1, objv)) != TCL_OK) 
	return TCL_ERROR;
    } else {

      /* Fallback to Tcl implementation */

      /* The code below takes the existing command information and replaces
       * the command word with the name of the tcl command for creating the DOM.
       */

      newobjv[0] = Tcl_NewStringObj("dom::tcl::create", -1);
      newcmd     = Tcl_NewListObj(objc, objv);
      if (Tcl_ListObjReplace(interp, newcmd, 0, 2, 1, newobjv) != TCL_OK) {
	return TCL_ERROR;
      }
      if (Tcl_GlobalEvalObj(interp, newcmd) != TCL_OK) {
	return TCL_ERROR;
      }
    }

    if (namePtr) {
      newcmd = Tcl_NewListObj(0, NULL);

      Tcl_ListObjAppendElement(interp, newcmd, 
			       Tcl_NewStringObj("::dom::document", -1));
      Tcl_ListObjAppendElement(interp, newcmd,
			       Tcl_NewStringObj("createElement", -1));
      Tcl_ListObjAppendElement(interp, newcmd,
			       Tcl_GetObjResult(interp));
      Tcl_ListObjAppendElement(interp, newcmd, namePtr);

      return Tcl_GlobalEvalObj(interp, newcmd);
    }

    break;

  case TCLDOM_IMPL_CREATEDOCUMENTTYPE:
    Tcl_SetResult(interp, "not yet implemented - use implementation command", NULL);
    return TCL_ERROR;

  case TCLDOM_IMPL_CREATENODE:

    /*
     * Use Tcl implementation.
     * It only makes generic calls.
     */

    newcmd = Tcl_NewListObj(objc, objv);
    newobjv[0] = Tcl_NewStringObj("dom::tcl::DOMImplementation", -1);
    Tcl_ListObjReplace(interp, newcmd, 0, 1, 1, newobjv);

    return Tcl_GlobalEvalObj(interp, newcmd);

  case TCLDOM_IMPL_ISNODE:
    Tcl_SetResult(interp, "not yet implemented - use implementation command", NULL);
    break;

  case TCLDOM_IMPL_PARSE:

    if (objc < 3) {
      Tcl_WrongNumArgs(interp, 2, objv, "parse xml ?options?");
      return TCL_ERROR;
    }

    if (defaultImpl && defaultImpl->parse) {
      if (((defaultImpl->parse)(NULL, interp, objc - 1, objv + 1)) != TCL_OK) 
	return TCL_ERROR;
    } else {

      /* Fallback to Tcl implementation */

      newobjv[0] = Tcl_NewStringObj("dom::tcl::parse", -1);
      newobjv[1] = NULL;
      newcmd = Tcl_NewListObj(objc, objv);
      if (Tcl_ListObjReplace(interp, newcmd, 0, 2, 1, newobjv) != TCL_OK)
	return TCL_ERROR;
      if (Tcl_GlobalEvalObj(interp, newcmd) != TCL_OK)
	return TCL_ERROR;
    }

    break;

  case TCLDOM_IMPL_SELECTNODE:
    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 1, objv, "selectNode doc xpath");
      return TCL_ERROR;
    }

    if (defaultImpl && defaultImpl->select) {
      if (((defaultImpl->select)(NULL, interp, objc - 1, objv + 1)) != TCL_OK)
	return TCL_ERROR;
    } else {

      /* Fallback to Tcl implementation */

      newobjv[0] = Tcl_NewStringObj("dom::tcl::DOMImplementation", -1);
      newobjv[1] = NULL;
      newcmd = Tcl_NewListObj(objc, objv);
      if (Tcl_ListObjReplace(interp, newcmd, 0, 2, 1, newobjv) != TCL_OK)
	return TCL_ERROR;
      if (Tcl_GlobalEvalObj(interp, newcmd) != TCL_OK)
	return TCL_ERROR;
    }

    break;

  case TCLDOM_IMPL_SERIALIZE:

    if (objc < 3) {
      Tcl_WrongNumArgs(interp, 2, objv, "serialize doc ?options?");
      return TCL_ERROR;
    }

    if (defaultImpl && defaultImpl->serialize) {
      if (((defaultImpl->serialize)(NULL, interp, objc - 1, objv + 1)) != TCL_OK) 
	return TCL_ERROR;
    } else {

      /* Fallback to Tcl implementation */

      newobjv[0] = Tcl_NewStringObj("dom::tcl::serialize", -1);
      newobjv[1] = NULL;
      newcmd = Tcl_NewListObj(objc, objv);
      if (Tcl_ListObjReplace(interp, newcmd, 0, 2, 1, newobjv) != TCL_OK)
	return TCL_ERROR;
      if (Tcl_GlobalEvalObj(interp, newcmd) != TCL_OK)
	return TCL_ERROR;
    }

    break;

  case TCLDOM_IMPL_TRIM:
    Tcl_SetResult(interp, "not yet implemented - use implementation command", NULL);
    break;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMDocumentCmd --
 *
 *  dom::document command.
 *
 * Results:
 *  Depends on the method.
 *
 * Side effects:
 *  Depends on the method.
 *
 *----------------------------------------------------------------------------
 */

static int
TclDOMDocumentCmd(dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  Tcl_HashEntry *entryPtr;
  TclDOM_Implementation *implPtr;
  Tcl_Obj *newcmd;
  Tcl_Obj *newobjv[2];

  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 2, objv, "method token ?args?");
    return TCL_ERROR;
  }

  /*
   * See if the internal rep of the token matches
   * a registered implementation.
   */

  entryPtr = Tcl_FindHashEntry(&registeredImplementations, (char *)objv[2]->typePtr);

  if (entryPtr) {

    implPtr = (TclDOM_Implementation *) Tcl_GetHashValue(entryPtr);
    if (implPtr->document) {
      return (implPtr->document)(dummy, interp, objc, objv);
    }
  }

  /*
   * No DOM implementations match this object type,
   * so fallback to default implementation
   */

  if (defaultImpl && defaultImpl->document) {
    return ((defaultImpl->document)(NULL, interp, objc, objv));
  }

  /*
   * Finally, fallback to Tcl implementation.
   */

  newcmd = Tcl_NewListObj(objc, objv);
  newobjv[0] = Tcl_NewStringObj("dom::tcl::document", -1);
  newobjv[1] = NULL;
  if (Tcl_ListObjReplace(interp, newcmd, 0, 1, 1, newobjv) != TCL_OK)
    return TCL_ERROR;

  return Tcl_GlobalEvalObj(interp, newcmd);
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMDocumentFragmentCmd --
 *
 *  dom::documentfragment command.
 *
 * Results:
 *  Depends on the method.
 *
 * Side effects:
 *  Depends on the method.
 *
 *----------------------------------------------------------------------------
 */

static int
TclDOMDocumentFragmentCmd(dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  Tcl_HashEntry *entryPtr;
  TclDOM_Implementation *implPtr;
  Tcl_Obj *newcmd;
  Tcl_Obj *newobjv[2];

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 2, objv, "method token ?args?");
    return TCL_ERROR;
  }

  /*
   * See if the internal rep of the token matches
   * a registered implementation.
   */

  entryPtr = Tcl_FindHashEntry(&registeredImplementations, (char *)objv[2]->typePtr);

  if (entryPtr) {

    implPtr = (TclDOM_Implementation *) Tcl_GetHashValue(entryPtr);
    if (implPtr->documentfragment) {
      return (implPtr->documentfragment)(dummy, interp, objc, objv);
    }
  }

  /*
   * No DOM implementations match this object type,
   * so fallback to default implementation
   */

  if (defaultImpl && defaultImpl->documentfragment) {
    return ((defaultImpl->documentfragment)(NULL, interp, objc, objv));
  }

  /*
   * Finally, fallback to Tcl implementation.
   */

  newcmd = Tcl_NewListObj(objc, objv);
  newobjv[0] = Tcl_NewStringObj("dom::tcl::documentfragment", -1);
  newobjv[1] = NULL;
  if (Tcl_ListObjReplace(interp, newcmd, 0, 1, 1, newobjv) != TCL_OK)
    return TCL_ERROR;

  return Tcl_GlobalEvalObj(interp, newcmd);
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMNodeCmd --
 *
 *  dom::node command.
 *
 * Results:
 *  Depends on the method.
 *
 * Side effects:
 *  Depends on the method.
 *
 *----------------------------------------------------------------------------
 */

static int
TclDOMNodeCmd(dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  Tcl_HashEntry *entryPtr;
  TclDOM_Implementation *implPtr;
  Tcl_Obj *newcmd;
  Tcl_Obj *newobjv[2];

  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 2, objv, "method token ?args?");
    return TCL_ERROR;
  }

  /*
   * See if the internal rep of the token matches
   * a registered implementation.
   */

  entryPtr = Tcl_FindHashEntry(&registeredImplementations, (char *)objv[2]->typePtr);

  if (entryPtr) {

    implPtr = (TclDOM_Implementation *) Tcl_GetHashValue(entryPtr);
    if (implPtr->node) {
      return (implPtr->node)(dummy, interp, objc, objv);
    }
  }

  /*
   * No DOM implementations match this object type,
   * so fallback to default implementation
   */

  if (defaultImpl && defaultImpl->node) {
    return ((defaultImpl->node)(NULL, interp, objc, objv));
  }

  /*
   * Finally, fallback to Tcl implementation.
   */

  newcmd = Tcl_NewListObj(objc, objv);
  newobjv[0] = Tcl_NewStringObj("dom::tcl::node", -1);
  newobjv[1] = NULL;
  if (Tcl_ListObjReplace(interp, newcmd, 0, 1, 1, newobjv) != TCL_OK)
    return TCL_ERROR;

  return Tcl_GlobalEvalObj(interp, newcmd);
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMElementCmd --
 *
 *  dom::element command.
 *
 * Results:
 *  Depends on the method.
 *
 * Side effects:
 *  Depends on the method.
 *
 *----------------------------------------------------------------------------
 */

static int
TclDOMElementCmd(dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  Tcl_HashEntry *entryPtr;
  TclDOM_Implementation *implPtr;
  Tcl_Obj *newcmd;
  Tcl_Obj *newobjv[2];

  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 2, objv, "method token ?args?");
    return TCL_ERROR;
  }

  /*
   * See if the internal rep of the token matches
   * a registered implementation.
   */

  entryPtr = Tcl_FindHashEntry(&registeredImplementations, (char *)objv[2]->typePtr);

  if (entryPtr) {

    implPtr = (TclDOM_Implementation *) Tcl_GetHashValue(entryPtr);
    if (implPtr->element) {
      return (implPtr->element)(dummy, interp, objc, objv);
    }
  }

  /*
   * No DOM implementations match this object type,
   * so fallback to default implementation
   */

  if (defaultImpl && defaultImpl->element) {
    return ((defaultImpl->element)(NULL, interp, objc, objv));
  }

  /*
   * Finally, fallback to Tcl implementation.
   */

  newcmd = Tcl_NewListObj(objc, objv);
  newobjv[0] = Tcl_NewStringObj("dom::tcl::element", -1);
  newobjv[1] = NULL;
  if (Tcl_ListObjReplace(interp, newcmd, 0, 1, 1, newobjv) != TCL_OK)
    return TCL_ERROR;

  return Tcl_GlobalEvalObj(interp, newcmd);
}

