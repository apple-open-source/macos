Summary: cscope is an interactive, screen-oriented tool that allows the user to browse through C source files for specified elements of code.
Name: cscope
Version: 15.3
Release: 1
Copyright: BSD
Group: Development/Tools
Source: cscope-15.3.tar.gz
Buildroot: /tmp/%{name}-%{version}

%description
cscope is an interactive, screen-oriented tool that allows the user to browse through C source files for specified elements of code.

%prep
%setup

%build
./configure
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/usr/man/man1

install -s -m 755 src/cscope $RPM_BUILD_ROOT/usr/bin/cscope
install -m 755 doc/cscope.1 $RPM_BUILD_ROOT/usr/man/man1/cscope.1

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc TODO COPYING ChangeLog AUTHORS README NEWS INSTALL

/usr/bin/cscope
/usr/man/man1/cscope.1

%changelog
* Mon Jul 2 2001 Cscope development team
- Version 15.3 release
- New flex scanner
- XEmacs support improvements
- Vim support improvements
- 64 bit fixes
- MSDOS support
- More editing keys
- Webcscope added to contrib
* Wed Nov 20 2000 Cscope development team
- Version 15.1 release
- New menu and line matching interface
- Support for up to 62 (up from 9) matching lines on screen
- Numerous fixes
- Updated documentation
* Tue May 15 2000 Cscope development team
- Version 15.0bl2 (build 2) pre-alpha release
- Fixes and enhancements
- Updated documentation
- Autoconf/automake support
- directory restructuring
* Sun Apr 16 2000 Petr Sorfa <petrs@sco.com>
- Initial Open Source release
- Ported to GNU environment
- Created rpm package
