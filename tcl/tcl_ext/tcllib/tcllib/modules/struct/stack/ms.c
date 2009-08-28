/* struct::stack - critcl - layer 2 definitions
 *
 * -> Support for the stack methods in layer 3.
 */

#include <ms.h>
#include <m.h>
#include <s.h>
#include <util.h>

/* .................................................. */
/*
 *---------------------------------------------------------------------------
 *
 * stms_objcmd --
 *
 *	Implementation of stack objects, the main dispatcher function.
 *
 * Results:
 *	A standard Tcl result code.
 *
 * Side effects:
 *	Per the called methods.
 *
 *---------------------------------------------------------------------------
 */

int
stms_objcmd (ClientData cd, Tcl_Interp* interp, int objc, Tcl_Obj* CONST* objv)
{
    S*  s = (S*) cd;
    int m;

    static CONST char* methods [] = {
	"clear", "destroy", "peek",   "pop",
	"push",	 "rotate",  "size",
	NULL
    };
    enum methods {
	M_CLEAR, M_DESTROY, M_PEEK,   M_POP,
	M_PUSH,	 M_ROTATE,  M_SIZE
    };

    if (objc < 2) {
	Tcl_WrongNumArgs (interp, objc, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    } else if (Tcl_GetIndexFromObj (interp, objv [1], methods, "option",
				    0, &m) != TCL_OK) {
	return TCL_ERROR;
    }

    /* Dispatch to methods. They check the #args in detail before performing
     * the requested functionality
     */

    switch (m) {
    case M_CLEAR:	return stm_CLEAR   (s, interp, objc, objv);
    case M_DESTROY:	return stm_DESTROY (s, interp, objc, objv);
    case M_PEEK:	return stm_PEEK    (s, interp, objc, objv, 0 /* peek */);
    case M_POP:		return stm_PEEK    (s, interp, objc, objv, 1 /* pop  */);
    case M_PUSH:	return stm_PUSH    (s, interp, objc, objv);
    case M_ROTATE:	return stm_ROTATE  (s, interp, objc, objv);
    case M_SIZE:	return stm_SIZE    (s, interp, objc, objv);
    }
    /* Not coming to this place */
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
