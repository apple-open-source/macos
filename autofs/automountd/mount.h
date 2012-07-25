/*
 * This file was generated using rpcgen, and then hand-tweaked to get rid
 * of recursion when processing lists.  In this case, to iterate is human
 * but efficient; to recurse is divine but wasteful.
 */

#ifndef _MOUNT_H_RPCGEN
#define _MOUNT_H_RPCGEN

#define RPCGEN_VERSION	199506

#include <oncrpc/rpc.h>

#define MNTPATHLEN 1024
#define MNTNAMLEN 255
#define FHSIZE 32
#define FHSIZE3 64

typedef char fhandle[FHSIZE];
#ifdef __cplusplus
extern "C" bool_t xdr_fhandle(XDR *, fhandle);
#elif __STDC__
extern  bool_t xdr_fhandle(XDR *, fhandle);
#else /* Old Style C */
bool_t xdr_fhandle();
#endif /* Old Style C */


typedef struct {
	u_int fhandle3_len;
	char *fhandle3_val;
} fhandle3;
#ifdef __cplusplus
extern "C" bool_t xdr_fhandle3(XDR *, fhandle3*);
#elif __STDC__
extern  bool_t xdr_fhandle3(XDR *, fhandle3*);
#else /* Old Style C */
bool_t xdr_fhandle3();
#endif /* Old Style C */


struct fhstatus {
	u_int fhs_status;
	union {
		fhandle fhs_fhandle;
	} fhstatus_u;
};
typedef struct fhstatus fhstatus;
#ifdef __cplusplus
extern "C" bool_t xdr_fhstatus(XDR *, fhstatus*);
#elif __STDC__
extern  bool_t xdr_fhstatus(XDR *, fhstatus*);
#else /* Old Style C */
bool_t xdr_fhstatus();
#endif /* Old Style C */


enum mountstat3 {
	MNT3_OK = 0,
	MNT3ERR_PERM = 1,
	MNT3ERR_NOENT = 2,
	MNT3ERR_IO = 5,
	MNT3ERR_ACCES = 13,
	MNT3ERR_NOTDIR = 20,
	MNT3ERR_INVAL = 22,
	MNT3ERR_NAMETOOLONG = 63,
	MNT3ERR_NOTSUPP = 10004,
	MNT3ERR_SERVERFAULT = 10006,
};
typedef enum mountstat3 mountstat3;
#ifdef __cplusplus
extern "C" bool_t xdr_mountstat3(XDR *, mountstat3*);
#elif __STDC__
extern  bool_t xdr_mountstat3(XDR *, mountstat3*);
#else /* Old Style C */
bool_t xdr_mountstat3();
#endif /* Old Style C */


struct mountres3_ok {
	fhandle3 fhandle;
	struct {
		u_int auth_flavors_len;
		int *auth_flavors_val;
	} auth_flavors;
};
typedef struct mountres3_ok mountres3_ok;
#ifdef __cplusplus
extern "C" bool_t xdr_mountres3_ok(XDR *, mountres3_ok*);
#elif __STDC__
extern  bool_t xdr_mountres3_ok(XDR *, mountres3_ok*);
#else /* Old Style C */
bool_t xdr_mountres3_ok();
#endif /* Old Style C */


struct mountres3 {
	mountstat3 fhs_status;
	union {
		mountres3_ok mountinfo;
	} mountres3_u;
};
typedef struct mountres3 mountres3;
#ifdef __cplusplus
extern "C" bool_t xdr_mountres3(XDR *, mountres3*);
#elif __STDC__
extern  bool_t xdr_mountres3(XDR *, mountres3*);
#else /* Old Style C */
bool_t xdr_mountres3();
#endif /* Old Style C */


typedef char *dirpath;
#ifdef __cplusplus
extern "C" bool_t xdr_dirpath(XDR *, dirpath*);
#elif __STDC__
extern  bool_t xdr_dirpath(XDR *, dirpath*);
#else /* Old Style C */
bool_t xdr_dirpath();
#endif /* Old Style C */


typedef char *name;
#ifdef __cplusplus
extern "C" bool_t xdr_name(XDR *, name*);
#elif __STDC__
extern  bool_t xdr_name(XDR *, name*);
#else /* Old Style C */
bool_t xdr_name();
#endif /* Old Style C */


typedef struct mountbody *mountlist;
#ifdef __cplusplus
extern "C" bool_t xdr_mountlist(XDR *, mountlist*);
#elif __STDC__
extern  bool_t xdr_mountlist(XDR *, mountlist*);
#else /* Old Style C */
bool_t xdr_mountlist();
#endif /* Old Style C */


struct mountbody {
	name ml_hostname;
	dirpath ml_directory;
	mountlist ml_next;
};
typedef struct mountbody mountbody;


typedef struct groupnode *groups;
#ifdef __cplusplus
extern "C" bool_t xdr_groups(XDR *, groups*);
#elif __STDC__
extern  bool_t xdr_groups(XDR *, groups*);
#else /* Old Style C */
bool_t xdr_groups();
#endif /* Old Style C */


struct groupnode {
	name gr_name;
	groups gr_next;
};
typedef struct groupnode groupnode;


typedef struct exportnode *exports;
#ifdef __cplusplus
extern "C" bool_t xdr_exports(XDR *, exports*);
#elif __STDC__
extern  bool_t xdr_exports(XDR *, exports*);
#else /* Old Style C */
bool_t xdr_exports();
#endif /* Old Style C */


struct exportnode {
	dirpath ex_dir;
	groups ex_groups;
	exports ex_next;
};
typedef struct exportnode exportnode;


#define MOUNTPROG ((rpc_uint)100005)
#define MOUNTVERS ((rpc_uint)1)

#ifdef __cplusplus
#define MOUNTPROC_NULL ((rpc_uint)0)
extern "C" void * mountproc_null_1(void *, CLIENT *);
extern "C" void * mountproc_null_1_svc(void *, struct svc_req *);
#define MOUNTPROC_MNT ((rpc_uint)1)
extern "C" fhstatus * mountproc_mnt_1(dirpath *, CLIENT *);
extern "C" fhstatus * mountproc_mnt_1_svc(dirpath *, struct svc_req *);
#define MOUNTPROC_DUMP ((rpc_uint)2)
extern "C" mountlist * mountproc_dump_1(void *, CLIENT *);
extern "C" mountlist * mountproc_dump_1_svc(void *, struct svc_req *);
#define MOUNTPROC_UMNT ((rpc_uint)3)
extern "C" void * mountproc_umnt_1(dirpath *, CLIENT *);
extern "C" void * mountproc_umnt_1_svc(dirpath *, struct svc_req *);
#define MOUNTPROC_UMNTALL ((rpc_uint)4)
extern "C" void * mountproc_umntall_1(void *, CLIENT *);
extern "C" void * mountproc_umntall_1_svc(void *, struct svc_req *);
#define MOUNTPROC_EXPORT ((rpc_uint)5)
extern "C" exports * mountproc_export_1(void *, CLIENT *);
extern "C" exports * mountproc_export_1_svc(void *, struct svc_req *);
#define MOUNTPROC_EXPORTALL ((rpc_uint)6)
extern "C" exports * mountproc_exportall_1(void *, CLIENT *);
extern "C" exports * mountproc_exportall_1_svc(void *, struct svc_req *);

#elif __STDC__
#define MOUNTPROC_NULL ((rpc_uint)0)
extern  void * mountproc_null_1(void *, CLIENT *);
extern  void * mountproc_null_1_svc(void *, struct svc_req *);
#define MOUNTPROC_MNT ((rpc_uint)1)
extern  fhstatus * mountproc_mnt_1(dirpath *, CLIENT *);
extern  fhstatus * mountproc_mnt_1_svc(dirpath *, struct svc_req *);
#define MOUNTPROC_DUMP ((rpc_uint)2)
extern  mountlist * mountproc_dump_1(void *, CLIENT *);
extern  mountlist * mountproc_dump_1_svc(void *, struct svc_req *);
#define MOUNTPROC_UMNT ((rpc_uint)3)
extern  void * mountproc_umnt_1(dirpath *, CLIENT *);
extern  void * mountproc_umnt_1_svc(dirpath *, struct svc_req *);
#define MOUNTPROC_UMNTALL ((rpc_uint)4)
extern  void * mountproc_umntall_1(void *, CLIENT *);
extern  void * mountproc_umntall_1_svc(void *, struct svc_req *);
#define MOUNTPROC_EXPORT ((rpc_uint)5)
extern  exports * mountproc_export_1(void *, CLIENT *);
extern  exports * mountproc_export_1_svc(void *, struct svc_req *);
#define MOUNTPROC_EXPORTALL ((rpc_uint)6)
extern  exports * mountproc_exportall_1(void *, CLIENT *);
extern  exports * mountproc_exportall_1_svc(void *, struct svc_req *);

#else /* Old Style C */
#define MOUNTPROC_NULL ((rpc_uint)0)
extern  void * mountproc_null_1();
extern  void * mountproc_null_1_svc();
#define MOUNTPROC_MNT ((rpc_uint)1)
extern  fhstatus * mountproc_mnt_1();
extern  fhstatus * mountproc_mnt_1_svc();
#define MOUNTPROC_DUMP ((rpc_uint)2)
extern  mountlist * mountproc_dump_1();
extern  mountlist * mountproc_dump_1_svc();
#define MOUNTPROC_UMNT ((rpc_uint)3)
extern  void * mountproc_umnt_1();
extern  void * mountproc_umnt_1_svc();
#define MOUNTPROC_UMNTALL ((rpc_uint)4)
extern  void * mountproc_umntall_1();
extern  void * mountproc_umntall_1_svc();
#define MOUNTPROC_EXPORT ((rpc_uint)5)
extern  exports * mountproc_export_1();
extern  exports * mountproc_export_1_svc();
#define MOUNTPROC_EXPORTALL ((rpc_uint)6)
extern  exports * mountproc_exportall_1();
extern  exports * mountproc_exportall_1_svc();
#endif /* Old Style C */
#define MOUNTVERS3 ((rpc_uint)3)

#ifdef __cplusplus
extern "C" void * mountproc_null_3(void *, CLIENT *);
extern "C" void * mountproc_null_3_svc(void *, struct svc_req *);
extern "C" mountres3 * mountproc_mnt_3(dirpath *, CLIENT *);
extern "C" mountres3 * mountproc_mnt_3_svc(dirpath *, struct svc_req *);
extern "C" mountlist * mountproc_dump_3(void *, CLIENT *);
extern "C" mountlist * mountproc_dump_3_svc(void *, struct svc_req *);
extern "C" void * mountproc_umnt_3(dirpath *, CLIENT *);
extern "C" void * mountproc_umnt_3_svc(dirpath *, struct svc_req *);
extern "C" void * mountproc_umntall_3(void *, CLIENT *);
extern "C" void * mountproc_umntall_3_svc(void *, struct svc_req *);
extern "C" exports * mountproc_export_3(void *, CLIENT *);
extern "C" exports * mountproc_export_3_svc(void *, struct svc_req *);

#elif __STDC__
extern  void * mountproc_null_3(void *, CLIENT *);
extern  void * mountproc_null_3_svc(void *, struct svc_req *);
extern  mountres3 * mountproc_mnt_3(dirpath *, CLIENT *);
extern  mountres3 * mountproc_mnt_3_svc(dirpath *, struct svc_req *);
extern  mountlist * mountproc_dump_3(void *, CLIENT *);
extern  mountlist * mountproc_dump_3_svc(void *, struct svc_req *);
extern  void * mountproc_umnt_3(dirpath *, CLIENT *);
extern  void * mountproc_umnt_3_svc(dirpath *, struct svc_req *);
extern  void * mountproc_umntall_3(void *, CLIENT *);
extern  void * mountproc_umntall_3_svc(void *, struct svc_req *);
extern  exports * mountproc_export_3(void *, CLIENT *);
extern  exports * mountproc_export_3_svc(void *, struct svc_req *);

#else /* Old Style C */
extern  void * mountproc_null_3();
extern  void * mountproc_null_3_svc();
extern  mountres3 * mountproc_mnt_3();
extern  mountres3 * mountproc_mnt_3_svc();
extern  mountlist * mountproc_dump_3();
extern  mountlist * mountproc_dump_3_svc();
extern  void * mountproc_umnt_3();
extern  void * mountproc_umnt_3_svc();
extern  void * mountproc_umntall_3();
extern  void * mountproc_umntall_3_svc();
extern  exports * mountproc_export_3();
extern  exports * mountproc_export_3_svc();
#endif /* Old Style C */

#endif /* !_MOUNT_H_RPCGEN */
