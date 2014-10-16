#include <portable.h>

#include <ac/string.h>
#include <slap.h>
#include <lutil.h>

/* Need dynacl... */

#ifdef SLAP_DYNACL

#define MAX_ACE_LEN 1024
/* idattr_type */
typedef enum idattr_type_e
{
	UUID_TYPE = 0,
	SID_TYPE,
	OWNER_TYPE,
	USERS_TYPE,
	SELFWRITE_TYPE
} idattr_type_t;

typedef struct idattr_t {
	slap_style_t		idattr_style;
	struct berval		idattr_pat;
	struct berval		idattr_applyto_uuid;
	struct berval		idattr_applyto_dn;
	struct berval		idattr_owner_dn;
	idattr_type_t		idattr_type;
	ber_tag_t			idattr_ops;
	u_int32_t					idattr_applyto;
	struct berval		idattr_selfattr;
	AttributeDescription *idattr_selfattrDesc;
	struct berval		idattr_boolattr;
	AttributeDescription *idattr_boolattrDesc;
} idattr_t;

typedef struct idattr_id_to_dn_t {
	u_int32_t		found;
	struct berval	target_dn;
} idattr_id_to_dn_t;

typedef struct idattr_ismember_t {
	u_int32_t				found;
} idattr_ismember_t;

static ObjectClass		*idattr_posixGroup;
AttributeDescription	*idattr_memberUid;
static ObjectClass		*idattr_posixAccount;
static AttributeDescription	*idattr_uidNumber;

static ObjectClass		*idattr_apple_user;
static ObjectClass		*idattr_extensible_object;
AttributeDescription	*idattr_uuid;
static AttributeDescription	*idattr_owneruuid;
static AttributeDescription	*idattr_sid;
AttributeDescription	*idattr_memberships;
static AttributeDescription	*idattr_expandedmemberships;

static const char *group_subtree="cn=groups";

static int idattr_dynacl_destroy( void *priv );

static int idattr_dynacl_set_boolattr(char *attr, idattr_t *id)
{

	if (attr == NULL) {
		Debug( LDAP_DEBUG_ACL, "idattr_dynacl_set_boolattr: BOOLATTR NOT APPLICABLE\n", 0, 0, 0 );			
	} else {
		ber_str2bv( attr, 0, 1, &id->idattr_boolattr );
		Debug( LDAP_DEBUG_ACL, "idattr_dynacl_set_boolattr: (%s)BOOLATTR  len[%d] value[%s]\n",attr , id->idattr_boolattr.bv_len, id->idattr_boolattr.bv_val );			
	}

	return 0;
}

static int idattr_dynacl_set_selfattr(char *attr, idattr_t *id)
{

	if (attr == NULL) {
		Debug( LDAP_DEBUG_ACL, "idattr_dynacl_set_selfattr: SELFATTR NOT APPLICABLE\n", 0, 0, 0 );			
	} else {
		ber_str2bv( attr, 0, 1, &id->idattr_selfattr );
		Debug( LDAP_DEBUG_ACL, "idattr_dynacl_set_selfattr: (%s)SELFATTR  len[%d] value[%s]\n",attr , id->idattr_selfattr.bv_len, id->idattr_selfattr.bv_val );			
	}

	return 0;
}

static int idattr_dynacl_set_applyto(char *applyto, idattr_t *id)
{

	if (applyto == NULL) {
		Debug( LDAP_DEBUG_ACL, "idattr_dynacl_set_applyto: APPLY TO ALL\n", 0, 0, 0 );			
	} else {
		ber_str2bv( applyto, 0, 1, &id->idattr_applyto_uuid );
		id->idattr_applyto = 1;
		Debug( LDAP_DEBUG_ACL, "idattr_dynacl_set_applyto: (%s)APPLYTO  len[%d] value[%s]\n",applyto , id->idattr_applyto_uuid.bv_len, id->idattr_applyto_uuid.bv_val );			
	}

	return 0;
}

static int idattr_dynacl_set_operations(char *ops, idattr_t *id)
{

	if (ops == NULL) {
		Debug( LDAP_DEBUG_ACL, "idattr_dynacl_parse_ops: APPLY TO ALL\n", ops, 0, 0 );			
		id->idattr_ops = 0;	
	} else if (strcmp("BIND",ops) == 0) {
		id->idattr_ops = LDAP_REQ_BIND;
	} else if (strcmp("UNBIND",ops) == 0) {
		id->idattr_ops = LDAP_REQ_UNBIND;
	} else if (strcmp("SEARCH",ops) == 0) {
		id->idattr_ops = LDAP_REQ_SEARCH;
	} else if (strcmp("MODIFY",ops) == 0) {
		id->idattr_ops = LDAP_REQ_MODIFY;
	} else if (strcmp("ADD",ops) == 0) {
		id->idattr_ops = LDAP_REQ_ADD;
	} else if (strcmp("DELETE",ops) == 0) {
		id->idattr_ops = LDAP_REQ_DELETE;
	} else if (strcmp("RENAME",ops) == 0) {
		id->idattr_ops = LDAP_REQ_MODDN;
	} else if (strcmp("COMPARE",ops) == 0) {
		id->idattr_ops = LDAP_REQ_COMPARE;
	} else if (strcmp("ABANDON",ops) == 0) {
		id->idattr_ops = LDAP_REQ_ABANDON;
	} else if (strcmp("EXTENDED",ops) == 0) {
		id->idattr_ops = LDAP_REQ_EXTENDED;
	} else {
		Debug( LDAP_DEBUG_ACL, "idattr_dynacl_parse_ops: UNKNOWN OPT - DEFAULT: APPLY TO ALL\n", ops, 0, 0 );			
		id->idattr_ops = 0;	
	}
	Debug( LDAP_DEBUG_ACL, "idattr_dynacl_parse_ops: opts (%s) op_cmd(%d)\n", ops, id->idattr_ops, 0 );

	return 0;
}

#ifdef IDATTR_DEBUG
static int __attribute__ ((noinline)) idattr_dynacl_parse_ops(const char	*ops, idattr_t *id)
#else
static int idattr_dynacl_parse_ops(const char	*ops, idattr_t *id)
#endif
{
	Debug( LDAP_DEBUG_ACL, "idattr_dynacl_parse_ops: opts (%s) \n", ops, 0, 0 );

	char *tmp = NULL, *current = NULL;
	char *original = NULL;
	char *option = NULL, *value = NULL;
	
	option = tmp;
	
	if (ops) {
		tmp = current = original = ch_strdup(ops);
	}
	
	while (current) {
		// option:value;option:value.exact=
		if (current[0] == '.')
			break;
		// option
		tmp = strsep(&current, ":");
		if (tmp != NULL)
			option = tmp;
		else
			option = NULL;

		// value
		tmp = strsep(&current, ";");
		if (tmp != NULL)
			value = tmp;
		else
			value = NULL;

		if (option && value) {
			Debug( LDAP_DEBUG_ACL, "idattr_dynacl_parse_ops: option(%s)  value(%s)\n", option, value, 0 );
			if (strcmp("OP",option) == 0) {
				idattr_dynacl_set_operations(value, id);
			} else if (strcmp("APPLYTO",option) == 0) {
				idattr_dynacl_set_applyto(value, id);
			} else if (strcmp("SELFATTR",option) == 0) {
				idattr_dynacl_set_selfattr(value, id);				
			} else if (strcmp("BOOLATTR",option) == 0) {
				idattr_dynacl_set_boolattr(value, id);				
			}
		}
	}
	if (original)
		ch_free(original);
	
	return 0;
}

static int
idattr_dynacl_parse(
	const char	*fname,
	int 		lineno,
	const char	*opts,
	slap_style_t	style,
	const char	*pattern,
	void		**privp )
{
	idattr_t		*id;
	int		rc;
	const char	*text = NULL;

	id = ch_calloc( 1, sizeof( idattr_t ) );

	id->idattr_style = style;

	idattr_dynacl_parse_ops(opts, id);
	
	switch ( id->idattr_style ) {
	case ACL_STYLE_BASE:
	case ACL_STYLE_EXPAND:
		ber_str2bv( pattern, 0, 1, &id->idattr_pat );
		break;

	default:
		fprintf( stderr, "%s line %d: idattr ACL: "
			"unsupported style \"%s\".\n",
			fname, lineno, style_strings[ id->idattr_style ] );
		goto cleanup;
	}

	if (strncmp("S-1",pattern, 3) == 0)  {
		id->idattr_type = SID_TYPE;
	} else if (strcmp("OWNER",pattern) == 0) {
		id->idattr_type = OWNER_TYPE;
	} else if (strcmp("USERS",pattern) == 0) {	
		id->idattr_type = USERS_TYPE;
	} else if (strcmp("SELFWRITE",pattern) == 0) {
		id->idattr_type = SELFWRITE_TYPE;
	}
	
	if (idattr_posixGroup == NULL) {
		idattr_posixGroup = oc_find( "posixGroup" );
		if ( idattr_posixGroup == NULL ) {
			fprintf( stderr, "%s line %d: idattr ACL: "
				"unable to lookup \"posixGroup\" "
				"objectClass.\n",
				fname, lineno );
			goto cleanup;
		}
	}
	if (idattr_posixAccount == NULL) {
		idattr_posixAccount = oc_find( "posixAccount" );
		if ( idattr_posixGroup == NULL ) {
			fprintf( stderr, "%s line %d: idattr ACL: "
				"unable to lookup \"posixAccount\" "
				"objectClass.\n",
				fname, lineno );
			goto cleanup;
		}
	}
	if (idattr_apple_user == NULL) {	
		idattr_apple_user = oc_find( "apple-user" );
		if ( idattr_apple_user == NULL ) {
			fprintf( stderr, "%s line %d: idattr ACL: "
				"unable to lookup \"apple-user\" "
				"objectClass.\n",
				fname, lineno );
			goto cleanup;
		}
	}
	if (idattr_extensible_object == NULL) {		
		idattr_extensible_object = oc_find( "extensibleObject" );
		if ( idattr_extensible_object == NULL ) {
			fprintf( stderr, "%s line %d: idattr ACL: "
				"unable to lookup \"extensibleObject\" "
				"objectClass.\n",
				fname, lineno );
			goto cleanup;
		}
	}
	if (idattr_memberUid == NULL) {		
		rc = slap_str2ad( "memberUid", &idattr_memberUid, &text );
		if ( rc != LDAP_SUCCESS ) {
			fprintf( stderr, "%s line %d: idattr ACL: "
				"unable to lookup \"memberUid\" "
				"attributeDescription (%d: %s).\n",
				fname, lineno, rc, text );
			goto cleanup;
		}
	}
	if (idattr_uidNumber == NULL) {		
		rc = slap_str2ad( "uidNumber", &idattr_uidNumber, &text );
		if ( rc != LDAP_SUCCESS ) {
			fprintf( stderr, "%s line %d: idattr ACL: "
				"unable to lookup \"uidNumber\" "
				"attributeDescription (%d: %s).\n",
				fname, lineno, rc, text );
			goto cleanup;
		}
	}
	if (idattr_uuid == NULL) {		
		rc = slap_str2ad( "apple-generateduid", &idattr_uuid, &text );
		if ( rc != LDAP_SUCCESS ) {
			fprintf( stderr, "%s line %d: idattr ACL: "
				"unable to lookup \"apple-generateduid\" "
				"attributeDescription (%d: %s).\n",
				fname, lineno, rc, text );
			goto cleanup;
		}
	}
	if (idattr_owneruuid == NULL) {		
		rc = slap_str2ad( "apple-ownerguid", &idattr_owneruuid, &text );
		if ( rc != LDAP_SUCCESS ) {
			fprintf( stderr, "%s line %d: idattr ACL: "
				"unable to lookup \"apple-ownerguid\" "
				"attributeDescription (%d: %s).\n",
				fname, lineno, rc, text );
			goto cleanup;
		}
	}
	
	if (idattr_sid == NULL) {		
		rc = slap_str2ad( "sambaSID", &idattr_sid, &text );
		if ( rc != LDAP_SUCCESS ) {
			fprintf( stderr, "%s line %d: idattr ACL: "
				"unable to lookup \"sambaSID\" "
				"attributeDescription (%d: %s).\n",
				fname, lineno, rc, text );
			goto cleanup;
		}
	}
	if (idattr_memberships == NULL) {		
		rc = slap_str2ad( "apple-group-memberguid", &idattr_memberships, &text );
		if ( rc != LDAP_SUCCESS ) {
			fprintf( stderr, "%s line %d: idattr ACL: "
				"unable to lookup \"memberships\" "
				"attributeDescription (%d: %s).\n",
				fname, lineno, rc, text );
			goto cleanup;
		}
	}
	#ifdef USES_EXPANDED_MEMBERSHIPS
	if (idattr_expandedmemberships == NULL) {		
		rc = slap_str2ad( "apple-group-expandednestedgroup", &idattr_expandedmemberships, &text );
		if ( rc != LDAP_SUCCESS ) {
			fprintf( stderr, "%s line %d: idattr ACL: "
				"unable to lookup \"memberships\" "
				"attributeDescription (%d: %s).\n",
				fname, lineno, rc, text );
			goto cleanup;
		}
	}
	#endif
	if (id->idattr_type == SELFWRITE_TYPE ) {
		if (!BER_BVISNULL( &id->idattr_selfattr)) {
			rc = slap_str2ad( id->idattr_selfattr.bv_val, &id->idattr_selfattrDesc, &text );
			if ( rc != LDAP_SUCCESS ) {
				fprintf( stderr, "%s line %d: idattr ACL: "
					"unable to lookup SELFATTR \"%s\" "
					"attributeDescription (%d: %s).\n",
					fname, lineno, id->idattr_selfattr.bv_val, rc, text );
				goto cleanup;
			}
		} else {
			fprintf( stderr, "%s line %d: idattr ACL: SELFATTR required for SELFWRITE - dynacl/idattr/SELFATTR:<ATTRIBUTE>.exact=SELFWRITE", fname, lineno);
			goto cleanup;
		}
	}
	if (!BER_BVISNULL( &id->idattr_boolattr)) {
		rc = slap_str2ad( id->idattr_boolattr.bv_val, &id->idattr_boolattrDesc, &text );
		if ( rc != LDAP_SUCCESS ) {
			fprintf( stderr, "%s line %d: idattr ACL: "
				"unable to lookup BOOLATTR \"%s\" "
				"attributeDescription (%d: %s).\n",
				fname, lineno, id->idattr_boolattr.bv_val, rc, text );
			goto cleanup;
		}
	}

	*privp = (void *)id;
	return 0;

cleanup:
	(void)idattr_dynacl_destroy( (void *)id );

	return 1;
}
idattr_is_member_cb (
	Operation	*op,
	SlapReply	*rs
)
{
	Debug(LDAP_DEBUG_ACL, "idattr_is_member_cb sr_type[%d] sr_err[%d]\n", rs->sr_type, rs->sr_err, 0);
	idattr_ismember_t *ismember = (idattr_ismember_t *)op->o_callback->sc_private;
	
	if (rs->sr_err == LDAP_COMPARE_TRUE)
		ismember->found = 1;
	else
		ismember->found = 0;
	
	return 0;
}


int __attribute__ ((noinline))
idattr_is_member (
	Operation	*op,
	struct berval* groupDN,
	AttributeDescription *searchAttr,
	struct berval* searchID,
	u_int32_t * result)
{
	Operation nop = *op;
	AttributeAssertion	ava = { NULL, BER_BVNULL };
	SlapReply 		sreply = {REP_RESULT};
	slap_callback cb = { NULL, idattr_is_member_cb, NULL, NULL };
	BackendDB	*target_bd = NULL;
	idattr_ismember_t ismember =  {0};
	int rc = 0;
	
	target_bd = select_backend(&op->o_req_ndn, 0);
	
	if (!target_bd || !target_bd->be_compare)
		return LDAP_NOT_SUPPORTED;
	
	sreply.sr_entry = NULL;
	sreply.sr_nentries = 0;
	
	nop.orc_ava = &ava;
	nop.orc_ava->aa_desc = searchAttr;
	nop.orc_ava->aa_value = *searchID;
	
	nop.o_tag = LDAP_REQ_COMPARE;
	nop.o_protocol = LDAP_VERSION3;
	nop.o_callback = &cb;
	nop.o_time = slap_get_time();
	nop.o_do_not_cache = 0;
	nop.o_dn = target_bd->be_rootdn;
	nop.o_ndn = target_bd->be_rootndn;
	nop.o_bd = target_bd;

	nop.o_req_dn = *groupDN;
	nop.o_req_ndn = *groupDN;
	cb.sc_private = &ismember;

	rc = nop.o_bd->be_compare( &nop, &sreply );
	Debug(LDAP_DEBUG_ACL, "idattr_is_member be_compare[%d] ismember[%d]\n", rc, ismember.found, 0);
	if (ismember.found)
		*result = 1;
	else
		*result = 0;
	return 0;
}

static int
idattr_id_to_dn_cb (
	Operation	*op,
	SlapReply	*rs
)
{
	Debug(LDAP_DEBUG_ACL, "idattr_id_to_dn_cb sr_type[%d]\n", rs->sr_type, 0, 0);
//dump_slap_entry(rs->sr_entry);
// && rs->sr_un.sru_search.r_entry->e_nname
	idattr_id_to_dn_t *object_dn = (idattr_id_to_dn_t *)op->o_callback->sc_private;
	
	if ( rs->sr_type == REP_SEARCH ) {
		if (rs->sr_un.sru_search.r_entry) {
			Debug( LDAP_DEBUG_ACL, "idattr_id_to_dn_cb len[%d] [%s]\n", rs->sr_entry->e_nname.bv_len, rs->sr_entry->e_nname.bv_val, 0);	
			ber_dupbv_x( &object_dn->target_dn, &(rs->sr_entry->e_nname), op->o_tmpmemctx);
			object_dn->found = 1;
		}
	} else {
        if(rs->sr_type != REP_INTERMEDIATE)
            assert( rs->sr_type == REP_RESULT );
	}

	return 0;
}

static int __attribute__ ((noinline))
idattr_id_to_dn (
	Operation	*op,
	AttributeDescription *searchAttr,
	struct berval* searchID,
	struct berval* resultDN)
{
	Operation nop = *op;
	char			filter_str[128];
	AttributeAssertion	ava = { NULL, BER_BVNULL };
	Filter			filter = {LDAP_FILTER_EQUALITY};
	SlapReply 		sreply = {REP_RESULT};
	slap_callback cb = { NULL, idattr_id_to_dn_cb, NULL, NULL };
	BackendDB	*target_bd = NULL;
	idattr_id_to_dn_t dn_result =  {0};
	int rc = 0;
	
	target_bd = select_backend(&op->o_req_ndn, 0);
	
	if (!target_bd || !target_bd->be_search)
		return LDAP_NOT_SUPPORTED;
	
	sreply.sr_entry = NULL;
	sreply.sr_nentries = 0;
	nop.ors_filterstr.bv_len = snprintf(filter_str, sizeof(filter_str),
		"(%s=%s)",searchAttr->ad_cname.bv_val, searchID->bv_val);
	filter.f_ava = &ava;
	filter.f_av_desc = searchAttr;
	filter.f_av_value = *searchID;

	nop.o_tag = LDAP_REQ_SEARCH;
	nop.o_protocol = LDAP_VERSION3;
	nop.o_callback = &cb;
	nop.o_time = slap_get_time();
	nop.o_do_not_cache = 0;
	nop.o_dn = target_bd->be_rootdn;
	nop.o_ndn = target_bd->be_rootndn;
	nop.o_bd = target_bd;

	nop.o_req_dn = target_bd->be_suffix[0];
	nop.o_req_ndn = target_bd->be_nsuffix[0];
	nop.ors_scope = LDAP_SCOPE_SUBTREE;
	nop.ors_deref = LDAP_DEREF_NEVER;
	nop.ors_slimit = SLAP_NO_LIMIT;
	nop.ors_tlimit = SLAP_NO_LIMIT;
	nop.ors_filter = &filter;
	nop.ors_filterstr.bv_val = filter_str;
	nop.ors_filterstr.bv_len = strlen(filter_str);
	nop.ors_attrs = NULL;
	nop.ors_attrsonly = 0;

	cb.sc_private = &dn_result;

	rc = nop.o_bd->be_search( &nop, &sreply );
	if (dn_result.found) {
		ber_dupbv(resultDN, &dn_result.target_dn );
	}
		
	return 0;
}

static int __attribute__ ((noinline))
idattr_check_isowner (
	Operation	*op,
	Entry *user,
	Entry   *target,
	AttributeDescription *searchAttr,
	idattr_t *id)
{
	int rc = LDAP_INSUFFICIENT_ACCESS;
	int entry_rc = LDAP_INSUFFICIENT_ACCESS;
	Entry		*targetEntry = NULL;
	Entry		*requestEntry = NULL;
	Attribute	*ownerIDAttr = NULL;
	
	if (op->o_conn->c_authz.c_sai_krb5_auth_data_provisioned) {
		entry_rc = be_entry_get_rw( op, &target->e_nname, NULL, idattr_owneruuid, 0, &targetEntry );
		
		// check target for owner
		if (entry_rc == LDAP_SUCCESS && targetEntry != NULL) {
			Attribute *theOwnerIDAttr = NULL;

			theOwnerIDAttr = attr_find( targetEntry->e_attrs, idattr_owneruuid);

			if (theOwnerIDAttr && ber_bvcmp(&op->o_conn->c_authz.c_sai_krb5_pac_id , &theOwnerIDAttr->a_nvals[0]) == 0) {
				rc = LDAP_SUCCESS;
				Debug(LDAP_DEBUG_ACL, "idattr_check_isowner by request[%s] - PROXYUSER BY TARGET\n", op->o_req_ndn.bv_val, 0, 0);
			} else {
				rc =  LDAP_INSUFFICIENT_ACCESS;			
			}
				
		}
		// check entry for owner
		if ((rc != LDAP_SUCCESS) && !BER_BVISNULL(&op->o_req_ndn)) {
			entry_rc = be_entry_get_rw( op, &op->o_req_ndn, NULL, idattr_owneruuid, 0, &requestEntry );
			if (entry_rc == LDAP_SUCCESS && requestEntry != NULL) {
				Attribute *theOwnerIDAttr = NULL;
				theOwnerIDAttr = attr_find( requestEntry->e_attrs, idattr_owneruuid);
				
				if (theOwnerIDAttr == NULL) {
					rc =  LDAP_INSUFFICIENT_ACCESS;
					goto exitonerror;
				}
	
				if (ber_bvcmp(&op->o_conn->c_authz.c_sai_krb5_pac_id , &theOwnerIDAttr->a_nvals[0]) == 0) {
					rc = LDAP_SUCCESS;
					Debug(LDAP_DEBUG_ACL, "idattr_check_isowner by request[%s] - PROXYUSER BY ENTRY\n", op->o_req_ndn.bv_val, 0, 0);
				}
					
			}
		}
		
	} else {
		if (user == NULL) {
			rc =  LDAP_INSUFFICIENT_ACCESS;
			goto exitonerror;
		}
		
		ownerIDAttr = attr_find( user->e_attrs, searchAttr);
		
		if (ownerIDAttr == NULL) {
			rc =  LDAP_INSUFFICIENT_ACCESS;
			goto exitonerror;
		}
	
		entry_rc = be_entry_get_rw( op, &target->e_nname, NULL, idattr_owneruuid, 0, &targetEntry );
		
		if (entry_rc == LDAP_SUCCESS && targetEntry != NULL) {
			Attribute *theAttr = NULL;
			
			theAttr = attr_find( targetEntry->e_attrs, idattr_owneruuid);
			if (theAttr) {
				entry_rc = value_find_ex( idattr_owneruuid,
					SLAP_MR_ATTRIBUTE_VALUE_NORMALIZED_MATCH |
					SLAP_MR_ASSERTED_VALUE_NORMALIZED_MATCH,
					theAttr->a_nvals, ownerIDAttr->a_nvals,
					op->o_tmpmemctx );
				if (entry_rc == LDAP_SUCCESS) {
					rc = LDAP_SUCCESS;
					Debug(LDAP_DEBUG_ACL, "idattr_check_isowner by target[%s]\n", target->e_nname.bv_val, 0, 0);
				}

				// check group membership if applicable
				if (rc != LDAP_SUCCESS) {
					u_int32_t isMember = 0;
					if (BER_BVISNULL(&id->idattr_owner_dn)) {
						idattr_id_to_dn (op,searchAttr, theAttr->a_nvals, &id->idattr_owner_dn);
					}
					if (!BER_BVISNULL(&id->idattr_owner_dn) && strstr(id->idattr_owner_dn.bv_val, group_subtree) != NULL) { 
						Debug( LDAP_DEBUG_ACL, "idattr_check_isowner: len[%d] idattr_owner_dn[%s] \n", id->idattr_owner_dn.bv_len, id->idattr_owner_dn.bv_val, 0 );
						idattr_is_member ( op, &id->idattr_owner_dn, idattr_memberships, ownerIDAttr->a_nvals , &isMember); 
						if (isMember) {
							Debug( LDAP_DEBUG_ACL, "idattr_check_isowner: user is member of Group (from target)\n", 0, 0, 0 );
							rc = LDAP_SUCCESS;
						}									
					}
				}
			}
		}
		// check op->o_req_ndn
		// target: cn=record,dc=com
		// request_dn: cn=myrecord,cn=record,dc=com
		// access to the parent (target) is required prior to requesting right access to the child (request_dn)
		// lookahead at the child to check for ownership
		if ((rc != LDAP_SUCCESS) && !BER_BVISNULL(&op->o_req_ndn)) {
			entry_rc = be_entry_get_rw( op, &op->o_req_ndn, NULL, idattr_owneruuid, 0, &requestEntry );
			
			if (entry_rc == LDAP_SUCCESS && requestEntry != NULL) {
				Attribute *theAttr = NULL;
				
				theAttr = attr_find( requestEntry->e_attrs, idattr_owneruuid);
				if (theAttr) {
					entry_rc = value_find_ex( idattr_owneruuid,
						SLAP_MR_ATTRIBUTE_VALUE_NORMALIZED_MATCH |
						SLAP_MR_ASSERTED_VALUE_NORMALIZED_MATCH,
						theAttr->a_nvals, ownerIDAttr->a_nvals,
						op->o_tmpmemctx );
					if (entry_rc == LDAP_SUCCESS) {
						rc = LDAP_SUCCESS;
						Debug(LDAP_DEBUG_ACL, "idattr_check_isowner by request[%s]\n", op->o_req_ndn.bv_val, 0, 0);
					}

					// check group membership if applicable
					u_int32_t isMember = 0;
					if (BER_BVISNULL(&id->idattr_owner_dn)) {
						idattr_id_to_dn (op,searchAttr, theAttr->a_nvals, &id->idattr_owner_dn);
					}
					if (!BER_BVISNULL(&id->idattr_owner_dn) && strstr(id->idattr_owner_dn.bv_val, group_subtree) != NULL) { 
						Debug( LDAP_DEBUG_ACL, "idattr_check_isowner: len[%d] id->idattr_owner_dn[%s] \n", id->idattr_owner_dn.bv_len, id->idattr_owner_dn.bv_val, 0 );
						idattr_is_member ( op, &id->idattr_owner_dn, idattr_memberships, ownerIDAttr->a_nvals , &isMember); 
						if (isMember) {
							Debug( LDAP_DEBUG_ACL, "idattr_check_isowner: user is member of Group (by parent)\n", 0, 0, 0 );
							rc = LDAP_SUCCESS;
						}									
					}
				}
			}
		}
	}
exitonerror:
	if (targetEntry)
		be_entry_release_r(op, targetEntry);
	if (requestEntry)
		be_entry_release_r(op, requestEntry);
	return rc;
}

static int __attribute__ ((noinline))
idattr_check_selfwrite (
	Operation	*op,
	Entry *target,
	Entry *user,
	struct berval* val,
	idattr_t *id)
{
	int rc = LDAP_INSUFFICIENT_ACCESS;
	int entry_rc = LDAP_INSUFFICIENT_ACCESS;
	int bool_rc = LDAP_INSUFFICIENT_ACCESS;
	Entry		*targetEntry = NULL;
	Attribute	*theAttr = NULL;
	int match  = 0;
	const char *dummy = NULL;
	struct berval bool_true_val;
	
	if ( val == NULL || user == NULL ) {
		rc =  LDAP_INSUFFICIENT_ACCESS;
		goto exitonerror;
	}
	
	if (op->o_conn->c_authz.c_sai_krb5_auth_data_provisioned) {
		rc =  LDAP_INSUFFICIENT_ACCESS;
		goto exitonerror;
	}
	
	if (id->idattr_boolattrDesc) {
		entry_rc = be_entry_get_rw( op, &target->e_nname, NULL, id->idattr_boolattrDesc, 0, &targetEntry );
		if (entry_rc == LDAP_SUCCESS && targetEntry != NULL) {
			theAttr = attr_find( targetEntry->e_attrs, id->idattr_boolattrDesc);
			if (theAttr != NULL) {
				ber_str2bv( "1", 0, 0, &bool_true_val );
				bool_rc = value_match( &match, id->idattr_boolattrDesc,
						id->idattr_boolattrDesc->ad_type->sat_equality, 0,
						&bool_true_val, &theAttr->a_nvals[ 0 ], &dummy );			
				if ( bool_rc != LDAP_SUCCESS || match != 0 ) {
					rc =  LDAP_INSUFFICIENT_ACCESS;
					goto exitonerror;
				}
			} else {
				rc =  LDAP_INSUFFICIENT_ACCESS;
				goto exitonerror;
			}
		} else {
			rc =  LDAP_INSUFFICIENT_ACCESS;
			goto exitonerror;
		}
	}
	
	theAttr = attr_find( user->e_attrs, id->idattr_selfattrDesc);  // user unique ID
	if ( !theAttr || !BER_BVISNULL( &theAttr->a_nvals[ 1 ] ) ) {
		rc = LDAP_NO_SUCH_ATTRIBUTE;
	} else {
		
		rc = value_match( &match, id->idattr_selfattrDesc,
				id->idattr_selfattrDesc->ad_type->sat_equality, 0,
				val, &theAttr->a_nvals[ 0 ], &dummy );
		if ( rc != LDAP_SUCCESS || match != 0 )
			rc = LDAP_INSUFFICIENT_ACCESS;
	}

	Debug(LDAP_DEBUG_ACL, "[%d]idattr_check_selfwrite match[%d]\n", rc, match, 0);
exitonerror:
	if (targetEntry)
		be_entry_release_r(op, targetEntry);	
	return rc;
}

static int __attribute__ ((noinline))
idattr_check_isuser (
	Operation	*op,
	Entry *user,
	AttributeDescription *searchAttr,
	idattr_t		*id)
{
	int rc = LDAP_INSUFFICIENT_ACCESS;
	Attribute	*theAttr = NULL;
	
	if (id->idattr_type == SELFWRITE_TYPE || id->idattr_type == OWNER_TYPE || id->idattr_type == USERS_TYPE) {
		if ((op->o_conn->c_authz.c_sai_krb5_auth_data_provisioned && (id->idattr_type == OWNER_TYPE || id->idattr_type == USERS_TYPE))
			|| (user != NULL)) {		
			rc = LDAP_SUCCESS;
		} else {
			rc = LDAP_INSUFFICIENT_ACCESS;
		}
	} else if (op->o_conn->c_authz.c_sai_krb5_auth_data_provisioned) {
		if (ber_bvcmp(&op->o_conn->c_authz.c_sai_krb5_pac_id , &id->idattr_pat) == 0) {
			rc = LDAP_SUCCESS;
		} else {
			rc = LDAP_INSUFFICIENT_ACCESS;
		}
	} else {
		theAttr = attr_find( user->e_attrs, searchAttr);
		if ( !theAttr || !BER_BVISNULL( &theAttr->a_nvals[ 1 ] ) ) {
			rc = LDAP_NO_SUCH_ATTRIBUTE;
		} else {
			// check user match
			rc = value_find_ex( searchAttr,
				SLAP_MR_ATTRIBUTE_VALUE_NORMALIZED_MATCH |
				SLAP_MR_ASSERTED_VALUE_NORMALIZED_MATCH,
				theAttr->a_nvals, &(id->idattr_pat),
				op->o_tmpmemctx );
			Debug( LDAP_DEBUG_ACL, "#####idattr_check_isuser <Find By ID> value_find_ex [%d]#####\n", rc, 0, 0);	
			
#ifdef PAC_PROXY_USERS
			if (rc != LDAP_SUCCESS) {
				theAttr = attr_find( user->e_attrs, idattr_memberships);
				if ( !theAttr || !BER_BVISNULL( &theAttr->a_nvals[ 1 ] ) ) {
					rc = LDAP_NO_SUCH_ATTRIBUTE;
				} else {
					rc = value_find_ex( idattr_memberships,
						SLAP_MR_ATTRIBUTE_VALUE_NORMALIZED_MATCH |
						SLAP_MR_ASSERTED_VALUE_NORMALIZED_MATCH,
						theAttr->a_nvals, &(id->idattr_pat),
						op->o_tmpmemctx );				
					Debug( LDAP_DEBUG_ACL, "#####idattr_check_isuser <Find By GROUP ID> value_find_ex [%d]#####\n", rc, 0, 0);		
				}
			}
#endif
		}
	}
	
	return rc;
}

static int __attribute__ ((noinline))
idattr_check_applyto (
	Operation	*op,
	Entry *target,
	AttributeDescription *searchAttr,
	idattr_t		*id)
{
	int rc = LDAP_INSUFFICIENT_ACCESS;
	int entry_rc = LDAP_NO_SUCH_OBJECT;
	Entry *theEntry = NULL;
	
// is target a member of APPLYTO group
	// retrieve and cache dn of APPLYTO group
	if (BER_BVISNULL(&id->idattr_applyto_dn)) {
		idattr_id_to_dn (op,searchAttr, &(id->idattr_applyto_uuid), &(id->idattr_applyto_dn));
		Debug( LDAP_DEBUG_ACL, "idattr_dynacl_mask: len[%d] idattr_applyto_dn[%s] \n", id->idattr_applyto_dn.bv_len, id->idattr_applyto_dn.bv_val, 0 );
	}

	if (!BER_BVISNULL(&id->idattr_applyto_dn)) {
		entry_rc = be_entry_get_rw( op, &target->e_nname, NULL , idattr_uuid, 0, &theEntry );
		
		if (entry_rc == LDAP_SUCCESS && theEntry) {
			Attribute *uuidAttr = NULL;
			u_int32_t isMember = 0;
			
			uuidAttr = attr_find( theEntry->e_attrs, idattr_uuid);
			if (uuidAttr) {
				entry_rc = value_find_ex( idattr_uuid,
					SLAP_MR_ATTRIBUTE_VALUE_NORMALIZED_MATCH |
					SLAP_MR_ASSERTED_VALUE_NORMALIZED_MATCH,
					uuidAttr->a_nvals, &(id->idattr_applyto_uuid),
					op->o_tmpmemctx );
				if (entry_rc == LDAP_SUCCESS) { // TARGET == APPLYTO_TARGET
					rc = LDAP_SUCCESS;				
				} else { // TARGET == MEMBEROF(APPLYTO)
					idattr_is_member ( op, &(id->idattr_applyto_dn), idattr_memberships, uuidAttr->a_nvals , &isMember); // flat list
					if (isMember) {
						rc = LDAP_SUCCESS;
					}									
				}
			}
		}
	}

exitonerror:
	if (theEntry)
		be_entry_release_r(op, theEntry);	
	return rc;
}


static int idattr_dynacl_unparse_style(idattr_t *id,  char **ace)
{
	if (id == NULL || ace == NULL || *ace == NULL)
		 return -1;

	switch ( id->idattr_style ) {
	case ACL_STYLE_BASE:
		*ace = lutil_strcopy( *ace, ".exact=" );
		break;

	case ACL_STYLE_EXPAND:
		*ace = lutil_strcopy( *ace, ".expand=" );
		break;
	default:
		break;
	}

	return 0;
}

static int idattr_dynacl_unparse_boolattr(idattr_t *id,  char **ace)
{
	if (id == NULL || ace == NULL || *ace == NULL)
		 return -1;

	if (!BER_BVISNULL(&id->idattr_boolattr)) {
		*ace = lutil_strcopy( *ace, "BOOLATTR:" );	
		*ace = lutil_strcopy( *ace, id->idattr_boolattr.bv_val );	
		*ace = lutil_strcopy( *ace, ";" );	
	}

	return 0;
}

static int idattr_dynacl_unparse_selfattr(idattr_t *id,  char **ace)
{
	if (id == NULL || ace == NULL || *ace == NULL)
		 return -1;

	if (!BER_BVISNULL(&id->idattr_selfattr)) {
		*ace = lutil_strcopy( *ace, "SELFATTR:" );	
		*ace = lutil_strcopy( *ace, id->idattr_selfattr.bv_val );	
		*ace = lutil_strcopy( *ace, ";" );	
	}

	return 0;
}

static int idattr_dynacl_unparse_applyto(idattr_t *id,  char **ace)
{
	if (id == NULL || ace == NULL || *ace == NULL)
		 return -1;

	if (!BER_BVISNULL(&id->idattr_applyto_uuid)) {
		*ace = lutil_strcopy( *ace, "APPLYTO:" );	
		*ace = lutil_strcopy( *ace, id->idattr_applyto_uuid.bv_val );	
		*ace = lutil_strcopy( *ace, ";" );	
	}

	return 0;
}

static int idattr_dynacl_unparse_operation(idattr_t *id,  char **ace)
{
	
	if (id == NULL || ace == NULL || *ace == NULL)
		 return -1;
	
	if (id->idattr_ops != 0) {
		*ace = lutil_strcopy( *ace, "OP:" );	
	}
	
	switch ( id->idattr_ops ) {
	case LDAP_REQ_BIND:
		*ace = lutil_strcopy( *ace, "BIND" );
		break;
	case LDAP_REQ_UNBIND:
		*ace = lutil_strcopy( *ace, "UNBIND" );
		break;
	case LDAP_REQ_SEARCH:
		*ace = lutil_strcopy( *ace, "SEARCH" );
		break;
	case LDAP_REQ_MODIFY:
		*ace = lutil_strcopy( *ace, "BIND" );
		break;
	case LDAP_REQ_ADD:
		*ace = lutil_strcopy( *ace, "ADD" );
		break;
	case LDAP_REQ_DELETE:
		*ace = lutil_strcopy( *ace, "DELETE" );
		break;
	case LDAP_REQ_MODDN:
		*ace = lutil_strcopy( *ace, "RENAME" );
		break;
	case LDAP_REQ_COMPARE:
		*ace = lutil_strcopy( *ace, "COMPARE" );
		break;
	case LDAP_REQ_ABANDON:
		*ace = lutil_strcopy( *ace, "ABANDON" );
		break;
	case LDAP_REQ_EXTENDED:
		*ace = lutil_strcopy( *ace, "EXTENDED" );
		break;

	default:
		break;
	}

	return 0;
}
static int idattr_dynacl_unparse_optionsavailable(idattr_t *id,  char **ace)
{
	if (id == NULL || ace == NULL || *ace == NULL)
		 return -1;
	if ((id->idattr_ops != 0) ||
		!BER_BVISNULL(&id->idattr_applyto_uuid) ||
		!BER_BVISNULL(&id->idattr_selfattr)     ||
		!BER_BVISNULL(&id->idattr_boolattr)) {
		*ace = lutil_strcopy( *ace, "/" );			
	}
	
	return 0;
}

static int idattr_dynacl_unparse(
	void		*priv,
	struct berval	*bv )
{
	idattr_t		*id = (idattr_t *)priv;
	char		*ptr = NULL, *start = NULL;
	size_t len = 0;
	bv->bv_len = MAX_ACE_LEN;
	bv->bv_val = ch_calloc( 1, bv->bv_len + 1 );

	start = ptr = lutil_strcopy( bv->bv_val, " dynacl/idattr" );

	if (idattr_dynacl_unparse_optionsavailable(id, &ptr) != 0)
		goto exit_on_error;
		
	if (idattr_dynacl_unparse_operation(id, &ptr) != 0)
		goto exit_on_error;
	if (idattr_dynacl_unparse_applyto(id, &ptr) != 0)
		goto exit_on_error;
	if (idattr_dynacl_unparse_selfattr(id, &ptr) != 0)
		goto exit_on_error;
	if (idattr_dynacl_unparse_boolattr(id, &ptr) != 0)
		goto exit_on_error;
	
	// remove trailing option terminator
	len = strlen(start);
	if (len && start[len - 1] == ';') {
		start[len - 1] = '\0';
		ptr = start + len - 1;
	}
	
	if (idattr_dynacl_unparse_style(id, &ptr) != 0)
		goto exit_on_error;

	ptr = lutil_strncopy( ptr, id->idattr_pat.bv_val, id->idattr_pat.bv_len );
	ptr[ 0 ] = '\0';

	bv->bv_len = ptr - bv->bv_val;
	
exit_on_error:
	return 0;
}

static int
idattr_dynacl_mask(
	void			*priv,
	Operation		*op,
	Entry			*target,
	AttributeDescription	*desc,
	struct berval		*val,
	int			nmatch,
	regmatch_t		*matches,
	slap_access_t		*grant,
	slap_access_t		*deny )
{
	idattr_t		*id = (idattr_t *)priv;
	Entry   *user = NULL;
	int		rc = LDAP_INSUFFICIENT_ACCESS;
	int		user_rc = LDAP_INSUFFICIENT_ACCESS;
	int		target_rc = LDAP_INSUFFICIENT_ACCESS;
	Backend		*be = op->o_bd,
			*group_be = NULL,
			*user_be = NULL;
	AttributeDescription *searchAttr = NULL;
	ObjectClass *searchObject = NULL;
	struct berval *searchTarget = NULL;
	Entry *searchEntry = NULL;
	
	ACL_INVALIDATE( *deny );

	Debug( LDAP_DEBUG_ACL, "idattr_dynacl_mask: pat(%s) type(%d) op(%d)\n", id->idattr_pat.bv_val, id->idattr_type, id->idattr_ops );

	Debug( LDAP_DEBUG_ACL, "idattr_dynacl_mask: target(%s) op->o_ndn(%s)\n", target->e_nname.bv_val, op->o_ndn.bv_val, 0 );
	
	if (id->idattr_ops && (id->idattr_ops != op->o_tag)) {
		Debug( LDAP_DEBUG_ACL, "idattr_dynacl_mask:  Operation Not Allowed - Ops allowed (%d), Requested Op (%d)\n", id->idattr_ops, op->o_tag, 0 );
		// LDAP_INSUFFICIENT_ACCESS
		return 0;
	}

	if (!op->o_conn->c_authz.c_sai_krb5_auth_data_provisioned) {
		user_be = op->o_bd = select_backend( &op->o_ndn, 0);
		if ( op->o_bd == NULL ) {
			Debug( LDAP_DEBUG_ACL, "idattr_dynacl_mask: op->o_bd == NULL \n", 0, 0, 0 );
			op->o_bd = be;
			return 0;
		}
	}
	// check ACL TYPE
	Debug( LDAP_DEBUG_ACL, "idattr_dynacl_mask: idattr_type(%d)\n", id->idattr_type, 0, 0 );
	if (id->idattr_type == UUID_TYPE || id->idattr_type == OWNER_TYPE) {
		searchAttr = idattr_uuid;
		searchObject = idattr_extensible_object;
		searchTarget = &op->o_ndn; 
	} else if (id->idattr_type == SID_TYPE) {
		searchAttr = idattr_sid;
		searchObject = idattr_extensible_object;
		searchTarget = &op->o_conn->c_authz.sai_ndn;
	} else if (id->idattr_type == SELFWRITE_TYPE) {
		searchAttr = id->idattr_selfattrDesc;
		searchObject = idattr_extensible_object;
		searchTarget = &op->o_ndn; 
	} else if (id->idattr_type == USERS_TYPE) {
		if (BER_BVISEMPTY( &op->o_ndn )) {
			// LDAP_INSUFFICIENT_ACCESS
			op->o_bd = be;
			return 0;
		} else {
			// check for APPLYTO clause
			if (id->idattr_applyto) {
				rc = idattr_check_applyto(op, target, searchAttr, id);			
			} else {
				rc =  LDAP_SUCCESS;
			}
			goto assignaccess;
		}
	} else {
		Debug( LDAP_DEBUG_ACL, "idattr_dynacl_mask: unknown idattr_type(%s)\n", id->idattr_type, 0, 0 );
		// LDAP_INSUFFICIENT_ACCESS
		return 0;
	}
	if (op->o_conn->c_authz.c_sai_krb5_auth_data_provisioned) {
	 	user_rc = LDAP_SUCCESS;
	} else if (searchAttr) {
		// lookup user record	
		user_rc = be_entry_get_rw( op, searchTarget, NULL, searchAttr, 0, &user );

		Debug( LDAP_DEBUG_ACL, "idattr_dynacl_mask: be_entry_get_rw user_rc(%d) user(%x) \n", user_rc, user, 0 );
		
		if ( user_rc != LDAP_SUCCESS || user == NULL ) {
			op->o_bd = be; 
			return 0;
		}
	}

	if ( user_rc == LDAP_SUCCESS || user != NULL ) {
			user_rc = idattr_check_isuser(op, user, searchAttr, id);
			
			if (user_rc == LDAP_SUCCESS) { 
				switch ( id->idattr_type ) {
					case SELFWRITE_TYPE:
						user_rc = idattr_check_selfwrite(op, target, user, val, id);
						break;
					case OWNER_TYPE:	
						user_rc = idattr_check_isowner(op, user, target, searchAttr, id);
						break;			
					case UUID_TYPE:
					case SID_TYPE:
						user_rc = LDAP_SUCCESS;
						break;
					default:
						break;
				}
			
				if (user_rc == LDAP_SUCCESS) {
					if (id->idattr_applyto) {
						rc = idattr_check_applyto(op, target, searchAttr, id);			
					} else {
						rc = LDAP_SUCCESS;
					}
				}
			}				
	} else {
		rc = LDAP_NO_SUCH_OBJECT;
	}

assignaccess:
	if ( rc == LDAP_SUCCESS ) {
		ACL_LVL_ASSIGN_WRITE( *grant );
	}

cleanup:

	if ( user != NULL) {
		op->o_bd = user_be;
		be_entry_release_r( op, user );
		op->o_bd = be;
	}
	op->o_bd = be;

	return 0;
}

static int
idattr_dynacl_destroy(
	void		*priv )
{
	idattr_t		*id = (idattr_t *)priv;

	if ( id != NULL ) {
		if ( !BER_BVISNULL( &id->idattr_pat ) ) {
			ber_memfree( id->idattr_pat.bv_val );
			id->idattr_pat.bv_len = 0;
			id->idattr_pat.bv_val = NULL;
		}
		if ( !BER_BVISNULL( &id->idattr_applyto_dn ) ) {
			ber_memfree( id->idattr_applyto_dn.bv_val );
			id->idattr_applyto_dn.bv_len = 0;
			id->idattr_applyto_dn.bv_val = NULL;
		}
		if ( !BER_BVISNULL( &id->idattr_owner_dn ) ) {
			ber_memfree( id->idattr_owner_dn.bv_val );
			id->idattr_owner_dn.bv_len = 0;
			id->idattr_owner_dn.bv_val = NULL;
		}
		if ( !BER_BVISNULL( &id->idattr_applyto_uuid ) ) {
			ber_memfree( id->idattr_applyto_uuid.bv_val );
			id->idattr_applyto_uuid.bv_len = 0;
			id->idattr_applyto_uuid.bv_val = NULL;
		}
		if ( !BER_BVISNULL( &id->idattr_selfattr ) ) {
			ber_memfree( id->idattr_selfattr.bv_val );
			id->idattr_selfattr.bv_len = 0;
			id->idattr_selfattr.bv_val = NULL;
		}
		if ( !BER_BVISNULL( &id->idattr_boolattr ) ) {
			ber_memfree( id->idattr_boolattr.bv_val );
			id->idattr_boolattr.bv_len = 0;
			id->idattr_boolattr.bv_val = NULL;
		}
		ch_free( id );
	}

	return 0;
}

static struct slap_dynacl_t idattr_dynacl = {
	"idattr",
	idattr_dynacl_parse,
	idattr_dynacl_unparse,
	idattr_dynacl_mask,
	idattr_dynacl_destroy
};

int
dynacl_idattr_init( void )
{
	int	rc;

	rc = slap_dynacl_register( &idattr_dynacl );
	
	return rc;
}

#if SLAPD_DYNACL_idattr == SLAPD_MOD_DYNAMIC && defined(PIC)
int
init_module( int argc, char *argv[] )
{
	return slap_dynacl_register( &idattr_dynacl );
}
#endif

#endif /* SLAP_DYNACL */
