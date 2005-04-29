/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * mslp_dat.c : Minimal SLP v2 DATable
 *
 * Version: 1.10
 * Date:    10/05/99
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
 /*
	Portions Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>		// for _POSIX_THREADS
#include <pthread.h>	// for pthread_*_t

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslp_dat.h"    /* Definitions for mslp_dat             */
#include "mslplib.h"     /* Definitions specific to the mslplib  */

static void remove_dae( DATable *pdat, struct sockaddr_in sin);

/* --------------------------------------------------------------------------- 
 * externally visible functions
 */

/*
 * delete_dat
 *
 *   Frees all internal state of the dat.
 *
 * Note:
 *   The DA Table cannot be used after this!
 */
EXPORT void dat_delete( DATable *pdat ) {

  if (pdat) {
    int i;
    for (i = 0; i < pdat->iSize; i++) {
      SLPFree(pdat->pDAE[i].pcScopeList);
    }

    SLPFree(pdat->pDAE);
    SLPFree(pdat);
  }
}

/*
 * init_dat
 *
 *   This function cleanly initializes a DATable.
 *
 * Note:
 *   The scope list, if there is one, is copied here (belonging to the datable)
 */
EXPORT DATable * dat_init() 
{
    DATable *pdat = (DATable*) safe_malloc(sizeof(DATable),0,0);
    
    pdat->iSize = 0;
    pdat->iMax  = DATINCR;
    pdat->pDAE  = (DAEntry*) safe_malloc(sizeof(DAEntry) * DATINCR,0,0);
    pdat->initialized = SLP_FALSE;
    
    LockGlobalDATable();

// moved preconfigured da parsing to SLPSystemConfiguration
    #ifdef EXTRA_MSGS  
    pdat->pcSASList = NULL;
    pdat->iSASListSz = 0;
    #endif /* EXTRA_MSGS */
    pdat->initialized = SLP_TRUE;
        
    UnlockGlobalDATable();
    
    return pdat;
}

/*
 * dat_daadvert_in
 * 
 *
 * Returns:
 *   1 if the Advert is new and stored services should be forwarded to it.
 *   0 otherwise.
 *   if negative, it corresponds to a SLPInternalError.
 */
EXPORT int dat_daadvert_in(DATable *ignore, struct sockaddr_in sin, 
		    const char *pcScopeList, long lBootTime) 
{
    DATable* 	pdat = GetGlobalDATable();
    int 		retval = 0;
   
    if (!pdat || !pcScopeList ) return SLP_PARAMETER_BAD;
    
    if (SLPGetProperty("com.sun.slp.noDA") && !SDstrcasecmp(SLPGetProperty("com.sun.slp.noDA"),"true")) 
		return 0;
		
    if (lBootTime == 0) 
        remove_dae(pdat,sin);
    else 
    {
        int i,found = 0;
    
        LockGlobalDATable();
        
        for (i = 0; i < pdat->iSize; i++)
        {
            if (pdat->pDAE[i].sin.sin_addr.s_addr == sin.sin_addr.s_addr) 
            {
                found = 1;
                /* update the entry */
                if (pdat->pDAE[i].lBootTime < lBootTime || strcmp(pcScopeList, pdat->pDAE[i].pcScopeList) != 0) 
                { /* da rebooted or changed its scopelist */
                    pdat->pDAE[i].lBootTime = lBootTime;
                    free(pdat->pDAE[i].pcScopeList);
                    pdat->pDAE[i].pcScopeList = list_pack(pcScopeList);
                    pdat->pDAE[i].iStrikes = 0;		// reset this
                    retval = 1; /* indicates a new entry */
                    
                    if ( pdat->pDAE[i].iDAIsScopeSponser )
                    {
                        // this DA told us what scope to use, we need to make sure that our default registration scope
                        // hasn't changed.
                    }
                    
#ifdef ENABLE_SLP_LOGGING
					SLP_LOG (SLP_LOG_MSG, "Updating DA [%s], in list with scopelist: %s", inet_ntoa(sin.sin_addr), pcScopeList );
#endif
                }
                break;
            }
        }

        if (found == 0) 
        { /* add the daentry */
    
            if ((pdat->iSize+1) == pdat->iMax) 
            { /* resize the table if needed */
                DAEntry *pdae = (DAEntry*)
                safe_malloc((pdat->iMax+DATINCR)*sizeof(DAEntry),(char*)pdat->pDAE,
                pdat->iMax*sizeof(DAEntry));
                free(pdat->pDAE);
                pdat->pDAE = pdae;
                pdat->iMax += DATINCR;
            }

            pdat->pDAE[pdat->iSize].sin.sin_addr.s_addr = sin.sin_addr.s_addr;
            pdat->pDAE[pdat->iSize].sin.sin_family = AF_INET;
            pdat->pDAE[pdat->iSize].sin.sin_port   = htons(SLP_PORT);
        
            pdat->pDAE[pdat->iSize].pcScopeList = list_pack(pcScopeList);      
            pdat->pDAE[pdat->iSize].lBootTime = lBootTime;
            pdat->pDAE[pdat->iSize].iStrikes = 0;
            pdat->pDAE[pdat->iSize].iDAIsScopeSponser = false;
            pdat->iSize++;
            retval = 1; /* indicates a new entry */
                    
#ifdef ENABLE_SLP_LOGGING
            SLP_LOG( SLP_LOG_MSG, "Adding DA [%s], to list with scopelist: %s", inet_ntoa(sin.sin_addr), pcScopeList );
#endif
        }
        UnlockGlobalDATable();
    }
    
    return retval;
}

#ifdef EXTRA_MSGS
/*
 * dat_saadvert_in
 *
 *   This is called when active SA discovery is used and SAAdverts
 *   have been detected.
 *
 *     pdat           The DATable which stores the total SA scope list.
 *     pcScopeList    The scope list of the individual discovered SA.
 *     pcAttrlist     The attribute list of the discovered SA.
 *                    This last parameter is currently ignored.
 *
 * Side Effect:
 *   The DATable is updated with the new scopes, if any, in the sa
 *   advert, for the pcSASList field.
 */
EXPORT void dat_saadvert_in(DATable *ignore,
			    const char *pcScopeList,
			    const char *pcAttrlist) {
  DATable* pdat = GetGlobalDATable();
  
  if (!pcScopeList) return;
  if (pdat->pcSASList == NULL) {
    char *pcPacked = list_pack(pcScopeList);
    int i = strlen(pcPacked);
    pdat->pcSASList = safe_malloc(i+LISTINCR,pcPacked,i);
    pdat->iSASListSz = LISTINCR+i;
    free(pcPacked);
  } else {
    list_merge(pcScopeList, &(pdat->pcSASList), &(pdat->iSASListSz), CHECK);
  }
}
#endif

EXPORT SLPInternalError dat_get_da(const DATable *ignore, const char *pcScopeList,
	       struct sockaddr_in *psin) 
{

    int 		i, found = 0;
    DATable*	pdat = GetGlobalDATable();	// ignore what they pass in, only reference the globaly defined table
    SLPInternalError	error = SLP_OK;
    
    if (!pdat || !pcScopeList || !psin) return SLP_PARAMETER_BAD;
    
    LockGlobalDATable();
    
    psin->sin_addr.s_addr = 0L;
    
    /* Testing mode!  Return no DA if this property is set */
    if (SLPGetProperty("com.sun.slp.noDA") && !SDstrcasecmp(SLPGetProperty("com.sun.slp.noDA"),"true")) 
    {
        error = SLP_OK;
    }
    else
    {
        for (i = 0; i < pdat->iSize; i++)
        {
            if (list_intersection(pcScopeList,pdat->pDAE[i].pcScopeList)) 
            {
                *psin = pdat->pDAE[i].sin;
                found = 1;
                break;
            }
        }
        
        if ( pdat->iSize && !found )
        {
            // we have DA's discovered but no nothing about the scope they are asking for...
            error =  SLP_SCOPE_NOT_SUPPORTED;
        }
    }

    UnlockGlobalDATable();
    
    return error;
}

#define kNumberOfStrikesAllowed	3
EXPORT void dat_boot_off_struck_out_das( void )
{
    DATable* 	pdat = GetGlobalDATable();
    int			i;
#ifdef ENABLE_SLP_LOGGING
	int			initialSize = pdat->iSize;
#endif    
    LockGlobalDATable();

#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_MSG, "dat_boot_off_struck_out_das called, %d DAs in list.",initialSize);
#endif
    for (i = 0; i < pdat->iSize; i++)
    {
        if ( pdat->pDAE[i].iStrikes > kNumberOfStrikesAllowed ) 
        {
#ifdef ENABLE_SLP_LOGGING
            SLP_LOG( SLP_LOG_MSG, "dat_boot_off_struck_out_das called, removing DA [%s] from list as it has too many strikes against it.",inet_ntoa(pdat->pDAE[i].sin.sin_addr));
#endif
            remove_dae(pdat,pdat->pDAE[i].sin); /* struck out */
            break;
        } 
    }
    
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_MSG, "dat_boot_off_struck_out_das struck out, %d DAs.",initialSize-(pdat->iSize));
#endif
    UnlockGlobalDATable();
}

EXPORT SLPInternalError dat_strike_da(DATable *ignore, struct sockaddr_in sin) 
{
    int 		i;
    DATable* 	pdat = GetGlobalDATable();
    
    if (!pdat)
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG(SLP_LOG_DEBUG, "dat_strike_da called but we have no global DATable" );
#endif    
        return SLP_PARAMETER_BAD;
    }
    
    LockGlobalDATable();

#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_MSG, "dat_strike_da called, %d DAs in list.",pdat->iSize );
#endif
    for (i = 0; i < pdat->iSize; i++)
    {
        if (pdat->pDAE[i].sin.sin_addr.s_addr == sin.sin_addr.s_addr) 
        {
            // only strike out a non-preconfigured DA.  Otherwise just add the strikes and move it
            // to the end of the table
            if (!OnlyUsePreConfiguredDAs() && pdat->pDAE[i].iStrikes++ > kNumberOfStrikesAllowed) 
            {
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_MSG, "dat_strike_da called, removing DA [%s] from list as it has too many strikes against it.",inet_ntoa(sin.sin_addr) );
#endif
                remove_dae(pdat,sin); /* struck out */
                break;
            } 
            else 
            {
                DAEntry daeTemp = pdat->pDAE[i];

#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_MSG, "dat_strike_da called, adding a strike to DA [%s] (%d strikes).",inet_ntoa(sin.sin_addr), pdat->pDAE[i].iStrikes );
#endif 
                if (i < (pdat->iSize-1)) 
                { /* if not the last item on the list */
                    memmove(&(pdat->pDAE[i]),&(pdat->pDAE[i+1]),
                        sizeof(DAEntry) * (pdat->iSize - i));
                    /* put struck dae at end of list */
                    pdat->pDAE[pdat->iSize-1] = daeTemp;
                    break;
                }
            }
        }
    }
    
    UnlockGlobalDATable();
    
    return SLP_OK;
}

EXPORT SLPInternalError dat_update_da_scope_sponser_info(struct sockaddr_in sin, bool isScopeSponser) 
{
    int 		i;
    DATable* 	pdat = GetGlobalDATable();
    
    if (!pdat)
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG(SLP_LOG_DEBUG, "dat_update_da_scope_sponser_info called but we have no global DATable" );
#endif    
        return SLP_PARAMETER_BAD;
    }
    
    LockGlobalDATable();

#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_MSG, "dat_update_da_scope_sponser_info called, %d DAs in list.",pdat->iSize );
#endif
    for (i = 0; i < pdat->iSize; i++)
    {
        if (pdat->pDAE[i].sin.sin_addr.s_addr == sin.sin_addr.s_addr) 
        {
            pdat->pDAE[i].iDAIsScopeSponser = isScopeSponser;
        }
    }
    
    UnlockGlobalDATable();
    
    return SLP_OK;
}

/*
 * dat_get_scopes
 *
 *   If there is a net.slp.useScopes property, these scopes are
 *   returned.  Otherwise, starting with the scope list of the first
 *   element, each DA's list is merged in (looking individually at
 *   each of the already-obtained scopes to see if there is a list
 *   intersection with the scopes of each of the subsequent DAs.)
 *
 *   If there are no configured scopes and no DAs, and SA discovery
 *   functionality has been compiled in, then SA discovery will be
 *   used to 
 *
 *    slph         This is used for SA discovery if needed.
 *    pcTypeHint   This is used for SA discovery, to optimize it.
 *    pdat         The da table which is checked for a scope list.
 *    ppcScopes    The scope list to return (an OUT parameter.)
 *
 * Return:
 *
 *   Error if any.
 *   *ppcScopes is set to the scope list obtained.  If no scopes have
 *   been found, this parameter will be set to NULL.
 *
 * Side Effects:
 *
 *   The string list returned in ppcScopes must be freed by the caller.
 *
 * Notes:
 *
 *   SA discovery will be implemented later...
 *
 */
EXPORT SLPInternalError dat_get_scopes(SLPHandle slph,
			       const char *pcTypeHint,
			       const DATable *ignore,
			       char **ppcScopes)
{
    DATable* pdat = GetGlobalDATable();
    char *pcList, *pcScan;
    int iListLen = LISTINCR;
    int  i;
    
    LockGlobalDATable();
        
    *ppcScopes = NULL;
    
    if (!pdat || !ppcScopes) 
        return SLP_PARAMETER_BAD;
  
    if (SLPGetProperty("net.slp.useScopes") != NULL) 
    {
        *ppcScopes = strdup(SLPGetProperty("net.slp.useScopes"));
        return SLP_OK;
    }

#ifdef EXTRA_MSGS  
    if (slph != NULL && pdat->iSize == 0) 
    {
        active_sa_discovery(slph,pcTypeHint);
        if (pdat->pcSASList) 
        {
            int iLen = strlen(pdat->pcSASList);
            *ppcScopes = safe_malloc(iLen+1,pdat->pcSASList,iLen);
        }
        
        return SLP_OK;
    }
#endif /* EXTRA_MSGS */
  
    /* send a merged list of scopes from the DATable */
    if (pdat->iSize == 0) 
        return SLP_OK;
    
    iListLen += strlen(pdat->pDAE[0].pcScopeList);
    pcList = safe_malloc(iListLen, (char*)pdat->pDAE[0].pcScopeList,
                        iListLen-LISTINCR);
    if( !pcList ) return SLP_INTERNAL_SYSTEM_ERROR;
    
    if (pdat->iSize > 1) 
    {
        for (i = 1; i < pdat->iSize; i++) 
        {
            pcScan = list_pack(pdat->pDAE[i].pcScopeList);
            list_merge(pcScan,&pcList,&iListLen,CHECK);
            free(pcScan);
        }
    }
    
    *ppcScopes = pcList;
    
    UnlockGlobalDATable();
    
    return SLP_OK;
}


/* ---------------------------------------------------------------------------
 * internal functions to support dae functions
 */

/*
 * remove_dae
 *
 *  Simply frees the scope list and shifts the entire datable down in the
 *  array representation and decrements the total size.
 *
 */
static void remove_dae(DATable *pdat, struct sockaddr_in sin) {
  int i;
  for (i = 0; i < pdat->iSize; i++) {
    if (pdat->pDAE[i].sin.sin_addr.s_addr == sin.sin_addr.s_addr) {
      free(pdat->pDAE[i].pcScopeList);
      memmove(&(pdat->pDAE[i]),&(pdat->pDAE[i+1]),
	      sizeof(DAEntry) * (pdat->iSize - i));
      (pdat->iSize)--;
      break;
    }
  }
}

