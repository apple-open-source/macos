/* SFDecls.h */

/* We don't rely on compiler doing the right thing
 * in absence of declarations. 
 * C compilers should never have accepted this braindamage.
 * KG <garloff@suse.de>, 2002-01-28
 */

/* Dir.c */
int SFcompareEntries(const void *vp, const void *vq);
int SFgetDir(SFDir *dir);

/* Path.c */
int SFchdir (char *path);
void SFupdatePath(void);
void SFsetText(char *path);
//void SFbuttonPressList(Widget w, int n, XButtonPressedEvent *event);
//void SFbuttonReleaseList(Widget w, int n, XButtonPressedEvent *event)
//void SFdirModTimer(XtPointer cl, XtIntervalId *id);
//char SFstatChar (struct stat *statBuf);

/* SelFile.c */
//void SFpositionWidget(Widget w);
FILE * SFopenFile(char *name, char *mode, char *prompt, char *failed);
void SFtextChanged(void);
//int XsraSelFile(Widget toplevel, char *prompt, char *ok, char *cancel, char *failed,
//	    char *init_path, char *mode, int (*show_entry)(), char *name_return, int name_size);


/* Draw.c */
void SFinitFont(void);
void SFcreateGC(void);
void SFclearList(long n, int doScroll);
void SFdrawList(long n, int doScroll);
void SFdrawLists(int doScroll);
//void SFenterList(Widget w, register int n, register XEnterWindowEvent *event);
//void SFleaveList(Widget w, register int n, XEvent *event);
//void SFmotionList(Widget w, register int n, XMotionEvent *event);
// ...



