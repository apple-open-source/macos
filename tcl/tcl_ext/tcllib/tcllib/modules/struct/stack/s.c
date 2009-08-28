/* struct::stack - critcl - layer 1 definitions
 * (c) Stack functions
 */

#include <s.h>
#include <util.h>

/* .................................................. */

S*
st_new (void)
{
    S* s = ALLOC (S);

    s->max   = 0;
    s->stack = Tcl_NewListObj (0,NULL);
    Tcl_IncrRefCount (s->stack);

    return s;
}

void
st_delete (S* s)
{
    /* Delete a stack in toto.
     */

    Tcl_DecrRefCount (s->stack);
    ckfree ((char*) s);
}

/* .................................................. */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
