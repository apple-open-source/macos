/**********************************************************************

  main.c -

  $Author: ko1 $
  created at: Fri Aug 19 13:19:58 JST 1994

  Copyright (C) 1993-2007 Yukihiro Matsumoto

**********************************************************************/

#undef RUBY_EXPORT
#include "ruby.h"
#include "vm_debug.h"
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef RUBY_DEBUG_ENV
#include <stdlib.h>
#endif

#ifdef __APPLE__
#include "version.h"
#include <msgtracer_client.h>
#include <msgtracer_keys.h>
#include <libproc.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
/* ruby defines its own snprintf that does not handle snprintf(NULL, 0, ...) */
#undef snprintf

const char *pswhitelist[] = {
"/bin/",
"/sbin/",
"/usr/bin/",
"/usr/sbin/",
"/usr/libexec/",
"/System/",
"/Applications/App Store.app",
"/Applications/Apple Directory.app",
"/Applications/AppleConnect.app",
"/Applications/Automator.app",
"/Applications/Calculator.app",
"/Applications/Calendar.app",
"/Applications/Chess.app",
"/Applications/Contacts.app",
"/Applications/Dashboard.app",
"/Applications/Dictionary.app",
"/Applications/Display Menu.app",
"/Applications/DVD Player.app",
"/Applications/eApproval.app",
"/Applications/FaceTime.app",
"/Applications/Font Book.app",
"/Applications/GarageBand.app",
"/Applications/iBooks.app",
"/Applications/Image Capture.app",
"/Applications/iMovie.app",
"/Applications/iPhoto.app",
"/Applications/iTunes.app",
"/Applications/Keynote.app",
"/Applications/Launchpad.app",
"/Applications/Mail.app",
"/Applications/Maps.app",
"/Applications/Messages.app",
"/Applications/Mission Control.app",
"/Applications/NFA.app",
"/Applications/Notes.app",
"/Applications/Numbers.app",
"/Applications/Pages.app",
"/Applications/Photo Booth.app",
"/Applications/Photos.app",
"/Applications/Preview.app",
"/Applications/QuickTime Player.app",
"/Applications/Reminders.app",
"/Applications/Safari.app",
"/Applications/Siri.app",
"/Applications/Stickies.app",
"/Applications/System Preferences.app",
"/Applications/TextEdit.app",
"/Applications/Time Machine.app",
"/Applications/Xcode.app",
"/Applications/Utilities/Activity Monitor.app",
"/Applications/Utilities/AirPort Utility.app",
"/Applications/Utilities/Audio MIDI Setup.app",
"/Applications/Utilities/Bluetooth File Exchange.app",
"/Applications/Utilities/Boot Camp Assistant.app",
"/Applications/Utilities/ColorSync Utility.app",
"/Applications/Utilities/Console.app",
"/Applications/Utilities/Digital Color Meter.app",
"/Applications/Utilities/Disk Utility.app",
"/Applications/Utilities/Grab.app",
"/Applications/Utilities/Grapher.app",
"/Applications/Utilities/Keychain Access.app",
"/Applications/Utilities/Migration Assistant.app",
"/Applications/Utilities/Script Editor.app",
"/Applications/Utilities/System Information.app",
"/Applications/Utilities/Terminal.app",
"/Applications/Utilities/VoiceOver Utility.app",
NULL
};

void
mt_log_BSDServices_ScriptingLanguageUse(const char *signature)
{
    char name[1024];
    struct proc_bsdshortinfo pinfo;
    char pidpath[PROC_PIDPATHINFO_MAXSIZE];
    pid_t p = getppid();
    char *pstree = NULL;
    size_t oldsize = 0;
    while(proc_pidinfo(p, PROC_PIDT_SHORTBSDINFO, 0, &pinfo, sizeof(pinfo)) != -1) {
        struct stat sb;
        char found = 0;
        if(stat("/Library/Caches/com.apple.DiagnosticReporting.HasBeenAppleInternal", &sb) == 0) {
            found = 1;
        } else {
            if(proc_pidpath(p, pidpath, sizeof(pidpath)) == -1) {
                break;
            }
            for(int i = 0; pswhitelist[i]; ++i) {
                if(strncmp(pidpath, pswhitelist[i], strlen(pswhitelist[i])-1) == 0) {
                    found = 1;
                    break;
                }
            }
        }
        if(!found) {
            strlcpy(name, "[redacted]", sizeof(name));
        } else {
            if(proc_name(pinfo.pbsi_pid, name, (uint32_t)sizeof(name)) == -1) {
                break;
            }
        }
        int newsize = snprintf(NULL, 0, "%s %s", pstree, name);
        if ((newsize > 0) && (newsize < INT_MAX)) {
            if (oldsize > newsize) newsize = oldsize;
            pstree = realloc(pstree, newsize+1);
            if (pstree) {
                memset(pstree + oldsize, 0, (newsize+1) - oldsize);
                strlcat(pstree, " ", newsize+1);
                strlcat(pstree, name, newsize+1);
                oldsize = newsize+1;
            }
        }
        p = pinfo.pbsi_ppid;
	if (p <= 1) break;
    }

    aslmsg m = asl_new(ASL_TYPE_MSG);
    asl_set(m, "com.apple.message.domain", "com.apple.BSDServices.ScriptingLanguageUse" );
    asl_set(m, "com.apple.message.signature", signature);
    asl_set(m, "com.apple.message.summarize", "YES");
    asl_set(m, ASL_KEY_LEVEL, ASL_STRING_NOTICE);
    asl_set(m, ASL_KEY_MSG, "");
    asl_send(NULL, m);
    asl_free(m);

    aslmsg mv2 = asl_new(ASL_TYPE_MSG);
    asl_set(mv2, "com.apple.message.domain", "com.apple.BSDServices.ScriptingLanguageUse_v2" );
    asl_set(mv2, "com.apple.message.signature", signature);
    asl_set(mv2, "com.apple.message.signature2", RUBY_VERSION);
    asl_set(mv2, "com.apple.message.signature3", pstree);
    asl_set(mv2, "com.apple.message.summarize", "YES");
    asl_set(mv2, ASL_KEY_LEVEL, ASL_STRING_NOTICE);
    asl_set(mv2, ASL_KEY_MSG, "");
    asl_send(NULL, mv2);
    asl_free(mv2);

    free(pstree);
}
#endif

int
main(int argc, char **argv)
{
#ifdef RUBY_DEBUG_ENV
    ruby_set_debug_option(getenv("RUBY_DEBUG"));
#endif
#ifdef HAVE_LOCALE_H
    setlocale(LC_CTYPE, "");
#endif

#ifdef __APPLE__
    mt_log_BSDServices_ScriptingLanguageUse("ruby");
#endif

    ruby_sysinit(&argc, &argv);
    {
	RUBY_INIT_STACK;
	ruby_init();
	return ruby_run_node(ruby_options(argc, argv));
    }
}
