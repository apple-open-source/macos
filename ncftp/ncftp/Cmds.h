/* Cmds.h */

#ifndef _cmds_h_
#define _cmds_h_

#define SETASCII SetType('A')
#define SETBINARY SetType('I')

/* Command result codes. */
#define kUsageErr (88)
#define kNoErr (0)
#define kCmdErr (-1)

#define UNIMPLEMENTED_CMD(a) ((a == 500) || (a == 502) || (a == 504))

/* Possible values returned by GetDateAndTime. */
#define kSizeUnknown (-1L)
#define kModTimeUnknown ((time_t) 0)

/* GetRemoteCWD uses this in case it couldn't PWD. */
#define kDidNotChdir ((char *) 0)

int DoSystem(char *, size_t);
int LocalPageCmd(int, char **);
int LocalPwdCmd(void);
int PwdCmd(void);
int DoMdtm(char *, time_t *);
int DoSize(char *, long *);
int TryQuietChdir(char *);
int DoChdir(char *);
int ChdirCmd(int, char **);
int DoLocalChdir(char *, int);
int LocalChdirCmd(int, char **);
int DebugCmd(int, char **);
int VerboseCmd(int, char **);
int SetMode(int);
int SetType(int);
void DoType(char *);
int TypeCmd(int, char **);
int ModeCmd(int, char **);
void DoQuit(int);
int QuitCmd(void);
int HelpCmd(int, char **);
int VersionCmd(void);
int GenericGlobCmd(int, char **, char *, int);
int GenericCmd(int, char **, char *, int);
int DeleteCmd(int, char **);
int RmdirCmd(int, char **);
int MkdirCmd(int, char **);
int RenameCmd(int, char **);
int QuoteCmd(int, char **);
int ClearCmd(void);
int RmtHelpCmd(int, char **);
int ShellCmd(int, char **);
int EchoCmd(int, char **);
int LookupCmd(int, char **);
int BookmarkCmd(int, char **);
#endif	/* _cmds_h_ */

