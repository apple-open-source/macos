/*
 * mslp_list.c : Minimal SLP v2 string list manipulation utilities
 *
 *  All reads, writes, message composing and decomposing is done here.
 *
 * Version: 1.7
 * Date:    03/30/99
 *
 * Licensee will, at its expense,  defend and indemnify Sun Microsystems,
 * Inc.  ("Sun")  and  its  licensors  from  and  against any third party
 * claims, including costs and reasonable attorneys' fees,  and be wholly
 * responsible for  any liabilities  arising  out  of  or  related to the
 * Licensee's use of the Software or Modifications.   The Software is not
 * designed  or intended for use in  on-line  control  of  aircraft,  air
 * traffic,  aircraft navigation,  or aircraft communications;  or in the
 * design, construction, operation or maintenance of any nuclear facility
 * and Sun disclaims any express or implied warranty of fitness  for such
 * uses.  THE SOFTWARE IS PROVIDED TO LICENSEE "AS IS" AND ALL EXPRESS OR
 * IMPLIED CONDITION AND WARRANTIES, INCLUDING  ANY  IMPLIED  WARRANTY OF
 * MERCHANTABILITY,   FITNESS  FOR  WARRANTIES,   INCLUDING  ANY  IMPLIED
 * WARRANTY  OF  MERCHANTABILITY,  FITNESS FOR PARTICULAR PURPOSE OR NON-
 * INFRINGEMENT, ARE DISCLAIMED. IN NO EVENT WILL SUN BE LIABLE HEREUNDER
 * FOR ANY DIRECT DAMAGES OR ANY INDIRECT, PUNITIVE, SPECIAL, INCIDENTAL
 * OR CONSEQUENTIAL DAMAGES OF ANY KIND.
 *
 * (c) Sun Microsystems, 1998, All Rights Reserved.
 * Author: Erik Guttman
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"

/*
 * list_intersection
 *
 * return:
 *   1 if there is an overlap, 0 otherwise.
 */
EXPORT int list_intersection(const char *pcL1, const char *pcL2)
{
    int i1 = 0, i2 = 0;
    char *pcS1, *pcS2;
    char c;
    
    if (!pcL1 || !pcL2) 
    {
        LOG(SLP_LOG_ERR,"list_intersection: got NULL value as a parameter!");
        return 0;
    }
    
    if (*pcL1 == '\0' && *pcL2 == '\0') 
        return 1;
    
    for ( pcS1=get_next_string(",",pcL1,&i1,&c); pcS1; pcS1=get_next_string(",",pcL1,&i1,&c) ) 
    {
        i2 = 0;
        for (pcS2=get_next_string(",",pcL2,&i2,&c); pcS2;pcS2=get_next_string(",",pcL2,&i2,&c))
        {
            int result = SDstrcasecmp(pcS1,pcS2);
            SLPFree(pcS2);
            if (result==0)
            {
                SLPFree(pcS1);
                return 1;
            }
        }
        
        SLPFree(pcS1);
    }
    
    return 0;
}

/*
 * list_merge
 *
 *   This function will build a list of unique elements.  It assumes
 *   that the items arriving with pcNewList are packed, for efficiency.
 *   If the list grows too large, it will be grown to take in the new
 *   string and 'breathing room' for further expansion.
 *
 *     pcNewList    The new list to merge in with the old one.
 *     ppcList      A pointer to the buffer with the list to be built onto.
 *     piListLen    The max size of the list list to be build onto.
 *     iCheck       If this is 0, don't check for duplicates.
 *
 * Returns:  None.
 * 
 * Side Effects:
 *
 *   A call to this function can result in *ppcList (the buffer) being
 *   reallocated and *piListLen (the max buffer size) being expanded.)
 */
EXPORT void list_merge(const char *pcNewList, char **ppcList, int *piListLen, int iCheck)
{
    int offset = 0;
    char c, *pcScope;
    int initial = 0;
    
    if (!pcNewList) 
        return;
    
    if (!ppcList)
        return;
    
    if (!*ppcList)
    {
        *ppcList = safe_malloc(strlen(pcNewList)+1,pcNewList,strlen(pcNewList));
        *piListLen = strlen(*ppcList);
        return;
    }
    
    if (*piListLen == 0) 
        initial = 1; /* suppresses initial comma in new lists */
    
    if (!iCheck) 
    {
        int iSLen = strlen(pcNewList);
        if ((iSLen + (int) strlen(*ppcList)+1) >= *piListLen) 
        { /* too big? */
            char *pcOld = *ppcList;
            *piListLen += iSLen + LISTINCR;
            *ppcList = safe_malloc(*piListLen,*ppcList,strlen(*ppcList));
            SLPFree(pcOld);
        }
        
        if (!initial) 
            slp_strcat(*ppcList,",");
            
        slp_strcat(*ppcList,pcNewList);
        return;
    }
    
    while((pcScope = get_next_string(",",pcNewList,&offset,&c))) 
    {
        if (!list_intersection(pcScope,*ppcList)) 
        {
            int iSLen = strlen(pcScope);
        
            if ((iSLen + (int) strlen(*ppcList)+1) >= *piListLen) 
            { /* too big? */
                char *pcOld = *ppcList;
                *piListLen += iSLen + LISTINCR;
                *ppcList = safe_malloc(*piListLen,*ppcList,strlen(*ppcList));
                SLPFree(pcOld);
            }
    
            if (initial != 1) 
                slp_strcat(*ppcList,","); /* supress initial comma */
        
            slp_strcat(*ppcList,pcScope); /* append the scope not already on list */
        }
        
        SLPFree(pcScope);
    }   
}

/*
 * list_subset
 *
 *    Compares two lists.  All the elements in the first list must be
 *    in the second and neither may be empty lists.
 *
 *  pcSub    The list which must have all elements in pcSuper
 *  pcSuper  The list which must contain all or more elements than in pcSub
 *
 * Returns:
 *    0 if not a subset, 1 if it is a subset.
 *
 * Side effects:
 *    None.
 */
EXPORT int list_subset(const char *pcSub, const char *pcSuper) {

  int offset = 0;
  char c, *pcScope;

  if (!pcSub || !pcSuper ||                          /* either are NULL */
      (pcSub[0] == '\0' && pcSuper[0] != '\0')) {    /* sub empty, super not */
    return 0;
  }
     
  while((pcScope = get_next_string(",",pcSub,&offset,&c))) {
    if (!list_intersection(pcScope,pcSuper)) {
      SLPFree(pcScope);
      return 0;
    }
    SLPFree(pcScope);
  }

  return 1;
}

/*
 * Take any extraneous spaces out of a string or string list for the
 * sake of comparison.
 *
 * The function is smart enough to deal with attribute lists too.
 *
 * Will create a copy of a list, and pack the values into it, returning
 * that one. 
 */
EXPORT char * list_pack(const char *pc) {

  const char *pcSrc;
  char *pcTemp,  *pcDest;
  
//  if (!pc) return NULL;
  if ( !pc || !*pc) return safe_malloc(1,0,0);
  pcTemp = safe_malloc(strlen(pc)+1,0,0); /* clone */
  pcDest = pcTemp;
  pcSrc  = pc;
  
  while(*pcSrc) {
    char *pcStart = NULL;
    /* initial space */
    while (*pcSrc && isspace(*pcSrc)) 
      pcSrc++; /* advance past the initial space and do not copy */

    if (*pcSrc == ',' || *pcSrc == '(' || *pcSrc == ')' || *pcSrc == '=') {
      *pcDest++ = *pcSrc++;
      continue;
    }
    
    pcStart = NULL;
    /* nonspace value */
    while (*pcSrc) {
      if (*pcSrc == ',' || *pcSrc == '(' || *pcSrc == ')' || *pcSrc == '=') {
	if (pcStart) {
	  *--pcDest = *pcSrc++; /* overwrite terminal 'space' */
	  pcDest++;
	  pcStart = NULL;
	} else {
	  *pcDest++ = *pcSrc++;  
	}
	break;
      }
      if (*pcSrc && !isspace(*pcSrc)) {
	*pcDest++ = *pcSrc++;
	pcStart = NULL; /* we might have several series of ' ' */
      } else if (pcStart == NULL) {
	/* only copy ONE space internal to values */
	/* we also have to save where it starts so that trailing
	   spaces can be eliminated before the end of the string
	   or the end of terms (ie. followed by a ',' '(' ')' or '=') */
	pcStart = pcDest; 
	*pcDest++ = ' '; /* don't use pcDest as it might be a CR or HT */
	pcSrc++;
	while (*pcSrc && isspace(*pcSrc)) pcSrc++; /* do not copy */
      } else {
	pcSrc++;
      }
    }
    if (!*pcSrc) {
      /* there has been a NULL in the Src stream in an item */
      if (pcStart) { 
        *--pcDest = '\0'; 
      }
      break;
    }

  }

  return pcTemp;
}

/*
 * Take the element out of a list.
 *
 * This function is intended for scope lists.
 *
 * Will create a copy of a list without the element.  If the element
 * doesn't reside in the list, it will return null. 
 */
EXPORT char * list_remove_element(const char *list, const char *element) 
{
    int			elementLen, newListLen;
    char		tempElem[1024];
    char*		curPtr;
    char*		newList = NULL;
    
    if ( !element || !list || strlen(list) < strlen(element) )
        return NULL;
        
    elementLen = strlen(element);
    
    strcpy( tempElem, "," );
    slp_strcat( tempElem, element );
    slp_strcat( tempElem, "," );
    
    curPtr = strstr( list, tempElem );
    
    if ( curPtr )
    {
        // ok, we found element within the list
        newListLen = strlen(list)-(strlen(tempElem)-1);		// not including null terminator
        newList = (char*)malloc(newListLen+1);
        memcpy( newList, list, curPtr-list );
        
        memcpy( newList+(curPtr-list), curPtr+strlen(tempElem)-1, strlen(list)-((curPtr-list)+strlen(tempElem)-1) );	// whew, that's ugly
        newList[newListLen] = '\0';
    }
    else if ( memcmp( list, &tempElem[1], strlen(tempElem)-1 ) == 0 )
    {
        // the element at the beginning of the list
        newListLen = strlen(list)-(strlen(tempElem)-1);		// not including null terminator
        newList = (char*)malloc(newListLen+1);				// including terminator
        memcpy( newList, list+(strlen(tempElem)-1), newListLen );
        newList[newListLen] = '\0';
    }
    else if ( memcmp( &list[strlen(list)-(strlen(tempElem)-1)], tempElem, strlen(tempElem)-1 ) == 0 )
    {
        // the element at the end of the list
        newListLen = strlen(list)-(strlen(tempElem)-1);		// not including null terminator
        newList = (char*)malloc(newListLen+1);				// include terminator
        memcpy( newList, list, newListLen );
        newList[newListLen] = '\0';
    }
    else if ( strcmp( list, element ) == 0 )		
    {
        // list only contains element
        newList = (char*)malloc(1);
        newList[0] = '\0';
    }
    
    return newList;
}



