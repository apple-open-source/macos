/*$Id: exopen.h,v 1.1.1.2 2001/07/20 19:38:15 bbraun Exp $*/

int
 unique Q((const char*const full,char*p,const size_t len,const mode_t mode,
  const int verbos,const int flags)),
 myrename P((const char*const old,const char*const newn)),
 rlink P((const char*const old,const char*const newn,struct stat*st)),
 hlink P((const char*const old,const char*const newn));

#define UNIQnamelen	30	 /* require how much space as a first guess? */
#define MINnamelen	14		      /* cut to this on ENAMETOOLONG */

#define doCHOWN		1
#define doCHECK		2
#define doLOCK		4
#define doFD		8
#define doMAILDIR	16
