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
#include "tclobj.hxx"

#ifdef TCL_UTF_MAX
extern Tcl_ObjType TclodbcEncodedString;
#endif

extern char* strMemoryAllocationFailed;
char* strFunctionSequenceError = "Function sequence error in tclodbc.";

TclObj::TclObj(const char* s, int len) : p (NULL)
{
#if TCL_MAJOR_VERSION >= 8
    p = Tcl_NewStringObj((char*) s, len);
#else
    p = allocObj();
    Tcl_DStringAppend(&p->s, (char*) s, len);
#endif
    Tcl_IncrRefCount(p);
}

void TclObj::setLength(int len) 
{
#if TCL_MAJOR_VERSION >= 8
    Tcl_SetObjLength((Tcl_Obj*)(*this), len);
#else
    Tcl_DStringSetLength((Tcl_DString*)(*this), len);
#endif
}

TclObj::TclObj(const long i) : p (NULL)
{
#if TCL_MAJOR_VERSION >= 8
    p = Tcl_NewLongObj(i);
#else
    p = allocObj();
    char s[30];
    sprintf(s, "%ld", i);
    Tcl_DStringAppend(&p->s, s, -1);
    p->refCount = 0;
#endif
    Tcl_IncrRefCount(p);
}

TclObj::TclObj(const TclObj& o) : p (NULL)
{
    p = o.p;
    if (p) {
	Tcl_IncrRefCount(p);
    }
}

#if TCL_MAJOR_VERSION >= 8

TclObj::TclObj(Tcl_Obj* o) : p (NULL)
{
    p = o;
    if (p) {
	Tcl_IncrRefCount(p);
    }
}

TclObj::operator Tcl_Obj* () 
{
    if (!p) {
	p = Tcl_NewObj();
	Tcl_IncrRefCount(p); 
    }
    return p;
}
#else

TclObj::TclObj(DStringObj* s) : p (NULL)
{
    p = s;
    if (p) {
	Tcl_IncrRefCount(p);
    }
}

TclObj::operator DStringObj* () 
{
    if (!p) {
        p = allocObj();
	Tcl_IncrRefCount(p); 
    }
    return p;
}

TclObj::operator Tcl_DString* () 
{
    return &((DStringObj*)*this)->s;
}

DStringObj* TclObj::allocObj()
{
    DStringObj* p;
    p = (DStringObj*) Tcl_Alloc(sizeof(DStringObj));
    if (!p) {
        THROWSTR(strMemoryAllocationFailed);
    }
    Tcl_DStringInit(&p->s);
    p->refCount = 0;
    p->list = NULL;
    p->llength = 0;
    return p;
}

void TclObj::splitList (Tcl_Interp* interp) {
    char** argv = NULL;
    if (p) {
        invalidateList();
        // split the original into char array
        if (Tcl_SplitList(interp, *this, &p->llength, &argv) && interp) {
            THROWSTR(interp->result);
	}

        // organize character array to array of DStringObj's
        if (p->llength) {
            p->list = (DStringObj**) Tcl_Alloc(sizeof(DStringObj*)*p->llength);
            if (!p->list) {
                THROWSTR(strMemoryAllocationFailed);
	    }
            for (int i=0; i < p->llength; ++i) {
                p->list[i] = TclObj::allocObj();
                Tcl_DStringAppend(&(p->list[i]->s), argv[i], -1);
		        Tcl_IncrRefCount(p->list[i]); 
            }
        }

        // free character array space
        if (argv) Tcl_Free ((char*) argv);
    }
}

void TclObj::invalidateList () {
    if (p) {
        if (p->list) {
            for (int i=0; i < p->llength; ++i) {
		Tcl_DecrRefCount(p->list[i]); 
            }
            Tcl_Free ((char*) p->list);
        }
        p->list = NULL;
        p->llength = 0;
    }
}

void TclFreeObj(DStringObj* p) 
{
    Tcl_DStringFree(&p->s); 
    if (p->list) {
        for (int i=0; i < p->llength; ++i) {
            Tcl_DecrRefCount(p->list[i]);
        }
        Tcl_Free((char*) p->list);
    }
    Tcl_Free((char*) p); 
}

#endif

TclObj::~TclObj()
{
    if (p) {
	Tcl_DecrRefCount(p);
    }
}

TclObj& TclObj::operator= (const TclObj& o)
{
    if (p != o.p) { // check for equality
	if (p) Tcl_DecrRefCount(p); 
	p = o.p;
	if (p) Tcl_IncrRefCount(p);
    }
    return *this;
}

TclObj& TclObj::set (const char* s, int len)
{
    if (p) Tcl_DecrRefCount(p);
    if (s) {
#if TCL_MAJOR_VERSION >= 8
	p = Tcl_NewStringObj((char*) s, len);
#else
	p = allocObj();
	Tcl_DStringAppend(&p->s, (char*) s, len);
#endif
	Tcl_IncrRefCount(p);
    } else {
        p = NULL;
    }
    return *this;
}

TclObj::operator char* () const
{
    if (p) {
#if TCL_MAJOR_VERSION >= 8
	return Tcl_GetStringFromObj(p,NULL);
#else
        return Tcl_DStringValue(&p->s);
#endif
    } else {
	return "";
    }
}

TclObj TclObj::appendElement (TclObj obj, Tcl_Interp* interp) {
#if TCL_MAJOR_VERSION >= 8
    if (Tcl_ListObjAppendElement(interp, *this, obj) != TCL_OK && interp) {
        THROWOBJ(TclObj(Tcl_GetObjResult(interp)));
    }
#else
    Tcl_DStringAppendElement(*this, obj);
    invalidateList();
#endif
    return *this;
};

TclObj TclObj::append (const char* c, int len) {
#if TCL_MAJOR_VERSION >= 8
    Tcl_AppendToObj(*this, (char*) c, len); 
#else
    Tcl_DStringAppend(*this, (char*) c, len);
    invalidateList();
#endif
    return *this;
};

TclObj TclObj::lindex (int pos, Tcl_Interp* interp) {
#if TCL_MAJOR_VERSION >= 8
    Tcl_Obj* objPtr;
    if (Tcl_ListObjIndex(interp, *this, pos, &objPtr) != TCL_OK && interp) {
        THROWOBJ(TclObj(Tcl_GetObjResult(interp)));
    }
    return TclObj(objPtr);

#else
    if (!p->list) {
        splitList(interp);
    }

    if (pos < 0 || p->llength <= pos) {
        THROWSTR("Invalid list index");
    }
    
    return TclObj(p->list[pos]);
#endif
};

int TclObj::llenght (Tcl_Interp* interp) {
#if TCL_MAJOR_VERSION >= 8
    int i;
    if (Tcl_ListObjLength(interp, *this, &i) != TCL_OK && interp) {
        THROWOBJ(TclObj(Tcl_GetObjResult(interp)));
    }
    return i;
#else
    if (!p->list) {
        splitList(interp);
    }
    return p->llength;
#endif
};

int TclObj::lenght () {
    int i;
    if (p) {
#if TCL_MAJOR_VERSION >= 8
        Tcl_GetStringFromObj(p, &i);
#else
        i = Tcl_DStringLength(&p->s);
#endif
        return i;
    } else {
        return 0;
    }
};

int TclObj::asInt (Tcl_Interp* interp) 
{
    int i = 0;
#if TCL_MAJOR_VERSION >= 8
    if (Tcl_GetIntFromObj(interp, *this, &i) != TCL_OK && interp) {
        THROWOBJ(TclObj(Tcl_GetObjResult(interp)));
    }
#else
    if (Tcl_GetInt(interp, *this, &i) != TCL_OK && interp) {
        THROWSTR(interp->result);
    }
#endif
    return i;
}

int TclObj::isNull() {
    return (p == NULL);
}


TclObj::TclObj(const char* s, const Tcl_Encoding e, int len) : p (NULL)
{
    *this = TclObj(s, len);
#ifdef TCL_UTF_MAX
    Decode(e);
#endif
}

int TclObj::Encode(const Tcl_Encoding e, Tcl_Interp* interp)
{
#ifdef TCL_UTF_MAX
    char* c;
    int len;
    Tcl_DString ds;
    Tcl_DString *pds;
    Tcl_Obj* o;

    o = *this;
    if (!o) {
        THROWSTR(strFunctionSequenceError);
    }

    // initialize DString
    Tcl_DStringInit(&ds);

    // if the string is already encoded with the same encoding, do not encode
    // again
    if (o->typePtr == &TclodbcEncodedString
	    && o->internalRep.twoPtrValue.ptr1 == e) {
        return TCL_OK;
    }

    // get utf string representation
    c = Tcl_GetStringFromObj(o, &len);
    
    // free old internal representation, if any
    if (o->typePtr && o->typePtr->freeIntRepProc) {
	o->typePtr->freeIntRepProc(o);
    }

    // store type pointer and encoding
    o->typePtr = &TclodbcEncodedString;
    o->internalRep.twoPtrValue.ptr1 = e;
    o->internalRep.twoPtrValue.ptr2 = NULL;

    // increment encoding reference count
    if (e) { Tcl_GetEncoding(NULL,Tcl_GetEncodingName(e)); }

    // encode using a DString stored in the stack
    Tcl_UtfToExternalDString(e, c, len, &ds);

    // check, if the strings differ
    if (len != Tcl_DStringLength(&ds)
	    || memcmp(Tcl_DStringValue(&ds), c, len)) {
        // allocate permanent storage for encoded data
        pds = (Tcl_DString*) Tcl_Alloc(sizeof(Tcl_DString));

        // init structure
        Tcl_DStringInit(pds);

        // store values to the struct
        Tcl_DStringAppend(pds,  Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));
        o->internalRep.twoPtrValue.ptr2 = pds;
    }

    // finalize
    Tcl_DStringFree(&ds);
#endif

    return TCL_OK;
}

int TclObj::Decode(const Tcl_Encoding e)
{
#ifdef TCL_UTF_MAX
    // if p == NULL, nothing has to be done here
    if (p) {
        Tcl_DString ds;
        Tcl_DString* pds;

        int len;
        char* s;

        // decode in the stack, and store the utf representation
        Tcl_DStringInit(&ds);
        s = (char*) *this;
        len = this->lenght();
        Tcl_ExternalToUtfDString (e, s, len, &ds);

        // check, if the strings differ, store encoded value only when
        // necessary
        if (len != Tcl_DStringLength(&ds)
		|| memcmp(Tcl_DStringValue(&ds), s, len)) {
            // allocate permanent storage for encoded data, init structure
            pds = (Tcl_DString*) Tcl_Alloc(sizeof(Tcl_DString));
            Tcl_DStringInit(pds);

            // save encoded representation
            Tcl_DStringAppend(pds, s, len);

            // replace original string representation with decoded value
            Tcl_SetStringObj(p, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));

            // encoded representation is saved to the object struct, saves
            // encoding effort if the same string is passed as encoded string
            // later.
            p->internalRep.twoPtrValue.ptr2 = pds;
        } else {
            p->internalRep.twoPtrValue.ptr2 = NULL;
        }

        // store decoded value to object and set object type
        p->typePtr = &TclodbcEncodedString;
        p->internalRep.twoPtrValue.ptr1 = e;

        // increment encoding reference count
        if (e) { Tcl_GetEncoding(NULL,Tcl_GetEncodingName(e)); }

        // finalize
        Tcl_DStringFree(&ds);
    }
#endif

    return TCL_OK;
}

int TclObj::EncodedLenght()
{
#ifdef TCL_UTF_MAX
    if (p && p->typePtr == &TclodbcEncodedString
	    && p->internalRep.twoPtrValue.ptr2) {
        return
	    Tcl_DStringLength((Tcl_DString*)(p->internalRep.twoPtrValue.ptr2));
    } else
#endif
        return lenght();
}

char* TclObj::EncodedValue()
{
#ifdef TCL_UTF_MAX
    if (p && p->typePtr == &TclodbcEncodedString
	    && p->internalRep.twoPtrValue.ptr2) {
        return
	    Tcl_DStringValue((Tcl_DString*)(p->internalRep.twoPtrValue.ptr2));
    } else 
#endif
        return (char*) *this;
}


void TclObj::eval (Tcl_Interp* interp) {
#if TCL_MAJOR_VERSION == 8 
//#if TCL_MINOR_VERSION >= 1
//    if (Tcl_EvalObj(interp, (Tcl_Obj*) *this, TCL_EVAL_DIRECT) == TCL_ERROR)
//#else
    if (Tcl_EvalObj(interp, (Tcl_Obj*) *this) == TCL_ERROR) {
//#endif
        THROWOBJ(TclObj(Tcl_GetObjResult(interp)));
    }
#elif TCL_MAJOR_VERSION == 7
    if (Tcl_Eval(interp, (char*) *this) == TCL_ERROR) {
        THROWSTR(interp->result);
    }
#endif
}

