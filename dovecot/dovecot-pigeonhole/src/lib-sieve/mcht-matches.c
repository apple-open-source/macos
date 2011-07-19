/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */
 
/* Match-type ':matches' 
 */

#include "lib.h"
#include "str.h"

#include "sieve-match-types.h"
#include "sieve-comparators.h"
#include "sieve-match.h"

#include <string.h>
#include <stdio.h>

/*
 * Forward declarations
 */

static int mcht_matches_match_key
	(struct sieve_match_context *mctx, const char *val, size_t val_size, 
		const char *key, size_t key_size);

/*
 * Match-type object
 */

const struct sieve_match_type_def matches_match_type = {
	SIEVE_OBJECT("matches", &match_type_operand, SIEVE_MATCH_TYPE_MATCHES),
	NULL,
	sieve_match_substring_validate_context, 
	NULL, NULL, NULL,
	mcht_matches_match_key,
	NULL
};

/*
 * Match-type implementation
 */

/* Quick 'n dirty debug */
//#define MATCH_DEBUG
#ifdef MATCH_DEBUG
#define debug_printf(...) printf ("match debug: " __VA_ARGS__)
#else
#define debug_printf(...) 
#endif

/* FIXME: Naive implementation, substitute this with dovecot src/lib/str-find.c
 */
static inline bool _string_find(const struct sieve_comparator *cmp, 
	const char **valp, const char *vend, const char **keyp, const char *kend)
{
	while ( (*valp < vend) && (*keyp < kend) ) {
		if ( !cmp->def->char_match(cmp, valp, vend, keyp, kend) )
			(*valp)++;
	}
	
	return (*keyp == kend);
}

static char _scan_key_section
	(string_t *section, const char **wcardp, const char *key_end)
{
	/* Find next wildcard and resolve escape sequences */	
	str_truncate(section, 0);
	while ( *wcardp < key_end && **wcardp != '*' && **wcardp != '?') {
		if ( **wcardp == '\\' ) {
			(*wcardp)++;
		}
		str_append_c(section, **wcardp);
		(*wcardp)++;
	}
	
	/* Record wildcard character or \0 */
	if ( *wcardp < key_end ) {			
		return **wcardp;
	} 
	
	i_assert( *wcardp == key_end );
	return '\0';
}

static int mcht_matches_match_key
(struct sieve_match_context *mctx, const char *val, size_t val_size, 
	const char *key, size_t key_size)
{
	const struct sieve_comparator *cmp = mctx->comparator;
	struct sieve_match_values *mvalues;
	string_t *mvalue = NULL, *mchars = NULL;
	string_t *section, *subsection;
	const char *vend, *kend, *vp, *kp, *wp, *pvp;
	bool backtrack = FALSE; /* TRUE: match of '?'-connected sections failed */
	char wcard = '\0';      /* Current wildcard */
	char next_wcard = '\0'; /* Next  widlcard */
	unsigned int key_offset = 0;

	if ( cmp->def == NULL || cmp->def->char_match == NULL )
		return FALSE;

	/* Key sections */
	section = t_str_new(32);    /* Section (after beginning or *) */
	subsection = t_str_new(32); /* Sub-section (after ?) */
	
	/* Mark end of value and key */
	vend = (const char *) val + val_size;
	kend = (const char *) key + key_size;

	/* Initialize pointers */
	vp = val;                   /* Value pointer */
	kp = key;                   /* Key pointer */
	wp = key;                   /* Wildcard (key) pointer */
	pvp = val;                  /* Previous value Pointer */

	/* Start match values list if requested */
	if ( (mvalues = sieve_match_values_start(mctx->runenv)) != NULL ) {
		/* Skip ${0} for now; added when match succeeds */
		sieve_match_values_add(mvalues, NULL);

		mvalue = t_str_new(32);     /* Match value (*) */
		mchars = t_str_new(32);     /* Match characters (.?..?.??) */
	}
	
	/* Match the pattern: 
	 *   <pattern> = <section>*<section>*<section>...
	 *   <section> = <sub-section>?<sub-section>?<sub-section>...
	 *
	 * Escape sequences \? and \* need special attention. 
	 */
	 
	debug_printf("=== Start ===\n");
	debug_printf("  key:   %s\n", t_strdup_until(key, kend));
	debug_printf("  value: %s\n", t_strdup_until(val, vend));

	/* Loop until either key or value ends */
	while (kp < kend && vp < vend ) {
		const char *needle, *nend;
		
		if ( !backtrack ) {
			/* Search the next '*' wildcard in the key string */

			wcard = next_wcard;
			
			/* Find the needle to look for in the string */	
			key_offset = 0;	
			for (;;) {
				next_wcard = _scan_key_section(section, &wp, kend);
				
				if ( wcard == '\0' || str_len(section) > 0 ) 
					break;
					
				if ( next_wcard == '*' ) {	
					break;
				}
					
				if ( wp < kend ) 
					wp++;
				else 
					break;
				key_offset++;
			}
			
			debug_printf("found wildcard '%c' at pos [%d]\n", 
				next_wcard, (int) (wp-key));
	
			if ( mvalues != NULL )			
				str_truncate(mvalue, 0);
		} else {
			/* Backtracked; '*' wildcard is retained */
			debug_printf("backtracked");
			backtrack = FALSE;
		}
		
		/* Determine what we are looking for */
		needle = str_c(section);
		nend = PTR_OFFSET(needle, str_len(section));		
		 
		debug_printf("  section needle:  '%s'\n", t_strdup_until(needle, nend));
		debug_printf("  section key:     '%s'\n", t_strdup_until(kp, kend));
		debug_printf("  section remnant: '%s'\n", t_strdup_until(wp, kend));
		debug_printf("  value remnant:   '%s'\n", t_strdup_until(vp, vend));
		debug_printf("  key offset:      %d\n", key_offset);
		
		pvp = vp;
		if ( next_wcard == '\0' ) {
			/* No more wildcards; find the needle substring at the end of string */
	
			const char *qp, *qend;
			
			debug_printf("next_wcard = NUL; must find needle at end\n");				 

			/* Check if the value is still large enough */			
			if ( vend - str_len(section) < vp ) {
				debug_printf("  wont match: value is too short\n");
				break;
			}

			/* Move value pointer to where the needle should be */
			vp = PTR_OFFSET(vend, -str_len(section));

			/* Record match values */
			qend = vp;
			qp = vp - key_offset;
		
			if ( mvalues != NULL )
				str_append_n(mvalue, pvp, qp-pvp);
					
			/* Compare needle to end of value string */
			if ( !cmp->def->char_match(cmp, &vp, vend, &needle, nend) ) {	
				debug_printf("  match at end failed\n");				 
				break;
			}
			
			/* Add match values */
			if ( mvalues != NULL ) {
				/* Append '*' match value */
				sieve_match_values_add(mvalues, mvalue);

				/* Append any initial '?' match values */
				for ( ; qp < qend; qp++ )
					sieve_match_values_add_char(mvalues, *qp); 
			}

			/* Finish match */
			kp = kend;
			vp = vend;

			debug_printf("  matched end of value\n");
			break;
		} else {
			/* Next wildcard found; match needle before next wildcard */

			const char *prv = NULL; /* Stored value pointer for backtrack */
			const char *prk = NULL; /* Stored key pointer for backtrack */
			const char *prw = NULL; /* Stored wildcard pointer for backtrack */
			const char *chars;

			/* Reset '?' match values */
			if ( mvalues != NULL )		
				str_truncate(mchars, 0);
							
			if ( wcard == '\0' ) {
				/* No current wildcard; match needs to happen right at the beginning */
				debug_printf("wcard = NUL; needle should be found at the beginning.\n");
				debug_printf("  begin needle: '%s'\n", t_strdup_until(needle, nend));
				debug_printf("  begin value:  '%s'\n", t_strdup_until(vp, vend));

				if ( !cmp->def->char_match(cmp, &vp, vend, &needle, nend) ) {	
					debug_printf("  failed to find needle at beginning\n");				 
					break;
				}

			} else {
				/* Current wildcard present; match needle between current and next wildcard */
				debug_printf("wcard != NUL; must find needle at an offset (>= %d).\n",
					key_offset);

				/* Match may happen at any offset (>= key offset): find substring */				
				vp += key_offset;
				if ( (vp >= vend) || !_string_find(cmp, &vp, vend, &needle, nend) ) {
					debug_printf("  failed to find needle at an offset\n"); 
					break;
				}

				prv = vp - str_len(section);
				prk = kp;
				prw = wp;		
	
				/* Append match values */
				if ( mvalues != NULL ) {
					const char *qend = vp - str_len(section);
					const char *qp = qend - key_offset;

					/* Append '*' match value */
					str_append_n(mvalue, pvp, qp-pvp);

					/* Append any initial '?' match values (those that caused the key
					 * offset.
					 */
					for ( ; qp < qend; qp++ )
						str_append_c(mchars, *qp);
				}
			}
			
			/* Update wildcard and key pointers for next wildcard scan */
			if ( wp < kend ) wp++;
			kp = wp;
		
			/* Scan successive '?' wildcards */
			while ( next_wcard == '?' ) {
				debug_printf("next_wcard = '?'; need to match arbitrary character\n");
				
				/* Add match value */ 
				if ( mvalues != NULL )
					str_append_c(mchars, *vp);

				vp++;

				/* Scan for next '?' wildcard */				
				next_wcard = _scan_key_section(subsection, &wp, kend);
				debug_printf("found next wildcard '%c' at pos [%d] (fixed match)\n", 
					next_wcard, (int) (wp-key));
					
				/* Determine what we are looking for */
				needle = str_c(subsection);
				nend = PTR_OFFSET(needle, str_len(subsection));

				debug_printf("  sub key:       '%s'\n", t_strdup_until(needle, nend));
				debug_printf("  value remnant: '%s'\n", vp <= vend ? t_strdup_until(vp, vend) : "");

				/* Try matching the needle at fixed position */
				if ( (needle == nend && next_wcard == '\0' && vp < vend ) || 
					!cmp->def->char_match(cmp, &vp, vend, &needle, nend) ) {	
					
					/* Match failed: now we have a problem. We need to backtrack to the previous
					 * '*' wildcard occurence and start scanning for the next possible match.
					 */

					debug_printf("  failed fixed match\n");
					
					/* Start backtrack */
					if ( prv != NULL && prv + 1 < vend ) {
						/* Restore pointers */
						vp = prv;
						kp = prk;
						wp = prw;
				
						/* Skip forward one value character to scan the next possible match */
						if ( mvalues != NULL )
							str_append_c(mvalue, *vp);
						vp++;
				
						/* Set wildcard state appropriately */
						wcard = '*';
						next_wcard = '?';
				
						/* Backtrack */
						backtrack = TRUE;				 

						debug_printf("  BACKTRACK\n");
					}

					/* Break '?' wildcard scanning loop */
					break;
				}
				
				/* Update wildcard and key pointers for next wildcard scan */
				if ( wp < kend ) wp++;
				kp = wp;
			}
			
			if ( !backtrack ) {
				unsigned int i;
				
				if ( next_wcard == '?' ) {
					debug_printf("failed to match '?'\n");	
					break;
				}
				
				if ( mvalues != NULL ) {
					if ( prv != NULL )
						sieve_match_values_add(mvalues, mvalue);

					chars = (const char *) str_data(mchars);

					for ( i = 0; i < str_len(mchars); i++ ) {
						sieve_match_values_add_char(mvalues, chars[i]);
					}
				}

				if ( next_wcard != '*' ) {
					debug_printf("failed to match at end of string\n");
					break;
				}
			}
		}
					
		/* Check whether string ends in a wildcard 
		 * (avoid scanning the rest of the string)
		 */
		if ( kp == kend && next_wcard == '*' ) {
			/* Add the rest of the string as match value */
			if ( mvalues != NULL ) {
				str_truncate(mvalue, 0);
				str_append_n(mvalue, vp, vend-vp);
				sieve_match_values_add(mvalues, mvalue);
			}
		
			/* Finish match */
			kp = kend;
			vp = vend;
		
			debug_printf("key ends with '*'\n");
			break;
		}			
					
		debug_printf("== Loop ==\n");
	}

	/* Eat away a trailing series of *s */
	if ( vp == vend ) {
		while ( kp < kend && *kp == '*' ) kp++;
	}

	/* By definition, the match is only successful if both value and key pattern
	 * are exhausted.
	 */
	
	debug_printf("=== Finish ===\n");
	debug_printf("  result: %s\n", (kp == kend && vp == vend) ? "true" : "false");
	
	if (kp == kend && vp == vend) {
		/* Activate new match values after successful match */
		if ( mvalues != NULL ) {
			/* Set ${0} */
			string_t *matched = str_new_const(pool_datastack_create(), val, val_size);
			sieve_match_values_set(mvalues, 0, matched);

			/* Commit new match values */
			sieve_match_values_commit(mctx->runenv, &mvalues);
		}
		return TRUE;
	}

	/* No match; drop collected match values */
	sieve_match_values_abort(&mvalues);
	return FALSE;
}
			 
