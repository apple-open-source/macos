#include "timeStr.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/*
 * Given a string containing either a UTC-style or "generalized time"
 * time string, convert to a struct tm (in GMT/UTC). Returns nonzero on
 * error. 
 */
int appTimeStringToTm(
	const char			*str,
	unsigned			len,
	struct tm			*tmp)
{
	char 		szTemp[5];
	unsigned 	isUtc;
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
  		case UTC_TIME_STRLEN:			// 2-digit year, not Y2K compliant
  			isUtc = 1;
  			break;
  		case GENERALIZED_TIME_STRLEN:	// 4-digit year
  			isUtc = 0;
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
		 *   50 <= year <  70 : illegal per PKIX
		 *   70 <  year <= 99 : assume century 20
		 */
		if(x < 50) {
			x += 2000;
		}
		else if(x < 70) {
			return 1;
		}
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
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
  	x = atoi( szTemp );
	if((x > 59) || (x < 0)) {
		return 1;
	}
  	tmp->tm_sec = x;
	return 0;
}

