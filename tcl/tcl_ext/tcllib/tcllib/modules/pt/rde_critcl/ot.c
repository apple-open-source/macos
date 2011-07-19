/*
 * = = == === ===== ======== ============= =====================
 * == pt::rde (critcl) - Data Structures - Tcl_ObjType for interned strings.
 *
 */

#include <ot.h>    /* Our public API */
#include <util.h>  /* Allocation macros */
#include <pInt.h>  /* API to basic intern(ing) of strings */
#include <string.h>

/*
 * = = == === ===== ======== ============= =====================
 */

static void ot_free_rep   (Tcl_Obj* obj);
static void ot_dup_rep    (Tcl_Obj* obj, Tcl_Obj* dup);
static void ot_string_rep (Tcl_Obj* obj);
static int  ot_from_any   (Tcl_Interp* ip, Tcl_Obj* obj);

static Tcl_ObjType ot_type = {
    "tcllib/pt::rde/critcl",
    ot_free_rep,
    ot_dup_rep,
    ot_string_rep,
    ot_from_any
};

/*
 * = = == === ===== ======== ============= =====================
 */

int
rde_ot_intern (Tcl_Obj* obj, RDE_STATE p, char* pfx, char* sfx)
{
    int id;
    RDE_STRING* rs;

    TRACE (("rde_ot_intern (%p, '%s','%s' of %p = '%s')", p, pfx, sfx, obj, Tcl_GetString(obj)));

    /*
     * Quick exit if we have a cached and valid value.
     */

    if ((obj->typePtr == &ot_type) &&
	(obj->internalRep.twoPtrValue.ptr1 == p)) {
	rs = (RDE_STRING*) obj->internalRep.twoPtrValue.ptr2;
	TRACE (("CACHED %p = %d", rs, rs->id));
	return rs->id;
    }

    TRACE (("INTERNALIZE"));

    /*
     * Drop any previous internal rep.
     */

    if (obj->typePtr != NULL && obj->typePtr->freeIntRepProc != NULL) {
	obj->typePtr->freeIntRepProc(obj);
    }

    /*
     * Compute the new int-rep, interning the prefix-modified string.
     */

    if (!pfx && !sfx) {
	id = param_intern (p, obj->bytes);

    } else if (pfx && sfx) {
	int plen  = strlen(pfx);
	int slen  = strlen(sfx);
	char* buf = NALLOC (plen + slen + obj->length + 3, char);

	sprintf (buf, "%s %s %s", pfx, obj->bytes, sfx);

	id = param_intern (p, buf);
	ckfree(buf);

    } else if (pfx) {
	int plen  = strlen(pfx);
	char* buf = NALLOC (plen + obj->length + 2, char);

	sprintf (buf, "%s %s", pfx, obj->bytes);

	id = param_intern (p, buf);
	ckfree(buf);

    } else /* sfx */ {
	int slen  = strlen(sfx);
	char* buf = NALLOC (slen + obj->length + 2, char);

	sprintf (buf, "%s %s", obj->bytes, sfx);

	id = param_intern (p, buf);
	ckfree(buf);
    }

    rs = ALLOC (RDE_STRING);
    rs->next = p->sfirst;
    rs->self = obj;
    rs->id   = id;
    p->sfirst = rs;

    obj->internalRep.twoPtrValue.ptr1 = p;
    obj->internalRep.twoPtrValue.ptr2 = rs;
    obj->typePtr = &ot_type;

    return id;
}

/*
 * = = == === ===== ======== ============= =====================
 */

static void 
ot_free_rep(Tcl_Obj* obj)
{
    RDE_STATE   p  = (RDE_STATE)   obj->internalRep.twoPtrValue.ptr1;
    RDE_STRING* rs = (RDE_STRING*) obj->internalRep.twoPtrValue.ptr2;

    /* Take structure out of the tracking list. */
    if (p->sfirst == rs) {
	p->sfirst = rs->next;
    } else {
	RDE_STRING* iter = p->sfirst;
	while (iter->next != rs) {
	    iter = iter->next;
	}
	iter->next = rs->next;
    }

    /* Nothing to release. */
    ckfree ((char*) rs);
    obj->internalRep.twoPtrValue.ptr1 = NULL;
    obj->internalRep.twoPtrValue.ptr2 = NULL;
}
        
static void
ot_dup_rep(Tcl_Obj* obj, Tcl_Obj* dup)
{
    RDE_STRING* ors = (RDE_STRING*) obj->internalRep.twoPtrValue.ptr2;
    RDE_STRING* drs;
    RDE_STATE   p = ((RDE_STATE) obj->internalRep.twoPtrValue.ptr1);

    drs = ALLOC (RDE_STRING);
    drs->next = p->sfirst;
    drs->self = dup;
    drs->id   = ors->id;
    p->sfirst = drs;

    dup->internalRep.twoPtrValue.ptr1 = obj->internalRep.twoPtrValue.ptr1;
    dup->internalRep.twoPtrValue.ptr2 = drs;
    dup->typePtr = &ot_type;
}
        
static void
ot_string_rep(Tcl_Obj* obj)
{
    ASSERT (0, "Attempted reconversion of rde string to string rep");
}
    
static int
ot_from_any(Tcl_Interp* ip, Tcl_Obj* obj)
{
    ASSERT (0, "Illegal conversion into rde string");
    return TCL_ERROR;
}
/*
 * = = == === ===== ======== ============= =====================
 */


/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
