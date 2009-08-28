/*
 * Wrapper for Tcl hash tables. From Adrian Zimmer's book Tcl/Tk for Programmers.
 */
 
#include "tcl.h"

typedef void        (Tfp_ArrayDeleteProc) (ClientData);

typedef struct {
    Tcl_HashTable       *table;
    Tfp_ArrayDeleteProc *cleanProc;
} Tfp_ArrayType;

Tfp_ArrayType       *Tfp_ArrayInit( Tfp_ArrayDeleteProc *cleanProc );
void                Tfp_ArrayDestroy( Tfp_ArrayType *arr );
int                 Tfp_ArrayGet( Tfp_ArrayType *arr, char *key, ClientData *returnValue );
void                Tfp_ArraySet( Tfp_ArrayType *arr, char *key, ClientData value );
void                Tfp_ArrayDelete( Tfp_ArrayType *arr, char *key );


