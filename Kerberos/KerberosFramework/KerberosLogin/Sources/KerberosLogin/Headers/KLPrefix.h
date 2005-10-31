/*
 * KLPrefix.h
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLogin/Headers/KLPrefix.h,v 1.9 2005/03/22 18:31:49 lxs Exp $
 *
 * Copyright 2003 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

// Krb4 # defines which are no longer provided by krb.h
#define KRB_REMOTE_COMMAND_TICKET "rcmd"

// Define so we get prototypes for KLSetApplicationOptions and KLGetApplicationOptions
#define KERBEROSLOGIN_DEPRECATED

#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <ApplicationServices/ApplicationServices.h>

#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <mach-o/dyld.h>
#include <nameser.h>
#include <resolv.h>
#include <pthread.h>

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#include <Kerberos/KerberosDebug.h>
#include <Kerberos/LoginSessions.h>
#include <Kerberos/mach_client_utilities.h>

#include <Kerberos/com_err.h>
#include <Kerberos/profile.h>
#include <Kerberos/des.h>
#include <Kerberos/krb.h>
#include "krb4int.h"  // must include before krb5.h to avoid warnings
#include <Kerberos/krb5.h>
#include <Kerberos/CredentialsCache.h>

#include <Kerberos/KerberosLogin.h>
#include <Kerberos/KerberosLoginPrivate.h>
#include <Kerberos/KLLoginLogoutNotification.h>
#include <Kerberos/KLPrincipalTranslation.h>

#include "KLCCacheManagement.h"
#include "KLChangePassword.h"
#include "KLConfiguration.h"
#include "KLEnvironment.h"
#include "KLGraphicalUI.h"
#include "KLLockFile.h"
#include "KLLoginLogoutNotifier.h"
#include "KLLoginOptions.h"
#include "KLPreferences.h"
#include "KLPrincipal.h"
#include "KLPrincipalTranslator.h"
#include "KLString.h"
#include "KLTerminalUI.h"
#include "KLTicketManagement.h"
#include "KLUserInterface.h"

