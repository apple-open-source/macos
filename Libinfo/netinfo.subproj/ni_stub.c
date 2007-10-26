#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#define NI_FAILED 9999
#define RPC_FAILED 16

typedef struct 
{
	int a;
	int b;
} stub_type_0;

typedef struct
{
	int a;
	void *b;
} stub_type_1;

typedef struct
{
	void *a;
	stub_type_1 b;
} stub_type_2;

static stub_type_1 empty_stub_type_1()
{
	stub_type_1 a;
	
	memset(&a, 0, sizeof(stub_type_1));
	return a;
}

static stub_type_2 empty_stub_type_2()
{
	stub_type_2 a;
	
	memset(&a, 0, sizeof(stub_type_2));
	return a;
}

static int debug_log(const char *func)
{
	syslog(LOG_ERR, "NetInfo stub: %s", func);
	return NI_FAILED;
}

const char *ni_error(int a) { return "Communication failure"; }
int multi_call(int a, void *b, int c, int d, int e, void *f, void *g, int h, void *i, void *j, int (*k)(void *, void *, int), int l) { return RPC_FAILED; }
stub_type_2 ni_prop_dup(stub_type_2 a) { return empty_stub_type_2(); }
int ni_name_match(void *a, void *b) { return 0; }
void *ni_name_dup(void *a) { return NULL; }

stub_type_1 ni_idlist_dup(stub_type_1 a)   { return empty_stub_type_1(); }
stub_type_1 ni_proplist_dup(stub_type_1 a) { return empty_stub_type_1(); }
stub_type_1 ni_namelist_dup(stub_type_1 a) { return empty_stub_type_1(); }

int ni_idlist_delete(void *a, int b)                   { return -1; }
int ni_proplist_match(stub_type_1 a, void *b, void *c) { return -1; }
int ni_namelist_match(stub_type_1 a, void *b)          { return -1; }

void ni_idlist_insert(void *a, int b, int c)                            {}
void ni_idlist_free(void *a)                                            {}
void ni_proplist_insert(void *a, stub_type_2 b, int c)                  {}
void ni_proplist_delete(void *a, int b)                                 {}
void ni_proplist_free(void *a)                                          {}
void ni_proplist_list_free(void *a)                                     {}
void ni_prop_free(void *a)                                              {}
void ni_name_free(void *a)                                              {}
void ni_namelist_free(void *a)                                          {}
void ni_namelist_insert(void *a, void *b, int c)                        {}
void ni_namelist_delete(void *a, int b)                                 {}
void ni_entrylist_insert(void *a, stub_type_1 b)                        {}
void ni_entrylist_delete(void *a, int b)                                {}
void ni_entrylist_free(void *a)                                         {}
void ni_parse_url(void *a, void *b, void *c, void *d, void *e, void *f) {}
void ni_setabort(void *a, int b)                                        {}
void ni_setwritetimeout(void *a, int b)                                 {}
void ni_setreadtimeout(void *a, int b)                                  {}
void ni_needwrite(void *a, int b)                                       {}
void ni_free(void *a)                                                   {}

int ni_find(void *a, void *b, void *c, int d)                     { return debug_log(__FUNCTION__); }
int ni_open(void *a, void *b, void *c)                            { return debug_log(__FUNCTION__); }
int ni_fancyopen(void *a, void *b, void *c, void *d)              { return debug_log(__FUNCTION__); }
int ni_host_domain(void *a, void *b, void *c)                     { return debug_log(__FUNCTION__); }
int ni_url(void *a, void *b, void *c)                             { return debug_log(__FUNCTION__); }

void *_ni_dup(void *a)                                            { debug_log(__FUNCTION__); return NULL; }
void *ni_connect(void *a, void *b)                                { debug_log(__FUNCTION__); return NULL; }
void *ni_new(void *a, void *b)                                    { debug_log(__FUNCTION__); return NULL; }

int ni_lookupprop(void *a, void *b, void *c, void *d)             { return NI_FAILED; }
int ni_search(void *a, void *b, void *c, void *d, int e, void *f) { return NI_FAILED; }
int ni_pathsearch(void *a, void *b, void *c)                      { return NI_FAILED; }
int ni_pwdomain(void *a, void *b)                                 { return NI_FAILED; }
int ni_addrtag(void *a, void *b, void *c)                         { return NI_FAILED; }
int ni_statistics(void *q, void *b)                               { return NI_FAILED; }
int ni_root(void *a, void *b)                                     { return NI_FAILED; }
int ni_self(void *a, void *b)                                     { return NI_FAILED; }
int ni_parent(void *q, void *b, void *c)                          { return NI_FAILED; }
int ni_children(void *q, void *b, void *c)                        { return NI_FAILED; }
int ni_create(void *a, void *b, stub_type_1 c, void *d, int e)    { return NI_FAILED; }
int ni_destroy(void *a, void *b, stub_type_0 c)                   { return NI_FAILED; }
int ni_write(void *a, void *b, stub_type_1 c)                     { return NI_FAILED; }
int ni_read(void *a, void *b, void *c)                            { return NI_FAILED; }
int ni_lookup(void *a, void *b, void *c, void *d, void *e)        { return NI_FAILED; }
int ni_lookupread(void *a, void *b, void *c, void *d, void *e)    { return NI_FAILED; }
int ni_list(void *a, void *b, void *c, void *d)                   { return NI_FAILED; }
int ni_listall(void *a, void *b, void *c)                         { return NI_FAILED; }
int ni_readprop(void *a, void *b, int c, void *d)                 { return NI_FAILED; }
int ni_writeprop(void *a, void *b, int c, stub_type_1 d)          { return NI_FAILED; }
int ni_listprops(void *a, void *b, void *c)                       { return NI_FAILED; }
int ni_createprop(void *a, void *b, stub_type_2 c, int d)         { return NI_FAILED; }
int ni_destroyprop(void *a, void *b, int c)                       { return NI_FAILED; }
int ni_renameprop(void *a, void *b, int c, void *d)               { return NI_FAILED; }
int ni_createname(void *a, void *b, int c, void *d, int e)        { return NI_FAILED; }
int ni_destroyname(void *a, void *b, int c, int d)                { return NI_FAILED; }
int ni_writename(void *a, void *b, int c, int d, void *e)         { return NI_FAILED; }
int ni_readname(void *a, void *b, int c, int d, void *e)          { return NI_FAILED; }
int ni_resync(void *a)                                            { return NI_FAILED; }
int ni_setuser(void *a, void *b)                                  { return NI_FAILED; }
int ni_setpassword(void *a, void *b)                              { return NI_FAILED; }

int xdr_ni_id(void *a, void *b)              { return 0; }
int xdr_ni_name(void *a, void *b)            { return 0; }
int xdr_ni_namelist(void *a, void *b)        { return 0; }
int xdr_ni_property(void *a, void *b)        { return 0; }
int xdr_ni_proplist(void *a, void *b)        { return 0; }
int xdr_ni_idlist(void *a, void *b)          { return 0; }
int xdr_ni_object(void *a, void *b)          { return 0; }
int xdr_ni_status(void *a, void *b)          { return 0; }
int xdr_ni_id_res(void *a, void *b)          { return 0; }
int xdr_ni_parent_stuff(void *a, void *b)    { return 0; }
int xdr_ni_parent_res(void *a, void *b)      { return 0; }
int xdr_ni_children_stuff(void *a, void *b)  { return 0; }
int xdr_ni_children_res(void *a, void *b)    { return 0; }
int xdr_ni_entry(void *a, void *b)           { return 0; }
int xdr_ni_entrylist(void *a, void *b)       { return 0; }
int xdr_ni_list_stuff(void *a, void *b)      { return 0; }
int xdr_ni_list_res(void *a, void *b)        { return 0; }
int xdr_ni_proplist_stuff(void *a, void *b)  { return 0; }
int xdr_ni_create_args(void *a, void *b)     { return 0; }
int xdr_ni_proplist_res(void *a, void *b)    { return 0; }
int xdr_ni_create_stuff(void *a, void *b)    { return 0; }
int xdr_ni_create_res(void *a, void *b)      { return 0; }
int xdr_ni_destroy_args(void *a, void *b)    { return 0; }
int xdr_ni_lookup_args(void *a, void *b)     { return 0; }
int xdr_ni_lookup_stuff(void *a, void *b)    { return 0; }
int xdr_ni_lookup_res(void *a, void *b)      { return 0; }
int xdr_ni_name_args(void *a, void *b)       { return 0; }
int xdr_ni_createprop_args(void *a, void *b) { return 0; }
int xdr_ni_writeprop_args(void *a, void *b)  { return 0; }
int xdr_ni_prop_args(void *a, void *b)       { return 0; }
int xdr_ni_namelist_stuff(void *a, void *b)  { return 0; }
int xdr_ni_namelist_res(void *a, void *b)    { return 0; }
int xdr_ni_propname_args(void *a, void *b)   { return 0; }
int xdr_ni_createname_args(void *a, void *b) { return 0; }
int xdr_ni_nameindex_args(void *a, void *b)  { return 0; }
int xdr_ni_writename_args(void *a, void *b)  { return 0; }
int xdr_ni_readname_stuff(void *a, void *b)  { return 0; }
int xdr_ni_readname_res(void *a, void *b)    { return 0; }
int xdr_ni_binding(void *a, void *b)         { return 0; }
int xdr_ni_rparent_res(void *a, void *b)     { return 0; }
int xdr_ni_object_list(void *a, void *b)     { return 0; }
int xdr_ni_object_node(void *a, void *b)     { return 0; }
int xdr_ni_readall_stuff(void *a, void *b)   { return 0; }
int xdr_ni_readall_res(void *a, void *b)     { return 0; }
int xdr_ni_proplist_list(void *a, void *b)   { return 0; }
int xdr_ni_listall_stuff(void *a, void *b)   { return 0; }
int xdr_ni_listall_res(void *a, void *b)     { return 0; }
