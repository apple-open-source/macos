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
 * mslplib_regipc.c : Minimal SLP v2 Service Agent API implementation - optional.
 *
 * Version: 1.1
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
 * (c) Sun Microsystems, 1999, All Rights Reserved.
 * Author: Erik Guttman
 */
 /*
	Portions Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslpd_store.h"
#include "mslp_dat.h"
#include "mslplib.h"

 /*
 * This module is only compiled if EXTRA_MSGS is defined.  This turns
 * on all the additional features of the SLP implementation.
 */
#ifdef EXTRA_MSGS

static int copy_till_entry(FILE *fpSrc,FILE *fpDest, const char *pcURL,
			   const char *pcLang, const char *pcSt);
static SLPInternalError add_entry(FILE *fpDest, const char *pcURL, const char *pcLang,
			  unsigned short usLifetime, const char *pcSt);
static SLPInternalError serialize_attributes(FILE *fpDest,
				     MslpHashtable *phash,const char *pcAtt);
static int scan_till_next_entry(MslpHashtable *phash, FILE *fpDest);
static void copy_remaining(FILE *fpSrc,FILE *fpDest);
static SLPInternalError mslplib_regipc_rename(const char *pc1, const char *pc2);


SLPInternalError mslplib_Reg(	UA_State *puas,
                        const char *pcURL,
                        unsigned short usLifetime,
                        const char *pcSt, 
                        const char *pcAtt,
                        SLPBoolean fresh) 
{
    SLPInternalError err = SLP_OK;
    FILE *fpSrc = NULL;
    FILE *fpDest = NULL;
    const char *pcPropertyLang = SLPGetProperty("net.slp.locale");
    
    /*
    * Obtain a lock on the reg file.  Once we do that, we can read it,
    * write a temporary file with the changes and replace the reg file.
    * The mslpd will detect the changed file, reread it and propogate
    * registrations to DAs.
    */
    SDLock(puas->pvMutex);
    if ((fpSrc = fopen(SLPGetProperty("com.sun.slp.regfile"),"rb")) == NULL) 
    {
        char buf[160];
        
        sprintf(buf,
            "mslplib_Reg: [WARNING] regfile \"%s\" could not be opened: %s\n\tAssume we should start a new regfile.",
            SLPGetProperty("com.sun.slp.regfile"),strerror(errno));
        SLP_LOG( SLP_LOG_DEBUG,buf);
    }
    
    if ((fpDest = fopen(SLPGetProperty("com.sun.slp.tempfile"),"wb")) == NULL) 
    {
        char buf[160];
        
        sprintf(buf,"mslplib_Reg: regfile could not be opened: %s", strerror(errno));
        err = SLP_INTERNAL_SYSTEM_ERROR;
        LOG(SLP_LOG_ERR,buf);
        goto mslplib_Reg_done;
    }
    
    /*
    * Read through the config file (fp), copying to the temp file (fp2)
    * until we encounter the line with the entry we have been passed.
    * This is indicated by the URL and Lang arguments.  At that point
    * add the new entry, skip the old version and and copy all the
    * remaining, subsequent entries in the config file.  If there is
    * no old entry, add this entry to the end of the old config file.
    */
    
    if (!fpSrc) 
    {
        /*
        * We have exhausted the config file.  Add the new entry to the end.
        */
        if (((err = add_entry(fpDest,pcURL,pcPropertyLang,usLifetime,pcSt)) != SLP_OK) 
            || ((err = serialize_attributes(fpDest,NULL,pcAtt)) != SLP_OK)) 
        {
            LOG(SLP_LOG_ERR,"mslplib_Reg: entry or attributes had problems");
            goto mslplib_Reg_done;
        }     
    } 
    else if (copy_till_entry(fpSrc, fpDest, pcURL, pcPropertyLang, pcSt) < 0 != SLP_OK) 
    {
        LOG(SLP_LOG_ERR,"mslplib_Reg: could not copy regfile to destfile");
        err = SLP_INVALID_REGISTRATION;
        goto mslplib_Reg_done;
    } 
    else 
    { 
        int more;
        MslpHashtable *phash;
        
        if (fresh == SLP_TRUE)
            phash = NULL;
        else
            phash = mslp_hash_init();
        
        if ((more=scan_till_next_entry(phash,fpSrc)) < 0) 
        {
            LOG(SLP_LOG_ERR,"mslplib_Reg: could not scan till next entry");
            err = (SLPInternalError) more;
            goto mslplib_Reg_done;
        } 
        else if (more == 0) 
        { /* exhausted config file - add entry to the end. */
    
            if ( ((err = add_entry(fpDest,pcURL,pcPropertyLang,usLifetime,pcSt)) != SLP_OK)
                || ((err = serialize_attributes(fpDest,phash,pcAtt)) != SLP_OK) ) 
            {
                LOG(SLP_LOG_ERR,"mslplib_Reg: entry or attributes had problems");
                goto mslplib_Reg_done;
            }
        } 
        else 
        { /* replace the old entry with a new entry */
    
            if (pcSt == NULL) 
            {
                fprintf(fpDest,"%s,%s,%u\n",pcURL, pcPropertyLang, usLifetime);
            } 
            else 
            {
                fprintf(fpDest,"%s,%s,%u,%s\n",pcURL,pcPropertyLang,usLifetime,pcSt);
            }
        
            if ((err = serialize_attributes(fpDest,phash,pcAtt)) != SLP_OK) 
            {
                LOG(SLP_LOG_ERR,"mslplib_Reg: could not serialize attributes");
                goto mslplib_Reg_done;
            }
        }
    
        fprintf(fpDest,"\n"); /* add a blank line after the last reg entry */
        copy_remaining(fpSrc,fpDest); /* should not need to do this */
    } 
    
    mslplib_Reg_done:
    if (fpSrc) 
        fclose(fpSrc);
        
    if (fpDest) 
        fclose(fpDest);
    
    if (err == SLP_OK) 
        err = mslplib_regipc_rename(SLPGetProperty("com.sun.slp.tempfile"), SLPGetProperty("com.sun.slp.regfile"));
    
    SDUnlock(puas->pvMutex);
    
    return err;
}

SLPInternalError mslplib_Dereg(UA_State *puas, const char *pcURL, const char *pcScopes) 
{
    FILE *fpSrc = NULL, *fpDest = NULL;
    SLPInternalError err = SLP_OK;
    
    SDLock(puas->pvMutex);
    
    if ((fpSrc = fopen(SLPGetProperty("com.sun.slp.regfile"),"rb")) == NULL) 
    {
        char buf[160];
        sprintf(buf,"mslplib_Reg: regfile could not be opened: %s", strerror(errno));
        LOG(SLP_LOG_ERR,buf);
        err = SLP_INTERNAL_SYSTEM_ERROR;
        goto mslplib_Dereg_done;
    }
    
    if ((fpDest = fopen(SLPGetProperty("com.sun.slp.tempfile"),"wb")) == NULL) {
        char buf[160];
        sprintf(buf,"mslplib_Dereg: temp could not be opened: %s",
            strerror(errno));
        LOG(SLP_LOG_ERR,buf);
        err = SLP_INTERNAL_SYSTEM_ERROR;
        goto mslplib_Dereg_done;
    }
    
    /*
    * This copies everything in Src to Dest except the services with
    * pcURL as their URL.
    */
    if (copy_till_entry(fpSrc,fpDest,pcURL,NULL,NULL) < 0) 
    {
        LOG(SLP_LOG_ERR,"mslplib_Dereg: could not scan to find entry to deregister");
        err = SLP_INTERNAL_SYSTEM_ERROR;
        goto mslplib_Dereg_done;
    } 
    else 
    {
        if (feof(fpSrc)) 
        {
            LOG(SLP_LOG_ERR,"mslplib_Dereg: requested URL not in reg file");
            err = SLP_INVALID_REGISTRATION;
            goto mslplib_Dereg_done;
        } 
        else 
        {
            if (scan_till_next_entry(NULL,fpSrc) < 0) 
            {
                LOG(SLP_LOG_ERR,"mslplib_Dereg: could not scan past the dereg'ed entry");
                err = SLP_INTERNAL_SYSTEM_ERROR;
                goto mslplib_Dereg_done;
            }
            copy_remaining(fpSrc,fpDest);
        }
    }

mslplib_Dereg_done:  
    if (fpSrc) 
        fclose(fpSrc);
        
    if (fpDest) 
        fclose(fpDest);
    if (err == SLP_OK) 
    {
        err = mslplib_regipc_rename(SLPGetProperty("com.sun.slp.tempfile"),
                    SLPGetProperty("com.sun.slp.regfile"));
    }
    
    SDUnlock(puas->pvMutex);
    return err;
}

/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

/*
 * copy_till_entry
 *
 *   This copies every entry until one is found which matches the
 *   specified URL, language and service type.  In this case the
 *   matching line is not copied over and a '1' is returned.
 *
 *   If the entire file is copied over and a match is not found,
 *   a '0' is returned.
 *
 * Parameters:
 *
 *   fpSrc       The source file - the existing reg file.
 *   fpDest      The dest file - the reg file we're building up.
 *   pcURL       The URL of the service we're looking for.
 *   pcLang      The language of the attributes in the registration.
 *               If this is NULL, it is ignored for the comparison.
 *   pcSt        The service type of the service in the registration.
 *               This is NOT used for identifying the entry:  This
 *               is solely the pcURL and pcLang parameter.  The
 *               previously registered service will take the newly
 *               registered service type.
 *
 * Returns:
 *
 *   SLP_OK if copying worked, some error for other cases.
 *
 * Side Effects:
 *
 *   fpSrc will be advanced.
 *   fpDest will be written to with contents from fpSrc until either
 *   the service entry which will be replaced or the end of the src
 *   file if the registration is a new entry.
 */
static int copy_till_entry(FILE *fpSrc,FILE *fpDest, const char *pcURL,
			   const char *pcLang, const char *pcSt) {

  char pcLine[MAXLINE];
  SLPInternalError err = SLP_OK;

  char *pcURL_   = NULL;  /* these are fields parsed out of the url lines */
  char *pcLang_  = NULL;
  char *pcTemp_  = NULL;  /* this is used to store the lifetime, ignored */
  char *pcSt_    = NULL;

  char c;            /* delimiter for parsing each line */
  int  index = 0;    /* index for parsing each line with get_next_string() */
  
  while(fgets(pcLine,MAXLINE,fpSrc)) {
    
    /* copy over space and comments */
    if (pcLine[0] == ';'  || pcLine[0] == '#'  || 
	pcLine[0] == '\n' || pcLine[0] == '\r' || pcLine == '\0') {
      fprintf(fpDest, "%s", pcLine);
      continue;
    }

    index = 0;
    
    /* URL line follows */
    if (!(pcURL_ = get_next_string(",",pcLine,&index,&c))) {
      LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"mslplib_Reg: bad file url",SLP_INVALID_REGISTRATION);
    }

    if (c == '\0' || c == '\n' || c == EOF ||
	!(pcLang_ = get_next_string(",",pcLine,&index,&c))) {
      if ( pcLang )
      {
        LOG(SLP_LOG_ERR,"mslplib_Reg: bad lang, use default.");
        pcLang_ = safe_malloc(strlen(pcLang)+1,pcLang,strlen(pcLang));
      }
    }
      
    if (c == '\0' || c == '\n' || c == EOF ||
	!(pcTemp_ = get_next_string(",",pcLine,&index,&c))) {
      LOG(SLP_LOG_ERR,"mslplib_Reg: bad lifetime - ignore");
    }
    SLPFree(pcTemp_); /* we don't use the lifetime */
    pcTemp_ = NULL;
    
    if (c == '\0' || c == '\n' || c == EOF ||
	(pcTemp_ = get_next_string("\000",pcLine,&index,&c)) == NULL) {
      /* infer the service type from the URL */
      int i = 0;
      while (pcURL_[i] != '\0' && pcURL_[i] != '/') i++;
      if (pcURL_[i] == '\0') {
	LOG(SLP_LOG_ERR,"mslplib_Reg: bad service type in URL in file");
	err = SLP_INVALID_REGISTRATION;
	break;
      }
      pcSt_ = safe_malloc(i,pcURL_,i-1); /* copy up to but not last ':' */
    } else {
      pcSt_ = pcTemp_;
    }

    if (!strcmp(pcURL,pcURL_) &&
	(!pcLang || !SDstrcasecmp(pcLang,pcLang_))) {
      /* we have a match, just return, do *not* write the entry */
      err = SLP_OK;
      break;
    }

    SLPFree(pcSt_);    pcSt_ = NULL;
    SLPFree(pcURL_);   pcURL_ = NULL;
    SLPFree(pcLang_);  pcLang_ = NULL;

    fprintf(fpDest,"%s",pcLine);

    /* copy over attrs till we get to the next entry - an empty line */
    while (fgets(pcLine,MAXLINE,fpSrc)) {
      char *pcPacked;
      fprintf(fpDest,"%s",pcLine); /* copy over each line, empty also */
      pcPacked = list_pack(pcLine);  /* spaces, etc. become empty lines */
      if (pcPacked[0] == '\0') {
	SLPFree(pcPacked);
	pcPacked = NULL;
	break;                       /* from attribute copying loop */
      }
      SLPFree(pcPacked);
      pcPacked = NULL;
    }
  }
      
  SLPFree(pcSt_);
  SLPFree(pcURL_);
  SLPFree(pcLang_);
  return err;
  
}

/*
 * add_entry
 *
 *   This routine adds an entire entry (less the attributes) to the
 *   destination file.
 *
 * Parameters:
 *
 *   fpDest     The destination file.
 *   pcURL      The service URL
 *   pcLang     The language of the service attributes
 *              If this parameter is NULL, the global property for locale
 *              is used.
 *   usLifetime The lifetime of the service registration (ignored by mslpd!)
 *   pcSt       The service type of the registered service.  If this is NULL
 *              the service type may be derived from the pcURL.
 *              If this parameter is NULL it is not included in the entry.
 *
 * Returns:
 *
 *   This returns an error if the parameters are absent or malformed. 
 *
 * Side Effects:
 *
 *   This adds the service entry to the destination file.
 */
static SLPInternalError add_entry(FILE *fpDest, const char *pcURL, const char *pcLang,
			  unsigned short usLifetime, const char *pcSt) {
  if (!pcURL || !fpDest || usLifetime == 0) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"add_entry: missing vital parameters",
	      SLP_PARAMETER_BAD);
  }
  if (pcLang == NULL) pcLang = SLPGetProperty("net.slp.locale");

  fprintf(fpDest,"%s,%s,%u",pcURL,pcLang,usLifetime);
  if (pcSt == NULL || pcSt[0] == '\0') {
    fprintf(fpDest,"\n");
  } else {
    fprintf(fpDest,",%s\n",pcSt);
  }
  return SLP_OK;
}

/*
 * reg_each
 *
 *   This is a MslpHashDoFun which will be used to add each line to
 *   the destination reg file.
 *
 * Parameters:
 *
 *   pcKey
 *   pcVal
 *   pvParam
 *
 * Results:
 *
 *   None.
 *
 * Side Effects:
 *
 *   Data is written to the destination file.
 */
void reg_each(const char *pcKey, const char *pcVal, void *pvParam) {
  FILE *fpDest = (FILE *) pvParam;
  if (pcVal[0] == '\0') {
    fprintf(fpDest,"%s\n",pcKey);
  } else {
    fprintf(fpDest,"%s=%s\n",pcKey,pcVal);
  }
}
    
/*
 * serialize_attributes
 *
 *   This function transforms a parameterized attribute list into the
 *   format required by a registration file.  That is: "(a=1,2),boo,(c=false)"
 *   will be rendered:
 *
 *   a=1,2
 *   boo
 *   c=false
 *
 * Parameters:
 *
 *   fpDest     The file pointer to the destination file
 *   phash      A hashtable for previous values in non-fresh cases.
 *   pcAtt      The attribute string to be serialized out.
 *
 * Returns:
 *
 *   SLP_PARSE_ERR is returned if the attribute string is malformed.
 *
 * Side Effects:
 *
 *   The destination file is modified, by adding the contents of the
 *   attribute string in the proper format.
 *
 *   The hashtable parameter is FREED by this routine!
 */
static SLPInternalError serialize_attributes(FILE *fpDest, MslpHashtable *phash, const char *pcAtt) 
{
    const char *pcSrc = pcAtt;
    char *pcTemp = NULL;
    char c;
    SLPInternalError err = SLP_OK;
  
    if (!fpDest || !pcAtt) 
    {
        LOG(SLP_LOG_ERR,"serialize_attributes: missing param");
        err = SLP_PARAMETER_BAD;
        goto serialize_attributes_done;
    }

    pcTemp = safe_malloc(strlen(pcAtt)+1,pcAtt,strlen(pcAtt));
    
    if (phash == NULL) 
    {
        phash = mslp_hash_init();
    }

    while (*pcSrc) 
    {
        while (*pcSrc && isspace(*pcSrc))
            pcSrc++; /* eat initial white space */
            
        if (*pcSrc == '\0') 
        {
            SLPFree(pcTemp);
            pcTemp = NULL;
            LOG(SLP_LOG_ERR,"serialize_attributes: empty term!");
            err = SLP_INVALID_REGISTRATION;
            goto serialize_attributes_done;      
        }
        
        if (*pcSrc != '(') 
        { /* keyword - take till ',' or EOL */
            char *pcDest = pcTemp;
            memset(pcTemp,0,strlen(pcAtt));
            while (*pcSrc != '\0' && *pcSrc != ',') 
            {
                char c = *pcSrc;
                if (c == '(' || c == ')' || c <= 0x20 || c == '=' || c == '!' ||
                    c == '~' || c == '>' || c == '<'  || c == '*' || c == '_' ) 
                {
                    SLPFree(pcTemp);
                    pcTemp = NULL;
                    LOG(SLP_LOG_ERR,"serialize_attributes: bad keyword char");
                    err = SLP_INVALID_REGISTRATION;
                    goto serialize_attributes_done;
                }

                *pcDest++ = *pcSrc++;
            }
            if ( pcTemp[0] == ',') 
            {
                SLP_LOG( SLP_LOG_DEBUG,"serialize_attributes: empty attr ending with ','");
            } 
            else 
            {
                mslp_hash_add(phash,pcTemp,"");
            }
            if (*pcSrc == ',')
                pcSrc++; /* advance past the ',' */
        
        } 
        else 
        { /* attribute */
            int   empty = 1;
            char *pcDest = pcTemp;
            char *pcKey = NULL;
            memset(pcTemp,0,strlen(pcAtt));
        
            pcSrc++; /* advance past the '(' */
            
            while (*pcSrc != '\0' && *pcSrc != '=') 
            {
                c = *pcSrc;
                if (c == '(' || c == ')' || c < 0x20  || c == '=' || c == '!' ||
                    c == '~' || c == '>' || c == '<'  || c == '*' || c == '_' ||
                    c == ',' ) 
                {
                    SLP_LOG(SLP_LOG_ERR,"serialize_attributes: bad tag char: %s", pcTemp);
                    SLPFree(pcTemp);
                    pcTemp = NULL;
                    err = SLP_INVALID_REGISTRATION;
                    goto serialize_attributes_done;	  
                }
                if (!isspace(c))
                    empty = 0;
                *pcDest++ = *pcSrc++;
            }
            if (*pcSrc == '\0') 
            {
                SLPFree(pcTemp);
                pcTemp = NULL;
                LOG(SLP_LOG_ERR,"serialize_attributes: unexpected EOL in tag");
                err = SLP_INVALID_REGISTRATION;
                goto serialize_attributes_done;
            }

            if (empty || *pcSrc == '\0') 
            {
                SLPFree(pcTemp);
                pcTemp = NULL;
                LOG(SLP_LOG_ERR,"serialize_attributes: unexpected empty tag");
                err = SLP_INVALID_REGISTRATION;
                goto serialize_attributes_done;
            }
	  
            pcKey = safe_malloc(strlen(pcTemp)+1,pcTemp,strlen(pcTemp));
        
            pcDest = pcTemp;
            memset(pcTemp,0,strlen(pcAtt));
            pcSrc++; /* advance past the '=' */
            while (*pcSrc != '\0' && *pcSrc != ')') 
            {
                char c = *pcSrc;
                if (c == '(' || c == '!' || c == '~' || c == '>' || c == '<' ||
                    (c < 0x20 && c != 0x0a && c != 0x0d && c != 0x09)) 
                {
                    SLP_LOG(SLP_LOG_ERR,"serialize_attributes: bad value char: %s", pcTemp);
                    SLPFree(pcTemp);
                    pcTemp = NULL;
					SLPFree(pcKey);
					pcKey = NULL;
                    err = SLP_INVALID_REGISTRATION;
                    goto serialize_attributes_done;
                }
                *pcDest++ = *pcSrc++;
            }

            if (pcTemp[0] == '\0') 
            {
				SLPFree(pcKey);
				pcKey = NULL;
                LOG(SLP_LOG_ERR,"serialize_attributes: unexpected omitted val");
                err = SLP_INVALID_REGISTRATION;
                goto serialize_attributes_done;
            }
      
            if (*pcSrc == '\0') 
            {
                SLPFree(pcTemp);
                pcTemp = NULL;			
				SLPFree(pcKey);
				pcKey = NULL;
                LOG(SLP_LOG_ERR,"serialize_attributes: unexpected EOL in value");
                err = SLP_INVALID_REGISTRATION;
                goto serialize_attributes_done;	
            }
            pcSrc++; /* advance past the ')' */
            while (*pcSrc && isspace(*pcSrc))
                pcSrc++; /* eat white space */
                
            if (pcSrc && *pcSrc == ',') 
                pcSrc++; /* advance past the ',' */

            mslp_hash_add(phash,pcKey,pcTemp);
			
			SLPFree(pcKey);
			pcKey = NULL;
		}
    }
    mslp_hash_do(phash,reg_each,(void*)fpDest);

serialize_attributes_done:
    mslp_hash_free(phash);  
    SLPFree(pcTemp);
    return err;
}

/*
 * scan_till_next_entry
 *
 *   This routine scans through the source file till the current
 *   entry ends (with a blank line).  This is called after we have
 *   decided to replace an entry in the source file and write a
 *   different entry into the destination file.  The remainder of
 *   the replaced entry needs to be skipped over.
 *
 * Parameters:
 *
 *   phash    A ptr to a hashtable to fill up with attributes as we
 *            scan, so they can be copied over.
 *   fpSrc    The source file with the entry to be scanned over.
 *
 * Returns:
 *
 *   0 if there is nothing after scanned over entry.
 *   1 if there is something after the scanned over entry.
 *   negative # is an SLPInternalError (ie parameter bad, etc.)
 *
 * Side Effects:
 *
 *   The file pointer in will have its location modified.
 */
static int scan_till_next_entry(MslpHashtable *phash, FILE *fpSrc) {

  char pcLine[MAXLINE];
  if (!fpSrc) return (int) SLP_PARAMETER_BAD;
  while (fgets(pcLine,MAXLINE,fpSrc)) {
    char *pcPacked = list_pack(pcLine);
    
    if (pcPacked[0] == '\0') {
      SLPFree(pcPacked);
      return 1; /* got to a blank line, end of entry */
    }
    SLPFree(pcPacked);

    /*
     * In this case, keep track of existing attrs so they can be merged
     * with the new attributes.
     */
    if (phash) { 
      char *pcTag = NULL;
      char *pcVal = NULL;
      int   index = 0;
      char  c;
      pcTag = get_next_string("=",pcLine,&index,&c);
      if (c != '\0') {
	pcVal = get_next_string("\000",pcLine, &index,&c);
      }
      mslp_hash_add(phash,pcTag,(pcVal)?pcVal:"");
      SLPFree(pcTag);
      SLPFree(pcVal);
    }
  }
  return 0; /* got to end of file */
}

/*
 * copy_remaining
 *
 *   This function copies all remaining contents of the source file
 *   to the destination file.  This is used when we have already
 *   modified the target entry and wish to simply copy over all the
 *   rest of the unmodified entries.
 *
 * Parameters:
 *
 *   fpSrc   The source, advanced to 'copy from' position.
 *   fpDest  The dest, advanced to 'copy to' position.
 *
 * Returns:
 *
 *   nothing
 *
 * Side Effects:
 *
 *   The destination file is modified.
 *   The source file pointer will be advanced to the EOF.
 */
static void copy_remaining(FILE *fpSrc,FILE *fpDest) {
  char pcLine[MAXLINE];
  while(fgets(pcLine, MAXLINE, fpSrc)) {
    fprintf(fpDest,"%s",pcLine);
  }
}

static SLPInternalError mslplib_regipc_rename(const char *pc1, const char *pc2) { 
  struct stat st1;
  char buf[100];
  FILE *fpSrc=NULL, *fpDest=NULL;
  int total;
  if (stat(pc1, &st1) < 0 ||
      (fpSrc = fopen(pc1,"rb")) == NULL ||
      (fpDest = fopen(pc2,"wb")) == NULL) {
    fclose(fpSrc);
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,
	      "mslplib_regipc_rename: open temp file or reg file failed",
	      SLP_INTERNAL_SYSTEM_ERROR);
  } else { /* move temp file to the reg file, then remove the temp file */
    total = st1.st_size;
    while (total > 0) {
      int xfer = (total > 100) ? 100 : total;
      int got;
      int wrote;
      if ((got = read(fileno(fpSrc),buf,xfer)) != xfer ||
	  (wrote = write(fileno(fpDest),buf,xfer)) != xfer) {
	fclose(fpSrc);
	fclose(fpDest);
	LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,
		  "mslplib_regipc_rename: read temp or write reg file failed",
		  SLP_INTERNAL_SYSTEM_ERROR);
      }
      total -= xfer;
    }

    if (fclose(fpSrc) || fclose(fpDest)) {
      LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,
		"mslplib_regipc_rename: could not close temp or reg file",
		SLP_INTERNAL_SYSTEM_ERROR);
    }
    if (remove(SLPGetProperty("com.sun.slp.tempfile")) < 0) {
      LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,
		"mslplib_regipc_rename: could not remove temp file",
		SLP_INTERNAL_SYSTEM_ERROR);
    }
  }
  return SLP_OK;
}

#ifdef READER_TEST
/*
 * Unit tests for the static functions in this module.
 * These are only wrappers to allow static (module private) functions
 * to be tested externally (in mslplib_regipc_testdriver.c).
 */

void regipc_copy_till_entry(FILE *fpSrc, FILE *fpDest, const char *pc1,
			    const char *pc2, const char *pc3) {

  copy_till_entry(fpSrc,fpDest,pc1,pc2,pc3);
  
}

SLPInternalError regipc_add_entry(FILE *fpDest, const char *pcURL,
			     const char *pcLang, unsigned short usLifetime,
			     const char *pcSt) {
  return add_entry(fpDest,pcURL,pcLang,usLifetime,pcSt);
}

SLPInternalError regipc_serialize_attributes(FILE *fpDest, MslpHashtable *phash,
				  const char *pcAtt) {
  return serialize_attributes(fpDest,phash,pcAtt);
}

int regipc_scan_till_next_entry(MslpHashtable *phash, FILE *fpDest) {
  return scan_till_next_entry(phash,fpDest);
}

#endif /* READER_TEST */

#endif /* EXTRA_MSGS */
