/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
 * cuTimeStr.cpp - time string routines
 */
#include "cuTimeStr.h" 
#include "cuCdsaUtils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <pthread.h>

/*
 * Given a string containing either a UTC-style or "generalized time"
 * time string, convert to a struct tm (in GMT/UTC). Returns nonzero on
 * error. 
 */
int cuTimeStringToTm(
	const char			*str,
	unsigned			len,
	struct tm			*tmp)
{
	char 		szTemp[5];
	unsigned 	isUtc = 0;
	unsigned	noSeconds = 0;
	unsigned 	x;
	unsigned 	i;
	char 		*cp;

	if((str == NULL) || (len == 0) || (tmp == NULL)) {
    	return 1;
  	}
  	
  	/* tolerate NULL terminated or not */
  	if(str[len - 1] == '\0') {
  		len--;
  	}
  	switch(len) {
		case UTC_TIME_NOSEC_LEN:		// 2-digit year, no seconds, not y2K compliant
			isUtc = 1;
			noSeconds = 1;
			break;
  		case UTC_TIME_STRLEN:			// 2-digit year, not Y2K compliant
  			isUtc = 1;
  			break;
  		case GENERALIZED_TIME_STRLEN:	// 4-digit year
  			break;
  		default:						// unknown format 
  			return 1;
  	}
  	
  	cp = (char *)str;
  	
	/* check that all characters except last are digits */
	for(i=0; i<(len - 1); i++) {
		if ( !(isdigit(cp[i])) ) {
		  	return 1;
		}
	}

  	/* check last character is a 'Z' */
  	if(cp[len - 1] != 'Z' )	{
		return 1;
  	}

  	/* YEAR */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	if(!isUtc) {
		/* two more digits */
		szTemp[2] = *cp++;
		szTemp[3] = *cp++;
		szTemp[4] = '\0';
	}
	else { 
		szTemp[2] = '\0';
	}
	x = atoi( szTemp );
	if(isUtc) {
		/* 
		 * 2-digit year. 
		 *   0  <= year <  50 : assume century 21
		 *   50 <= year <  70 : illegal per PKIX, though we tolerate
		 *   70 <  year <= 99 : assume century 20
		 */
		if(x < 50) {
			x += 2000;
		}
		/*
		else if(x < 70) {
			return 1;
		}
		*/
		else {
			/* century 20 */
			x += 1900;			
		}
	}
  	/* by definition - tm_year is year - 1900 */
  	tmp->tm_year = x - 1900;

  	/* MONTH */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	x = atoi( szTemp );
	/* in the string, months are from 1 to 12 */
	if((x > 12) || (x <= 0)) {
    	return 1;
	}
	/* in a tm, 0 to 11 */
  	tmp->tm_mon = x - 1;

 	/* DAY */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	x = atoi( szTemp );
	/* 1..31 in both formats */
	if((x > 31) || (x <= 0)) {
		return 1;
	}
  	tmp->tm_mday = x;

	/* HOUR */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	x = atoi( szTemp );
	if((x > 23) || (x < 0)) {
		return 1;
	}
	tmp->tm_hour = x;

  	/* MINUTE */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	x = atoi( szTemp );
	if((x > 59) || (x < 0)) {
		return 1;
	}
  	tmp->tm_min = x;

  	/* SECOND */
	if(noSeconds) {
		tmp->tm_sec = 0;
	}
	else {
		szTemp[0] = *cp++;
		szTemp[1] = *cp++;
		szTemp[2] = '\0';
		x = atoi( szTemp );
		if((x > 59) || (x < 0)) {
			return 1;
		}
		tmp->tm_sec = x;
	}
	return 0;
}

#define MAX_TIME_STR_LEN  	30

/* protects time(), gmtime() */
static pthread_mutex_t timeMutex = PTHREAD_MUTEX_INITIALIZER;	

char *cuTimeAtNowPlus(int secFromNow, 
	timeSpec spec)
{
	struct tm utc;
	char *outStr;
	time_t baseTime;
	
	pthread_mutex_lock(&timeMutex);
	baseTime = time(NULL);
	baseTime += (time_t)secFromNow;
	utc = *gmtime(&baseTime);
	pthread_mutex_unlock(&timeMutex);
	
	outStr = (char *)APP_MALLOC(MAX_TIME_STR_LEN);
	
	switch(spec) {
		case TIME_UTC:
			/* UTC - 2 year digits - code which parses this assumes that
			 * (2-digit) years between 0 and 49 are in century 21 */
			if(utc.tm_year >= 100) {
				utc.tm_year -= 100;
			}
			sprintf(outStr, "%02d%02d%02d%02d%02d%02dZ",
				utc.tm_year /* + 1900 */, utc.tm_mon + 1,
				utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
			break;
		case TIME_GEN:
			sprintf(outStr, "%04d%02d%02d%02d%02d%02dZ",
				/* note year is relative to 1900, hopefully it'll 
				 * have four valid digits! */
				utc.tm_year + 1900, utc.tm_mon + 1,
				utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
			break;
		case TIME_CSSM:
			sprintf(outStr, "%04d%02d%02d%02d%02d%02d",
				/* note year is relative to 1900, hopefully it'll have 
				 * four valid digits! */
				utc.tm_year + 1900, utc.tm_mon + 1,
				utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
			break;
	}
	return outStr;
}

/*
 * Convert a CSSM_X509_TIME, which can be in any of three forms (UTC,
 * generalized, or CSSM_TIMESTRING) into a CSSM_TIMESTRING. Caller
 * must free() the result. Returns NULL if x509time is badly formed. 
 */
char *cuX509TimeToCssmTimestring(
	const CSSM_X509_TIME 	*x509Time,
	unsigned				*rtnLen)		// for caller's convenience
{
	int len = x509Time->time.Length;
	const char *inStr = (char *)x509Time->time.Data;	
											// not NULL terminated!
	char *rtn;
	
	*rtnLen = 0;
	if((len == 0) || (inStr == NULL)) {
		return NULL;
	}
	rtn = (char *)malloc(CSSM_TIME_STRLEN + 1);
	rtn[0] = '\0';
	switch(len) {
		case UTC_TIME_STRLEN:
		{
			/* infer century and prepend to output */
			char tmp[3];
			int year;
			tmp[0] = inStr[0];
			tmp[1] = inStr[1];
			tmp[2] = '\0';
			year = atoi(tmp);
			
			/* 
			 *   0  <= year <  50 : assume century 21
			 *   50 <= year <  70 : illegal per PKIX
			 *   70 <  year <= 99 : assume century 20
			 */
			if(year < 50) {
				/* century 21 */
				strcpy(rtn, "20");
			}
			else if(year < 70) {
				free(rtn);
				return NULL;
			}
			else {
				/* century 20 */
				strcpy(rtn, "19");
			}
			memmove(rtn + 2, inStr, len - 1);		// don't copy the Z
			break;
		}
		case CSSM_TIME_STRLEN:
			memmove(rtn, inStr, len);				// trivial case
			break;
		case GENERALIZED_TIME_STRLEN:
			memmove(rtn, inStr, len - 1);			// don't copy the Z
			break;
		
		default:
			free(rtn);
			return NULL;
	}
	rtn[CSSM_TIME_STRLEN] = '\0';
	*rtnLen = CSSM_TIME_STRLEN;
	return rtn;
}

