/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * mslp_prefs.c : Hash table for mini slpv2 preference storage.
 *
 *   Contains code for an in memory associative array.
 *
 * Version: 1.8 
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
 * (c) Sun Microsystems, All Rights Reserved, 1998.
 * Author: Erik Guttman
 */
 /*
	Portions Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>		// for _POSIX_THREADS
#include <pthread.h>	// for pthread_*_t

#include "mslp_sd.h"  /* System dep. / source compatibility  */
#include "slp.h"      /* SLP API supported features          */
#include "mslp.h"     /* Definitions for mslp, shared        */

static unsigned int string_hash(const char *pcKey);
static void free_bucket(MslpHashbucket *pb);
static MslpHashbucket * new_bucket(const char *pcKey, const char *pcVal);
void mslp_file_param_add(const char *pcKey, const char *pcVal, void *pvParam);
void mslp_log_params(const char *pcKey, const char *pcVal, void *pvParam);

pthread_mutex_t	gHashTableLock;
int				gHashTableLockNotInitialized = 1;

void mslp_hash_write(MslpHashtable *ph, const char *pcFileName) 
{
    FILE *fp;
    char buf[kMaxSizeOfParam];
    
    if (ph == NULL)
        LOG(SLP_LOG_ERR,"mslp_hash_write called without an initialized hash table");
    
    if (pcFileName == NULL)
        return;
    
    fp = fopen(pcFileName,"w");
    
    if (fp == NULL) 
    {
        mslplog(SLP_LOG_MSG,"mslp_hash_write: Could not open config file",pcFileName);
        return;
    }
    
    mslp_hash_do( ph, mslp_file_param_add, fp );		// iterate over all hash values and write to file
    
    while (fgets(buf,kMaxSizeOfParam,fp) != NULL) 
    {
        char *pcKey,*pcVal,cDelim;
        int   offset = 0;
        
        if (buf[0] == '\n' || buf[0] == '\0' || buf[0] == '#' || buf[0] == ';')
            continue;
    
        pcKey = get_next_string("=",buf,&offset,&cDelim);
        pcVal = get_next_string("=",buf,&offset,&cDelim);
        if (pcKey == NULL || pcVal == NULL) 
        	continue;
        else
            mslp_hash_add(ph,pcKey,pcVal);
    }

    fclose(fp);
}

void mslp_file_param_add(const char *pcKey, const char *pcVal, void *pvParam) {
  FILE *fpDest = (FILE *) pvParam;

  if (pcVal[0] == '\0') {
    fprintf(fpDest,"%s\n",pcKey);
  } else {
    fprintf(fpDest,"%s=%s\n",pcKey,pcVal);
  }
}

void mslp_log_params(const char *pcKey, const char *pcVal, void *pvParam) 
{
    if ( !pcKey )
        SLP_LOG( SLP_LOG_ERR, "mslp_log_params was passed in a null key!" );
    else if (!pcVal || pcVal[0] == '\0') 
        SLP_LOG( SLP_LOG_STATE, "%s\n", pcKey );
    else
        SLP_LOG( SLP_LOG_STATE, "%s=%s", pcKey, pcVal );
}

void mslp_hash_read(MslpHashtable *ph, const char *pcFileName) 
{    
    FILE *fp;
    char buf[kMaxSizeOfParam];
    
    if (ph == NULL) {
        LOG(SLP_LOG_FAIL,"mslp_hash_read called without an initialized hash table");
    }
    
    if (pcFileName == NULL) return;
    
    fp = fopen(pcFileName,"r");
    
    if (fp == NULL) {
        mslplog(SLP_LOG_MSG,"mslp_hash_read: Could not open config file",pcFileName);
        return;
    }
    
    while (fgets(buf,kMaxSizeOfParam,fp) != NULL) {
    
        char *pcKey,*pcVal,cDelim;
        int   offset = 0;
        
        if (buf[0] == '\n' || buf[0] == '\0' || buf[0] == '#' || buf[0] == ';')
        continue;
    
        pcKey = get_next_string("=",buf,&offset,&cDelim);
        pcVal = get_next_string("=",buf,&offset,&cDelim);
        if (pcKey == NULL || pcVal == NULL) {
        continue;
        } else {
        mslp_hash_add(ph,pcKey,pcVal);
        }
        SLPFree(pcKey);
        SLPFree(pcVal);
    }
    
    fclose(fp);
}

int  mslp_conf_int(MslpHashtable *ph, const char *pcKey, int index, int iDefault) 
{
    const char *pcVal = mslp_hash_find(ph,pcKey);
    int loop, offset = 0, result;
    char *pcIter = NULL, cDelim;
    
    if (pcVal == NULL) 
        return iDefault;
        
    if (!SDstrcasecmp(pcVal,"true")) 
        return 1;
        
    if (!SDstrcasecmp(pcVal,"false")) 
        return 0;
    
    for (loop = 0; loop <= index; loop++) 
    {
        if (pcIter != NULL) 
            SLPFree(pcIter);

        pcIter = get_next_string(",",pcVal,&offset,&cDelim);

        if (pcIter == NULL) 
            return iDefault;
    }

    if (pcIter != NULL) 
    {
		char*	endPtr = NULL;
        result = strtol(pcIter,&endPtr,10);

        SLPFree(pcIter);
        return result;
    }
    
    return iDefault;  
}

EXPORT MslpHashtable * mslp_hash_init() 
{
    MslpHashtable *ph = (MslpHashtable *)safe_malloc(sizeof(MslpHashtable),NULL,0);
    
    ph->bucket = (MslpHashbucket **)safe_malloc(NUMBUCKETS * sizeof(MslpHashbucket*),NULL,0);
    
    memset(ph->bucket,0,NUMBUCKETS * sizeof(MslpHashbucket*));
    
    if ( gHashTableLockNotInitialized )
    {
        pthread_mutex_init (&gHashTableLock, NULL) ;
        gHashTableLockNotInitialized = 0;
    }
    
    return ph;
}

#ifdef EXTRA_MSGS
/*
 * mslp_hash_free
 *
 *   Free the hash table.
 *
 * Parameters:    ph   Hash table.
 * Results:       None.
 * Side Effects:  The hash table is freed. 
 */
EXPORT void mslp_hash_free(MslpHashtable *ph) 
{
    int i;
    MslpHashbucket *phb = NULL;
    
    if (ph) 
    {
        pthread_mutex_lock( &gHashTableLock );

        for (i=0;i<NUMBUCKETS; i++) 
        {
            if (ph->bucket[i]) 
            {
                for (phb = ph->bucket[i]; phb; ) 
                {
                    MslpHashbucket *phbTemp = phb->pBucketNext;
                    free_bucket(phb);
                    phb = phbTemp;
                }
                ph->bucket[i] = NULL;
            }
        }
        
        SLPFree(ph->bucket);
        SLPFree(ph);

        pthread_mutex_unlock( &gHashTableLock );

    }
}

/*
 * mslp_hash_do
 *
 *   Call the supplied function with the key and value and opaque parameter
 *   for each element in the hash function.
 *
 * Parameters:
 *
 *   ph       A hash table.
 *   dofun    A function to call.
 *   pvParam  An opaque parameter.
 *
 * Results:
 *
 *   None.
 *
 * Side effects:
 *
 *   None.
 */
EXPORT void mslp_hash_do(MslpHashtable *ph, MslpHashDoFun dofun,void *pvParam)
{
    int i;
    MslpHashbucket *phb = NULL;
    
    if (ph) 
    {
        pthread_mutex_lock( &gHashTableLock );

        for (i=0;i<NUMBUCKETS; i++) 
        {
            if (ph->bucket && ph->bucket[i]) 
            {
                for (phb = ph->bucket[i]; phb; phb = phb->pBucketNext) 
                {
                    dofun(phb->pcKey,phb->pcVal,pvParam);
                }
            }
        }

        pthread_mutex_unlock( &gHashTableLock );

    }
}

#endif /* EXTRA_MSGS */

EXPORT const char 
*mslp_hash_find(MslpHashtable *ph, const char *pcKey)
{
    unsigned int 	iBucket = string_hash(pcKey) % NUMBUCKETS;
    char*			value = NULL;
    
    if (ph == NULL)
    {
        LOG(SLP_LOG_FAIL,"mslp_hash_find called without an initialized ht");
        return NULL;
    }  
    
    if (ph->bucket[iBucket] == NULL)
    {
        return NULL;
    } 
    else 
    {
        MslpHashbucket *pb = ph->bucket[iBucket];    
    
        pthread_mutex_lock( &gHashTableLock );

        /* traverse the linked list till we find the entry */
        while (pb && pb->pcKey) 
        {
            if (!SDstrcasecmp(pcKey,pb->pcKey))
            {
                value = pb->pcVal;
                break;
            } 
            else 
            {
                pb = pb->pBucketNext;
            }
        }     
    
        pthread_mutex_unlock( &gHashTableLock );
    }

    return value;
}

EXPORT void mslp_hash_add(MslpHashtable *ph, const char *pcKey, const char *pcVal) 
{
    unsigned int	iBucket = string_hash(pcKey) % NUMBUCKETS;
	Boolean			found = false;
	
    if (ph == NULL)
        LOG(SLP_LOG_FAIL,"mslp_hash_add called without an initialized ht");
    
    if (pcKey == NULL) 
        return;
    
    pthread_mutex_lock( &gHashTableLock );

    if (ph->bucket[iBucket] == NULL) 
    {
        if (pcVal)
        {           
            ph->bucket[iBucket] = new_bucket(pcKey,pcVal);
        }
    } 
    else
    { 
        MslpHashbucket *pb = ph->bucket[iBucket];
        MslpHashbucket *pbPrev = pb;
    
        while (pb) 
        {        
            if (!SDstrcasecmp(pcKey,pb->pcKey)) 
            {        
                found = true;
				if (pcVal == NULL) 
                { /* delete an entry */    
                    if (pb == ph->bucket[iBucket]) 
                    { /* special initial case */
                        ph->bucket[iBucket] = pb->pBucketNext;
                    } 
                    else 
                    {
                        pbPrev->pBucketNext = pb->pBucketNext;
                    }

                    free_bucket(pb);
                    pb = NULL;
                    break;
                } 
                else 
                {             /* update an entry */
                    SLPFree(pb->pcVal);
                    pb->pcVal = safe_malloc(strlen(pcVal)+1,pcVal,strlen(pcVal));
                    
                    break;
                }
            } 
            else 
            {    
                pbPrev = pb;
                pb = pb->pBucketNext;    
            }
        }
    
        /* if we get here there is no entry in the Bucket yet.  Add it. */
        if ( pcVal && !found )
            pbPrev->pBucketNext = new_bucket(pcKey,pcVal);
    }
    
    pthread_mutex_unlock( &gHashTableLock );	// don't forget to clear this!
}

static void free_bucket(MslpHashbucket *pb)
{
    SLPFree(pb->pcKey);
	pb->pcKey = NULL;
	
    SLPFree(pb->pcVal);
	pb->pcVal = NULL;
	
    SLPFree(pb);
}

static MslpHashbucket * new_bucket(const char *pcKey, const char *pcVal) 
{
    MslpHashbucket *pb = (MslpHashbucket *)safe_malloc(sizeof(MslpHashbucket),NULL,0);
    
    pb->pcKey = safe_malloc(strlen(pcKey)+1,pcKey,strlen(pcKey));
    pb->pcVal = safe_malloc(strlen(pcVal)+1,pcVal,strlen(pcVal));
    pb->pBucketNext = NULL;
    
    return pb;
}

static unsigned int string_hash(const char *pcKey) 
{
    unsigned int result = 0;
    int c;
    
    while (1) 
    {
        c = *pcKey;
        pcKey++;
        if (c == 0) {
        break;
        }
        result += (result<<3) + c;
    }
    return result;
} 

EXPORT void mslp_hash_log_values(MslpHashtable *ph)
{
    SLP_LOG( SLP_LOG_STATE, "Current SLP Configuration State:" );
    mslp_hash_do( ph, mslp_log_params, NULL );		// iterate over all hash values and write to file
} 




