/*
 * Copyright (c) 2007,2008 Apple Inc. All rights reserved.
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

#include <map>
#include <vector>
#include <sstream>

class LaunchService;

#define SMB_RUN_CONFIG_PATH	"/var/db/smb.conf"
#define SMB_RUN_TEMPLATE	"/var/db/.smb.conf.XXXXXXXX"

#define SMB_SHARES_CONFIG_PATH	"/var/db/samba/smb.shares"
#define SMB_SHARES_TEMPLATE	"/var/db/samba/.smb.shares.XXXXXXXX"

/* Base config file handling. */
struct basic_config
{
    basic_config(const std::string& path, const std::string& tmpl)
	: m_cpath(path), m_ctmpl(tmpl) {
    }

    virtual void format(std::ostream&) const = 0;
    bool writeback() const;

    typedef std::pair<std::string, std::string>		param_type;
    typedef std::vector<param_type>			paramlist_type;

private:
    std::string m_cpath; // Path to write config file to
    std::string m_ctmpl; // Path to use for mkstemp template
};

/* Class to encapsulate the state of the SMB configuration. Includes the state
 * of the smb.conf file and the service daemons.
 */
class SmbConfig : public basic_config
{
public:
    typedef enum {
	GLOBAL,
	PRINTER,
	HOMES,
	NETLOGON,
	PROFILES
    } section_type;

    typedef std::map<section_type, paramlist_type> smbconf_type;

    SmbConfig();
    ~SmbConfig();

    LaunchService& SmbdService() { return *(this->m_smbd); }
    LaunchService& NmbdService() { return *(this->m_nmbd); }
    LaunchService& WinbindService() { return *(this->m_winbind); }

    const LaunchService& SmbdService() const { return *(this->m_smbd); }
    const LaunchService& NmbdService() const { return *(this->m_nmbd); }
    const LaunchService& WinbindService() const { return *(this->m_winbind); }

    void set_param(section_type section, const param_type& param);

    /* Meta-data parameters are formatted into the smb.conf header to give the
     * reader a guide to the intent of the configuration.
     */
    void set_meta(const param_type& param);
    bool meta_changed(param_type& param) const;

    /* Emit the formatted smb.conf to the given stream. */
    void format(std::ostream& out) const;

private:
    SmbConfig(const SmbConfig&); // nocopy
    SmbConfig& operator=(const SmbConfig&); // nocopy

    smbconf_type    m_smbconf;	    /* accumulated smb.conf parameters */
    paramlist_type  m_metaconf;	    /* accumulated smb.conf metadata */

    paramlist_type  m_lastmeta;	    /* sampled metadata */

    LaunchService * m_smbd;
    LaunchService * m_nmbd;
    LaunchService * m_winbind;
};

/* Class to hold the collection of SMB share definitions. */
class SmbShares : public basic_config
{
public:
    typedef std::map<std::string, paramlist_type> smbshares_type;

    SmbShares()
	: basic_config(SMB_SHARES_CONFIG_PATH, SMB_SHARES_TEMPLATE) {
    }
    ~SmbShares() {
    }

    /* Emit the formatted smb.conf to the given stream. */
    void format(std::ostream& out) const;

    void set_param(const std::string& share_name, const param_type& param);

private:
    SmbShares(const SmbShares&); // nocopy
    SmbShares& operator=(const SmbShares&); // nocopy

    smbshares_type m_smbshares;
};

template <class T> basic_config::param_type
make_smb_param(const std::string& key, const T& val)
{
    std::ostringstream ostr;
    ostr << val;
    return make_smb_param<std::string>(key, ostr.str());
}

template <> basic_config::param_type
make_smb_param<std::string>(const std::string& key, const std::string& val);

template <> basic_config::param_type
make_smb_param<bool>(const std::string& key, const bool& val);

class SmbOption;

/* Encapsulate the SMB configuration rules. We take a set of options and apply
 * a set of rules to them, which updates the SMB configuration state.
 */
class SmbRules
{
public:
    SmbRules();
    ~SmbRules();

    typedef std::vector<SmbOption *> option_list;

    typedef option_list::iterator	    iterator;
    typedef option_list::const_iterator	    const_iterator;
    typedef option_list::size_type	    size_type;

    size_type size() const;

    iterator begin() { return this->m_options.begin(); }
    iterator end() { return this->m_options.end(); }
    const_iterator begin() const { return this->m_options.begin(); }
    const_iterator end() const { return this->m_options.end(); }

    const std::string& version() const { return m_version; }

private:
    SmbRules(const SmbRules&); //nocopy
    SmbRules& operator=(const SmbRules&); //nocopy

    option_list m_options;
    std::string m_version;

};

/* vim: set cindent ts=8 sts=4 tw=79 : */
