/* $XFree86$ */

extern int XEEnableCtrlKeys(void(*rtn)(int));
extern int XEEnableCtrlC(void (*rtn)(int));
extern int XEEnableCtrlY(void (*rtn)(int));
extern int XEClearCtrlKeys(void);
extern int XEDeclExitHndlr(void (*rtn)(int));

