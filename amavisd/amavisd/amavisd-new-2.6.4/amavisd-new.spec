# Upstream: <amavis-user$lists,sf,net>
#
#

%define logmsg logger -t %{name}/rpm

Summary: Mail virus-scanner
Name: amavisd-new
Version: 2.6.0
Release: 1
License: GPL
Group: System Environment/Daemons
URL: http://www.ijs.si/software/amavisd/

Packager: Marius Andreiana <marius_andreiana@epon_ro>
Vendor: Amavisd-new

Source: http://www.ijs.si/software/amavisd/amavisd-new-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

Requires: arc >= 5.21e, nomarch >= 1.2, unrar >= 2.71, zoo >= 2.10
Requires: bzip2, cpio, file, freeze, lha, lzop, ncompress, unarj
Requires: perl(Archive::Tar), perl(Archive::Zip), perl(Compress::Zlib)
Requires: perl(Convert::TNEF), perl(Convert::UUlib), perl(IO::Stringy)
Requires: perl(MIME::Base64), perl(MIME::Tools), perl(Unix::Syslog)
Requires: perl(Time::HiRes), perl(Digest::MD5), perl(Digest::SHA1)
Requires: perl(Digest::HMAC), perl(Net::DNS), perl(Mail::SpamAssassin)
Requires: perl-MailTools, perl(Net::Server) >= 0.86, perl-HTML-Parser >= 3.24
Requires: perl(DB_File), perl(Mail::DKIM) >= 0.31
Obsoletes: amavisd

%description
AMaViS is a program that interfaces a mail transfer agent (MTA) with
one or more virus scanners.

Amavisd-new is a branch created by Mark Martinec that adds serveral
performance and robustness features. It's partly based on
work being done on the official amavisd branch. Please see the
README.amavisd-new-RELNOTES file for a detailed description.

%prep
%setup -n amavisd-new-%{version}

%{__cat} <<'EOF' >amavisd.sysconfig
EOF

%{__cat} <<'EOF' >amavisd.sysv
#!/bin/bash
#
# Init script for AMaViS email virus scanner.
#
# Written by Dag Wieers <dag@wieers.com>.
# Modified by Marius Andreiana.
#
# chkconfig: 2345 79 31
# description: AMaViS virus scanner.
#
# processname: amavisd
# config: %{_sysconfdir}/amavisd.conf
# pidfile: %{_localstatedir}/run/amavisd.pid

source %{_initrddir}/functions

[ -x %{_sbindir}/amavisd ] || exit 1
[ -r %{_sysconfdir}/amavisd.conf ] || exit 1

### Default variables
AMAVIS_ACCOUNT="amavis"
SYSCONFIG="%{_sysconfdir}/sysconfig/amavisd"
prog_config_file=%{_sysconfdir}/amavisd.conf

### Read configuration
[ -r "$SYSCONFIG" ] && source "$SYSCONFIG"

RETVAL=0
prog="amavisd"
desc="Mail Virus Scanner"

start() {
	echo -n $"Starting $desc ($prog): "
	daemon --user "$AMAVIS_ACCOUNT" %{_sbindir}/$prog -c $prog_config_file
	RETVAL=$?
	echo
	[ $RETVAL -eq 0 ] && touch %{_localstatedir}/lock/subsys/$prog
	return $RETVAL
}

stop() {
	echo -n $"Shutting down $desc ($prog): "
	su - $AMAVIS_ACCOUNT -c "%{_sbindir}/$prog -c $prog_config_file stop"
	RETVAL=$?
	echo
	[ $RETVAL -eq 0 ] && rm -f %{_localstatedir}/lock/subsys/$prog
	return $RETVAL
}

reload() {
	echo -n $"Reloading $desc ($prog): "
	su - $AMAVIS_ACCOUNT -c "%{_sbindir}/$prog -c $prog_config_file reload"
	RETVAL=$?
	echo
	return $RETVAL
}

restart() {
	stop
	start
}

case "$1" in
  start)
	start
	;;
  stop)
	stop
	;;
  restart)
	restart
	;;
  reload)
	reload
	;;
  condrestart)
	[ -e %{_localstatedir}/lock/subsys/$prog ] && restart
	RETVAL=$?
	;;
  status)
	status $prog
	RETVAL=$?
	;;
  *)
	echo $"Usage: $0 {start|stop|restart|reload|condrestart|status}"
	RETVAL=1
esac

exit $RETVAL
EOF

%build

%install
%{__rm} -rf %{buildroot}
%{__install} -d -m0755 %{buildroot}%{_sbindir}

%{__perl} -pi.orig -e '
		s|=\s*'\''vscan'\''|= "amavis"|;
		s|^#*(\$MYHOME)\s*=.*$|$1 = "%{_localstatedir}/spool/amavis";|;
		s|^#*(\$QUARANTINEDIR)\s*=.*$|$1 = "\$MYHOME/virusmails";|
	' amavisd.conf

%{__install} -d -m0700 %{buildroot}%{_localstatedir}/spool/amavis/virusmails/
%{__install} -d -m0700 %{buildroot}%{_localstatedir}/amavis/tmp
%{__install} -d -m0700 %{buildroot}%{_localstatedir}/amavis/db

%{__install} -D -m0755 amavisd %{buildroot}%{_sbindir}/amavisd
%{__install} -D -m0755 amavisd-nanny %{buildroot}%{_sbindir}/amavisd-nanny
%{__install} -D -m0755 amavisd-agent %{buildroot}%{_sbindir}/amavisd-agent
%{__install} -D -m0755 p0f-analyzer.pl %{buildroot}%{_sbindir}/p0f-analyzer.pl
%{__install} -D -m0755 amavisd.sysv %{buildroot}%{_initrddir}/amavisd
%{__install} -D -m0700 amavisd.conf %{buildroot}%{_sysconfdir}/amavisd.conf
%{__install} -D -m0644 LDAP.schema %{buildroot}%{_sysconfdir}/openldap/schema/amavisd-new.schema
%{__install} -D -m0644 amavisd.sysconfig %{buildroot}%{_sysconfdir}/sysconfig/amavisd

%clean
%{__rm} -rf %{buildroot}

%pre
/usr/sbin/useradd -c "AMaViS email scanner user" -M -s /bin/sh -r amavis \
		-d "/var/spool/amavis" &>/dev/null || :

%post
/sbin/chkconfig --add amavisd

if [ -r /etc/postfixes/aliases ]; then
	if ! grep -q "^virusalert:" /etc/postfix/aliases; then
		echo -e "virusalert:\troot" >> /etc/postfix/aliases
		if [ -x /usr/bin/newaliases ]; then
			/usr/bin/newaliases &>/dev/null
		else
			%logmsg "Cannot exec newaliases. Please run it manually."
		fi
	fi
fi

if [ -r /etc/mail/aliases ]; then
	if ! grep -q "^virusalert:" /etc/mail/aliases; then
		echo -e "virusalert:\troot" >> /etc/mail/aliases
		if [ -x /usr/bin/newaliases ]; then
			/usr/bin/newaliases &>/dev/null
		else
			%logmsg "Cannot exec newaliases. Please run it manually."
		fi
	fi
fi

%preun
if [ $1 -eq 0 ] ; then
    /sbin/service amavisd stop &>/dev/null || :
    /sbin/chkconfig --del amavisd
fi

%postun
if [ $1 -ne 0 ]; then
    /sbin/service amavisd condrestart &>/dev/null || :
fi

if [ "`getent passwd amavis`" ]; then
    echo -en "removing user amavis.\n"
    /usr/sbin/userdel "amavis" 2>/dev/null || :
fi
if [ "`getent group amavis`" ]; then
    echo -en "removing group amavis.\n"
    /usr/sbin/groupdel "amavis" 2>/dev/null || :
fi

%files
%defattr(-, root, root, 0755)
%doc AAAREADME.first LDAP.schema LICENSE MANIFEST RELEASE_NOTES README_FILES/* test-messages/
%config %{_initrddir}/amavisd
%config %{_sysconfdir}/openldap/schema/*.schema
#%{_sbindir}/amavis
%{_sbindir}/amavisd

%defattr(0640, root, amavis, 0755)
%config(noreplace) %{_sysconfdir}/amavisd.conf
%config(noreplace) %{_sysconfdir}/sysconfig/amavisd

%defattr(0700, amavis, amavis, 0700)
%dir %{_localstatedir}/spool/amavis/
%dir %{_localstatedir}/spool/amavis/virusmails/
%dir %{_localstatedir}/amavis
%dir %{_localstatedir}/amavis/tmp
%dir %{_localstatedir}/amavis/db


%changelog
* Mon Oct 06 2004  Marius Andreiana
- Use amavisd's stop, reload, as Mark suggested
- remove amavis user/group on uninstall
- fix perms on /var/amavis

* Mon Oct 04 2004  Marius Andreiana
- Initial release, changed DAG's spec file

