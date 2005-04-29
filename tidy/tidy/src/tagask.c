/* tagask.c -- Interrogate node type

  (c) 1998-2004 (W3C) MIT, ERCIM, Keio University
  See tidy.h for the copyright notice.

  CVS Info :

    $Author: rbraun $ 
    $Date: 2004/05/04 20:05:14 $ 
    $Revision: 1.1.1.1 $ 

*/

#include "tidy-int.h"
#include "tags.h"
#include "tidy.h"

Bool tidyNodeIsText( TidyNode tnod )
{ return nodeIsText( tidyNodeToImpl(tnod) );
}
Bool tidyNodeCMIsBlock( TidyNode tnod ); /* not exported yet */
Bool tidyNodeCMIsBlock( TidyNode tnod )
{ return nodeCMIsBlock( tidyNodeToImpl(tnod) );
}
Bool tidyNodeCMIsInline( TidyNode tnod ); /* not exported yet */
Bool tidyNodeCMIsInline( TidyNode tnod )
{ return nodeCMIsInline( tidyNodeToImpl(tnod) );
}
Bool tidyNodeCMIsEmpty( TidyNode tnod ); /* not exported yet */
Bool tidyNodeCMIsEmpty( TidyNode tnod )
{ return nodeCMIsEmpty( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsHeader( TidyNode tnod )
{ return nodeIsHeader( tidyNodeToImpl(tnod) );
}

Bool tidyNodeIsHTML( TidyNode tnod )
{ return nodeIsHTML( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsHEAD( TidyNode tnod )
{ return nodeIsHEAD( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsTITLE( TidyNode tnod )
{ return nodeIsTITLE( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsBASE( TidyNode tnod )
{ return nodeIsBASE( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsMETA( TidyNode tnod )
{ return nodeIsMETA( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsBODY( TidyNode tnod )
{ return nodeIsBODY( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsFRAMESET( TidyNode tnod )
{ return nodeIsFRAMESET( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsFRAME( TidyNode tnod )
{ return nodeIsFRAME( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsIFRAME( TidyNode tnod )
{ return nodeIsIFRAME( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsNOFRAMES( TidyNode tnod )
{ return nodeIsNOFRAMES( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsHR( TidyNode tnod )
{ return nodeIsHR( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsH1( TidyNode tnod )
{ return nodeIsH1( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsH2( TidyNode tnod )
{ return nodeIsH2( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsPRE( TidyNode tnod )
{ return nodeIsPRE( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsLISTING( TidyNode tnod )
{ return nodeIsLISTING( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsP( TidyNode tnod )
{ return nodeIsP( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsUL( TidyNode tnod )
{ return nodeIsUL( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsOL( TidyNode tnod )
{ return nodeIsOL( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsDL( TidyNode tnod )
{ return nodeIsDL( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsDIR( TidyNode tnod )
{ return nodeIsDIR( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsLI( TidyNode tnod )
{ return nodeIsLI( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsDT( TidyNode tnod )
{ return nodeIsDT( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsDD( TidyNode tnod )
{ return nodeIsDD( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsTABLE( TidyNode tnod )
{ return nodeIsTABLE( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsCAPTION( TidyNode tnod )
{ return nodeIsCAPTION( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsTD( TidyNode tnod )
{ return nodeIsTD( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsTH( TidyNode tnod )
{ return nodeIsTH( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsTR( TidyNode tnod )
{ return nodeIsTR( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsCOL( TidyNode tnod )
{ return nodeIsCOL( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsCOLGROUP( TidyNode tnod )
{ return nodeIsCOLGROUP( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsBR( TidyNode tnod )
{ return nodeIsBR( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsA( TidyNode tnod )
{ return nodeIsA( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsLINK( TidyNode tnod )
{ return nodeIsLINK( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsB( TidyNode tnod )
{ return nodeIsB( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsI( TidyNode tnod )
{ return nodeIsI( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsSTRONG( TidyNode tnod )
{ return nodeIsSTRONG( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsEM( TidyNode tnod )
{ return nodeIsEM( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsBIG( TidyNode tnod )
{ return nodeIsBIG( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsSMALL( TidyNode tnod )
{ return nodeIsSMALL( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsPARAM( TidyNode tnod )
{ return nodeIsPARAM( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsOPTION( TidyNode tnod )
{ return nodeIsOPTION( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsOPTGROUP( TidyNode tnod )
{ return nodeIsOPTGROUP( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsIMG( TidyNode tnod )
{ return nodeIsIMG( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsMAP( TidyNode tnod )
{ return nodeIsMAP( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsAREA( TidyNode tnod )
{ return nodeIsAREA( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsNOBR( TidyNode tnod )
{ return nodeIsNOBR( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsWBR( TidyNode tnod )
{ return nodeIsWBR( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsFONT( TidyNode tnod )
{ return nodeIsFONT( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsLAYER( TidyNode tnod )
{ return nodeIsLAYER( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsSPACER( TidyNode tnod )
{ return nodeIsSPACER( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsCENTER( TidyNode tnod )
{ return nodeIsCENTER( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsSTYLE( TidyNode tnod )
{ return nodeIsSTYLE( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsSCRIPT( TidyNode tnod )
{ return nodeIsSCRIPT( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsNOSCRIPT( TidyNode tnod )
{ return nodeIsNOSCRIPT( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsFORM( TidyNode tnod )
{ return nodeIsFORM( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsTEXTAREA( TidyNode tnod )
{ return nodeIsTEXTAREA( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsBLOCKQUOTE( TidyNode tnod )
{ return nodeIsBLOCKQUOTE( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsAPPLET( TidyNode tnod )
{ return nodeIsAPPLET( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsOBJECT( TidyNode tnod )
{ return nodeIsOBJECT( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsDIV( TidyNode tnod )
{ return nodeIsDIV( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsSPAN( TidyNode tnod )
{ return nodeIsSPAN( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsINPUT( TidyNode tnod )
{ return nodeIsINPUT( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsQ( TidyNode tnod )
{ return nodeIsQ( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsLABEL( TidyNode tnod )
{ return nodeIsLABEL( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsH3( TidyNode tnod )
{ return nodeIsH3( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsH4( TidyNode tnod )
{ return nodeIsH4( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsH5( TidyNode tnod )
{ return nodeIsH5( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsH6( TidyNode tnod )
{ return nodeIsH6( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsADDRESS( TidyNode tnod )
{ return nodeIsADDRESS( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsXMP( TidyNode tnod )
{ return nodeIsXMP( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsSELECT( TidyNode tnod )
{ return nodeIsSELECT( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsBLINK( TidyNode tnod )
{ return nodeIsBLINK( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsMARQUEE( TidyNode tnod )
{ return nodeIsMARQUEE( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsEMBED( TidyNode tnod )
{ return nodeIsEMBED( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsBASEFONT( TidyNode tnod )
{ return nodeIsBASEFONT( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsISINDEX( TidyNode tnod )
{ return nodeIsISINDEX( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsS( TidyNode tnod )
{ return nodeIsS( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsSTRIKE( TidyNode tnod )
{ return nodeIsSTRIKE( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsU( TidyNode tnod )
{ return nodeIsU( tidyNodeToImpl(tnod) );
}
Bool tidyNodeIsMENU( TidyNode tnod )
{ return nodeIsMENU( tidyNodeToImpl(tnod) );
}

