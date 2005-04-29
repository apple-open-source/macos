Summary: procmail mail delivery agent
Name: procmail
Version: 3.22
Release: 1
Copyright: GPL
Group: Daemons
Source: ftp://ftp.%{name}.org/pub/%{name}/%{name}-%{version}.tar.gz
BuildRoot: /var/tmp/%{name}-rpmroot
Provides: localmail procmail
Packager: Bruce Guenter <bruce.guenter@qcc.sk.ca>

%description
Most mail servers such as sendmail need to have a local delivery agent.
Procmail can be used as the local delivery agent for you mail server.  It
supports a rich command set that allows you to pre-sort, archive, or re-mail
incoming mail automatically.  SmartList also needs procmail to operate.

%prep
%setup
perl -ni -le 'next if /^LOCKINGTEST=/; s/^#// if /^#LOCKINGTEST=/; print' Makefile

%build
LOCKINGTEST='/tmp .' make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/{bin,man/man{1,5}}
make BASENAME=$RPM_BUILD_ROOT/usr VISIBLE_BASENAME=/usr install

%files
%attr(4511,root,root) /usr/bin/procmail
%defattr(-,root,root)
/usr/bin/lockfile
/usr/bin/mailstat
/usr/bin/formail
%doc /usr/man/*/*
%doc [A-Z]* examples

%changelog
* Mon Sep 10 2001 Philip Guenther <guenther@sendmail.com>
  - 3.22-1
* Thu Jun 29 2001 Philip Guenther <guenther@sendmail.com>
  - 3.21-1
* Thu Jun 28 2001 Philip Guenther <guenther@sendmail.com>
  - 3.20-1
* Mon Jan 08 2001 Bennett Todd <bet@rahul.net>
  - 3.15.1-1: Massive cleanup, simplify
  - parameterize on %{name} and %{version}
  - make cleaner buildroot handling with wildcards in %files
  - moved this changelog to the bottom, cleaned some whitespace out of it.
* Sat Nov 18 2000 Bennett Todd <bet@rahul.net>
  - 3.15-1
* Fri Jun 23 2000 Bennett Todd <bet@rahul.net>
  - Release 4: rebuilt from fresh tarball released today
* Wed Jun 21 2000 Bennett Todd <bet@rahul.net>
  - Release 3: rebuilt from fresh tarball released today
* Sat Dec 26 1999 Bennett Todd <bet@rahul.net>
  - Release 2 --- fixed perms following recommendation from
    Philip Guenther <guenther@gac.edu>
* Wed Dec 22 1999 Bennett Todd <bet@mordor.net>
  - Version 3.15pre
* Wed Nov 24 1999 Bennett Todd <bet@mordor.net>
  - Version 3.14, Release 1
  - dropped all patches
* Fri Sep 24 1999 Bennett Todd <bet@mordor.net>
  - Added invert-Y patch, to make formail default to not trusting Content-Length.
  - changed from Name=>procmail Release=>maildir-3 to Name=>procmail-maildir
    Release=>4, since I couldn't get my rpm(1) to tolerate a Release of
    "maildir-4".
* Thu Sep 09 1999 Bruce Guenter <brucg@em.ca>
- Fixed permissions on mailstat (shell script must be readable).
- Clarified man page information on how deliveries are done.
* Tue Apr 06 1999 Bruce Guenter <bruce.guenter@qcc.sk.ca>
- Added nospoollock patch to avoid creating /var/spool/mail/USERNAME.
- Updated to procmail 3.13.1
* Mon Apr 05 1999 Bruce Guenter <bruce.guenter@qcc.sk.ca>
- Added maildir patch
  Added no-lock-directory patch
* Mon Apr 05 1999 James Bourne <jbourne@affinity-systems.ab.ca>
- updated to procmail 3.13
* Tue Jan 12 1999 James Bourne <jbourne@affinity-systems.ab.ca>
- added attr's to files section
* Thu Jan 07 1999 James Bourne <jbourne@affinity-systems.ab.ca>
- Rebuilt RPM and SRPM with pgp signature and proper spec file for rhcn.
* Thu Dec 17 1998 James Bourne <jbourne@affinity-systems.ab.ca>
- built RPM and SRPM.  only changes are that the spec file uses it's own
	install section and does not use the procmail install methods.
