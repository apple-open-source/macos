/* attrget.c -- Locate attribute value by type

  (c) 1998-2003 (W3C) MIT, ERCIM, Keio University
  See tidy.h for the copyright notice.
  
  CVS Info:
    $Author: rbraun $ 
    $Date: 2004/05/04 20:05:14 $ 
    $Revision: 1.1.1.1 $ 

*/

#include "tidy-int.h"
#include "tags.h"
#include "attrs.h"
#include "tidy.h"

TidyAttr tidyAttrGetHREF( TidyNode tnod )
{
    return tidyImplToAttr( attrGetHREF( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetSRC( TidyNode tnod )
{
    return tidyImplToAttr( attrGetSRC( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetID( TidyNode tnod )
{
    return tidyImplToAttr( attrGetID( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetNAME( TidyNode tnod )
{
    return tidyImplToAttr( attrGetNAME( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetSUMMARY( TidyNode tnod )
{
    return tidyImplToAttr( attrGetSUMMARY( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetALT( TidyNode tnod )
{
    return tidyImplToAttr( attrGetALT( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetLONGDESC( TidyNode tnod )
{
    return tidyImplToAttr( attrGetLONGDESC( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetUSEMAP( TidyNode tnod )
{
    return tidyImplToAttr( attrGetUSEMAP( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetISMAP( TidyNode tnod )
{
    return tidyImplToAttr( attrGetISMAP( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetLANGUAGE( TidyNode tnod )
{
    return tidyImplToAttr( attrGetLANGUAGE( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetTYPE( TidyNode tnod )
{
    return tidyImplToAttr( attrGetTYPE( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetVALUE( TidyNode tnod )
{
    return tidyImplToAttr( attrGetVALUE( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetCONTENT( TidyNode tnod )
{
    return tidyImplToAttr( attrGetCONTENT( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetTITLE( TidyNode tnod )
{
    return tidyImplToAttr( attrGetTITLE( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetXMLNS( TidyNode tnod )
{
    return tidyImplToAttr( attrGetXMLNS( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetDATAFLD( TidyNode tnod )
{
    return tidyImplToAttr( attrGetDATAFLD( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetWIDTH( TidyNode tnod )
{
    return tidyImplToAttr( attrGetWIDTH( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetHEIGHT( TidyNode tnod )
{
    return tidyImplToAttr( attrGetHEIGHT( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetFOR( TidyNode tnod )
{
    return tidyImplToAttr( attrGetFOR( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetSELECTED( TidyNode tnod )
{
    return tidyImplToAttr( attrGetSELECTED( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetCHECKED( TidyNode tnod )
{
    return tidyImplToAttr( attrGetCHECKED( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetLANG( TidyNode tnod )
{
    return tidyImplToAttr( attrGetLANG( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetTARGET( TidyNode tnod )
{
    return tidyImplToAttr( attrGetTARGET( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetHTTP_EQUIV( TidyNode tnod )
{
    return tidyImplToAttr( attrGetHTTP_EQUIV( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetREL( TidyNode tnod )
{
    return tidyImplToAttr( attrGetREL( tidyNodeToImpl(tnod) ) );
}

TidyAttr tidyAttrGetOnMOUSEMOVE( TidyNode tnod )
{
    return tidyImplToAttr( attrGetOnMOUSEMOVE( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetOnMOUSEDOWN( TidyNode tnod )
{
    return tidyImplToAttr( attrGetOnMOUSEDOWN( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetOnMOUSEUP( TidyNode tnod )
{
    return tidyImplToAttr( attrGetOnMOUSEUP( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetOnCLICK( TidyNode tnod )
{
    return tidyImplToAttr( attrGetOnCLICK( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetOnMOUSEOVER( TidyNode tnod )
{
    return tidyImplToAttr( attrGetOnMOUSEOVER( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetOnMOUSEOUT( TidyNode tnod )
{
    return tidyImplToAttr( attrGetOnMOUSEOUT( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetOnKEYDOWN( TidyNode tnod )
{
    return tidyImplToAttr( attrGetOnKEYDOWN( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetOnKEYUP( TidyNode tnod )
{
    return tidyImplToAttr( attrGetOnKEYUP( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetOnKEYPRESS( TidyNode tnod )
{
    return tidyImplToAttr( attrGetOnKEYPRESS( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetOnFOCUS( TidyNode tnod )
{
    return tidyImplToAttr( attrGetOnFOCUS( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetOnBLUR( TidyNode tnod )
{
    return tidyImplToAttr( attrGetOnBLUR( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetBGCOLOR( TidyNode tnod )
{
    return tidyImplToAttr( attrGetBGCOLOR( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetLINK( TidyNode tnod )
{
    return tidyImplToAttr( attrGetLINK( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetALINK( TidyNode tnod )
{
    return tidyImplToAttr( attrGetALINK( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetVLINK( TidyNode tnod )
{
    return tidyImplToAttr( attrGetVLINK( tidyNodeToImpl(tnod) ) );
}

TidyAttr tidyAttrGetTEXT( TidyNode tnod )
{
    return tidyImplToAttr( attrGetTEXT( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetSTYLE( TidyNode tnod )
{
    return tidyImplToAttr( attrGetSTYLE( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetABBR( TidyNode tnod )
{
    return tidyImplToAttr( attrGetABBR( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetCOLSPAN( TidyNode tnod )
{
    return tidyImplToAttr( attrGetCOLSPAN( tidyNodeToImpl(tnod) ) );
}
TidyAttr tidyAttrGetROWSPAN( TidyNode tnod )
{
    return tidyImplToAttr( attrGetROWSPAN( tidyNodeToImpl(tnod) ) );
}
