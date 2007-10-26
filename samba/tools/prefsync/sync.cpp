/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include "smb_server_prefs.h"
#include "macros.hpp"
#include "lib/common.hpp"
#include "lib/SmbConfig.hpp"
#include "lib/SmbOption.hpp"

#include <cstdlib>
#include <cerrno>
#include <iostream>
#include <memory> /* std::auto_ptr */
#include <sys/stat.h>
#include <poll.h>
#include <signal.h>
#include <sys/event.h>
#include <fcntl.h>

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

#define DEF_SIGNATURE "Defaults signature"
#define PREF_SIGNATURE "Preferences signature"
#define RULES_SIGNATURE "Configuration rules"

/* Poll time for linger mode is 200 msec */
#define LINGER_INTERVAL_NSEC MSEC_TO_NSEC(200)

typedef int (*linger_callback)(void);

#ifdef TESTLEAKS
static void signal_recv(int s) { }
#endif

static void apply_preferences(SmbRules& rules, Preferences& prefs)
{
    SmbRules::iterator i;

    for (i = rules.begin(); i != rules.end(); ++i) {
	(*i)->reset(prefs);
    }
}

static void emit_options(SmbRules& rules, SmbConfig& smb)
{
    SmbRules::iterator i;

    for (i = rules.begin(); i != rules.end(); ++i) {
	(*i)->emit(smb);
    }
}

static void sync_service(LaunchService& svc, bool stale)
{
    bool enabled;

    if (Options::ForceSuspend) {
	bool ret = launchd_unload_job(svc);
	VERBOSE("suspending service %s (%s)\n",
		    svc.label().c_str(), ret ? "succeeded" : "failed");
	return;
    }

    /* Sample the launchd state once to guarantee consistent results. */
    enabled = svc.enabled();

    if (svc.required()) {
	/* Figure out if we need to start the service. */
	if (enabled) {
	    if (stale || Options::ForceRestart) {
		bool ret = launchd_stop_job(svc.label().c_str());
		VERBOSE("restarting service %s (%s)\n",
			svc.label().c_str(), ret ? "succeeded" : "failed");
		return;
	    }
	} else {
	    bool ret = launchd_load_job(svc);
	    VERBOSE("enabling service %s (%s)\n",
		    svc.label().c_str(), ret ? "succeeded" : "failed");
	    return;
	}
    } else {
	/* Figure out if we need to stop the service. */
	if (enabled) {
	    bool ret = launchd_unload_job(svc);
	    VERBOSE("disabling service %s (%s)\n",
		    svc.label().c_str(), ret ? "succeeded" : "failed");
	    return;
	}
    }

    VERBOSE("no change to service %s\n", svc.label().c_str());
}

static std::string data_string(CFDataRef data)
{
    std::ostringstream ostr;
    ostr << data;
    return ostr.str();
}

static bool is_config_stale(const SmbConfig& smb,
			    const char * tag,
			    const std::string& current)
{
    SmbConfig::param_type lastsig;

    lastsig.first = tag;
    return smb.meta_changed(lastsig);
}

static bool update_preferences(SmbRules& rules, SmbConfig& smb)
{
    bool stale = false;
    CFDataRef sig;
    Preferences defaults(is_server_system() ? kSMBPreferencesServerDefaults
					    : kSMBPreferencesDesktopDefaults);
    Preferences current(kSMBPreferencesAppID);

    sig = defaults.create_signature();
    smb.set_meta(make_smb_param(DEF_SIGNATURE, sig));
    stale |= is_config_stale(smb, DEF_SIGNATURE, data_string(sig));
    safe_release(sig);

    sig = current.create_signature();
    smb.set_meta(make_smb_param(PREF_SIGNATURE, sig));
    stale |= is_config_stale(smb, PREF_SIGNATURE, data_string(sig));
    safe_release(sig);

    smb.set_meta(make_smb_param(RULES_SIGNATURE, rules.version()));
    stale |= is_config_stale(smb, RULES_SIGNATURE, rules.version());

    VERBOSE("updating preferences from defaults\n");
    apply_preferences(rules, defaults);

    VERBOSE("updating preferences from current config\n");
    apply_preferences(rules, current);

    return stale;
}

/* Return 0 if there are unsynchronised changes, 2 otherwise. */
static int cmd_changes_pending(void)
{
    SmbConfig smb;
    SmbRules rules;
    bool stale;

    stale = update_preferences(rules, smb);
    VERBOSE("configuration is %s\n", stale ? "out of date" : "current");

    return stale ? EXIT_SUCCESS : 2;
}

/* Print the current settings in smb.conf format. */
static int cmd_list_pending(void)
{
    SmbConfig	smb;
    SmbRules	rules;

    update_preferences(rules, smb);
    emit_options(rules, smb);
    smb.format(std::cout);
    return EXIT_SUCCESS;
}

/* Print the defaults in smb.conf format. */
static int cmd_list_defaults(void)
{
    Preferences defaults(is_server_system() ? kSMBPreferencesServerDefaults
					    : kSMBPreferencesDesktopDefaults);
    SmbConfig	smb;
    SmbRules	rules;
    CFDataRef	sig;

    sig = defaults.create_signature();
    smb.set_meta(make_smb_param(DEF_SIGNATURE, sig));
    smb.set_meta(make_smb_param(RULES_SIGNATURE, rules.version()));

    apply_preferences(rules, defaults);
    emit_options(rules, smb);
    smb.format(std::cout);

    safe_release(sig);
    return EXIT_SUCCESS;
}

/* Synchronise SMB system state to the current preference values. */
static int cmd_sync_prefs(void)
{
    SmbConfig	smb;
    SmbRules	rules;
    bool	stale;

    /* We need to do this whole function under the exclusive preferences lock
     * to guarantee that we synchronise the system to a consisetnt state.
     */
    SyncMutex mutex;

    /* If we allow non-root to get much further very bad things can happen,
     * like loading launchd jobs into the calling user's session.
     */
    if (getuid() != 0) {
	VERBOSE("only root can synchronize SMB preferences\n");
	return 1;
    }

    try {
	stale = update_preferences(rules, smb);

	/* We always need to emit options to give them a chance to register
	 * their launchd requirements.
	 */
	emit_options(rules, smb);
	if (stale || Options::ForceSync) {
	    bool success = false;
	    VERBOSE("rewriting SMB configuration\n");

	    for (int tries = 5; tries; --tries) {
		if (smb.writeback()) {
		    success = true;
		    break;
		}
	    }

	    if (!success) {
		VERBOSE("failed to write new SMB configuration\n");
		return 1;
	    }
	}

	/* Verify launch job state */
	sync_service(smb.SmbdService(), stale);
	sync_service(smb.NmbdService(), stale);
	sync_service(smb.WinbindService(), stale);

	/* The "smb" service counts as running if either smbd or nmbd is
	 * enabled. This maintains ServerAdmin's idea of the SMB service
	 * state.
	 */
	if (smb.SmbdService().enabled() || smb.NmbdService().enabled()) {
	    post_service_notification("smb", "RUNNING");
	} else {
	    post_service_notification("smb", "STOPPED");
	}
    } catch (std::exception& e) {
	VERBOSE("exception during sync: %s\n", e.what());
	return 1;
    }

    return EXIT_SUCCESS;
}

static struct timespec * timeout_until_ts(long long end_time_usec,
					long long now_time_usec,
					struct timespec * timeout)
{
    unsigned long long timeout_usec = 0;

    /* Make sure that the timeout can't wrap. */
    if (end_time_usec > now_time_usec) {
	timeout_usec = end_time_usec - now_time_usec;
    }

    /* We don't care about timeout accuracy of less than a second. */
    timeout->tv_sec = USEC_TO_SEC(timeout_usec);
    timeout->tv_nsec = 0;

    /* Make sure we do actually have a timeout, and don't have something
     * stupid.
     */
    if (timeout->tv_sec == 0 || timeout->tv_sec > 1000) {
	timeout->tv_sec = 1;
    }

    return timeout;
}

/* This is not really kosher, but it happens to be the plist that
 * corresponds the SMB SCPreferences.
 */
#define SMB_PREFS_PARENT "/Library/Preferences/SystemConfiguration"
#define SMB_PREFS_PATH ( SMB_PREFS_PARENT "/" kSMBPreferencesAppID )

/* The set of vnode events that indicate that the content of the path
 * has changed.
 */
#define NEW_CONTENT_VNODE_EVENTS ( NOTE_DELETE | NOTE_WRITE | \
	NOTE_EXTEND | NOTE_RENAME | NOTE_REVOKE )

/* The set of vnode events that require us to obtain a new fd on the path
 * we are interested in.
 */
#define FD_STALE_VNODE_EVENTS (NOTE_DELETE | NOTE_RENAME | NOTE_REVOKE)

static void delete_kevent_watch(int kq, struct kevent *ev)
{
    ASSERT(ev->filter == EVFILT_VNODE);
    ev->flags = EV_DELETE;
    kevent(kq, ev, 1, NULL, 0, NULL);
    close(ev->ident);
    ev->ident = -1;
}

static int add_kevent_watch(int kq, struct kevent * ev, const char * path)
{
    int fd;
    int err;

    EV_SET(ev, -1 /* ident */, EVFILT_VNODE, EV_ADD | EV_CLEAR | EV_ENABLE,
	    NEW_CONTENT_VNODE_EVENTS, 0, NULL);

    if ((fd = open(path, O_EVTONLY|O_NOFOLLOW)) == -1) {
	return -1;
    }

    ev->ident = fd;
    fcntl(fd, F_SETFD, FD_CLOEXEC);

    err = kevent(kq, ev, 1, NULL, 0, NULL);
    if (err == -1) {
	int errsav = errno;
	close(ev->ident);
	ev->ident = -1;
	errno = errsav;
    }

    return err;
}

static int wait_for_path_until(int kq,
		    const char * dirpath,
		    const char * childpath,
		    unsigned long long end_time_usec)
{
    struct kevent ev;
    struct kevent change;
    struct timespec timeout;
    unsigned long long now_time_usec = time_now_usec();
    struct stat sbuf;
    int err;

    err = add_kevent_watch(kq, &ev, dirpath);
    if (err == -1) {
	VERBOSE("failed to watch parent %s: %s\n",
		dirpath, strerror(errno));
	return -1;
    }

    do {
	if (stat(childpath, &sbuf) != -1) {
	    err = 0;
	    goto done;
	}

	zero_struct(change);
	err = kevent(kq, NULL, 0, &change, 1,
		    timeout_until_ts(end_time_usec, now_time_usec, &timeout));

	if (err == -1) {
	    VERBOSE("failed to set up kqueue: %s\n", strerror(errno));
	    goto done;
	}

	if (err == 0) {
	    VERBOSE("timed out watching: %s\n", dirpath);
	    continue;
	}

	if (change.fflags & FD_STALE_VNODE_EVENTS) {
	    /* We are waiting for a file to appear within a directory,
	     * but something happened that might mean that the directory
	     * went away. Rather than attempting to handle this corner
	     * case, we should just exit ASAP so that launchd can get us
	     * back on track.
	     */
	    err = -1;
	    goto done;
	}

    } while ((now_time_usec = time_now_usec()) < end_time_usec);

done:
    /* Delete the parent watch before returning. */
    delete_kevent_watch(kq, &ev);
    return err;
}

/* NOTE: We should be using SCPreferencesSetCallback and a CFRunLoop to figure
 * out when the preference changes happen. Unfortunately, System Prefs writes
 * the plist directly so we have to resort to checking the mtime.
 */
static int cmd_linger_sync_prefs(linger_callback action,
				    unsigned idle_usec)
{
    /* This is not really kosher, but it happens to be the plist that
     * corresponds the SMB SCPreferences.
     */
    std::auto_ptr<Preferences> prefs(new Preferences (SMB_PREFS_PATH));
    unsigned long long end_time_usec;
    unsigned long long now_time_usec;
    CFDataRef last = NULL;
    CFDataRef current = NULL;
    int kq = -1;
    struct kevent ev;

    VERBOSE("lingering until %usec idle timeout\n",
	    USEC_TO_SEC(idle_usec));

    zero_struct(ev);

    if ((kq = kqueue()) == -1) {
	/*Linger mode requires kqueue, so if we don't have one we are toast. */
	VERBOSE("unable to create a kqueue: %s\n", strerror(errno));
	return action();
    } else {
	fcntl(kq, F_SETFD, FD_CLOEXEC);

	if (add_kevent_watch(kq, &ev, SMB_PREFS_PATH) == -1) {
	    VERBOSE("failed to watch %s: %s\n",
		    SMB_PREFS_PATH, strerror(errno));
	}
    }

    /* We were launched because someone wanted us to run the action,
     * so do it right away.
     */
    action();

    last = prefs->create_signature();

    /* Now drop into linger mode. Wait around until the current preferences
     * change and run the action again when they do. Remember, that is is safe
     * to run the action unnecessarily because only cmd_sync_prefs() has
     * side-effects, and that does it's own (more thorough) check for
     * staleness.
     */
    end_time_usec = time_now_usec() + idle_usec;
    while ((now_time_usec = time_now_usec()) < end_time_usec) {
	int err = 0;

	ASSERT(current == NULL);

	if (ev.ident == (uintptr_t)-1) {
	    if (add_kevent_watch(kq, &ev, SMB_PREFS_PATH) == -1) {

		/* If we couldn't add the watch because the path wasn't there,
		 * then there is a good chance that we are racing against an
		 * update/rename operation. We should longer on the parent path
		 * and see whether the file comes back.
		 */
		if (errno != ENOENT) {
		    VERBOSE("failed to watch %s: %s\n",
			    SMB_PREFS_PATH, strerror(errno));
		    return 1;
		}

		if (wait_for_path_until(kq, SMB_PREFS_PARENT, SMB_PREFS_PATH,
				    end_time_usec) == -1) {
		    /* If we can't wait on the parent, just give up and get
		     * back to launchd.
		     */
		    return 1;
		}

		/* Either we timed out, or the path we were waiting for
		 * appeared. In either case, the right thing to do is to
		 * attempt a new signature check and see whether we have to
		 * synchronise.
		 */
	    }

	}

	/* So, when we started the preferences file might not have been
	 * present, in which case we couldn't figure out what type of
	 * preferences we have and couldn't create a signature. At this point,
	 * we have a pretty reasonable idea that the preferences file is
	 * present, so we should reload.
	 */
	if (!prefs->is_loaded()) {
	    delete prefs.release();
	    prefs.reset(new Preferences(SMB_PREFS_PATH));
	}

	current = prefs->create_signature();
	if (ev.ident != (uintptr_t)-1 &&
	    cftype_equal<CFDataRef>(last, current)) {
	    /* Listen for changes if we have an active kqueue and no
	     * pending update.
	     */
	    struct kevent change = {0};
	    struct timespec timeout_ts;

	    timeout_until_ts(end_time_usec, now_time_usec, &timeout_ts);

	    VERBOSE("waiting for preferences update (%lusec timeout)\n",
		    (unsigned long)timeout_ts.tv_sec);

	    err = kevent(kq, NULL, 0, &change, 1, &timeout_ts);
	    if (err == -1) {
		/* If the watch failed, just get back to launchd ASAP. But we
		 * don't know whether we actually waited or not, so we first
		 * need to check our signatures to make sure we don't lose any
		 * events.
		 */
		VERBOSE("failed to watch kqueue: %s\n", strerror(errno));
		delete_kevent_watch(kq, &ev);
	    } else if (change.fflags & FD_STALE_VNODE_EVENTS) {
		/* Reopen fd if the file it refers to might have gone away. */
		VERBOSE("kevent fd %d gone stale (fflags=%#x), "
			"deleting kevent watch\n",
			(int)ev.ident, ev.fflags);

		delete_kevent_watch(kq, &ev);
	    }

	    safe_release(current);
	    current = prefs->create_signature();
	}

	if (cftype_equal<CFDataRef>(last, current)) {
	    VERBOSE("preferences signatures match, spurious linger wakeup\n");
	} else {

#if DEBUG
	    std::string last_str(data_string(last));
	    std::string current_str(data_string(current));

	    ASSERT(last_str != current_str);
	    DEBUGMSG("last    = %s\n", last_str.c_str());
	    DEBUGMSG("current = %s\n", current_str.c_str());

#endif /* DEBUG */

	    action();

	    /* Make the last signature current. We used to only do this if the
	     * command succeeded, but the exit status of the command doesn't
	     * have any strong relationship with whether any update succeeded
	     * or not.
	     *
	     * NOTE: if we failed to get a signature (eg. the file was
	     * missing), current and last can be NULL.
	     */
	    safe_release(last);
	    last = current;
	    safe_retain(last); /* because we CFRelease current below. */

	    VERBOSE("pushing idle timeout %lusec forward\n",
			(unsigned long)USEC_TO_SEC(idle_usec));
	    end_time_usec = time_now_usec() + idle_usec;
	}

	safe_release(current);

	/* kevent(2) failed - best to get back to launchd ASAP. */
	if (err == -1) {
	    return 1;
	}
    }

    safe_release(last);
    safe_release(current);

    /* Now that the idle time is expired, the goal is to get exited ASAP so
     * that launchd knows we are gone and can go back to watching the path for
     * us. We used to run the action one last time here, but that results in a
     * spurious service restart unless we check the signatures again.
     */
    return EXIT_SUCCESS;
}

int main(int argc, char * const * argv)
{
    unsigned	    idle_sec = 30;
    linger_callback action;

    setprogname("synchronize-preferences");
    umask(0);

    Options::parse(argc, argv);

#if DEBUG
    Options::Debug = true;
    std::cout << "WARNING: debug mode emabled\n";
#endif

    if (Options::Linger) {
	if (!launchd_checkin(&idle_sec)) {
	    VERBOSE("launchd checking failed: %s\n",
		    strerror(errno));
	} else {
	    VERBOSE("lingering for %u secs\n",
		    idle_sec);
	}
    }

#ifdef TESTLEAKS
    signal(SIGHUP, signal_recv);

    while (1) {
	cmd_changes_pending();
	cmd_list_pending();
	cmd_list_defaults();
	cmd_sync_prefs();

	printf("done\n");
	poll(NULL, 0, -1); /* wait for a signal */
    }
    return EXIT_SUCCESS;
#endif

    switch(Options::Command) {
    case Options::CHANGES_PENDING:
	action = cmd_changes_pending;
	break;
    case Options::LIST_PENDING:
	action = cmd_list_pending;
	break;
    case Options::LIST_DEFAULTS:
	action = cmd_list_defaults;
	break;
    default:
	ASSERT(Options::Command == Options::SYNC);
	action = cmd_sync_prefs;
    }

    return Options::Linger ?
		    cmd_linger_sync_prefs(action, SEC_TO_USEC(idle_sec))
			    : action();
}

/* vim: set cindent ts=8 sts=4 tw=79 : */
