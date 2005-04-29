%define version 1.4.3

Summary: A front end for testing other programs.
Name: dejagnu
Version: %{version}
Release: 0
Copyright: GPL
Source: ftp://ftp.gnu.org/gnu/dejagnu/snapshots/dejagnu-%{version}.tar.gz

#Patch0: dejagnu-1.4.3-rth.patch
Group: Development/Tools
# Since we're building this on a debian system, we can't require these.
Requires: tcl >= 8.0, expect >= 5.21
BuildRoot: /tmp/%{name}-root

%description
DejaGnu is an Expect/Tcl based framework for testing other programs.
DejaGnu has several purposes: to make it easy to write tests for any
program; to allow you to write tests which will be portable to any
host or target where a program must be tested; and to standardize the
output format of all tests (making it easier to integrate the testing
into software development).

%prep
%setup -q -n dejagnu-%{version}

%build
./configure -v
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr
mkdir -p $RPM_BUILD_ROOT/usr/include
mkdir -p $RPM_BUILD_ROOT/usr/share/dejagnu
mkdir -p $RPM_BUILD_ROOT/usr/doc/dejagnu-%{version}
make prefix=$RPM_BUILD_ROOT/usr install
make prefix=$RPM_BUILD_ROOT/usr install-doc

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/bin/runtest
/usr/include/dejagnu.h
/usr/share/dejagnu/*

# %config site.exp

%doc COPYING NEWS README AUTHORS INSTALL ChangeLog doc/overview doc/overview.ps doc/overview.pdf
 
%changelog
* Thu Aug 29 2002 Rob Savoye <rob@welcomehome.org>
- Update version number for 1.4.3 release.

* Wed Apr 11 2001 Rob Savoye <rob@welcomehome.org>
- Update version number for 1.4.2 release.

* Wed Apr 11 2001 Rob Savoye <rob@welcomehome.org>
- Added installing dejagnu.h.
- Install the ps and pdf formatted docs too

* Wed Feb 21 2001 Rob Savoye <rob@welcomehome.org>
- Fixed Requires line, and changed the URL to the new ftp site.

* Sun Oct 31 1999 Rob Savoye <rob@welcomehome.org>
- updated to the latest snapshot
- added doc files
- added the site.exp config file

* Mon Jul 12 1999 Tim Powers <timp@redhat.com>
- updated to 19990628
- updated patches as needed
- added %defattr in files section

* Wed Mar 10 1999 Jeff Johnson <jbj@redhat.com>
- add alpha expect patch (#989)
- use %configure

* Thu Dec 17 1998 Jeff Johnson <jbj@redhat.com>
- Update to 19981215.

* Thu Nov 12 1998 Jeff Johnson <jbj@redhat.com>
- Update to 1998-10-29.

* Wed Jul  8 1998 Jeff Johnson <jbj@redhat.com>
- Update to 1998-05-28.

* Sun Feb  1 1998 Jeff Johnson <jbj@jbj.org>
- Create.
 