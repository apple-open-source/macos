/* $Xorg: globals.c,v 1.4 2001/02/09 02:05:59 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xsm/globals.c,v 1.6 2001/12/14 20:02:25 dawes Exp $ */

#include <X11/ICE/ICEutil.h>

int		Argc;
char		**Argv;

List		*RunningList;
List		*PendingList;
List		*RestartAnywayList;
List		*RestartImmedList;

List		*WaitForSaveDoneList;
List		*InitialSaveList;
List		*FailedSaveList;
List		*WaitForInteractList;
List		*WaitForPhase2List;

Bool		wantShutdown = False;
Bool		shutdownInProgress = False;
Bool		phase2InProgress = False;
Bool		saveInProgress = False;
Bool		shutdownCancelled = False;

Bool		verbose = False;

char		*sm_id = NULL;

char		*networkIds = NULL;
char		*session_name = NULL;

IceAuthDataEntry *authDataEntries = NULL;
int		numTransports = 0;

Bool		client_info_visible = False;
Bool		client_prop_visible = False;
Bool		client_log_visible = False;

String 		*clientListNames = NULL;
ClientRec	**clientListRecs = NULL;
int		numClientListNames = 0;

int		current_client_selected;

int		sessionNameCount = 0;
String		*sessionNamesShort = NULL;
String		*sessionNamesLong = NULL;
Bool		*sessionsLocked = NULL;

int		num_clients_in_last_session = -1;

char		**non_session_aware_clients = NULL;
int		non_session_aware_count = 0;

char		*display_env = NULL, *non_local_display_env = NULL;
char		*session_env = NULL, *non_local_session_env = NULL;
char		*audio_env = NULL;

Bool		need_to_name_session = False;

Bool		remote_allowed;

XtAppContext	appContext;
Widget		topLevel;

XtSignalId	sig_term_id, sig_usr1_id;
