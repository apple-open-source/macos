/*$Id: misc.h,v 1.56 2001/06/30 01:14:19 guenther Exp $*/

struct dyna_array{int filled,tspace;char*vals;};
union offori{off_t o;int i;};

#define app_val_type(sp,t,s,v)	(*(t*)app_val_(&sp,sizeof(s))=(v))
#define app_valo(sp,val)	app_val_type(sp,off_t,union offori,val)
#define app_vali(sp,val)	app_val_type(sp,int,union offori,val)

#define app_vall(sp,val)	app_val_type(sp,long,long,val)
#define app_valp(sp,val)	app_val_type(sp,const char*,const char*,val)

#define acc_val_(sp,t,s,off)	(*(t*)&(((s*)sp.vals)[off]))

/* these are lvalues */

#define acc_valo(sp,off)	acc_val_(sp,off_t,union offori,off)
#define acc_vali(sp,off)	acc_val_(sp,int,union offori,off)
#define acc_vall(sp,off)	acc_val_(sp,long,long,off)
#define acc_valp(sp,off)	acc_val_(sp,const char*,const char*,off)

struct dynstring{struct dynstring*enext;char ename[255];};

void
 elog P((const char*const newt)),
 ignoreterm P((void)),
 shutdesc P((void)),
 checkroot P((const int c,const unsigned long Xid)),
 setids P((void)),
 writeerr P((const char*const line)),
 progerr P((const char*const line,int xitcode,int okay)),
 chderr P((const char*const dir)),
 readerr P((const char*const file)),
 verboff P((void)),
 verbon P((void)),
 newid P((void)),
 zombiecollect P((void)),
 yell P((const char*const a,const char*const b)),
 nlog P((const char*const a)),
 logqnl P((const char*const a)),
 skipped P((const char*const x)),
 onguard P((void)),
 offguard P((void)),
 Terminate P((void)) __attribute__((noreturn)),
 suspend P((void)),
 *app_val_ P((struct dyna_array*const sp,int size)),
 setupsigs P((void));
int
 forkerr Q((const pid_t pid,const char*const a)),
 buildpath P((const char*name,const char*const path,const char*const file)),
 nextrcfile P((void)),
 enoughprivs Q((const auth_identity*const passinvk,const uid_t euid,
  const gid_t egid,const uid_t uid,const gid_t gid)),
 conditions P((char flags[],const int prevcond,const int lastsucc,
  const int lastcond,const int skipping,int nrcond));
char
 *tstrdup P((const char*const a)),
 *cstr P((char*const a,const char*const b)),
 *egrepin P((char*expr,const char*source,const long len,int casesens));
const char
 *newdynstring P((struct dynstring**const adrp,const char*const chp));

extern int fakedelivery;
