
#define ft_NOTYET	(-3)		     /* spool file doesn't exist yet */
#define ft_CANTCREATE	(-2)	/* wrong file type and can't change our mind */
#define ft_TOOLONG	(-1)		    /* path + UNIQnamelen > linebuf? */
#define ft_PIPE		  0		    /* program, stdout, or /dev/null */
#define ft_MAILDIR	  1				   /* maildir folder */
#define ft_MH		  2					/* MH folder */
#define ft_FILE		  3					/* real file */
#define ft_DIR		  4			     /* msg.inode# directory */

#define ft_lock(type)	   ((type)>ft_MAILDIR)		   /* kernel lock fd */
#define ft_atime(type)	   ((type)==ft_FILE)	      /* force atime < mtime */
#define ft_dotlock(type)   ((type)==ft_FILE)		 /* dotlock $DEFAULT */
#define ft_delim(type)	   ((type)==ft_FILE)		   /* add MMDF delim */
#define ft_checkcloser(type) ((type)>ft_MH)
#define ft_forceblank(type) ((type)!=ft_MAILDIR)  /* force blank line at end */

int
 foldertype Q((int type,int forcedir,mode_t*const modep,
  struct stat*const paranoid)),
 screenmailbox Q((char*chp,const gid_t egid,const int Deliverymode));

extern const char maildirnew[];
extern int accspooldir;

#ifdef TESTING
static const char*FT2str[]=
{ "Not-Yet","Can't-Create","Too-Long",
  "Pipe","Maildir","MH","File","Directory"
};
#define ft2str	(FT2str-ft_NOTYET)
#endif
