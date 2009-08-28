#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <krb5.h>
#include <com_err.h>
#include "krb5_constants.c"

#ifdef __cplusplus
}
#endif

/* change this if 10 hours doesn't suit you */
#define KRB5_DEFAULT_LIFE 60*60*10

typedef krb5_ccache		Authen__Krb5__Ccache;
typedef krb5_principal		Authen__Krb5__Principal;
typedef krb5_auth_context	Authen__Krb5__AuthContext;
typedef krb5_rcache		Authen__Krb5__Rcache;
typedef krb5_creds		*Authen__Krb5__Creds;
typedef krb5_ap_rep_enc_part	*Authen__Krb5__ApRepEncPart;
typedef krb5_ticket		*Authen__Krb5__Ticket;
typedef krb5_keytab		Authen__Krb5__Keytab;
typedef krb5_enc_tkt_part	*Authen__Krb5__EncTktPart;
typedef krb5_error		*Authen__Krb5__Error;
typedef krb5_address		*Authen__Krb5__Address;
typedef krb5_keyblock		*Authen__Krb5__Keyblock;
typedef krb5_keytab_entry	*Authen__Krb5__KeytabEntry;
typedef krb5_kt_cursor          *Authen__Krb5__KeytabCursor;
typedef krb5_cc_cursor          *Authen__Krb5__CcacheCursor;
typedef krb5_keyblock		*Authen__Krb5__KeyBlock;

static krb5_context context = 0;
static krb5_error_code err;
static krb5_keytab_entry keytab_entry_init;

/*
 * The following three routines implement a "safehouse" for nested Kerberos
 * data structures which shouldn't be freed before their parent data
 * structures are freed.  Without this, "Bad free() ignored" errors as well
 * as core dumps could occur when the parent structures are eventually freed.
 *
 * If a method returns a newly allocated object, it calls can_free() to
 * register the object as "freeable," since the memory was not in use
 * beforehand.  This module will only free objects that have been registered
 * with can_free(), and lets Kerberos free the others.
 *
 * Doing it the other way (registering objects which *shouldn't* be freed)
 * is more complicated than it first seems, so I did it this way.
 */


HV *free_hash = NULL; /* might as well take advantage of Perl! */

void can_free(SV *sv)
{
	char key[80];

	sprintf(key,"%p",sv);
	if (!free_hash) free_hash = newHV();
	hv_store(free_hash,key,strlen(key),&PL_sv_yes,0);
}

int should_free(SV *sv)
{
	char key[80];

	if (!free_hash) return 0;
	sprintf(key,"%p",sv);
	return hv_exists(free_hash,key,strlen(key));
}

void freed(SV *sv)
{
	char key[80];

	if (!free_hash) return;
	sprintf(key,"%p",sv);
	hv_delete(free_hash,key,strlen(key),G_DISCARD);
}


MODULE = Authen::Krb5		PACKAGE = Authen::Krb5		PREFIX = krb5_

double
constant(name, arg)
	char *name
	int arg

void
krb5_error(e = 0)
	krb5_error_code e;

	CODE:
	if (e) {
		ST(0) = sv_2mortal(newSVpv((char *)error_message(e), 0));
	}
	else {
		ST(0) = sv_2mortal(newSVpv((char *)error_message(err), 0));
		SvUPGRADE(ST(0), SVt_PVIV);
		SvIVX(ST(0)) = err;
		SvIOK_on(ST(0));
	}

void
krb5_init_context()

	CODE:
	if (context) croak("Authen::Krb5 already initialized");
	err = krb5_init_context(&context);
	if (err) XSRETURN_UNDEF;
	XSRETURN_YES;

void
krb5_free_context()

	CODE:
	if (!context) croak("Authen::Krb5 not yet initialized");
	krb5_free_context(context);

void
krb5_init_ets()

	CODE:
#if KRB5_DEPRECATED
	krb5_init_ets(context);
#endif /* KRB5_DEPRECATED */
	XSRETURN_YES;

void
krb5_get_default_realm()

	PREINIT:
	char *realm;

	PPCODE:
	err = krb5_get_default_realm(context,&realm);
	if (err || !realm) XSRETURN_UNDEF;
	XPUSHs(sv_2mortal(newSVpv(realm,strlen(realm))));
	Safefree(realm);

void
krb5_get_host_realm(host)
	char *host

	PREINIT:
	char **realmlist;
	int i;

	PPCODE:
	err = krb5_get_host_realm(context,host,&realmlist);
	if (err || !realmlist) XSRETURN_UNDEF;
	for (i = 0; realmlist[i]; i++) {
		XPUSHs(sv_2mortal(newSVpv(realmlist[i],
			strlen(realmlist[i]))));
	}
	krb5_free_host_realm(context,realmlist);

void
krb5_get_krbhst(realm)
	char *realm

	PREINIT:
	krb5_data realm_data;
	char **hostlist;
	int i;

	PPCODE:
	realm_data.data = realm;
	realm_data.length = strlen(realm);
	err = krb5_get_krbhst(context,&realm_data,&hostlist);
	if (err || !hostlist) XSRETURN_UNDEF;
	for (i = 0; hostlist[i]; i++) {
		XPUSHs(sv_2mortal(newSVpv(hostlist[i],
			strlen(hostlist[i]))));
	}
	krb5_free_krbhst(context,hostlist);

Authen::Krb5::Principal
krb5_build_principal_ext(p)
	Authen::Krb5::Principal p

	CODE:
	err = krb5_build_principal_ext(context, &RETVAL,
		krb5_princ_realm(context, p)->length,
		krb5_princ_realm(context, p)->data,
		KRB5_TGS_NAME_SIZE, KRB5_TGS_NAME,
		krb5_princ_realm(context, p)->length,
		krb5_princ_realm(context, p)->data,
		0);

	if (err) XSRETURN_UNDEF;

	can_free((SV *)RETVAL);

	OUTPUT:
	RETVAL

Authen::Krb5::Principal
krb5_parse_name(name)
	char *name

	CODE:
	err = krb5_parse_name(context,name,&RETVAL);
	if (err) XSRETURN_UNDEF;

	can_free((SV *)RETVAL);

	OUTPUT:
	RETVAL

Authen::Krb5::Principal
krb5_sname_to_principal(hostname,sname,type)
	char *hostname
	char *sname
	krb5_int32 type

	CODE:
	err = krb5_sname_to_principal(context,hostname,sname,type,&RETVAL);
	if (err) XSRETURN_UNDEF;

	can_free((SV *)RETVAL);

	OUTPUT:
	RETVAL

Authen::Krb5::Ccache
krb5_cc_resolve(string_name)
	char *string_name

	CODE:
	err = krb5_cc_resolve(context, string_name, &RETVAL);
	if (err) XSRETURN_UNDEF;

	can_free((SV *)RETVAL);

	OUTPUT:
	RETVAL

char *
krb5_cc_default_name()

	CODE:
	RETVAL = (char *)krb5_cc_default_name(context);

	OUTPUT:
	RETVAL

Authen::Krb5::Ccache
krb5_cc_default()

	CODE:
	err = krb5_cc_default(context, &RETVAL);
	if (err) XSRETURN_UNDEF;

	can_free((SV *)RETVAL);

	OUTPUT:
	RETVAL

Authen::Krb5::Keytab
krb5_kt_resolve(string_name)
	char *string_name

	CODE:
	err = krb5_kt_resolve(context, string_name, &RETVAL);
	if (err) XSRETURN_UNDEF;

	can_free((SV *)RETVAL);

	OUTPUT:
	RETVAL

char *
krb5_kt_default_name()
        CODE:
        char name[BUFSIZ];
        err = krb5_kt_default_name(context, name, sizeof name - 1);
	if (err)
                XSRETURN_UNDEF;
        name[sizeof name - 1] = '\0';
	ST(0) = sv_2mortal(newSVpv(name, 0));

Authen::Krb5::Keytab
krb5_kt_default()
        CODE:
        err = krb5_kt_default(context, &RETVAL);
	if (err)
                XSRETURN_UNDEF;

        OUTPUT:
        RETVAL

Authen::Krb5::Keyblock
krb5_kt_read_service_key(name, principal, kvno = 0, enctype = 0)
        char *name
        Authen::Krb5::Principal principal
        krb5_kvno kvno
        krb5_enctype enctype

        CODE:
        err = krb5_kt_read_service_key(context, name, principal, kvno, enctype,
            &RETVAL);
        if (err)
                XSRETURN_UNDEF;
        can_free((SV *)RETVAL);

        OUTPUT:
        RETVAL

void
krb5_get_in_tkt_with_password(client, server, password, cc)
	Authen::Krb5::Principal client
	Authen::Krb5::Principal server
	char *password
	Authen::Krb5::Ccache cc

	PREINIT:
	krb5_creds cr;
	krb5_timestamp now;
	krb5_deltat lifetime = 0;

	CODE:
	memset((char *)&cr,0,sizeof(krb5_creds));
	krb5_timeofday(context, &now);
	cr.client = client;
	cr.server = server;
	cr.times.starttime = now;
	cr.times.endtime = now + KRB5_DEFAULT_LIFE;
	cr.times.renew_till = 0;

	err = krb5_get_in_tkt_with_password(context, 0, 0, NULL, NULL,
		password, cc, &cr, 0);

	if (err) XSRETURN_UNDEF;
	XSRETURN_YES;

void
krb5_get_in_tkt_with_keytab(client, server, keytab, cc)
	Authen::Krb5::Principal client
	Authen::Krb5::Principal server
	Authen::Krb5::Keytab    keytab
	Authen::Krb5::Ccache cc

	PREINIT:
	krb5_creds cr;
	krb5_timestamp now;
	krb5_deltat lifetime = 0;

	CODE:
	memset((char *)&cr,0,sizeof(krb5_creds));
	krb5_timeofday(context, &now);
	cr.client = client;
	cr.server = server;
	cr.times.starttime = now;
	cr.times.endtime = now + KRB5_DEFAULT_LIFE;
	cr.times.renew_till = 0;

	err = krb5_get_in_tkt_with_keytab(context, 0, 0, NULL, NULL,
		keytab, cc, &cr, 0);

	if (err) XSRETURN_UNDEF;
	XSRETURN_YES;

SV *
krb5_mk_req(auth_context, ap_req_options, service, hostname, in, cc)
	Authen::Krb5::AuthContext auth_context
	krb5_flags ap_req_options
	char *service
	char *hostname
	SV *in
	Authen::Krb5::Ccache cc

	PREINIT:
	krb5_data in_data, out_data;

	CODE:
	in_data.data = SvPV(in,in_data.length);
	err = krb5_mk_req(context,&auth_context,ap_req_options,service,hostname,
		&in_data,cc,&out_data);
	if (err) XSRETURN_UNDEF;
	RETVAL = newSVpv(out_data.data,out_data.length);

	OUTPUT:
	RETVAL

Authen::Krb5::Ticket
krb5_rd_req(auth_context,in,server,keytab=0)
	Authen::Krb5::AuthContext auth_context
	SV *in
	Authen::Krb5::Principal server
	Authen::Krb5::Keytab keytab

	PREINIT:
	krb5_data in_data;
	krb5_ticket *t;
	krb5_flags ap_req_options;

	CODE:
	if (!New(0,t,1,krb5_ticket)) XSRETURN_UNDEF;
	in_data.data = SvPV(in,in_data.length);
	err = krb5_rd_req(context,&auth_context,&in_data,server,keytab,
		NULL,&t);
	if (err) XSRETURN_UNDEF;
	RETVAL = t;

	can_free((SV *)RETVAL);

	OUTPUT:
	RETVAL

Authen::Krb5::Address
gen_portaddr(addr,port)
	Authen::Krb5::Address addr
	unsigned short port

	CODE:
	err = krb5_gen_portaddr(context,addr,(krb5_pointer)&port,&RETVAL);
	if (err) XSRETURN_UNDEF;

	OUTPUT:
	RETVAL

void
genaddrs(auth_context,fh,flags)
	Authen::Krb5::AuthContext auth_context
	FILE *fh; 
	krb5_flags flags

	PREINIT:
	int fd;

	CODE:
	fd = fileno(fh);
	err = krb5_auth_con_genaddrs(context,auth_context,fd,flags);
	if (err) XSRETURN_UNDEF;
	XSRETURN_YES;  

char *
gen_replay_name(addr,uniq)
	Authen::Krb5::Address addr
	char *uniq

	CODE:
	err = krb5_gen_replay_name(context,addr,uniq,&RETVAL);
	if (err) XSRETURN_UNDEF;

	OUTPUT:
	RETVAL
	
void
krb5_mk_priv(auth_context,in)
	Authen::Krb5::AuthContext auth_context
	SV *in

	PREINIT:
	krb5_data in_data, out_data;

	PPCODE:
	in_data.data = SvPV(in,in_data.length);
	err = krb5_mk_priv(context,auth_context,&in_data,&out_data,NULL);
	if (err) XSRETURN_UNDEF;
	XPUSHs(sv_2mortal(newSVpv(out_data.data,out_data.length)));
	/* krb5_free_data(context,&out_data); */

void
krb5_rd_priv(auth_context,in)
	Authen::Krb5::AuthContext auth_context
	SV *in

	PREINIT:
	krb5_data in_data, out_data;

	PPCODE:
	in_data.data = SvPV(in,in_data.length);
	err = krb5_rd_priv(context,auth_context,&in_data,&out_data,NULL);
	if (err) XSRETURN_UNDEF;
	XPUSHs(sv_2mortal(newSVpv(out_data.data,out_data.length)));

Authen::Krb5::Rcache
krb5_get_server_rcache(piece)
	SV *piece

	PREINIT:
	krb5_data rc_data;

	CODE:
	rc_data.data=SvPV(piece,rc_data.length);
	err = krb5_get_server_rcache(context,&rc_data,&RETVAL);

	if (err) XSRETURN_UNDEF;

	OUTPUT:
	RETVAL

void
krb5_sendauth(auth_context,fh,version,client,server,options,in,in_creds,cc)
	Authen::Krb5::AuthContext auth_context
	FILE *fh
	char *version
	Authen::Krb5::Principal client
	Authen::Krb5::Principal server
	int options
	SV *in
	Authen::Krb5::Creds in_creds
	Authen::Krb5::Ccache cc

	PREINIT:
	krb5_data in_data;
	krb5_creds *out_creds = NULL;
	int fd;

	PPCODE:
	fd = fileno(fh);
	in_data.data = SvPV(in,in_data.length);
	err = krb5_sendauth(context,&auth_context,&fd,version,client,server,
		options,&in_data,in_creds,cc,NULL,NULL,&out_creds);
	if (err) XSRETURN_UNDEF;
	XSRETURN_YES;

void
krb5_recvauth(auth_context,fh,version,server,keytab)
	Authen::Krb5::AuthContext auth_context
	FILE *fh
	char *version
	Authen::Krb5::Principal server
	Authen::Krb5::Keytab keytab

	PREINIT:
	krb5_ticket *ticket = NULL;
	int fd;

	PPCODE:
	fd = fileno(fh);
	err = krb5_recvauth(context,&auth_context,&fd,version,server,0,
		keytab,&ticket);
	if (err) XSRETURN_UNDEF;
	ST(0) = sv_newmortal();
	sv_setref_pv(ST(0),"Authen::Krb5::Ticket",(void*)ticket);
	XSRETURN(1);


MODULE = Authen::Krb5	PACKAGE = Authen::Krb5::Principal

void
realm(p)
	Authen::Krb5::Principal p

	CODE:
	ST(0) = sv_2mortal(newSVpv(p->realm.data,p->realm.length));

krb5_int32
type(p)
	Authen::Krb5::Principal p

	CODE:
	RETVAL = p->type;

	OUTPUT:
	RETVAL

void
data(p)
	Authen::Krb5::Principal p

	PPCODE:
	if (p->length > 0) {
		int len = p->length;
		krb5_data *data;

		EXTEND(sp,len);
		for (data = p->data; len--; data++) {
			PUSHs(sv_2mortal(newSVpv(data->data,data->length)));
		}
	}

void
DESTROY(p)
	Authen::Krb5::Principal p

	CODE:
	if (p && should_free((SV *)p)) {
		krb5_free_principal(context,p);
		freed((SV *)p);
	}


MODULE = Authen::Krb5	PACKAGE = Authen::Krb5::Ccache

void
initialize(cc, p)
	Authen::Krb5::Ccache cc
	Authen::Krb5::Principal p

	CODE:
	err = krb5_cc_initialize(context, cc, p);
	if (err) XSRETURN_UNDEF;
	else {
		can_free((SV *)cc);
		XSRETURN_YES;
	}

const char *
get_name(cc)
	Authen::Krb5::Ccache cc

	CODE:
	RETVAL = krb5_cc_get_name(context, cc);

	OUTPUT:
	RETVAL

Authen::Krb5::Principal
get_principal(cc)
	Authen::Krb5::Ccache cc

	CODE:
	err = krb5_cc_get_principal(context, cc, &RETVAL);
	if (err) XSRETURN_UNDEF;

	can_free((SV *)RETVAL);

	OUTPUT:
	RETVAL

void
destroy(cc)
	Authen::Krb5::Ccache cc

	CODE:
	if (!should_free((SV*)cc)) XSRETURN_UNDEF;

	err = krb5_cc_destroy(context, cc);
	if (err) {
		XSRETURN_UNDEF;
	}
	else {
		freed((SV*)cc);
		XSRETURN_YES;
	}

krb5_cc_cursor *
start_seq_get(cc)
	Authen::Krb5::Ccache cc

	CODE:
	if (!New(0, RETVAL, 1, krb5_cc_cursor))
		XSRETURN_UNDEF;
        err = krb5_cc_start_seq_get(context, cc, RETVAL);
	if (err)
                XSRETURN_UNDEF;

        OUTPUT:
        RETVAL

Authen::Krb5::Creds
next_cred(cc, cursor)
        krb5_cc_cursor *cursor
        Authen::Krb5::Ccache cc

        CODE:
	if (!New(0, RETVAL, 1, krb5_creds))
		XSRETURN_UNDEF;
        err = krb5_cc_next_cred(context, cc, cursor, RETVAL);
	if (err)
                XSRETURN_UNDEF;
        can_free((SV *)RETVAL);

        OUTPUT:
        RETVAL

void
end_seq_get(cc, cursor)
        Authen::Krb5::Ccache cc
        krb5_cc_cursor *cursor

        CODE:
        err = krb5_cc_end_seq_get(context, cc, cursor);
        if (err)
                XSRETURN_UNDEF;
        XSRETURN_YES;

void
DESTROY(cc)
	Authen::Krb5::Ccache cc

	CODE:
	if (should_free((SV *)cc)) {
		krb5_cc_close(context, cc);
		freed((SV *)cc);
	}

MODULE = Authen::Krb5	PACKAGE = Authen::Krb5::KeyBlock

int
length(kb)
	Authen::Krb5::KeyBlock kb

	CODE:
	RETVAL = kb->length;

	OUTPUT:
	RETVAL

void
contents(kb)
	Authen::Krb5::KeyBlock kb

	PPCODE:
	/* sv_2mortal here causes 'Attempt to free unreferenced scalar' later */
	XPUSHs(newSVpvn((char*)(kb->contents), kb->length));

int
enctype(kb)
	Authen::Krb5::KeyBlock kb

	CODE:
	RETVAL = (int)kb->enctype;

	OUTPUT:
	RETVAL

void
enctype_string(kb)
	Authen::Krb5::KeyBlock kb

	PREINIT:
	char buf[256];

	PPCODE:
	err = krb5_enctype_to_string(kb->enctype, buf, 255);
	if (err) {
		XSRETURN_UNDEF;
	}
	XPUSHs(newSVpv(buf, 0));

void
DESTROY(kb)
	Authen::Krb5::KeyBlock kb

	CODE:
	if (kb && should_free((SV *)kb)) {
		krb5_free_keyblock(context,kb);
		freed((SV *)kb);
	}

MODULE = Authen::Krb5	PACKAGE = Authen::Krb5::AuthContext

Authen::Krb5::AuthContext
new(class)
	char *class

	CODE:
	err = krb5_auth_con_init(context, &RETVAL);
	if (err) XSRETURN_UNDEF;

	can_free((SV *)RETVAL);

	OUTPUT:
	RETVAL

int
getflags(auth_context)
	Authen::Krb5::AuthContext auth_context

	PREINIT:
	krb5_int32 flags;

	CODE:
	err = krb5_auth_con_getflags(context,auth_context,&flags);
	RETVAL = (int)flags;

	OUTPUT:
	RETVAL

void
setflags(auth_context,flags)
	Authen::Krb5::AuthContext auth_context
	krb5_int32 flags

	CODE:
	err = krb5_auth_con_setflags(context,auth_context,flags);

	if(err) XSRETURN_UNDEF;
	XSRETURN_YES;

Authen::Krb5::Rcache
getrcache(auth_context)
	Authen::Krb5::AuthContext auth_context

	CODE:
	err = krb5_auth_con_getrcache(context,auth_context,&RETVAL);
	if (err) XSRETURN_UNDEF;

	OUTPUT:
	RETVAL

void
setrcache(auth_context,rc)
	Authen::Krb5::AuthContext auth_context
	Authen::Krb5::Rcache rc

	CODE:
	err = krb5_auth_con_setrcache(context,auth_context,rc);
	if (err) XSRETURN_UNDEF;
	XSRETURN_YES;

void
getaddrs(auth_context)
	Authen::Krb5::AuthContext auth_context

	PREINIT:
	krb5_address *local, *remote;

	CODE:
	err = krb5_auth_con_getaddrs(context,auth_context,&local,&remote);
	if (err) XSRETURN_EMPTY;

	ST(0) = sv_newmortal();
	ST(1) = sv_newmortal();
	sv_setref_pv(ST(0), "Authen::Krb5::Address", (void*)local);
	sv_setref_pv(ST(1), "Authen::Krb5::Address", (void*)remote);
	XSRETURN(2);

void
setaddrs(auth_context,laddr,raddr)
	Authen::Krb5::AuthContext auth_context
	Authen::Krb5::Address laddr
	Authen::Krb5::Address raddr

	CODE:
	if (!SvOK((SV*)ST(1))) laddr = NULL;
	if (!SvOK((SV*)ST(2))) raddr = NULL;
	err = krb5_auth_con_setaddrs(context,auth_context,laddr,raddr);
	if (err) XSRETURN_UNDEF;
	XSRETURN_YES;

void
setports(auth_context,laddr,raddr)
	Authen::Krb5::AuthContext auth_context
	Authen::Krb5::Address laddr
	Authen::Krb5::Address raddr

	CODE:
	if (!SvOK((SV*)ST(1))) laddr = NULL;
	if (!SvOK((SV*)ST(2))) raddr = NULL;
	err = krb5_auth_con_setports(context,auth_context,laddr,raddr);
	if (err) XSRETURN_UNDEF;
	XSRETURN_YES;

Authen::Krb5::KeyBlock
getkey(auth_context)
	Authen::Krb5::AuthContext auth_context;

	PREINIT:
	SV *sv;

	CODE:
	err = krb5_auth_con_getkey(context, auth_context, &RETVAL);
	if (err) XSRETURN_UNDEF;
	can_free((SV *)RETVAL);

	OUTPUT:
	RETVAL

void
DESTROY(auth_context)
	Authen::Krb5::AuthContext auth_context;

	CODE:
	if (auth_context) {
		krb5_auth_con_free(context, auth_context);
		freed((SV *)auth_context);
	}

MODULE = Authen::Krb5	PACKAGE = Authen::Krb5::Ticket

Authen::Krb5::Principal
server(t)
	Authen::Krb5::Ticket t

	CODE:
	RETVAL = t->server;

	OUTPUT:
	RETVAL

Authen::Krb5::EncTktPart
enc_part2(t)
	Authen::Krb5::Ticket t

	CODE:
	RETVAL = t->enc_part2;

	OUTPUT:
	RETVAL

void
DESTROY(t)
	Authen::Krb5::Ticket t

	CODE:
	if (t) {
		krb5_free_ticket(context,t);
		freed((SV *)t);
	}

MODULE = Authen::Krb5	PACKAGE = Authen::Krb5::EncTktPart

Authen::Krb5::Principal
client(etp)
	Authen::Krb5::EncTktPart etp

	CODE:
	RETVAL = etp->client;

	OUTPUT:
	RETVAL

void
DESTROY(etp)
	Authen::Krb5::EncTktPart etp

	CODE:
	if (etp && should_free((SV *)etp)) {
		krb5_free_enc_tkt_part(context,etp);
		freed((SV *)etp);
	}

MODULE = Authen::Krb5	PACKAGE = Authen::Krb5::Address

Authen::Krb5::Address
new(class,addrtype,contents)
	char *class
	unsigned int addrtype
	SV *contents

	CODE:
	if (!New(0,RETVAL,1,krb5_address)) XSRETURN_UNDEF;
	RETVAL->addrtype = addrtype;
	RETVAL->contents = (krb5_octet *)SvPV(contents,RETVAL->length);
	
	OUTPUT:
	RETVAL

void
DESTROY(addr)
	Authen::Krb5::Address addr

	CODE:
	if (addr && should_free((SV *)addr)) {
		krb5_free_address(context,addr);
		freed((SV *)addr);
	}

MODULE = Authen::Krb5	PACKAGE = Authen::Krb5::Keyblock

 
krb5_enctype
enctype(keyblock)
	Authen::Krb5::Keyblock keyblock

        CODE:
        RETVAL = keyblock->enctype;

        OUTPUT:
        RETVAL
 
unsigned int
length(keyblock)
        Authen::Krb5::Keyblock keyblock

        CODE:
        RETVAL = keyblock->length;

        OUTPUT:
        RETVAL

krb5_octet *
contents(keyblock)
        Authen::Krb5::Keyblock keyblock

        CODE:
        ST(0) = keyblock->contents
	    ? sv_2mortal(newSVpv(keyblock->contents, keyblock->length))
            : &PL_sv_undef;


void
DESTROY(keyblock)
	Authen::Krb5::Keyblock	keyblock

        CODE:
	if (keyblock->contents) {
		memset(keyblock->contents, 0, keyblock->length);
		free(keyblock->contents);
		keyblock->contents = NULL;
	}

MODULE = Authen::Krb5   PACKAGE = Authen::Krb5::Keytab

void
add_entry(keytab, entry)
        Authen::Krb5::Keytab keytab
        Authen::Krb5::KeytabEntry entry

        CODE:
        err = krb5_kt_add_entry(context, keytab, entry);
	if (err)
                XSRETURN_UNDEF;
	XSRETURN_YES;

void
end_seq_get(keytab, cursor)
        Authen::Krb5::Keytab keytab
        krb5_kt_cursor *cursor

        CODE:
        err = krb5_kt_end_seq_get(context, keytab, cursor);
        if (err)
                XSRETURN_UNDEF;
        XSRETURN_YES;

Authen::Krb5::KeytabEntry
get_entry(keytab, principal, vno = 0, enctype = 0)
        Authen::Krb5::Keytab keytab
        Authen::Krb5::Principal principal
        krb5_kvno vno
        krb5_enctype enctype

        CODE:
	if (!New(0, RETVAL, 1, krb5_keytab_entry))
		XSRETURN_UNDEF;
        err = krb5_kt_get_entry(context, keytab, principal, vno, enctype,
            RETVAL);
	if (err)
		XSRETURN_UNDEF;
        can_free((SV *)RETVAL);
	
	OUTPUT:
	RETVAL
        
SV *
get_name(keytab)
        Authen::Krb5::Keytab keytab

        PREINIT:
        char name[MAX_KEYTAB_NAME_LEN+1];

        CODE:
        err = krb5_kt_get_name(context, keytab, name, MAX_KEYTAB_NAME_LEN);
	if (err)
                XSRETURN_UNDEF;
	RETVAL = sv_2mortal(newSVpv(name, 0));
        can_free((SV *)RETVAL);

        OUTPUT:
        RETVAL

Authen::Krb5::KeytabEntry
next_entry(keytab, cursor)
        krb5_kt_cursor *cursor
        Authen::Krb5::Keytab keytab

        CODE:
	if (!New(0, RETVAL, 1, krb5_keytab_entry))
		XSRETURN_UNDEF;
        err = krb5_kt_next_entry(context, keytab, RETVAL, cursor);
	if (err)
                XSRETURN_UNDEF;
        can_free((SV *)RETVAL);

        OUTPUT:
        RETVAL

void
remove_entry(keytab, entry)
        Authen::Krb5::Keytab keytab
        Authen::Krb5::KeytabEntry entry

        CODE:
        err = krb5_kt_remove_entry(context, keytab, entry);
	if (err)
                XSRETURN_UNDEF;
	XSRETURN_YES;

krb5_kt_cursor *
start_seq_get(keytab)
        Authen::Krb5::Keytab keytab

        CODE:
	if (!New(0, RETVAL, 1, krb5_kt_cursor))
		XSRETURN_UNDEF;
        err = krb5_kt_start_seq_get(context, keytab, RETVAL);
	if (err)
                XSRETURN_UNDEF;

        OUTPUT:
        RETVAL

void
DESTROY(keytab)
        Authen::Krb5::Keytab keytab

        CODE:
        if (keytab && should_free((SV *)keytab)) {
                krb5_kt_close(context, keytab);
                freed((SV *)keytab);
        }

MODULE = Authen::Krb5   PACKAGE = Authen::Krb5::KeytabEntry

Authen::Krb5::KeytabEntry
new(class, principal, vno, key)
	char *class
	Authen::Krb5::Principal principal
	krb5_kvno vno
        Authen::Krb5::Keyblock key

	CODE:
	if (!New(0, RETVAL, 1, krb5_keytab_entry))
		XSRETURN_UNDEF;
        *RETVAL = keytab_entry_init;
	RETVAL->principal = principal;
	RETVAL->vno = vno;
        RETVAL->key = *key;

        can_free((SV *)RETVAL);
	
	OUTPUT:
	RETVAL

Authen::Krb5::Principal
principal(entry)
	Authen::Krb5::KeytabEntry entry

        CODE:
        err = krb5_copy_principal(context, entry->principal, &RETVAL);
        if (err)
                XSRETURN_UNDEF;
        can_free((SV *)RETVAL);

        OUTPUT:
        RETVAL

krb5_timestamp
timestamp(entry)
	Authen::Krb5::KeytabEntry entry

        CODE:
        RETVAL = entry->timestamp;

        OUTPUT:
        RETVAL
 
krb5_kvno
kvno(entry)
	Authen::Krb5::KeytabEntry entry

        CODE:
        RETVAL = entry->vno;

        OUTPUT:
        RETVAL
 
Authen::Krb5::Keyblock
key(entry)
	Authen::Krb5::KeytabEntry entry

        CODE:
        err = krb5_copy_keyblock(context, &entry->key, &RETVAL);
        if (err)
                XSRETURN_UNDEF;
        can_free((SV *)RETVAL);

        OUTPUT:
        RETVAL
