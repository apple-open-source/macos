/*
 *  odlocales.c
 */



#include "portable.h"
#ifdef SLAPD_OVER_ODLOCALES
#include "overlayutils.h"

#include <arpa/inet.h>
#define ODLOCALES_BACK_CONFIG 1

#include <ac/string.h>
#include <ac/ctype.h>
#include "slap.h"
#include "ldif.h"
#include "config.h"

static const char* defaultLocaleName = "DefaultLocale";


static slap_overinst odlocales;

typedef struct odlocales_info {
	struct odlocales_info *ci_next;
	struct berval ci_dn;
	AttributeDescription *ci_ad;
} odlocales_info;

typedef struct odloc_res_s {
	struct berval *ndn;
	char *lname;
	int count;
	
} odloc_res;

static int odloc_count_attr_cb(
							   Operation *op,
							   SlapReply *rs
							   )
{
	odloc_res *uc;
	int rc = 0;
	/* because you never know */
	if(!op || !rs) return(0);
	
	/* Only search entries are interesting */
	if(rs->sr_type != REP_SEARCH) return(0);
	
	uc = op->o_callback->sc_private;
	dump_slap_attr(rs->sr_un.sru_search.r_entry->e_attrs);
	Attribute *someVal = NULL;
	AttributeDescription	*ad = NULL;
	const char	*text = NULL;
	rc = slap_str2ad( "apple-group-realname", &ad, &text );
	someVal = attr_find(rs->sr_un.sru_search.r_entry->e_attrs, ad);
    if (someVal != NULL) {
        uc->lname = someVal->a_vals->bv_val;
        uc->count++;
    }
	
	return(0);
}

static int odloc_cloud_attr_cb(
							   Operation *op,
							   SlapReply *rs
							   )
{
	odloc_res *uc;
	int rc = 0;
	/* because you never know */
	if(!op || !rs) return(0);
	
	/* Only search entries are interesting */
	if(rs->sr_type != REP_SEARCH) return(0);
	
	uc = op->o_callback->sc_private;
	dump_slap_attr(rs->sr_un.sru_search.r_entry->e_attrs);
	Attribute *someVal = NULL;
	AttributeDescription	*ad = NULL;
	const char	*text = NULL;
	rc = slap_str2ad( "apple-dns-domain", &ad, &text );
	someVal = attr_find(rs->sr_un.sru_search.r_entry->e_attrs, ad);
    if (someVal != NULL) {
        uc->lname = someVal->a_vals->bv_val;
        uc->count++;
    }
	
	return(0);
}


static char* odlocale_record_search(
                                    Operation *op,
                                    char *key,
                                    struct berval *searchbase
                                    )
{
	slap_overinst *on = (slap_overinst *) op->o_bd->bd_info;
	SlapReply nrs = { REP_RESULT };
	slap_callback cb = { NULL, NULL, NULL, NULL }; /* XXX */
	odloc_res uq = { NULL, 0 };
	int rc;
    Operation *nop = NULL;
    OperationBuffer opbuf = {0};
	
    memset(&opbuf, 0, sizeof(opbuf));
	nop = (Operation*)&opbuf;
	nop->o_hdr = &opbuf.ob_hdr;
	nop->o_controls = opbuf.ob_controls;
	operation_fake_init(op->o_conn, (Operation*)&opbuf, ldap_pvt_thread_pool_context(), 0);
	nop = &opbuf.ob_op;
	nop->o_dn = nop->o_ndn = op->o_bd->be_rootndn;
    
    nop->ors_filter = str2filter(key);
	ber_str2bv(key, 0, 0, &nop->ors_filterstr);
	if (strstr(key, "(cn=locales)")){
		cb.sc_response	= (slap_response*)odloc_cloud_attr_cb;
	}else{
		cb.sc_response	= (slap_response*)odloc_count_attr_cb;
	}	
	cb.sc_private	= &uq;
    
	nop->o_callback	= &cb;
	nop->o_tag	= LDAP_REQ_SEARCH;
	nop->ors_scope	= LDAP_SCOPE_SUBTREE;
	nop->ors_deref	= LDAP_DEREF_NEVER;
	nop->ors_limit	= NULL;
	nop->ors_slimit	= SLAP_NO_LIMIT;
	nop->ors_tlimit	= SLAP_NO_LIMIT;
	nop->ors_attrs	= slap_anlist_all_attributes;
	nop->ors_attrsonly = 0;
	
	uq.ndn = &op->o_req_ndn;
	
	nop->o_req_dn	= *searchbase;
	nop->o_req_ndn	= *searchbase;
	
	nop->o_bd = on->on_info->oi_origdb;
	rc = nop->o_bd->be_search(nop, &nrs);
	if(nop->ors_filter)
        filter_free( nop->ors_filter );
    
	if ((rc == LDAP_SUCCESS) && (uq.count !=0)){
		return(uq.lname);
	}else{
		return NULL;
	}
	
	
}




static char* get_locales(Operation *op,
                         char *myip,
                         struct berval *searchbase){
    
	char *mySite = NULL;
	in_addr_t   y;
    int         c;
	char        ipOut[16];
	char        NetObj[100];
	char *localeName = NULL;
    
	in_addr_t x;
	inet_aton(myip, &x);
    
	for ( c=32; c > 0 && localeName == NULL; c--){
        
		const char      *attrs[2];
		y = (ntohl(x) >> (32-c)) << (32-c);
        
		unsigned int        tempAddr = htonl( y );
		unsigned char *X = (unsigned char *)&tempAddr;
        
		snprintf( ipOut, sizeof(ipOut), "%i.%i.%i.%i", (int)X[0], (int)X[1], (int)X[2], (int)X[3]);
		snprintf( NetObj, sizeof(NetObj), "(apple-locale-subnets=%s/%i)", ipOut, c );
        
		localeName = odlocale_record_search(op,NetObj,searchbase);
	}
    
	if (localeName != NULL) {
		Debug( LDAP_DEBUG_TRACE, "found locale %s \n", localeName, 0, 0 );
		return localeName;
	} else {
		return defaultLocaleName;
	}
    
}

static int odlocales_response( Operation *op, SlapReply *rs )
{	
	if ((rs->sr_type != REP_SEARCH) || (op->oq_search.rs_attrs == NULL) ){
		return SLAP_CB_CONTINUE;
	}
	if (strstr(op->oq_search.rs_attrs->an_name.bv_val, "netlogon") == NULL) {
        return SLAP_CB_CONTINUE;
	}
	
	char *theLocale;
	int rc = 0;
	Debug( LDAP_DEBUG_TRACE, "OD Locale search called by client %s \n", op->o_hdr->oh_conn->c_peer_name.bv_val, 0, 0 );
    
	// get the baseDN
	struct berval searchbase;
	Attribute *bDN = NULL;
	AttributeDescription	*ad = NULL;
	const char	*text = NULL;
	rc = slap_str2ad( "namingContexts", &ad, &text );
	bDN = attr_find(rs->sr_un.sru_search.r_entry->e_attrs, ad);
    if(!bDN || !bDN->a_vals->bv_val) {
        return SLAP_CB_CONTINUE;
    }
    
    char *baseDN = bDN->a_vals->bv_val;
	
	ber_str2bv( baseDN, 0, 0, &searchbase );
	
	// Get the locale domain name from locales config record
	char *theCloud;
	theCloud = odlocale_record_search(op,"(cn=locales)",&searchbase);
    
	
	// get the IP of the client to search for
	char *clientIP = strtok ( op->o_hdr->oh_conn->c_peer_name.bv_val, ":=" );
	clientIP = strtok ( NULL,":=");
	
	//Look up the locale.  If no locale is found, it will return "DefaultLocale"
	theLocale = get_locales(op,clientIP,&searchbase);
	
	// build the response
	
	// If the locale is the default locale, just return that.  If it's
	// something other than the default, return both that locale and the
	// default locale.
    char* entry = NULL;
    if (strncmp(theLocale, defaultLocaleName, strlen(defaultLocaleName)) == 0) {
        asprintf(&entry, "dn: \nClientSiteName: %s\nDNSDomainName: %s\nDNSForestName: %s\n", theLocale, theCloud,theCloud);
    }
    else {
        asprintf(&entry, "dn: \nClientSiteName: %s\nClientSiteName: %s\nDNSDomainName: %s\nDNSForestName: %s\n", theLocale, defaultLocaleName, theCloud,theCloud);
    }
    
    if (entry) {
        rs->sr_err = LDAP_SUCCESS;
        rs->sr_un.sru_search.r_entry = str2entry2(entry, 0);
        free(entry);
        entry = NULL;
    }
	
	//make sure we return all attributes, otherwise, it'll return none!
	rs->sr_attrs = slap_anlist_all_attributes;
	
	
	/* Default is to just fall through to the normal processing */
	return SLAP_CB_CONTINUE;
}


// currently not used
static int
odlocales_db_config(
                    BackendDB	*be,
                    const char	*fname,
                    int		lineno,
                    int		argc,
                    char		**argv )
{
	
	if ( strcasecmp( argv[ 0 ], "odlocales-base" ) == 0 ) {
        Debug( LDAP_DEBUG_TRACE, "OD Locale search base configured as: %s \n", argv[ 1 ], 0, 0 );
        
        
		return 0;
		
	}
	
}	

static ConfigDriver	odlocales_cf;

static ConfigTable localecfg[] = {
	{ "odlocales", "enabled", 1, 1, 0,
		ARG_MAGIC, odlocales_cf,
		"( OLcfgOvAt:700.10 NAME 'olcLocalesEnabled' "
		"DESC 'Enables LDAP Ping for OD Locales' "
		"EQUALITY booleanMatch "
		"SYNTAX OMsBoolean SINGLE-VALUE )", NULL, NULL },
	{ NULL, NULL, 0, 0, 0, ARG_IGNORED }
};


static ConfigOCs localeocs[] = {
	{ "( OLcfgOvOc:700.10 "
		"NAME 'olcODLocale' "
		"DESC 'OD Locale Overlay configuration' "
		"SUP olcOverlayConfig "
		"MAY (olcLocalesEnabled) )",
		Cft_Overlay, localecfg, NULL, NULL },
	{ NULL, 0, NULL }
};


static int odlocales_cf( ConfigArgs *c )
{
	Debug( LDAP_DEBUG_TRACE, "OD Locales overlay odlocales_cf \n", 0, 0, 0 );
	slap_overinst *on = (slap_overinst *)c->bi;
	int rc = 1;
	
	//Stuff goes here :-)
	
	return rc;
}


int odlocales_initialize() {
	int rc = 0;
	Debug( LDAP_DEBUG_TRACE, "==> OD Locales overlay initialize called \n", 0, 0, 0 );
	memset( &odlocales, 0, sizeof( slap_overinst ) );
	
	odlocales.on_bi.bi_type = "odlocales";
	odlocales.on_response = odlocales_response;
	//odlocales.on_bi.bi_db_config = odlocales_db_config;
	odlocales.on_bi.bi_cf_ocs = localeocs;
	
	rc = config_register_schema( localecfg, localeocs );
	if ( rc ) {
		return rc;
	}
	
	
	return (overlay_register(&odlocales));
}

#if SLAPD_OVER_ODLOCALES == SLAPD_MOD_DYNAMIC
int
init_module( int argc, char *argv[] )
{
	Debug( LDAP_DEBUG_TRACE, "OD Locales overlay init_module \n", 0, 0, 0 );
	return odlocales_initialize();
}
#endif
#endif /* SLAPD_OVER_ODLOCALES */
