/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * compiler/back_ends/c_gen/tag_util.c  - utilities for dealing with tags
 *
 * MS 92
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * INSERT_VDA_COMMENTS
 *
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/compiler/back-ends/Attic/tag-util.c,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: tag-util.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:27  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:39  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 18:15:28  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:26:07  rj
 * snacc_config.h and other superfluous .h files removed.
 *
 * Revision 1.1  1994/08/28  09:48:39  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <stdio.h>

#include "asn-incl.h"
#include "asn1module.h"
#include "mem.h"
#include "define.h"
#include "lib-types.h"
#include "c-gen/rules.h"
#include "c-gen/type-info.h"
#include "str-util.h"
#include "snacc-util.h"
#include "c-gen/util.h"
#include "tag-util.h"



/*
 * returns the tags for the given type (stops at next type definition).
 * if no tags have been grabbed yet and an untagged CHOICE is encountered,
 * all of the CHOICE's top level tags are returned and the stoleChoiceTags
 * flag is set.  If the type has no tags an empty list is returned, not
 * NULL.
 *
 * ASSUMES: tag list's and implicit flags have been adjusted according
 *          to module level IMPLICIT/EXPLICIT-TAGS and type level
 *          IMPLICIT/EXPLICIT tagging.
 *
 * EXAMPLE:
 *
 *   typeX ::= SEQUENCE                         SomeChoice ::= CHOICE
 *   {                                          {
 *         foo [0] INTEGER,                          [0] INTEGER,
 *         bar SomeChoice,                           [1] BOOLEAN,
 *         bell [1] IMPLICIT BOOLEAN,                [2] IA5String
 *         gumby [2] SomeChoice,                }
           poki SomeOtherChoice
 *   }
 *
 *  SomeOtherChoice ::= [APPLICATION 99] CHOICE { ....}
 *
 *  GetTags (foo's type) -->  CNTX 0, UNIV INTEGER_TAG_CODE   stoleChoiceTags = FALSE
 *  GetTags (bar)        -->  CNTX 0, CNTX 1, CNTX 2   (SomeChoice Elmt's first Tags)
 *                                                      stoleChoiceTags = TRUE
 *  GetTags (bell)       -->  CNTX 1    stoleChoiceTags = FALSE
 *  GetTags (gumby)      -->  CNTX 2    stoleChoiceTags = FALSE
 *  GetTags (poki)       -->  APPLICATION 99    stoleChoiceTags = FALSE
 *
 * MS 92/03/04 Added tag form information
 */
TagList*
GetTags PARAMS ((t, stoleChoiceTags),
    Type *t _AND_
    int *stoleChoiceTags)
{
    Tag *tag;
    TagList *tl;
    TagList *retVal;
    Tag *last;
    Tag  *tagCopy;
    Tag **tagHndl;
    int implicitRef;
    int stoleChoicesAgain;
    NamedType *e;

    tl = t->tags;
    if (tl != NULL)
        AsnListFirst (tl);

    retVal = (TagList*) AsnListNew (sizeof (void*));
    implicitRef = FALSE;
    *stoleChoiceTags = FALSE;

    for (;;)
    {
         /*
          * go through tag list local to this type if any
          */

        FOR_REST_LIST_ELMT (tag, tl)
        {
            tagCopy = (Tag*)Malloc (sizeof (Tag));
            memcpy (tagCopy, tag, sizeof (Tag));
            tagHndl = (Tag**)AsnListAppend (retVal);
            *tagHndl = tagCopy;

        }

        /*
         * follow tags of referenced types
         */

        if ((t->basicType->choiceId == BASICTYPE_LOCALTYPEREF) ||
             (t->basicType->choiceId == BASICTYPE_IMPORTTYPEREF))
        {
            if (!implicitRef)
                implicitRef = t->implicit;


            if (t->basicType->a.localTypeRef->link == NULL)
            {
                fprintf (stderr,"ERROR - unresolved type ref, cannot get tags for decoding>\n");
                break;
            }
            t = t->basicType->a.localTypeRef->link->type;
            tl = t->tags;

            if (tl != NULL)
            {
                AsnListFirst (tl); /* set curr ptr to first node */
                if ((!LIST_EMPTY (tl)) && implicitRef)
                {
                    AsnListNext (tl);
                    implicitRef = FALSE;
                }
            }

        }

        /*
         * if untagged choice and no tags found yet
         */
        else if ((t->basicType->choiceId == BASICTYPE_CHOICE) && (LIST_EMPTY (retVal)))
        {
            /*
             * Return list of top level tags from this choice
             * and set "stoleChoiceTags" bool param
             */
            if (implicitRef)
                fprintf (stderr,"ERROR - IMPLICITLY Tagged CHOICE\n");

            *stoleChoiceTags = TRUE;

            FOR_EACH_LIST_ELMT (e, t->basicType->a.choice)
            {
                stoleChoicesAgain = FALSE;
                tl = GetTags (e->type, &stoleChoicesAgain);

                if (tl == NULL)
                    break;

		AsnListFirst (tl);
                if (stoleChoicesAgain)
                {
                    FOR_EACH_LIST_ELMT (tag, tl)
                    {
                        tagCopy = (Tag*)Malloc (sizeof (Tag));
                        memcpy (tagCopy, tag, sizeof (Tag));
			tagHndl = (Tag**)AsnListAppend (retVal);
			*tagHndl = tagCopy;

                    }
                }
                else
                {
		  tag = (Tag*)FIRST_LIST_ELMT (tl);
		  tagCopy = (Tag*)Malloc (sizeof (Tag));
		  memcpy (tagCopy, tag, sizeof (Tag));
		  tagHndl = (Tag**)AsnListAppend (retVal);
		  *tagHndl = tagCopy;
                }
                FreeTags (tl);
            }

            break;  /* exit for loop */
        }

        else
            break; /* exit for loop */
    }


    if (!*stoleChoiceTags && (retVal != NULL) && !LIST_EMPTY (retVal))
    {
        last = (Tag*)LAST_LIST_ELMT (retVal);
        FOR_EACH_LIST_ELMT (tag, retVal)
        {
            tag->form = CONS;
        }
        last->form = LIBTYPE_GET_TAG_FORM (GetBuiltinType (t));
    }

    AsnListFirst (retVal);
    return retVal;

}  /* GetTags */


void
FreeTags PARAMS ((tl),
    TagList *tl)
{
    Tag *tag;
    AsnListNode *listNode;
    AsnListNode *ln;

    /* free tags */
    FOR_EACH_LIST_ELMT (tag, tl)
    {
        Free (tag);
    }

    /* free list nodes */
    for (ln = FIRST_LIST_NODE (tl); ln != NULL; )
    {
        listNode = ln;
	ln = ln->next;
        Free (listNode);
    }

    /* free list head */
    Free (tl);

}  /* FreeTags */

/*
 * Returns the number of tags that GetTags would return for
 * the same type.
 */
int
CountTags PARAMS ((t),
    Type *t)
{
    int tagCount;
    Tag *tag;
    TagList *tl;
    int implicitRef;
    int stoleChoicesAgain;
    NamedType *e;

    tl = t->tags;
    if (tl != NULL)
        AsnListFirst (tl);

    tagCount = 0;
    implicitRef = FALSE;

    for (;;)
    {
         /*
          * go through tag list local to this type if any
          */

        FOR_REST_LIST_ELMT (tag, tl)
        {
	    tagCount++;
        }

        /*
         * follow tags of referenced types
         */

        if ((t->basicType->choiceId == BASICTYPE_LOCALTYPEREF) ||
             (t->basicType->choiceId == BASICTYPE_IMPORTTYPEREF))
        {
            if (!implicitRef)
                implicitRef = t->implicit;


            if (t->basicType->a.localTypeRef->link == NULL)
            {
                fprintf (stderr,"ERROR - unresolved type ref, cannot get tags for decoding>\n");
                break;
            }
            t = t->basicType->a.localTypeRef->link->type;
            tl = t->tags;

            if (tl != NULL)
            {
                AsnListFirst (tl); /* set curr ptr to first node */
                if ((!LIST_EMPTY (tl)) && implicitRef)
                {
                    AsnListNext (tl);
                    implicitRef = FALSE;
                }
            }

        }
        else
            break;
    }

    return tagCount;

}  /* CountTags */


unsigned long int
TagByteLen PARAMS ((tagCode),
    unsigned long int tagCode)
{
    unsigned long int tagLen;

    if (tagCode < 31)
        tagLen = 1;
    else if (tagCode < 128)
        tagLen = 2;
    else if (tagCode < 16384)
        tagLen = 3;
    else if (tagCode < 2097152)
        tagLen = 4;
    else
        tagLen = 5;

    return tagLen;
} /* TagByteLen */



char*
Class2ClassStr PARAMS ((class),
    int class)
{
    switch (class)
    {
        case UNIV:
            return "UNIV";
            break;

        case APPL:
            return "APPL";
            break;

        case CNTX:
            return "CNTX";
            break;

        case PRIV:
            return "PRIV";
            break;

        default:
            return "UNKNOWN";
            break;
    }
} /* Class2ClassStr */



char*
Form2FormStr PARAMS ((form),
    BER_FORM form)
{
    switch (form)
    {
        case PRIM:
            return "PRIM";
            break;

        case CONS:
            return "CONS";
            break;

        default:
            return "UNKNOWN";
            break;
    }
} /* Form2FormStr */



char*
Code2UnivCodeStr PARAMS ((code),
    BER_UNIV_CODE code)
{
    switch (code)
    {
      case BOOLEAN_TAG_CODE:
        return "BOOLEAN_TAG_CODE";
        break;

      case INTEGER_TAG_CODE:
        return "INTEGER_TAG_CODE";
        break;

      case BITSTRING_TAG_CODE:
        return "BITSTRING_TAG_CODE";
        break;

      case OCTETSTRING_TAG_CODE:
        return "OCTETSTRING_TAG_CODE";
        break;

      case NULLTYPE_TAG_CODE:
        return "NULLTYPE_TAG_CODE";
        break;

      case OID_TAG_CODE:
        return "OID_TAG_CODE";
        break;

      case OD_TAG_CODE:
        return "OD_TAG_CODE";
        break;

      case EXTERNAL_TAG_CODE:
        return "EXTERNAL_TAG_CODE";
        break;

      case REAL_TAG_CODE:
        return "REAL_TAG_CODE";
        break;

      case ENUM_TAG_CODE:
        return "ENUM_TAG_CODE";
        break;

      case SEQ_TAG_CODE:
        return "SEQ_TAG_CODE";
        break;

      case SET_TAG_CODE:
        return "SET_TAG_CODE";
        break;

      case NUMERICSTRING_TAG_CODE:
        return "NUMERICSTRING_TAG_CODE";
        break;

      case PRINTABLESTRING_TAG_CODE:
        return "PRINTABLESTRING_TAG_CODE";
        break;

      case TELETEXSTRING_TAG_CODE:
        return "TELETEXSTRING_TAG_CODE";
        break;

      case VIDEOTEXSTRING_TAG_CODE:
        return "VIDEOTEXSTRING_TAG_CODE";
        break;

      case IA5STRING_TAG_CODE:
        return "IA5STRING_TAG_CODE";
        break;

      case UTCTIME_TAG_CODE:
        return "UTCTIME_TAG_CODE";
        break;

      case GENERALIZEDTIME_TAG_CODE:
        return "GENERALIZEDTIME_TAG_CODE";
        break;

      case GRAPHICSTRING_TAG_CODE:
        return "GRAPHICSTRING_TAG_CODE";
        break;

      case VISIBLESTRING_TAG_CODE:
        return "VISIBLESTRING_TAG_CODE";
        break;

      case GENERALSTRING_TAG_CODE:
        return "GENERALSTRING_TAG_CODE";
        break;

#ifdef VDADER_RULES

      case UNIVERSALSTRING_TAG_CODE:
        return "UNIVERSALSTRING_TAG_CODE";
        break;

      case BMPSTRING_TAG_CODE:
        return "BMPSTRING_TAG_CODE";
        break;

       default:
        { 
            /* if the universal type is not known then just return the
             * unvisersal tag code.  This is useful for defining new types
             * in local modules w/o having to modify the compiler.
             */
            static char retstring[3];
            sprintf(retstring, "%d", code); 
            return retstring; 
        }
#else

      default:
        return "UNKNOWN";
#endif

    }
} /* TagId2FormStr */
