/*
 * tclexpat.c --
 *
 *	TclXML driver for James Clark's expat XML parser
 *
 * Copyright (c) 1998-2003 Steve Ball, Zveno Pty Ltd
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
 * $Id: tclexpat.c,v 1.15 2003/02/25 04:09:00 balls Exp $
 *
 */

#include <tcl.h>
#include "tclxml.h"
#include <xmlparse.h>

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

/*
 * The structure below is used to refer to an expat parser object.
 */

typedef struct TclExpatInfo {
  XML_Parser parser;		/* The expat parser structure */
  Tcl_Interp *interp;		/* Interpreter for this instance */

  TclXML_Info *xmlinfo;		/* Generic data structure */

} TclExpatInfo;

/*
 * Prototypes for procedures defined later in this file:
 */

static ClientData TclExpatCreate _ANSI_ARGS_((Tcl_Interp *interp, 
                    TclXML_Info *xmlinfo));
static ClientData TclExpatCreateEntityParser _ANSI_ARGS_((
                    Tcl_Interp *interp, ClientData clientData));
static int	TclExpatDelete _ANSI_ARGS_((ClientData clientData));
static int	TclExpatParse _ANSI_ARGS_((ClientData clientData, 
                    char *data, int len, int final));
static int	TclExpatConfigure _ANSI_ARGS_((ClientData clientdata, 
                    int objc, Tcl_Obj *CONST objv[]));
static int	TclExpatCget _ANSI_ARGS_((ClientData clientData, 
                    int objc, Tcl_Obj *CONST objv[]));
static int	TclExpatGet _ANSI_ARGS_((ClientData clientData, 
                    int objc, Tcl_Obj *CONST objv[]));

static void	TclExpatElementStartHandler _ANSI_ARGS_((
	            void *userData,
		    const XML_Char *name,
		    const XML_Char **atts));
static void	TclExpatElementEndHandler _ANSI_ARGS_((
                    void *userData,
		    const XML_Char *name));
static void	TclExpatCharacterDataHandler _ANSI_ARGS_((
                    void *userData,
		    const XML_Char *s,
		    int len));
static void	TclExpatProcessingInstructionHandler _ANSI_ARGS_((
                    void *userData,
		    const XML_Char *target,
		    const XML_Char *data));
static void	TclExpatDefaultHandler _ANSI_ARGS_((
                    void *userData,
		    const XML_Char *s,
		    int len));
static void	TclExpatUnparsedDeclHandler _ANSI_ARGS_((
		    void *userData,
		    const XML_Char *entityName,
		    const XML_Char *base,
		    const XML_Char *systemId,
		    const XML_Char *publicId,
		    const XML_Char *notationName));
static void	TclExpatNotationDeclHandler _ANSI_ARGS_((
		    void *userData,
		    const XML_Char *notationName,
		    const XML_Char *base,
		    const XML_Char *systemId,
		    const XML_Char *publicId));
static void	TclExpatCommentHandler _ANSI_ARGS_((
		    void *userData,
		    const XML_Char *data));
static int	TclExpatNotStandaloneHandler _ANSI_ARGS_((
		    void *userData));
static void	TclExpatStartCdataSectionHandler _ANSI_ARGS_((
		    void *userData));
static void	TclExpatEndCdataSectionHandler _ANSI_ARGS_((
		    void *userData));
static void	TclExpatElementDeclHandler _ANSI_ARGS_((
		    void *userData,
		    const XML_Char *name,
		    XML_Char ***contentspec));
static void	TclExpatAttlistDeclHandler _ANSI_ARGS_((
		    void *userData,
		    const XML_Char *name,
		    XML_Char ***attributes));
static void	TclExpatStartDoctypeDeclHandler _ANSI_ARGS_((
		    void *userData,
		    const XML_Char *doctypeName));
static void	TclExpatEndDoctypeDeclHandler _ANSI_ARGS_((
		    void *userData));

static int	TclExpatExternalEntityRefHandler _ANSI_ARGS_((
     XML_Parser parser,
     const XML_Char *context,
     const XML_Char *base,
     const XML_Char *systemId,
     const XML_Char *publicId));
static int	TclExpatUnknownEncodingHandler _ANSI_ARGS_((
     void *encodingHandlerData,
     const XML_Char *name,
     XML_Encoding *info
));

EXTERN int	Tclexpat_Init _ANSI_ARGS_((Tcl_Interp *interp));

/*
 *----------------------------------------------------------------------------
 *
 * Tclexpat_Init --
 *
 *	Initialisation routine for loadable module
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Registers xerces XML parser.
 *
 *----------------------------------------------------------------------------
 */

int
Tclexpat_Init (interp)
     Tcl_Interp *interp; /* Interpreter to initialise. */
{
  TclXML_ParserClassInfo *classinfo;

#ifdef USE_TCL_STUBS
  if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
    return TCL_ERROR;
  }
#endif
#ifdef USE_TCLXML_STUBS
  if (TclXML_InitStubs(interp, TCLXML_VERSION, 1) == NULL) {
    return TCL_ERROR;
  }
#endif

  classinfo = (TclXML_ParserClassInfo *) ckalloc(sizeof(TclXML_ParserClassInfo));
  classinfo->name = Tcl_NewStringObj("expat", -1);
  classinfo->create = TclExpatCreate;
  classinfo->createCmd = NULL;
  classinfo->createEntity = TclExpatCreateEntityParser;
  classinfo->createEntityCmd = NULL;
  classinfo->parse = TclExpatParse;
  classinfo->parseCmd = NULL;
  classinfo->configure = TclExpatConfigure;
  classinfo->configureCmd = NULL;
  classinfo->get = TclExpatGet;
  classinfo->getCmd = NULL;
  classinfo->destroy = TclExpatDelete;
  classinfo->destroyCmd = NULL;
  classinfo->reset = NULL;
  classinfo->resetCmd = NULL;

  if (TclXML_RegisterXMLParser(interp, classinfo) != TCL_OK) {
    Tcl_SetResult(interp, "unable to register parser", NULL);
    return TCL_ERROR;
  }

  if (Tcl_PkgProvide(interp, "xml::expat", TCLXML_VERSION) != TCL_OK) {
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatCreate --
 *
 *	Create an expat parser instance.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	This creates an expat parser.
 *
 *----------------------------------------------------------------------------
 */

static ClientData
TclExpatCreate(interp, xmlinfo)
     Tcl_Interp *interp;
     TclXML_Info *xmlinfo;
{
  TclExpatInfo *expat;

  /*
   * Create the data structures for this parser.
   */

  if (!(expat = (TclExpatInfo *) ckalloc(sizeof(TclExpatInfo)))) {
    ckfree((char *)expat);
    Tcl_SetResult(interp, "unable to create parser", NULL);
    return NULL;
  }
  expat->interp = interp;
  expat->xmlinfo = xmlinfo;

  if (!(expat->parser = XML_ParserCreate(NULL))) {
    Tcl_SetResult(interp, "unable to create expat parser", NULL);
    ckfree((char *)expat);
    return NULL;
  }
  
  /*
   * Set all handlers for the parser.
   */

  XML_SetElementHandler(expat->parser, TclExpatElementStartHandler,
			TclExpatElementEndHandler);
  XML_SetCharacterDataHandler(expat->parser, TclExpatCharacterDataHandler);
  XML_SetProcessingInstructionHandler(expat->parser,
	  TclExpatProcessingInstructionHandler);
  XML_SetDefaultHandler(expat->parser, TclExpatDefaultHandler);

  XML_SetUnparsedEntityDeclHandler(expat->parser,
	  TclExpatUnparsedDeclHandler);
  XML_SetNotationDeclHandler(expat->parser, TclExpatNotationDeclHandler);

  XML_SetExternalEntityRefHandler(expat->parser,
	  TclExpatExternalEntityRefHandler);

  XML_SetUnknownEncodingHandler(expat->parser, TclExpatUnknownEncodingHandler,
	  (void *) xmlinfo);
  
  /* Added by ericm@scriptics.com, 1999.6.25 */
  /* Tell expat to use the TclExpat comment handler */
  XML_SetCommentHandler(expat->parser, TclExpatCommentHandler);
  
  /* Tell expat to use the TclExpat "not standalone" handler */
  XML_SetNotStandaloneHandler(expat->parser, TclExpatNotStandaloneHandler);

#ifdef TCLXML_CDATASECTIONS
  /* Tell expat to use the TclExpat CdataSection handlers */
  XML_SetCdataSectionHandler(expat->parser, TclExpatStartCdataSectionHandler,
	  TclExpatEndCdataSectionHandler);
#endif /* TCLXML_CDATASECTIONS */

  /* Tell expat to use the TclExpat Element decl handler */
  XML_SetElementDeclHandler(expat->parser, TclExpatElementDeclHandler);

  /* Tell expat to use the TclExpat Attlist decl handler */
  XML_SetAttlistDeclHandler(expat->parser, TclExpatAttlistDeclHandler);

  /* Tell expat to use the TclExpat DOCTYPE decl handlers */
  XML_SetDoctypeDeclHandler(expat->parser,
	  TclExpatStartDoctypeDeclHandler,
	  TclExpatEndDoctypeDeclHandler);

  XML_SetUserData(expat->parser, (void *) expat);
  
  return (ClientData) expat;
}

/*
 *----------------------------------------------------------------------
 *
 * TclExpatCreateEntity --
 *
 *	Create an expat entity parser, based on the original
 *      parser referred to by parent.
 *
 * Results:
 *	New external entity parser created and initialized.
 *
 * Side effects:
 *	The TclExpatInfo struct pointed to by expat is modified.
 *
 *----------------------------------------------------------------------
 */

static ClientData
TclExpatCreateEntityParser(interp, clientData)
     Tcl_Interp *interp;
     ClientData clientData;
{
  TclExpatInfo *parent = (TclExpatInfo *) clientData;
  TclExpatInfo *expat;

  if (!(expat = (TclExpatInfo *) ckalloc(sizeof(TclExpatInfo)))) {
    ckfree((char *)expat);
    Tcl_SetResult(interp, "unable to create parser", NULL);
    return NULL;
  }
  expat->interp = parent->interp;
  expat->xmlinfo = parent->xmlinfo;

    if (!(expat->parser = XML_ExternalEntityParserCreate(parent->parser,
	    (XML_Char *) Tcl_GetUnicode(parent->xmlinfo->context), NULL))) {
	Tcl_SetResult(interp, "unable to create expat external entity parser",
		NULL);
	return NULL;
    }
  
    XML_SetUserData(expat->parser, (void *) expat);
    return (ClientData) expat;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatDelete --
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

static int
TclExpatDelete(clientData)
     ClientData clientData;
{
  TclExpatInfo *expat = (TclExpatInfo *) clientData;

  XML_ParserFree(expat->parser);
  ckfree((char *)expat);

  return TCL_OK;
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

static int
TclExpatParse(clientData, data, len, final)
     ClientData clientData;
     char *data;
     int len;
{
  TclExpatInfo *expat = (TclExpatInfo *) clientData;
  int result;
  char s[255];

  result = XML_Parse(expat->parser, 
		     data, len, 
		     final);

  if (!result) {
    Tcl_ResetResult(expat->interp);
    sprintf(s, "%d", XML_GetCurrentLineNumber(expat->parser));
    Tcl_AppendResult(expat->interp, "error \"",
	    XML_ErrorString(XML_GetErrorCode(expat->parser)),
	    "\" at line ", s, " character ", NULL);
    sprintf(s, "%d", XML_GetCurrentColumnNumber(expat->parser));
    Tcl_AppendResult(expat->interp, s, NULL);

    return TCL_ERROR;
  }

  return TCL_OK;
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

static int
TclExpatConfigure (clientData, objc, objv)
     ClientData clientData;
     int objc;
     Tcl_Obj *CONST objv[];
{
  TclExpatInfo *expat = (TclExpatInfo *) clientData;

  static CONST84 char *switches[] = {
    "-baseurl", 
    "-defaultexpandinternalentities",
    "-paramentityparsing",
    (char *) NULL
  };
  enum switches {
    EXPAT_BASE, 
    EXPAT_DEFAULTEXPANDINTERNALENTITIES,
    EXPAT_PARAMENTITYPARSING,
  };
  static CONST84 char *paramEntityParsingValues[] = {
      "always",
      "never",
      "notstandalone"
  };
  enum paramEntityParsingValues {
      EXPAT_PARAMENTITYPARSINGALWAYS,
      EXPAT_PARAMENTITYPARSINGNEVER,
      EXPAT_PARAMENTITYPARSINGNOTSTANDALONE
  };

  int index, value, bool;
  Tcl_Obj *CONST *objPtr = objv;

  while (objc > 1) {
    if (Tcl_GetIndexFromObj(expat->interp, objPtr[0], switches,
			    "switch", 0, &index) != TCL_OK) {
      Tcl_ResetResult(expat->interp);
      return TCL_OK;
    }
    switch ((enum switches) index) {

      case EXPAT_BASE:			/* -base */

	if (XML_SetBase(expat->parser, (const XML_Char*)Tcl_GetString(objPtr[1])) == 0) {
	  Tcl_SetResult(expat->interp, "unable to set base URL", NULL);
	  return TCL_ERROR;
	}
	break;

      case EXPAT_DEFAULTEXPANDINTERNALENTITIES: 

        /* -defaultexpandinternalentities */
	/* ericm@scriptics */
	if (Tcl_GetBooleanFromObj(expat->interp, objPtr[1], &bool) != TCL_OK) {
	  return TCL_ERROR;
	}
	
        XML_SetDefaultExpandInternalEntities(expat->parser, bool);

	break;

      case EXPAT_PARAMENTITYPARSING: /* -paramentityparsing */
	  /* ericm@scriptics */
	  if (Tcl_GetIndexFromObj(expat->interp, objPtr[1], paramEntityParsingValues,
		  "value", 0, &value) != TCL_OK) {
	      return TCL_ERROR;
	  }
	  switch ((enum paramEntityParsingValues) value) {
	      case EXPAT_PARAMENTITYPARSINGALWAYS:
		  XML_SetParamEntityParsing(expat->parser,
			  XML_PARAM_ENTITY_PARSING_ALWAYS);
		  break;
	      case EXPAT_PARAMENTITYPARSINGNEVER:
		  XML_SetParamEntityParsing(expat->parser,
			  XML_PARAM_ENTITY_PARSING_NEVER);
		  break;
	      case EXPAT_PARAMENTITYPARSINGNOTSTANDALONE:
		  XML_SetParamEntityParsing(expat->parser,
			  XML_PARAM_ENTITY_PARSING_UNLESS_STANDALONE);
		  break;
	  }
	  break;

    default:
      return TCL_OK;
	  break;

    }

    objPtr += 2;
    objc -= 2;

  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatGet --
 *
 *	Returns runtime parser information, depending on option
 *      ericm@scriptics.com, 1999.6.28
 *
 * Results:
 *	Option value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */

static int
TclExpatGet (clientData, objc, objv)
     ClientData clientData;
     int objc;
     Tcl_Obj *CONST objv[];
{
  TclExpatInfo *expat = (TclExpatInfo *) clientData;
  static CONST84 char *switches[] = {
    "-specifiedattributecount",
    "-currentbytecount",
    "-currentlinenumber",
    "-currentcolumnnumber",
    "-currentbyteindex",
    (char *) NULL
  };
  enum switches {
    EXPAT_SPECIFIEDATTRCOUNT,
    EXPAT_CURRENTBYTECOUNT,
    EXPAT_CURRENTLINENUMBER,
    EXPAT_CURRENTCOLUMNNUMBER,
    EXPAT_CURRENTBYTEINDEX
  };
  int index, doParse = 0;
  Tcl_Obj *resultPtr;

  if (objc > 1) {
    Tcl_SetResult(expat->interp, "Only one value may be requested at a time", 
		  TCL_STATIC);
    return TCL_ERROR;
  }

  if (Tcl_GetIndexFromObj(expat->interp, objv[0], switches,
			  "switch", 0, &index) != TCL_OK) {
    return TCL_ERROR;
  }

  resultPtr = Tcl_GetObjResult(expat->interp);
  
  switch ((enum switches) index) {
    
    case EXPAT_SPECIFIEDATTRCOUNT:

      Tcl_SetIntObj(resultPtr, XML_GetSpecifiedAttributeCount(expat->parser));
      return TCL_OK;
      break;

    case EXPAT_CURRENTBYTECOUNT:
      
      Tcl_SetIntObj(resultPtr, XML_GetCurrentByteCount(expat->parser));
      return TCL_OK;
      break;

    case EXPAT_CURRENTLINENUMBER:
      
      Tcl_SetIntObj(resultPtr, XML_GetCurrentLineNumber(expat->parser));
      return TCL_OK;
      break;

    case EXPAT_CURRENTCOLUMNNUMBER:

      Tcl_SetIntObj(resultPtr, XML_GetCurrentColumnNumber(expat->parser));
      return TCL_OK;
      break;

    case EXPAT_CURRENTBYTEINDEX:
      
      Tcl_SetLongObj(resultPtr, XML_GetCurrentByteIndex(expat->parser));
      return TCL_OK;
      break;

    default:

      return TCL_ERROR;
      break;
  }

  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpat*Handler --
 *
 *	Called by expat for various document features.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Event passed to TclXML generic layer.
 *
 *----------------------------------------------------------------------------
 */

static void TclExpatElementStartHandler(userdata, name, atts)
     void *userdata;
     const XML_Char *name;
     const XML_Char **atts;
{
  TclExpatInfo *expat = (TclExpatInfo *) userdata;
  Tcl_Obj *attListObj;
  int count;

  attListObj = Tcl_NewListObj(0, NULL);
  for (count = 0; atts[count]; count += 2) {
    Tcl_ListObjAppendElement(NULL, attListObj, Tcl_NewStringObj((XML_Char*)atts[count], -1));
    Tcl_ListObjAppendElement(NULL, attListObj, Tcl_NewStringObj((XML_Char*)atts[count + 1], -1));
  }

  TclXML_ElementStartHandler(
        expat->xmlinfo,
	Tcl_NewStringObj((XML_Char*)name, -1),
	attListObj);
}

static void TclExpatElementEndHandler(userdata, name)
     void *userdata;
     const XML_Char *name;
{
  TclExpatInfo *expat = (TclExpatInfo *) userdata;

  TclXML_ElementEndHandler(
        expat->xmlinfo,
	Tcl_NewStringObj((XML_Char*)name, -1));
}

static void TclExpatCharacterDataHandler(userdata, s, len)
     void *userdata;
     const XML_Char *s;
     int len;
{
  TclExpatInfo *expat = (TclExpatInfo *) userdata;

  TclXML_CharacterDataHandler(
        expat->xmlinfo,
	Tcl_NewStringObj((XML_Char*)s, len));
}

static void TclExpatProcessingInstructionHandler(userdata, target, data)
     void *userdata;
     const XML_Char *target;
     const XML_Char *data;
{
  TclExpatInfo *expat = (TclExpatInfo *) userdata;

  TclXML_ProcessingInstructionHandler(
        expat->xmlinfo,
	Tcl_NewStringObj((XML_Char*)target, -1),
	Tcl_NewStringObj((XML_Char*)data, -1));
}

static void TclExpatDefaultHandler(userdata, s, len)
     void *userdata;
     const XML_Char *s;
     int len;
{
  TclExpatInfo *expat = (TclExpatInfo *) userdata;

  TclXML_DefaultHandler(
        expat->xmlinfo,
	Tcl_NewStringObj((XML_Char*)s, len));
}

static void TclExpatUnparsedDeclHandler(userdata, entityName, base, systemId, publicId, notationName)
     void *userdata;
     const XML_Char *entityName;
     const XML_Char *base;
     const XML_Char *systemId;
     const XML_Char *publicId;
     const XML_Char *notationName;
{
  TclExpatInfo *expat = (TclExpatInfo *) userdata;

  TclXML_UnparsedDeclHandler(
        expat->xmlinfo,
	Tcl_NewStringObj((XML_Char*)entityName, -1),
	Tcl_NewStringObj((XML_Char*)base, -1),
	Tcl_NewStringObj((XML_Char*)systemId, -1),
	Tcl_NewStringObj((XML_Char*)publicId, -1),
	Tcl_NewStringObj((XML_Char*)notationName, -1)
	);
}

static void TclExpatNotationDeclHandler(userdata, notationName, base, systemId, publicId)
     void *userdata;
     const XML_Char *notationName;
     const XML_Char *base;
     const XML_Char *systemId;
     const XML_Char *publicId;
{
  TclExpatInfo *expat = (TclExpatInfo *) userdata;

  TclXML_NotationDeclHandler(
        expat->xmlinfo,
	Tcl_NewStringObj((XML_Char*)notationName, -1),
	Tcl_NewStringObj((XML_Char*)base, -1),
	Tcl_NewStringObj((XML_Char*)systemId, -1),
	Tcl_NewStringObj((XML_Char*)publicId, -1)
	);
}

static void TclExpatCommentHandler(userdata, data)
     void *userdata;
     const XML_Char *data;
{
  TclExpatInfo *expat = (TclExpatInfo *) userdata;

  TclXML_CommentHandler(
        expat->xmlinfo,
	Tcl_NewStringObj((XML_Char*)data, -1));
}

static int TclExpatNotStandaloneHandler(userdata)
     void *userdata;
{
  TclExpatInfo *expat = (TclExpatInfo *) userdata;

  return TclXML_NotStandaloneHandler(expat->xmlinfo);
}

static void TclExpatStartCdataSectionHandler(userdata)
     void *userdata;
{
  TclExpatInfo *expat = (TclExpatInfo *) userdata;

  /*
   * TclXML makes no distinction between PCDATA and CDATA
   */
}

static void
TclExpatEndCdataSectionHandler(userdata)
     void *userdata;
{
  TclExpatInfo *expat = (TclExpatInfo *) userdata;

  /*
   * TclXML makes no distinction between PCDATA and CDATA
   */
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

static int
TclExpatExternalEntityRefHandler(parser, context, base, systemId, publicId)
     XML_Parser parser;
     const XML_Char *context;
     const XML_Char *base;
     const XML_Char *systemId;
     const XML_Char *publicId;
{
  TclExpatInfo *expat = (TclExpatInfo *) XML_GetUserData(parser);

  TclXML_ExternalEntityRefHandler(
        (ClientData) expat->xmlinfo,
	Tcl_NewStringObj((XML_Char*)context, -1), 
	Tcl_NewStringObj((XML_Char*)base, -1), 
	Tcl_NewStringObj((XML_Char*)systemId, -1), 
	Tcl_NewStringObj((XML_Char*)publicId, -1));

  return 1;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclExpatUnknownEncodingHandler --
 *
 *	Called by parser instance for processing a reference to a character in an 
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
     const XML_Char *name;
     XML_Encoding *info;
{
  int result;

  result = TclXML_UnknownEncodingHandler(
                 encodingHandlerData, 
		 Tcl_NewStringObj((XML_Char*)name, -1), 
		 (ClientData) info);

  return result;
}

static void TclExpatElementDeclHandler(userdata, name, contentspec)
     void *userdata;
     const XML_Char *name;
     XML_Char ***contentspec;
{
  TclExpatInfo *expat = (TclExpatInfo *) userdata;
  Tcl_Obj *contentspecObj;
  int count;

  contentspecObj = Tcl_NewListObj(0, NULL);
  for (count = 0; contentspec[0][count] != NULL; count++) {
    Tcl_ListObjAppendElement(expat->interp, contentspecObj, Tcl_NewStringObj(contentspec[0][count], -1));
  }

  TclXML_ElementDeclHandler(
        expat->xmlinfo,
	Tcl_NewStringObj((XML_Char*)name, -1),
	contentspecObj);
}

static void TclExpatAttlistDeclHandler(userdata, name, attributes)
     void *userdata;
     const XML_Char *name;
     XML_Char ***attributes;
{
  TclExpatInfo *expat = (TclExpatInfo *) userdata;
  Tcl_Obj *attdefnObj;
  int count;

  attdefnObj = Tcl_NewListObj(0, NULL);
  for (count = 0; (*attributes)[count] != NULL; count++) {
    Tcl_ListObjAppendElement(expat->interp, attdefnObj, Tcl_NewStringObj((*attributes)[count], -1));
  }

  TclXML_AttlistDeclHandler(
        expat->xmlinfo,
	Tcl_NewStringObj((XML_Char*)name, -1),
	attdefnObj);
}

static void TclExpatStartDoctypeDeclHandler(userdata, name)
     void *userdata;
     const XML_Char *name;
{
  TclExpatInfo *expat = (TclExpatInfo *) userdata;

  TclXML_StartDoctypeDeclHandler(
        expat->xmlinfo,
	Tcl_NewStringObj((XML_Char*)name, -1));
}

static void TclExpatEndDoctypeDeclHandler(userdata)
     void *userdata;
{
  TclExpatInfo *expat = (TclExpatInfo *) userdata;

  TclXML_EndDoctypeDeclHandler(expat->xmlinfo);
}
