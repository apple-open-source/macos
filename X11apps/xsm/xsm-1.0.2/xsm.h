/* $Xorg: xsm.h,v 1.4 2001/02/09 02:06:01 xorgcvs Exp $ */
/* $XdotOrg: $ */
/******************************************************************************

Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
******************************************************************************/
/* $XFree86: xc/programs/xsm/xsm.h,v 1.7 2001/12/08 18:33:45 herrb Exp $ */

#ifndef _XSM_H_
#define _XSM_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <X11/Xos.h>
#include <X11/Xfuncs.h>

#ifndef X_NOT_POSIX
#ifdef _POSIX_SOURCE
#include <limits.h>
#else
#define _POSIX_SOURCE
#include <limits.h>
#undef _POSIX_SOURCE
#endif
#endif /* X_NOT_POSIX */
#ifndef PATH_MAX
#include <sys/param.h>
#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 1024
#endif
#endif
#endif /* PATH_MAX */

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#include <stdio.h>
#undef _POSIX_SOURCE
#else
#include <stdio.h>
#endif

#include <ctype.h>
#include <stdlib.h>

#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>

#include <X11/ICE/ICEutil.h>

#include <X11/SM/SMlib.h>

#include "list.h"

/*
 * Each time the format of the sm's save file changes, bump up
 * the version.
 */

#define SAVEFILE_VERSION 3

#define DEFAULT_SESSION_NAME "Default"
#define FAILSAFE_SESSION_NAME "Fail Safe"

#define RESTART_MANAGERS 	1
#define RESTART_REST_OF_CLIENTS	2

typedef struct _ClientRec {
    SmsConn	 	smsConn;
    IceConn		ice_conn;
    char 		*clientId;
    char		*clientHostname;
    List		*props;
    char		*discardCommand;
    char		*saveDiscardCommand;

    unsigned int	restarted : 1;
    unsigned int	userIssuedCheckpoint : 1;
    unsigned int	restartHint : 2;
    unsigned int        receivedDiscardCommand : 1;
    unsigned int	freeAfterBadSavePopup : 1;

} ClientRec;

typedef struct _PendingClient {
    char		*clientId;
    char		*clientHostname;
    List		*props;
} PendingClient;

typedef struct _Prop {
    char		*name;
    char		*type;
    List		*values;
} Prop;

typedef struct _PropValue {
    XtPointer		value;
    int			length;
} PropValue;


extern char		**environ;

extern int		Argc;
extern char		**Argv;

extern char		*display_env, *non_local_display_env;
extern char		*session_env, *non_local_session_env;
extern char		*audio_env;

extern Bool		need_to_name_session;

extern Bool		remote_allowed;

extern Bool		verbose;

extern char		*sm_id;

extern char		*networkIds;
extern char		*session_name;

extern IceAuthDataEntry *authDataEntries;
extern int		numTransports;

extern List		*RunningList;
extern List		*PendingList;
extern List		*RestartAnywayList;
extern List		*RestartImmedList;

extern List		*WaitForSaveDoneList;
extern List		*FailedSaveList;
extern List		*WaitForInteractList;
extern List		*WaitForPhase2List;

extern Bool		client_info_visible;
extern Bool		client_prop_visible;
extern Bool		client_log_visible;
extern String 		*clientListNames;
extern ClientRec	**clientListRecs;
extern int		numClientListNames;
extern int		current_client_selected;

extern Bool		shutdownInProgress;
extern Bool		phase2InProgress;
extern Bool	        saveInProgress;
extern Bool		shutdownCancelled;
extern Bool		wantShutdown;

extern int		sessionNameCount;
extern String		*sessionNamesShort;
extern String		*sessionNamesLong;
extern Bool		*sessionsLocked;

extern int		num_clients_in_last_session;

extern char		**non_session_aware_clients;
extern int		non_session_aware_count;

extern XtAppContext	appContext;
extern Widget		topLevel;
extern Widget		mainWindow;
extern Widget		clientInfoButton;
extern Widget		logButton;
extern Widget		checkPointButton;
extern Widget		shutdownButton;
extern Widget		shutdownDontSave;

extern XtSignalId	sig_term_id, sig_usr1_id;

extern void fprintfhex(FILE *fp, unsigned int len, char *cp);
extern Status StartSession(char *name, Bool use_default);
extern void EndSession(int status);
extern void SetWM_DELETE_WINDOW(Widget widget, String delAction);
extern void SetAllSensitive(Bool on);
extern void FreeClient(ClientRec *client, Bool freeProps);
extern void CloseDownClient(ClientRec *client);


/* misc.c */
extern int strbw(char *a, char *b);
extern void nomem(void);


#define Strstr strstr

/* Fix ISC brain damage.  When using gcc fdopen isn't declared in <stdio.h>. */
#if defined(ISC) && __STDC__
extern FILE *fdopen(int, char const *);
#endif

#if defined(sun) && defined(SVR4)
extern int System();
#define system(s) System(s)
#endif

/* remote.c */
extern void remote_start(char *restart_protocol, char *restart_machine, 
			 char *program, char **args, char *cwd, char **env, 
			 char *non_local_display_env, 
			 char *non_local_session_env );

/* signals.c */
extern void sig_child_handler(int sig);
extern void sig_term_handler(int sig);
extern void sig_usr1_handler(int sig);
extern void xt_sig_term_handler(XtPointer closure, XtSignalId *id);
extern void xt_sig_usr1_handler(XtPointer closure, XtSignalId *id);
extern void register_signals(XtAppContext);
extern int execute_system_command(char *s);

extern int checkpoint_from_signal;

#ifdef XKB
#include <X11/extensions/XKBbells.h>
#endif

#endif /* _XSM_H_ */
