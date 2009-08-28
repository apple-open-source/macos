/*
 * tclexpat.c --
 *
 *	A Tcl interface to James Clark's expat XML parser
 *
 * Copyright (c) 1998 Steve Ball, Zveno Pty Ltd
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
 * $Id: tclexpat.c,v 1.1 2004/05/23 22:50:39 neumann Exp $
 *
 */

#include <tcl.h>
#include <xotcl.h>
#include <string.h>
#include "xmlparse.h"


/*
 * The structure below is used to refer to an expat parser object.
 */

typedef struct TclExpatInfo {
  XML_Parser parser;		/* The expat parser structure */
  Tcl_Interp *interp;		/* Interpreter for this instance */
  Tcl_Obj *name;		/* name of this instance */

  int final;			/* input data complete? */

  int status;			/* application status */
  Tcl_Obj *result;		/* application return result */

  int continueCount;		/* reference count for continue */

  Tcl_Obj *elementstartcommand;	/* Script for element start */
  Tcl_Obj *elementendcommand;	/* Script for element end */
  Tcl_Obj *datacommand;		/* Script for character data */
  Tcl_Obj *picommand;		/* Script for processing instruction */
  Tcl_Obj *defaultcommand;	/* Script for default data */
  Tcl_Obj *unparsedcommand;	/* Script for unparsed entity declaration */
  Tcl_Obj *notationcommand;	/* Script for notation declaration */
  Tcl_Obj *externalentitycommand;	/* Script for external entity */
  Tcl_Obj *unknownencodingcommand;	/* Script for unknown character encoding */

} TclExpatInfo;

/*
 * Prototypes for procedures defined later in this file:
 */

static Tcl_ObjCmdProc TclExpatObjCmd;
static Tcl_ObjCmdProc TclExpatInstanceCmd;
static Tcl_CmdDeleteProc TclExpatDeleteCmd;
static int (TclExpatCreateParser) _ANSI_ARGS_((Tcl_Interp *interp,
					       TclExpatInfo *expat));
static void (TclExpatFreeParser)  _ANSI_ARGS_((TclExpatInfo *expat));
static int (TclExpatParse) _ANSI_ARGS_((Tcl_Interp *interp,
					TclExpatInfo *expat,
					char *data,
					size_t len));
static int (TclExpatConfigure) _ANSI_ARGS_((Tcl_Interp *interp,
					    TclExpatInfo *expat,
					    int objc,
					    Tcl_Obj *CONST objv[]));
static int (TclExpatCget) _ANSI_ARGS_((Tcl_Interp *interp,
				       TclExpatInfo *expat,
				       int objc,
				       Tcl_Obj *CONST objv[]));

static void * (TclExpatElementStartHandler) _ANSI_ARGS_((void *userdata,
							 const XML_Char *name,
							 const XML_Char **atts));
static void * (TclExpatElementEndHandler) _ANSI_ARGS_((void *userData,
						       const XML_Char *name));
static void * (TclExpatCharacterDataHandler) _ANSI_ARGS_((void *userData,
							  const XML_Char *s,
							  int len));
static void * (TclExpatProcessingInstructionHandler) _ANSI_ARGS_((void *userData,
								  const XML_Char *target,
								  const XML_Char *data));
static void * (TclExpatExternalEntityRefHandler) _ANSI_ARGS_((XML_Parser parser,
							      const XML_Char *openEntityNames,
							      const XML_Char *base,
							      const XML_Char *systemId,
							      const XML_Char *publicId));
static void * (TclExpatDefaultHandler) _ANSI_ARGS_ ((void *userData,
						     const XML_Char *s,
						     int len));
static void * (TclExpatUnparsedDeclHandler) _ANSI_ARGS_ ((void *userData,
							  const XML_Char *entityname,
							  const XML_Char *base,
							  const XML_Char *systemId,
							  const XML_Char *publicId,
							  const XML_Char *notationName));
static void * (TclExpatNotationDeclHandler) _ANSI_ARGS_ ((void *userData,
							  const XML_Char *notationName,
							  const XML_Char *base,
							  const XML_Char *systemId,							  const XML_Char *publicId));
static int (TclExpatUnknownEncodingHandler) _ANSI_ARGS_ ((void *encodingHandlerData,
							 const XML_Char *name,
							 XML_Encoding *info));

#if defined(PRE81)

/*
 *----------------------------------------------------------------------------
 *
 * Tcl_GetString --
 *
 *	Compatibility routine for Tcl 8.0
 *
 * Results:
 *	String representation of object..
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */

char *
Tcl_GetString (obj)
     Tcl_Obj *obj; /* Object to retrieve string from. */
{
  char *s;
  int i;

  s = Tcl_GetStringFromObj(obj, &i);
  return s;
}
#endif 

/*
 *----------------------------------------------------------------------------
 *
 * TclExpat_Init --
 *
 *	Initialisation routine for loadable module
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Defines "expat" command in the interpreter.
 *
 *----------------------------------------------------------------------------
 */

/* this should be done via the stubs ... for the time being
   simply export */
#ifdef VISUAL_CC
DLLEXPORT extern int Xotclexpat_Init(Tcl_Interp * interp);
# define CONST_XOTCL_EXPAT
#else
# if defined(PRE84)
#  define CONST_XOTCL_EXPAT
# else
#  define CONST_XOTCL_EXPAT CONST84
# endif
#endif

extern int
Xotclexpat_Init (interp)
     Tcl_Interp *interp; /* Interpreter to initialise. */
{
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
        return TCL_ERROR;
    }
#ifdef USE_XOTCL_STUBS
    if (Xotcl_InitStubs(interp, "1.1", 0) == NULL) {
        return TCL_ERROR;
    }
#endif
#else
    if (Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 0) == NULL) {
        return TCL_ERROR;
    }
#endif


  Tcl_PkgProvide(interp, "xotcl::xml::expat", PACKAGE_VERSION);

  Tcl_CreateObjCommand(interp, "expat", TclExpatObjCmd, NULL, NULL);

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatObjCmd --
 *
 *	Creation command for expat class.
 *
 * Results:
 *	The name of the newly created parser instance.
 *
 * Side effects:
 *	This creates an expat parser.
 *
 *----------------------------------------------------------------------------
 */

int
TclExpatObjCmd(dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  TclExpatInfo *expat;

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "name ?args?");
    return TCL_ERROR;
  }

  /*
   * Create the data structures for this parser.
   */

  if (!(expat = (TclExpatInfo *) ckalloc(sizeof(TclExpatInfo)))) {
    ckfree((char*)expat);
    Tcl_SetResult(interp, "unable to create parser", NULL);
    return TCL_ERROR;
  }
  expat->interp = interp;
  Tcl_IncrRefCount(objv[1]);
  expat->name = objv[1];

  expat->elementstartcommand = NULL;
  expat->elementendcommand = NULL;
  expat->datacommand = NULL;
  expat->picommand = NULL;
  expat->defaultcommand = NULL;
  expat->unparsedcommand = NULL;
  expat->notationcommand = NULL;
  expat->externalentitycommand = NULL;
  expat->unknownencodingcommand = NULL;

  if (TclExpatCreateParser(interp, expat) != TCL_OK) {
    ckfree((char*)expat);
    return TCL_ERROR;
  }

  /*
   * Register a Tcl command for this parser instance.
   */

  Tcl_CreateObjCommand(interp, Tcl_GetString(expat->name), TclExpatInstanceCmd, (ClientData) expat, TclExpatDeleteCmd);

  /*
   * Handle configuration options
   */

  if (objc > 2) {
    TclExpatConfigure(interp, expat, objc - 2, objv + 2);
  }

  Tcl_SetObjResult(interp, expat->name);

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatCreateParser --
 *
 *	Create the expat parser and initialise (some of) the TclExpatInfo
 *	structure.
 *
 *	Note that callback commands are not affected by this routine,
 *	to allow a reset to leave these intact.
 *
 * Results:
 *	New parser instance created and initialised.
 *
 * Side effects:
 *	Creates an expat parser.
 *	Modifies TclExpatInfo fields.
 *
 *----------------------------------------------------------------------------
 */

int
TclExpatCreateParser(interp, expat)
     Tcl_Interp *interp;
     TclExpatInfo *expat;
{
  if (!(expat->parser = XML_ParserCreate(NULL))) {
    Tcl_SetResult(interp, "unable to create expat parser", NULL);
    return TCL_ERROR;
  }

  expat->final = 1;
  expat->status = TCL_OK;
  expat->result = NULL;
  expat->continueCount = 0;

  /*
   * Set handlers for the parser to routines in this module.
   */

  XML_SetElementHandler(expat->parser,
			(XML_StartElementHandler) TclExpatElementStartHandler,
			(XML_EndElementHandler) TclExpatElementEndHandler);
  XML_SetCharacterDataHandler(expat->parser,
			      (XML_CharacterDataHandler) TclExpatCharacterDataHandler);
  XML_SetProcessingInstructionHandler(expat->parser,
				      (XML_ProcessingInstructionHandler) TclExpatProcessingInstructionHandler);
  XML_SetDefaultHandler(expat->parser,
			(XML_DefaultHandler) TclExpatDefaultHandler);
  XML_SetUnparsedEntityDeclHandler(expat->parser,
				   (XML_UnparsedEntityDeclHandler) TclExpatUnparsedDeclHandler);
  XML_SetNotationDeclHandler(expat->parser,
			     (XML_NotationDeclHandler) TclExpatNotationDeclHandler);
  XML_SetExternalEntityRefHandler(expat->parser,
				  (XML_ExternalEntityRefHandler) TclExpatExternalEntityRefHandler);
  XML_SetUnknownEncodingHandler(expat->parser,
				(XML_UnknownEncodingHandler) TclExpatUnknownEncodingHandler,
				(void *) expat);
  XML_SetUserData(expat->parser,
		  (void *) expat);

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatFreeParser --
 *
 *	Destroy the expat parser structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees any memory allocated for the XML parser.
 *
 *----------------------------------------------------------------------------
 */

void
TclExpatFreeParser(expat)
     TclExpatInfo *expat;
{
  XML_ParserFree(expat->parser);
  expat->parser = NULL;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatInstanceCmd --
 *
 *	Implements instance command for expat class objects.
 *
 * Results:
 *	Depends on the method.
 *
 * Side effects:
 *	Depends on the method.
 *
 *----------------------------------------------------------------------------
 */

int
TclExpatInstanceCmd (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  TclExpatInfo *expat = (TclExpatInfo *) clientData;
  char *data;
  size_t len;
  int index, result = TCL_OK;
  static char CONST_XOTCL_EXPAT *options[] = {
    "configure", "cget", "parse", "reset", NULL
  };
  enum options {
    EXPAT_CONFIGURE, EXPAT_CGET, EXPAT_PARSE, EXPAT_RESET
  };

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "method ?args?");
    return TCL_ERROR;
  }

  if (Tcl_GetIndexFromObj(interp, objv[1], options, "option", 0,
			  &index) != TCL_OK) {
    return TCL_ERROR;
  }

  switch ((enum options) index) {
    case EXPAT_CONFIGURE:

      result = TclExpatConfigure(interp, (TclExpatInfo *) clientData, objc - 2, objv + 2);
      break;

    case EXPAT_CGET:

      result = TclExpatCget(interp, (TclExpatInfo *) clientData, objc - 2, objv + 2);
      break;

    case EXPAT_PARSE:

      if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "data");
	return TCL_ERROR;
      }

      data = Tcl_GetStringFromObj(objv[2], &len);

      result = TclExpatParse(interp, expat, data, len);

      break;

    case EXPAT_RESET:

      if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "");
	return TCL_ERROR;
      }

      /*
       * Destroy the parser and create a fresh one.
       */

      TclExpatFreeParser(expat);
      TclExpatCreateParser(interp, expat);

      break;

    default:

      Tcl_SetResult(interp, "unknown method", NULL);
      return TCL_ERROR;

  }

  return result;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatParse --
 *
 *	Wrapper to invoke expat parser and check return result.
 *
 * Results:
 *     TCL_OK if no errors, TCL_ERROR otherwise.
 *
 * Side effects:
 *     Sets interpreter result as appropriate.
 *
 *----------------------------------------------------------------------------
 */

int
TclExpatParse (interp, expat, data, len)
     Tcl_Interp *interp;
     TclExpatInfo *expat;
     char *data;
     size_t len;
{
  int result;
  char s[255];

  expat->status = TCL_OK;
  if (expat->result != NULL) {
    Tcl_DecrRefCount(expat->result);
  }
  expat->result = NULL;

  result = XML_Parse(expat->parser,
		     data, len,
		     expat->final);

  if (!result) {
    Tcl_ResetResult(interp);
    sprintf(s, "%d", XML_GetCurrentLineNumber(expat->parser));
    Tcl_AppendResult(interp, "error \"", 
 		     XML_ErrorString(XML_GetErrorCode(expat->parser)),
		     "\" at line ", s, " character ", NULL);
    sprintf(s, "%d", XML_GetCurrentColumnNumber(expat->parser));
    Tcl_AppendResult(interp, s, NULL);

    return TCL_ERROR;
  }

  switch (expat->status) {
    case TCL_OK:
    case TCL_BREAK:
    case TCL_CONTINUE:
      Tcl_ResetResult(interp);
      return TCL_OK;

    case TCL_ERROR:
      Tcl_SetObjResult(interp, expat->result);
      return TCL_ERROR;

    default:
      Tcl_SetResult(interp, "unknown parsing status", NULL);
      return TCL_ERROR;
  }

}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatConfigure --
 *
 *	Implements instance command for expat class objects.
 *
 * Results:
 *	Depends on the method.
 *
 * Side effects:
 *	Depends on the method.
 *
 *----------------------------------------------------------------------------
 */

int
TclExpatConfigure (interp, expat, objc, objv)
     Tcl_Interp *interp;
     TclExpatInfo *expat;
     int objc;
     Tcl_Obj *CONST objv[];
{
  static CONST_XOTCL_EXPAT char *switchTable[] = {
    "-final",
    "-baseurl",
    "-elementstartcommand",
    "-elementendcommand",
    "-characterdatacommand",
    "-processinginstructioncommand",
    "-defaultcommand",
    "-unparsedentitydeclcommand",
    "-notationdeclcommand",
    "-externalentitycommand",
    "-unknownencodingcommand",
    (char *) NULL
  };
  enum switches {
    EXPAT_FINAL, EXPAT_BASE,
    EXPAT_ELEMENTSTARTCMD, EXPAT_ELEMENTENDCMD,
    EXPAT_DATACMD, EXPAT_PICMD,
    EXPAT_DEFAULTCMD,
    EXPAT_UNPARSEDENTITYCMD, EXPAT_NOTATIONCMD,
    EXPAT_EXTERNALENTITYCMD, EXPAT_UNKNOWNENCODINGCMD
  };
  int index, bool, doParse = 0;
  Tcl_Obj *CONST *objPtr = objv;

  while (objc > 1) {
    if (Tcl_GetIndexFromObj(interp, objPtr[0], switchTable,
			    "switch", 0, &index) != TCL_OK) {
      return TCL_ERROR;
    }
    switch ((enum switches) index) {
      case EXPAT_FINAL:			/* -final */

	if (Tcl_GetBooleanFromObj(interp, objPtr[1], &bool) != TCL_OK) {
	  return TCL_ERROR;
	}

	if (bool && !expat->final) {

	  expat->final = bool;
	  doParse = 1;

	} else if (!bool && expat->final) {

	  /*
	   * Reset the parser for new input
	   */

	  TclExpatFreeParser(expat);
	  TclExpatCreateParser(interp, expat);
	  doParse = 0;

	}

	break;

      case EXPAT_BASE:			/* -base */

	if (XML_SetBase(expat->parser, Tcl_GetString(objPtr[1])) == 0) {
	  Tcl_SetResult(interp, "unable to set base URL", NULL);
	  return TCL_ERROR;
	}
	break;

      case EXPAT_ELEMENTSTARTCMD:	/* -elementstartcommand */

	if (expat->elementstartcommand != NULL) {
	  Tcl_DecrRefCount(expat->elementstartcommand);
	}

	expat->elementstartcommand = objPtr[1];
	Tcl_IncrRefCount(expat->elementstartcommand);

	break;

      case EXPAT_ELEMENTENDCMD:		/* -elementendcommand */

	if (expat->elementendcommand != NULL) {
	  Tcl_DecrRefCount(expat->elementendcommand);
	}

	expat->elementendcommand = objPtr[1];
	Tcl_IncrRefCount(expat->elementendcommand);

	break;

      case EXPAT_DATACMD:		/* -characterdatacommand */

	if (expat->datacommand != NULL) {
	  Tcl_DecrRefCount(expat->datacommand);
	}

	expat->datacommand = objPtr[1];
	Tcl_IncrRefCount(expat->datacommand);

	break;

      case EXPAT_PICMD:			/* -processinginstructioncommand */

	if (expat->picommand != NULL) {
	  Tcl_DecrRefCount(expat->picommand);
	}

	expat->picommand = objPtr[1];
	Tcl_IncrRefCount(expat->picommand);

	break;

      case EXPAT_DEFAULTCMD:		/* -defaultcommand */

	if (expat->defaultcommand != NULL) {
	  Tcl_DecrRefCount(expat->defaultcommand);
	}

	expat->defaultcommand = objPtr[1];
	Tcl_IncrRefCount(expat->defaultcommand);

	break;

      case EXPAT_UNPARSEDENTITYCMD:		/* -unparsedentitydeclcommand */

	if (expat->unparsedcommand != NULL) {
	  Tcl_DecrRefCount(expat->unparsedcommand);
	}

	expat->unparsedcommand = objPtr[1];
	Tcl_IncrRefCount(expat->unparsedcommand);

	break;

      case EXPAT_NOTATIONCMD:			/* -notationdeclcommand */

	if (expat->notationcommand != NULL) {
	  Tcl_DecrRefCount(expat->notationcommand);
	}

	expat->notationcommand = objPtr[1];
	Tcl_IncrRefCount(expat->notationcommand);

	break;

      case EXPAT_EXTERNALENTITYCMD:	/* -externalentitycommand */

	if (expat->externalentitycommand != NULL) {
	  Tcl_DecrRefCount(expat->externalentitycommand);
	}

	expat->externalentitycommand = objPtr[1];
	Tcl_IncrRefCount(expat->externalentitycommand);

	break;

      case EXPAT_UNKNOWNENCODINGCMD:		/* -unknownencodingcommand */

	/* Not implemented */
	break;

	if (expat->unknownencodingcommand != NULL) {
	  Tcl_DecrRefCount(expat->unknownencodingcommand);
	}

	expat->unknownencodingcommand = objPtr[1];
	Tcl_IncrRefCount(expat->unknownencodingcommand);

	break;

    }

    objPtr += 2;
    objc -= 2;

  }

  if (doParse) {
    return TclExpatParse(interp, expat->parser, "", 0);
  } else {
    return TCL_OK;
  }

}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatCget --
 *
 *	Returns setting of configuration option.
 *	Not yet implemented.
 *
 * Results:
 *	Option value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */

int
TclExpatCget (interp, expat, objc, objv)
     Tcl_Interp *interp;
     TclExpatInfo *expat;
     int objc;
     Tcl_Obj *CONST objv[];
{
  Tcl_SetResult(interp, "method not implemented", NULL);
  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatHandlerResult --
 *
 *	Manage the result of the application callback.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Further invocation of callback scripts may be inhibited.
 *
 *----------------------------------------------------------------------------
 */

void
TclExpatHandlerResult(expat, result)
     TclExpatInfo *expat;
     int result;
{
  switch (result) {
    case TCL_OK:
      expat->status = TCL_OK;
      break;

    case TCL_CONTINUE:
      /*
       * Skip callbacks until the matching end element event
       * occurs for the currently open element.
       * Keep a reference count to handle nested
       * elements.
       */
      expat->status = TCL_CONTINUE;
      expat->continueCount = 1;
      break;

    case TCL_BREAK:
      /*
       * Skip all further callbacks, but return OK.
       */
      expat->status = TCL_BREAK;
      break;

    case TCL_ERROR:
      /*
       * Skip all further callbacks, and return error.
       */
    default:
      expat->status = TCL_ERROR;
      expat->result = Tcl_GetObjResult(expat->interp);
      Tcl_IncrRefCount(expat->result);
      break;
  }
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatElementStartHandler --
 *
 *	Called by expat for each start tag.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Callback script is invoked.
 *
 *----------------------------------------------------------------------------
 */

static void *
TclExpatElementStartHandler(userData, name, atts)
     void *userData;
     const char *name;
     const char **atts;
{
  TclExpatInfo *expat = (TclExpatInfo *) userData;
  Tcl_Obj *atList, *cmdPtr;
  const char **atPtr;
  int result;

  if (expat->status == TCL_CONTINUE) {

    /*
     * We're currently skipping elements looking for the
     * close of the continued element.
     */

    expat->continueCount++;
    return NULL;
  }

  if (expat->elementstartcommand == NULL ||
      expat->status != TCL_OK) {
    return NULL;
  }

  /*
   * Convert the attribute list into a Tcl key-value paired list.
   */

  atList = Tcl_NewListObj(0, NULL);
  for (atPtr = atts; atPtr[0] && atPtr[1]; atPtr += 2) {
    Tcl_ListObjAppendElement(expat->interp, atList, Tcl_NewStringObj((char *)atPtr[0], strlen(atPtr[0])));
    Tcl_ListObjAppendElement(expat->interp, atList, Tcl_NewStringObj((char *)atPtr[1], strlen(atPtr[1])));
  }

  /*
   * Take a copy of the callback script so that arguments may be appended.
   */

  cmdPtr = Tcl_DuplicateObj(expat->elementstartcommand);
  Tcl_IncrRefCount(cmdPtr);
  Tcl_Preserve((ClientData) expat->interp);

  Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)name, strlen(name)));
  Tcl_ListObjAppendElement(expat->interp, cmdPtr, atList);

  /*
   * It would be desirable to be able to terminate parsing
   * if the return result is TCL_ERROR or TCL_BREAK.
   */
#if defined(PRE81)
  result = Tcl_GlobalEvalObj(expat->interp, cmdPtr);
#elif defined(PRE82)
  result = Tcl_EvalObj(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#else
  result = Tcl_EvalObjEx(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#endif

  Tcl_DecrRefCount(cmdPtr);
  Tcl_Release((ClientData) expat->interp);

  TclExpatHandlerResult(expat, result);

  return NULL;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatElementEndHandler --
 *
 *	Called by expat for each end tag.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Callback script is invoked.
 *
 *----------------------------------------------------------------------------
 */

static void *
TclExpatElementEndHandler(userData, name)
     void *userData;
     CONST char *name;
{
  TclExpatInfo *expat = (TclExpatInfo *) userData;
  Tcl_Obj *cmdPtr;
  int result;

  if (expat->status == TCL_CONTINUE) {
    /*
     * We're currently skipping elements looking for the
     * end of the currently open element.
     */

    if (!--(expat->continueCount)) {
      expat->status = TCL_OK;
      return NULL;
    }
  }

  if (expat->elementendcommand == NULL ||
      expat->status != TCL_OK) {
    return NULL;
  }

  /*
   * Take a copy of the callback script so that arguments may be appended.
   */

  cmdPtr = Tcl_DuplicateObj(expat->elementendcommand);
  Tcl_IncrRefCount(cmdPtr);
  Tcl_Preserve((ClientData) expat->interp);

  Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)name, strlen(name)));

  /*
   * It would be desirable to be able to terminate parsing
   * if the return result is TCL_ERROR or TCL_BREAK.
   */
#if defined(PRE81)
  result = Tcl_GlobalEvalObj(expat->interp, cmdPtr);
#elif defined(PRE82) 
  result = Tcl_EvalObj(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#else
  result = Tcl_EvalObjEx(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#endif

  Tcl_DecrRefCount(cmdPtr);
  Tcl_Release((ClientData) expat->interp);

  TclExpatHandlerResult(expat, result);

  return NULL;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatCharacterDataHandler --
 *
 *	Called by expat for character data.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Callback script is invoked.
 *
 *----------------------------------------------------------------------------
 */

static void *
TclExpatCharacterDataHandler(userData, s, len)
     void *userData;
     CONST char *s;
     int len;
{
  TclExpatInfo *expat = (TclExpatInfo *) userData;
  Tcl_Obj *cmdPtr;
  int result;

  if (expat->datacommand == NULL ||
      expat->status != TCL_OK) {
    return NULL;
  }

  /*
   * Take a copy of the callback script so that arguments may be appended.
   */

  cmdPtr = Tcl_DuplicateObj(expat->datacommand);
  Tcl_IncrRefCount(cmdPtr);
  Tcl_Preserve((ClientData) expat->interp);

  Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)s, len));

  /*
   * It would be desirable to be able to terminate parsing
   * if the return result is TCL_ERROR or TCL_BREAK.
   */
#if defined(PRE81)
  result = Tcl_GlobalEvalObj(expat->interp, cmdPtr);
#elif defined(PRE82)
  result = Tcl_EvalObj(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#else
  result = Tcl_EvalObjEx(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#endif

  Tcl_DecrRefCount(cmdPtr);
  Tcl_Release((ClientData) expat->interp);

  TclExpatHandlerResult(expat, result);

  return NULL;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatProcessingInstructionHandler --
 *
 *	Called by expat for processing instructions.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Callback script is invoked.
 *
 *----------------------------------------------------------------------------
 */

static void *
TclExpatProcessingInstructionHandler(userData, target, data)
     void *userData;
     CONST char *target;
     CONST char *data;
{
  TclExpatInfo *expat = (TclExpatInfo *) userData;
  Tcl_Obj *cmdPtr;
  int result;

  if (expat->picommand == NULL ||
      expat->status != TCL_OK) {
    return NULL;
  }

  /*
   * Take a copy of the callback script so that arguments may be appended.
   */

  cmdPtr = Tcl_DuplicateObj(expat->picommand);
  Tcl_IncrRefCount(cmdPtr);
  Tcl_Preserve((ClientData) expat->interp);

  Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)target, strlen(target)));
  Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)data, strlen(data)));

  /*
   * It would be desirable to be able to terminate parsing
   * if the return result is TCL_ERROR or TCL_BREAK.
   */
#if defined(PRE81)
  result = Tcl_GlobalEvalObj(expat->interp, cmdPtr);
#elif defined(PRE82)
  result = Tcl_EvalObj(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#else
  result = Tcl_EvalObjEx(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#endif

  Tcl_DecrRefCount(cmdPtr);
  Tcl_Release((ClientData) expat->interp);

  TclExpatHandlerResult(expat, result);

  return NULL;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatDefaultHandler --
 *
 *	Called by expat for processing data which has no other handler.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Callback script is invoked.
 *
 *----------------------------------------------------------------------------
 */

static void *
TclExpatDefaultHandler(userData, s, len)
     void *userData;
     CONST char *s;
     int len;
{
  TclExpatInfo *expat = (TclExpatInfo *) userData;
  Tcl_Obj *cmdPtr;
  int result;

  if (expat->defaultcommand == NULL ||
      expat->status != TCL_OK) {
    return NULL;
  }

  /*
   * Take a copy of the callback script so that arguments may be appended.
   */

  cmdPtr = Tcl_DuplicateObj(expat->defaultcommand);
  Tcl_IncrRefCount(cmdPtr);
  Tcl_Preserve((ClientData) expat->interp);

  Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)s, len));

  /*
   * It would be desirable to be able to terminate parsing
   * if the return result is TCL_ERROR or TCL_BREAK.
   */
#if defined(PRE81)
  result = Tcl_GlobalEvalObj(expat->interp, cmdPtr);
#elif defined(PRE82)
  result = Tcl_EvalObj(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#else
  result = Tcl_EvalObjEx(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#endif

  Tcl_DecrRefCount(cmdPtr);
  Tcl_Release((ClientData) expat->interp);

  TclExpatHandlerResult(expat, result);

  return NULL;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatUnparsedDeclHandler --
 *
 *	Called by expat for processing an unparsed entity references.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Callback script is invoked.
 *
 *----------------------------------------------------------------------------
 */

static void *
TclExpatUnparsedDeclHandler(userData, entityname, base, systemId, publicId, notationName)
     void *userData;
     CONST char *entityname;
     CONST char *base;
     CONST char *systemId;
     CONST char *publicId;
     CONST char *notationName;
{
  TclExpatInfo *expat = (TclExpatInfo *) userData;
  Tcl_Obj *cmdPtr;
  int result;

  if (expat->unparsedcommand == NULL ||
      expat->status != TCL_OK) {
    return NULL;
  }

  /*
   * Take a copy of the callback script so that arguments may be appended.
   */

  cmdPtr = Tcl_DuplicateObj(expat->unparsedcommand);
  Tcl_IncrRefCount(cmdPtr);
  Tcl_Preserve((ClientData) expat->interp);

  Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)entityname, strlen(entityname)));
  Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)base, strlen(base)));
  Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)systemId, strlen(systemId)));
  if (publicId == NULL) {
    Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewListObj(0, NULL));
  } else {
    Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)publicId, strlen(publicId)));
  }
  if (notationName == NULL) {
    Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewListObj(0, NULL));
  } else {
    Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)notationName, strlen(notationName)));
  }

  /*
   * It would be desirable to be able to terminate parsing
   * if the return result is TCL_ERROR or TCL_BREAK.
   */
#if defined(PRE81)
  result = Tcl_GlobalEvalObj(expat->interp, cmdPtr);
#elif defined(PRE82)
  result = Tcl_EvalObj(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#else
  result = Tcl_EvalObjEx(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#endif

  Tcl_DecrRefCount(cmdPtr);
  Tcl_Release((ClientData) expat->interp);

  TclExpatHandlerResult(expat, result);

  return NULL;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatNotationDeclHandler --
 *
 *	Called by expat for processing a notation declaration.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Callback script is invoked.
 *
 *----------------------------------------------------------------------------
 */

static void *
TclExpatNotationDeclHandler(userData, notationName, base, systemId, publicId)
     void *userData;
     CONST char *notationName;
     CONST char *base;
     CONST char *systemId;
     CONST char *publicId;
{
  TclExpatInfo *expat = (TclExpatInfo *) userData;
  Tcl_Obj *cmdPtr;
  int result;

  if (expat->notationcommand == NULL ||
      expat->status != TCL_OK) {
    return NULL;
  }

  /*
   * Take a copy of the callback script so that arguments may be appended.
   */

  cmdPtr = Tcl_DuplicateObj(expat->notationcommand);
  Tcl_IncrRefCount(cmdPtr);
  Tcl_Preserve((ClientData) expat->interp);

  Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)notationName, strlen(notationName)));
  Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)base, strlen(base)));
  if (systemId == NULL) {
    Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewListObj(0, NULL));
  } else {
    Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)systemId, strlen(systemId)));
  }
  if (publicId == NULL) {
    Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewListObj(0, NULL));
  } else {
    Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)publicId, strlen(publicId)));
  }

  /*
   * It would be desirable to be able to terminate parsing
   * if the return result is TCL_ERROR or TCL_BREAK.
   */
#if defined(PRE81)
  result = Tcl_GlobalEvalObj(expat->interp, cmdPtr);
#elif defined(PRE82)
  result = Tcl_EvalObj(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#else
  result = Tcl_EvalObjEx(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#endif

  Tcl_DecrRefCount(cmdPtr);
  Tcl_Release((ClientData) expat->interp);

  TclExpatHandlerResult(expat, result);

  return NULL;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatUnknownEncodingHandler --
 *
 *	Called by expat for processing a reference to a character in an
 *	unknown encoding.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Callback script is invoked.
 *
 *----------------------------------------------------------------------------
 */

static int
TclExpatUnknownEncodingHandler(encodingHandlerData, name, info)
     void *encodingHandlerData;
     CONST char *name;
     XML_Encoding *info;
{
  TclExpatInfo *expat = (TclExpatInfo *) encodingHandlerData;
  Tcl_Obj *cmdPtr;
  int result;

  Tcl_SetResult(expat->interp, "not implemented", NULL);
  return 0;

  if (expat->unknownencodingcommand == NULL ||
      expat->status != TCL_OK) {
    return 0;
  }

  /*
   * Take a copy of the callback script so that arguments may be appended.
   */

  cmdPtr = Tcl_DuplicateObj(expat->unknownencodingcommand);
  Tcl_IncrRefCount(cmdPtr);
  Tcl_Preserve((ClientData) expat->interp);

  /*
   * Setup the arguments
   */

  /*
   * It would be desirable to be able to terminate parsing
   * if the return result is TCL_ERROR or TCL_BREAK.
   */
#if defined(PRE81)
  result = Tcl_GlobalEvalObj(expat->interp, cmdPtr);
#elif defined(PRE82)
  result = Tcl_EvalObj(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#else
  result = Tcl_EvalObjEx(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#endif

  Tcl_DecrRefCount(cmdPtr);
  Tcl_Release((ClientData) expat->interp);

  TclExpatHandlerResult(expat, result);

  /*
   * NOTE: have to decide whether to return 0 or 1 here,
   * since Expat is waiting for an answer.
   */
  return 0;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatExternalEntityRefHandler --
 *
 *	Called by expat for processing external entity references.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Callback script is invoked.
 *
 *----------------------------------------------------------------------------
 */

static void *
TclExpatExternalEntityRefHandler(parser, openEntityNames, base, systemId, publicId)
     XML_Parser parser;
     CONST char *openEntityNames;
     CONST char *base;
     CONST char *systemId;
     CONST char *publicId;
{
  TclExpatInfo *expat = (TclExpatInfo *) XML_GetUserData(parser);
  Tcl_Obj *cmdPtr;
  int result;

  if (expat->externalentitycommand == NULL ||
      expat->status != TCL_OK) {
    return NULL;
  }

  /*
   * Take a copy of the callback script so that arguments may be appended.
   */

  cmdPtr = Tcl_DuplicateObj(expat->externalentitycommand);
  Tcl_IncrRefCount(cmdPtr);
  Tcl_Preserve((ClientData) expat->interp);

  Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)openEntityNames, strlen(openEntityNames)));
  Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)base, strlen(base)));
  Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)systemId, strlen(systemId)));
  Tcl_ListObjAppendElement(expat->interp, cmdPtr, Tcl_NewStringObj((char *)publicId, strlen(publicId)));

  /*
   * It would be desirable to be able to terminate parsing
   * if the return result is TCL_ERROR or TCL_BREAK.
   */
#if defined(PRE81)
  result = Tcl_GlobalEvalObj(expat->interp, cmdPtr);
#elif defined(PRE82)
  result = Tcl_EvalObj(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#else
  result = Tcl_EvalObjEx(expat->interp, cmdPtr, TCL_EVAL_GLOBAL);
#endif

  Tcl_DecrRefCount(cmdPtr);
  Tcl_Release((ClientData) expat->interp);

  TclExpatHandlerResult(expat, result);

  return NULL;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatDeleteCmd --
 *
 *	Called when a expat parser is deleted.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Memory structures are freed.
 *
 *----------------------------------------------------------------------------
 */

static void
TclExpatDeleteCmd(clientData)
     ClientData clientData;
{
  TclExpatInfo *expat = (TclExpatInfo *) clientData;

  TclExpatFreeParser(expat);

  Tcl_DecrRefCount(expat->name);

  if (expat->elementstartcommand) {
    Tcl_DecrRefCount(expat->elementstartcommand);
  }
  if (expat->elementendcommand) {
    Tcl_DecrRefCount(expat->elementendcommand);
  }
  if (expat->datacommand) {
    Tcl_DecrRefCount(expat->datacommand);
  }
  if (expat->picommand) {
    Tcl_DecrRefCount(expat->picommand);
  }
  if (expat->externalentitycommand) {
    Tcl_DecrRefCount(expat->externalentitycommand);
  }
}
