/************************************************************************
 *	recommend	Analyses the installation, and makes		*
 *			recommendations about suid/sgid modes		*
 ************************************************************************/
/*$Id: recommend.c,v 1.1.1.1 1999/09/23 17:30:07 wsanchez Exp $*/
#include "includes.h"

#ifndef SYSTEM_MBOX
#define SYSTEM_MBOX	SYSTEM_MAILBOX
#endif

#define PERMIS	(S_IRWXU|S_IRWXG&~S_IWGRP|S_IRWXO&~S_IWOTH)

char mailspooldir[]=MAILSPOOLDIR;
const char dirsep[]=DIRSEP,
 *const checkf[]={"/bin/mail","/bin/lmail","/usr/lib/sendmail",
 "/usr/lib/smail",0};

main(argc,argv)const int argc;const char*const argv[];
{ struct group*grp;struct stat stbuf;gid_t gid=(gid_t)-1;
  const char*const*p;mode_t sgid=0;int chmdir=0;
  if(argc!=3)
   { fprintf(stderr,"Please run this program via 'make recommend'\n");
     return EX_USAGE;
   }
  strchr(mailspooldir,'\0')[-1]='\0';		     /* strip last character */
  for(p=checkf;*p;p++)
     if(!stat(*p,&stbuf)&&stbuf.st_mode&S_ISGID)
      { if(stbuf.st_mode&S_ISGID)
	   sgid=S_ISGID,gid=stbuf.st_gid;
	break;
      }
  if(!stat(mailspooldir,&stbuf)&&!(stbuf.st_mode&S_IWOTH))
     if(stbuf.st_mode&S_ISVTX)
	chmdir=2;
     else
      { if(!(stbuf.st_mode&S_IWGRP))
	   chmdir=1;
	sgid=S_ISGID;gid=stbuf.st_gid;
      }
  if(gid!=stbuf.st_gid)
     sgid=0;
  printf("chown root %s\n",argv[1]);
  if(sgid)
     if(grp=getgrgid(gid))
	printf("chgrp %s %s %s\n",grp->gr_name,argv[1],argv[2]);
     else
	printf("chgrp %u %s %s\n",(unsigned)gid,argv[1],argv[2]);
  printf("chmod %o %s\n",sgid|S_ISUID|PERMIS,argv[1]);
  if(sgid)
     printf("chmod %o %s\n",sgid|PERMIS,argv[2]);
  else if(chmdir==1)
     goto nogchmod;
  if(chmdir)
     printf("chmod %c+w %s/.\n",chmdir==1?'g':'a',mailspooldir);
nogchmod:
  return EXIT_SUCCESS;
}
