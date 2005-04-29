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

#ifndef TCLOBJ_H
#define TCLOBJ_H

#include <tcl.h>

// Errors are thrown with these two macros. In debug version also
// line information is included.

#ifdef _DEBUG
#define THROWSTR(str) \
{\
    TclObj errObj (str);\
    TclObj file (__FILE__);\
	errObj.append(" File: ");\
	errObj.append(file);\
    TclObj line (__LINE__);\
	errObj.append(" Line: ");\
	errObj.append(line);\
    throw errObj;\
}
#else
#define THROWSTR(str) \
{\
    throw TclObj(str);\
}
#endif
#ifdef _DEBUG
#define THROWOBJ(obj) \
{\
    TclObj errObj ((char*)obj);\
    TclObj file (__FILE__);\
	errObj.append(" File: ");\
	errObj.append(file);\
    TclObj line (__LINE__);\
	errObj.append(" Line: ");\
	errObj.append(line);\
    throw errObj;\
}
#else
#define THROWOBJ(obj) \
{\
    throw obj;\
}
#endif

// Simple TclObj class for handling Tcl_Obj* pointers and
// reference counting.

// This encapsulation of reference counting is particulary
// useful with exception handling to ensure that no created
// object pointers remain allocated when procedure is terminated 
// by thrown exception.

// Object implementation is done with Tcl_DString in earlier tcl
// versions, and Tcl_Obj with tcl 8.0.

#if TCL_MAJOR_VERSION < 8
#define Tcl_GetStringFromObj(o,l) (o)
#define Tcl_SetObjResult(i,r) Tcl_DStringResult(i,r)
#define Tcl_ObjSetVar2(i,n1,n2,v,f) Tcl_SetVar2(i,n1,n2,v,f)
#define Tcl_CreateObjCommand(i,c,p,d,p2) Tcl_CreateCommand(i,c,p,d,p2)
#define TCL_CMDARGS char* objv[]
struct DStringObj {
    Tcl_DString s;
    int refCount;
    DStringObj** list;
    int llength;
};
void TclFreeObj(DStringObj*);
#define Tcl_IncrRefCount(objPtr) \
	++(objPtr)->refCount
#define Tcl_DecrRefCount(objPtr) \
	if (--(objPtr)->refCount <= 0) TclFreeObj(objPtr)
#else
#define TCL_CMDARGS Tcl_Obj * const objv[]
#endif

#ifdef TCL_UTF_MAX
// Unicode-enabled Tcl stuff
#else
#define Tcl_Encoding void*
#endif

class TclObj
{
public:
    TclObj() : p(NULL) {};
    TclObj(const char*, int len = -1);
    TclObj(const long);
    TclObj(const TclObj&);
    TclObj(const char*, const Tcl_Encoding, int len = -1);

    ~TclObj();

    operator char* () const;

    TclObj& operator= (const TclObj&);

    TclObj& set (const char*, int len = -1);
    // sets object from character string

    void setLength (int len);
    // sets object lenght

    TclObj append (const char*, int len = -1);
    // appends to string

    TclObj appendElement (TclObj,Tcl_Interp* interp=NULL);
    // appends list element to list

    TclObj lindex (int pos,Tcl_Interp* interp=NULL);
    // gets list element from a list

    void eval (Tcl_Interp* interp);
    // evaluate command in tcl interpreter, return tcl success code

    int lenght ();
    // string lenght

    int llenght (Tcl_Interp* interp=NULL);
    // list lenght

    int asInt(Tcl_Interp* interp=NULL);
    // return value as integer

    int isNull();
    // return boolean value indicating, if the obj has been initialized

    int Encode(const Tcl_Encoding, Tcl_Interp* interp=NULL);
    // make the value an encoded string

    int EncodedLenght();
    // the length of a previously encoded string

    char* EncodedValue();
    // the string value of a previously encoded string

    int Decode(const Tcl_Encoding);
    // move the current string value to encoded value, and decode 
    // string value

#if TCL_MAJOR_VERSION >= 8
    TclObj(Tcl_Obj*);
    operator Tcl_Obj* ();
#else
    TclObj(DStringObj*);
    operator DStringObj* ();
    operator Tcl_DString* ();
#endif

private:
#if TCL_MAJOR_VERSION >= 8
    Tcl_Obj* p;
#else
    DStringObj* p;
    static DStringObj* allocObj();
    void splitList(Tcl_Interp* interp);
    void invalidateList();
#endif
};


#endif

