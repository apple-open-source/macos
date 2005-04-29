/**********************************************************************

                  - TCLODBC COPYRIGHT NOTICE - 

This software is copyrighted by Roy Nurmi, contact address by email at
Roy.Nurmi@iki.fi. The following terms apply to all files associated with 
the software unless explicitly disclaimed in individual files.

The author hereby grant permission to use, copy, modify, distribute,
and license this software and its documentation for any purpose, provided
that existing copyright notices are retained in all copies and that this
notice is included verbatim in any distributions. No written agreement,
license, or royalty fee is required for any of the authorized uses.
Modifications to this software may be copyrighted by their authors
and need not follow the licensing terms described here, provided that
the new terms are clearly indicated on the first page of each file where
they apply.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
MODIFICATIONS.

GOVERNMENT USE: If you are acquiring this software on behalf of the
U.S. government, the Government shall have only "Restricted Rights"
in the software and related documentation as defined in the Federal 
Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
are acquiring the software on behalf of the Department of Defense, the
software shall be classified as "Commercial Computer Software" and the
Government shall have only "Restricted Rights" as defined in Clause
252.227-7013 (c) (1) of DFARs.  Notwithstanding the foregoing, the

  authors grant the U.S. Government and others acting in its behalf
permission to use and distribute the software in accordance with the
terms specified in this license. 

***********************************************************************/

#include <string.h>
#include <tcl.h>

#if TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION >= 1

void FreeTclodbcEncodedString(Tcl_Obj *p) {
    //extern Tcl_ObjType TclodbcEncodedString;
    Tcl_DString* pds = (Tcl_DString*) p->internalRep.twoPtrValue.ptr2;
    if (pds) {
        Tcl_DStringFree(pds);
        Tcl_Free((char*)pds);
    }
    if (p->internalRep.twoPtrValue.ptr1)
        Tcl_FreeEncoding((Tcl_Encoding) p->internalRep.twoPtrValue.ptr1);
    p->internalRep.twoPtrValue.ptr1 = NULL;
    p->internalRep.twoPtrValue.ptr2 = NULL;
    p->typePtr = NULL;
}

void DuplicateTclodbcEncodedString(Tcl_Obj *src, Tcl_Obj *dst) {
    Tcl_DString* pds1;
    Tcl_DString* pds2;

    // copy encoding value
    dst->internalRep.twoPtrValue.ptr1 = src->internalRep.twoPtrValue.ptr1;

    // duplicate DString
    pds1 = (Tcl_DString*) src->internalRep.twoPtrValue.ptr2;
    if (pds1) {
        pds2 = (Tcl_DString*) Tcl_Alloc(sizeof(Tcl_DString));
        Tcl_DStringInit(pds2);
        Tcl_DStringAppend(pds2,  Tcl_DStringValue(pds1), Tcl_DStringLength(pds1));
        dst->internalRep.twoPtrValue.ptr2 = pds2;
    } else {
        dst->internalRep.twoPtrValue.ptr2 = NULL;
    }
}

void UpdateTclodbcEncodedString(Tcl_Obj *o) {
    unsigned int len;
    Tcl_Encoding e = (Tcl_Encoding) o->internalRep.twoPtrValue.ptr1;
    Tcl_DString* pds = (Tcl_DString*) o->internalRep.twoPtrValue.ptr2;

    // This is needed only, if external value is actually stored in ptr2.
    // In any other case the representations are equal, and decoding is
    // not necessary.
    if (pds) {
        // this encodes strings from 'normal' strings to UTF-8, which is
        // the internal representation of tcl 8.0
        Tcl_DString ds;
        Tcl_DStringInit(&ds);
        Tcl_ExternalToUtfDString (e, Tcl_DStringValue(pds), Tcl_DStringLength(pds), &ds);

        len = Tcl_DStringLength(&ds);
        o->bytes = Tcl_Alloc(len+1);
        o->bytes[len] = '\0';
        o->length = len;
        memcpy (o->bytes, Tcl_DStringValue(&ds), len);
        Tcl_DStringFree(&ds);
    }
}

int SetTclodbcEncodedString(Tcl_Interp *interp, Tcl_Obj *p) {
    if (interp)
        Tcl_SetResult(interp, "Not supported", TCL_STATIC);

    return TCL_ERROR;
}

Tcl_ObjType TclodbcEncodedString = {
    "TclodbcEncodedString", 
    FreeTclodbcEncodedString, 
    DuplicateTclodbcEncodedString, 
    UpdateTclodbcEncodedString, 
    SetTclodbcEncodedString
};

#endif
