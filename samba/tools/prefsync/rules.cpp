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

#include <arpa/inet.h>

typedef SimpleOption<std::string>   SimpleStringOption;
typedef SimpleOption<bool>	    SimpleBoolOption;
typedef SimpleOption<unsigned>	    SimpleIntOption;

typedef std::vector<std::string> string_list;

static bool
cfstring_array_convert(SmbOption& opt,
			CFPropertyListRef val,
			string_list& strings)
{
    if (CFGetTypeID(val) == CFStringGetTypeID()) {
	/* Accept a string as an array of 1. */
	strings.clear();
	strings.push_back(cfstring_convert((CFStringRef)val));
    } else if (CFGetTypeID(val) == CFArrayGetTypeID()) {
	/* The normal case is to just copy the strings out of the array. */
	strings.clear();
	for (CFIndex i = 0; i < CFArrayGetCount((CFArrayRef)val); ++i) {
	    CFTypeRef strval = CFArrayGetValueAtIndex((CFArrayRef)val, i);

	    if (CFGetTypeID(strval) != CFStringGetTypeID()) {
		VERBOSE("%s ignoring unexpected %s value\n",
			opt.name(),
			cftype_string(CFGetTypeID(strval)).c_str());
		continue;
	    }

	    strings.push_back(cfstring_convert((CFStringRef)strval));
	}
    } else {
	VERBOSE("%s expected %s or %s, but found %s\n",
		opt.name(),
		cftype_string(CFStringGetTypeID()).c_str(),
		cftype_string(CFArrayGetTypeID()).c_str(),
		cftype_string(CFGetTypeID(val)).c_str());
	return false;
    }

    return true;
}

class KerberosOption : public SmbOption
{
public:
    KerberosOption() : SmbOption("KerberosOption") {}

    ~KerberosOption() {}

    void reset(const Preferences& prefs);
    void emit(SmbConfig& smb);

private:
    std::string	m_managed;
    std::string	m_local;
};

void KerberosOption::reset(const Preferences& prefs)
{
    CFPropertyListRef val;

    if ((val = prefs.get_value(CFSTR(kSMBPrefKerberosRealm)))) {
	CATCH_TYPE_ERROR(this->m_managed =
			    property_convert<std::string>(val));

    }

    if ((val = prefs.get_value(CFSTR(kSMBPrefLocalKerberosRealm)))) {
	CATCH_TYPE_ERROR(this->m_local =
			    property_convert<std::string>(val));
    }

}

void KerberosOption::emit(SmbConfig& smb)
{
    if (this->m_managed.size() == 0 && this->m_local.size() == 0) {
	/* No Kerberos at all */
	return;
    }

    smb.set_param(SmbConfig::GLOBAL,
	    make_smb_param("use kerberos keytab", true));

    if (this->m_local.size() != 0) {
	smb.set_param(SmbConfig::GLOBAL,
		make_smb_param("com.apple: lkdc realm", this->m_local));

    }

    if (this->m_managed.size() != 0) {
	smb.set_param(SmbConfig::GLOBAL,
		make_smb_param("realm", this->m_managed));
    } else if (this->m_local.size() != 0) {
	/* If we have a local realm, but no managed realm, set both to the
	 * local realm.
	 */
	smb.set_param(SmbConfig::GLOBAL,
		make_smb_param("realm", this->m_local));
    }
}

class GuestAccessOption : public SmbOption
{
public:
    GuestAccessOption(bool yesno)
	: SmbOption(kSMBPrefAllowGuestAccess), m_guest(yesno) {}
    ~GuestAccessOption() {}

    void reset(const Preferences& prefs);
    void emit(SmbConfig& smb);

private:
    bool m_guest;
};

void GuestAccessOption::reset(const Preferences& prefs)
{
    CFPropertyListRef val;

    if ((val = prefs.get_value(CFSTR(kSMBPrefAllowGuestAccess)))) {
	CATCH_TYPE_ERROR(this->m_guest =
			    property_convert<bool>(val));
    }

}

void GuestAccessOption::emit(SmbConfig& smb)
{
    smb.set_param(SmbConfig::GLOBAL,
		make_smb_param("map to guest",
				this->m_guest ? "Bad User" : "Never"));

    smb.set_meta(make_smb_param("Guest access",
				this->m_guest ? "per-share" : "never"));
}

class BrowseLevelOption : public SmbOption
{
public:
    BrowseLevelOption() :SmbOption("BrowseLevel") {}
    ~BrowseLevelOption() {}

    void reset(const Preferences& prefs);
    void emit(SmbConfig& smb);

private:
    std::string m_role;	    /* server role */
    std::string	m_browse;   /* browse level */
};

void BrowseLevelOption::reset(const Preferences& prefs)
{
    CFPropertyListRef val;

    if ((val = prefs.get_value(CFSTR(kSMBPrefServerRole)))) {
	CATCH_TYPE_ERROR(this->m_role =
			    property_convert<std::string>(val));
    }

    if ((val = prefs.get_value(CFSTR(kSMBPrefMasterBrowser)))) {
	CATCH_TYPE_ERROR(this->m_browse =
				property_convert<std::string>(val));
    }

}

void BrowseLevelOption::emit(SmbConfig& smb)
{
    bool is_master = false;

    if (this->m_role == kSMBPrefServerRolePDC) {
	/* If we are a PDC, we have to also be a master browser. */
	this->m_browse = kSMBPrefMasterBrowserDomain;
    } else if (this->m_role == kSMBPrefServerRoleBDC) {
	/* If we are a BDC, we have to also be a local browser. */
	this->m_browse = kSMBPrefMasterBrowserLocal;
    }

    if (this->m_browse == kSMBPrefMasterBrowserDomain) {
	/* Become a domain master browser. */
	smb.set_meta(make_smb_param("NetBIOS browsing",
				    "domain master browser"));

	smb.set_param(SmbConfig::GLOBAL,
		make_smb_param("domain master", true));
	smb.set_param(SmbConfig::GLOBAL,
		make_smb_param("preferred master", true));
	smb.set_param(SmbConfig::GLOBAL,
		make_smb_param("os level", 65));
	is_master = true;
    } else if (this->m_browse == kSMBPrefMasterBrowserLocal) {
	/* Become a local master browser. */
	smb.set_meta(make_smb_param("NetBIOS browsing",
				    "local master browser"));

	smb.set_param(SmbConfig::GLOBAL,
		make_smb_param("domain master", false));
	smb.set_param(SmbConfig::GLOBAL,
		make_smb_param("preferred master", true));
	smb.set_param(SmbConfig::GLOBAL,
		make_smb_param("os level", 65));
	is_master = true;
    } else {
	/* Don't become a master browser. */
	smb.set_meta(make_smb_param("NetBIOS browsing",
				    "not a master browser"));

	smb.set_param(SmbConfig::GLOBAL,
		make_smb_param("domain master", false));
	smb.set_param(SmbConfig::GLOBAL,
		make_smb_param("preferred master", false));
    }

   /* Yes, to be a master browser, we need to be running both smbd
    * and nmbd.
    */
    if (is_master) {
	smb.SmbdService().required(true);
	smb.NmbdService().required(true);
    }
}

class WinsRegisterOption : public SmbOption
{
public:
    WinsRegisterOption(bool yesno)
	: SmbOption(kSMBPrefRegisterWINSName), m_register(yesno) {}
    ~WinsRegisterOption() {}

    void reset(const Preferences& prefs);
    void emit(SmbConfig& smb);

private:

    bool	m_register;
    string_list m_servers;
};

void WinsRegisterOption::reset(const Preferences& prefs)
{
    CFPropertyListRef val;

    if ((val = prefs.get_value(CFSTR(kSMBPrefRegisterWINSName)))) {
	CATCH_TYPE_ERROR(this->m_register =
				property_convert<bool>(val));
    }

    if ((val = prefs.get_value(CFSTR(kSMBPrefWINSServerAddressList)))) {
	cfstring_array_convert(*this, val, this->m_servers);
    }
}

void WinsRegisterOption::emit(SmbConfig& smb)
{
    bool first = true;
    struct in_addr addr;
    std::ostringstream ostr;

    /* Don't bother unless we are registering and we have at least one
     * WINS server address in the list.
     */
    if (this->m_servers.size() == 0 || !this->m_register) {
	return;
    }

    for (string_list::const_iterator s = this->m_servers.begin();
	    s != this->m_servers.end(); ++s) {

	/* We only accept IPv4 addreses for the WINS server. */
	if (!::inet_aton(s->c_str(), &addr)) {
	    VERBOSE("%s: '%s' is not a valid IPv4 address\n",
		this->name(), s->c_str());
	    continue;
	}

	if (!first) {
	    ostr << ' ';
	}

	ostr << (*s);
	first = false;
    }

    /* Set the WINS servers even if we are not going to register because the
     * smbfs userland can look at this.
     */
    smb.set_param(SmbConfig::GLOBAL,
		make_smb_param("wins server", ostr.str()));

    /* We only need nmbd running if we are actually going to register. */
    if (this->m_register) {
	smb.NmbdService().required(true);
    }
}

class ServicesOption : public SmbOption
{
public:
    ServicesOption(bool disk, bool print, bool wins)
	: SmbOption(kSMBPrefEnabledServices),
	m_disk(disk), m_print(print), m_wins(wins)
    {}
    ~ServicesOption() {}

    void reset(const Preferences& prefs);
    void emit(SmbConfig& smb);

private:
    string_list m_services;
    bool    m_disk;
    bool    m_print;
    bool    m_wins;
};

void ServicesOption::reset(const Preferences& prefs)
{
    CFPropertyListRef val;

    if ((val = prefs.get_value(CFSTR(kSMBPrefEnabledServices)))) {
	cfstring_array_convert(*this, val, this->m_services);
    }
}

void ServicesOption::emit(SmbConfig& smb)
{
    /* Figure out what service we are going to run. Print and disk services
     * require both smbd and nmbd because we want these resources to be
     * browseable.
     */
    for (string_list::const_iterator s = this->m_services.begin();
	    s != this->m_services.end(); ++s) {
	if (*s == kSMBPrefEnabledServicesDisk) {
	    this->m_disk = true;
	    smb.SmbdService().required(true);
	    smb.NmbdService().required(true);
	} else if (*s == kSMBPrefEnabledServicesPrint) {
	    this->m_print = true;
	    smb.SmbdService().required(true);
	    smb.NmbdService().required(true);
	} else if (*s == kSMBPrefEnabledServicesWins) {
	    this->m_wins = true;
	    smb.NmbdService().required(true);
	}
    }

    smb.set_param(SmbConfig::GLOBAL,
	    make_smb_param("enable disk services", this->m_disk));
    smb.set_param(SmbConfig::GLOBAL,
	    make_smb_param("enable print services", this->m_print));
    smb.set_param(SmbConfig::GLOBAL,
	    make_smb_param("wins support", this->m_wins));
}

class SuspendOption : public SmbOption
{
public:
    SuspendOption(bool yesno)
	: SmbOption(kSMBPrefSuspendServices),
	    m_value(yesno), m_isset(false)
    {}
    ~SuspendOption() {}

    void reset(const Preferences& prefs);
    void emit(SmbConfig& smb);

private:
    bool m_value;   /* current setting */
    bool m_isset;   /* have we changed from the default? */
};

void SuspendOption::reset(const Preferences& prefs)
{
    CFPropertyListRef val;

    if ((val = prefs.get_value(CFSTR(kSMBPrefSuspendServices)))) {
	/* Hmm. Preprocessor abuse. */
	CATCH_TYPE_ERROR(
		this->m_value = property_convert<bool>(val);
		this->m_isset = true
	);
    }

}

void SuspendOption::emit(SmbConfig& smb)
{
    /* Note that we don't clobber the command-line setting of this option
     * unless we have a value actually set in the preferences.
     */
    if (this->m_isset) {
	VERBOSE("setting %s %s\n", this->name(),
	    this->m_value ? "on" : "off");

	Options::ForceSuspend = this->m_value;
    }
}

class AutoSharesOption : public SmbOption
{
public:
    AutoSharesOption(bool homes, bool admin)
	: SmbOption("AutoShares"), m_homes(homes), m_admin(admin) {}
    ~AutoSharesOption() {}

    void reset(const Preferences& prefs);
    void emit(SmbConfig& smb);

private:
    bool m_homes;
    bool m_admin;
};

void AutoSharesOption::reset(const Preferences& prefs)
{
    CFPropertyListRef val;

    if ((val = prefs.get_value(CFSTR(kSMBPrefVirtualHomeShares)))) {
	CATCH_TYPE_ERROR(this->m_homes =
				property_convert<bool>(val));
    }

    if ((val = prefs.get_value(CFSTR(kSMBPrefVirtualAdminShares)))) {
	CATCH_TYPE_ERROR(this->m_admin =
				    property_convert<bool>(val));
    }

}

void AutoSharesOption::emit(SmbConfig& smb)
{
    /* Unfortunately, the admin shares implementation is based on the homes
     * share implementation. It shouldn't be, but it is, so if either of these
     * are set, we need [homes]. fmeh.
     */
    if (this->m_homes || this->m_admin) {
	smb.set_param(SmbConfig::HOMES,
		make_smb_param("comment", "User Home Directories"));
	smb.set_param(SmbConfig::HOMES, make_smb_param("browseable", false));
	smb.set_param(SmbConfig::HOMES, make_smb_param("read only", false));
	smb.set_param(SmbConfig::HOMES, make_smb_param("create mode", "0750"));
	smb.set_param(SmbConfig::HOMES, make_smb_param("guest ok", false));
    }

    smb.set_param(SmbConfig::HOMES,
	make_smb_param("com.apple: show admin all volumes", this->m_admin));
}

class ServerRole : public SmbOption
{
public:
    ServerRole(std::string role, bool guest)
	: SmbOption(kSMBPrefServerRole), m_role(role), m_guest(guest) {}
    ~ServerRole() {}

    void reset(const Preferences& prefs);
    void emit(SmbConfig& smb);

private:
    void configure_domain_logon(SmbConfig& smb);

    std::string m_role;
    bool	m_guest;

    static const char machine_script[];
    static const char user_script[];

};

const char ServerRole::machine_script[] =
    "/usr/bin/opendirectorypdbconfig -c create_computer_account "
	"-r %u -n /LDAPv3/127.0.0.1";
const char ServerRole::user_script[] =
    "/usr/bin/opendirectorypdbconfig -c create_user_account "
	"-r %u -n /LDAPv3/127.0.0.1";

void ServerRole::reset(const Preferences& prefs)
{
    CFPropertyListRef val;

    if ((val = prefs.get_value(CFSTR(kSMBPrefServerRole)))) {
	CATCH_TYPE_ERROR(this->m_role =
				property_convert<std::string>(val));
    }

    if ((val = prefs.get_value(CFSTR(kSMBPrefAllowGuestAccess)))) {
	CATCH_TYPE_ERROR(this->m_guest =
				property_convert<bool>(val));
    }

}

void ServerRole::emit(SmbConfig& smb)
{
    const char * auth = "odsam";

    smb.WinbindService().required(false);

    if (this->m_role == kSMBPrefServerRoleADS) {
	/* Active Directory domain member. Turn winbindd on so that we can use
	 * it to do pass-through NTLM authentication.
	 */
	smb.WinbindService().required(true);
	smb.set_param(SmbConfig::GLOBAL, make_smb_param("security", "ADS"));
    } else if (this->m_role == kSMBPrefServerRolePDC) {
	/* NT4 primary domain controller. */
	smb.set_param(SmbConfig::GLOBAL, make_smb_param("security", "USER"));
	/* PDC gets to create users and computers in the directory. */
	smb.set_param(SmbConfig::GLOBAL,
	    make_smb_param("add machine script", ServerRole::machine_script));
	smb.set_param(SmbConfig::GLOBAL,
	    make_smb_param("add user script", ServerRole::user_script));
	this->configure_domain_logon(smb);
    } else if (this->m_role == kSMBPrefServerRoleBDC) {
	/* NT4 backup domain controller. */
	smb.set_param(SmbConfig::GLOBAL, make_smb_param("security", "USER"));
	this->configure_domain_logon(smb);
    } else if (this->m_role == kSMBPrefServerRoleDomain) {
	/* NT4 domain member. */
	smb.set_param(SmbConfig::GLOBAL, make_smb_param("security", "DOMAIN"));
	auth = "ntdomain odsam";
    } else {
	/* Default is standalone server. */
	this->m_role = kSMBPrefServerRoleStandalone;
	smb.set_param(SmbConfig::GLOBAL, make_smb_param("security", "USER"));
    }

    /* Prepend the guest auth module if guest is enabled. Yes, it does have to
     * be prepended.
     */
    smb.set_param(SmbConfig::GLOBAL, make_smb_param("auth methods",
		this->m_guest ? (std::string("guest ") + auth) : (auth)));

    smb.set_meta(make_smb_param("Server role", this->m_role));
}

void ServerRole::configure_domain_logon(SmbConfig& smb)
{
    ASSERT(this->m_role == kSMBPrefServerRoleBDC ||
	    this->m_role == kSMBPrefServerRolePDC);

    smb.set_param(SmbConfig::GLOBAL, make_smb_param("domain logons", true));

    /* Auto-create user home directories. */
    smb.set_param(SmbConfig::HOMES, make_smb_param("root preexec",
					"/usr/sbin/inituser %U"));

   smb.set_param(SmbConfig::GLOBAL, make_smb_param("logon drive", "H:"));
   smb.set_param(SmbConfig::GLOBAL, make_smb_param("logon path",
					    "\\\\%N\\profiles\\%u"));

   /* Add a [netlogon] share. */
   smb.set_param(SmbConfig::NETLOGON, make_smb_param("path", "/etc/netlogon"));
   smb.set_param(SmbConfig::NETLOGON, make_smb_param("browseable", false));
   smb.set_param(SmbConfig::NETLOGON, make_smb_param("write list", "@admin"));
   smb.set_param(SmbConfig::NETLOGON, make_smb_param("oplocks", true));
   smb.set_param(SmbConfig::NETLOGON, make_smb_param("strict locking", false));

   /* Add a [profiles] share. */
   smb.set_param(SmbConfig::PROFILES, make_smb_param("path", "/Users/Profiles"));
   smb.set_param(SmbConfig::PROFILES, make_smb_param("browseable", false));
   smb.set_param(SmbConfig::PROFILES, make_smb_param("read only", false));
   smb.set_param(SmbConfig::PROFILES, make_smb_param("oplocks", true));
   smb.set_param(SmbConfig::PROFILES, make_smb_param("strict locking", false));
}

SmbRules::SmbRules()
{

    /* The rules version must be a unique tag that is guaranteed to change
     * when the configuration rules change. The SVN revision of this file
     * matches those needs quite well.
     */
    this->m_version = std::string("$Id: rules.cpp 32909 2007-08-17 23:07:40Z jpeach $");

    this->m_options.push_back(new ServerRole(kSMBPrefServerRoleStandalone,
					false /* guest */));

    this->m_options.push_back(new NullOption(kSMBPrefNetBIOSNodeType));
    this->m_options.push_back(new NullOption(kSMBPrefAllowKerberosAuth));
    this->m_options.push_back(new NullOption(kSMBPrefAllowNTLM2Auth));

    this->m_options.push_back(new SimpleStringOption(kSMBPrefNetBIOSName,
					"netbios name", ""));
    this->m_options.push_back(new SimpleStringOption(kSMBPrefNetBIOSScope,
					"netbios scope", ""));
    this->m_options.push_back(new SimpleStringOption(kSMBPrefWorkgroup,
					"workgroup", ""));
    this->m_options.push_back(new SimpleStringOption(kSMBPrefKerberosRealm,
					"realm", ""));
    this->m_options.push_back(new SimpleStringOption(kSMBPrefDOSCodePage,
					"dos charset", "CP437"));
    this->m_options.push_back(new SimpleStringOption(kSMBPrefPasswordServer,
					"password server", ""));
    this->m_options.push_back(new SimpleStringOption(kSMBPrefServerDescription,
					"server string", "Mac OS X"));

    this->m_options.push_back(new SimpleBoolOption(kSMBPrefAllowNTLMAuth,
					"ntlm auth", true));
    this->m_options.push_back(new SimpleBoolOption(kSMBPrefAllowLanManAuth,
					"lanman auth", false));

    this->m_options.push_back(new SimpleIntOption(kSMBPrefMaxClients,
					"max smbd processes", 10));
    this->m_options.push_back(new SimpleIntOption(kSMBPrefLoggingLevel,
					"log level", 1));

    this->m_options.push_back(new SuspendOption(false));
    this->m_options.push_back(new KerberosOption());
    this->m_options.push_back(new GuestAccessOption(false));

    /* At this time, there is no UI to set whether we want to register with
     * WINS or not. We set the registration default to true so that as soon as
     * we have any WINS server addresses we will start registering.
     */
    this->m_options.push_back(new WinsRegisterOption(true /* register? */));

    this->m_options.push_back(new BrowseLevelOption());

    /* This is the PFS default. Server on-disk defaults override this. */
    this->m_options.push_back(new AutoSharesOption(true, true));

    /* All services are off by default. */
    this->m_options.push_back(new ServicesOption(false /* disk */,
					false /* print */, false /* wins */));
}

SmbRules::~SmbRules()
{
    for (iterator i = this->begin(); i != this->end(); ++i) {
	delete *i;
    }
}

/* vim: set cindent ts=8 sts=4 tw=79 : */
