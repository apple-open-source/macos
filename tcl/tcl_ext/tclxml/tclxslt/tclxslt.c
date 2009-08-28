/*
 * tclxslt.c --
 *
 *  Interface to Gnome libxslt.
 *
 * Copyright (c) 2001-2002 Zveno Pty Ltd
 * http://www.zveno.com/
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
 * $Id: tclxslt.c,v 1.20 2002/11/29 23:29:24 rnurmi Exp $
 *
 */

#include "tclxslt.h"

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

#ifdef __WIN32__
#     include "win/win32config.h"
#endif

/*
 * For Darwin (MacOS X) in particular, but also others
 */
 
#ifndef __WIN32__
#	define DLLIMPORT EXTERN
#endif

/*
 * Manage stylesheet objects
 */

typedef struct TclXSLT_Stylesheet {
  Tcl_Interp *interp;
  char *name;
  xsltStylesheetPtr stylesheet;

  Tcl_Obj *messagecommand;
} TclXSLT_Stylesheet;

static int ssheetCntr = 0;

/*
 * Extension management
 */

typedef struct TclXSLT_Extension {
  Tcl_Interp *interp;
  Tcl_Obj *nsuri;
  Tcl_Obj *tclns;
  xsltTransformContextPtr xformCtxt;
} TclXSLT_Extension;

Tcl_HashTable extensions;

/*
 * Prototypes for procedures defined later in this file:
 */

/*
 * Declarations for externally visible functions.
 */

EXTERN int      Xslt_Init _ANSI_ARGS_((Tcl_Interp *interp));

/*
 * Forward declarations for private functions.
 */

static void TclXSLTGenericError _ANSI_ARGS_((void *ctx, const char *msg, ...));

static int TclXSLTCompileCommand _ANSI_ARGS_((ClientData dummy,
						Tcl_Interp *interp,
						int objc,
						Tcl_Obj *CONST objv[]));
static int TclXSLTInstanceCommand _ANSI_ARGS_((ClientData ssheet,
						Tcl_Interp *interp,
						int objc,
						Tcl_Obj *CONST objv[]));
static void TclXSLTDeleteStylesheet _ANSI_ARGS_((ClientData ssheet));
static int TclXSLTExtensionCommand _ANSI_ARGS_((ClientData dummy,
						Tcl_Interp *interp,
						int objc,
						Tcl_Obj *CONST objv[]));

static int TclXSLTTransform _ANSI_ARGS_((TclXSLT_Stylesheet *stylesheet,
                                         Tcl_Obj *source,
                                         int paramc,
                                         Tcl_Obj *CONST paramv[]));

static void TclXSLT_RegisterAll _ANSI_ARGS_((TclXSLT_Extension *extinfo,
						const xmlChar *nsuri));

/* static xsltExtInitFunction TclXSLTExtInit; */
static void *TclXSLTExtInit _ANSI_ARGS_((xsltTransformContextPtr ctxt,
					const xmlChar *URI));
/* static xsltExtShutdownFunction TclXSLTExtShutdown; */
static void TclXSLTExtShutdown _ANSI_ARGS_((xsltTransformContextPtr ctxt,
					    const xmlChar *URI,
					    void *userdata));
/* static xmlXPathEvalFunc TclXSLTExtFunction; */
static void TclXSLTExtFunction _ANSI_ARGS_((xmlXPathParserContextPtr xpathCtxt,
					    int nargs));
/* static xsltPreComputeFunction TclXSLTExtElementPreComp; */
static void TclXSLTExtElementPreComp _ANSI_ARGS_((xsltStylesheetPtr style,
						  xmlNodePtr inst,
						  xsltTransformFunction function));
/* static xsltTransformFunction TclXSLTExtElementTransform; */
static void TclXSLTExtElementTransform _ANSI_ARGS_((xsltTransformContextPtr ctxt,
					            xmlNodePtr node,
					            xmlNodePtr inst,
					            xsltStylePreCompPtr comp));

static Tcl_Obj * TclXSLT_ConvertXPathObjToTclObj _ANSI_ARGS_((Tcl_Interp *interp,
                                                              xmlXPathObjectPtr xpobj));
static xmlXPathObjectPtr TclXSLT_ConvertTclObjToXPathObj _ANSI_ARGS_((Tcl_Interp *interp,
                                                              Tcl_Obj *objPtr));

/*
 * Error context for passing error result back to caller.
 */

typedef struct GenericError_Info {
  Tcl_Interp *interp;
  TclXSLT_Stylesheet *stylesheet;
  int code;
  Tcl_Obj *msg;
} GenericError_Info;

/*
 * Switch tables
 */

#ifndef CONST84
#define CONST84 /* Before 8.4 no 'const' required */
#endif

static CONST84 char *instanceCommandMethods[] = {
  "cget",
  "configure",
  "transform",
  (char *) NULL
};
enum instanceCommandMethods {
  TCLXSLT_CGET,
  TCLXSLT_CONFIGURE,
  TCLXSLT_TRANSFORM
};
static CONST84 char *instanceCommandOptions[] = {
  "-messagecommand",
  "-method",
  (char *) NULL
};
enum instanceCommandOptions {
  TCLXSLT_OPTION_MESSAGECOMMAND,
  TCLXSLT_OPTION_METHOD
};

static CONST84 char *extensionCommandMethods[] = {
  "add",
  "remove",
  (char *) NULL
};
enum extensionCommandMethods {
  TCLXSLT_EXT_ADD,
  TCLXSLT_EXT_REMOVE
};

/*
 * Debugging
 */

static Tcl_Channel stderrChan;
static char dbgbuf[200];

/*
static void DumpTclObj(objPtr)
    Tcl_Obj *objPtr;
{
  Tcl_Obj *elPtr;
  int idx, len;
  
  if (objPtr->typePtr == Tcl_GetObjType("list")) {
    Tcl_WriteChars(stderrChan, " list(", -1);
    Tcl_ListObjLength(NULL, objPtr, &len);
    for (idx = 0; idx < len; idx++) {
      Tcl_ListObjIndex(NULL, objPtr, idx, &elPtr);
      DumpTclObj(elPtr);
    }
    Tcl_WriteChars(stderrChan, ")", -1);
  } else if (objPtr->typePtr == Tcl_GetObjType("libxml2-node")) {
    xmlNodePtr nodePtr = (xmlNodePtr) objPtr->internalRep.otherValuePtr;
    sprintf(dbgbuf, " nodePtr x%x name \"%s\" value \"%s\"", nodePtr, nodePtr->name, xmlNodeGetContent(nodePtr));
    Tcl_WriteChars(stderrChan, dbgbuf, -1);
  } else {
    sprintf(dbgbuf, " obj x%x \"%s\"", objPtr, Tcl_GetStringFromObj(objPtr, NULL));
    Tcl_WriteChars(stderrChan, dbgbuf, -1);
  }
}

static void DumpTree(nodePtr)
    xmlNodePtr nodePtr;
{
  xmlNodePtr child;
          switch (nodePtr->type) {
      case XML_ELEMENT_NODE:
	sprintf(dbgbuf, "adding element \"%s\" x%x (%s)\n", nodePtr->name, nodePtr, XML_GET_CONTENT(nodePtr));
        Tcl_WriteChars(stderrChan, dbgbuf, -1);
        for (child = nodePtr->children; child != NULL; child = child->next) {
          DumpTree(child);
        }
	break;
      case XML_ATTRIBUTE_NODE:
	Tcl_WriteChars(stderrChan, "adding attribute\n", -1);
	break;
      case XML_TEXT_NODE:
      case XML_CDATA_SECTION_NODE:
	sprintf(dbgbuf, "adding textNode \"%s\" x%x\n", XML_GET_CONTENT(nodePtr), nodePtr);
        Tcl_WriteChars(stderrChan, dbgbuf, -1);
	break;
      case XML_ENTITY_REF_NODE:
	Tcl_WriteChars(stderrChan, "adding entityReference\n", -1);
	break;
      case XML_ENTITY_NODE:
	Tcl_WriteChars(stderrChan, "adding entity\n", -1);
	break;
      case XML_PI_NODE:
	Tcl_WriteChars(stderrChan, "adding processingInstruction\n", -1);
	break;
      case XML_COMMENT_NODE:
	Tcl_WriteChars(stderrChan, "adding comment\n", -1);
	break;
      case XML_DOCUMENT_NODE:
	Tcl_WriteChars(stderrChan, "adding document\n", -1);
	break;
      case XML_DOCUMENT_TYPE_NODE:
	Tcl_WriteChars(stderrChan, "adding docType\n", -1);
	break;
      case XML_DOCUMENT_FRAG_NODE:
	Tcl_WriteChars(stderrChan, "adding documentFragment\n", -1);
	break;
      case XML_NOTATION_NODE:
	Tcl_WriteChars(stderrChan, "adding notation\n", -1);
	break;
      case XML_HTML_DOCUMENT_NODE:
	Tcl_WriteChars(stderrChan, "adding HTMLdocument\n", -1);
	break;
      case XML_DTD_NODE:
	Tcl_WriteChars(stderrChan, "adding dtd\n", -1);
	break;
      case XML_ELEMENT_DECL:
	Tcl_WriteChars(stderrChan, "adding elementDecl\n", -1);
	break;
      case XML_ATTRIBUTE_DECL:
	Tcl_WriteChars(stderrChan, "adding attributeDecl\n", -1);
	break;
      case XML_ENTITY_DECL:
	Tcl_WriteChars(stderrChan, "adding entityDecl\n", -1);
	break;
      case XML_NAMESPACE_DECL:
	Tcl_WriteChars(stderrChan, "adding namespaceDecl\n", -1);
	break;
      case XML_XINCLUDE_START:
	Tcl_WriteChars(stderrChan, "adding xincludeStart\n", -1);
	break;
      case XML_XINCLUDE_END:
	Tcl_WriteChars(stderrChan, "adding xincludeEnd\n", -1);
	break;
      default:
	Tcl_WriteChars(stderrChan, "adding unknown\n", -1);
        }
}
*/

/*
 *----------------------------------------------------------------------------
 *
 * Xslt_Init --
 *
 *  Initialisation routine for loadable module
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Creates commands in the interpreter,
 *
 *----------------------------------------------------------------------------
 */

int
Xslt_Init (interp)
     Tcl_Interp *interp;	/* Interpreter to initialise */
{
  int dbgMode;

#ifdef USE_TCL_STUBS
  if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
    return TCL_ERROR;
  }
#endif
#ifdef USE_TCLDOMXML_STUBS
  /* This is  "dom::libxml2"
   */
  if (Tcldomxml_InitStubs(interp, TCLDOMXML_VERSION, 1) == NULL) {
    return TCL_ERROR;
  }
#endif

  Tcl_CreateObjCommand(interp, "xslt::compile", TclXSLTCompileCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "xslt::extension", TclXSLTExtensionCommand, NULL, NULL);

  Tcl_InitHashTable(&extensions, TCL_STRING_KEYS);

  exsltRegisterAll();

  stderrChan = Tcl_GetChannel(interp, "stderr", &dbgMode);

  Tcl_PkgProvide(interp, "xslt", TCLXSLT_VERSION);

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclXSLTCompileCommand --
 *
 *  Class creation command for xslt stylesheet objects.
 *
 * Results:
 *  Compiles the XSLT stylesheet.
 *  Creates a Tcl command associated with that stylesheet.
 *
 * Side effects:
 *  Memory allocated, stylesheet is compiled.
 *
 *----------------------------------------------------------------------------
 */

static int
TclXSLTCompileCommand(dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  TclXSLT_Stylesheet *info;
  xmlDocPtr origDoc, doc;
  xsltStylesheetPtr ssheetPtr = NULL;
  GenericError_Info *errorInfoPtr;

  if (objc != 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "stylesheet-doc");
    return TCL_ERROR;
  }

  /* Copy the document object, since libxslt clobbers the _private field */
  if (TclDOM_GetDocFromObj(interp, objv[1], &origDoc) != TCL_OK) {
    return TCL_ERROR;
  }
  doc = xmlCopyDoc(origDoc, 1);
  /*
   * xmlCopyDoc doesn't copy some of the fields.
   */
  if (origDoc->URL) {
    doc->URL = Tcl_Alloc(strlen(origDoc->URL) + 1);
    strcpy((char *) doc->URL, origDoc->URL);
  }

  /*
   * Prepare for compiling stylesheet
   */

  errorInfoPtr = (GenericError_Info *) Tcl_Alloc(sizeof(GenericError_Info));
  errorInfoPtr->interp = interp;
  errorInfoPtr->stylesheet = NULL;
  errorInfoPtr->code = TCL_OK;
  errorInfoPtr->msg = NULL;

  /*
   * Compile stylesheet
   */

  if ((ssheetPtr = xsltParseStylesheetDoc(doc)) == NULL) {
    Tcl_SetResult(interp, "error compiling stylesheet", NULL);
    goto error;
  }

  if (ssheetPtr->errors > 0) {
    Tcl_SetResult(interp, "error compiling XSLT stylesheet", NULL);
    goto error;
  }

  if (errorInfoPtr->code != TCL_OK) {

    if (errorInfoPtr->msg) {
      Tcl_SetObjResult(interp, errorInfoPtr->msg);
    }

    goto error;
  }

  info = (TclXSLT_Stylesheet *) Tcl_Alloc(sizeof(TclXSLT_Stylesheet));
  info->interp = interp;
  info->name = Tcl_Alloc(20);
  sprintf(info->name, "style%d", ssheetCntr++);
  info->stylesheet = ssheetPtr;
  info->messagecommand = NULL;

  Tcl_CreateObjCommand(interp, info->name, TclXSLTInstanceCommand, (ClientData) info, TclXSLTDeleteStylesheet);

  Tcl_SetObjResult(interp, Tcl_NewStringObj(info->name, -1));

  return TCL_OK;

error:

  if (errorInfoPtr->msg) {
    Tcl_DecrRefCount(errorInfoPtr->msg);
  }
  Tcl_Free((char *) errorInfoPtr);

  if (ssheetPtr) {
    xsltFreeStylesheet(ssheetPtr);
  } else {
    xmlFreeDoc(doc);
  }
  
  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclXSLTDeleteStylesheet --
 *
 *  Class destruction command for xslt stylesheet objects.
 *
 * Results:
 *  Frees memory associated with a stylesheet.
 *
 * Side effects:
 *  Memory deallocated.
 *
 *----------------------------------------------------------------------------
 */

static void
TclXSLTDeleteStylesheet(clientData)
     ClientData clientData;
{
  TclXSLT_Stylesheet *ssheet = (TclXSLT_Stylesheet *) clientData;
  
  Tcl_Free(ssheet->name);
  if (ssheet->messagecommand) {
    Tcl_DecrRefCount(ssheet->messagecommand);
  }
  xsltFreeStylesheet(ssheet->stylesheet); /* Also frees document */
  Tcl_Free((char *) ssheet);
}
/*
 *----------------------------------------------------------------------------
 *
 * TclXSLTInstanceCommand --
 *
 *  Handles the stylesheet object command.
 *
 * Results:
 *  Depends on method.
 *
 * Side effects:
 *  Depends on method.
 *
 *----------------------------------------------------------------------------
 */

static int
TclXSLTInstanceCommand(clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  TclXSLT_Stylesheet *ssheet = (TclXSLT_Stylesheet *) clientData;
  int method, option;

  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "method ?args ...?");
    return TCL_ERROR;
  }

  if (Tcl_GetIndexFromObj(interp, objv[1], instanceCommandMethods, 
			    "method", 0, &method) != TCL_OK) {
    return TCL_ERROR;
  }

  switch ((enum instanceCommandMethods) method) {
  case TCLXSLT_CGET:

    if (objc != 3) {
      Tcl_WrongNumArgs(interp, 2, objv, "option");
      return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], instanceCommandOptions, 
			    "option", 0, &option) != TCL_OK) {
      return TCL_ERROR;
    }

    switch ((enum instanceCommandOptions) option) {

    case TCLXSLT_OPTION_METHOD:
      if (ssheet->stylesheet->method != NULL) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(ssheet->stylesheet->method, -1));
      }
      break;

    case TCLXSLT_OPTION_MESSAGECOMMAND:
      if (ssheet->messagecommand != NULL) {
        Tcl_SetObjResult(interp, ssheet->messagecommand);
      }
      break;

    default:
      Tcl_SetResult(interp, "unknown option", NULL);
      return TCL_ERROR;
    }

    break;

  case TCLXSLT_CONFIGURE:
    
    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 2, objv, "option value");
      return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], instanceCommandOptions, 
			    "option", 0, &option) != TCL_OK) {
      return TCL_ERROR;
    }

    switch ((enum instanceCommandOptions) option) {

    case TCLXSLT_OPTION_METHOD:
      Tcl_SetResult(interp, "read-only option", NULL);
      return TCL_ERROR;
      break;

    case TCLXSLT_OPTION_MESSAGECOMMAND:
      if (ssheet->messagecommand != NULL) {
        Tcl_DecrRefCount(ssheet->messagecommand);
      }
      ssheet->messagecommand = objv[3];
      Tcl_IncrRefCount(ssheet->messagecommand);
      break;

    default:
      Tcl_SetResult(interp, "unknown option", NULL);
      return TCL_ERROR;
    }

    break;

  case TCLXSLT_TRANSFORM:
    if (objc < 3) {
      Tcl_WrongNumArgs(interp, 2, objv, "source ?param value...?");
      return TCL_ERROR;
    }

    return TclXSLTTransform(ssheet, objv[2], objc - 3, &objv[3]);

    break;

  default:
    Tcl_SetResult(interp, "unknown method", NULL);
    return TCL_OK;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclXSLTTransform --
 *
 *  Performs an XSL transformation.
 *
 * Results:
 *  Result document created.
 *
 * Side effects:
 *  Memory allocated for result document.
 *
 *----------------------------------------------------------------------------
 */

static int
TclXSLTTransform(stylesheet, source, paramc, paramv)
    TclXSLT_Stylesheet *stylesheet;
    Tcl_Obj *source;
    int paramc;
    Tcl_Obj *CONST paramv[];
{
  xmlDocPtr doc, result;
  char **params = NULL;
  int nbparams = 0, i;
  GenericError_Info *errorInfoPtr;
  void *oldErrorCtx;
  xmlGenericErrorFunc old_xsltGenericError;

  errorInfoPtr = (GenericError_Info *) Tcl_Alloc(sizeof(GenericError_Info));
  errorInfoPtr->interp = stylesheet->interp;
  errorInfoPtr->stylesheet = stylesheet;
  errorInfoPtr->code = TCL_OK;
  errorInfoPtr->msg = NULL;

  if (TclDOM_GetDocFromObj(stylesheet->interp, source, &doc) != TCL_OK) {
    goto error;
  }

  params = (char **) Tcl_Alloc(sizeof(char **) * (paramc + 1));
  for (i = 0; i < paramc; i++) {
    params[nbparams++] = Tcl_GetStringFromObj(paramv[i++], NULL);
    params[nbparams++] = Tcl_GetStringFromObj(paramv[i], NULL);
  }
  params[nbparams] = NULL;

  /*
   * Perform the transformation
   */

  /*
   * Save the previous error context so that it can
   * be restored upon completion of the transformation.
   * This is necessary because transformations may occur
   * recursively (usually due to extensions).
   */
  old_xsltGenericError = xsltGenericError;
  oldErrorCtx = xsltGenericErrorContext;

  xsltSetGenericErrorFunc((void *) errorInfoPtr, TclXSLTGenericError);

  result = xsltApplyStylesheet(stylesheet->stylesheet, doc, (const char **)params);

  xsltSetGenericErrorFunc((void *) oldErrorCtx, old_xsltGenericError);

  if (result == NULL) {
    Tcl_Obj *resultPtr = Tcl_NewStringObj("no result document", -1);

    if (errorInfoPtr->msg) {
      Tcl_AppendToObj(resultPtr, ":\n", -1);
      Tcl_AppendObjToObj(resultPtr, errorInfoPtr->msg);
    }

    Tcl_SetObjResult(stylesheet->interp, resultPtr);
    goto error;
  }

  if (errorInfoPtr->code != TCL_OK && errorInfoPtr->msg && stylesheet->messagecommand) {

    /* We have produced a result, but there may possibly
     * have been errors.  Trouble is, there might also
     * have been some completely innocent messages.
     * -messageCommand is the only way to find out about these.
     */

    Tcl_Obj *cmdPtr = Tcl_DuplicateObj(stylesheet->messagecommand);
    if (Tcl_ListObjAppendElement(stylesheet->interp, cmdPtr, errorInfoPtr->msg) != TCL_OK) {
      goto error;
    }
    if (Tcl_GlobalEvalObj(stylesheet->interp, cmdPtr) != TCL_OK) {
      goto error;
    }

  }

  Tcl_SetObjResult(stylesheet->interp, TclDOM_CreateObjFromDoc(result));

  if (errorInfoPtr->msg) {
    Tcl_DecrRefCount(errorInfoPtr->msg);
  }
  Tcl_Free((char *) errorInfoPtr);
  Tcl_Free(params);

  return TCL_OK;

 error:

  if (errorInfoPtr->msg) {
    Tcl_DecrRefCount(errorInfoPtr->msg);
  }
  if (params) {
    Tcl_Free(params);
  }
  Tcl_Free((char *) errorInfoPtr);

  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclXSLTGenericError --
 *
 *  Handler for stylesheet errors.
 *
 *  NB. Cannot distinguish between errors and use of xsl:message element.
 *
 * Results:
 *  Stores error message.
 *
 * Side effects:
 *  Transform will return error condition.
 *
 *----------------------------------------------------------------------------
 */

static void
TclXSLTGenericError (void *ctx, const char *msg, ...)
{
  va_list args;
  char buf[2048];
  int len;
  GenericError_Info *errorInfoPtr = (GenericError_Info *) ctx;

  if (ctx < (void *) 0x1000) {
    fprintf(stderr, "TclXSLT: bad context\n");
    va_start(args,msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    return;
  }

  va_start(args,msg);
  len = vsnprintf(buf, 2047, msg, args);
  va_end(args);

  if (!errorInfoPtr->interp) {
    sprintf(dbgbuf, "TclXSLTGenericError: NULL interp, msg \"%s\"\n", buf);
    return;
  }

  if (errorInfoPtr->stylesheet->messagecommand) {

    Tcl_Obj *cmdPtr = Tcl_DuplicateObj(errorInfoPtr->stylesheet->messagecommand);
    if (Tcl_ListObjAppendElement(errorInfoPtr->interp, cmdPtr, Tcl_NewStringObj(buf, len)) != TCL_OK) {
      Tcl_BackgroundError(errorInfoPtr->interp);
      return;
    }
    if (Tcl_GlobalEvalObj(errorInfoPtr->interp, cmdPtr) != TCL_OK) {
      Tcl_BackgroundError(errorInfoPtr->interp);
      return;
    }

  } else {

    if (!errorInfoPtr->msg) {
      errorInfoPtr->msg = Tcl_NewObj();
      Tcl_IncrRefCount(errorInfoPtr->msg);
    }

    errorInfoPtr->code = TCL_ERROR;

    Tcl_AppendToObj(errorInfoPtr->msg, buf, len);

  }
}

/*
 *----------------------------------------------------------------------------
 *
 * TclXSLTExtensionCommand --
 *
 *  Command for xslt::extension command.
 *
 * Results:
 *  Depends on method.
 *
 * Side effects:
 *  Depends on method
 *
 *----------------------------------------------------------------------------
 */

static int
TclXSLTExtensionCommand(dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int method, new;
  TclXSLT_Extension *extinfo;
  Tcl_HashEntry *entry;

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "method ?args ...?");
    return TCL_ERROR;
  }

  if (Tcl_GetIndexFromObj(interp, objv[1], extensionCommandMethods,
			  "method", 0, &method) != TCL_OK) {
    return TCL_ERROR;
  }

  switch ((enum extensionCommandMethods) method) {

  case TCLXSLT_EXT_ADD:
    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 2, objv, "nsuri tcl-namespace");
      return TCL_ERROR;
    }

    if (xsltRegisterExtModule(Tcl_GetStringFromObj(objv[2], NULL),
			      TclXSLTExtInit,
			      TclXSLTExtShutdown)) {
      Tcl_SetResult(interp, "cannot register extension module", NULL);
    }

    extinfo = (TclXSLT_Extension *) Tcl_Alloc(sizeof(TclXSLT_Extension));
    extinfo->interp = interp;
    extinfo->nsuri = objv[2];
    Tcl_IncrRefCount(objv[2]);
    extinfo->tclns = objv[3];
    Tcl_IncrRefCount(objv[3]);

    extinfo->xformCtxt = NULL;

    entry = Tcl_CreateHashEntry(&extensions, Tcl_GetStringFromObj(objv[2], NULL), &new);

    if (!new) {
      Tcl_SetResult(interp, "extension already exists", NULL);
      Tcl_Free((char *) extinfo);
      return TCL_ERROR;
    }

    Tcl_SetHashValue(entry, extinfo);

    TclXSLT_RegisterAll(extinfo, (const xmlChar *) Tcl_GetStringFromObj(objv[2], NULL));

    Tcl_ResetResult(interp);

    break;

  case TCLXSLT_EXT_REMOVE:
    if (objc != 3) {
      Tcl_WrongNumArgs(interp, 2, objv, "nsuri");
      return TCL_ERROR;
    }

    /*
     * TODO: Remove previously registered elements and functions.
    */

    entry = Tcl_FindHashEntry(&extensions, Tcl_GetStringFromObj(objv[2], NULL));
    if (entry == NULL) {
      Tcl_SetResult(interp, "unknown XML Namespace URI", NULL);
      return TCL_ERROR;
    }

    extinfo = (TclXSLT_Extension *) Tcl_GetHashValue(entry);
    Tcl_DecrRefCount(extinfo->nsuri);
    Tcl_DecrRefCount(extinfo->tclns);
    Tcl_Free((char *) extinfo);

    Tcl_DeleteHashEntry(entry);

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
 * TclXSLTExtInit --
 *
 *  Load extensions into a transformation context.
 *
 * Results:
 *  Returns pointer to extension data.
 *  Elements and functions are pre-registered.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------------
 */

static void *
TclXSLTExtInit(ctxt, URI)
     xsltTransformContextPtr ctxt;
     const xmlChar *URI;
{
  Tcl_HashEntry *entry;
  TclXSLT_Extension *extinfo;

  entry = Tcl_FindHashEntry(&extensions, URI);
  if (entry == NULL) {
    /* Extension module was removed */
    return NULL;
  }

  extinfo = (TclXSLT_Extension *) Tcl_GetHashValue(entry);
  extinfo->xformCtxt = ctxt;

  return (void *) extinfo;
}

void
TclXSLT_RegisterAll(extinfo, nsuri)
    TclXSLT_Extension *extinfo;
    const xmlChar *nsuri;
{
  Tcl_Obj *cmdPtr, *objPtr;
  Tcl_Obj **reg;
  int ret, i, len;

  /*
   * Q: How to distinguish between extension elements and functions?
   * A: Use the formal parameters.  If the command can accept
   * a variable argument list, then it is registered as a function.
   * Otherwise it will be registered as an extension (and expected
   * to accept certain arguments).
   */

  cmdPtr = Tcl_NewStringObj("::xslt::getprocs ", -1);
  Tcl_IncrRefCount(cmdPtr);
  Tcl_AppendObjToObj(cmdPtr, extinfo->tclns);
  ret = Tcl_EvalObjEx(extinfo->interp, cmdPtr, TCL_EVAL_GLOBAL|TCL_EVAL_DIRECT);
  objPtr = Tcl_GetObjResult(extinfo->interp);
  Tcl_IncrRefCount(objPtr);
  Tcl_DecrRefCount(cmdPtr);

  if (ret != TCL_OK || objPtr == NULL) {
    /*
     * Something went wrong, therefore nothing to register.
     */
    return;
  }

  ret = Tcl_ListObjGetElements(extinfo->interp, objPtr, &len, &reg);
  if (ret != TCL_OK || len != 2) {
    /*
     * Something went wrong, therefore nothing to register.
     */
    return;
  }

  /*
   * reg[0] contains extension elements
   * reg[1] contains extension functions
   */

  /*
   * First register the extension elements.
   */

  ret = Tcl_ListObjLength(extinfo->interp, reg[0], &len);
  if (ret == TCL_OK && len > 0) {
    for (i = 0; i < len; i++) {

      if (Tcl_ListObjIndex(extinfo->interp, reg[0], i, &objPtr) != TCL_OK) {
        continue;
      }

      xsltRegisterExtModuleElement((const xmlChar *) Tcl_GetStringFromObj(objPtr, NULL),
                             nsuri,
                             (xsltPreComputeFunction) TclXSLTExtElementPreComp,
                             (xsltTransformFunction) TclXSLTExtElementTransform);
    }
  }

  /*
   * Now register the extension functions.
   */

  ret = Tcl_ListObjLength(extinfo->interp, reg[1], &len);
  if (ret != TCL_OK || len == 0) {
    return;
  }

  for (i = 0; i < len; i++) {

    if (Tcl_ListObjIndex(extinfo->interp, reg[1], i, &objPtr) != TCL_OK) {
      continue;
    }

    xsltRegisterExtModuleFunction((const xmlChar *) Tcl_GetStringFromObj(objPtr, NULL),
    	nsuri,
    	TclXSLTExtFunction);
  }

  Tcl_DecrRefCount(objPtr);

  return;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclXSLTExtElementPreComp --
 *
 *  Compilation step for extension element.
 *
 * Results:
 *  Not currently used.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------------
 */

static void 
TclXSLTExtElementPreComp(style, inst, function)
    xsltStylesheetPtr style;
    xmlNodePtr inst;
    xsltTransformFunction function;
{
  return;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclXSLTExtElementTransform --
 *
 *  Implements extension element.
 *
 * Results:
 *  Returns string returned by Tcl command evaluation.
 *
 * Side effects:
 *  Depends on Tcl command evaluated.
 *
 *----------------------------------------------------------------------------
 */

static void 
TclXSLTExtElementTransform(ctxt, node, inst, comp)
    xsltTransformContextPtr ctxt; /* unused */
    xmlNodePtr node;
    xmlNodePtr inst;
    xsltStylePreCompPtr comp; /* unused */
{
  TclXSLT_Extension *extinfo;
  Tcl_HashEntry *entry;
  Tcl_Obj *cmdPtr;
  int ret;

  if (inst == NULL) {
    return;
  }

  entry = Tcl_FindHashEntry(&extensions, inst->ns->href);
  if (entry == NULL) {
    /*
     * Cannot find extension module.
     * Must have been removed.
     */
    return;
  }

  extinfo = (TclXSLT_Extension *) Tcl_GetHashValue(entry);

  /*
   * Start constructing the script by first defining the command.
   */

  cmdPtr = Tcl_DuplicateObj(extinfo->tclns);
  Tcl_AppendStringsToObj(cmdPtr, "::", inst->name, NULL);

  if (Tcl_ListObjAppendElement(extinfo->interp, cmdPtr, TclDOM_CreateObjFromNode(node)) != TCL_OK) {
    Tcl_DecrRefCount(cmdPtr);
    return;
  }

  /*
   * Converting the stylesheet node to a TclDOM node may clobber the
   * _private pointer.  It would be nice to find the equivalent node
   * in the original DOM tree, but it may not even exist anymore :-(
   *
   * TODO: make extension elements more effective, and allow
   * pre-computation.
   */

  /*
   * Now evaluate the complete command.
   * Can't propagqte a return error result to
   * XSLT, so flag background error instead.
   */
  ret = Tcl_EvalObjEx(extinfo->interp, cmdPtr, TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
  if (ret != TCL_OK) {
    Tcl_BackgroundError(extinfo->interp);
  }
}

/*
 *----------------------------------------------------------------------------
 *
 * TclXSLTExtFunction --
 *
 *  Handles evaluation of an extension function.
 *
 * Results:
 *  Returns string returned by Tcl command evaluation.
 *
 * Side effects:
 *  Depends on Tcl command evaluated.
 *
 *----------------------------------------------------------------------------
 */

static void 
TclXSLTExtFunction(xpathCtxt, nargs)
     xmlXPathParserContextPtr xpathCtxt;
     int nargs;
{
  xsltTransformContextPtr xformCtxt;
  TclXSLT_Extension *extinfo;
  Tcl_Obj *cmdPtr, *resultPtr;
  xmlXPathObjectPtr obj;
  int ret, len;

  xformCtxt = xsltXPathGetTransformContext(xpathCtxt);

  /*
   * In order to find the instance data we need the
   * XML Namespace URI of this function.
   */

  extinfo = (TclXSLT_Extension *) xsltGetExtData(xformCtxt,
						 xpathCtxt->context->functionURI);

  /*
   * Start constructing the script by first defining the command.
   */

  cmdPtr = Tcl_DuplicateObj(extinfo->tclns);
  Tcl_IncrRefCount(cmdPtr);
  Tcl_AppendStringsToObj(cmdPtr, "::", xpathCtxt->context->function, NULL);

  /*
   * Each argument on the stack is converted to a Tcl_Obj
   * of an appropriate type and passed as an argument to the Tcl command.
   */

  while (nargs) {
    Tcl_Obj *objv[2];

    obj = (xmlXPathObjectPtr) valuePop(xpathCtxt);
    if (obj == NULL) {
      xmlXPathSetError(xpathCtxt, XPATH_INVALID_OPERAND);
      Tcl_DecrRefCount(cmdPtr);
      return;
    }

    objv[0] = TclXSLT_ConvertXPathObjToTclObj(extinfo->interp, obj);
    objv[1] = NULL;
    if (Tcl_ListObjReplace(extinfo->interp, cmdPtr, 1, 0, 1, objv) != TCL_OK) {
      Tcl_BackgroundError(extinfo->interp);
      Tcl_DecrRefCount(objv[0]);
      Tcl_DecrRefCount(cmdPtr);
      return;
    }

    /* When should this XPath object be freed?
     * Immediately before returning from the function call?
     * What if the application retains a pointer to it?
     * If the application destroys the contents, then memory
     * will leak because the XPath object is not freed.
     *
     * TODO: take a copy of the object's content and pass that
     * to the application callback.  That would allow this object
     * to be freed and allow the application to manage the copy.
     
     xmlXPathFreeObject(obj);
     */

    nargs--;
  }

  ret = Tcl_EvalObjEx(extinfo->interp, cmdPtr, TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
  resultPtr = Tcl_GetObjResult(extinfo->interp);
  Tcl_DecrRefCount(cmdPtr);
  Tcl_IncrRefCount(resultPtr);

  if (ret == TCL_OK) {
    obj = TclXSLT_ConvertTclObjToXPathObj(extinfo->interp, resultPtr);
    valuePush(xpathCtxt, obj);
  } else {
    xmlGenericError(xmlGenericErrorContext,
		    Tcl_GetStringFromObj(resultPtr, NULL));
    /* Need to define a new error code - this is the closest in meaning */
    xpathCtxt->error = XPATH_UNKNOWN_FUNC_ERROR;
  }

  Tcl_DecrRefCount(resultPtr);

}

/*
 *----------------------------------------------------------------------------
 *
 * TclXSLT_ConvertTclObjToXPathObj --
 *
 *  Convert a Tcl Object to an XPath object.
 *  Data type is preserved, with nodesets being
 *  mapped from a list of nodes.
 *
 * Results:
 *  XPath Object.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------------
 */

static xmlXPathObjectPtr
TclXSLT_ConvertTclObjToXPathObj(interp, objPtr)
     Tcl_Interp *interp;
     Tcl_Obj *objPtr;
{
  xmlNodePtr nodePtr;
  xmlDocPtr docPtr;

  if (TclDOM_GetNodeFromObj(interp, objPtr, &nodePtr) == TCL_OK) {
    return xmlXPathNewNodeSet(nodePtr);
  }
  
  /*
   * BUG: This is corrupting objPtr for some unknown reason.
   */
   if (TclDOM_GetDocFromObj(interp, objPtr, &docPtr) == TCL_OK) {
    return xmlXPathNewNodeSet((xmlNodePtr) docPtr);

  }
  
  if (objPtr->typePtr == Tcl_GetObjType("int") ||
      objPtr->typePtr == Tcl_GetObjType("double")) {
    double number;

    if (Tcl_GetDoubleFromObj(interp, objPtr, &number) == TCL_OK) {
      return xmlXPathNewFloat(number);
    } else {
      return NULL;
    }
  } else if (objPtr->typePtr == Tcl_GetObjType("boolean")) {
    int bool;

    if (Tcl_GetBooleanFromObj(interp, objPtr, &bool) == TCL_OK) {
      return xmlXPathNewBoolean(bool);
    } else {
      return NULL;
    }
  } else if (objPtr->typePtr == Tcl_GetObjType("list")) {
    /*
     * If each of the elements can be converted to a node,
     * then return a nodeset.
     */

    int i, len;
    Tcl_Obj **listPtr;
    xmlNodeSetPtr nset;

    Tcl_ListObjGetElements(interp, objPtr, &len, &listPtr);
    if (len == 0) {
      return xmlXPathNewNodeSet(NULL);
    }

    /*
     * First pass: check that the elements are all nodes.
     */
    for (i = 0; i < len; i++) {
      if (TclDOM_GetDocFromObj(interp, listPtr[i], &docPtr) == TCL_OK) {
        continue;
      }
      if (TclDOM_GetNodeFromObj(interp, listPtr[i], &nodePtr) != TCL_OK) {
        return xmlXPathNewString(Tcl_GetStringFromObj(objPtr, NULL));
      }
    }
    /*
     * Now go ahead and create the nodeset (we already did the hard
     * work to create internal reps in pass 1).
     */
    if (TclDOM_GetDocFromObj(interp, listPtr[0], &docPtr) == TCL_OK) {
      nset = xmlXPathNodeSetCreate((xmlNodePtr) docPtr);
    } else {
      TclDOM_GetNodeFromObj(interp, listPtr[0], &nodePtr);
      nset = xmlXPathNodeSetCreate(nodePtr);
    }
    for (i = 1; i < len; i++) {
      if (TclDOM_GetDocFromObj(interp, listPtr[i], &docPtr) == TCL_OK) {
        xmlXPathNodeSetAdd(nset, (xmlNodePtr) docPtr);
      } else {
        TclDOM_GetNodeFromObj(interp, listPtr[i], &nodePtr);
        xmlXPathNodeSetAdd(nset, nodePtr);
      }
    }
    return xmlXPathWrapNodeSet(nset);

  } else {
    return xmlXPathNewString(Tcl_GetStringFromObj(objPtr, NULL));
  }
}

/*
 *----------------------------------------------------------------------------
 *
 * TclXSLT_ConvertXPathObjToTclObj --
 *
 *  Convert an XPath object to a Tcl Object.
 *  Data type is preserved, with nodesets being
 *  mapped to a list of nodes.
 *
 * Results:
 *  Tcl Object.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------------
 */

static Tcl_Obj *
TclXSLT_ConvertXPathObjToTclObj(interp, xpobj)
     Tcl_Interp *interp;
     xmlXPathObjectPtr xpobj;
{
  Tcl_Obj *objPtr;
  int i;

  switch (xpobj->type) {
    case XPATH_XSLT_TREE:
    case XPATH_NODESET:

      objPtr = Tcl_NewListObj(0, NULL);
      for (i = 0; i < xpobj->nodesetval->nodeNr; i++) {
        Tcl_Obj *nodeObjPtr;
        nodeObjPtr = TclDOM_CreateObjFromNode(xpobj->nodesetval->nodeTab[i]);
        Tcl_ListObjAppendElement(interp, objPtr, nodeObjPtr);
      }

      break;

    case XPATH_BOOLEAN:
      objPtr = Tcl_NewBooleanObj(xpobj->boolval);
      break;
      
    case XPATH_NUMBER:
      objPtr = Tcl_NewDoubleObj(xpobj->floatval);
      break;

    case XPATH_STRING:
    case XPATH_UNDEFINED:
    case XPATH_POINT:
    case XPATH_RANGE:
    case XPATH_LOCATIONSET:
    case XPATH_USERS:
    default:
      objPtr = Tcl_NewStringObj(xmlXPathCastToString(xpobj), -1);

      break;
  }

  return objPtr;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclXSLTExtShutdown --
 *
 *  Clean up.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------------
 */

static void
TclXSLTExtShutdown(ctxt, URI, userdata)
     xsltTransformContextPtr ctxt;
     const xmlChar *URI;
     void *userdata;
{
  /* Nothing to do */
}
