/* $XFree86: xc/programs/xtrap/XEKeybCtrl.h,v 1.1 2001/11/02 23:29:34 dawes Exp $ */

extern int XEEnableCtrlKeys(void(*rtn)(int));
extern int XEEnableCtrlC(void (*rtn)(int));
extern int XEEnableCtrlY(void (*rtn)(int));
extern int XEClearCtrlKeys(void);
extern int XEDeclExitHndlr(void (*rtn)(int));

