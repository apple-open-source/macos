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

