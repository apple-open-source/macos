/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Copyright (c) 1997 Apple Computer, Inc.
 * All rights reserved.
 *
 * Partial implementation of the V3 API. We don't, however, support
 * things like controls, although the skeletal code is there.
 *
 * Based on draft-ietf-asid-ldap-c-api-00.txt.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>
#include <ctype.h>
#include <NetInfo/config.h>
#include "lber.h"
#include "ldap_ldap-int.h" 
#include "ldap.h"

#ifdef _UNIX_BSD_43_
extern char *strdup(char *);
#endif

int ldap_set_option( LDAP *ld, int option, void *optdata )
{
	int version;

	switch (option)
	{
		
		case LDAP_OPT_DESC:
			/* Underlying socket descriptor corresponding
			 * to the default LDAP connection. int *
			 */
			ld->ld_sb.sb_sd = *((int *)optdata);
			break;
		case LDAP_OPT_DEREF:
			/* How aliases are handled during search. int * */
			ld->ld_deref = *((int *)optdata);
			break;
		case LDAP_OPT_SIZELIMIT:
			/* The sizelimit. int * */
			ld->ld_sizelimit = *((int *)optdata);
			break;
		case LDAP_OPT_TIMELIMIT:
			/* The timelimit. int * */
			ld->ld_timelimit = *((int *)optdata);
			break;
		case LDAP_OPT_THREAD_FN_PTRS:
			/* Pointer to thread functions. struct ldap_thread_fns * */
			return LDAP_NOT_SUPPORTED;
			break;
		case LDAP_OPT_REBIND_FN:
			/* Function pointer. */
			ld->ld_rebindproc = (ldap_rebindproc_t *)optdata;
			break;
		case LDAP_OPT_REBIND_ARG:
			/* Rebind arg. void * */
			ld->ld_rebindarg = optdata;
			break;
		case LDAP_OPT_REFERRALS:
			/* off or on: do we follow referrals? */
			if (optdata == LDAP_OPT_ON)
			{
				ld->ld_options |= LDAP_INTERNAL_OPT_REFERRALS;
			}
			else if (optdata == LDAP_OPT_OFF)
			{
				ld->ld_options &= ~(LDAP_INTERNAL_OPT_REFERRALS);
			}
			else
			{
				return LDAP_PARAM_ERROR;
			}
			break;
		case LDAP_OPT_RESTART:
			/* off or on: whether I/O should be restarted */
			if (optdata == LDAP_OPT_ON)
			{
				ld->ld_options |= LDAP_INTERNAL_OPT_RESTART;
			}
			else if (optdata == LDAP_OPT_OFF)
			{
				ld->ld_options &= ~(LDAP_INTERNAL_OPT_RESTART);
			} else
			{
				return LDAP_PARAM_ERROR;
			}
			break;
		case LDAP_OPT_PROTOCOL_VERSION:
			/* LDAP_VERSION[23]: int * */
			version = *((int *)optdata);
			switch (version)
			{
				case LDAP_VERSION1:
				case LDAP_VERSION2:
				case LDAP_VERSION3:
					ld->ld_version = *(int *)optdata;
					break;
				default:
					return LDAP_PARAM_ERROR;
			}	
			break;
		case LDAP_OPT_HOST_NAME:
			if (ld->ld_defhost != NULL)
				free(ld->ld_defhost);
			
			ld->ld_defhost = strdup((char *)optdata);
			break;
		case LDAP_OPT_ERROR_NUMBER:
			/* The most recent LDAP error. int * */
			ld->ld_errno = *((int *)optdata);
			break;
		case LDAP_OPT_ERROR_STRING:
			if (ld->ld_error != NULL)
				free(ld->ld_error);
				
			ld->ld_error = strdup((char *)optdata);
			break;
		case LDAP_OPT_REFERRAL_HOP_LIMIT:
			ld->ld_refhoplimit = *((int *)optdata);
			break;
#ifdef notdef
		case LDAP_OPT_RES_INTERNAL:
			if (optdata == LDAP_OPT_ON)
			{
				ld->ld_sb.sb_options |= LBER_RES_NO_LOOKUPD;
			} else if (optdata == LDAP_OPT_OFF)
			{
				ld->ld_sb.sb_options &= ~(LBER_RES_NO_LOOKUPD);
			} else
			{
				return LDAP_PARAM_ERROR;
			}
			break;
#endif
		/* LDAPControl ** - these aren't supported yet. */
		case LDAP_OPT_SERVER_CONTROLS:
		case LDAP_OPT_CLIENT_CONTROLS:
		default:
			return LDAP_NOT_SUPPORTED;
			break;
	}
	
	return LDAP_SUCCESS;
}

int ldap_get_option( LDAP *ld, int option, void *optdata )
{
	switch (option)
	{
		case LDAP_OPT_DESC:
			/* Underlying socket descriptor corresponding
			 * to the default LDAP connection. int *
			 */
			 *((int *)optdata) = ld->ld_sb.sb_sd;
			break;
		case LDAP_OPT_DEREF:
			/* How aliases are handled during search. int * */
			*((int *)optdata) = ld->ld_deref;
			break;
		case LDAP_OPT_SIZELIMIT:
			/* The sizelimit. int * */
			*((int *)optdata) = ld->ld_sizelimit;
			break;
		case LDAP_OPT_TIMELIMIT:
			/* The timelimit. int * */
			*((int *)optdata) = ld->ld_timelimit;
			break;
		case LDAP_OPT_THREAD_FN_PTRS:
			/* Pointer to thread functions. struct ldap_thread_fns * */
			return LDAP_NOT_SUPPORTED;
			break;
		case LDAP_OPT_REBIND_FN:
			/* Function pointer. */
			*((ldap_rebindproc_t **)optdata) = ld->ld_rebindproc;
			break;
		case LDAP_OPT_REBIND_ARG:
			/* Rebind arg. void * */
			*((void **)optdata) = ld->ld_rebindarg;
			break;
		case LDAP_OPT_REFERRALS:
			/* off or on: do we follow referrals? */
			if (ld->ld_options & LDAP_INTERNAL_OPT_REFERRALS)
			{
				*((void **)optdata) = LDAP_OPT_ON;
			} else
			{
				*((void **)optdata) = LDAP_OPT_OFF;
			}
			break;
		case LDAP_OPT_RESTART:
			/* off or on: whether I/O should be restarted */
			if (ld->ld_options & LDAP_INTERNAL_OPT_RESTART)
			{
				*((void **)optdata) = LDAP_OPT_ON;
			} else
			{
				*((void **)optdata) = LDAP_OPT_OFF;
			}
			break;
		case LDAP_OPT_PROTOCOL_VERSION:
			/* LDAP_VERSION[23]: int * */
			*((int *)optdata) = ld->ld_version;
			break;
		case LDAP_OPT_HOST_NAME:
			*((char **)optdata) = strdup(ld->ld_defhost);
			break;
		case LDAP_OPT_ERROR_NUMBER:
			/* The most recent LDAP error. int * */
			*((int *)optdata) = ld->ld_errno;
			break;
		case LDAP_OPT_ERROR_STRING:
			/* The LDAP error string. char ** */
			*((char **)optdata) = strdup(ld->ld_error);
			break;
		case LDAP_OPT_REFERRAL_HOP_LIMIT:
			*((int *)optdata) = ld->ld_refhoplimit;
			break;
#ifdef notdef
		case LDAP_OPT_RES_INTERNAL:
			if (ld->ld_sb.sb_options & LBER_RES_NO_LOOKUPD)
			{
				*((void **)optdata) = LDAP_OPT_ON;
			} else 
			{
				*((void **)optdata) = LDAP_OPT_OFF;
			}
			break;
#endif
		/* LDAPControl ** - these aren't supported yet. */
		case LDAP_NOT_SUPPORTED:
		case LDAP_OPT_SERVER_CONTROLS:
		case LDAP_OPT_CLIENT_CONTROLS:
		default:
			return LDAP_NOT_SUPPORTED;
			break;
	}
	
	return LDAP_SUCCESS;

}

int ldap_version( LDAPVersion *ver )
{
	ver->protocol_version = LDAP_VERSION * 100;
	ver->reserved[0] = 0;
	ver->reserved[1] = 0;
	ver->reserved[2] = 0;
	ver->reserved[3] = 0;
	return SDK_VERSION;
}

int ldap_msgtype( LDAPMessage *res )
{
	return (res == NULL) ? LDAP_RES_ANY : res->lm_msgtype;
}

int ldap_msgid( LDAPMessage *res )
{
	return (res == NULL) ? -1 : res->lm_msgid;
}

void ldap_memfree( void *mem )
{
	if (mem != NULL)
		free(mem);
}

int ldap_get_lderrno( LDAP *ld, char **m, char **s )
{
	if (m != NULL)
		*m = NULL;

	if (ld == NULL)
		return LDAP_PARAM_ERROR;

	if (s != NULL)
		*s = (ld->ld_error == NULL) ? NULL : strdup(ld->ld_error);

	return ld->ld_errno;
}

int ldap_set_lderrno( LDAP *ld, int e, char *m, char *s )
{
	ld->ld_errno = e;
	
	if (s != NULL)
		ld->ld_error = strdup(s);

	return LDAP_SUCCESS;
}

char **ldap_explode_rdn( char *rdn, int notypes )
{
	char	*p, *q, *rdnstart, **rdns = NULL;
	int	state, count = 0, endquote, len;

	Debug( LDAP_DEBUG_TRACE, "ldap_explode_rdn\n", 0, 0, 0 );

#define INQUOTE		1
#define OUTQUOTE	2

	rdnstart = rdn;
	p = rdn-1;
	state = OUTQUOTE;

	do {

		++p;
		switch ( *p ) {
		case '\\':
			if ( *++p == '\0' )
				p--;
			break;
		case '"':
			if ( state == INQUOTE )
				state = OUTQUOTE;
			else
				state = INQUOTE;
			break;
		case '+':
		case '\0':
			if ( state == OUTQUOTE ) {
				++count;
				if ( rdns == NULL ) {
					if (( rdns = (char **)malloc( 8
						 * sizeof( char *))) == NULL )
						return( NULL );
				} else if ( count >= 8 ) {
					if (( rdns = (char **)realloc( rdns,
						(count+1) * sizeof( char *)))
						== NULL )
						return( NULL );
				}
				rdns[ count ] = NULL;
				endquote = 0;
				if ( notypes ) {
					for ( q = rdnstart;
					    q < p && *q != '='; ++q ) {
						;
					}
					if ( q < p ) {
						rdnstart = ++q;
					}
					if ( *rdnstart == '"' ) {
						++rdnstart;
					}
					
					if ( *(p-1) == '"' ) {
						endquote = 1;
						--p;
					}
				}

				len = p - rdnstart;
				if (( rdns[ count-1 ] = (char *)calloc( 1,
				    len + 1 )) != NULL ) {
				    	SAFEMEMCPY( rdns[ count-1 ], rdnstart,
					    len );
					rdns[ count-1 ][ len ] = '\0';
				}

				/*
				 *  Don't forget to increment 'p' back to where
				 *  it should be.  If we don't, then we will
				 *  never get past an "end quote."
				 */
				if ( endquote == 1 )
					p++;

				rdnstart = *p ? p + 1 : p;
				while ( isspace( *rdnstart ))
					++rdnstart;
			}
			break;
		}
	} while ( *p );

	return( rdns );

}

#ifdef _OS_NEXT_
#define STD_LIB_PATH_COUNT 3
char *STD_LIB_PATH[STD_LIB_PATH_COUNT] =
{
	"/NextLibrary",
	"/LocalLibrary",
	"~/Library"
};
#else
#define STD_LIB_PATH_COUNT 4
char *STD_LIB_PATH[STD_LIB_PATH_COUNT] =
{
	"/System/Library",
	"/Network/Library",
	"/Local/Library",
	"~/Library"
};
#endif

/*
 * Locate files in standard libraries
 */
char *ldap_locate_path( char *pathbuf, const char *file )
{
	int i;
	
	for (i = 0; i < STD_LIB_PATH_COUNT; i++)
	{
		strcpy(pathbuf, STD_LIB_PATH[i]);

		if (strlen(pathbuf) + strlen(file) + 1 > MAXPATHLEN)
			return NULL;
		
		strcat(pathbuf, file);
		
		if (access(pathbuf, F_OK) == 0)
			return pathbuf;
	}
	
	return NULL;
}
