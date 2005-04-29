/*
 * Copyright (c) 2004, Apple Computer, Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>

#include <libbsm.h>

char *delim = ",";

/*
 * Convert the character representation of audit values 
 * into the au_mask_t field 
 */ 
int getauditflagsbin(char *auditstr, au_mask_t *masks)
{
	char *tok;
	char sel, sub;
	struct au_class_ent *c;
	char *last;
	
	if((auditstr == NULL) || (masks == NULL)) {
		return -1;
	}
	
	masks->am_success = 0;
	masks->am_failure = 0;

	tok = strtok_r(auditstr, delim, &last);
	while(tok != NULL) {

		/* check for the events that should not be audited */
		if(tok[0] == '^') {
			sub = 1;
			tok++;
		}
		else {
			sub = 0;
		}
					
		/* check for the events to be audited for success */
		if(tok[0] == '+') {
			sel = AU_PRS_SUCCESS;
			tok++;
		}
		else if(tok[0] == '-') {
			sel = AU_PRS_FAILURE;
			tok++;
		}
		else {
			sel = AU_PRS_BOTH;
		}

		if((c = getauclassnam(tok)) != NULL) {
			if(sub) {
				SUB_FROM_MASK(masks, c->ac_class, sel);
			}
			else {
				ADD_TO_MASK(masks, c->ac_class, sel);	
			}
			free_au_class_ent(c);
		} else {
			return -1;
		}	

		/* Get the next class */
		tok = strtok_r(NULL, delim, &last);
	}
	return 0;
}

/*
 * Convert the au_mask_t fields into a string value
 * If verbose is non-zero the long flag names are used 
 * else the short (2-character)flag names are used 
 */  
int getauditflagschar(char *auditstr, au_mask_t *masks, int verbose)
{
	struct au_class_ent *c;
	char *strptr = auditstr;
	u_char sel;
	
	if((auditstr == NULL) || (masks == NULL)) {
		return -1;
	}
		
	/* 
	 * Enumerate the class entries, check if each is selected 
	 * in either the success or failure masks
	 */ 

	for (setauclass(); (c = getauclassent()) != NULL; free_au_class_ent(c)) {

		sel = 0;

		/* Dont do anything for class = no */
		if(c->ac_class == 0) {
			continue;
		}

		sel |= ((c->ac_class & masks->am_success) == c->ac_class) ? AU_PRS_SUCCESS : 0; 
		sel |= ((c->ac_class & masks->am_failure) == c->ac_class) ? AU_PRS_FAILURE : 0;

		/* 
		 * No prefix should be attached if both 
 		 * success and failure are selected 
		 */
		if((sel & AU_PRS_BOTH) == 0) {
			if((sel & AU_PRS_SUCCESS) != 0) {
				*strptr = '+';			
				strptr = strptr + 1;
			}
			else if((sel & AU_PRS_FAILURE) != 0) {
				*strptr = '-';			
				strptr = strptr + 1;
			}
		}

		if(sel != 0) {
			if(verbose) {
				strcpy(strptr, c->ac_desc);
				strptr += strlen(c->ac_desc);
			}
			else {
				strcpy(strptr, c->ac_name);
				strptr += strlen(c->ac_name);
			}
			*strptr = ','; /* delimiter */
			strptr = strptr + 1;
		}
	}

	/* Overwrite the last delimiter with the string terminator */
	if(strptr != auditstr) {
		*(strptr-1) = '\0';
	}
		
	return 0;	
}

