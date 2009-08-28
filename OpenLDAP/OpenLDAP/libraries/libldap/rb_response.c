/*
 * Copyright (c) 2008 Apple Inc.  All Rights Reserved.
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
 * 
 * Does this need to be released under the OpenLDAP License instead of, or in
 * addition to the APSL?
 * 
 */

/*
 * Implementation of the Red Black tree to index the
 * ldap response messages.
 */

#ifdef LDAP_RESPONSE_RB_TREE

#include "portable.h"

#include <stdio.h>
#include <ac/stdlib.h>
#include <ac/unistd.h>

#include "ldap-int.h"

#include "rb.h"
#include "rb_response.h"
#include "ldap_rb_stats.h"

 
#define RBNODE_TO_LM(n) \
    ((LDAPMessage *)((uintptr_t)n - offsetof(LDAPMessage, lm_link)))

static int ldap_resp_rbt_compare_nodes( const struct rb_node *n1,
                                        const struct rb_node *n2 )
{
	const ber_int_t msgid1 = RBNODE_TO_LM(n1)->lm_msgid;
	const ber_int_t msgid2 = RBNODE_TO_LM(n2)->lm_msgid;

    if ( msgid1 < msgid2 ) {
        return -1;
    }
    if ( msgid1 > msgid2 ) {
        return 1;
    }
    return 0;
}

static int ldap_resp_rbt_compare_key( const struct rb_node *n,
                                      const void *v )
{
	const ber_int_t msgid1 = RBNODE_TO_LM(n)->lm_msgid;
	const ber_int_t msgid2 = *((ber_int_t*)v);

    if ( msgid1 < msgid2 ) {
        return -1;
    }
    if ( msgid1 > msgid2 ) {
        return 1;
    }
    return 0;
}

static const struct rb_tree_ops ldap_resp_rbt_ops = {
    .rbto_compare_nodes = ldap_resp_rbt_compare_nodes,
    .rbto_compare_key   = ldap_resp_rbt_compare_key,
};

void ldap_resp_rbt_create( LDAP *ld )
{
    assert( ld != NULL );

    ld->ld_rbt_responses = LDAP_CALLOC( 1, sizeof(*ld->ld_rbt_responses) );
    assert( ld->ld_rbt_responses != NULL );

    rb_tree_init( ld->ld_rbt_responses, &ldap_resp_rbt_ops );
}

void ldap_resp_rbt_free( LDAP *ld )
{
    LDAPMessage *lm;
    LDAPMessage *doomed;

    assert( ld != NULL ); 
    assert( ld->ld_rbt_responses != NULL );

    /* Free all of the response messages in the tree. */
    lm = ldap_resp_rbt_get_first_msg( ld );
    while ( lm ) {
        /* Since the rb tree will re-balance when we delete, get the next
         * message before removing it from the rb tree.
         */
        doomed = lm;
        lm = ldap_resp_rbt_get_next_msg( ld, lm );
        ldap_resp_rbt_delete_msg( ld, doomed );
    }

    /* Now that the tree is empty, free the tree itself. */
    free( ld->ld_rbt_responses );
    ld->ld_rbt_responses = NULL;
}

void ldap_resp_rbt_insert_msg( LDAP *ld, LDAPMessage *lm )
{
    assert( ld != NULL ); 
    assert( ld->ld_rbt_responses != NULL );

    rb_tree_insert_node( ld->ld_rbt_responses, &lm->lm_link );
    
    if ( LDAP_RB_STATS_COUNT_ENABLED() ) {
        LDAP_RB_STATS_COUNT( ld->ld_rbt_responses->rbt_count, lm->lm_msgid, lm );
    }
}

/* Removes the message from the rb tree & deletes the message. */
void ldap_resp_rbt_delete_msg( LDAP *ld, LDAPMessage *lm )
{
    assert( ld != NULL ); 
    assert( ld->ld_rbt_responses != NULL );
    assert( lm != NULL );

    rb_tree_remove_node( ld->ld_rbt_responses, &lm->lm_link );
    ldap_msgfree( lm );

   if ( LDAP_RB_STATS_COUNT_ENABLED() ) {
        LDAP_RB_STATS_COUNT( ld->ld_rbt_responses->rbt_count, lm->lm_msgid, lm );
    }
}

/* Removes the message from the rb tree, but does _not_ delete it. */
void ldap_resp_rbt_unlink_msg( LDAP *ld, LDAPMessage *lm)
{
    assert( ld != NULL ); 
    assert( ld->ld_rbt_responses != NULL );
    assert( lm != NULL );

    rb_tree_remove_node( ld->ld_rbt_responses, &lm->lm_link );

    if ( LDAP_RB_STATS_COUNT_ENABLED() ) {
        LDAP_RB_STATS_COUNT( ld->ld_rbt_responses->rbt_count, lm->lm_msgid, lm );
    }
}

/* Removes the message from the rb tree, but inserts the next message in
 * the first message's response chain.
 */
void ldap_resp_rbt_unlink_partial_msg( LDAP *ld, LDAPMessage *lm )
{
    LDAPMessage *nextInChain;

    assert( ld != NULL ); 
    assert( ld->ld_rbt_responses != NULL );
    assert( lm != NULL );

    rb_tree_remove_node( ld->ld_rbt_responses, &lm->lm_link );
    nextInChain = lm->lm_chain;
	nextInChain->lm_chain_tail = ( lm->lm_chain_tail != lm ) ? lm->lm_chain_tail : lm->lm_chain;
    rb_tree_insert_node( ld->ld_rbt_responses, &nextInChain->lm_link );

    lm->lm_chain = NULL;
    lm->lm_chain_tail = NULL;
}

LDAPMessage *ldap_resp_rbt_find_msg( LDAP *ld, ber_int_t msgid )
{
    struct rb_node* rbn = NULL;
    LDAPMessage* lm  = NULL;

    assert( ld != NULL ); 
    assert( ld->ld_rbt_responses != NULL );

    rbn = rb_tree_find_node( ld->ld_rbt_responses, &msgid );
    if ( rbn ) {
        lm = RBNODE_TO_LM( rbn );
    }

    return lm;
}

LDAPMessage *ldap_resp_rbt_get_first_msg( LDAP *ld )
{
    struct rb_node *rbn;
    LDAPMessage *lm = NULL;

    assert( ld != NULL ); 
    assert( ld->ld_rbt_responses != NULL );

    rbn = rb_tree_iterate( ld->ld_rbt_responses, NULL, RB_DIR_RIGHT );
    if ( rbn ) {
        lm = RBNODE_TO_LM( rbn );
    }

    return lm;
}

LDAPMessage *ldap_resp_rbt_get_next_msg( LDAP *ld, LDAPMessage *lm )
{
    struct rb_node *rbn;
    LDAPMessage *next = NULL;

    assert( ld != NULL ); 
    assert( ld->ld_rbt_responses != NULL );

    rbn = rb_tree_iterate( ld->ld_rbt_responses, &lm->lm_link, RB_DIR_LEFT );
    if ( rbn ) {
        next = RBNODE_TO_LM( rbn );
    }

    return next;
}

void ldap_resp_rbt_dump( LDAP* ld )
{
    LDAPMessage	*lm;
	LDAPMessage *l;
    struct rb_node *rbn;

    assert( ld != NULL ); 
    assert( ld->ld_rbt_responses != NULL );

	fprintf( stderr, "** ld %p Red-Black Tree Response Queue:\n", (void *)ld );
    rbn = rb_tree_iterate( ld->ld_rbt_responses, NULL, RB_DIR_RIGHT );
	if ( rbn == NULL ) {
		fprintf( stderr, "   Empty\n" );
	}
	for ( ; rbn != NULL; rbn = rb_tree_iterate( ld->ld_rbt_responses, rbn, RB_DIR_LEFT ) )
	{
        lm = RBNODE_TO_LM(rbn);
		fprintf( stderr, " * msgid %d,  type %lu\n",
		        lm->lm_msgid,
		        (unsigned long) lm->lm_msgtype );

        l = lm->lm_chain;;
		if ( l != NULL )
		{
			fprintf( stderr, "   chained responses:\n" );
			for ( ; l != NULL; l = l->lm_chain )
			{
				fprintf( stderr,
				        "  * msgid %d,  type %lu\n",
				        l->lm_msgid,
				        (unsigned long) l->lm_msgtype );
			}
		}
	}
}

#endif /* LDAP_RESPONSE_RB_TREE */
