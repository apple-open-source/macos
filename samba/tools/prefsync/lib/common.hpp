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

#include <string>
#include <vector>

class RegularExpression
{
public:
    static const int NOMATCH;
    typedef std::vector<std::string> match_list;

    RegularExpression() : m_preg(0), m_errstr(0) {}
    ~RegularExpression();

    int compile(const char * pattern);
    int match(const std::string& strval, unsigned count);

    const match_list& get_matches(void) const
    {
	return this->m_matchlist;
    }

    const char * errstring(int errcode);

private:

    RegularExpression(const RegularExpression&); // nocopy
    RegularExpression& operator=(const RegularExpression&); // nocopy

    void reset(void);

    void *	m_preg;
    char *	m_errstr;
    match_list	m_matchlist;
};

class LaunchJobStatus
{
public:
    LaunchJobStatus() : m_pid(-1), m_program(), m_label() {}

    pid_t	    m_pid;
    std::string	    m_program;
    std::string	    m_label;
};

/* Check in with launchd and get our config variables. */
bool launchd_checkin(unsigned * idle_timeout_secs);

/* Ask launchd for the status of the named job. */
bool launchd_job_status(const char * job, LaunchJobStatus& info);

/* Ask launchd to stop the named job. */
bool launchd_stop_job(const char * job);

class LaunchService;
bool launchd_unload_job(const LaunchService& svc);
bool launchd_load_job(const LaunchService& svc);

class LaunchService
{
public:
    LaunchService(const std::string& job_label, const std::string& job_plist)
	: m_joblabel(job_label), m_jobplist(job_plist), m_required(false)
    {}

    const std::string& label(void) const { return m_joblabel; }
    const std::string& plist(void) const { return m_jobplist; }

    bool required(bool yesno) { return (m_required = yesno); }
    bool required(void) const { return m_required; }

    bool enabled(LaunchJobStatus& info) const
    {
	return ::launchd_job_status(this->m_joblabel.c_str(), info);
    }

    bool enabled() const
    {
	LaunchJobStatus info;
	return ::launchd_job_status(this->m_joblabel.c_str(), info);
    }

    void stop() const;
    void start() const;
    void restart() const;

private:
    std::string	    m_joblabel;
    std::string	    m_jobplist;
    bool	    m_required;
};

class Options
{
public:

    typedef enum {
	SYNC, CHANGES_PENDING, LIST_PENDING, LIST_DEFAULTS
    } command_type;

    static bool Verbose;
    static bool Debug;
    static bool Linger;
    static bool ForceSync;
    static bool ForceSuspend;
    static bool ForceRestart;

    static command_type Command;

    static void parse(int argc, char * const * argv);
};

class Preferences
{
public:
    Preferences(const char * pspec);
    ~Preferences();

    bool is_loaded() const { return this->m_plist || this->m_scpref; }

    CFPropertyListRef	get_value(CFStringRef key) const;
    CFDataRef		create_signature(void) const;

private:

    Preferences(const Preferences&); // nocopy
    Preferences& operator=(const Preferences&); // nocopy

    std::string		m_pspec;
    CFPropertyListRef	m_plist;
    SCPreferencesRef	m_scpref;
};

class SyncMutex
{
public:
    SyncMutex();
    ~SyncMutex();

private:
    SyncMutex(const SyncMutex&); // nocopy
    SyncMutex& operator=(const SyncMutex&); // nocopy

    int m_fd;
};

/* Current time in microseconds. */
unsigned long long time_now_usec(void);

void post_service_notification(const char * service_name,
				const char * service_state);

/* vim: set cindent ts=8 sts=4 tw=79 : */
