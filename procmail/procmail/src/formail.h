/*$Id: formail.h,v 1.1 1999/09/23 17:30:07 wsanchez Exp $*/

#define Bsize		128

#define FORMAILN	"formail"
#define HEAD_DELIMITER	':'

#define Re		(re+1)
#define putssn(a,l)	tputssn(a,(size_t)(l))
#define putcs(a)	(errout=putc(a,mystdout))
#define lputssn(a,l)	ltputssn(a,(size_t)(l))
#define PRDO		poutfd[0]
#define PWRO		poutfd[1]
#define FLD_HEADSIZ	((size_t)offsetof(struct field,fld_text[0]))

struct saved {const char*const headr;const int lenr;int rexl;char*rexp;};

extern const char binsh[],sfolder[],couldntw[],formailn[];
extern char ffileno[];
extern int errout,oldstdout,quiet,zap,buflast,lenfileno;
extern long initfileno;
extern pid_t child;
extern int childlimit;
extern unsigned long rhash;
extern FILE*mystdout;
extern int nrskip,nrtotal,retval;
extern size_t buflen,buffilled;
extern long Totallen;
extern char*buf,*logsummary;

extern struct field
 { size_t id_len;
   union {size_t uTot_len;struct field**ufld_ref;} len_fld;
   struct field*fld_next;
   char fld_text[255];
 }*rdheader,*xheader,*Xheader,*uheader,*Uheader;

#define Tot_len len_fld.uTot_len
#define fld_ref len_fld.ufld_ref

int
 eqFrom_ P((const char*const a)),
 breakfield Q((const char*const line,size_t len));
