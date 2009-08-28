/*----------------------------------------------------------------------------
|   Copyright (c) 2000 Jochen Loewer (loewerj@hotmail.com)
|-----------------------------------------------------------------------------
|
|   $Id: domxslt.h,v 1.6 2004/08/14 14:42:27 rolf Exp $
|
|
|   A (partial) XSLT implementation for tDOM, according to the W3C
|   recommendation (16 Nov 1999,
|   http://www.w3.org/TR/1999/REC-xslt-19991116).
|
|
|   The contents of this file are subject to the Mozilla Public License
|   Version 1.1 (the "License"); you may not use this file except in
|   compliance with the License. You may obtain a copy of the License at
|   http://www.mozilla.org/MPL/
|
|   Software distributed under the License is distributed on an "AS IS"
|   basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
|   License for the specific language governing rights and limitations
|   under the License.
|
|   The Original Code is tDOM.
|
|   The Initial Developer of the Original Code is Jochen Loewer
|   Portions created by Jochen Loewer are Copyright (C) 1998, 1999
|   Jochen Loewer. All Rights Reserved.
|
|   Contributor(s):
|
|
|   $Log: domxslt.h,v $
|   Revision 1.6  2004/08/14 14:42:27  rolf
|   Use 'Id' cvs keyword (instead of 'Header') in the file heads.
|
|   Revision 1.5  2003/01/11 00:19:02  rolf
|   Added conversion of XSLT stylesheet DOM trees to 'cached' xslt cmds
|   (new domDoc method toXSLTcmd). Works for non threaded tcl; needs
|   additional work for multi-threaded tcl and documentation.
|
|   Revision 1.4  2002/12/27 23:40:10  rolf
|   The xslt method now understands also the options
|   -ignoreUndeclaredParameters and -xsltmessagecmd.
|
|   Revision 1.3  2002/06/02 06:36:24  zoran
|   Added thread safety with capability of sharing DOM trees between
|   threads and ability to read/write-lock DOM documents
|
|   Revision 1.2  2002/04/08 02:11:12  rolf
|   Added -parameters option to domNode xslt method, to enable setting of
|   top level parameters from tcl level.
|
|   Revision 1.1.1.1  2002/02/22 01:05:35  rolf
|   tDOM0.7test with Jochens first set of patches
|
|   Revision 1.1  2002/02/04 08:08:19  jolo
|   Initial revision
|
|
|
|   written by Jochen Loewer
|   June, 2000
|
\---------------------------------------------------------------------------*/

#ifndef __DOMXSLT_H__
#define __DOMXSLT_H__

#include <dom.h>
#include <domxpath.h>


typedef void (*xsltMsgCB) (void *clientData, char *str, 
                          int length, int terminate);

/*----------------------------------------------------------------------------
|   Prototypes
|
\---------------------------------------------------------------------------*/
int xsltProcess (domDocument       * xsltDoc,
                 domNode           * xmlNode,
                 void              * xsltCmdData,
                 char             ** parameters,
                 int                 ignoreUndeclaredParameters,
                 xpathFuncCallback   funcCB,
                 void              * xpathFuncClientData,
                 xsltMsgCB           xsltMsgCB,
                 void              * xsltMsgClientData,
                 char             ** errMsg,
                 domDocument      ** resultDoc
                );

void * xsltCompileStylesheet (
    domDocument       * xsltDoc,
    xpathFuncCallback   funcCB,
    void              * xpathFuncClientData,
    int                 guardXSLTTree,
    char             ** errMsg
    );

void xsltFreeStateWrapper (void *clientData);

void sortByDocOrder (xpathResultSet *rs);

#endif

