/* pam_wheel module */

/*
 * Written by Cristian Gafton <gafton@redhat.com> 1996/09/10
 * See the end of the file for Copyright Information
 *
 *
 * 1.2 - added 'deny' and 'group=' options
 * 1.1 - added 'trust' option
 * 1.0 - the code is working for at least another person, so... :-)
 * 0.1 - use vsyslog instead of vfprintf/syslog in _pam_log
 *     - return PAM_IGNORE on success (take care of sloppy sysadmins..)
 *     - use pam_get_user instead of pam_get_item(...,PAM_USER,...)
 *     - a new arg use_uid to auth the current uid instead of the
 *       initial (logged in) one.
 * 0.0 - first release
 *
 * TODO:
 *  - try to use make_remark from pam_unix/support.c
 *  - consider returning on failure PAM_FAIL_NOW if the user is not
 *    a wheel member.
 */

#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/param.h>
#include <pwd.h>
#include <grp.h>

/*
 * here, we make a definition for the externally accessible function
 * in this file (this definition is required for static a module
 * but strongly encouraged generally) it is used to instruct the
 * modules include file to define the function prototypes.
 */

#define PAM_SM_AUTH

#include <pam/pam_modules.h>

#define MAX_GROUPS 5

/* some syslogging */

static void _pam_log(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("PAM-Wheel", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}

/* argument parsing */

#define PAM_DEBUG_ARG       0x0001
#define PAM_USE_UID_ARG     0x0002
#define PAM_TRUST_ARG       0x0004
#define PAM_DENY_ARG        0x0010  

static int _pam_parse(int argc, const char **argv, char **use_group,
		      int *num_groups)
{
     int ctrl=0;

     /* step through arguments */
     for (ctrl=0; argc-- > 0; ++argv) {

          /* generic options */

          if (!strcmp(*argv,"debug"))
               ctrl |= PAM_DEBUG_ARG;
          else if (!strcmp(*argv,"use_uid"))
               ctrl |= PAM_USE_UID_ARG;
          else if (!strcmp(*argv,"trust"))
               ctrl |= PAM_TRUST_ARG;
          else if (!strcmp(*argv,"deny"))
               ctrl |= PAM_DENY_ARG;
          else if (!strncmp(*argv,"group=",6)) {
               int glen = strlen(*argv);
               if( *num_groups > MAX_GROUPS )
                   continue;
               use_group[*num_groups] = (char *)malloc(glen+1);
               if( use_group[*num_groups] == NULL )
                   continue;
               memset(use_group[*num_groups], 0, glen+1);
	       strncpy(use_group[*num_groups],*argv+6,glen);
               (*num_groups)++;
          } else {
               _pam_log(LOG_ERR,"pam_parse: unknown option; %s",*argv);
          }
     }

     return ctrl;
}


/* --- authentication management functions (only) --- */

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh,int flags,int argc
			,const char **argv)
{
     int ctrl;
     const char *username;
     char *fromsu;
     struct passwd *pwd, *tpwd;
     struct group *grp;
     int retval = PAM_AUTH_ERR;
     char **use_group;
     int num_groups = 0, ggnum;
     int i, r = 0, n;
     gid_t groups[NGROUPS];
    
     /* Init the optional group */
     use_group = malloc(MAX_GROUPS * sizeof(char *));
     if( use_group == NULL )
         return PAM_SERVICE_ERR;
     memset(use_group, 0, MAX_GROUPS * sizeof(char *));
     
     ctrl = _pam_parse(argc, argv, use_group, &num_groups);
     retval = pam_get_user(pamh, &username, NULL);
     if ((retval != PAM_SUCCESS) || (!username)) {
        if (ctrl & PAM_DEBUG_ARG)
            _pam_log(LOG_DEBUG,"can not get the username");
        return PAM_SERVICE_ERR;
     }

     /* su to a uid 0 account ? */
     pwd = getpwnam(username);
     if (!pwd) {
        if (ctrl & PAM_DEBUG_ARG)
            _pam_log(LOG_NOTICE,"unknown user %s",username);
        return PAM_USER_UNKNOWN;
     }
     
     /* Now we know that the username exists, pass on to other modules...
      * the call to pam_get_user made this obsolete, so is commented out
      *
      * pam_set_item(pamh,PAM_USER,(const void *)username);
      */

     /* is this user an UID 0 account ? */
     if(pwd->pw_uid) {
        /* no need to check for wheel */
        return PAM_IGNORE;
     }
     
     if (ctrl & PAM_USE_UID_ARG) {
         tpwd = getpwuid(getuid());
         if (!tpwd) {
            if (ctrl & PAM_DEBUG_ARG)
                _pam_log(LOG_NOTICE,"who is running me ?!");
            return PAM_SERVICE_ERR;
         }
         fromsu = tpwd->pw_name;
     } else {
         fromsu = getlogin();
         if (!fromsu) {
             if (ctrl & PAM_DEBUG_ARG)
                _pam_log(LOG_NOTICE,"who is running me ?!");
             return PAM_SERVICE_ERR;
         }
     }
     
     if (num_groups == 0 )
         use_group[num_groups++] = strdup("wheel");

     ggnum = getgroups(NGROUPS, groups);
     if( ggnum == NULL ) {
        return PAM_SERVICE_ERR;
     }

     /* Loop through the groups specified.  r counts the number of groups
      * the user is a member of.
      */
     for( i = 0; i < num_groups; i++ ) {
         grp = getgrnam(use_group[i]);

         if (!grp) {
            if (ctrl & PAM_DEBUG_ARG) {
                    _pam_log(LOG_NOTICE,"no members in '%s' group",use_group[i]);
            }
            continue;
         }
         for( n = 0; n < ggnum; n++ ) {
            if( grp->gr_gid == groups[n] )
               r++;
         }
     }

     if(r == 0) {
          if( ctrl & PAM_DENY_ARG )
              retval = PAM_IGNORE;
          else
              retval = PAM_PERM_DENIED;
     } else {
          if( ctrl & PAM_DENY_ARG )
              retval = PAM_PERM_DENIED;
          else if( ctrl & PAM_TRUST_ARG )
              retval = PAM_SUCCESS;
          else
              retval = PAM_IGNORE;
     }

     for( i = 0; i < num_groups; i++ ) {
         free(use_group[num_groups]);
     }
     free(use_group);

     if( ctrl & PAM_DEBUG_ARG ) {
         if( retval == PAM_PERM_DENIED ) {
             _pam_log(LOG_NOTICE,"Access denied for '%s' to '%s'", fromsu,
             username);
         } else {
             _pam_log(LOG_NOTICE,"Access granted for '%s' to '%s'", fromsu,
             username);
         }
     }
     return retval;   
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh,int flags,int argc
		   ,const char **argv)
{
     return PAM_SUCCESS;
}


#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_wheel_modstruct = {
     "pam_wheel",
     pam_sm_authenticate,
     pam_sm_setcred,
     NULL,
     NULL,
     NULL,
     NULL,
};

#endif

/*
 * Copyright (c) Cristian Gafton <gafton@redhat.com>, 1996, 1997
 *                                              All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED `AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
