#include <sys/types.h>
#include <sys/queue.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <libbsm.h>

/* array of used descriptors */
static au_record_t* open_desc_table[MAX_AUDIT_RECORDS]; 

/* The current number of active record descriptors */ 
static int bsm_rec_count = 0; 
/* 
 * Records that can be recycled are maintained in the list given below
 * The maximum number of elements that can be present in this list is
 * bounded by MAX_AUDIT_RECORDS. Memory allocated for these records are never
 * freed 
 */ 

LIST_HEAD(, au_record) bsm_free_q;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* 
 * This call frees a token_t and its internal data.  
 *
 * XXX  Should it be a macro for speed?  
 */
void au_free_token(token_t *tok)
{
    if (tok)
    {
		if (tok->t_data)
			free(tok->t_data);
		free(tok);
    }
}

/*
 * This call reserves memory for the audit record. 
 * Memory must be guaranteed before any auditable event can be
 * generated. 
 * The au_record_t structure maintains a reference to the
 * memory allocated above and also the list of tokens associated 
 * with this record
 * Descriptors are recyled once the records are added to the audit 
 * trail following au_close(). 
 */  
int au_open(void)
{	
	au_record_t *rec = NULL;
	
	pthread_mutex_lock(&mutex);

	if(bsm_rec_count == 0) {
		LIST_INIT(&bsm_free_q);
	}

	/* 
	 * Find an unused descriptor, remove it from the free list, mark as used
	 */  
	if (!LIST_EMPTY(&bsm_free_q)) {
		rec = LIST_FIRST(&bsm_free_q);
		rec->used = 1;
		LIST_REMOVE(rec, au_rec_q);
	}	

	pthread_mutex_unlock(&mutex);

	if(rec == NULL) {
		/*
		 * Create a new au_record_t if no descriptors are available 
		 */
		rec = (au_record_t *) malloc (sizeof(au_record_t));
		if(rec == NULL) {
			return -1; /* Failed */
		}
		rec->data = (u_char *) malloc (MAX_AUDIT_RECORD_SIZE * sizeof(u_char));
		if(rec->data == NULL) {
			free(rec);
			return -1;
		}

		pthread_mutex_lock(&mutex);

		if(bsm_rec_count == MAX_AUDIT_RECORDS) {
			pthread_mutex_unlock(&mutex);
			free(rec->data);
			free(rec);

			/* XXX We need to increase size of MAX_AUDIT_RECORDS */
			return -1;
		}
		rec->desc = bsm_rec_count;
		open_desc_table[bsm_rec_count] = rec;
		bsm_rec_count++;

		pthread_mutex_unlock(&mutex);

	}

	memset(rec->data, 0, MAX_AUDIT_RECORD_SIZE);

	TAILQ_INIT(&rec->token_q);
	rec->len = 0;
	rec->used = 1;

	return rec->desc;
}

/*
 * Store the token with the record descriptor
 */ 
int au_write(int d, token_t *tok)
{
	au_record_t *rec;
		
	if(tok == NULL) {
		return -1; /* Invalid Token */
	}		

	/* Write the token to the record descriptor */
	rec = open_desc_table[d];	
	if((rec == NULL) || (rec->used == 0)) {
		return -1; /* Invalid descriptor */
	}

	/* Add the token to the tail */
	/* 
	 * XXX Not locking here -- we should not be writing to
	 * XXX the same descriptor from different threads
	 */ 
	TAILQ_INSERT_TAIL(&rec->token_q, tok, tokens);

	rec->len += tok->len; /* grow record length by token size bytes */
	
	/* Token should not be available after this call */	
	tok = NULL;
	return 0; /* Success */
}

/*
 * Add the header token, identify any missing tokens
 * Write out the tokens to the record memory and finally, 
 * call audit
 */
int au_close(int d, int keep, short event)
{
	au_record_t *rec;
	u_char *dptr;
	size_t tot_rec_size;
	token_t *tok, *hdr, *trail;
	int retval = 0;
		
	rec = open_desc_table[d];
	if((rec == NULL) || (rec->used == 0)) {
		return -1; /* Invalid descriptor */
	}	
	
	tot_rec_size = rec->len + HEADER_SIZE + TRAILER_SIZE;
	if(keep && (tot_rec_size <= MAX_AUDIT_RECORD_SIZE)) {
		/* Create the header token */
		/* No modifier for libbsm records */
		hdr = au_to_header32(tot_rec_size, event, 0);
			
		if(hdr != NULL) {
			/* Add to head of list */
			TAILQ_INSERT_HEAD(&rec->token_q, hdr, tokens);

			trail = au_to_trailer(tot_rec_size);
			if(trail != NULL) {
				/* Add to tail of list */
				TAILQ_INSERT_TAIL(&rec->token_q, trail, tokens);
			}
		}
		/* Serialize token data to the record */

		rec->len = tot_rec_size;
		dptr = rec->data;

		TAILQ_FOREACH(tok, &rec->token_q, tokens) {
			memcpy(dptr, tok->t_data, tok->len);		
			dptr += tok->len;
		}

		/* Call the kernel interface to audit */
		retval = audit(rec->data, rec->len);
	}

	/* CLEANUP */

	/* Free the token list */
	while ((tok = TAILQ_FIRST(&rec->token_q))) {
		TAILQ_REMOVE(&rec->token_q, tok, tokens);
		free(tok->t_data);
		free(tok);
	}	

	rec->used = 0;
	rec->len = 0;	

	pthread_mutex_lock(&mutex);

	/* Add the record to the freelist tail */
	LIST_INSERT_HEAD(&bsm_free_q, rec, au_rec_q);

	pthread_mutex_unlock(&mutex);

	return retval; 
}
 
