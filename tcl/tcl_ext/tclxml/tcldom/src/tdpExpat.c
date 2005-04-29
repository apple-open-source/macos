/*
 * tdpExpat.c --
 *
 * This file contains routines for manipulating DOM
 * object representations.
 * 
 *
 * Copyright (c) 1998 Steve Ball, Zveno Pty Ltd
 * Modified by Ajuba Solutions
 * Changes copyright (c) 1999-2000 Ajuba Solutions
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
 * $Id: tdpExpat.c,v 1.16 2003/03/28 20:41:11 jenglish Exp $
 *
 */

#include <string.h>
#include "tclDomProInt.h"
#include <expat.h>

/*
 * Prototypes for procedures defined later in this file:
 */

static int 	isExtender(int c);
static int 	isCombiningChar(int c);
static int 	isBaseChar(int c);
static int 	isIdeographic(int c);
static int 	isLetter(int c);

static void	TclDomExpatElementStartHandler _ANSI_ARGS_((void *userdata,
		    const XML_Char *name, const XML_Char **atts));
static void	TclDomExpatElementEndHandler _ANSI_ARGS_((void *userData,
		    const XML_Char *name));
static void	TclDomExpatCharacterDataHandler _ANSI_ARGS_((void *userData,
		    const XML_Char *s, int len));
static void	TclDomExpatProcessingInstructionHandler _ANSI_ARGS_((
		    void *userData, const XML_Char *target,
		    const XML_Char *data));
static int	TclDomExpatExternalEntityRefHandler _ANSI_ARGS_((
		    XML_Parser parser, const XML_Char *openEntityNames,
		    const XML_Char *base, const XML_Char *systemId,
		    const XML_Char *publicId));
static void	TclDomExpatDefaultHandler _ANSI_ARGS_ ((void *userData,
		    const XML_Char *s, int len));
static void	TclDomExpatUnparsedDeclHandler _ANSI_ARGS_ ((void *userData,
		    const XML_Char *entityname, const XML_Char *base,
		    const XML_Char *systemId, const XML_Char *publicId,
		    const XML_Char *notationName));
static void	TclDomExpatNotationDeclHandler _ANSI_ARGS_ ((void *userData,
		    const XML_Char *notationName, const XML_Char *base,
		    const XML_Char *systemId, const XML_Char *publicId));
static int	TclDomExpatUnknownEncodingHandler _ANSI_ARGS_ ((
		    void *encodingHandlerData, const XML_Char *name,
		    XML_Encoding *info));
static void	SerializeWalk(TclDomNode *nodePtr, Tcl_DString *output);
static void	RemoveAttributeFromArray(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr, TclDomNode *nodePtr,
		    TclDomAttributeNode *attributeNodePtr);
static void	SetAttributeInArray(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr, 
		    TclDomNode *nodePtr,
		    TclDomAttributeNode *attributeNodePtr);
static void	SerializeDocumentType(TclDomNode *nodePtr,
		    Tcl_DString *output);
static void	SerializeAttribute(TclDomAttributeNode *attributeNodePtr,
		    Tcl_DString *output);

/* Following added by ericm@scriptics, 1999.6.25 */
/* Prototype definition for the TclDomExpat comment handler */

static void	TclDomExpatCommentHandler _ANSI_ARGS_ ((void *userData, 
		    const XML_Char *data));
/* Prototype for TclDomExpat Not Standalone Handler */
static int	TclDomExpatNotStandaloneHandler _ANSI_ARGS_ ((void *userData));

/* Prototype for TclDomExpat {Start|End}CdataSectionHandler */
static void	TclDomExpatStartCdataSectionHandler(void *userData);
static void	TclDomExpatEndCdataSectionHandler(void *userData);
static void	TclDomExpatStartDoctypeDeclHandler(void *userData,
		    const XML_Char *doctypeName,
		    const XML_Char *sysid,
		    const XML_Char *pubid,
		    int has_internal_subset);
static void	TclDomExpatEndDoctypeDeclHandler(void *userData);



/*
 *--------------------------------------------------------------
 *
 * IsExtender --
 *
 *	This procedure determines if a character belongs to the
 *	Extender class, as defined in the XML specification.
 *
 * Results:
 *	1 if the character is an Extender; otherwise 0.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
isExtender(int c) 
{
    if (c < 0xff) return 0;
    return ((c == 0x00b7)
	    || (c == 0x02d0)
	    || (c == 0x02d1)
	    || (c == 0x0387)
	    || (c == 0x0640)
	    || (c == 0x0e46)
	    || (c == 0x0ec6)
	    || (c == 0x3005)
	    || (c >= 0x3031 && c <= 0x3035)
	    || (c >= 0x309d && c <= 0x309e)
	    || (c >= 0x30fc && c <= 0x30fe));
}


/*
 *--------------------------------------------------------------
 *
 * IsCombiningChar --
 *
 *	This procedure determines if a character belongs to the
 *	CombiningChar class, as defined in the XML specification.
 *
 * Results:
 *	1 if the character is a CombiningChar; otherwise 0.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
isCombiningChar(int c) 
{
	if (c < 0xff) return 0;
	return (
		(c >= 0x0300 && c <= 0x0345) || 
		(c >= 0x0360 && c <= 0x0361) || 
		(c >= 0x0483 && c <= 0x0486) || 
		(c >= 0x0591 && c <= 0x05A1) || 
		(c >= 0x05A3 && c <= 0x05B9) || 
		(c >= 0x05BB && c <= 0x05BD) || 
		(c == 0x05BF)				 || 
		(c >= 0x05C1 && c <= 0x05C2) || 
		(c == 0x05C4)				 || 
		(c >= 0x064B && c <= 0x0652) || 
		(c == 0x0670)				 || 
		(c >= 0x06D6 && c <= 0x06DC) || 
		(c >= 0x06DD && c <= 0x06DF) || 
		(c >= 0x06E0 && c <= 0x06E4) || 
		(c >= 0x06E7 && c <= 0x06E8) || 
		(c >= 0x06EA && c <= 0x06ED) || 
		(c >= 0x0901 && c <= 0x0903) || 
		(c == 0x093C)				 || 
		(c >= 0x093E && c <= 0x094C) || 
		(c == 0x094D)				 || 
		(c >= 0x0951 && c <= 0x0954) || 
		(c >= 0x0962 && c <= 0x0963) || 
		(c >= 0x0981 && c <= 0x0983) || 
		(c == 0x09BC)				 || 
		(c == 0x09BE)				 || 
		(c == 0x09BF)				 || 
		(c >= 0x09C0 && c <= 0x09C4) || 
		(c >= 0x09C7 && c <= 0x09C8) || 
		(c >= 0x09CB && c <= 0x09CD) || 
		(c == 0x09D7)				 || 
		(c >= 0x09E2 && c <= 0x09E3) || 
		(c == 0x0A02)				 || 
		(c == 0x0A3C)				 || 
		(c == 0x0A3E)				 || 
		(c == 0x0A3F)				 || 
		(c >= 0x0A40 && c <= 0x0A42) || 
		(c >= 0x0A47 && c <= 0x0A48) || 
		(c >= 0x0A4B && c <= 0x0A4D) || 
		(c >= 0x0A70 && c <= 0x0A71) || 
		(c >= 0x0A81 && c <= 0x0A83) || 
		(c == 0x0ABC)				 || 
		(c >= 0x0ABE && c <= 0x0AC5) || 
		(c >= 0x0AC7 && c <= 0x0AC9) || 
		(c >= 0x0ACB && c <= 0x0ACD) || 
		(c >= 0x0B01 && c <= 0x0B03) || 
		(c == 0x0B3C)				 || 
		(c >= 0x0B3E && c <= 0x0B43) || 
		(c >= 0x0B47 && c <= 0x0B48) || 
		(c >= 0x0B4B && c <= 0x0B4D) || 
		(c >= 0x0B56 && c <= 0x0B57) || 
		(c >= 0x0B82 && c <= 0x0B83) || 
		(c >= 0x0BBE && c <= 0x0BC2) || 
		(c >= 0x0BC6 && c <= 0x0BC8) || 
		(c >= 0x0BCA && c <= 0x0BCD) || 
		(c == 0x0BD7)				 || 
		(c >= 0x0C01 && c <= 0x0C03) || 
		(c >= 0x0C3E && c <= 0x0C44) || 
		(c >= 0x0C46 && c <= 0x0C48) || 
		(c >= 0x0C4A && c <= 0x0C4D) || 
		(c >= 0x0C55 && c <= 0x0C56) || 
		(c >= 0x0C82 && c <= 0x0C83) || 
		(c >= 0x0CBE && c <= 0x0CC4) || 
		(c >= 0x0CC6 && c <= 0x0CC8) || 
		(c >= 0x0CCA && c <= 0x0CCD) || 
		(c >= 0x0CD5 && c <= 0x0CD6) || 
		(c >= 0x0D02 && c <= 0x0D03) || 
		(c >= 0x0D3E && c <= 0x0D43) || 
		(c >= 0x0D46 && c <= 0x0D48) || 
		(c >= 0x0D4A && c <= 0x0D4D) || 
		(c == 0x0D57)				 || 
		(c == 0x0E31)				 || 
		(c >= 0x0E34 && c <= 0x0E3A) || 
		(c >= 0x0E47 && c <= 0x0E4E) || 
		(c == 0x0EB1)				 || 
		(c >= 0x0EB4 && c <= 0x0EB9) ||
		(c >= 0x0EBB && c <= 0x0EBC) || 
		(c >= 0x0EC8 && c <= 0x0ECD) || 
		(c >= 0x0F18 && c <= 0x0F19) ||
		(c == 0x0F35)				 || 
		(c == 0x0F37)				 || 
		(c == 0x0F39)				 || 
		(c == 0x0F3E)				 || 
		(c == 0x0F3F)				 || 
		(c >= 0x0F71 && c <= 0x0F84) || 
		(c >= 0x0F86 && c <= 0x0F8B) || 
		(c >= 0x0F90 && c <= 0x0F95) || 
		(c == 0x0F97)				 || 
		(c >= 0x0F99 && c <= 0x0FAD) || 
		(c >= 0x0FB1 && c <= 0x0FB7) || 
		(c == 0x0FB9)				 || 
		(c >= 0x20D0 && c <= 0x20DC) || 
		(c == 0x20E1)				 || 
		(c >= 0x302A && c <= 0x302F) || 
		(c == 0x3099)				 || 
		(c == 0x309A)
	);
}


/*
 *--------------------------------------------------------------
 *
 * IsBaseChar --
 *
 *	This procedure determines if a character belongs to the
 *	BaseChar class, as defined in the XML specification.
 *
 * Results:
 *	1 if the character is a BaseChar; otherwise 0.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
isBaseChar(int c) 
{
	return (
		(c >= 0x0041 && c <= 0x005A) || 
		(c >= 0x0061 && c <= 0x007A) || 
		(c >= 0x00C0 && c <= 0x00D6) || 
		(c >= 0x00D8 && c <= 0x00F6) ||
		(c >= 0x00F8 && c <= 0x00FF) ||
		(c >= 0x0100 && c <= 0x0131) || 
		(c >= 0x0134 && c <= 0x013E) || 
		(c >= 0x0141 && c <= 0x0148) || 
		(c >= 0x014A && c <= 0x017E) || 
		(c >= 0x0180 && c <= 0x01C3) || 
		(c >= 0x01CD && c <= 0x01F0) || 
		(c >= 0x01F4 && c <= 0x01F5) || 
		(c >= 0x01FA && c <= 0x0217) || 
		(c >= 0x0250 && c <= 0x02A8) || 
		(c >= 0x02BB && c <= 0x02C1) ||
		(c == 0x0386)				 || 
		(c >= 0x0388 && c <= 0x038A) ||
	    (c == 0x038C)		         || 
		(c >= 0x038E && c <= 0x03A1) || 
		(c >= 0x03A3 && c <= 0x03CE) || 
		(c >= 0x03D0 && c <= 0x03D6) || 
		(c == 0x03DA)				 || 
		(c == 0x03DC)				 || 
		(c == 0x03DE)				 || 
		(c == 0x03E0)				 || 
		(c >= 0x03E2 && c <= 0x03F3) || 
		(c >= 0x0401 && c <= 0x040C) || 
		(c >= 0x040E && c <= 0x044F) || 
		(c >= 0x0451 && c <= 0x045C) || 
		(c >= 0x045E && c <= 0x0481) || 
		(c >= 0x0490 && c <= 0x04C4) || 
		(c >= 0x04C7 && c <= 0x04C8) || 
		(c >= 0x04CB && c <= 0x04CC) || 
		(c >= 0x04D0 && c <= 0x04EB) || 
		(c >= 0x04EE && c <= 0x04F5) || 
		(c >= 0x04F8 && c <= 0x04F9) || 
		(c >= 0x0531 && c <= 0x0556) || 
		(c == 0x0559)				 || 
		(c >= 0x0561 && c <= 0x0586) || 
		(c >= 0x05D0 && c <= 0x05EA) || 
		(c >= 0x05F0 && c <= 0x05F2) || 
		(c >= 0x0621 && c <= 0x063A) || 
		(c >= 0x0641 && c <= 0x064A) || 
		(c >= 0x0671 && c <= 0x06B7) || 
		(c >= 0x06BA && c <= 0x06BE) || 
		(c >= 0x06C0 && c <= 0x06CE) || 
		(c >= 0x06D0 && c <= 0x06D3) || 
		(c == 0x06D5)				 || 
		(c >= 0x06E5 && c <= 0x06E6) || 
		(c >= 0x0905 && c <= 0x0939) || 
		(c == 0x093D)				 || 
		(c >= 0x0958 && c <= 0x0961) || 
		(c >= 0x0985 && c <= 0x098C) || 
		(c >= 0x098F && c <= 0x0990) || 
		(c >= 0x0993 && c <= 0x09A8) || 
		(c >= 0x09AA && c <= 0x09B0) || 
		(c == 0x09B2)				 || 
		(c >= 0x09B6 && c <= 0x09B9) || 
		(c >= 0x09DC && c <= 0x09DD) || 
		(c >= 0x09DF && c <= 0x09E1) || 
		(c >= 0x09F0 && c <= 0x09F1) || 
		(c >= 0x0A05 && c <= 0x0A0A) || 
		(c >= 0x0A0F && c <= 0x0A10) || 
		(c >= 0x0A13 && c <= 0x0A28) || 
		(c >= 0x0A2A && c <= 0x0A30) || 
		(c >= 0x0A32 && c <= 0x0A33) || 
		(c >= 0x0A35 && c <= 0x0A36) || 
		(c >= 0x0A38 && c <= 0x0A39) || 
		(c >= 0x0A59 && c <= 0x0A5C) || 
		(c == 0x0A5E)				 || 
		(c >= 0x0A72 && c <= 0x0A74) || 
		(c >= 0x0A85 && c <= 0x0A8B) || 
		(c == 0x0A8D)				 || 
		(c >= 0x0A8F && c <= 0x0A91) || 
		(c >= 0x0A93 && c <= 0x0AA8) || 
		(c >= 0x0AAA && c <= 0x0AB0) || 
		(c >= 0x0AB2 && c <= 0x0AB3) || 
		(c >= 0x0AB5 && c <= 0x0AB9) || 
		(c == 0x0ABD)				 || 
		(c == 0x0AE00)				 || 
		(c >= 0x0B05 && c <= 0x0B0C) || 
		(c >= 0x0B0F && c <= 0x0B10) || 
		(c >= 0x0B13 && c <= 0x0B28) || 
		(c >= 0x0B2A && c <= 0x0B30) || 
		(c >= 0x0B32 && c <= 0x0B33) || 
		(c >= 0x0B36 && c <= 0x0B39) || 
		(c == 0x0B3D)				 || 
		(c >= 0x0B5C && c <= 0x0B5D) || 
		(c >= 0x0B5F && c <= 0x0B61) || 
		(c >= 0x0B85 && c <= 0x0B8A) || 
		(c >= 0x0B8E && c <= 0x0B90) || 
		(c >= 0x0B92 && c <= 0x0B95) || 
		(c >= 0x0B99 && c <= 0x0B9A) || 
		(c == 0x0B9C)				 || 
		(c >= 0x0B9E && c <= 0x0B9F) || 
		(c >= 0x0BA3 && c <= 0x0BA4) || 
		(c >= 0x0BA8 && c <= 0x0BAA) || 
		(c >= 0x0BAE && c <= 0x0BB5) || 
		(c >= 0x0BB7 && c <= 0x0BB9) || 
		(c >= 0x0C05 && c <= 0x0C0C) || 
		(c >= 0x0C0E && c <= 0x0C10) || 
		(c >= 0x0C12 && c <= 0x0C28) || 
		(c >= 0x0C2A && c <= 0x0C33) || 
		(c >= 0x0C35 && c <= 0x0C39) || 
		(c >= 0x0C60 && c <= 0x0C61) || 
		(c >= 0x0C85 && c <= 0x0C8C) || 
		(c >= 0x0C8E && c <= 0x0C90) || 
		(c >= 0x0C92 && c <= 0x0CA8) || 
		(c >= 0x0CAA && c <= 0x0CB3) || 
		(c >= 0x0CB5 && c <= 0x0CB9) || 
		(c == 0x0CDE)				 || 
		(c >= 0x0CE0 && c <= 0x0CE1) || 
		(c >= 0x0D05 && c <= 0x0D0C) || 
		(c >= 0x0D0E && c <= 0x0D10) || 
		(c >= 0x0D12 && c <= 0x0D28) || 
		(c >= 0x0D2A && c <= 0x0D39) || 
		(c >= 0x0D60 && c <= 0x0D61) || 
		(c >= 0x0E01 && c <= 0x0E2E) || 
		(c == 0x0E30)				 || 
		(c >= 0x0E32 && c <= 0x0E33) || 
		(c >= 0x0E40 && c <= 0x0E45) || 
		(c >= 0x0E81 && c <= 0x0E82) || 
		(c == 0x0E84)				 || 
		(c >= 0x0E87 && c <= 0x0E88) || 
		(c == 0x0E8A)				 || 
		(c == 0x0E8D)				 || 
		(c >= 0x0E94 && c <= 0x0E97) || 
		(c >= 0x0E99 && c <= 0x0E9F) || 
		(c >= 0x0EA1 && c <= 0x0EA3) || 
		(c == 0x0EA5)				 || 
		(c == 0x0EA7)				 || 
		(c >= 0x0EAA && c <= 0x0EAB) || 
		(c >= 0x0EAD && c <= 0x0EAE) || 
		(c == 0x0EB0)				 || 
		(c >= 0x0EB2 && c <= 0x0EB3) || 
		(c == 0x0EBD)				 || 
		(c >= 0x0EC0 && c <= 0x0EC4) || 
		(c >= 0x0F40 && c <= 0x0F47) || 
		(c >= 0x0F49 && c <= 0x0F69) || 
		(c >= 0x10A0 && c <= 0x10C5) || 
		(c >= 0x10D0 && c <= 0x10F6) || 
		(c == 0x1100)				 || 
		(c >= 0x1102 && c <= 0x1103) || 
		(c >= 0x1105 && c <= 0x1107) || 
		(c == 0x1109)				 || 
		(c >= 0x110B && c <= 0x110C) || 
		(c >= 0x110E && c <= 0x1112) || 
		(c == 0x113C)				 || 
		(c == 0x113E)				 || 
		(c == 0x1140)				 || 
		(c == 0x114C)				 || 
		(c == 0x114E)		         || 
		(c == 0x1150)				 || 
		(c >= 0x1154 && c <= 0x1155) || 
		(c == 0x1159)				 || 
		(c >= 0x115F && c <= 0x1161) || 
		(c == 0x1163)				 || 
		(c == 0x1165)				 || 
		(c == 0x1167)				 || 
		(c == 0x1169)				 || 
		(c >= 0x116D && c <= 0x116E) || 
		(c >= 0x1172 && c <= 0x1173) || 
		(c == 0x1175)				 || 
		(c == 0x119E)				 || 
		(c == 0x11A8)				 || 
		(c == 0x11AB)				 || 
		(c >= 0x11AE && c <= 0x11AF) || 
		(c >= 0x11B7 && c <= 0x11B8) || 
		(c == 0x11BA)				 || 
		(c >= 0x11BC && c <= 0x11C2) || 
		(c == 0x11EB)				 || 
		(c == 0x11F0)				 || 
		(c == 0x11F9)				 || 
		(c >= 0x1E00 && c <= 0x1E9B) || 
		(c >= 0x1EA0 && c <= 0x1EF9) || 
		(c >= 0x1F00 && c <= 0x1F15) || 
		(c >= 0x1F18 && c <= 0x1F1D) || 
		(c >= 0x1F20 && c <= 0x1F45) || 
		(c >= 0x1F48 && c <= 0x1F4D) || 
		(c >= 0x1F50 && c <= 0x1F57) || 
		(c == 0x1F59)				 || 
		(c == 0x1F5B)				 || 
		(c == 0x1F5D)				 || 
		(c >= 0x1F5F && c <= 0x1F7D) || 
		(c >= 0x1F80 && c <= 0x1FB4) || 
		(c >= 0x1FB6 && c <= 0x1FBC) || 
		(c == 0x1FBE)				 || 
		(c >= 0x1FC2 && c <= 0x1FC4) || 
		(c >= 0x1FC6 && c <= 0x1FCC) || 
		(c >= 0x1FD0 && c <= 0x1FD3) || 
		(c >= 0x1FD6 && c <= 0x1FDB) || 
		(c >= 0x1FE0 && c <= 0x1FEC) || 
		(c >= 0x1FF2 && c <= 0x1FF4) || 
		(c >= 0x1FF6 && c <= 0x1FFC) || 
		(c == 0x2126)		         || 
		(c >= 0x212A && c <= 0x212B) || 
		(c == 0x212E)				 || 
		(c >= 0x2180 && c <= 0x2182) || 
		(c >= 0x3041 && c <= 0x3094) || 
		(c >= 0x30A1 && c <= 0x30FA) || 
		(c >= 0x3105 && c <= 0x312C) || 
		(c >= 0xAC00 && c <= 0xD7A3)
	);
}


/*
 *--------------------------------------------------------------
 *
 * IsIdeographic --
 *
 *	This procedure determines if a character belongs to the
 *	Ideographic class, as defined in the XML specification.
 *
 * Results:
 *	1 if the character is a Ideographic; otherwise 0.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
isIdeographic(int c)
{
	return (
		    (c >= 0x4e00 && c <= 0x9fa5) 
			|| (c == 0x3007)				 
			|| (c >= 0x3021 && c <= 0x3029)
	);
}


/*
 *--------------------------------------------------------------
 *
 * IsLetter --
 *
 *	This procedure determines if a character belongs to the
 *	Letter class, as defined in the XML specification.
 *
 * Results:
 *	1 if the character is an Letter; otherwise 0.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
isLetter(int c)
{
    return (isBaseChar(c) || isIdeographic(c));
}


/*
 *--------------------------------------------------------------
 *
 * UnlinkChild --
 *
 *	Remove a node from the document tree.
 *
 * Results:
 *	1 if the node was in the fragment list; 0 otherwise.
 *
 * Side effects:
 *	Reference nodes of iterators may be updated.
 *
 *--------------------------------------------------------------
 */

static void
UnlinkChild(TclDomInterpData *interpDataPtr, TclDomNode *childPtr)
{
    TclDomNodeIterator *nodeIteratorPtr;
    TclDomTreeWalker *treeWalkerPtr;
    Tcl_HashEntry *entry;
    Tcl_HashSearch search;
    TclDomNode *testPtr;

    /*
     * See if this action will cause a reference node to be deleted.
     * It's not clear that this implementation (i.e., keeping a list
     * of iterators, and then fixing up the iterators when nodes are
     * deleted) is the optimal way to do this. For small trees, and
     * a small number of iterators, it may not matter much. If we
     * wind up having many iterators, or doing many deletions, this 
     * may change.
     */

    for (entry = Tcl_FirstHashEntry(&interpDataPtr->iteratorHashTable,
	         &search); entry; entry = Tcl_NextHashEntry(&search)) {
	    nodeIteratorPtr = (TclDomNodeIterator *) Tcl_GetHashValue(entry);
	    if (nodeIteratorPtr->rootPtr && nodeIteratorPtr->rootPtr->containingDocumentPtr
	        	== childPtr->containingDocumentPtr) {
	        for (testPtr = nodeIteratorPtr->referencePtr;
		            testPtr != nodeIteratorPtr->rootPtr->parentNodePtr;
		            testPtr = testPtr->parentNodePtr) {
		        if (testPtr == childPtr) {
		            if (testPtr == nodeIteratorPtr->rootPtr) {
			            /*
			             * We're deleting the entire iterated tree, so
			             * there's no effect on the iterator
			             */
			            break;
		            }
		            /*
		             * We're deleting some portion of iterated tree
		             */
		            if (nodeIteratorPtr->position
			                == REFERENCE_IS_BEFORE_ITERATOR) {
			            TclDomNode *newRefPtr;
			            TclDomNodeBefore(testPtr, nodeIteratorPtr->rootPtr,
				                SHOW_ALL, NULL, &newRefPtr);
			            nodeIteratorPtr->referencePtr = newRefPtr;
		            } else {
			            TclDomNode *newRefPtr;
			            TclDomNodeAfter(testPtr, nodeIteratorPtr->rootPtr,
				                SHOW_ALL, NULL, &newRefPtr);
			            if (newRefPtr == NULL) {
				            /*
				             * Special case where reference node
				             * is last node
				             */
			                TclDomNodeBefore(testPtr, nodeIteratorPtr->rootPtr,
				                    SHOW_ALL, NULL, &newRefPtr);
			            }
			            nodeIteratorPtr->referencePtr = newRefPtr;
		            }
		        }
	        }
	    }
    }

    for (entry = Tcl_FirstHashEntry(&interpDataPtr->treeWalkerHashTable,
	        &search); entry; entry = Tcl_NextHashEntry(&search)) {
	    treeWalkerPtr = (TclDomTreeWalker *) Tcl_GetHashValue(entry);
	    if (treeWalkerPtr->rootPtr && treeWalkerPtr->rootPtr->containingDocumentPtr
		        == childPtr->containingDocumentPtr) {
	        for (testPtr = treeWalkerPtr->currentNodePtr;
		            testPtr != treeWalkerPtr->rootPtr->parentNodePtr;
		            testPtr = testPtr->parentNodePtr) {
		        if (testPtr == childPtr) {
		            if (testPtr == treeWalkerPtr->rootPtr) {
			            /*
			             * We're deleting the entire iterated tree, so
			             * there's no effect on the iterator
			             */
			            break;
		            } else {
			            /*
			             * We're deleting some portion of iterated tree
			             */
			            TclDomNode *newRefPtr;
#ifdef UNDEF
			            TclDomNodeBefore(testPtr, treeWalkerPtr->rootPtr, 
								SHOW_ALL, NULL, &newRefPtr);
#endif
                        TclDomTreeWalkerPreviousNode(testPtr, treeWalkerPtr->rootPtr, 
								SHOW_ALL, NULL, &newRefPtr);
			            treeWalkerPtr->currentNodePtr = newRefPtr;
		            }
		        }
	        }
	    }
    }

    if (childPtr->previousSiblingPtr) {
	    childPtr->previousSiblingPtr->nextSiblingPtr = 
				childPtr->nextSiblingPtr;
    } else if (childPtr->parentNodePtr) {
	    childPtr->parentNodePtr->firstChildPtr = 
	    			childPtr->nextSiblingPtr;
    }
    if (childPtr->nextSiblingPtr) {
	    childPtr->nextSiblingPtr->previousSiblingPtr
	            = childPtr->previousSiblingPtr;
    } else {
	    if (childPtr->parentNodePtr) {
	        childPtr->parentNodePtr->lastChildPtr
		            = childPtr->previousSiblingPtr;
	    }
    }
}


/*
 *--------------------------------------------------------------
 *
 * UnlinkDocumentFragment --
 *
 *	Remove a node from the fragment list.
 *
 * Results:
 *	1 if the node was in the fragment list; 0 otherwise.
 *
 *--------------------------------------------------------------
 */

static int
UnlinkDocumentFragment(
    TclDomDocument *documentPtr,
    TclDomNode *nodePtr)
{
    TclDomNode *fragmentNodesPtr;

    fragmentNodesPtr = documentPtr->fragmentsPtr;
    while (fragmentNodesPtr) {
	    if (fragmentNodesPtr == nodePtr) {
	        /*
	         * Remove child from fragment list
	         */

	        if (nodePtr->previousSiblingPtr) {
		        nodePtr->previousSiblingPtr->nextSiblingPtr = 
						nodePtr->nextSiblingPtr;
	        } else {
		        nodePtr->containingDocumentPtr->fragmentsPtr = 
						nodePtr->nextSiblingPtr;
	        }
	        if (nodePtr->nextSiblingPtr) {
		        nodePtr->nextSiblingPtr->previousSiblingPtr = NULL;
	        }
	        break;
	    }
	    fragmentNodesPtr = fragmentNodesPtr->nextSiblingPtr;
    }
    return (fragmentNodesPtr != NULL) ? 1 : 0;
}

/*
 *--------------------------------------------------------------
 *
 * AddDocumentFragment --
 *
 *	Add a newly-created node to the head 
 *	of the document fragment list.
 *
 * Results:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void 
AddDocumentFragment(
    TclDomDocument *documentPtr,
    TclDomNode *nodePtr)
{
    nodePtr->nextSiblingPtr = documentPtr->fragmentsPtr;
    if (documentPtr->fragmentsPtr) {
	    documentPtr->fragmentsPtr->previousSiblingPtr = nodePtr;
	    documentPtr->fragmentsPtr = nodePtr;
    } else {
	    documentPtr->fragmentsPtr = nodePtr;
    }
    return;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomIsName --
 *
 *	This procedure determines if a string matches the 
 *	Name[5] production, as defined in the XML 
 *	specification.
 *
 * Results:
 *	1 if the string is a Name; otherwise 0.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
TclDomIsName(
    char *s)			/* Name string in UTF-8 */
{
    Tcl_UniChar uChar;
    int length;

    length = Tcl_UtfToUniChar(s, &uChar);
    s += length;
    
    if (!isLetter(uChar) && (uChar != '_') && (uChar != ':')) {
	    return 0;
    }

    while (*s) {
	    length = Tcl_UtfToUniChar(s, &uChar);
	    s += length;
	    if (isLetter(uChar)) continue;
	    if (isdigit(uChar)) continue;
	    if (uChar == '.') continue;
	    if (uChar == '-') continue;
	    if (uChar == '_') continue;
	    if (uChar == ':') continue;
	    if (isCombiningChar(uChar)) continue;
	    if (isExtender(uChar)) continue;
	    return 0;
    }

    return 1;
}

#ifdef UNDEF
/*
 * Names that match the DOM spec
 */
static char *typeName[13] = {
    "",
    "ELEMENT_NODE",
    "ATTRIBUTE_NODE",
    "TEXT_NODE",
    "CDATA_SECTION_NODE",
    "ENTITY_REFERENCE_NODE",
    "ENTITY_NODE",
    "PROCESSING_INSTRUCTION_NODE",
    "COMMENT_NODE",
    "DOCUMENT_NODE",
    "DOCUMENT_TYPE_NODE",
    "DOCUMENT_FRAGMENT_NODE",
    "NOTATION_NODE"
    };
#else
    /*
     * Names that match the TclDom implementation
     */
static char *typeName[13] = {
    "",
    "element",
    "attribute",
    "textNode",
    "CDATASection",
    "entityReference",
    "entity",
    "processingInstruction",
    "comment",
    "document",
    "documentType",
    "documentFragment",
    "notation"
    };
#endif


/*
 *--------------------------------------------------------------
 *
 * TclDomTypeName --
 *
 *	This procedure converts an internal type to
 *	a human-readable string representation, and copies 
 *	the string value to the interpreter's result.
 *
 * Results:
 *	TCL_OK if the type is valid; TCL_ERROR otherwise.
 *
 * Side effects:
 *	Updates interpreter's result.
 *
 *--------------------------------------------------------------
 */

int
TclDomNodeTypeName(
    Tcl_Interp *interp,	    /* Interpreter whose result is to be updated */
    TclDomNode *nodePtr)    /* DOM node */
{


    if (nodePtr->nodeType < ELEMENT_NODE || nodePtr->nodeType > NOTATION_NODE) {
	    Tcl_AppendResult(interp, "invalid node type", (char *) NULL);
	    return TCL_ERROR;
    } else {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(typeName[nodePtr->nodeType], 
				-1));
	    return TCL_OK;
    }
}


/*
 *--------------------------------------------------------------
 *
 * TclDomGetTypeMaskFromName --
 *
 *	This procedure returns the node type mask
 *   used in tree traversal given the text name
 *   of a node type, or the value "all", 
 *   corresponding to all node types.
 *
 * Results:
 *	TCL_OK if the type is valid; TCL_ERROR otherwise.
 *
 * Side effects:
 *	Update the interpreter's error result if the node
 *   type is invalid.
 *
 *--------------------------------------------------------------
 */

int
TclDomGetTypeMaskFromName(
    Tcl_Interp *interp,
    char *nodeName,
    unsigned int *nodeMaskPtr)
{
    int i;

    if (strcmp(nodeName, "all") == 0) {
	    *nodeMaskPtr = 0xffff;
	    return TCL_OK;
    }

    for (i = 1; i < 13; i++) {
	    if (strcmp(nodeName, typeName[i]) == 0) {
	        *nodeMaskPtr = (1 << (i-1));
	        return TCL_OK;
	    }
    }
    Tcl_AppendResult(interp, "invalid node type", (char *) NULL);
    return TCL_ERROR;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomTypeName --
 *
 *	This procedure converts the name of a node type
 *   to a numeric type value.
 *
 * Results:
 *	TCL_OK if the type is valid; TCL_ERROR otherwise.
 *
 * Side effects:
 *	Updates interpreter's result.
 *
 *--------------------------------------------------------------
 */

int
TclDomGetTypeFromName(
    Tcl_Interp *interp,
    char *nodeName,
    unsigned int *nodeTypePtr)
{
    int i;

    for (i = 1; i < 13; i++) {
	    if (strcmp(nodeName, typeName[i]) == 0) {
	        *nodeTypePtr = i;
	        return TCL_OK;
	    }
    }
    Tcl_AppendResult(interp, "invalid node type", (char *) NULL);
    return TCL_ERROR;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomTypeName --
 *
 *	This procedure returns a pointer to a string version
 *   of a node type.
 *
 * Results:
 *	TCL_OK if the type is valid; TCL_ERROR otherwise.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
TclDomGetNameFromEnum(
    int nodeType,
    char **nodeNamePtr)
{
    if (nodeType < ELEMENT_NODE || nodeType > NOTATION_NODE) {
	    *nodeNamePtr = "";
	    return TCL_ERROR;
    } else {
	    *nodeNamePtr = typeName[nodeType];
	    return TCL_OK;
    }	
}


/*
 *--------------------------------------------------------------
 *
 * TclDomSetNodeValue --
 *
 *	This procedure sets the node value for a node.
 *
 * Results:
 *	TDP_OK if the node is writable; otherwise returns
 *	TDP_NO_MODIFICATION_ALLOWED
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

TdpDomError
TclDomSetNodeValue(
    TclDomNode *nodePtr,    /* DOM node */
    char *value)	    /* New value for node */
{
    if (nodePtr->nodeType	 == ELEMENT_NODE
	        || nodePtr->nodeType == ENTITY_REFERENCE_NODE
	        || nodePtr->nodeType == ENTITY_NODE
	        || nodePtr->nodeType == DOCUMENT_NODE
	        || nodePtr->nodeType == DOCUMENT_TYPE_NODE
	        || nodePtr->nodeType == DOCUMENT_FRAGMENT_NODE
	        || nodePtr->nodeType == NOTATION_NODE) {
	    return TDP_NO_MODIFICATION_ALLOWED_ERR;
    }

    if (nodePtr->nodeValue) {
	    ckfree(nodePtr->nodeValue);
    }
    nodePtr->valueLength = strlen(value);
    nodePtr->nodeValue = ckalloc(nodePtr->valueLength + 1);
    strcpy(nodePtr->nodeValue, value);
    return TDP_OK;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomGetNodeName --
 *
 *	This procedure returns the name for a node.
 *
 * Results:
 *	TCL_OK
 *
 * Side effects:
 *	Name of node is placed in interpreters result.
 *
 *--------------------------------------------------------------
 */

int
TclDomGetNodeName(
    Tcl_Interp *interp,	    /* Interpreter whose result is to be updated */
    TclDomNode *nodePtr)   /* DOM node */
{

    switch (nodePtr->nodeType) {
	    case ELEMENT_NODE:
	    case ATTRIBUTE_NODE:
	    case ENTITY_REFERENCE_NODE:
	    case ENTITY_NODE:
	    case PROCESSING_INSTRUCTION_NODE:
	    case DOCUMENT_TYPE_NODE:
	    case NOTATION_NODE:
	        if (nodePtr->nodeName) {
		        Tcl_SetObjResult(interp, 
						Tcl_NewStringObj(nodePtr->nodeName, -1));
	        }
	        return TCL_OK;

	    case TEXT_NODE:
	        Tcl_SetResult(interp, "#text", TCL_STATIC);
	        return TCL_OK;

	    case CDATA_SECTION_NODE:
	        Tcl_SetResult(interp, "#cdata-section", TCL_STATIC);
	        return TCL_OK;

	    case COMMENT_NODE:
	        Tcl_SetResult(interp, "#comment", TCL_STATIC);
	        return TCL_OK;

	    case DOCUMENT_NODE:
	        Tcl_SetResult(interp, "#document", TCL_STATIC);
	        return TCL_OK;

	    case DOCUMENT_FRAGMENT_NODE:
	        Tcl_SetResult(interp, "#document-fragment", TCL_STATIC);
	        return TCL_OK;

	    default:
	        return TCL_ERROR;
    }	
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * CloneNode --
 *
 *	This procedure make a copy of a DOM node. If the 
 *	"deep" flag is set then all descendants of the node
 *	will be copied also. See the DOM specification
 *	for further information.
 *
 * Results:
 *	A TclDom node of the appropriate type.
 *
 * Side effects:
 *	A sub-tree is allocated.
 *
 *--------------------------------------------------------------
 */

TclDomNode *
CloneNode(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */ 
    TclDomNode *nodePtr,		/* Node to be cloned */
    TclDomDocument *containingDocumentPtr, /* Doc for clone */
    int deepFlag)			/* True => copy children */
{
    TclDomNode *clonedNodePtr = NULL;
    TclDomNode *childNodePtr, *clonedChildNodePtr;
    TclDomTextNode *textNodePtr, *clonedTextNodePtr;
    TclDomAttributeNode *attributeNodePtr, *clonedAttributeNodePtr;
    TclDomDocTypeNode *docTypeNodePtr, *clonedDocTypeNodePtr;
    int nodeId;

    nodeId = ++interpDataPtr->nodeSeed;
    switch (nodePtr->nodeType) {
	    case ELEMENT_NODE:
	        clonedNodePtr = (TclDomNode *) ckalloc(sizeof(TclDomNode));
	        memset(clonedNodePtr, 0, sizeof(TclDomNode));
	        clonedNodePtr->nodeId = nodeId;
	        clonedNodePtr->nodeType = ELEMENT_NODE;
	        clonedNodePtr->containingDocumentPtr = containingDocumentPtr;
	        if (nodePtr->nodeName) {
		        clonedNodePtr->nodeName = 
						ckalloc(strlen(nodePtr->nodeName) + 1);
		        strcpy(clonedNodePtr->nodeName, nodePtr->nodeName);
	        }
	        if (nodePtr->nodeValue) {
		        clonedNodePtr->valueLength = nodePtr->valueLength;
		        clonedNodePtr->nodeValue = ckalloc(nodePtr->valueLength + 1);
		        strcpy(clonedNodePtr->nodeValue, nodePtr->nodeValue);
	        }
	        attributeNodePtr = nodePtr->firstAttributePtr;
	        while (attributeNodePtr) {
		        clonedAttributeNodePtr = 
						(TclDomAttributeNode *) CloneNode(interp, interpDataPtr,
                        (TclDomNode *) attributeNodePtr, 
						containingDocumentPtr, 0);
		        if (clonedNodePtr->firstAttributePtr == NULL) {
		            clonedNodePtr->firstAttributePtr = 
							clonedNodePtr->lastAttributePtr = 
							clonedAttributeNodePtr;
		        } else {
		            clonedNodePtr->lastAttributePtr->nextSiblingPtr = 
						    clonedAttributeNodePtr;
		            clonedNodePtr->lastAttributePtr = clonedAttributeNodePtr;
		        }
		        attributeNodePtr = attributeNodePtr->nextSiblingPtr;
	        }

	        if (deepFlag) {
		        childNodePtr = nodePtr->firstChildPtr;
		        while (childNodePtr) {
		            clonedChildNodePtr = 
							CloneNode(interp, interpDataPtr, childNodePtr, 
                            containingDocumentPtr, 1);
		            if (clonedNodePtr->firstChildPtr == NULL) {
			            clonedNodePtr->firstChildPtr = 
						        clonedNodePtr->lastChildPtr = 
								clonedChildNodePtr;
		            } else {
			            clonedChildNodePtr->previousSiblingPtr = 
								clonedNodePtr->lastChildPtr;
			            clonedNodePtr->lastChildPtr->nextSiblingPtr = 
								clonedChildNodePtr;
			            clonedNodePtr->lastChildPtr = clonedChildNodePtr;
		            }
		            childNodePtr = childNodePtr->nextSiblingPtr;
		        }
	        }
	        break;
    
	    case ATTRIBUTE_NODE:
	        attributeNodePtr = (TclDomAttributeNode *) nodePtr;
	        clonedAttributeNodePtr = (TclDomAttributeNode *) 
					ckalloc(sizeof(TclDomAttributeNode));
	        memset(clonedAttributeNodePtr, 0, sizeof(TclDomAttributeNode));
	        clonedAttributeNodePtr->nodeId = nodeId;
	        clonedAttributeNodePtr->nodeType = ATTRIBUTE_NODE;
	        clonedAttributeNodePtr->containingDocumentPtr = 
					containingDocumentPtr;
	        if (attributeNodePtr->nodeName) {
		        clonedAttributeNodePtr->nodeName = 
						ckalloc(strlen(attributeNodePtr->nodeName) + 1);
		        strcpy(clonedAttributeNodePtr->nodeName, 
						attributeNodePtr->nodeName);
	        }
	        if (attributeNodePtr->nodeValue) {
		        clonedAttributeNodePtr->valueLength = 
						attributeNodePtr->valueLength;
		        clonedAttributeNodePtr->nodeValue = 
						ckalloc(attributeNodePtr->valueLength + 1);
		        strcpy(clonedAttributeNodePtr->nodeValue, 
						attributeNodePtr->nodeValue);
	        }
	        clonedNodePtr = (TclDomNode *) clonedAttributeNodePtr;
	        break;

	    case TEXT_NODE:
	    case CDATA_SECTION_NODE:
	    case PROCESSING_INSTRUCTION_NODE:
	    case COMMENT_NODE:
	        textNodePtr = (TclDomTextNode *) nodePtr;
	        clonedTextNodePtr = (TclDomTextNode *) 
					ckalloc(sizeof(TclDomTextNode));
	        memset(clonedTextNodePtr, 0, sizeof(TclDomTextNode));
	        clonedTextNodePtr->nodeId = nodeId;
	        clonedTextNodePtr->nodeType = textNodePtr->nodeType;
	        clonedTextNodePtr->containingDocumentPtr = containingDocumentPtr;
	        if (textNodePtr->nodeName) {
		        clonedTextNodePtr->nodeName = 
						ckalloc(strlen(textNodePtr->nodeName) + 1);
		        strcpy(clonedTextNodePtr->nodeName, textNodePtr->nodeName);
	        }
	        if (textNodePtr->nodeValue) {
		        clonedTextNodePtr->valueLength = textNodePtr->valueLength;
		        clonedTextNodePtr->nodeValue = 
						ckalloc(textNodePtr->valueLength + 1);
		        strcpy(clonedTextNodePtr->nodeValue, textNodePtr->nodeValue);
	        }
	        clonedNodePtr = (TclDomNode *) clonedTextNodePtr;
	        break;

	    case ENTITY_REFERENCE_NODE:
	    case ENTITY_NODE:
	    case NOTATION_NODE:
	        break;

	    case DOCUMENT_NODE:
            containingDocumentPtr = TclDomEmptyDocument(interp, interpDataPtr);
            clonedNodePtr = containingDocumentPtr->selfPtr;
            attributeNodePtr = nodePtr->firstAttributePtr;
	        while (attributeNodePtr) {
		        clonedAttributeNodePtr = 
						(TclDomAttributeNode *) CloneNode(interp, interpDataPtr,
                        (TclDomNode *) attributeNodePtr, 
						containingDocumentPtr, 0);
		        if (clonedNodePtr->firstAttributePtr == NULL) {
		            clonedNodePtr->firstAttributePtr = 
							clonedNodePtr->lastAttributePtr = 
							clonedAttributeNodePtr;
		        } else {
		            clonedNodePtr->lastAttributePtr->nextSiblingPtr = 
						    clonedAttributeNodePtr;
		            clonedNodePtr->lastAttributePtr = clonedAttributeNodePtr;
		        }
		        attributeNodePtr = attributeNodePtr->nextSiblingPtr;
	        }
            if (deepFlag) {
		        childNodePtr = nodePtr->firstChildPtr;
		        while (childNodePtr) {
		            clonedChildNodePtr = 
							CloneNode(interp, interpDataPtr, childNodePtr, 
                            containingDocumentPtr, 1);
		            if (clonedNodePtr->firstChildPtr == NULL) {
			            clonedNodePtr->firstChildPtr = 
						        clonedNodePtr->lastChildPtr = 
								clonedChildNodePtr;
		            } else {
			            clonedChildNodePtr->previousSiblingPtr = 
								clonedNodePtr->lastChildPtr;
			            clonedNodePtr->lastChildPtr->nextSiblingPtr = 
								clonedChildNodePtr;
			            clonedNodePtr->lastChildPtr = clonedChildNodePtr;
		            }
		            childNodePtr = childNodePtr->nextSiblingPtr;
		        }
	        }
            break;

	    case DOCUMENT_TYPE_NODE:
            docTypeNodePtr = (TclDomDocTypeNode *) nodePtr;
	        clonedDocTypeNodePtr = (TclDomDocTypeNode *) 
					ckalloc(sizeof(TclDomDocTypeNode));
	        memset(clonedDocTypeNodePtr, 0, sizeof(TclDomDocTypeNode));
	        clonedDocTypeNodePtr->nodeId = nodeId;
	        clonedDocTypeNodePtr->nodeType = docTypeNodePtr->nodeType;
	        clonedDocTypeNodePtr->containingDocumentPtr = containingDocumentPtr;
	        if (docTypeNodePtr->nodeName) {
		        clonedDocTypeNodePtr->nodeName = 
						ckalloc(strlen(docTypeNodePtr->nodeName) + 1);
		        strcpy(clonedDocTypeNodePtr->nodeName, 
						docTypeNodePtr->nodeName);
	        }
	        if (docTypeNodePtr->nodeValue) {
		        clonedDocTypeNodePtr->valueLength = docTypeNodePtr->valueLength;
		        clonedDocTypeNodePtr->nodeValue = 
						ckalloc(docTypeNodePtr->valueLength + 1);
		        strcpy(clonedDocTypeNodePtr->nodeValue, 
						docTypeNodePtr->nodeValue);
	        }
	        clonedNodePtr = (TclDomNode *) clonedDocTypeNodePtr;
	        break;

	    case DOCUMENT_FRAGMENT_NODE:
	        break;

	    default:
	        break;
    }
    return clonedNodePtr;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomCloneNode --
 *
 *	This clones a DOM node and returns a handle in the
 *	interpreter's result. If the node is a document then
 *	a new document is created; otherwise the node is
 *	added to the current documents list of fragments.
 *
 * Results:
 *	Returns TCL_OK, or TCL_ERROR (shouldn't happen) if
 *	an internal error occurs.
 *
 * Side effects:
 *	Allocates a token for the new node.
 *
 *--------------------------------------------------------------
 */

int 
TclDomCloneNode(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */ 
    TclDomNode *nodePtr,		/* Node to be copied */
    int deepFlag)			/* True => copy children */
{
    TclDomNode *clonedNodePtr;

    clonedNodePtr = CloneNode(interp, interpDataPtr, nodePtr, 
            nodePtr->containingDocumentPtr, deepFlag);
    if (clonedNodePtr) {
	    if (clonedNodePtr->nodeType != DOCUMENT_NODE) {
	        /*
	         * Add the clone to the fragments list
	         */

	        if (nodePtr->containingDocumentPtr->fragmentsPtr) {
		        clonedNodePtr->nextSiblingPtr = 
		    		    nodePtr->containingDocumentPtr->fragmentsPtr;
		        nodePtr->containingDocumentPtr->fragmentsPtr->previousSiblingPtr = 
		    		    clonedNodePtr;
		        nodePtr->containingDocumentPtr->fragmentsPtr = clonedNodePtr;
	        } else {
		        nodePtr->containingDocumentPtr->fragmentsPtr = clonedNodePtr;
	        }
	    }
	    TclDomSetNodeResult(interp, interpDataPtr, clonedNodePtr);
    }

    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomImportNode --
 *
 *	This procedure imports a node from one document to
 *  another. The source node is not altered or removed
 *  from the original document; this method creates
 *  a new copy of the source node.
 *	
 *
 * Results:
 *	Returns TCL_OK, or TCL_ERROR if the node is of a type
 *  that can't be imported.
 *
 * Side effects:
 *	Allocates a token for the new node.
 *
 *--------------------------------------------------------------
 */

TclDomNode *
TclDomImportNode(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomDocument *documentPtr, /* Document copied into */
    TclDomNode *nodePtr,		/* Node to be copied */
    int deepFlag)			/* True => copy children */
{
    TclDomNode *clonedNodePtr;

    if (nodePtr->nodeType == DOCUMENT_NODE 
            || nodePtr->nodeType == DOCUMENT_TYPE_NODE) {
        Tcl_AppendResult(interp, NOT_SUPPORTED_ERR_TEXT, (char *) NULL);
        return NULL;
    }

    clonedNodePtr = CloneNode(interp, interpDataPtr, nodePtr, 
            documentPtr, deepFlag);
    if (clonedNodePtr) {
	    /*
	     * Add the clone to the fragments list
	     */

	    if (documentPtr->fragmentsPtr) {
		    clonedNodePtr->nextSiblingPtr = documentPtr->fragmentsPtr;
		    documentPtr->fragmentsPtr->previousSiblingPtr = clonedNodePtr;
		    documentPtr->fragmentsPtr = clonedNodePtr;
	    } else {
		    documentPtr->fragmentsPtr = clonedNodePtr;
	    }
	    TclDomSetNodeResult(interp, interpDataPtr, clonedNodePtr);
    }
    return clonedNodePtr;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomRemoveAttribute --
 *
 *	This procedure removes an attribute from an Element's
 *	list of attributes. Implements the Element 
 *	"removeAttribute" method. See the DOM specification
 *	for further information.
 *
 * Results:
 *	Returns TCL_OK; if the attribute was not found, 
 *	then TCL_ERROR is returned and a string corresponding
 *	to NOT_FOUND_ERR is written to the interpreter's result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int 
TclDomRemoveAttribute(
    Tcl_Interp *interp,			/* Tcl interpreter */ 
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomNode *nodePtr,		/* Node from which to remove attribute */
    char *name)				/* Attribute name */
{
    TclDomAttributeNode *attributeNodePtr, *previousPtr = NULL;

    /*
     * XXX
     *
     * Need to do something about default value!
     *
     */

    attributeNodePtr = nodePtr->firstAttributePtr;

    while (attributeNodePtr && strcmp(attributeNodePtr->nodeName, name)) {
	    previousPtr = attributeNodePtr;
	    attributeNodePtr = attributeNodePtr->nextSiblingPtr;
    }

    if (attributeNodePtr) {
	    if (previousPtr) {
	        previousPtr->nextSiblingPtr = attributeNodePtr->nextSiblingPtr;
	    } else {
	        nodePtr->firstAttributePtr = attributeNodePtr->nextSiblingPtr;
	    }
	    if (attributeNodePtr->nextSiblingPtr == NULL) {
	        nodePtr->lastAttributePtr = previousPtr;
	    }
	    RemoveAttributeFromArray(interp, interpDataPtr, nodePtr, 
				attributeNodePtr);
	    TclDomDeleteNode(interp, interpDataPtr, 
				(TclDomNode *) attributeNodePtr);
    } 
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomSetAttribute --
 *
 *	This procedure adds an attribute to an Element's
 *	list of attributes. Implements the Element 
 *	"setAttribute" method. See the DOM specification
 *	for further information.
 *
 * Results:
 *	Returns TCL_OK; if the attribute name is invalid,
 *	the returns TCL_ERROR and writes a string 
 *	corresponding to INVALID_CHARACTER_ERR to the 
 *	interpreter's result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int 
TclDomSetAttribute(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomNode *nodePtr,		/* Node for which attribute is to be set */
    char *name,				/* Attribute's name */
    char *value)			/* Attribute's value */
{
    TclDomAttributeNode *attributeNodePtr;

    attributeNodePtr = nodePtr->firstAttributePtr;

    while (attributeNodePtr && strcmp(attributeNodePtr->nodeName, name)) {
	    attributeNodePtr = attributeNodePtr->nextSiblingPtr;
    }

    if (attributeNodePtr) {
	    ckfree(attributeNodePtr->nodeValue);
	    attributeNodePtr->valueLength = strlen(value);
	    attributeNodePtr->nodeValue = 
				ckalloc(attributeNodePtr->valueLength + 1);
	    strcpy(attributeNodePtr->nodeValue, value);
    } else {
	    attributeNodePtr = (TclDomAttributeNode *) 
				ckalloc(sizeof(TclDomAttributeNode));
	    memset(attributeNodePtr, 0, sizeof(TclDomAttributeNode));
	    attributeNodePtr->nodeName = ckalloc(strlen(name) + 1);
	    strcpy(attributeNodePtr->nodeName, name);
	    attributeNodePtr->parentNodePtr = nodePtr;
	    attributeNodePtr->valueLength = strlen(value);
	    attributeNodePtr->nodeValue = 
				ckalloc(attributeNodePtr->valueLength + 1);
	    strcpy(attributeNodePtr->nodeValue, value);

	    if (nodePtr->firstAttributePtr) {
	        nodePtr->lastAttributePtr->nextSiblingPtr = attributeNodePtr;
	        nodePtr->lastAttributePtr = attributeNodePtr;
	    } else {
	        nodePtr->firstAttributePtr = nodePtr->lastAttributePtr = 
					attributeNodePtr;
	    }
    }
    SetAttributeInArray(interp, interpDataPtr, nodePtr, attributeNodePtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TclDomValidateChildType --
 *
 *	This procedure determines whether a node may legally
 *	be appended to another as a child.
 *
 * Results:
 *	Returns TCL_OK if the child's type is valid; otherwise
 *	returns TCL_ERROR and writes an appropriate error
 *	message to the interpreter's result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int 
TclDomValidateChildType(
    Tcl_Interp *interp,		/* Tcl interpreter */
    TclDomNode *nodePtr,	/* Node to receive child */
    TclDomNode *childPtr)	/* Child node to be added */
{
    /*
     * Handle DocumentFragment as a special case -- validate the children of the
     * DocumentFragment 
     */

    if (childPtr->nodeType == DOCUMENT_FRAGMENT_NODE) {
	    TclDomNode *tempNodePtr = childPtr->firstChildPtr;
	    while (tempNodePtr) {
	        if (TclDomValidateChildType(interp, nodePtr, tempNodePtr) 
					!= TCL_OK) {
		        return TCL_ERROR;
	        }
	        tempNodePtr = tempNodePtr->nextSiblingPtr;
	    }
	    return TCL_OK;
    }

    switch (nodePtr->nodeType) {
	    case ELEMENT_NODE:
	        if (childPtr->nodeType != ELEMENT_NODE 
		            && childPtr->nodeType != TEXT_NODE
		            && childPtr->nodeType != COMMENT_NODE
		            && childPtr->nodeType != PROCESSING_INSTRUCTION_NODE
		            && childPtr->nodeType != CDATA_SECTION_NODE
		            && childPtr->nodeType != ENTITY_REFERENCE_NODE) {
		        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, 
						(char *) NULL);
		        return TCL_ERROR;
	        }
	        break;
    
	    case ATTRIBUTE_NODE:
	        if (childPtr->nodeType != TEXT_NODE 
		            && childPtr->nodeType != ENTITY_REFERENCE_NODE) {
		        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, 
						(char *) NULL);
		        return TCL_ERROR;
	        }
	        break;

	    case TEXT_NODE:
	        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, (char *) NULL);
	        return TCL_ERROR;
	        break;

	    case CDATA_SECTION_NODE:
	        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, (char *) NULL);
	        return TCL_ERROR;
	        break;

	    case ENTITY_REFERENCE_NODE:
	        if (childPtr->nodeType != ELEMENT_NODE 
		            && childPtr->nodeType != TEXT_NODE
		            && childPtr->nodeType != COMMENT_NODE
		            && childPtr->nodeType != PROCESSING_INSTRUCTION_NODE
		            && childPtr->nodeType != CDATA_SECTION_NODE
		            && childPtr->nodeType != ENTITY_REFERENCE_NODE) {
		        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, 
						(char *) NULL);
		        return TCL_ERROR;
	        }
	        break;

	    case ENTITY_NODE:
	        if (childPtr->nodeType != ELEMENT_NODE 
		            && childPtr->nodeType != TEXT_NODE
		            && childPtr->nodeType != COMMENT_NODE
		            && childPtr->nodeType != PROCESSING_INSTRUCTION_NODE
		            && childPtr->nodeType != CDATA_SECTION_NODE
		            && childPtr->nodeType != ENTITY_REFERENCE_NODE) {
		        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, 
						(char *) NULL);
		        return TCL_ERROR;
	        }
	        break;

	    case PROCESSING_INSTRUCTION_NODE:
	        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, (char *) NULL);
	        return TCL_ERROR;
	        break;

	    case COMMENT_NODE:
	        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, (char *) NULL);
	        return TCL_ERROR;
	        break;

	    case DOCUMENT_NODE:
	        if (childPtr->nodeType != ELEMENT_NODE 
		            && childPtr->nodeType != COMMENT_NODE
		            && childPtr->nodeType != PROCESSING_INSTRUCTION_NODE
		            && childPtr->nodeType != DOCUMENT_TYPE_NODE) {
		        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, 
						(char *) NULL);
		        return TCL_ERROR;
	        }

	        /* 
	         * Allow only one child that is an element
		 * It is however legal to re-insert the root element.
	         */

	        if (childPtr->nodeType == ELEMENT_NODE) {
		        TclDomNode *tempNodePtr = nodePtr->firstChildPtr;
		        while (tempNodePtr) {
		            if ( tempNodePtr->nodeType == ELEMENT_NODE
			         && tempNodePtr != childPtr)
			    {
				Tcl_AppendResult(interp, 
					HIERARCHY_REQUEST_ERR_TEXT, 
					(char *) NULL);
			            return TCL_ERROR;
		            }
		            tempNodePtr = tempNodePtr->nextSiblingPtr;
		        }
	        }
	        break;

	    case DOCUMENT_TYPE_NODE:
	        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, (char *) NULL);
	        return TCL_ERROR;
	        break;

	    case DOCUMENT_FRAGMENT_NODE:
	        if (childPtr->nodeType != ELEMENT_NODE 
		            && childPtr->nodeType != TEXT_NODE
		            && childPtr->nodeType != COMMENT_NODE
		            && childPtr->nodeType != PROCESSING_INSTRUCTION_NODE
		            && childPtr->nodeType != CDATA_SECTION_NODE
		            && childPtr->nodeType != ENTITY_REFERENCE_NODE) {
		        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, 
						(char *) NULL);
		        return TCL_ERROR;
	        }
	        break;


	    case NOTATION_NODE:
	        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, (char *) NULL);
	        return TCL_ERROR;
	        break;

	    default:
	        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, (char *) NULL);
	        return TCL_ERROR;
    }
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * GetUniqueListVariableName --
 *
 *	Returns a unique name that may be used to
 *	represent a DOM live NodeList or NodeMap.
 *
 * Results:
 *	A Tcl_Obj whose string value is globally unique.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

Tcl_Obj *
GetUniqueListVariableName(
    Tcl_Interp *interp,	 /* Interpreter in which Tcl variable will be created */
    char *prefix,		    /* Fixed prefix for name */
    int createFlag)		    /* True => implies set variable value to null. */
{
    char *nameString;
    int seed = 0;
    int nameLength;
    const char *value;
    Tcl_Obj *objNamePtr;
    Tcl_Obj *listObjPtr;

    nameLength = strlen(prefix) + strlen(NAMESPACE_PREFIX) + 64;

    nameString = ckalloc(nameLength);

    sprintf(nameString, "%s_%s", NAMESPACE_PREFIX, prefix);
    while (1) {
	    value = Tcl_GetVar(interp, nameString, 0);
	    if (value == NULL) {
	        break;
	    }
	    sprintf(nameString, "%s_%s_%d", NAMESPACE_PREFIX, prefix, seed);
	    seed++;
    }

    objNamePtr = Tcl_NewStringObj(nameString, -1);
    ckfree(nameString);
    if (createFlag) {
	    listObjPtr = Tcl_NewListObj(0, NULL);
	    Tcl_ObjSetVar2(interp, objNamePtr, NULL, listObjPtr, 0);
    }

    return objNamePtr;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomUpdateChildNodeList --
 *
 *	This procedure updates the global Tcl variable
 *	whose value is the list of children of an node.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies a Tcl global variable.
 *
 *--------------------------------------------------------------
 */

void 
TclDomUpdateChildNodeList(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomNode *nodePtr)		/* Node for which child list is to be updated */
{
    if ((nodePtr->nodeType == ELEMENT_NODE 
	        || nodePtr->nodeType == DOCUMENT_NODE 
	        || nodePtr->nodeType == DOCUMENT_FRAGMENT_NODE) 
			&& nodePtr->childNodeListVarName) {
	    Tcl_Obj *newListPtr, *tokenPtr;
	    TclDomNode *childPtr;
	    int result;

	    newListPtr = Tcl_NewListObj(0, NULL);
	    childPtr = nodePtr->firstChildPtr;
	    while (childPtr) {
	        tokenPtr = TclDomGetNodeObj(interpDataPtr, childPtr);
	        result = Tcl_ListObjAppendElement(interp, newListPtr, tokenPtr);
	        if (result != TCL_OK) {
		        Tcl_DecrRefCount(tokenPtr);
		        return;
	        }
	        childPtr = childPtr->nextSiblingPtr;
	    }
	    Tcl_ObjSetVar2(interp, nodePtr->childNodeListVarName, NULL, 
				newListPtr, 0);
    }
}


/*
 *--------------------------------------------------------------
 *
 * TclDomGetChildNodeList --
 *
 *	This procedure creates a global Tcl variable whose
 *	value will be the list of children of the specified
 *	node. The variable is "live", and implements the
 *	NodeList object in the DOM specification.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates or modifies a Tcl global variable.
 *
 *--------------------------------------------------------------
 */

int 
TclDomGetChildNodeList(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomNode *nodePtr)		/* Node for which to get child list */
{
    if (nodePtr->nodeType != ELEMENT_NODE && nodePtr->nodeType != DOCUMENT_NODE 
	        && nodePtr->nodeType != DOCUMENT_FRAGMENT_NODE) {
	    /*
	     * This node type has no children;
	     * return a variable whose value is an invariant  list.
	     */
	    if (interpDataPtr->nullNodeListVarName == NULL) {
#ifdef UNDEF
	        char prefix[64];
	        sprintf(prefix, "doc%dEmptyList", 
		            nodePtr->containingDocumentPtr->selfPtr->nodeId);
	        interpDataPtr->nullNodeListVarName = 
			        GetUniqueListVariableName(interp, prefix, 1);
#endif
	        interpDataPtr->nullNodeListVarName = 
			        GetUniqueListVariableName(interp, "emptyList", 1);
	        Tcl_IncrRefCount(interpDataPtr->nullNodeListVarName);
	    }
	    Tcl_ObjSetVar2(interp, interpDataPtr->nullNodeListVarName, NULL, 
	            Tcl_NewStringObj("", -1), 0);
	    Tcl_SetObjResult(interp, interpDataPtr->nullNodeListVarName);
	    return TCL_OK;
    } else {
	    if (nodePtr->childNodeListVarName == NULL) {
	        char prefix[64];
	        sprintf(prefix, "node%dChildList", nodePtr->nodeId);
	        nodePtr->childNodeListVarName = GetUniqueListVariableName(interp, 
			        prefix, 1);
	        Tcl_IncrRefCount(nodePtr->childNodeListVarName);
	    }
	    TclDomUpdateChildNodeList(interp, interpDataPtr, nodePtr);
	    Tcl_SetObjResult(interp, nodePtr->childNodeListVarName);
	    return TCL_OK;
    }
}


/*
 *--------------------------------------------------------------
 *
 * TclDomGetChildren --
 *
 *	Returns a List object containing the children
 *	of the specified node.  
 *
 *	This value is *not* "live"; it is used to implement the 
 *	(nonstandard) dom::node children method.
 *
 * Results:
 *	The new Tcl_ListObj, or NULL if there was an error.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

Tcl_Obj * 
TclDomGetChildren(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomNode *nodePtr)		/* Node for which to get child list */
{
    Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
    TclDomNode *childPtr =
    	  TclDomHasChildren(nodePtr) ? nodePtr->firstChildPtr : NULL;
    Tcl_Obj *childObj = NULL;

    while (childPtr != NULL)
    {
	childObj = TclDomGetNodeObj(interpDataPtr, childPtr);
	if (Tcl_ListObjAppendElement(interp, listObj, childObj)
		!= TCL_OK) goto error;
	childObj = NULL;
	childPtr = childPtr->nextSiblingPtr;
    }
    return listObj;
error:
    if (childObj) Tcl_DecrRefCount(childObj);
    Tcl_DecrRefCount(listObj);
    return NULL;
}

/*
 *--------------------------------------------------------------
 *
 * RemoveAttributeFromArray --
 *
 *	This procedure removes an attribute from the Tcl
 *	array variable that represents the NamedNodeMap of 
 *	attributes of a node. Implements the Node "attributes"
 *	attribute.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies a Tcl global variable.
 *
 *--------------------------------------------------------------
 */

static void 
RemoveAttributeFromArray(
    Tcl_Interp *interp,			    /* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	    /* Extension state data */
    TclDomNode *nodePtr,		    /* Node from which attribute was removed */
    TclDomAttributeNode *attributeNodePtr)  /* Removed attribute */
{
    if (nodePtr->attributeArrayVarName) {
	    char *arrayName = Tcl_GetStringFromObj(nodePtr->attributeArrayVarName, 
				NULL);
	    Tcl_UnsetVar2(interp, arrayName, attributeNodePtr->nodeName, 0);
    }
}


/*
 *--------------------------------------------------------------
 *
 * SetAttributeInArray --
 *
 *	This procedure updates an attribute in the Tcl
 *	array variable that represents the NamedNodeMap of 
 *	attributes of a node. Implements the Node "attributes"
 *	attribute.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies a Tcl global variable.
 *
 *--------------------------------------------------------------
 */

static void 
SetAttributeInArray(
    Tcl_Interp *interp,			    /* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	    /* Extension state data */
    TclDomNode *nodePtr,		    /* Element node containing attribute */
    TclDomAttributeNode *attributeNodePtr)  /* Attribute node */
{
    Tcl_Obj *nameObjPtr;
    Tcl_Obj *valueObjPtr;
    if (nodePtr->attributeArrayVarName) {
	    nameObjPtr = Tcl_NewStringObj(attributeNodePtr->nodeName, -1);
	    valueObjPtr = Tcl_NewStringObj(attributeNodePtr->nodeValue, -1);
	    Tcl_ObjSetVar2(interp, nodePtr->attributeArrayVarName, nameObjPtr, 
				valueObjPtr, 0);
    }
}


/*
 *--------------------------------------------------------------
 *
 * InitializeAttributeArray --
 *
 *	This procedure initializes attributes in the Tcl
 *	array variable that represents the NamedNodeMap of 
 *	attributes of a node. Implements the Node "attributes"
 *	attribute.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies a Tcl global variable.
 *
 *--------------------------------------------------------------
 */

static void 
InitializeAttributeArray(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomNode *nodePtr)		/* Node containing attributes */
{
    if (nodePtr->nodeType == ELEMENT_NODE && nodePtr->attributeArrayVarName) {
	    TclDomAttributeNode *attributeNodePtr;
	    Tcl_Obj *nameObjPtr;
	    Tcl_Obj *valueObjPtr;

	    attributeNodePtr = nodePtr->firstAttributePtr;

	    while (attributeNodePtr) {
	        nameObjPtr = Tcl_NewStringObj(attributeNodePtr->nodeName, -1);
	        valueObjPtr = Tcl_NewStringObj(attributeNodePtr->nodeValue, -1);

	        Tcl_ObjSetVar2(interp, nodePtr->attributeArrayVarName, nameObjPtr, 
					valueObjPtr, 0);
	        attributeNodePtr = attributeNodePtr->nextSiblingPtr;
	    }
    }
}


/*
 *--------------------------------------------------------------
 *
 * TclDomAttributeArray --
 *
 *	This procedure creates and initializes a Tcl global
 *	array variable that represents the attributes of a
 *	node.  Implements the Node "attributes" attribute. See
 *	the DOM specification for futher information.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates if needed, and then modifies a Tcl global 
 *	variable.
 *
 *--------------------------------------------------------------
 */

int 
TclDomAttributeArray(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomNode *nodePtr)		/* Element node containing attributes */
{
    if (nodePtr->nodeType != ELEMENT_NODE) {
	    /*
	     * Just return null
	     */

	    return TCL_OK;
    } else {
	    if (nodePtr->attributeArrayVarName == NULL) {
	        char prefix[128];
	        sprintf(prefix, "node%dAttributes", nodePtr->nodeId);
	        nodePtr->attributeArrayVarName = 
					GetUniqueListVariableName(interp, prefix, 0);
	        Tcl_IncrRefCount(nodePtr->attributeArrayVarName);
	    }
	    InitializeAttributeArray(interp, interpDataPtr, nodePtr);
	    Tcl_SetObjResult(interp, nodePtr->attributeArrayVarName);
	    return TCL_OK;
    }
}


/*
 *--------------------------------------------------------------
 *
 * TclDomAppendChild
 *
 *	This procedure appends a child node to a nodes list
 *	of children. Implements the Node "appendChild"
 *	method.	 See the DOM specification for further
 *	information.
 *
 * Results:
 *	Returns TCL_OK, or TCL_ERROR if the node can't be added.
 *
 * Side effects:
 *	Will set the interpreter's result to appropriate error
 *	text if the action fails. May update the list
 *	of document fragments. Updates the parents NodeList
 *	if it exists.
 *
 *--------------------------------------------------------------
 */

int 
TclDomAppendChild(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomNode *nodePtr,		/* Node to append child to */
    TclDomNode *childPtr)		/* Child to be appended */
{
    TclDomNode *tempNodePtr;
    int isFragment;

    if (nodePtr->containingDocumentPtr != childPtr->containingDocumentPtr) {
	    Tcl_AppendResult(interp, WRONG_DOCUMENT_ERR_TEXT, (char *) NULL);
	    return TCL_ERROR;
    }

    if (childPtr->nodeType == DOCUMENT_FRAGMENT_NODE) {
	    TclDomNode *fragmentChildPtr = childPtr->firstChildPtr;
	    TclDomNode *nextChildPtr;
	    while (fragmentChildPtr) {
	        /*
	         * Need to pick up "nextSiblingPtr" now, as it will be trashed
		 * in TclDomAppendChild
	         */
	        nextChildPtr = fragmentChildPtr->nextSiblingPtr;
	        if (TclDomAppendChild(interp, interpDataPtr, nodePtr, 
				fragmentChildPtr) != TCL_OK) {
		        return TCL_ERROR;
	        }
	        fragmentChildPtr = nextChildPtr;
	        /*
	         * Need to keep "childPtr->firstChildPtr" valid in case 
		 * TclDomAppendChild fails for some reason
	         */
	        childPtr->firstChildPtr = fragmentChildPtr;
	    }
	    childPtr->lastChildPtr = NULL;
	    UnlinkDocumentFragment(nodePtr->containingDocumentPtr, childPtr);
	    TclDomDeleteNode(interp, interpDataPtr, childPtr);
	    return TCL_OK;
    }

    /*
     * Check that node to be appended is not an ancestor of the node
     */

    tempNodePtr = nodePtr;
    while (tempNodePtr) {
	    if (tempNodePtr->parentNodePtr == childPtr) {
	        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, (char *) NULL);
	        return TCL_ERROR;
	    }
	    tempNodePtr = tempNodePtr->parentNodePtr;
    }

    /*
     * If the child is in the fragment list, remove it
     *
     */

    isFragment = UnlinkDocumentFragment(nodePtr->containingDocumentPtr, 
		childPtr);

    if (isFragment == 0) {
	    /*
	     * Remove child from document tree
	     */

	    UnlinkChild(interpDataPtr, childPtr);
    }

    /*
     * Finally, append the child
     */

    if (nodePtr->lastChildPtr) {
	    nodePtr->lastChildPtr->nextSiblingPtr = childPtr;
	    childPtr->previousSiblingPtr = nodePtr->lastChildPtr;
    } else {
	    nodePtr->firstChildPtr = childPtr;
	    childPtr->previousSiblingPtr = NULL;
    }
    nodePtr->lastChildPtr = childPtr;
    childPtr->nextSiblingPtr = NULL;
    childPtr->parentNodePtr = nodePtr;

    /*
     * Update the "live" list
     */

    TclDomUpdateChildNodeList(interp, interpDataPtr, nodePtr);
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomInsertBefore
 *
 *	This procedure inserts a child node in nodes list
 *	of children. Implements the Node "insertBefore"
 *	method.	 See the DOM specification for further
 *	information.
 *
 * Results:
 *	Returns TCL_OK, or TCL_ERROR if the node can't be added.
 *
 * Side effects:
 *	Will set the interpreter's result to appropriate error
 *	text if the action fails. May update the list
 *	of document fragments. Updates the parents NodeList
 *	if it exists.
 *
 *--------------------------------------------------------------
 */

int 
TclDomInsertBefore(
    Tcl_Interp *interp,			/* Tcl interpreter */ 
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomNode *nodePtr,		/* Node in which to insert child */
    TclDomNode *childPtr,		/* Child to be inserted */
    TclDomNode *refChildPtr)		/* Insert child before this node */
{
    TclDomNode *tempNodePtr, *locationPtr;
    int isFragment;

    if (nodePtr->containingDocumentPtr != childPtr->containingDocumentPtr) {
	    Tcl_AppendResult(interp, WRONG_DOCUMENT_ERR_TEXT, (char *) NULL);
	    return TCL_ERROR;
    }

    if (childPtr->nodeType == DOCUMENT_FRAGMENT_NODE) {
	    TclDomNode *fragmentChildPtr = childPtr->firstChildPtr;
	    TclDomNode *nextChildPtr;
	    while (fragmentChildPtr) {
	        /*
	         * Need to pick up "nextSiblingPtr" now, as it will be trashed in 
			 * TclDomAppendChild
	         */
	        nextChildPtr = fragmentChildPtr->nextSiblingPtr;
	        if (TclDomInsertBefore(interp, interpDataPtr, nodePtr, 
			        fragmentChildPtr, refChildPtr) != TCL_OK) {
		        return TCL_ERROR;
	        }
	        fragmentChildPtr = nextChildPtr;
	        /*
	         * Need to keep "childPtr->firstChildPtr" valid in case 
			 * TclDomAppendChild fails for some reason
	         */
	        childPtr->firstChildPtr = fragmentChildPtr;
	    }
	    childPtr->lastChildPtr = NULL;
	    UnlinkDocumentFragment(nodePtr->containingDocumentPtr, childPtr);
	    TclDomDeleteNode(interp, interpDataPtr, childPtr);
	    return TCL_OK;
    }

    /*
     * Check that node to be appended is not an ancestor of the node
     */

    tempNodePtr = nodePtr;
    while (tempNodePtr) {
	    if (tempNodePtr->parentNodePtr == childPtr) {
	        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, (char *) NULL);
	        return TCL_ERROR;
	    }
	    tempNodePtr = tempNodePtr->parentNodePtr;
    }

    /*
     * Validate the reference node
     */
    
    locationPtr = nodePtr->firstChildPtr;
    while (locationPtr) {
	    if (locationPtr == refChildPtr) {
	        break;
	    }
	    locationPtr = locationPtr->nextSiblingPtr;
    }

    if (locationPtr == NULL) {
	    Tcl_AppendResult(interp, NOT_FOUND_ERR_TEXT, (char *) NULL);
	    return TCL_OK;
    }

    /*
     * If the child is in the fragment list, remove it
     */

    isFragment = UnlinkDocumentFragment(nodePtr->containingDocumentPtr, 
			childPtr);

    if (isFragment == 0) {
	    /*
	     * Remove child from document tree
	     */

	    UnlinkChild(interpDataPtr, childPtr);
    }

    /*
     * Finally, insert the child
     */

    childPtr->nextSiblingPtr = refChildPtr;
    if (refChildPtr->previousSiblingPtr) {
	    childPtr->previousSiblingPtr = refChildPtr->previousSiblingPtr;
	    refChildPtr->previousSiblingPtr->nextSiblingPtr = childPtr;
    } else {
	    nodePtr->firstChildPtr = childPtr;
	    childPtr->previousSiblingPtr = NULL;
    }
    refChildPtr->previousSiblingPtr = childPtr;
    childPtr->parentNodePtr = nodePtr;
    TclDomUpdateChildNodeList(interp, interpDataPtr, nodePtr);
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomReplaceChild
 *
 *	This procedure replaces a child node in nodes list
 *	of children. Implements the Node "replaceChild"
 *	method.	 See the DOM specification for further
 *	information.
 *
 * Results:
 *	Returns TCL_OK, or TCL_ERROR if the node can't be added.
 *
 * Side effects:
 *	Will set the interpreter's result to appropriate error
 *	text if the action fails. May update the list
 *	of document fragments. Updates the parents NodeList
 *	if it exists.
 *
 *--------------------------------------------------------------
 */

int 
TclDomReplaceChild(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* State data for interpreter */
    TclDomNode *nodePtr,		/* Parent of child node */
    TclDomNode *newChildPtr,		/* New child */
    TclDomNode *oldChildPtr)		/* Child to be replaced */
{
    TclDomNode *tempNodePtr, *locationPtr;
    int isFragment;

    if (nodePtr->containingDocumentPtr != newChildPtr->containingDocumentPtr) {
	    Tcl_AppendResult(interp, WRONG_DOCUMENT_ERR_TEXT, (char *) NULL);
	    return TCL_ERROR;
    }

    if (newChildPtr->nodeType == DOCUMENT_FRAGMENT_NODE) {
	    int haveReplaced = 0;
	    TclDomNode *replaceParentPtr;
	    TclDomNode *fragmentChildPtr = newChildPtr->firstChildPtr;
	    TclDomNode *nextChildPtr;
	    replaceParentPtr = oldChildPtr->parentNodePtr;
	    while (fragmentChildPtr) {
	        /*
	         * Need to pick up "nextSiblingPtr" now, as it will be trashed in 
			 * TclDomAppendChild
	         */
	        nextChildPtr = fragmentChildPtr->nextSiblingPtr;
	        if (haveReplaced == 0) {
		        if (TclDomReplaceChild(interp, interpDataPtr, nodePtr, 
						fragmentChildPtr, oldChildPtr) != TCL_OK) {
		            return TCL_ERROR;
		        }
		        haveReplaced = 1;
	        } else {
		        if (TclDomAppendChild(interp, interpDataPtr, replaceParentPtr, 
						fragmentChildPtr) != TCL_OK) {
		            return TCL_ERROR;
		        }
	        }
	        fragmentChildPtr = nextChildPtr;
	        /*
	         * Need to keep "childPtr->firstChildPtr" valid in case 
		     * TclDomAppendChild fails for some reason
	         */
	        newChildPtr->firstChildPtr = fragmentChildPtr;
	    }
	    newChildPtr->lastChildPtr = NULL;
	    UnlinkDocumentFragment(nodePtr->containingDocumentPtr, newChildPtr);
	    TclDomDeleteNode(interp, interpDataPtr, newChildPtr);
	    return TCL_OK;
    }

    /*
     * Check that node to be appended is not an ancestor of the node
     */

    tempNodePtr = nodePtr;
    while (tempNodePtr) {
	    if (tempNodePtr->parentNodePtr == newChildPtr) {
	        Tcl_AppendResult(interp, HIERARCHY_REQUEST_ERR_TEXT, (char *) NULL);
	        return TCL_ERROR;
	    }
	    tempNodePtr = tempNodePtr->parentNodePtr;
    }

    /*
     * Validate the node to be replaced
     */

    locationPtr = nodePtr->firstChildPtr;

    while (locationPtr) {
	    if (locationPtr == oldChildPtr) {
	        break;
	    }
	    locationPtr = locationPtr->nextSiblingPtr;
    }

    if (locationPtr == NULL) {
	    Tcl_AppendResult(interp, NOT_FOUND_ERR_TEXT, (char *) NULL);
	    return TCL_OK;
    }

    /*
     * If the child is in the fragment list, remove it
     */

    isFragment = UnlinkDocumentFragment(nodePtr->containingDocumentPtr, 
			newChildPtr);

    if (isFragment == 0) {
	    /*
	     * Remove child from document tree
	     */

	    UnlinkChild(interpDataPtr, newChildPtr);
    }

    /*
     * Finally, replace the old child with the new
     */

    newChildPtr->nextSiblingPtr = oldChildPtr->nextSiblingPtr;
    newChildPtr->previousSiblingPtr = oldChildPtr->previousSiblingPtr;
    newChildPtr->parentNodePtr = nodePtr;
    if (oldChildPtr->previousSiblingPtr) {
	    oldChildPtr->previousSiblingPtr->nextSiblingPtr = newChildPtr;
    } else {
	    oldChildPtr->parentNodePtr->firstChildPtr = newChildPtr;
    }
    if (oldChildPtr->nextSiblingPtr) {
	    oldChildPtr->nextSiblingPtr->previousSiblingPtr = newChildPtr;
    } else {
	    oldChildPtr->parentNodePtr->lastChildPtr = newChildPtr;
    }

    /*
     * Add the old child to the fragments list
     */

    if (oldChildPtr->containingDocumentPtr->fragmentsPtr) {
	    oldChildPtr->nextSiblingPtr = 
				oldChildPtr->containingDocumentPtr->fragmentsPtr;
	    oldChildPtr->containingDocumentPtr->fragmentsPtr->previousSiblingPtr = 
				oldChildPtr;
	    oldChildPtr->containingDocumentPtr->fragmentsPtr = oldChildPtr;
    } else {
	    oldChildPtr->containingDocumentPtr->fragmentsPtr = oldChildPtr;
	    oldChildPtr->nextSiblingPtr = NULL;
    }
    oldChildPtr->previousSiblingPtr = NULL;
    oldChildPtr->parentNodePtr = NULL;

    TclDomUpdateChildNodeList(interp, interpDataPtr, nodePtr);
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomRemoveChild
 *
 *	This procedure removes a child node from a node's list
 *	of children. Implements the Node "removeChild"
 *	method.	 See the DOM specification for further
 *	information.
 *
 * Results:
 *	Returns TCL_OK, or TCL_ERROR if the node can't be added.
 *
 * Side effects:
 *	Will set the interpreter's result to appropriate error
 *	text if the action fails. May update the list
 *	of document fragments. Updates the parents NodeList
 *	if it exists.
 *
 *--------------------------------------------------------------
 */

int 
TclDomRemoveChild(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomNode *nodePtr,		/* Node containing child */
    TclDomNode *childPtr)		/* Child to be removed */
{
    TclDomNode *foundPtr;

    /*
     * Find the child
     */

    foundPtr = nodePtr->firstChildPtr;
    while (foundPtr && foundPtr != childPtr) {
	    foundPtr = foundPtr->nextSiblingPtr;
    }

    if (foundPtr) {
	    /*
	     * Remove child
	     */
	    UnlinkChild(interpDataPtr, childPtr);

	    /*
	     * Add child to node fragments list 
	     */

	    if (childPtr->containingDocumentPtr->fragmentsPtr) {
	        childPtr->nextSiblingPtr = 
					childPtr->containingDocumentPtr->fragmentsPtr;
	        childPtr->containingDocumentPtr->fragmentsPtr->previousSiblingPtr =
					childPtr;
	        childPtr->containingDocumentPtr->fragmentsPtr = childPtr;
	    } else {
	        childPtr->containingDocumentPtr->fragmentsPtr = childPtr;
	        childPtr->nextSiblingPtr = NULL;
	    }

	    childPtr->previousSiblingPtr = NULL;
	    childPtr->parentNodePtr = NULL;

	    return TclDomSetNodeResult(interp, interpDataPtr, childPtr);
    }

    Tcl_AppendResult(interp, NOT_FOUND_ERR_TEXT, (char *) NULL);
    return TCL_ERROR;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomCreateProcessingInstructionNode
 *
 *	This procedure implements the Document 
 *	"createProcessingInstruction" method.  See the
 *	DOM specification for further information.
 *
 * Results:
 *	A TclDomNode for the new node.
 *
 * Side effects:
 *	The node is added to the list of document fragments.
 *
 *--------------------------------------------------------------
 */

TclDomNode *
TclDomCreateProcessingInstructionNode(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* State data for extension */
    TclDomDocument *documentPtr,	/* Parent document */
    char *target,			/* Target part of PI */
    char *data)				/* Data for the node */
{
    TclDomNode *nodePtr;

    nodePtr = (TclDomNode *) ckalloc(sizeof(TclDomNode));
    memset(nodePtr, 0, sizeof(TclDomNode));
    nodePtr->nodeType = PROCESSING_INSTRUCTION_NODE;
    nodePtr->containingDocumentPtr = documentPtr;
    nodePtr->nodeId = ++interpDataPtr->nodeSeed;
    nodePtr->nodeName = ckalloc(strlen(target) + 1);
    nodePtr->nodeComplete = 1;
    strcpy(nodePtr->nodeName, target);
    nodePtr->valueLength = strlen(data);
    nodePtr->nodeValue = ckalloc(nodePtr->valueLength + 1);
    strcpy(nodePtr->nodeValue, data);
    AddDocumentFragment(documentPtr, nodePtr);
    return nodePtr;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomCreateDocType
 *
 *	This procedure creates a DocType node. 
 *
 * Results:
 *	A TclDomNode for the new node.
 *
 * Side effects:
 *	The node is added to the list of document fragments.
 *
 *--------------------------------------------------------------
 */

TclDomNode *
TclDomCreateDocType(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomDocument *documentPtr,	/* Parent document */
    char *doctypeName,		/* Document name for node */
    char *publicId,			/* PublicId for node */
    char *systemId)			/* SystemId for node */
{
    TclDomDocTypeNode *nodePtr;

    nodePtr = (TclDomDocTypeNode *) ckalloc(sizeof(TclDomDocTypeNode));
    memset(nodePtr, 0, sizeof(TclDomDocTypeNode));
    nodePtr->nodeType = DOCUMENT_TYPE_NODE;
    nodePtr->containingDocumentPtr = documentPtr;
    nodePtr->nodeId = ++interpDataPtr->nodeSeed;

    nodePtr->nodeName = ckalloc(strlen(doctypeName) + 1);
    strcpy(nodePtr->nodeName, doctypeName);

    if (publicId) {
    	nodePtr->publicId = ckalloc(strlen(publicId) + 1);
    	strcpy(nodePtr->publicId, publicId);
    }

    if (systemId) {
    	nodePtr->systemId = ckalloc(strlen(systemId) + 1);
    	strcpy(nodePtr->systemId, systemId);
    }

    AddDocumentFragment(documentPtr, (TclDomNode*)nodePtr);
    return (TclDomNode *) nodePtr;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomCreateElement
 *
 *	This procedure implements the Document 
 *	"createElement" method.	 See the
 *	DOM specification for further information.
 *
 * Results:
 *	A TclDomNode for the new node.
 *
 * Side effects:
 *	The node is added to the list of document fragments.
 *
 *--------------------------------------------------------------
 */

TclDomNode *
TclDomCreateElement(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomDocument *documentPtr,	/* Parent document */
    char *tagName)			/* Tag name for node */
{
    TclDomNode *nodePtr;

    nodePtr = (TclDomNode *) ckalloc(sizeof(TclDomNode));
    memset(nodePtr, 0, sizeof(TclDomNode));
    nodePtr->nodeType = ELEMENT_NODE;
    nodePtr->containingDocumentPtr = documentPtr;
    nodePtr->nodeId = ++interpDataPtr->nodeSeed;
    nodePtr->nodeComplete = 1;
    nodePtr->nodeName = ckalloc(strlen(tagName) + 1);
    strcpy(nodePtr->nodeName, tagName);
    AddDocumentFragment(documentPtr, nodePtr);
    return nodePtr;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomCreateDocumentFragment
 *
 *	This procedure implements the Document 
 *	"createDocumentFragment" method.  See the
 *	DOM specification for further information.
 *
 * Results:
 *	A TclDomNode for the new node.
 *
 * Side effects:
 *	The node is added to the list of document fragments.
 *
 *--------------------------------------------------------------
 */

TclDomNode *
TclDomCreateDocumentFragment(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomDocument *documentPtr)	/* Parent document */
{
    TclDomNode *nodePtr;

    nodePtr = (TclDomNode *) ckalloc(sizeof(TclDomNode));
    memset(nodePtr, 0, sizeof(TclDomNode));
    nodePtr->nodeType = DOCUMENT_FRAGMENT_NODE;
    nodePtr->containingDocumentPtr = documentPtr;
    nodePtr->nodeId = ++interpDataPtr->nodeSeed;
    nodePtr->nodeComplete = 1;
    if (documentPtr->fragmentsPtr) {
	    nodePtr->nextSiblingPtr = documentPtr->fragmentsPtr;
	    documentPtr->fragmentsPtr = nodePtr;
    } else {
	    documentPtr->fragmentsPtr = nodePtr;
    }
    return nodePtr;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomCreateCharacterDataNode
 *
 *	This procedure creates node corresponding to 
 *	the CharacterData interface as defined in
 *	the DOM specification.
 *
 * Results:
 *	A TclDomNode for the new node.
 *
 * Side effects:
 *	The node is added to the list of document fragments.
 *
 *--------------------------------------------------------------
 */

static TclDomTextNode *
TclDomCreateCharacterDataNode(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomDocument *documentPtr,	/* Parent document */
    TclDomNodeType nodeType,		/* Node type to be created */
    char *characterData)		/* Text data for node */
{
    TclDomTextNode *nodePtr;

    nodePtr = (TclDomTextNode *) ckalloc(sizeof(TclDomTextNode));
    memset(nodePtr, 0, sizeof(TclDomTextNode));
    nodePtr->nodeType = nodeType;
    nodePtr->containingDocumentPtr = documentPtr;
    nodePtr->nodeId = ++interpDataPtr->nodeSeed;
    nodePtr->nodeComplete = 1;
    nodePtr->valueLength = strlen(characterData);
    nodePtr->nodeValue = ckalloc(nodePtr->valueLength + 1);
    strcpy(nodePtr->nodeValue, characterData);
    AddDocumentFragment(documentPtr, (TclDomNode*)nodePtr);
    return nodePtr;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomCreateTextNode
 *
 *	This procedure implements the Document 
 *	"createTextNode" method.  See the
 *	DOM specification for further information.
 *
 * Results:
 *	A TclDomNode for the new node.
 *
 * Side effects:
 *	The node is added to the list of document fragments.
 *
 *--------------------------------------------------------------
 */

TclDomTextNode *
TclDomCreateTextNode(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state */
    TclDomDocument *documentPtr,	/* Parent document */
    char *text)
{
    return TclDomCreateCharacterDataNode(interp, interpDataPtr, documentPtr, 
			TEXT_NODE, text);
}


/*
 *--------------------------------------------------------------
 *
 * TclDomCreateCommentNode
 *
 *	This procedure implements the Document 
 *	"createComment" method.	 See the
 *	DOM specification for further information.
 *
 * Results:
 *	A TclDomNode for the new node.
 *
 * Side effects:
 *	The node is added to the list of document fragments.
 *
 *--------------------------------------------------------------
 */

TclDomNode *
TclDomCreateCommentNode(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomDocument *documentPtr,	/* Parent document */
    char *text)
{
    return (TclDomNode *) TclDomCreateCharacterDataNode(interp, interpDataPtr, 
			documentPtr, COMMENT_NODE, text);
}


/*
 *--------------------------------------------------------------
 *
 * TclDomCreateCDATANode
 *
 *	This procedure implements the Document 
 *	"createCDATASection" method.  See the
 *	DOM specification for further information.
 *
 * Results:
 *	A TclDomNode for the new node.
 *
 * Side effects:
 *	The node is added to the list of document fragments.
 *
 *--------------------------------------------------------------
 */

TclDomNode *
TclDomCreateCDATANode(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension stata data */
    TclDomDocument *documentPtr,	/* Parent document */
    char *text)
{
    return (TclDomNode *) TclDomCreateCharacterDataNode(interp, interpDataPtr, 
			documentPtr, CDATA_SECTION_NODE, text);
}


/*
 *--------------------------------------------------------------
 *
 * TclDomGetNodeFromToken
 *
 *	This procedure maps a TclDomPro node token
 *	into a TclDomNode pointer.
 *
 * Results:
 *	A pointer to the TclDomNode, or null if the
 *	token can't be found.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

TclDomNode *
TclDomGetNodeFromToken(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    Tcl_Obj *nodeTokenPtr)		/* Token string value */
{
    char *token;
    Tcl_HashEntry *entryPtr;
    TclDomNode *nodePtr;

    token = Tcl_GetStringFromObj(nodeTokenPtr, NULL);

    entryPtr = Tcl_FindHashEntry(&interpDataPtr->nodeHashTable, token);
    if (entryPtr == NULL) {
	    Tcl_AppendResult(interp, "token not found", NULL);
	    return NULL;
    }
    nodePtr = (TclDomNode *) Tcl_GetHashValue(entryPtr);
    return nodePtr;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomGetDocumentFromToken
 *
 *	This procedure maps a TclDomPro node token
 *	into its containing document.
 *
 * Results:
 *	A pointer to the TclDomDocument that contains
 *	the node, or NULL if the token can't be found.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */


TclDomDocument *
TclDomGetDocumentFromToken(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    Tcl_Obj *nodeTokenPtr)		/* Token string value */
{
    TclDomNode *nodePtr;

    nodePtr = TclDomGetNodeFromToken(interp, interpDataPtr, nodeTokenPtr);

    /*
     * XXX
     * Steve Ball's tcldom currently doesn't do any type checking; however,
     * we should check the DOM spec and do the correct thing here.
     */

    if (nodePtr) {
	    return nodePtr->containingDocumentPtr;
    } else {
	    return NULL;
    }
}

/*
 *--------------------------------------------------------------
 *
 * TclDomGetDocumentElement --
 *
 *	Returns the document element node of a TclDomDocument,
 *	NULL if none exists.
 *
 *--------------------------------------------------------------
 */

TclDomNode *TclDomGetDocumentElement(TclDomDocument *documentPtr)
{
    TclDomNode *documentNode = documentPtr->selfPtr;
    TclDomNode *childPtr = documentNode ? documentNode->firstChildPtr : NULL;

    while (childPtr && childPtr->nodeType != ELEMENT_NODE)
    	childPtr = childPtr->nextSiblingPtr;

    return childPtr;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomGetDoctypeNode --
 *
 *	Returns the document type declaration node of a TclDomDocument,
 *	NULL if none exists.
 *
 *--------------------------------------------------------------
 */

TclDomNode *TclDomGetDoctypeNode(TclDomDocument *documentPtr)
{
    TclDomNode *documentNode = documentPtr->selfPtr;
    TclDomNode *childPtr = documentNode ? documentNode->firstChildPtr : NULL;

    while (childPtr && childPtr->nodeType != DOCUMENT_TYPE_NODE)
    	childPtr = childPtr->nextSiblingPtr;

    return childPtr;
}


/*
 *--------------------------------------------------------------
 *
 * DestroyDocument
 *
 *	This procedure deletes a DOM document.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes the document and all its children, so 
 *	memory is released and hash tables are deleted.
 *
 *--------------------------------------------------------------
 */


static void 
DestroyDocument(
    char *dataPtr)	/* Document object */
{
    TclDomNode *nodePtr, *tempNodePtr;
    char keyString[32];
    Tcl_HashEntry *entryPtr;
    TclDomNodeIterator *nodeIteratorPtr;
    TclDomTreeWalker *treeWalkerPtr;
    Tcl_HashSearch search;


    TclDomDocument *documentPtr = (TclDomDocument *) dataPtr;

    Tcl_Interp *interp = documentPtr->interp;
    TclDomInterpData *interpDataPtr = documentPtr->interpDataPtr;

    for (entryPtr = Tcl_FirstHashEntry(&interpDataPtr->iteratorHashTable,
	         &search); entryPtr; entryPtr = Tcl_NextHashEntry(&search)) {
	    nodeIteratorPtr = (TclDomNodeIterator *) Tcl_GetHashValue(entryPtr);
	    if (nodeIteratorPtr->rootPtr && 
				nodeIteratorPtr->rootPtr->containingDocumentPtr 
				== documentPtr) {
            nodeIteratorPtr->rootPtr = NULL;
            nodeIteratorPtr->referencePtr = NULL;	        
	    }
    }

    for (entryPtr = Tcl_FirstHashEntry(&interpDataPtr->treeWalkerHashTable,
	        &search); entryPtr; entryPtr = Tcl_NextHashEntry(&search)) {
	    treeWalkerPtr = (TclDomTreeWalker *) Tcl_GetHashValue(entryPtr);
	    if (treeWalkerPtr->rootPtr 
				&& treeWalkerPtr->rootPtr->containingDocumentPtr
		        == documentPtr) {
            treeWalkerPtr->rootPtr = NULL;
            treeWalkerPtr->currentNodePtr = NULL;	       
	    }
    }

    /*
     * Delete the node of the document
     */
    if (documentPtr->selfPtr) {
	    sprintf(keyString, "node%u", documentPtr->selfPtr->nodeId);
	    TclDomDeleteNode(interp, interpDataPtr, documentPtr->selfPtr);
    }

    /*
     * Delete any dangling document fragments
     */

    if (documentPtr->fragmentsPtr) {
	    nodePtr = documentPtr->fragmentsPtr;
	    while (nodePtr) {
	        tempNodePtr = nodePtr->nextSiblingPtr;
	        TclDomDeleteNode(interp, interpDataPtr, nodePtr);
	        nodePtr = tempNodePtr;
	    }
    }

    entryPtr = Tcl_FindHashEntry(&interpDataPtr->documentHashTable, keyString);
    if (entryPtr) {
	    Tcl_DeleteHashEntry(entryPtr);
    }

    ckfree((char *) documentPtr);
}


/*
 *--------------------------------------------------------------
 *
 * TclDomDeleteDocument
 *
 *	This procedure deletes a DOM document when it is no
 *   longer referenced.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void 
TclDomDeleteDocument(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */  
    TclDomDocument *documentPtr)	/* Document object */
{

    Tcl_EventuallyFree((ClientData) documentPtr, DestroyDocument);
}


/*
 *--------------------------------------------------------------
 *
 * TclDomGetNodeObj
 *
 *	This creates a Tcl_Obj for a token by which a node may 
 *	be referenced at the Tcl level. Tokens are created on 
 *	demand, either when returned from Tcl commands, or when 
 *	needed to populate global Tcl variables corresponding to 
 *	NodeLists or NodeNameMaps.
 *
 * Returns:
 *	Tcl_Obj corresponding to the node token.
 *
 * Side effects:
 *	An entry is added to a hash table maintained per
 *	interpreter, if it doesn't already exist for this node.
 *
 *--------------------------------------------------------------
 */

Tcl_Obj * 
TclDomGetNodeObj(
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomNode *nodePtr)		/* Node object */
{
    char workString[64];
    int newFlag;
    Tcl_HashEntry *entryPtr;
    
    if (nodePtr != NULL) {
	    sprintf(workString, "node%u", nodePtr->nodeId);
	    if (nodePtr->entryPtr == NULL) {
	        entryPtr = Tcl_CreateHashEntry(&interpDataPtr->nodeHashTable, 
					workString, &newFlag);
	        Tcl_SetHashValue(entryPtr, nodePtr);
	        nodePtr->entryPtr = entryPtr;
	    }
    } else {
        *workString = 0;
    }

    return Tcl_NewStringObj(workString, -1);
}


/*
 *--------------------------------------------------------------
 *
 * TclDomSetNodeResult
 *
 *	This procedure sets the interpreter's result to
 *	the token value for a node.
 *
 * Results:
 *	Return TCL_OK, or TCL_ERROR if an internal error occurs.
 *
 * Side effects:
 *	May cause a new token to be created.
 *
 *--------------------------------------------------------------
 */

int 
TclDomSetNodeResult(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomNode *nodePtr)		/* Node object */
{
    Tcl_Obj *objPtr = TclDomGetNodeObj(interpDataPtr, nodePtr);
    Tcl_SetObjResult(interp, objPtr);
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomSetDomError
 *
 *	This procedure sets the interpreter's result to
 *	a DOM error string.
 *
 * Results:
 *  None
 *
 * Side effects:
 *  The interpreter's result is updated.
 *
 *--------------------------------------------------------------
 */

void
TclDomSetDomError(
    Tcl_Interp *interp,			/* Tcl interpreter */
	TdpDomError domError)		/* The DOM error to report */
{
	static char *domErrorString[] = {
		"",
		INDEX_SIZE_ERR_TEXT,
		DOMSTRING_SIZE_ERR_TEXT,
		HIERARCHY_REQUEST_ERR_TEXT,
		WRONG_DOCUMENT_ERR_TEXT,
		INVALID_CHARACTER_ERR_TEXT,
		NO_DATA_ALLOWED_ERR_TEXT,
		NO_MODIFICATION_ALLOWED_ERR_TEXT,
		NOT_FOUND_ERR_TEXT,
		NOT_SUPPORTED_ERR_TEXT,
		INUSE_ATTRIBUTE_ERR_TEXT,
	};

	Tcl_AppendResult(interp, domErrorString[domError], (char *) NULL);
}



/*
 *--------------------------------------------------------------
 *
 * TclDomDeleteNode
 *
 *	This procedure deletes a node and its children.
 *	NodeList and NodeNameMap Tcl global variables are
 *	currently just dereferenced; is this the correct
 *	behavior?
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Releases memory and deletes hash tables.
 *
 *--------------------------------------------------------------
 */

void 
TclDomDeleteNode(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomNode *nodePtr)		/* Node object */
{
    TclDomNode *childPtr;
    TclDomNode *tempNodePtr;
    TclDomAttributeNode *attributeNodePtr;

    if (nodePtr->nodeType == ELEMENT_NODE
	        || nodePtr->nodeType == DOCUMENT_FRAGMENT_NODE 
	        || nodePtr->nodeType == DOCUMENT_NODE) {
	    childPtr = nodePtr->lastChildPtr;
	    while (childPtr) {
	        tempNodePtr = childPtr->previousSiblingPtr;
	        TclDomDeleteNode(interp, interpDataPtr, childPtr);
	        childPtr = tempNodePtr;
	    }
	    attributeNodePtr = nodePtr->firstAttributePtr;
	    while (attributeNodePtr) {
	        tempNodePtr = (TclDomNode *) attributeNodePtr->nextSiblingPtr;
	        TclDomDeleteNode(interp, interpDataPtr, 
					(TclDomNode *) attributeNodePtr);
	        attributeNodePtr = (TclDomAttributeNode *) tempNodePtr;
	    }
	    if (nodePtr->childNodeListVarName) {
	        Tcl_DecrRefCount(nodePtr->childNodeListVarName);
	    }
    } else if (nodePtr->nodeType == DOCUMENT_TYPE_NODE) {
	TclDomDocTypeNode *docTypePtr = (TclDomDocTypeNode *)nodePtr;
	if (docTypePtr->systemId) {
	    ckfree(docTypePtr->systemId);
	}
	if (docTypePtr->publicId) {
	    ckfree(docTypePtr->publicId);
	}
	if (docTypePtr->internalSubset) {
	    ckfree(docTypePtr->internalSubset);
	}
    } 

    if (nodePtr->nodeValue) {
	    ckfree(nodePtr->nodeValue);
    } 

    if (nodePtr->nodeName) {
	    ckfree(nodePtr->nodeName);
    }

    if (nodePtr->entryPtr) {
	    Tcl_DeleteHashEntry(nodePtr->entryPtr);
    }

    ckfree((char *) nodePtr);
}


/*
 *--------------------------------------------------------------
 *
 * TclDomEmptyDocument
 *
 *	This procedure creates a document with no children.
 *	The node "selfPtr" is a container for the children
 *	of the document.
 *
 * Results:
 *	Returns TCL_OK, or TCL_ERROR if an internal error occurs.
 *
 * Side effects:
 *	A hash table entry is created for the document.
 *
 *--------------------------------------------------------------
 */

TclDomDocument*
TclDomEmptyDocument(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr)	/* Extension state */
{
    TclDomDocument *documentPtr;
    TclDomNode *nodePtr;
    char workString[128];
    int newFlag;
    Tcl_HashEntry *entryPtr;

    documentPtr = (TclDomDocument *) ckalloc(sizeof(TclDomDocument));
    memset(documentPtr, 0, sizeof(TclDomDocument));

    documentPtr->interp = interp;
    documentPtr->interpDataPtr = interpDataPtr;

    /*
     * Create root node 
     */

    nodePtr = (TclDomNode *) ckalloc(sizeof(TclDomNode));
    memset(nodePtr, 0, sizeof(TclDomNode));
    nodePtr->nodeType = DOCUMENT_NODE;
    nodePtr->nodeId = ++interpDataPtr->nodeSeed;
    nodePtr->nodeComplete = 1;

    nodePtr->nodeName = ckalloc(1);
    nodePtr->nodeName[0] = 0;
    nodePtr->containingDocumentPtr = documentPtr;

    documentPtr->selfPtr = nodePtr;

    /*
     * Save the root object so we can delete documents on an error exit
     */

    sprintf(workString, "node%u", documentPtr->selfPtr->nodeId);
    entryPtr = Tcl_CreateHashEntry(&interpDataPtr->documentHashTable, 
			workString, &newFlag);
    if (entryPtr == NULL) {
	    Tcl_AppendResult(interp, "couldn't create documentElement", 
				(char *) NULL);
        ckfree((char *) nodePtr);
        ckfree((char *) documentPtr);
	    return NULL;
    }
    Tcl_SetHashValue(entryPtr, documentPtr);
    return documentPtr;   
}


/*
 *--------------------------------------------------------------
 *
 * TclDomCreateEmptyDocumentNode
 *
 *	This procedure creates an empty document and allocates
 *   a token for it.
 *
 * Results:
 *	Returns TCL_OK, or TCL_ERROR if an internal error occurs.
 *
 * Side effects:
 *	A token is allocated for the document.
 *
 *--------------------------------------------------------------
 */

int 
TclDomCreateEmptyDocumentNode(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr)	/* Extension state */
{
    TclDomDocument *documentPtr;

    documentPtr = TclDomEmptyDocument(interp, interpDataPtr);
    if (documentPtr && documentPtr->selfPtr) {
        return TclDomSetNodeResult(interp, interpDataPtr, documentPtr->selfPtr);
    } else {
        return TCL_ERROR;
    }
}

static void
SwapShort(unsigned short *p)
{
    unsigned short low, high;
    low = *p & 0xff;
    high = *p & 0xff00;
    *p = (low << 8) + (high >> 8);
}


/*
 *--------------------------------------------------------------
 *
 * TclDomReadDocument
 *
 *	This procedure creates a new document from XML source.
 *	An empty document is created, and then the expat
 *	parser is invoked to populate it from the source.
 *
 * Results:
 *	TCL_OK, or TCL_ERROR if the XML is not well-formed.
 *	If parsing succeeds, the interpreter's result is set
 *	to a token for the new document; otherwise the result
 *	is set to error information as returned by expat.
 *
 * Side effects:
 *	A document tree and associated data structures are created.
 *
 *--------------------------------------------------------------
 */
#define TCLDOM_ERR_WINDOW 8		/* size of xml fragment displayed
					 * as error */

int
TclDomReadDocument(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state */
    char *xmlSource,			/* XML source in UTF-8 */
    int length,				/* Length of XML source in bytes */
    int final,			/* If true, this is the last chunk. */
    int trim)			/* If true, then eliminate null text nodes. */
{
    TclDomDocument *documentPtr;
    TclDomNode *selfPtr;
    char workString[128];
    int newFlag;
    Tcl_HashEntry *entryPtr;

    if (interpDataPtr->parser == NULL) {
	    unsigned short *encodingPtr = (unsigned short *) xmlSource;

	    documentPtr = (TclDomDocument *) ckalloc(sizeof(TclDomDocument));
	    memset(documentPtr, 0, sizeof(TclDomDocument));

        documentPtr->interp = interp;
        documentPtr->interpDataPtr = interpDataPtr;

	    /*
	     * Determine the document encoding; information needed to
	     * determine if we can display additional error text, and
	     * how to decode XML.
	     */

	    if (*encodingPtr == 0xfeff) {
	        documentPtr->encoding = UTF16;
	    } else if (*encodingPtr == 0xfffe) {
	        documentPtr->encoding = UTF16SWAPPED;
	    } else if (length > 1 && *xmlSource == '<' && *(xmlSource+1) == '?') {
	        documentPtr->encoding = UTF8;
	    } else {
	        documentPtr->encoding = OTHER;
	    }

	    selfPtr = (TclDomNode *) ckalloc(sizeof(TclDomNode));
	    memset(selfPtr, 0, sizeof(TclDomNode));
	    selfPtr->nodeType = DOCUMENT_NODE;
	    selfPtr->nodeId = ++interpDataPtr->nodeSeed;

	    selfPtr->nodeName = ckalloc(1);
	    selfPtr->nodeName[0] = 0;
	    selfPtr->containingDocumentPtr = documentPtr;
	    documentPtr->selfPtr = selfPtr;

	    /*
	     * Save the root object so we can delete documents on an error exit
	     */

	    sprintf(workString, "node%u", documentPtr->selfPtr->nodeId);
	    entryPtr = Tcl_CreateHashEntry(&interpDataPtr->documentHashTable, 
				workString, &newFlag);
	    if (entryPtr == NULL) {
	        Tcl_AppendResult(interp, "couldn't create documentElement", 
					(char *) NULL);
	        return TCL_ERROR;
	    }
	    Tcl_SetHashValue(entryPtr, documentPtr);

	    interpDataPtr->parser = XML_ParserCreate(NULL);

	    memset(&interpDataPtr->parserInfo, 0, sizeof(TclDomExpatInfo));

	    interpDataPtr->parserInfo.documentPtr = documentPtr;
	    interpDataPtr->parserInfo.parser = interpDataPtr->parser;
	    interpDataPtr->parserInfo.interpDataPtr = interpDataPtr;
	    interpDataPtr->parserInfo.interp = interp;
	    interpDataPtr->parserInfo.trim = trim;
 
	    /*
	     * Turn on external parameter entity parsing so we can retrieve
	     * the systemId and publicId from the DOCTYPE declaration.
	     */

	    XML_SetParamEntityParsing(interpDataPtr->parser,
		    XML_PARAM_ENTITY_PARSING_ALWAYS);

	    XML_SetElementHandler(interpDataPtr->parser,
		        TclDomExpatElementStartHandler,
		TclDomExpatElementEndHandler);
	    XML_SetCharacterDataHandler(interpDataPtr->parser,
		        TclDomExpatCharacterDataHandler);
	    XML_SetProcessingInstructionHandler(interpDataPtr->parser,
		    TclDomExpatProcessingInstructionHandler);
	    XML_SetDefaultHandler(interpDataPtr->parser,
		        TclDomExpatDefaultHandler);

	    XML_SetDoctypeDeclHandler(interpDataPtr->parser,
		    TclDomExpatStartDoctypeDeclHandler, 
		    TclDomExpatEndDoctypeDeclHandler);

	    XML_SetUnparsedEntityDeclHandler(interpDataPtr->parser,
		        TclDomExpatUnparsedDeclHandler);
	    XML_SetNotationDeclHandler(interpDataPtr->parser,
		        TclDomExpatNotationDeclHandler);
	    XML_SetExternalEntityRefHandler(interpDataPtr->parser,
		        TclDomExpatExternalEntityRefHandler);
	    XML_SetUnknownEncodingHandler(interpDataPtr->parser,
		        TclDomExpatUnknownEncodingHandler,
		        (void *) &interpDataPtr->parserInfo);
  
	    XML_SetCommentHandler(interpDataPtr->parser,
		        TclDomExpatCommentHandler);
  
	    /* Tell expat to use the TclDomExpat "not standalone" handler */
	    XML_SetNotStandaloneHandler(interpDataPtr->parser,
		        TclDomExpatNotStandaloneHandler);
  
	    /* Tell expat to use the TclDomExpat CdataSection handlers */
	    XML_SetCdataSectionHandler(interpDataPtr->parser,
		    TclDomExpatStartCdataSectionHandler,
		    TclDomExpatEndCdataSectionHandler);

	    XML_SetUserData(interpDataPtr->parser, &interpDataPtr->parserInfo);
    }

    if (!XML_Parse(interpDataPtr->parser, xmlSource, length, final)) {
	    int byteIndex;
	    TclDomDocumentEncoding encoding;

	    documentPtr = interpDataPtr->parserInfo.documentPtr;
	    encoding = documentPtr->encoding;

	    if (documentPtr) {
	        TclDomDeleteDocument(interp, interpDataPtr, documentPtr);
	    }

	    Tcl_ResetResult(interp);
	    sprintf(workString, "%d", 
                XML_GetCurrentLineNumber(interpDataPtr->parser));
	            Tcl_AppendResult(interp, "error \"",
		XML_ErrorString(XML_GetErrorCode(interpDataPtr->parser)),
		        "\" at line ", workString, " character ", NULL);
	    sprintf(workString, "%d",
		        XML_GetCurrentColumnNumber(interpDataPtr->parser));
	    Tcl_AppendResult(interp, workString, (char *) NULL);
	    byteIndex = XML_GetCurrentByteIndex(interpDataPtr->parser);
	    if ((encoding != OTHER) && (byteIndex >= 0 && byteIndex < length)) {
	        char errorString[4 * TCLDOM_ERR_WINDOW * TCL_UTF_MAX];
	        char contextString[4 * TCLDOM_ERR_WINDOW * TCL_UTF_MAX];
	        int i, len, contextChars, charPos, contextPos;
	        int filterOK = 0;
		const char *s;

	        charPos = XML_GetCurrentColumnNumber(interpDataPtr->parser);

	        /*
	         * Generate a string for that includes the error character plus
	         * surrounding text.  Filter out CR & LF unless the character
	         * generating the error was a CF or LF.
	         */

	        if (encoding != UTF8) {
		        char *utf;
		        unsigned short *xmlSourceWS = (unsigned short *) xmlSource;
		        unsigned short tempUTF16;

		    /*
		     * Back up to beginning of window
		     */

		    contextPos = byteIndex / 2;
		    contextChars = TCLDOM_ERR_WINDOW;

		    for (i = 0; i < TCLDOM_ERR_WINDOW; contextPos--) {
		        if (contextPos == 1) break;
		        tempUTF16= *(xmlSourceWS + contextPos);
		        if (encoding == UTF16SWAPPED) {
			        SwapShort(&tempUTF16);
		        }
		        Tcl_UniCharToUtf(tempUTF16, contextString);
		        if ((i != 0) && (*contextString == '\r'
			        || *contextString == '\n')) {
			        continue;
		        }
		        i++;
		        contextChars++;
		    }

		    /*
		     * Transfer to utf-8 buffer, filtering out CR & LF
		     * We replace the first CR or LF after the error postion
		     * with a space, and skip over any successive ones.
		     */
		    for (utf = contextString, i = 0; i < contextChars;
		            contextPos++) {
		        if ((2 * contextPos) >= length) break;
		        tempUTF16 = *(xmlSourceWS + contextPos);
		        if (encoding == UTF16SWAPPED) {
			        SwapShort(&tempUTF16);
		        }
		        len = Tcl_UniCharToUtf(tempUTF16, utf);
		        if (*utf == '\r' || *utf == '\n') {
			        if (filterOK || (contextPos < byteIndex / 2)) {
			            utf += len;
			            continue;
			        }
			        if (contextPos == byteIndex / 2) {
			            *utf = ' ';
			        }
			        filterOK = 1;
		        }
		        utf += len;
		        i++;
		    }
		    *utf = 0;
	    } else {
		    const char *s, *next;
		    char *d;
		    int j;
		    int goodIndex;
		    int index;
		    int len;

		    /*
		     * This requires that we back up over UTF-8 characters
		     */

		    s = xmlSource + byteIndex;

		    index = goodIndex = byteIndex;

		    contextChars = TCLDOM_ERR_WINDOW;

		    for (i = 0; i < TCLDOM_ERR_WINDOW; ) {
		        if (index == 0) break;
		        for (j = 0; j < TCL_UTF_MAX; j++) {
			        index--;
			        if (Tcl_UtfCharComplete(xmlSource+index, j+1)) break;
		        }
		        if (!Tcl_UtfCharComplete(xmlSource+index, j+1)) {
			        break;
		        }
		        goodIndex = index;
		        if ((*(xmlSource+index) != '\r') 
						&& (*(xmlSource+index) != '\n')) {
			        contextChars++;
			        i++;
		        }
		    }

		    contextPos = goodIndex;

		    s = xmlSource + contextPos;
		    d = contextString;

		    /*
		     * Transfer to buffer, removing any CR or LF chars
		     */
		    for (i = 0; i < contextChars ;) {
		        if (s >= (xmlSource + length)) break;
		        if (*s == '\r' || *s == '\n') {
			        if (filterOK || (s < (xmlSource + byteIndex))) {
			            s++;
			            continue;
			        }
			        if (s == xmlSource + byteIndex) {
			            *d = ' ';
			            filterOK = 1;
			            s++;
			            d++;
			            i++;
			            continue;
			        }
			        filterOK = 1;
		        }
		        next = Tcl_UtfNext(s);
		        len = next-s;
		        memcpy(d, s, len);
		        d += len;
		        s = next;
		        i++;
		    }
		    *d = 0;
	    }
	    
	    /*
	     * Generate a buffer containing the code where the error occurred
	     */

	    if (encoding != UTF8) {
		    char *utf;
		    int errorPos;
		    unsigned short *xmlSourceWS = (unsigned short *) xmlSource;

		    errorPos = byteIndex / 2;
		    if (errorPos == 0) {
		        errorPos++;
		    }

		    /*
		     * Transfer to utf-8 buffer
		     */
		    if (encoding == UTF16SWAPPED) {
		        unsigned short temp;
		        for (utf = errorString, i = 0; i < TCLDOM_ERR_WINDOW; i++) {
			        temp = *(xmlSourceWS + errorPos + i);
			        SwapShort(&temp);
			        if ((2 * (errorPos + i)) >= length) break;
			        utf += Tcl_UniCharToUtf(temp, utf);
		        }
		    } else {
		        for (utf = errorString, i = 0; i < TCLDOM_ERR_WINDOW; i++) {
			        if ((2 * (errorPos + i)) >= length) break;
			        utf += Tcl_UniCharToUtf(*(xmlSourceWS + errorPos + i), utf);
		        }
		    }
		    *utf = 0;		
	    } else {
		    int sourceLength;
		    sourceLength = TCLDOM_ERR_WINDOW * TCL_UTF_MAX;
		    if ((byteIndex + sourceLength) > length) {
		        sourceLength = length - byteIndex;
		    }

		    memcpy(errorString, xmlSource+byteIndex, sourceLength);
		    errorString[sourceLength] = 0;
	    } 

	    /*
	     * Ignore any characters after the first CR or LF that is not the 
	     * error character
	     */

	    for (s = errorString, i = 0; *s && (i < TCLDOM_ERR_WINDOW); i++) {
		    if ((i != 0) && ((*s == '\r' || *s == '\n'))) break;
		    s = Tcl_UtfNext(s);
	        }

		errorString[s-errorString] = 0;
	        if (strcmp(errorString, contextString) == 0) {
		        Tcl_AppendResult(interp, "; at \"", errorString, 
					    "\"", (char *) NULL);
	        } else {
		        Tcl_AppendResult(interp, "; at \"", errorString, 
						"\" within \"", contextString, "\"", (char *) NULL);
	        }
	    }
	    XML_ParserFree(interpDataPtr->parser);
	    interpDataPtr->parser = NULL;
	    memset(&interpDataPtr->parserInfo, 0, sizeof(TclDomExpatInfo));
	    return TCL_ERROR;
    }

    documentPtr = interpDataPtr->parserInfo.documentPtr;

    if (final) {
	    documentPtr->selfPtr->nodeComplete = 1;
	    XML_ParserFree(interpDataPtr->parser);
	    interpDataPtr->parser = NULL;
	    memset(&interpDataPtr->parserInfo, 0, sizeof(TclDomExpatInfo));
    } 

    return TclDomSetNodeResult(interp, interpDataPtr, documentPtr->selfPtr);
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDomExpatElementStartHandler --
 *
 *	    Called by expat for each start tag.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    A node is added to the current tree.
 *
 *----------------------------------------------------------------------------
 */

static void
TclDomExpatElementStartHandler(
    void *userData,		    /* Our context for parser */
    const char *name,		    /* Element name */
    const char **atts)		    /* Array of name, value pairs */
{
    TclDomExpatInfo *infoPtr = (TclDomExpatInfo *) userData;
    TclDomNode *nodePtr;
    TclDomNode *parentNodePtr;
    const char **attributePtr;
    TclDomAttributeNode *attributeNodePtr;

    /*
     * Invoke the default handler to get the current width
     */

    XML_DefaultCurrent(infoPtr->parser);


    nodePtr = (TclDomNode *) ckalloc(sizeof(TclDomNode));
    memset(nodePtr, 0, sizeof(TclDomNode));
    nodePtr->nodeType = ELEMENT_NODE;
    nodePtr->nodeId = ++infoPtr->interpDataPtr->nodeSeed;

   /*  node->nodeName =	 XXX */
    nodePtr->nodeName = ckalloc(strlen(name) + 1);
    strcpy(nodePtr->nodeName, name);
    nodePtr->containingDocumentPtr = infoPtr->documentPtr;

    if (infoPtr->depth == 0) {
	    parentNodePtr = infoPtr->documentPtr->selfPtr;
    } else {
	    parentNodePtr = infoPtr->currentNodePtr;
    }
    nodePtr->parentNodePtr = parentNodePtr;
    if (parentNodePtr->firstChildPtr) {
	    parentNodePtr->lastChildPtr->nextSiblingPtr = nodePtr;
	    nodePtr->previousSiblingPtr = parentNodePtr->lastChildPtr;
	    parentNodePtr->lastChildPtr = nodePtr;
    } else {
	    parentNodePtr->firstChildPtr = parentNodePtr->lastChildPtr = nodePtr;
    }

    infoPtr->currentNodePtr = nodePtr;
    nodePtr->startLine = XML_GetCurrentLineNumber(infoPtr->parser);
    nodePtr->startColumn = XML_GetCurrentColumnNumber(infoPtr->parser);
    nodePtr->startWidth = infoPtr->currentWidth;

    /*
     * Add the attribute nodes
     */

    for (attributePtr = atts; attributePtr[0] && attributePtr[1]; 
			attributePtr += 2) {
	    /* XXX add attribute to hash table */
	    attributeNodePtr = (TclDomAttributeNode *) 
				ckalloc(sizeof(TclDomAttributeNode));
	    memset(attributeNodePtr, 0, sizeof(TclDomAttributeNode));
	    attributeNodePtr->nodeType = ATTRIBUTE_NODE;
	    attributeNodePtr->containingDocumentPtr = 
				nodePtr->containingDocumentPtr;
	    attributeNodePtr->nodeName = ckalloc(strlen(attributePtr[0]) + 1);
	    strcpy(attributeNodePtr->nodeName, attributePtr[0]);
	    attributeNodePtr->parentNodePtr = nodePtr;
	    attributeNodePtr->valueLength = strlen((char *) attributePtr[1]);
	    attributeNodePtr->nodeValue = (char *) 
				ckalloc(attributeNodePtr->valueLength+1);
	    strcpy(attributeNodePtr->nodeValue, (char *) attributePtr[1]);

	    if (nodePtr->firstAttributePtr) {
	        nodePtr->lastAttributePtr->nextSiblingPtr = attributeNodePtr;
	        nodePtr->lastAttributePtr = attributeNodePtr;
	    } else {
	        nodePtr->firstAttributePtr = nodePtr->lastAttributePtr = 
					attributeNodePtr;
	    }
    }


    infoPtr->depth++;
    return;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDomExpatElementEndHandler --
 *
 *	    Called by expat for each end tag.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    Position information is updated for the Element node.
 *
 *----------------------------------------------------------------------------
 */

static void
TclDomExpatElementEndHandler(
    void *userData,		/* Our context for parser */
    CONST char *name)		/* Element name */
{
    TclDomExpatInfo *infoPtr = (TclDomExpatInfo *) userData;
    TclDomNode *nodePtr;

    /*
     * Invoke the default handler to get the current width
     */

    XML_DefaultCurrent(infoPtr->parser);

    nodePtr = infoPtr->currentNodePtr;
    nodePtr->endLine = XML_GetCurrentLineNumber(infoPtr->parser);
    nodePtr->endColumn = XML_GetCurrentColumnNumber(infoPtr->parser);
    nodePtr->endWidth = infoPtr->currentWidth;
    nodePtr->nodeComplete = 1;

    /*
     * Now trim any empty text nodes if necessary.
     */

    if (infoPtr->trim) {
	    int empty;
	    char *p, *last;
	    Tcl_UniChar ch;
	    TclDomNode *childPtr, *nextSiblingPtr;

        for (childPtr = nodePtr->firstChildPtr; childPtr != NULL;
	            childPtr = nextSiblingPtr) {
            nextSiblingPtr = childPtr->nextSiblingPtr;
	        if (childPtr->nodeType == TEXT_NODE) {
		        empty = 1;
		        p = childPtr->nodeValue;
		        last = p + childPtr->valueLength;
		        while (p < last) {
		            p += Tcl_UtfToUniChar(p, &ch);
		            if (!Tcl_UniCharIsSpace(ch)) {
			            empty = 0;
			            break;
                    }
                }
		        if (empty) {
		            UnlinkChild(infoPtr->interpDataPtr, childPtr);
		            TclDomDeleteNode(NULL, infoPtr->interpDataPtr, childPtr);
                }
            }
        }
    }

    infoPtr->depth--;
    if (infoPtr->depth != 0) {
	    infoPtr->currentNodePtr = infoPtr->currentNodePtr->parentNodePtr;
    }
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDomExpatCharacterDataHandler --
 *	Called by expat for character data.
 *
 * Side Effects:
 *	If the current node is a TEXT or CDATA_SECTION node,
 *	appends character data to the current node.
 *	Otherwise, begins a new TEXT node.
 *
 *----------------------------------------------------------------------------
 */

static void
TclDomExpatCharacterDataHandler(			       
     void *userData,		    /* Our context for parser */
     CONST char *s,		    /* Character text */
     int len)			    /* Text length in bytes */
{
    TclDomExpatInfo *infoPtr = (TclDomExpatInfo *) userData;
    TclDomNode *parentNodePtr;
    TclDomTextNode *nodePtr;

    parentNodePtr = infoPtr->currentNodePtr;

    if (parentNodePtr->lastChildPtr 
        && (parentNodePtr->lastChildPtr->nodeType == TEXT_NODE
            || (parentNodePtr->lastChildPtr->nodeType == CDATA_SECTION_NODE
	         && !parentNodePtr->lastChildPtr->nodeComplete))) 
    {
	    /* 
	     * Combine with sibling TEXT or CDATA_SECTION node
	     */
	    nodePtr = (TclDomTextNode *) parentNodePtr->lastChildPtr;
	    nodePtr->nodeValue = Tcl_Realloc(nodePtr->nodeValue,
		nodePtr->valueLength + len + 1);
	    memmove(nodePtr->nodeValue + nodePtr->valueLength, s, len);
	    nodePtr->valueLength += len;
	    nodePtr->nodeValue[nodePtr->valueLength] = 0;
	    nodePtr->startWidth = Tcl_NumUtfChars(nodePtr->nodeValue,
		nodePtr->valueLength);
    } else {
	/* 
	 * Create a new TEXT node.
	 */
    	nodePtr = (TclDomTextNode *) ckalloc(sizeof(TclDomTextNode));
    	memset(nodePtr, 0, sizeof(TclDomTextNode));
    	nodePtr->nodeType = TEXT_NODE;
    	nodePtr->nodeId = ++infoPtr->interpDataPtr->nodeSeed;
    	nodePtr->valueLength = len;
    	nodePtr->nodeValue = (char *) ckalloc(len + 1);
    	memmove(nodePtr->nodeValue, s, len);
    	nodePtr->nodeValue[len] = 0;

    	nodePtr->containingDocumentPtr = infoPtr->documentPtr;
    	nodePtr->parentNodePtr = parentNodePtr;

    	nodePtr->startLine = XML_GetCurrentLineNumber(infoPtr->parser);
    	nodePtr->startColumn = XML_GetCurrentColumnNumber(infoPtr->parser);
    	nodePtr->startWidth = Tcl_NumUtfChars(s, len);

    	if (parentNodePtr->nodeType == ELEMENT_NODE) {
	        if (parentNodePtr->firstChildPtr) {
		        parentNodePtr->lastChildPtr->nextSiblingPtr
		                = (TclDomNode *) nodePtr;
		        nodePtr->previousSiblingPtr = parentNodePtr->lastChildPtr;
		        parentNodePtr->lastChildPtr = (TclDomNode *) nodePtr;
	        } else {
		        parentNodePtr->firstChildPtr
		                = parentNodePtr->lastChildPtr = (TclDomNode *) nodePtr;
	        }
	}
	nodePtr->nodeComplete = 1;
    }
    return;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDomExpatProcessingInstructionHandler --
 *
 *	    Called by expat for processing instructions.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    A node is added to the tree.
 *
 *----------------------------------------------------------------------------
 */

static void
TclDomExpatProcessingInstructionHandler(
     void *userData,		/* Our context for parse */
     CONST char *target,	/* Target name */
     CONST char *data)		/* Node data */
{
    TclDomExpatInfo *infoPtr	= (TclDomExpatInfo*)userData;
    TclDomNode *nodePtr		= (TclDomNode*)ckalloc(sizeof(TclDomNode));
    TclDomNode *parentNodePtr	= infoPtr->currentNodePtr
    				? infoPtr->currentNodePtr 
				: infoPtr->documentPtr->selfPtr
				;

    memset(nodePtr, 0, sizeof(TclDomNode));

    nodePtr->nodeType = PROCESSING_INSTRUCTION_NODE;
    nodePtr->containingDocumentPtr = infoPtr->documentPtr;
    nodePtr->nodeId = ++infoPtr->interpDataPtr->nodeSeed;

    nodePtr->nodeName = ckalloc(strlen(target) + 1);
    strcpy(nodePtr->nodeName, target);

    nodePtr->valueLength = strlen(data);
    nodePtr->nodeValue = ckalloc(nodePtr->valueLength + 1);
    strcpy(nodePtr->nodeValue, data);


    nodePtr->startLine = XML_GetCurrentLineNumber(infoPtr->parser);
    nodePtr->startColumn = XML_GetCurrentColumnNumber(infoPtr->parser);
    /* ??? nodePtr->startWidth = ??? */

    nodePtr->parentNodePtr = parentNodePtr;
    if (parentNodePtr->firstChildPtr) {
	parentNodePtr->lastChildPtr->nextSiblingPtr = nodePtr;
	nodePtr->previousSiblingPtr = parentNodePtr->lastChildPtr;
	parentNodePtr->lastChildPtr = nodePtr;
    } else {
	parentNodePtr->firstChildPtr = parentNodePtr->lastChildPtr = nodePtr;
    }

    nodePtr->nodeComplete = 1;
    return;
}


/*
 *----------------------------------------------------------------------------
 *
 * ParseXMLDecl --
 *
 *	    This procedure parses an XMLDecl[23] string. We're assuming that it's
 *	been through xpat and is well-formed.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	May add a node to the tree.
 * 
 *
 *----------------------------------------------------------------------------
 */
void 
ParseXMLDecl(
    TclDomNode *nodePtr,    /* The document node */
    CONST char *s,	    /* The XMLDecl text */
    int len)		    /* Length of text */
{
    int c = *s;
    TclDomAttributeNode *attributeNodePtr = NULL;
    enum parseState {
	    UNDEFINED, VERSION_INFO, ENCODING_DECL, SD_DECL, VALUE
    };
    enum parseState state = UNDEFINED;

    while (len) {
	    c = *s;
	    if (c == ' ' || c == '\t' || c == '\n' || c == '\r'
		        || c == '=') {
	        s++;
	        len--;
	        continue;
	    }
	    if (strncmp(s, "<?xml", 5) == 0) {
	        s += 5;
	        len -= 5;
	        continue;
	    }
	    if (strncmp(s, "version", 6) == 0) {
	        s += 7;
	        len -= 7;
	        state = VERSION_INFO;
	    } else if (strncmp(s, "encoding", 8) == 0) {
	        s += 8;
	        len -= 8;
	        state = ENCODING_DECL;
	    } else if (strncmp(s, "standalone", 10) == 0) {
	        s += 10;
	        len -= 10;
	        state = SD_DECL;
	    } else if (c == '\'' || c == '\"') {
	        int count;
	        char *endChar;
	        s++;
	        len--;
	        count = 0;
	        endChar = (char *) s;
	        while (count < len) {
		        if (*endChar == c) break;
		        endChar++;
		        count++;
	        }
	        if (*endChar != c) {
		        /*
		         * expat should really never get us to this state
		         * Just skip over this attribute
		         */
		        if (attributeNodePtr) {
		            if (attributeNodePtr->nodeName) {
			            ckfree(attributeNodePtr->nodeName);
		            }
		            ckfree((char *) attributeNodePtr);
		            attributeNodePtr = NULL;
		        continue;
		        }
	        }
	        if (attributeNodePtr) {
		        attributeNodePtr->valueLength = count;
		        attributeNodePtr->nodeValue = (char *) ckalloc(count+1);
		        memcpy(attributeNodePtr->nodeValue, s, count);
		        attributeNodePtr->nodeValue[count] = 0;
		        if (nodePtr->firstAttributePtr) {
		            nodePtr->lastAttributePtr->nextSiblingPtr = 
							attributeNodePtr;
		            nodePtr->lastAttributePtr = attributeNodePtr;
		        } else {
		            nodePtr->firstAttributePtr = nodePtr->lastAttributePtr = 
							attributeNodePtr;
		        }
		        attributeNodePtr = NULL;
	        }
	        len -= (count+1);
	        s += (count+1);
	        continue;
	    }

	    if (state == VERSION_INFO || state == ENCODING_DECL 
				|| state == SD_DECL) {
	        attributeNodePtr = (TclDomAttributeNode *) 
					ckalloc(sizeof(TclDomAttributeNode));
	        memset(attributeNodePtr, 0, sizeof(TclDomAttributeNode));
	        attributeNodePtr->nodeType = ATTRIBUTE_NODE;
	        attributeNodePtr->containingDocumentPtr = 
					nodePtr->containingDocumentPtr;
	        if (state == VERSION_INFO) {
		        attributeNodePtr->nodeName = ckalloc(8);
		        strcpy(attributeNodePtr->nodeName, "version");
	        } else if (state == ENCODING_DECL) {
		        attributeNodePtr->nodeName = ckalloc(9);
		        strcpy(attributeNodePtr->nodeName, "encoding");
	        } else {
		        attributeNodePtr->nodeName = ckalloc(11);
		        strcpy(attributeNodePtr->nodeName, "standalone");
	        }
	        attributeNodePtr->parentNodePtr = nodePtr;
	        state = VALUE;
	    }
	    len--;
	    s++;
    }
}


/*
 *----------------------------------------------------------------------------
 *
 * TclDomExpatDefaultHandler --
 *
 *	    Called by expat for processing data which has no other handler.
 *	If the node is an XMLDecl[23], then we save the version info, 
 *	etc. If the node is a doctypedecl[28], then we do likewise.
 *	Otherwise, this handler has been explicitly invoked by an 
 *	Element, Text, etc., handler, and we compute width information
 *	for the node.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	May add a node to the tree.
 * 
 *
 *----------------------------------------------------------------------------
 */

static void
TclDomExpatDefaultHandler(
     void *userData,	    /* Our context for parser */
     CONST char *s,	    /* String value of token */
     int len)		    /* String length */
{
    TclDomExpatInfo *infoPtr = (TclDomExpatInfo *) userData;

    if (strncmp("<?xml", s, 5) == 0) {
	    ParseXMLDecl(infoPtr->documentPtr->selfPtr, s, len);
    }

    /*
     * Set the width information
     */
    infoPtr->currentWidth = Tcl_NumUtfChars(s, len);
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * TclDomExpatStartDoctypeDeclHandler --
 *
 *	Called by expat to process the doctype declaration.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the doctype.
 *
 *----------------------------------------------------------------------
 */

static void
TclDomExpatStartDoctypeDeclHandler(
    void *userData,		 /* Our context for parser. */
    const XML_Char *doctypeName, /* The root element of the document. */
    const XML_Char *sysid,	 /* SYSTEM identifier */
    const XML_Char *pubid,	 /* PUBLIC identifier */
    int has_internal_subset)
{
    TclDomExpatInfo *infoPtr = (TclDomExpatInfo *) userData;
    TclDomNode *parentNodePtr;
    TclDomDocTypeNode *nodePtr;

    if (infoPtr->currentNodePtr) {
	parentNodePtr = infoPtr->currentNodePtr;
    } else {
	parentNodePtr = infoPtr->documentPtr->selfPtr;
    }

    nodePtr = (TclDomDocTypeNode *) ckalloc(sizeof(TclDomDocTypeNode));
    memset(nodePtr, 0, sizeof(TclDomDocTypeNode));
    nodePtr->nodeType = DOCUMENT_TYPE_NODE;
    nodePtr->nodeId = ++infoPtr->interpDataPtr->nodeSeed;

    nodePtr->containingDocumentPtr = infoPtr->documentPtr;
    nodePtr->parentNodePtr = parentNodePtr;

    infoPtr->currentNodePtr = (TclDomNode *) nodePtr;
    nodePtr->startLine = XML_GetCurrentLineNumber(infoPtr->parser);
    nodePtr->startColumn = XML_GetCurrentColumnNumber(infoPtr->parser);

    nodePtr->nodeName = ckalloc(strlen(doctypeName)+1);
    strcpy(nodePtr->nodeName, doctypeName);

    nodePtr->systemId = nodePtr->publicId = nodePtr->internalSubset = NULL;
    if (sysid) {
	nodePtr->systemId = ckalloc(strlen(sysid)+1);
	strcpy(nodePtr->systemId, sysid);
    }
    if (pubid) {
	nodePtr->publicId = ckalloc(strlen(pubid)+1);
	strcpy(nodePtr->publicId, pubid);
    }

    if (parentNodePtr->firstChildPtr) {
	parentNodePtr->lastChildPtr->nextSiblingPtr = (TclDomNode *) nodePtr;
	nodePtr->previousSiblingPtr = parentNodePtr->lastChildPtr;
	parentNodePtr->lastChildPtr = (TclDomNode *) nodePtr;
    } else {
	parentNodePtr->firstChildPtr
	    = parentNodePtr->lastChildPtr = (TclDomNode *) nodePtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclDomExpatEndDoctypeDeclHandler --
 *
 *	Called by expat to process the end of the doctype declaration.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the doctype.
 *
 *----------------------------------------------------------------------
 */

static void
TclDomExpatEndDoctypeDeclHandler(
    void *userData)		/* Our context for parser */
{
    TclDomExpatInfo *infoPtr = (TclDomExpatInfo *) userData;
    TclDomNode *nodePtr;

    nodePtr = infoPtr->currentNodePtr;
    nodePtr->endLine = XML_GetCurrentLineNumber(infoPtr->parser);
    nodePtr->endColumn = XML_GetCurrentColumnNumber(infoPtr->parser);
    nodePtr->nodeComplete = 1;
    infoPtr->currentNodePtr = NULL;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDomExpatUnparsedDeclHandler --
 *
 *	    Called by expat for processing an unparsed entity references.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *
 *	Currently none.	     
 *
 *----------------------------------------------------------------------------
 */

static void
TclDomExpatUnparsedDeclHandler(
     void *userData,		/* Our context for parser */
     CONST char *entityname,	/* Name of entity */
     CONST char *base,		/* Base string	*/
     CONST char *systemId,	/* System id string */
     CONST char *publicId,	/* Public id string */
     CONST char *notationName)	/* Notation name string */
{
    TclDomExpatInfo *expat = (TclDomExpatInfo *) userData;
    return;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDomExpatNotationDeclHandler --
 *
 *	    Called by expat for processing a notation declaration.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    Currently none.
 *
 *----------------------------------------------------------------------------
 */

static void
TclDomExpatNotationDeclHandler(
     void *userData,		    /* Our context for parser */
     CONST char *notationName,	    /* Notation name string */
     CONST char *base,		    /* Base string */
     CONST char *systemId,	    /* System Id string */
     CONST char *publicId)	    /* Public id string */
{
    TclDomExpatInfo *expat = (TclDomExpatInfo *) userData;
    return;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDomExpatUnknownEncodingHandler --
 *
 *	    Called by expat for processing a reference to a character in an 
 *	    unknown encoding.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    Currently none.
 *
 *----------------------------------------------------------------------------
 */

static int
TclDomExpatUnknownEncodingHandler(
     void *encodingHandlerData,		/* Our context for parser */
     CONST char *name,			/* Character */
     XML_Encoding *info)		/* Encoding info */
{
    TclDomExpatInfo *expat = (TclDomExpatInfo *) encodingHandlerData;
    return 0;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDomExpatExternalEntityRefHandler --
 *
 *	    Called by expat for processing external entity references.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    Currently none.
 *
 *----------------------------------------------------------------------------
 */

static int
TclDomExpatExternalEntityRefHandler(
     XML_Parser parser,		    /* Our context for parser */
     CONST char *openEntityNames,   /* Open entities */
     CONST char *base,		    /* Base */
     CONST char *systemId,	    /* System id string */
     CONST char *publicId)	    /* Public id string */
{
    TclDomExpatInfo *infoPtr = (TclDomExpatInfo *) XML_GetUserData(parser);
    TclDomDocTypeNode *nodePtr = (TclDomDocTypeNode *) infoPtr->currentNodePtr;

    return 1;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDomExpatCommentHandler --
 *
 *	    Called by expat to handle comments encountered while parsing
 *	Added by ericm@scriptics.com, 1999.6.25.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    A node is added to the tree.
 *
 *----------------------------------------------------------------------------
 */
static void
TclDomExpatCommentHandler(
    void *userData,		    /* Our context for parser */
    const char *data)		    /* Comment string */
{
    TclDomExpatInfo *infoPtr = (TclDomExpatInfo *) userData;
    TclDomNode *parentNodePtr;
    TclDomTextNode *nodePtr;
    int len = strlen(data);

    /*
     * Invoke the default handler to get the current width
     */

    XML_DefaultCurrent(infoPtr->parser);

    if (infoPtr->currentNodePtr) {
	    parentNodePtr = infoPtr->currentNodePtr;
    } else {
	    parentNodePtr = infoPtr->documentPtr->selfPtr;
    }
   
    nodePtr = (TclDomTextNode *) ckalloc(sizeof(TclDomTextNode));
    memset(nodePtr, 0, sizeof(TclDomTextNode));
    nodePtr->nodeType = COMMENT_NODE;
    nodePtr->nodeId = ++infoPtr->interpDataPtr->nodeSeed;
    nodePtr->valueLength = len;
    nodePtr->nodeValue = (char *) ckalloc(len + 1);
    memmove(nodePtr->nodeValue, data, len);
    nodePtr->nodeValue[len] = 0;

    nodePtr->containingDocumentPtr = infoPtr->documentPtr;
    nodePtr->parentNodePtr = parentNodePtr;
    nodePtr->startLine = nodePtr->endLine = 
			XML_GetCurrentLineNumber(infoPtr->parser);
    nodePtr->startColumn = nodePtr->endLine = 
			XML_GetCurrentColumnNumber(infoPtr->parser);
    nodePtr->startWidth = nodePtr->endWidth = infoPtr->currentWidth;
    nodePtr->nodeComplete = 1;

    if (parentNodePtr->nodeType == ELEMENT_NODE 
	        || parentNodePtr->nodeType == DOCUMENT_NODE
	        || parentNodePtr->nodeType == DOCUMENT_FRAGMENT_NODE
	        || parentNodePtr->nodeType == ENTITY_REFERENCE_NODE
	        || parentNodePtr->nodeType == ENTITY_NODE) {
	    if (parentNodePtr->firstChildPtr) {
	        parentNodePtr->lastChildPtr->nextSiblingPtr = 
					(TclDomNode *) nodePtr;
	        nodePtr->previousSiblingPtr = parentNodePtr->lastChildPtr;
	        parentNodePtr->lastChildPtr = (TclDomNode *) nodePtr;
	    } else {
	        parentNodePtr->firstChildPtr = parentNodePtr->lastChildPtr = 
					(TclDomNode *) nodePtr;
	    }
    } else {
    }
    return;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDomExpatNotStandaloneHandler --
 *
 *	    Called by expat to handle "not standalone" documents (ie, documents
 *	that have an external subset or a reference to a parameter entity, 
 *	but do not have standalone="yes")
 *	Added by ericm@scriptics.com, 1999.6.25.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    Currently none.
 *
 *----------------------------------------------------------------------------
 */
static int
TclDomExpatNotStandaloneHandler(
    void *userData)		    /* Data */
{
    TclDomExpatInfo *infoPtr = (TclDomExpatInfo *) userData;
    return 1;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDomExpatStartCdataSectionHandler --
 *
 *	Called by expat to handle CDATA section starts.
 *	Added by ericm@scriptics.com, 1999.6.25.
 *
 * Side Effects:
 *	Begins a new CDATA_SECTION node.
 *
 *----------------------------------------------------------------------------
 */
static void
TclDomExpatStartCdataSectionHandler(void *userData)			
{
    TclDomExpatInfo *infoPtr = (TclDomExpatInfo *) userData;
    TclDomNode      *parentNodePtr;
    TclDomTextNode  *nodePtr;

    parentNodePtr = infoPtr->currentNodePtr;
    
    /* Allocate a new text node */
    nodePtr = (TclDomTextNode *) ckalloc(sizeof(TclDomTextNode));
    memset(nodePtr, 0, sizeof(TclDomTextNode));
    nodePtr->nodeType = CDATA_SECTION_NODE;
    nodePtr->nodeId = ++infoPtr->interpDataPtr->nodeSeed;

    /* configure for our tree */
    nodePtr->containingDocumentPtr = infoPtr->documentPtr;
    nodePtr->parentNodePtr = parentNodePtr;
    nodePtr->startLine = XML_GetCurrentLineNumber(infoPtr->parser);
    nodePtr->startColumn = XML_GetCurrentColumnNumber(infoPtr->parser);

    /* insert into the tree */
    if (parentNodePtr->nodeType == ELEMENT_NODE) {
	if (parentNodePtr->firstChildPtr) {
	    parentNodePtr->lastChildPtr->nextSiblingPtr
		    = (TclDomNode *) nodePtr;
	    nodePtr->previousSiblingPtr = parentNodePtr->lastChildPtr;
	    parentNodePtr->lastChildPtr = (TclDomNode *) nodePtr;
	} else {
	    parentNodePtr->firstChildPtr
		    = parentNodePtr->lastChildPtr = (TclDomNode *) nodePtr;
	}
    } else {
    }
}


/*
 *----------------------------------------------------------------------------
 *
 * TclDomExpatEndCdataSectionHandler
 *	Called by expat to handle CDATA section ends
 *
 * Side Effects:
 *	Finishes the current CDATA_SECTION_NODE.
 *
 *----------------------------------------------------------------------------
 */
static void
TclDomExpatEndCdataSectionHandler(void *userData)
{
    TclDomExpatInfo *infoPtr = (TclDomExpatInfo *) userData;
    TclDomNode *parentNodePtr = infoPtr->currentNodePtr;
    TclDomNode *nodePtr = parentNodePtr->lastChildPtr;

    nodePtr->endLine = XML_GetCurrentLineNumber(infoPtr->parser);
    nodePtr->endColumn = XML_GetCurrentColumnNumber(infoPtr->parser);
    nodePtr->nodeComplete = 1;
    return;
}


/*
 *----------------------------------------------------------------------------
 *
 * SerializeDocument
 *
 *	    This procedure serializes a Document node. 
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    None.
 *
 *----------------------------------------------------------------------------
 */

static void 
SerializeDocument(
    TclDomNode *nodePtr,	    /* Node to be serialized */
    Tcl_DString *output)	    /* Output string to append to */
{
    TclDomNode *childPtr;
    TclDomAttributeNode *attributeNodePtr;

    if (nodePtr->firstAttributePtr) {
	    Tcl_DStringAppend(output, "<?xml", 5);
	    for (attributeNodePtr = nodePtr->firstAttributePtr; attributeNodePtr; 
		        attributeNodePtr = attributeNodePtr->nextSiblingPtr) {
	        SerializeAttribute(attributeNodePtr, output);
	    }
	    Tcl_DStringAppend(output, "?>", 2);
    } else {
	    Tcl_DStringAppend(output, "<?xml version='1.0'?>", -1);
    }
    Tcl_DStringAppend(output, "\n", 1);


    if (TclDomGetDoctypeNode(nodePtr->containingDocumentPtr) == NULL) {
	/*
	 * Fabricate docType from first element 
	 */
	TclDomNode *documentElementPtr 
		= TclDomGetDocumentElement(nodePtr->containingDocumentPtr);
	if (documentElementPtr && documentElementPtr->nodeName) {
	    Tcl_DStringAppend(output, "<!DOCTYPE ", -1);
	    Tcl_DStringAppend(output, documentElementPtr->nodeName, -1);
	    Tcl_DStringAppend(output, ">", 1);
	}
	Tcl_DStringAppend(output, "\n", 1);
    }

    for (childPtr = nodePtr->firstChildPtr; childPtr; 
			childPtr = childPtr->nextSiblingPtr) {
	    SerializeWalk(childPtr, output);
    }
}


/*
 *----------------------------------------------------------------------------
 *
 * SerializeElement
 *
 *	    This procedure serializes an Element node. 
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    None.
 *
 *----------------------------------------------------------------------------
 */

static void 
SerializeElement(
    TclDomNode *nodePtr,	    /* Node to be serialized */
    Tcl_DString *output)	    /* Output string to append to */
{
    TclDomNode *childPtr;
    TclDomAttributeNode *attributeNodePtr;
    int isDocumentElement = nodePtr->parentNodePtr != NULL 
			&& nodePtr->parentNodePtr->nodeType == DOCUMENT_NODE;

    if (nodePtr->firstChildPtr || isDocumentElement)
    {
	    Tcl_DStringAppend(output, "<", 1);
	    Tcl_DStringAppend(output, nodePtr->nodeName, -1);
	    for (attributeNodePtr = nodePtr->firstAttributePtr; attributeNodePtr; 
		        attributeNodePtr = attributeNodePtr->nextSiblingPtr) {
	        SerializeAttribute(attributeNodePtr, output);
	    }
	    Tcl_DStringAppend(output, ">", 1);
	    for (childPtr = nodePtr->firstChildPtr; childPtr; 
				childPtr = childPtr->nextSiblingPtr) {
	        SerializeWalk(childPtr, output);
	    } 
	    Tcl_DStringAppend(output, "</", 2);
	    Tcl_DStringAppend(output, nodePtr->nodeName, -1);
	    Tcl_DStringAppend(output, ">", 1);
    } else {
	    Tcl_DStringAppend(output, "<", 1);
	    Tcl_DStringAppend(output, nodePtr->nodeName, -1);
	    for (attributeNodePtr = nodePtr->firstAttributePtr; attributeNodePtr; 
		        attributeNodePtr = attributeNodePtr->nextSiblingPtr) {
	        SerializeAttribute(attributeNodePtr, output);
	    }
	    Tcl_DStringAppend(output, "/>", 2);
    }
}


/*
 *----------------------------------------------------------------------------
 *
 * EscapeText --
 *	Helper function for SerializeAttribute() and SerializeText().
 *	Appends text to output DString buffer, replacing XML markup
 *	characters '<', '&', and '>' with appropriate entity references.
 *
 *	If the 'escapeAll' flag is set, also replaces ' and ".
 *
 * BUGS:
 *	This is not UNICODE-aware.
 *
 *----------------------------------------------------------------------------
 */

static void
EscapeText(Tcl_DString *output, TclDomString s, int escapeAll)
{
    char *escapeChars = escapeAll ? "<>&\"'" : "<>&";

    while (*s) {
    	char *t = strpbrk(s, escapeChars);
	if (!t) { /* No escapable characters left */
	    Tcl_DStringAppend(output,s,-1);
	    break;
	}
	if (t > s)
	    Tcl_DStringAppend(output,s,t-s);
	switch (*t) {
	    case '<' : Tcl_DStringAppend(output, "&lt;", -1); break;
	    case '>' : Tcl_DStringAppend(output, "&gt;", -1); break;
	    case '&' : Tcl_DStringAppend(output, "&amp;", -1); break;
	    case '"' : Tcl_DStringAppend(output, "&quot;", -1); break;
	    case '\'': Tcl_DStringAppend(output, "&apos;", -1); break;
	    default:   Tcl_DStringAppend(output, t, 1);
	}
	s = t+1;
    }
}


/*
 *----------------------------------------------------------------------------
 *
 * SerializeAttribute
 *	This procedure serializes an attribute node. 
 *
 * Side Effects:
 *	Appends an attribute value specification " attname = 'attval'"
 *	to the output buffer.
 *
 *----------------------------------------------------------------------------
 */

static void 
SerializeAttribute(
    TclDomAttributeNode *attributeNodePtr,	/* Node to be serialized */
    Tcl_DString *output)			/* Output string to append to */
{
    Tcl_DStringAppend(output, " ", 1);
    Tcl_DStringAppend(output, attributeNodePtr->nodeName, -1);
    Tcl_DStringAppend(output, "=\'", 2);
    EscapeText(output, attributeNodePtr->nodeValue, 1);
    Tcl_DStringAppend(output, "\'", 1);
}


/*
 *----------------------------------------------------------------------------
 *
 * SerializeText
 *	This procedure serializes a Text node.
 *
 * Side Effects:
 *	Appends character data to the output buffer.
 *	with XML markup characters replaced by entity references.
 *
 *----------------------------------------------------------------------------
 */

static void 
SerializeText(
    TclDomNode *nodePtr,			/* Node to be serialized */
    Tcl_DString *output)			/* Output string to append to */
{
    TclDomTextNode *textNodePtr = (TclDomTextNode *) nodePtr;
    EscapeText(output, textNodePtr->nodeValue, 0);
}


/*
 *----------------------------------------------------------------------------
 *
 * SerializeComment
 *
 *	    This procedure serializes a Comment node.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    None.
 *
 *----------------------------------------------------------------------------
 */

static void 
SerializeComment(
    TclDomNode *nodePtr,			/* Node to be serialized */
    Tcl_DString *output)			/* Output string to append to */
{
    TclDomTextNode *commentNodePtr = (TclDomTextNode *) nodePtr;

    Tcl_DStringAppend(output, "<!--", 4);
    Tcl_DStringAppend(output, commentNodePtr->nodeValue, -1);
    Tcl_DStringAppend(output, "-->", 3);
}


/*
 *----------------------------------------------------------------------------
 *
 * SerializeProcessingInstruction
 *
 *	    This procedure serializes a ProcessingInstruction node.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    None.
 *
 *----------------------------------------------------------------------------
 */

static void 
SerializeProcessingInstruction(
    TclDomNode *nodePtr,			/* Node to be serialized */
    Tcl_DString *output)			/* Output string to append to */
{
    Tcl_DStringAppend(output, "<?", 2);
    Tcl_DStringAppend(output, nodePtr->nodeName, -1);
    Tcl_DStringAppend(output, " ", 1);
    Tcl_DStringAppend(output, nodePtr->nodeValue, -1);
    Tcl_DStringAppend(output, "?>", 2);
}


/*
 *----------------------------------------------------------------------------
 *
 * SerializeEntity
 *
 *	    This procedure serializes an Entity node.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    None.
 *
 *----------------------------------------------------------------------------
 */

static void 
SerializeEntity(
    TclDomNode *nodePtr,			/* Node to be serialized */
    Tcl_DString *output)			/* Output string to append to */
{
}



/*
 *----------------------------------------------------------------------------
 *
 * SerializeNotation
 *
 *	    This procedure serializes a Notation node.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    None.
 *
 *----------------------------------------------------------------------------
 */

static void 
SerializeNotation(
    TclDomNode *nodePtr,			/* Node to be serialized */
    Tcl_DString *output)			/* Output string to append to */
{
}



/*
 *----------------------------------------------------------------------------
 *
 * SerializeDocumentType
 *
 *	    This procedure serializes a DocumentType node.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    None.
 *
 *----------------------------------------------------------------------------
 */


static void 
SerializeDocumentType(
    TclDomNode *nodePtr,		    /* Node to be serialized */
    Tcl_DString *output)		    /* Output string to append to */
{
    TclDomDocTypeNode *docTypeNodePtr = (TclDomDocTypeNode *) nodePtr;
    Tcl_DStringAppend(output, "<!DOCTYPE", -1);
	if (docTypeNodePtr->nodeName) {
	    Tcl_DStringAppend(output, " ", 1);
	    Tcl_DStringAppend(output, docTypeNodePtr->nodeName, -1);
		if (docTypeNodePtr->publicId && docTypeNodePtr->systemId) {
	    	Tcl_DStringAppend(output, " PUBLIC ", 1);
	    	Tcl_DStringAppend(output, docTypeNodePtr->publicId, -1);
	    	Tcl_DStringAppend(output, " ", 1);
	     	Tcl_DStringAppend(output, docTypeNodePtr->systemId, -1);
		} else if (docTypeNodePtr->systemId) {
	    	Tcl_DStringAppend(output, " SYSTEM ", 1);
	     	Tcl_DStringAppend(output, docTypeNodePtr->systemId, -1);
		}
	} else if (docTypeNodePtr->nodeValue) {
	    int c0 = *docTypeNodePtr->nodeValue;
	    if (c0 != ' ' && c0 != '\t' && c0 != '\n' && c0 != '\r') {
	        Tcl_DStringAppend(output, " ", 1);
	    }
	    Tcl_DStringAppend(output, docTypeNodePtr->nodeValue, -1);
    }
    Tcl_DStringAppend(output, ">\n", 2);
}



/*
 *----------------------------------------------------------------------------
 *
 * SerializeEntityReference
 *
 *	    This procedure serializes an EntityReference node.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    None.
 *
 *----------------------------------------------------------------------------
 */

static void 
SerializeEntityReference(
    TclDomNode *nodePtr,			/* Node to be serialized */
    Tcl_DString *output)			/* Output string to append to */
{
}



/*
 *----------------------------------------------------------------------------
 *
 * SerializeCDATA
 *
 *	    This procedure serializes a CDATA Section node.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    None.
 *
 *----------------------------------------------------------------------------
 */

static void 
SerializeCDATA(
    TclDomNode *nodePtr,		    /* Node to be serialized */
    Tcl_DString *output)		    /* Output string to append to */
{
    Tcl_DStringAppend(output, "<![CDATA[", 9);
    Tcl_DStringAppend(output, nodePtr->nodeValue, -1);
    Tcl_DStringAppend(output, "]]>", 3);
}



/*
 *----------------------------------------------------------------------------
 *
 * SerializeWalk
 *
 *	    This procedure walks the tree to serialize a node.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    None.
 *
 *----------------------------------------------------------------------------
 */

static void 
SerializeWalk(
    TclDomNode *nodePtr,		/* Node to be serialized */
    Tcl_DString *output)		/* Output string to append to */
{
    switch (nodePtr->nodeType) {
	    case ELEMENT_NODE:
	        SerializeElement(nodePtr, output);
	        break;
    
	    case TEXT_NODE:
	        SerializeText(nodePtr, output);
	        break;

	    case CDATA_SECTION_NODE:
	        SerializeCDATA(nodePtr, output);
	        break;

	    case ENTITY_REFERENCE_NODE:
	        SerializeEntityReference(nodePtr, output);
	        break;

	    case ENTITY_NODE:
	        SerializeEntity(nodePtr, output);
	        break;

	    case PROCESSING_INSTRUCTION_NODE:
	        SerializeProcessingInstruction(nodePtr, output);
	        break;

	    case COMMENT_NODE:
	        SerializeComment(nodePtr, output);
	        break;

	    case DOCUMENT_NODE:
	        SerializeDocument(nodePtr, output);
	        break;

	    case DOCUMENT_TYPE_NODE:
	        SerializeDocumentType(nodePtr, output);
	        break;

	    case DOCUMENT_FRAGMENT_NODE:
	        /*
	         * Shouldn't occur in a document
	         */
	        break;

	    case NOTATION_NODE:
	        SerializeNotation(nodePtr, output);
	        break;

	    default:
	        break;
    }
}



/*
 *----------------------------------------------------------------------------
 *
 * TclDomSerialize
 *
 *	    This procedure serializes a node and returns the
 *	XML to the interpreter's output.
 *
 * Results:
 *	    None.
 *
 * Side Effects:
 *	    None.
 *
 *----------------------------------------------------------------------------
 */

int 
TclDomSerialize(
    Tcl_Interp *interp,		    /* Tcl intepreter to output to */
    TclDomNode *nodePtr)    /* Document to serialize */
{
    Tcl_DString output;

    if (nodePtr->nodeType == DOCUMENT_NODE 
	&& TclDomGetDocumentElement(nodePtr->containingDocumentPtr) == NULL) {
	    Tcl_AppendResult(interp, "document has no document element", 
				(char *) NULL);
	    return TCL_ERROR;
    }

    Tcl_DStringInit(&output);

    SerializeWalk(nodePtr, &output);

    Tcl_DStringResult(interp, &output);
    return TCL_OK;
}

int TclDomHasChildren(TclDomNode *nodePtr)
{
    int hasChildren = ((nodePtr->nodeType == ELEMENT_NODE
		    || nodePtr->nodeType == DOCUMENT_NODE)
            && (nodePtr->firstChildPtr != NULL));
    return hasChildren;
}


