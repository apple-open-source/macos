
/*
 * mslplib_prlist.c : PRList utilities for the mslplib.
 *
 * Version: 1.7
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
 
#include <stdio.h>
#include <string.h>
#include <unistd.h>		// for _POSIX_THREADS
#include <pthread.h>	// for pthread_*_t

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
pthread_mutex_t	gPRListLock = PTHREAD_MUTEX_INITIALIZER;

void prlist_modify(char **ppcList, struct sockaddr_in sin) 
{
	char pcSrcBuf[16] = {0};
    char *pcSrc = inet_ntop(AF_INET, &sin.sin_addr, pcSrcBuf, sizeof(pcSrcBuf));
    char *pcTemp = *ppcList;
    int len;
    
	if ( !pcSrc )
		return;
		
    pthread_mutex_lock(&gPRListLock);
    
    if (*ppcList != NULL) 
    {
        if (!list_intersection(pcSrc,*ppcList)) 
        {  
            len = strlen(*ppcList) + strlen(pcSrc) + 1; /* old, comma, new item */
            *ppcList = safe_malloc(len+2,*ppcList,strlen(*ppcList));
            slp_strcat(*ppcList,",");
            slp_strcat(*ppcList,pcSrc);
            SLPFree(pcTemp);
        }
    } else {
        *ppcList = safe_malloc(strlen(pcSrc)+1,pcSrc,strlen(pcSrc));
    }
    pthread_mutex_unlock(&gPRListLock);
}

int getlen(const char *pc) {

  int i = (0xff0000 & (*(unsigned char*) &(pc[2]) << 16));
  i += (0xff00 & (*(unsigned char*) &(pc[3]) << 8));
  i += (0xff & (*(unsigned char*) &(pc[4])));
  return i;
}

/*
 * recalc_sendBuf
 *
 *   This routine recreates a buffer to send with a new Previous
 *   Responder list.  It does this inside the same fixed length
 *   buffer it was passed in.
 *
 *   [HDR]<prlist>{data}
 *
 *      becomes, by copying the data forward
 *
 *   [HDR]...space...{data}
 *
 *      which becomes, by writing in the new (longer) pr list
 *
 *   [HDR]< pr list >{data}
 *
 *    pcBuf      the send buffer to modify.
 *    iLen       the length of the send buffer.
 *    pcList     the new pr list to insert into the send buffer.
 *
 */
void recalc_sendBuf(char *pcBuf, int iLen, const char *pcList) {

  int iPROffset = HDRLEN+GETLANGLEN(pcBuf);             /* begin of PRList */
  int iCurrentPRListSz = GETSHT(pcBuf,iPROffset);   /* current prlist size */
  int iDataOffset = iPROffset+2+iCurrentPRListSz;      /* data starts here */
  int iToCopy = getlen(pcBuf) - iDataOffset;           /* stuff to move up */
  int iOutLen = getlen(pcBuf) + strlen(pcList) - iCurrentPRListSz;
  int loop;
  
  if (iOutLen >= iLen) return;                 /* too big to expand - punt */

  /* copy the data forward to make space for the new, longer, pr list */
  for (loop = 0; loop < iToCopy; loop++) {
    pcBuf[iOutLen - loop -1] = pcBuf[getlen(pcBuf) - loop -1];
  }
  
  /* write the new, longer, pr list into the space we have made */
  if (add_string(pcBuf,iLen,pcList,&iPROffset) != SLP_OK) {
    LOG(SLP_LOG_ERR,"recalc_sendBuf:  could not add_string the new PR List");
    return;
  }

  /* update the length field in the header of the slp message for new len */
  SETLEN(pcBuf,iOutLen);

}
