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
 * mslpd_reader.c : Serialized registration file reader for mslpd.
 *
 *   This file transforms the serialized reg file into the store for
 *   the mslpd.
 *
 * Version: 1.17
 * Date:    10/06/99
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
 * (c) Sun Microsystems, 1998-9, All Rights Reserved.
 * Author: Erik Guttman
 *
 * NOTE:  At this point, each line has a max size of 4k. 
 *
 * The grammar for the serialized registration file comes from 
 * draft-ietf-svrloc-api-08.txt.
 *

      ser-file      =  reg-list
      reg-list      =  reg / reg reg-list
      reg           =  creg / ser-reg
      creg          =  comment-line ser-reg
      comment-line  =  ( "#" / ";" ) 1*allchar newline

 * Each serialized registration line ends with 2 newlines

      ser-reg       =  url-props [slist] [attr-list] newline newline
      url-props     =  surl "," lang "," ltime [ "," type ] newline
      surl          =  ;The registration's URL. See
                       ; [10] for syntax.
      lang          =  1*8ALPHA [ "-" 1*8ALPHA ]
                       ;RFC 1766 Language Tag see [8].
      ltime         =  1*5DIGIT
                       ; A positive 16-bit integer
                       ; giving the lifetime
                       ; of the registration.
      type          =  ; The service type name, see [9]
                       ; and [10] for syntax.

 * A scope list is the first attribute, if it is present.
 * It is on a line by itself.

      slist         =  "scopes" "=" scope-list newline
      scope-list    =  scope-name / scope-name "," scope-list
      scope         =  ; See grammar of [9] for
                       ; scope-name syntax.

 * The attr-list places one attribute definition on a line.

      attr-list     =  attr-def / attr-def attr-list
      attr-def      =  ( attr / keyword ) newline
      keyword       =  attr-id
      attr          =  attr-id "=" attr-val-list
      attr-id       =  ;Attribute id, see [9] for syntax.
      attr-val-list =  attr-val / attr-val "," attr-val-list
      attr-val      =  ;Attribute value, see [9] for syntax.
      allchar       =  char / WSP
      char          =  DIGIT / ALPHA / other
      other         =  %x21-%x2f / %x3a-%x40 /
                       %x5b-%x60 / %7b-%7e
                       ; All printable, nonwhitespace US-ASCII
                       ; characters.
      newline       =  CR / ( CRLF )

 * Notes on the data structure:
 *
 *   The SAStore is arranged as a collection of arrays.  Each service
 *   occupies the 'nth' position in each of the arrays.  For iteration
 *   bounds the size of the entire store is saved in the SAStore struct.
 *   The tags and attributes are arrays which are terminated by a NULL
 *   pointer - so iteration continues over the array till a NULL item
 *   in the array is reached.
 *
 *   This data structure is static, but requires little space.
 */

 /*
	Portions Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslpd_store.h"

static int      init_pstore(FILE *fp, char *pcLine, SAStore *pstore);
static SLPInternalError parse_url_props(const char *pcLine, SAStore *pstore, int item);
static int      count_attrs(FILE *fp, char *pcLine, int *piGotScopes);
static char *   get_srvtype(const char *pcURL);
static SLPInternalError fill_values(Values *pv, const char *pc, int offset);
static SLPInternalError fill_attrs(FILE *fp, char *pcLine, SAStore *pstore,
			   int item, const char *pcSL);
static void     serialize_attrs(SAStore *pstore, int item);

#if defined(EXTRA_MSGS)
static void     set_sa_srvtype_attr(const SAStore *pstore);
#endif /* EXTRA_MSGS */

#define MAXLINE 4096

void delete_regfile(const char *pcFile) 
{
    char*	command = (char*)malloc(strlen(pcFile) + 4);
    sprintf( command, "rm %s", pcFile );
    system( command );
    free(command);
}
/*
 * process_regfile
 *
 *  Takes a registration file and a pointer to a store structure, either
 *  fills in the store structure with data (packed arrays, terminated in
 *  NULLs, with no sorting at all) or an error.
 *
 *    pstore  - A pointer to the store which is filled in by reads.
 *    pcFile  - The name of a file to read, containing a serialized
 *              registration.
 *
 * returns:
 *
 *   SLP_OK if all went well, otherwise and SLP error.
 */
SLPInternalError process_regfile(SAStore *pstore, const char *pcFile) 
{
  /* read the config file to find out how many lines are not comments */
  /* open the file */
	FILE *fp;
	char pcLine[MAXLINE];
	int count = 0;  /* used for counting lines */
	int item  = 0;  /* used for keeping track of most recent entry index */
    const char *pcSL = GetEncodedScopeToRegisterIn();
	assert(pcSL); /* the scope list has to be defined at this point! */

	if (pstore == NULL || pcFile == NULL) 
	{
		return SLP_PARAMETER_BAD;
	}

	if ((fp = fopen(pcFile,"rb")) == NULL) 
	{
		sprintf(pcLine,"process_regfile: could not open file [%s]\n",pcFile);
		LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,pcLine,SLP_PARAMETER_BAD);
	}
 
	if ((count = init_pstore(fp,pcLine,pstore)) == 0) 
	{
        fclose(fp);
		return SLP_OK;
	} 
	else if (count < 0) 
	{
        fclose(fp);
		return (SLPInternalError) count;
	}

	rewind(fp);
    
  /* parse the line into the store, and allocate entry */

	while (fgets(pcLine,MAXLINE,fp)) 
	{ 
		SLPInternalError err;
    
		if (pcLine[0] == ';'  || pcLine[0] == '#'  || 
			pcLine[0] == '\n' || pcLine[0] == '\r' || pcLine[0] == '\0') 
		{
			continue;
		}

		if ((err = parse_url_props(pcLine, pstore, item)) != SLP_OK) 
		{
            SLP_LOG( SLP_LOG_ERR, "process_regfile encountered an error" );
            fclose(fp);
			return err;
		}
    
    /*
     * Get one or more attributes.  The first one may be the scopes
     * attribute.
     */
		if ((err = fill_attrs(fp, pcLine, pstore, item, pcSL)) != SLP_OK) 
		{
            fclose(fp);
			SLP_LOG(SLP_LOG_DEBUG,"mslpd_reader.c, process_regfile(), fill_attrs: (%d)", err);
            return err;
		}
     
		item++;
	}

	fclose(fp);

#if defined(EXTRA_MSGS)
	set_sa_srvtype_attr(pstore);
#endif /* EXTRA_MSGS */
	if (pstore && SLPGetProperty("net.slp.traceReg") && 
		!SDstrcasecmp("true",SLPGetProperty("net.slp.traceReg"))) 
	{
		int i, j, k;
		printf("regfile dump: %d items\n",pstore->size);

		for (i = 0; i < pstore->size; i++) 
		{
			printf("%d. URL [%s] scope [%s] type [%s] lang [%s] life [%i]\n",
					i, pstore->url[i], pstore->scope[i], 
					pstore->srvtype[i], pstore->lang[i], pstore->life[i]);
		
			if (pstore->tag[i]) 
			{
		        for (j = 0; pstore->tag[i][j]; j++) 
		        {
          			printf("\t%d. %s",j,pstore->tag[i][j]);
					if (pstore->values[i] && pstore->values[i][j].numvals > 0) 
					{
						for (k = 0; k < pstore->values[i][j].numvals; k++) 
						{
							switch(pstore->values[i][j].type) 
							{
								case TYPE_BOOL:
									printf("<boolean:%s>",
										(pstore->values[i][j].pval[k].v_i)?"true":"false");
								break;
								case TYPE_INT:
									printf("<int:%d>",pstore->values[i][j].pval[k].v_i);
								break;
				                case TYPE_STR:
									printf("<string:%s>",pstore->values[i][j].pval[k].v_pc);
								break;
				                case TYPE_OPAQUE:
									printf("<opaque:%s>",pstore->values[i][j].pval[k].v_pc);
								break;
				                default:
									printf("<unknown>");
								break;
							}
							
							if (k < (pstore->values[i][j].numvals - 1)) 
								printf(",");
						}
					}
					
					printf("\n"); /* end of an individual attribute */
				
				} /* for each attribute */
			} /* there are attributes */
		} /* for each entry */
	} /* there is a store and we're debugging */
	
    {
        char	logStr[256];
        
        sprintf( logStr, "process_regfile finished, %d items are currently registered", pstore->size );
        LOG( SLP_LOG_REG, logStr );
    }
    
	return SLP_OK;
}

/* --------------------------------------------------------------------- */

/*
 * init_pstore
 *
 * Initializes the pstore.  The configuration file is read and the
 * number of entries are counted from it.  Arrays are set out to
 * contain places for all data which will be later read in.
 *
 *  fp        A pointer to the open registration file.
 *  pcLine    A pointer to a buffer (MAXLINE in size) to use to
 *            read in each line of the registration file.
 *
 * Returns:
 *
 *    0 or more indicates a proper registration file.  The value
 *    is the number of services registered.
 *
 *    Negative values are error codes and should be interpreted
 *    as SLPInternalError codes.
 *
 * Side Effects:
 *
 *    fp is advanced to the end and must be rewound.
 *
 *    pcLine is filled with arbitrary data.
 */
static int init_pstore(FILE *fp, char *pcLine, SAStore *pstore) {

  char *pc = pcLine;
  int count = 0;

  if (fp == NULL || pcLine == NULL) {
    return SLP_PARAMETER_BAD;
  }
  
  /* How many services?  Allocate store entry arrays. */
  while (pc != NULL) {

    do {

      pc = fgets(pcLine,MAXLINE,fp);

    } while ( pc != NULL &&
	      (pcLine[0] == ';'  || pcLine[0] == '#' ||    /* comment line */
	       pcLine[0] == 0x0a || pcLine[0] == 0x0d  ||  /* empty line */

		   pcLine[0] == 0x00));

    if (pc != NULL) {
      count++;
    }
  
    while (    pc != NULL 
		   && (pcLine[0] != 0x0a && pcLine[0] != 0x0d && pcLine[0] != 0x00)) {
      pc = fgets(pcLine,MAXLINE,fp);
    }
  }

  pstore->size    = count;
  pstore->srvtype = (char**) safe_malloc((count+1) * sizeof(char*),0,0);
  pstore->scope   = (char**) safe_malloc((count+1) * sizeof(char**),0,0);
  pstore->lang    = (char**) safe_malloc((count+1) * sizeof(char*),0,0);
  pstore->life    = (int*)   safe_malloc((count+1) * sizeof(int), 0, 0);
  pstore->url     = (char**) safe_malloc((count+1) * sizeof(char*),0,0);
  pstore->tag     = (char***) safe_malloc((count+1) * sizeof(char**),0,0);
  pstore->values  = (Values**) safe_malloc((count+1) * sizeof(Values*),0,0);
  pstore->attrlist= (char**) safe_malloc((count+1) * sizeof(char*),0,0);

  return count;
}

/*
 * parse_url_props
 *   
 *   This routine parses out the url, language, lifetime and service type
 *   from the pcLine buffer.  It makes allowances for omitted terms,
 *   supplying default values.
 *
 *   pcLine    The line to parse.  The grammar should be:

       url-props     =  surl "," lang "," ltime [ "," type ] newline
       
 *   pstore    The service store - to be filled in for item # 'item'.
 *   item      The item number to fill in.
 *
 * Returns:
 *
 *   SLPInternalError code - could indicate a parse error.
 *
 * Side Effects:
 *
 *   The SAStore is modified.
 */
static SLPInternalError parse_url_props(const char *pcLine,
				SAStore *pstore, int item) {

  int index = 0;      /* used for parsing lines */
  char c, *pcTemp = NULL;
  const char *pcLoc = SLPGetProperty("net.slp.locale");

  if (pcLine == NULL || pstore == NULL || item < 0 || item >= pstore->size) {
    return SLP_PARAMETER_BAD;
  }

  if (!(pstore->url[item] = get_next_string(",",pcLine,&index,&c))) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"process_regfile bad url",SLP_INVALID_REGISTRATION);
  }
  
  if (c == '\0' || c == '\n' || c == EOF ||
      !(pstore->lang[item] = get_next_string(",",pcLine,&index,&c))) {
    LOG(SLP_LOG_DEBUG,"process_regfile no language: set to default language.");
    pstore->lang[item] = safe_malloc(strlen(pcLoc)+1,pcLoc,strlen(pcLoc));
  }

  /*
   * Ignore the lifetime associated with the service.
   * mslpd always registers services continuously.
   */
  if (c == '\0' || c == '\n' || c == EOF ||
      !(pcTemp = get_next_string(",",pcLine,&index,&c))) {
    LOG(SLP_LOG_DEBUG,"process_regfile bad lifetime - ignored.");
  }
  if (pcTemp == NULL) {
    pstore->life[item] = 0xffff; /* use default */
  } else {
	char*	endPtr = NULL;
    pstore->life[item] = strtol(pcTemp,&endPtr,10);
    if (pstore->life[item] < 1 || pstore->life[item] > 0xffff) {
      pstore->life[item] = 0xffff;
      LOG(SLP_LOG_DEBUG,"process_regfile: lifetime <min or >max, use default");
    }
  }
    
  SLPFree((void*)pcTemp); 
  
  /*
   * There may or may not be a Type string as the last term on this line.
   * If not, use the one from the URL string provided.
   */
  
  if (c == '\0' || c == '\n' || c == EOF ||
      (pcTemp = get_next_string("\000",pcLine,&index,&c)) == NULL) {
    pstore->srvtype[item] = get_srvtype(pstore->url[item]);
  } else {
    pstore->srvtype[item] = pcTemp;
  }
  
  return SLP_OK;
}

/* --------------------------------------------------------------------- */

/*
 * fill_values
 *
 *  Fills in the values for an attribute.  It first has to count the
 *  number of values and allocate an array.  The end of the array is
 *  signaled by an NULL pointer in the Values array.
 *
 *    pVals   - A pointer to an allocated 'Values' struct to fill in.
 *    pc      - A pointer to the characters to parse.  This will be
 *              advanced past the last value token.
 *    offset  - The current offset into the parsed line.
 *
 * Return:
 *
 *    SLPInternalError code
 *
 * Side Effects:
 *
 *    The pv array is filled in with values.
 */
static SLPInternalError fill_values(Values *pv, const char *pc, int offset) {

  /*
   * count how many values there are 
   */
  SLPInternalError err = SLP_OK;
  int loop, iValCount = 0;

  char *pcPacked = list_pack(&pc[offset]);
  char *pcScan = pcPacked;
  int  iValueOffset = 0; /* for parsing through the value string */
  
  if (pv == NULL || pc == NULL || offset < 0) {
    return SLP_PARAMETER_BAD;
  }

  if (pcPacked == NULL || pcPacked[0] == '\0') {
    SLPFree(pcPacked);
    pv->numvals = 0;
    return SLP_OK;
  }
  
  /* we are presently past the '=', count ','s till we get to a ')'
     unless we hit an EOF, in which case we have a parse error. */
  for( ; pcScan; pcScan++) {

    if (*pcScan == ',') iValCount++;
    if (*pcScan == '\n' || *pcScan == '\0' || *pcScan == EOF) {
      iValCount++;
      break;
    }
  }

  /*
   * Allocate space for the values
   */
  pv->pval = (Val*) safe_malloc((iValCount+1)*sizeof(Val), NULL, 0);
  pv->type = TYPE_UNKNOWN;  


  for (loop = 0; loop < iValCount; loop++) {
    
    if ((err = fill_value(pv, loop, pcPacked, &iValueOffset)) != SLP_OK) {
      SLPFree(pcPacked);
      return err;
    }
  }
  
  pv->numvals = iValCount;
  SLPFree(pcPacked);
  return SLP_OK;
}

/*
 * fill_value
 *
 * This function determines the type and value of the particular
 * value currently being interpreted.  It fills in this value for
 * the particular Value in the value array.  If possible, it frees
 * the string obtained by get_next_string, but in some cases this
 * string is retained (for inclusion in the stored value.)
 *
 *   pv        The array of Values.  We've already counted them,
 *             so there's no chance of going off the end of the array.
 *   i         The index into the array of Values to fill in.
 *   pc        A pointer to the buffer we're currently interpreting.
 *   piOffset  A moving offset as we continue interpreting, into the
 *             buffer pointed at by pc.
 *
 * Returns:
 *
 *   An SLP error code.  
 *
 * Side Effects:
 *
 *   Value pv[i] is filled in.  The offset *piOffset increases as
 *   we interpret values.
 */
SLPInternalError fill_value(Values *pv, int i, const char *pc, int *piOffset) {

  int result;
  char c;     /* delimiter found */
  char *pcVal;

  if (pv==NULL || pc == NULL || piOffset == NULL || i < 0 || *piOffset < 0) {
    return SLP_PARAMETER_BAD;
  }

  pcVal= get_next_string(",",pc,piOffset,&c);

  if (pcVal == NULL || *pcVal == '\0') {
    return SLP_INVALID_REGISTRATION;
  }
    
  if (!SDstrcasecmp(pcVal,"true")) {

    pv->type = TYPE_BOOL;
    pv->pval[i].v_i = 1;
    SLPFree((void*)pcVal);

  } else if (!SDstrcasecmp(pcVal,"false")) {

    pv->type = TYPE_BOOL;
    pv->pval[i].v_i = 0;
    SLPFree((void*)pcVal);

  } else if (!SDstrncasecmp(pcVal,"\\FF",3)) {

    pv->type = TYPE_OPAQUE;
    pv->pval[i].v_pc = pcVal;

  } else {

    char *ptr;
    long lVal;
    errno = 0;
    lVal = strtol(pcVal,&ptr,10);
    if (*ptr == '\0' && errno == 0) {

      pv->type = TYPE_INT;
      pv->pval[i].v_i = (int) lVal;
      SLPFree(pcVal);

    } else {

      if (errno == ERANGE) {
	mslplog(SLP_LOG_DEBUG,"fill_value: Value over or under allowed integer: ",
		pcVal);
      }
      result = isAttrvalEscapedOK(pcVal);
      if (result < 0) return (SLPInternalError) result;
      /* for now leave the string escaped */
      pv->type = TYPE_STR;
      pv->pval[i].v_pc = pcVal;

    }
  }
  return SLP_OK;
}


/*
 * fill_attrs
 *
 * This function advances through the configuration file.  Using
 * count_attrs the total number of attributes are obtained and
 * the tag and values are all stored away in pstore.  The pcLine
 * buffer is borrowed from the caller to reduce memory requirements.
 *
 * The first attribute may be scopes.  If so, and if the scopes are
 * a proper subset of the scopes passed in with pcSL, these scopes
 * are used for the service registration.  In any other case, the
 * scopes passed in with pcSL are used.  The scopes 'attribute' is
 * never stored with a service entry.
 *
 *    fp      The config file advanced to where the attributes for
 *            service # 'item' are.
 *    pcLine  A buffer allocated externally.  This will be used to
 *            read in attributes line by line.  The size of the buffer
 *            is determined by the value of MAXLINE.
 *    pstore  The store of all services.  Item number 'item' will be
 *            filled in with attribute tags and values.
 *    item    The item in the pstore that we are filling.
 *    pcSL    This is the scope list determined by the mslpd configuration
 *            property "net.slp.useScopes".
 *
 * Returns:
 *
 *   SLP error code.
 *
 * Side Effects:
 *
 *   pstore[item]->tag and pstore[item]->scope and pstore[item]->values
 *   are filled in.
 *
 *   fp is advanced over the attribute.
 *
 *   pcLine's contents change, but this is not important, since it is
 *   overwritten every time a line is read in.
 */

static SLPInternalError fill_attrs(FILE *fp, char *pcLine, SAStore *pstore, int item, const char *pcSL) 
{
    int loop;
    int gotScope = 0;
    int iNumAttrs;
    char *pc;
    char *pcTrim;
    char *pcTag = NULL;
    int offset = 0;
    char c;
        
    if ( fp==NULL       || pcLine == NULL ||
        pstore == NULL || item < 0   || pcSL == NULL ) 
    {
        return SLP_PARAMETER_BAD;
    }
    
    iNumAttrs = count_attrs(fp,pcLine,&gotScope);
    if (iNumAttrs < 0) 
    {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"mslpd_reader.c, process_regfile(), count_attrs", SLP_INVALID_REGISTRATION);
    }

    if (gotScope) 
    {
        char *pcTemp;
        
        /* read in the line with the scopes */
        pc = fgets(pcLine,MAXLINE,fp);
    
        pcTemp = get_next_string("=", pcLine, &offset, &c);
        SLPFree(pcTemp);
    
        pcTemp = get_next_string("\000", pcLine, &offset, &c); 
        pcTrim = list_pack(pcTemp);
        SLPFree(pcTemp);
        
        // we don't care if the scope is in the scope list or not, we'll just add it to the scope
        // list right now!
        if (!list_subset(pcTrim, SLPGetProperty("net.slp.useScopes"))) 
        {
            char		newScopeList[1024];		// this has GOT to be big enough... (I know, shoot me later)
            
            sprintf( newScopeList, "%s,%s", SLPGetProperty("net.slp.useScopes"), pcTrim );
    
            SLPSetProperty("net.slp.useScopes", newScopeList );		// update our scopelist
        } 
    
        pstore->scope[item] = pcTrim;
    } 
    else 
    {
        pstore->scope[item] = safe_malloc(strlen(pcSL)+1,pcSL,strlen(pcSL));
    }

    iNumAttrs -= gotScope; /* scope is not really an attribute */
    if (iNumAttrs == 0) 
    {
        pstore->tag[item] = NULL;
        pstore->values[item] = NULL;
    } 
    else 
    {  
        /* create the arrays of attribute tags and values */
        pstore->tag[item] = (char **) safe_malloc((iNumAttrs+1)*sizeof(char*),0,0);
        pstore->values[item] =
        (Values *) safe_malloc((iNumAttrs+1)*sizeof(Values),0,0);
        
        for (loop = 0; loop < iNumAttrs; loop++) 
        {
            fgets(pcLine,MAXLINE,fp);
            offset = 0;
            
            pcTag = get_next_string("=", pcLine, &offset, &c);
            pcTrim = list_pack(pcTag);
        
            /* if there's an attribute which has no tag or only white space */
            if (pcTrim == NULL || pcTrim[0] == '\0') 
            {
                SLPFree(pcTrim);
                SLPFree(pcTag);
                pcTag = safe_malloc(1,0,0);
            } 
            else 
            { /* use the trimmed attribute tag! */
                SLPFree(pcTag);
                pcTag = pcTrim;
            }
            /* if scope attr in the attribute list, decrement the tag index */
            pstore->tag[item][loop] = pcTag; 
        
            if (c == '=') 
            { /* there are values */
    
        /* if scope attr in the attribute list, decrement the values index */
                SLPInternalError err = fill_values(&(pstore->values[item][loop]),pcLine,offset);
                
                if (err != SLP_OK)
                    return err;
                        
                if (!pstore->tag[item][loop]) 
                    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"fill_attr: prob getting vals",err);
            } 
            else 
            {
                pstore->values[item][loop].type = TYPE_KEYWORD;
                pstore->values[item][loop].numvals = 0;
                pstore->values[item][loop].pval = NULL;
            }
        }
    }
    
    serialize_attrs(pstore,item);
    
    /* leave empty lines! */
    return SLP_OK;
}

/*
 * count_attrs
 *
 *   This routine counts how many attributes are associated with the
 *   service.  It also notes if a scopes= line is included in the
 *   attr list though it includes this in the count.
 *
 *   After counting the attributes, the file pointer is returned to
 *   the position it was in when this routine was called.
 *
 *  fp      A pointer to an open registration file, at the proper
 *          point in the file to count attributes (ie. after the
 *          url-props line.
 *  pcLine  The buffer to read lines into.
 *  piScope Sets this to 0 if no scope line, 1 otherwise.
 *
 * Result:
 *
 *   0 or more indicates a successful count.  The value is the number
 *   of attributes which follow.
 *
 * Side Effects:
 *
 *   piScope is set depending on whether the first attr is 'scopes='
 */
static int count_attrs(FILE *fp, char *pcLine, int *piScope) 
{
    int count = 0;
    long start;
    char *pcPacked = NULL;
    
    *piScope = 0;
    
    if (fp == NULL) 
    {
        return SLP_PARAMETER_BAD;
    }
    
    start = ftell(fp);
    
    do {
    
        if (fgets(pcLine,MAXLINE,fp) == NULL) 
            break;

        pcPacked = list_pack(pcLine);
    
        if (pcPacked[0] == '\0') 
        {
            break;
        } 
        else 
        {        
            if (count == 0 && !SDstrncasecmp("scopes=",pcPacked,7)) 
            {
                *piScope = 1;
            }
        
            count++;
        }
    
        SLPFree(pcPacked);
        pcPacked = NULL;
    } while (1);
    
    
    if (fseek(fp,start,SEEK_SET) < 0) 
    {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"mslpd_reader.c, count_attrs(), fseek failed", SLP_INTERNAL_SYSTEM_ERROR);
    }
    
    
    SLPFree(pcPacked);
    return count;
}

static void init_or_merge(const char *pcNewString, char **pcList, int *len) {

  if (pcNewString == NULL) return;
  
  if (*pcList == NULL || *pcList[0] == '\0') { /* initialize the list */
    *len = strlen(pcNewString);
    *pcList = safe_malloc(*len+1,pcNewString,*len);
  } else {
    list_merge(pcNewString,pcList,len,CHECK);
  }
}

/*
 * serialize_values
 */
char * serialize_values(SAStore *pstore, int item, int attr) {

  int vcount;
  char *pcVals = safe_malloc(1,0,0);
  int vListLen = 0;
  
  for (vcount=0; vcount < pstore->values[item][attr].numvals; vcount++){
    char buf[32];
    if (pstore->values[item][attr].type == TYPE_INT) {
      sprintf(buf,"%d",pstore->values[item][attr].pval[vcount].v_i);
      init_or_merge(buf,&pcVals,&vListLen);
    } else if (pstore->values[item][attr].type == TYPE_BOOL) {
      if (pstore->values[item][attr].pval[vcount].v_i == 0) {
	sprintf(buf,"FALSE");
      } else {
	sprintf(buf,"TRUE");
      }
      init_or_merge(buf,&pcVals,&vListLen);	  
    } else {
      init_or_merge(pstore->values[item][attr].pval[vcount].v_pc,
		    &pcVals,&vListLen);
    }
  }
  return pcVals;
}

/*
 * serialize_attrs
 *
 *   This takes the attrs in the tag and values fields of items and
 *   converts them into a serialized attr list to be inserted into
 *   a registration string.
 *
 * Side Effects:
 *
 *   pstore->attrlist[item] is filled in with the attribute list.
 */
static void serialize_attrs(SAStore *pstore, int item) {
  int icount = 0;
  int iListLen = 0;
  char *pcAttrs = NULL;
  char pcItem[MAXLINE+4];
  
  memset(pcItem,0,MAXLINE+4);

  if (pstore->tag[item] == NULL) return;
  
  for (icount = 0; pstore->tag[item][icount]; icount++) {
    if (pstore->values[item][icount].numvals == 0) { /* keyword */
      init_or_merge(pstore->tag[item][icount],&pcAttrs,&iListLen);
    } else {
      char *pcVals = serialize_values(pstore,item,icount);
      sprintf(pcItem,"(%s=%s)",pstore->tag[item][icount],pcVals);
      SLPFree(pcVals);      
    }
    init_or_merge(pcItem,&pcAttrs,&iListLen);
  } /* after adding all attributes */
  
  pstore->attrlist[item] = pcAttrs; 
}


/*
 * get_srvtype
 *
 *   This is a pretty naive way of getting a srvtype, namely up to one
 *   character before the first '/'.
 *
 *     pcURL - any URL with an addrspec separated out by /s
 *
 * Returns:
 *
 *   newly allocated buffer with the srvtype
 */
static char * get_srvtype(const char *pcURL) {

  int i=0;
  char *pcSrvtype;

  assert(pcURL);

  while (pcURL[i] != '\0' && pcURL[i] != '/') i++;
  if (pcURL[i] == '\0') return NULL;
  pcSrvtype = safe_malloc(i,pcURL,i-1);
  
  return pcSrvtype;
}

/* ------------------------------------------------------------------------- */

#if defined(EXTRA_MSGS)
static void set_sa_srvtype_attr(const SAStore *pstore) {

  int i;
  char *pcSTList = safe_malloc(1,0,0);
  int   iSTListLen = 0;
  
  for (i=0; i<pstore->size; i++) {
    list_merge(pstore->srvtype[i],&pcSTList,&iSTListLen, CHECK);
  }
  SLPSetProperty("com.sun.slp.saSrvTypes",pcSTList);
  SLPFree(pcSTList);

}
#endif /* EXTRA_MSGS */


/*
 * Unit tests are coded below.  They have to be in this file since
 * the units they are testing are declared 'static' above.
 *
 * Further tests are available in ./test/mslpd_reader_testdriver.c
 */
#ifdef READER_TEST



/* testing wrappers for functions */

char *reader_get_srvtype(const char *pcURL) {
  return get_srvtype(pcURL);
}
     
SLPInternalError reader_parse_url_props(const char *pcLine, SAStore *pstore,
				int item) {
  return parse_url_props(pcLine, pstore, item);
}

int reader_count_attrs(FILE *fp,int *piGotScopes) {
  char line[MAXLINE];
  return count_attrs(fp,line,piGotScopes);
}

SLPInternalError reader_fill_attrs(FILE *fp, char *pcLine, SAStore *pstore,
			   int item, const char *pcSL) {
  return fill_attrs(fp, pcLine, pstore, item, pcSL);
}

SLPInternalError reader_fill_values(Values *pv, const char *pc, int piOffset) {
  return fill_values(pv,pc,piOffset);
}

int reader_init_pstore(FILE *fp, char *pcLine, SAStore *pstore) {
  return init_pstore(fp, pcLine,pstore);
}

#endif /* READER_TEST */



