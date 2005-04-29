/* attrask.c -- Interrogate attribute type

  (c) 1998-2003 (W3C) MIT, ERCIM, Keio University
  See tidy.h for the copyright notice.
  
  CVS Info:
    $Author: rbraun $ 
    $Date: 2004/05/04 20:05:14 $ 
    $Revision: 1.1.1.1 $ 

*/

#include "tidy-int.h"
#include "tidy.h"
#include "attrs.h"

Bool tidyAttrIsHREF( TidyAttr tattr )
{
    return attrIsHREF( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsSRC( TidyAttr tattr )
{
    return attrIsSRC( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsID( TidyAttr tattr )
{
    return attrIsID( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsNAME( TidyAttr tattr )
{
    return attrIsNAME( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsSUMMARY( TidyAttr tattr )
{
    return attrIsSUMMARY( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsALT( TidyAttr tattr )
{
    return attrIsALT( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsLONGDESC( TidyAttr tattr )
{
    return attrIsLONGDESC( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsUSEMAP( TidyAttr tattr )
{
    return attrIsUSEMAP( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsISMAP( TidyAttr tattr )
{
    return attrIsISMAP( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsLANGUAGE( TidyAttr tattr )
{
    return attrIsLANGUAGE( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsTYPE( TidyAttr tattr )
{
    return attrIsTYPE( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsVALUE( TidyAttr tattr )
{
    return attrIsVALUE( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsCONTENT( TidyAttr tattr )
{
    return attrIsCONTENT( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsTITLE( TidyAttr tattr )
{
    return attrIsTITLE( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsXMLNS( TidyAttr tattr )
{
    return attrIsXMLNS( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsDATAFLD( TidyAttr tattr )
{
    return attrIsDATAFLD( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsWIDTH( TidyAttr tattr )
{
    return attrIsWIDTH( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsHEIGHT( TidyAttr tattr )
{
    return attrIsHEIGHT( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsFOR( TidyAttr tattr )
{
    return attrIsFOR( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsSELECTED( TidyAttr tattr )
{
    return attrIsSELECTED( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsCHECKED( TidyAttr tattr )
{
    return attrIsCHECKED( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsLANG( TidyAttr tattr )
{
    return attrIsLANG( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsTARGET( TidyAttr tattr )
{
    return attrIsTARGET( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsHTTP_EQUIV( TidyAttr tattr )
{
    return attrIsHTTP_EQUIV( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsREL( TidyAttr tattr )
{
    return attrIsREL( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsEvent( TidyAttr tattr )
{
    return attrIsEvent( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsOnMOUSEMOVE( TidyAttr tattr )
{
    return attrIsOnMOUSEMOVE( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsOnMOUSEDOWN( TidyAttr tattr )
{
    return attrIsOnMOUSEDOWN( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsOnMOUSEUP( TidyAttr tattr )
{
    return attrIsOnMOUSEUP( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsOnCLICK( TidyAttr tattr )
{
    return attrIsOnCLICK( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsOnMOUSEOVER( TidyAttr tattr )
{
    return attrIsOnMOUSEOVER( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsOnMOUSEOUT( TidyAttr tattr )
{
    return attrIsOnMOUSEOUT( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsOnKEYDOWN( TidyAttr tattr )
{
    return attrIsOnKEYDOWN( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsOnKEYUP( TidyAttr tattr )
{
    return attrIsOnKEYUP( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsOnKEYPRESS( TidyAttr tattr )
{
    return attrIsOnKEYPRESS( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsOnFOCUS( TidyAttr tattr )
{
    return attrIsOnFOCUS( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsOnBLUR( TidyAttr tattr )
{
    return attrIsOnBLUR( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsBGCOLOR( TidyAttr tattr )
{
    return attrIsBGCOLOR( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsLINK( TidyAttr tattr )
{
    return attrIsLINK( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsALINK( TidyAttr tattr )
{
    return attrIsALINK( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsVLINK( TidyAttr tattr )
{
    return attrIsVLINK( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsTEXT( TidyAttr tattr )
{
    return attrIsTEXT( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsSTYLE( TidyAttr tattr )
{
    return attrIsSTYLE( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsABBR( TidyAttr tattr )
{
    return attrIsABBR( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsCOLSPAN( TidyAttr tattr )
{
    return attrIsCOLSPAN( tidyAttrToImpl(tattr) );
}
Bool tidyAttrIsROWSPAN( TidyAttr tattr )
{
    return attrIsROWSPAN( tidyAttrToImpl(tattr) );
}
