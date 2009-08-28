/*
 * Wrapper for Tcl hash tables. From Adrian Zimmer's book Tcl/Tk for Programmers.
 */

#include "Tfp_Arrays.h"

Tfp_ArrayType       
*Tfp_ArrayInit( Tfp_ArrayDeleteProc *cleanProc )
{
    Tfp_ArrayType   *arr;
    
    arr = (Tfp_ArrayType *) Tcl_Alloc( sizeof(Tfp_ArrayType) );
    arr->table = (Tcl_HashTable *) Tcl_Alloc( sizeof(Tcl_HashTable) );
    Tcl_InitHashTable( arr->table, TCL_STRING_KEYS );
    arr->cleanProc = cleanProc;
    return arr;
}

void                
Tfp_ArrayDestroy( Tfp_ArrayType *arr )
{
    Tcl_HashEntry   *p;
    Tcl_HashSearch  s;
    
    if (arr->cleanProc != (Tfp_ArrayDeleteProc *) NULL) {
        for (p = Tcl_FirstHashEntry( arr->table, &s ); p != (Tcl_HashEntry *) NULL;
                p = Tcl_NextHashEntry( &s )) {
            (*arr->cleanProc) ( Tcl_GetHashValue( p ) );
        }
    }
    Tcl_DeleteHashTable( arr->table );
    Tcl_Free( (char *) arr->table );
    Tcl_Free( (char *) arr );
}

int                 
Tfp_ArrayGet( Tfp_ArrayType *arr, char *key, ClientData *returnValue )
{
    Tcl_HashEntry   *p;
    
    p = Tcl_FindHashEntry( arr->table, key );
    if (p == (Tcl_HashEntry *) NULL) {
        return 0;
    }
    *returnValue = Tcl_GetHashValue( p );
    return 1;
}

void                 
Tfp_ArraySet( Tfp_ArrayType *arr, char *key, ClientData value )
{
    int             junk;
    Tcl_HashEntry   *p;
    
    p = Tcl_CreateHashEntry( arr->table, key, &junk );
    Tcl_SetHashValue( p, value );
}

void                
Tfp_ArrayDelete( Tfp_ArrayType *arr, char *key )
{
    Tcl_HashEntry   *p;
    
    p = Tcl_FindHashEntry( arr->table, key );
    if (p == (Tcl_HashEntry *) NULL) {
        return;
    }    
    (*arr->cleanProc) ( Tcl_GetHashValue( p ) );
    Tcl_DeleteHashEntry( p );
}

/*---------------------------------------------------------------------------*/