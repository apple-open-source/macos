/*
 * compiler/core/dependency.c - sorts types/values in order of dependency.
 *                typeDefs list is re-ordered
 *                going from independent->dependent types
 *
 * this is done after all import linking is done
 *
 * Mike Sample
 * 91/08/12
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/compiler/core/Attic/dependency.c,v 1.1 2001/06/20 21:27:56 dmitch Exp $
 * $Log: dependency.c,v $
 * Revision 1.1  2001/06/20 21:27:56  dmitch
 * Adding missing snacc compiler files.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:46  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.4  1995/07/25 19:41:22  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.3  1994/10/08  03:48:37  rj
 * since i was still irritated by cpp standing for c++ and not the C preprocessor, i renamed them to cxx (which is one known suffix for C++ source files). since the standard #define is __cplusplus, cplusplus would have been the more obvious choice, but it is a little too long.
 *
 * Revision 1.2  1994/09/01  00:31:56  rj
 * snacc_config.h removed; dependency.h includet.
 *
 * Revision 1.1  1994/08/28  09:49:00  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <stdio.h>

#include "asn-incl.h"
#include "mem.h"
#include "asn1module.h"
#include "snacc-util.h"
#include "dependency.h"


/* prototypes */

void SortTypeDependencies PROTO ((Module *m));

void SortInterModuleDependencies PROTO ((ModuleList *m));

TypeDefList *RemoveAndSortIndependents PROTO ((TypeDefList *tdl));

void SortTypeDefs PROTO ((TypeDefList *tdl));

void BuildLocalRefList PROTO ((Type *t, TypeDefList *refList));

void BuildWeightedLocalRefList PROTO ((Type *t, TypeDefList *refList));


long int GetElmtIndex PROTO ((TypeDef *td, TypeDefList *tdl));

/*
void MoveAfter PROTO ((unsigned long int currIndex, unsigned long int afterIndex, AsnList *l));
*/

/*
 * Sorts type dependencies by reodering TypeDefs linear list
 * with least dependent types followed by dependent types
 */
void
SortAllDependencies PARAMS ((modList),
    ModuleList *modList)
{
    Module *m;

    FOR_EACH_LIST_ELMT (m, modList)
    {
        SortTypeDependencies (m);
    }

/*    SortInterModuleDependencies (modList); */

} /* SortAllDependencies */


/*
 * This attempts to sort the types in order of dependency
 * (least dependent --> dependent)
 *
 *  This should only be called after the CTypeInfo or CxxTypeInfo
 *  has been added to the  types.
 *    (the isPtr field is used to help determine ordering)
 *
 *  Algorithm: (wierd!)
 *
 *  First separte the ASN.1 type defs into 4 separate groups
 *
 *    1. Type defs that are defined directly from primitive/library types
 *         eg Foo ::= INTEGER {one (1), two (2) }
 *
 *    2. Type defs reference no local types in a way that needs a
 *       forward decl. of the ref'd type (ie ptr refs)
 *
 *    3. Type defs that reference local types in a way that needs
 *       a previous decl of the ref'd type (ie non ptr refs for SET/SEQ
 *       elmts)
 *
 *    4. Type defs that are not referenced by any local types
 *       (hence no local types depend on them so they can go last)
 *
 *
 *  The type defs in group 3 are further sorted by the SortTypeDefs routine
 *
 *  Then all of the groups are merged in the order 1-2-3-4.
 *
 * Some wierd recursive types might cause problems...
 *
 *
 * MS 92
 */
void
SortTypeDependencies PARAMS ((m),
    Module *m)
{
    TypeDef *curr;
    TypeDefList *prims;
    TypeDefList *noRefs;
    TypeDefList *refs;
    TypeDefList *notRefd;
    TypeDef **newElmtHndl;

    prims = AsnListNew (sizeof (void*));
    noRefs = AsnListNew (sizeof (void*));
    refs = AsnListNew (sizeof (void*));
    notRefd = AsnListNew (sizeof (void*));

    /* put each TypeDef in the appropriate list (1-4)*/
    FOR_EACH_LIST_ELMT (curr, m->typeDefs)
    {
        if (IsDefinedByLibraryType (curr->type))
            newElmtHndl = (TypeDef**) AsnListAppend (prims);

        else if (curr->localRefCount == 0)
            newElmtHndl = (TypeDef**) AsnListAppend (notRefd);

        else
        {
            /* get list of local types that this type def refs */
            curr->refList = AsnListNew (sizeof (void*));
            BuildLocalRefList (curr->type, curr->refList);

            if (LIST_EMPTY (curr->refList))
            {
                newElmtHndl = (TypeDef**) AsnListAppend (noRefs);
                Free (curr->refList);
                curr->refList = NULL;
            }
            else
                newElmtHndl = (TypeDef**) AsnListAppend (refs);
        }

        *newElmtHndl = curr;
    }

    /* sort problem types */
    SortTypeDefs (refs);

    /* free refList space */
    FOR_EACH_LIST_ELMT (curr, refs)
    {
        if (curr->refList != NULL)
        {
            AsnListFree (curr->refList);
            curr->refList = NULL;
        }
    }

    /*
     * combine the typdef lists with the prims followed by the
     * types that don't reference other types
     * then prims, followed by composite types
     */
    prims = AsnListConcat (prims, noRefs);
    prims = AsnListConcat (prims, refs);
    prims = AsnListConcat (prims, notRefd);

    AsnListFree (m->typeDefs);
    Free (noRefs);
    Free (refs);
    Free (notRefd);

    m->typeDefs = prims;

} /* SortTypeDependencies */




/*
 * Attempt to sort modules in order of "depends on none" to
 * "depends on all" where a dependency is caused by importing
 * from another module.
 * cyclic dependencies are a pain
 */
/*
 *  Not implemented yet... perhaps best left in user's hands
 *  ie set it by the cmd line order
 */
/*
void
SortInterModuleDependencies PARAMS ((m),
    ModuleList *m)
{

}  SortInterModuleDependencies */



/*
 * Given a non-empty TypeDef list, the refLists of TypeDefs
 * are used to divide the list into two lists, one list
 * that is sorted the order of dependency (independed-->dependent)
 * and the other list contains types that are mutually dependent
 * (recursive or depend on recursive types)
 * The sorted list is returned and the passed in list has those
 * TypeDefs that are now in the sorted list removed.
 */
TypeDefList*
RemoveAndSortIndependents PARAMS ((tdl),
    TypeDefList *tdl)
{
    TypeDef *last;
    TypeDef *currTd;
    TypeDef **tdHndl;
    TypeDef *tdRef;
    AsnListNode *nextListNode;
    long int tdIndex;
    long int lastSLCount;
    TypeDefList *subList;
    int keep;

    /*
     * iterate through the list making sub lists that don't depend
     * on the others in the active list.  Join sub lists in order
     * and then deal with the active list if any
     */
    lastSLCount = -1; /* just to start */
    subList = AsnListNew (sizeof (void*));

    if (LIST_EMPTY (tdl))
        return subList;

    /* iterate through each type def in the tdl */
    while ((LIST_COUNT (subList) > lastSLCount) && !LIST_EMPTY (tdl))
    {
        lastSLCount = LIST_COUNT (subList);
        last = (TypeDef*)LAST_LIST_ELMT (tdl);
        SET_CURR_LIST_NODE (tdl, FIRST_LIST_NODE (tdl));
        currTd = (TypeDef*)CURR_LIST_ELMT (tdl);
        while (1)
        {
            nextListNode = NEXT_LIST_NODE (tdl);
            keep = 0;

            /*
             * iterate through this type def's local type refs.
             *
             * if any type def in the current type's local type ref list
             * is in the tdl, then teh current type must remain in the tdl
             * because it depends on that type.
             */
            FOR_EACH_LIST_ELMT (tdRef, currTd->refList)
            {
                /* don't worry about recursive refs to self */
                if (tdRef != currTd)
                {
                    /*
                     * if the tdRef is not in tdl
                     * GetElmtIndex will return < 0
                     * if the tdRef is in the tdl, then the
                     * currTd must remain in the tdl.
                     */
                    tdIndex = GetElmtIndex (tdRef, tdl);
                    if (tdIndex >= 0)
                        keep = 1;
                }
            }
            if (!keep)
            {
                /* append to sublist and remove for tdl */
                tdHndl = (TypeDef**) AsnListAppend (subList);
                *tdHndl = currTd;
                AsnListRemove (tdl);
            }
            if (currTd == last)
                break; /* exit while */

            SET_CURR_LIST_NODE (tdl, nextListNode);
            currTd = (TypeDef*)CURR_LIST_ELMT (tdl);
        }
    }
    return subList;

} /* RemoveAndSortIndependents */


/*
 * Given a list of types that depend on each other, this attempts
 * to sort the list from independent--> most dependent.
 *
 * Kind of wierd algorithm
 *  1. first separate and sort out linearly dependent types and place in
 *     a properly ordered list (RemoveAndSortIndependents) (call it "A")
 *
 *  2. if types with non-linear (recursive) dependencies remain,
 *     divide them into two groups, recursive (call it "B")(see recursive.c)
 *     and non-recursive (call it "C".  The non-recursive ones will depend
 *     on the recursive ones (otherwise step 1 would have grabbed 'em).
 *
 *  3. Sort the types in list C as done in step one - there should be
 *     no problems (ie unsorted leftovers) since none of them are recursive.
 *
 *  4. For the recursive types in list B, re-do their refLists such that
 *     any types ref'd by a Ptr are not included in the refList
 *     (may have to update this wrt how the ref is used -
 *         eg in an inline of the ref'ing type).  Then sort as in Step 1.
 *     Any types that could not be sorted have a definite problem and
 *     compiliation problems will occur. (.. And the code generation
 *     technique must be changed)
 *     (for C only the SET OF and SEQ OF Types are stripped from this
 *     since they are 'generic' - ie don't depend on the list elmt type)
 *
 *   5. re-combine all of the lists in order of dependency ie
 *      A-B-(B's leftovers)-C
 *
 *     (the stripped C lists go after 'A')
 */
void
SortTypeDefs PARAMS ((tdl),
    TypeDefList *tdl)
{
    TypeDef *last;
    TypeDef *currTd;
    TypeDef **tdHndl;
    TypeDef *tmpTd;
    TypeDef *tdRef;
    AsnListNode *tdNodeToMove;
    AsnListNode *nextListNode;
    long int maxRefCount;
    TypeDefList *subList;  /* "A" */
    TypeDefList *nonRec;
    TypeDefList *sortedRec;  /* "B" */
    TypeDefList *sortedNonRec; /* "C" */
    TypeDefList *cLists;

    if ((tdl == NULL) || (LIST_EMPTY (tdl)))
        return;

    subList = RemoveAndSortIndependents (tdl);

    /* return if simple sort worked (no recursive types) */
    if (LIST_EMPTY (tdl))
    {
        *tdl = *subList;
        Free (subList);
        return;
    }

    /*
     * divide the remaining interdepedent types into
     * two groups recursive and non-recursive.
     * leave the recursive in the tdl and put the others in a new list.
     * The non-recursive ones obviously depend on the recursive
     * on since all of the simple type dependencies have been
     * dealt with by RemoveAndSortIndependents
     */
    last = (TypeDef*)LAST_LIST_ELMT (tdl);
    SET_CURR_LIST_NODE (tdl, FIRST_LIST_NODE (tdl));
    currTd = (TypeDef*)CURR_LIST_ELMT (tdl);
    nonRec = AsnListNew (sizeof (void*));

    while (1)
    {
        nextListNode = NEXT_LIST_NODE (tdl);

        if (!currTd->recursive)
        {
            tdHndl = (TypeDef**)AsnListAppend (nonRec);
            *tdHndl = currTd;
            AsnListRemove (tdl);
        }

        if (currTd == last)
            break; /* exit while */

        SET_CURR_LIST_NODE (tdl, nextListNode);
        currTd = (TypeDef*)CURR_LIST_ELMT (tdl);
    }

    /* sort the non-recusive types */
    sortedNonRec = RemoveAndSortIndependents (nonRec);

    if (!LIST_EMPTY (nonRec))
    {
        fprintf (stderr,"SortTypeDefs: internal compiler error - non recursive type defs failed sort.\n");
        sortedNonRec = AsnListConcat (sortedNonRec, nonRec);
    }
    Free (nonRec);

    /*
     * Remove list types from the list since they are generic.
     * put them in "cLists".
     * then re-do the dependency list for each type definition that
     * remain in the recursive list with weighting - ie types
     * that are ref'd as ptrs don't count. Then re-sort.
     */
    last = (TypeDef*)LAST_LIST_ELMT (tdl);
    SET_CURR_LIST_NODE (tdl, FIRST_LIST_NODE (tdl));
    currTd = (TypeDef*)CURR_LIST_ELMT (tdl);

    cLists = AsnListNew (sizeof (void*));
    while (1)
    {
        nextListNode = NEXT_LIST_NODE (tdl);

        /* nuke old ref list */
        AsnListFree (currTd->refList);
        currTd->refList = NULL;

        /* for C only, remove lists since they are generic */
        if ((currTd->cTypeDefInfo != NULL) &&
            ((currTd->type->basicType->choiceId == BASICTYPE_SETOF) ||
             (currTd->type->basicType->choiceId == BASICTYPE_SEQUENCEOF)))
        {
            tdHndl = (TypeDef**)AsnListAppend (cLists);
            *tdHndl = currTd;
            AsnListRemove (tdl);
        }

        if (currTd == last)
            break; /* exit while */

        SET_CURR_LIST_NODE (tdl, nextListNode);
        currTd = (TypeDef*)CURR_LIST_ELMT (tdl);
    }



    FOR_EACH_LIST_ELMT (currTd, tdl)
    {
        currTd->refList = AsnListNew (sizeof (void*));
        BuildWeightedLocalRefList (currTd->type, currTd->refList);
    }

    sortedRec = RemoveAndSortIndependents (tdl);

    /*
     * now merge subLists and put in tdl:
     *  tdl = cLists + sortedRec + impossible rec in tdl  + sorted nonRec
     */
    subList = AsnListConcat (subList, cLists);
    subList = AsnListConcat (subList, sortedRec);
    subList = AsnListConcat (subList, tdl);
    subList = AsnListConcat (subList, sortedNonRec);
    *tdl = *subList;

    Free (cLists);
    Free (subList);
    Free (sortedRec);
    Free (sortedNonRec);

}  /* SortTypeDefs */




/*
 * Builds list of TypeDefs in this module that the given type refs.
 * Does not follow type refs to include their type refs.
 */
void
BuildLocalRefList PARAMS ((t, refList),
    Type *t _AND_
    TypeDefList *refList)
{
    NamedType *e;
    TypeDef **tdHndl;

    switch (t->basicType->choiceId)
    {
        case BASICTYPE_CHOICE:
        case BASICTYPE_SET:
        case BASICTYPE_SEQUENCE:
            FOR_EACH_LIST_ELMT (e, t->basicType->a.choice)
            {
                BuildLocalRefList (e->type, refList);
            }
            break;

        case BASICTYPE_SETOF:
        case BASICTYPE_SEQUENCEOF:
            BuildLocalRefList (t->basicType->a.setOf, refList);
            break;

       case BASICTYPE_LOCALTYPEREF:
            tdHndl = (TypeDef**)AsnListAppend (refList);
            *tdHndl = t->basicType->a.localTypeRef->link;
            break;

        /*
         * default: other types are not aggregate and
         * and can be ignored
         */
    }
} /* BuildLocalRefList */


/*
 * Builds list of TypeDefs in this module that the given type references.
 * Does not follow type refs to include their type refs.
 * Does not include types that are ref'd as ptrs since
 * If the target lang is C the type SET OF/SEQ OF types reference
 * are not counted due to the current 'genericness' of the C list type
 * (it doesn't need type info)
 * they shouldn't affect type ordering.
 */
void
BuildWeightedLocalRefList PARAMS ((t, refList),
    Type *t _AND_
    TypeDefList *refList)
{
    NamedType *e;
    TypeDef **tdHndl;

    switch (t->basicType->choiceId)
    {
        case BASICTYPE_CHOICE:
        case BASICTYPE_SET:
        case BASICTYPE_SEQUENCE:
            FOR_EACH_LIST_ELMT (e, t->basicType->a.choice)
            {
                BuildWeightedLocalRefList (e->type, refList);
            }
            break;



        case BASICTYPE_SETOF:
        case BASICTYPE_SEQUENCEOF:
            /*
             * normalize makes embedded list defs into
             * separate type defs now so this clause will
             * not fire. (ie they will be a LOCAL_TYPEREF
             * to the removed list type instead)
             */

            /*
             * list types for C don't really depend on
             * the component type (void*). So if the target lang
             * is C then can achieve better ordering
             * for ugly recursive defs by using this relaxation
             * (ie not including the component type in the ref list)
             */
            if (t->cTypeRefInfo == NULL)
                BuildWeightedLocalRefList (t->basicType->a.setOf, refList);

            break;

        case BASICTYPE_LOCALTYPEREF:

            if (((t->cxxTypeRefInfo != NULL) &&
                  !(t->cxxTypeRefInfo->isPtr)) ||
                ((t->cTypeRefInfo != NULL) && !(t->cTypeRefInfo->isPtr)))
            {
                tdHndl = (TypeDef**)AsnListAppend (refList);
                *tdHndl = t->basicType->a.localTypeRef->link;
            }
            break;

        /*
         * default: other types are not aggregate and
         * and can be ignored
         */
    }
} /* BuildWeightedLocalRefList */



/*
 * Returns the index (starting a 0 for the first elmt)
 * of the given td in the td list (tdl)
 * returns -1 if td is not in the list
 */
long int
GetElmtIndex PARAMS ((td, tdl),
    TypeDef *td _AND_
    TypeDefList *tdl)
{
    void *tmp;
    TypeDef *tmpTd;
    long int index;

    index = 0;
    tmp = (void*) CURR_LIST_NODE (tdl);
    FOR_EACH_LIST_ELMT (tmpTd, tdl)
    {
        if (tmpTd == td)
        {
            SET_CURR_LIST_NODE (tdl, tmp);
            return index;
        }
        else
            index++;
    }

    SET_CURR_LIST_NODE (tdl, tmp);
    return -1;

}  /* GetElmtIndex */





/*
 * Attempts to order the types in tdl from independent-->most depenedent
 * uses insertion after TypeDef that the given type def depends on.
 * Hoky - doesn't work very well - differing results depending on
 * initial order
  NO LONGER USED
void
AttemptDependencySort PARAMS ((tdl),
    TypeDefList *tdl)
{
    TypeDef *last;
    TypeDef *currTd;
    TypeDef **tdHndl;
    TypeDef *tdRef;
    AsnListNode *nextListNode;
    long int tdIndex;
    long int maxTdIndex;
    long int currIndex;

    if (LIST_EMPTY (tdl))
        return;

    last = (TypeDef*)LAST_LIST_ELMT (tdl);

    FOR_EACH_LIST_ELMT (currTd, tdl)
    {
        currTd->visited = FALSE;
    }

    SET_CURR_LIST_NODE (tdl, FIRST_LIST_NODE (tdl));
    currTd = (TypeDef*)CURR_LIST_ELMT (tdl);

    while (1)
    {
        nextListNode = NEXT_LIST_NODE (tdl);

        if (!currTd->visited)
        {
            currTd->visited = TRUE;
            maxTdIndex = -1;
            FOR_EACH_LIST_ELMT (tdRef, currTd->refList)
            {
                tdIndex = GetElmtIndex (tdRef, tdl);
                if (tdIndex > maxTdIndex)
                    maxTdIndex = tdIndex;
            }
        }

        currIndex = GetElmtIndex (currTd, tdl);

        if ((maxTdIndex >= 0) && (currIndex < maxTdIndex))
        {
            MoveAfter (currIndex, maxTdIndex, tdl);
        }

        if (currTd == last)
            break;

        SET_CURR_LIST_NODE (tdl, nextListNode);
        currTd = (TypeDef*)CURR_LIST_ELMT (tdl);
    }
}  AttemptDependencySort */



/*
 * Moves list node at currIndex to after Node at afterIndex
 * in the given list l.  Indexes start at 0 for the first elmt.
 * May confuse the 'curr' pointer of the list
  NO LONGER USED
void
MoveAfter PARAMS ((currIndex, afterIndex, l),
    unsigned long int currIndex _AND_
    unsigned long int afterIndex _AND_
    AsnList *l)
{
    void *tmp;
    AsnListNode *nodeToMove;
    AsnListNode *afterNode;
    int i;

    if ((l == NULL) ||
        (LIST_COUNT (l) <= currIndex) ||
        (LIST_COUNT (l) <= afterIndex))
    {
        fprintf (stderr,"Internal compiler error - index confusion in MoveAfter\n");
        return;
    }

    tmp = (void*) CURR_LIST_NODE (l);

    nodeToMove = l->first;
    for (i = 0; i < currIndex; i++)
        nodeToMove = nodeToMove->next;

    afterNode = l->first;
    for (i = 0; i < afterIndex; i++)
        afterNode = afterNode->next;

     pop out node to move
    if (nodeToMove->next)
        nodeToMove->next->prev = nodeToMove->prev;
    else
        l->last = nodeToMove->prev;

    if (nodeToMove->prev)
        nodeToMove->prev->next = nodeToMove->next;
    else
        l->first = nodeToMove->next;

    insert node to move after selected node
    nodeToMove->next = afterNode->next;
    nodeToMove->prev = afterNode;

    if (afterNode->next)
        afterNode->next->prev = nodeToMove;
    else
        l->last = nodeToMove;

    afterNode->next = nodeToMove;

} MoveAfter */
