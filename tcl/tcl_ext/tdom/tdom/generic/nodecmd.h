/*----------------------------------------------------------------------------
|   Copyright (C) 1999  Jochen C. Loewer (loewerj@hotmail.com)
+-----------------------------------------------------------------------------
|
|   $Id: nodecmd.h,v 1.6 2005/01/07 15:09:40 rolf Exp $

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
|
|   Portions created by Jochen Loewer are Copyright (C) 1998, 1999
|   Jochen Loewer. All Rights Reserved.
|
|   Portions created by Zoran Vasiljevic are Copyright (C) 2000-2002
|   Zoran Vasiljevic. All Rights Reserved.
|
|   Portions created by Rolf Ade are Copyright (C) 1999-2002
|   Rolf Ade. All Rights Reserved.
|
|   Written by Zoran Vasiljevic
|   July 12, 2000
|
\---------------------------------------------------------------------------*/

int nodecmd_createNodeCmd (Tcl_Interp    * interp,
                           int             objc,
                           Tcl_Obj *CONST  objv[],
                           int             checkName,
                           int             checkCharData);

int nodecmd_appendFromScript (Tcl_Interp *interp, 
                              domNode    *node,
                              Tcl_Obj    *cmdObj);

int nodecmd_insertBeforeFromScript (Tcl_Interp *interp, 
                                    domNode    *node,
                                    Tcl_Obj    *cmdObj,
                                    domNode    *refChild);

/* EOF $RCSfile $ */

/* Emacs Setup Variables */
/* Local Variables:      */
/* mode: C               */
/* indent-tabs-mode: nil */
/* c-basic-offset: 4     */
/* End:                  */

