# $XTermId: xterm.spec,v 1.8 2011/02/14 00:19:58 tom Exp $
Summary: A text-based Web browser
Name: xterm
Version: dev
Release: 269
License: X11
Group: Applications/Internet
Source: xterm-%{release}.tgz
# URL: http://invisible-island.net/xterm/
Provides: x-terminal-emulator

%description
xterm is the standard terminal emulator for the X Window System.
It provides DEC VT102 and Tektronix 4014 compatible terminals for
programs that cannot use the window system directly.  This version
implements ISO/ANSI colors, Unicode, and most of the control sequences
used by DEC VT220 terminals.

This package provides four commands:
 a) xterm, which is the actual terminal emulator
 b) uxterm, which is a wrapper around xterm which sets xterm to use UTF-8
    encoding when the user's locale supports this,
 c) koi8rxterm, a wrapper similar to uxterm for locales that use the
    KOI8-R character set, and
 d) resize.

A complete list of control sequences supported by the X terminal emulator
is provided in /usr/share/doc/xterm.

The xterm program uses bitmap images provided by the xbitmaps package.

Those interested in using koi8rxterm will likely want to install the
xfonts-cyrillic package as well.

%prep

%define my_suffix -dev

%define desktop_vendor  dickey
%define desktop_utils   %(if which desktop-file-install 2>&1 >/dev/null ; then echo "yes" ; fi)

%define apps_shared %(test -d /usr/share/X11/app-defaults && echo 1 || echo 0)
%define apps_syscnf %(test -d /etc/X11/app-defaults && echo 1 || echo 0)

%if %{apps_shared}
%define _xresdir    %{_datadir}/X11/app-defaults
%else
%define _xresdir    %{_sysconfdir}/X11/app-defaults
%endif

%define _iconsdir   %{_datadir}/icons
%define _pixmapsdir %{_datadir}/pixmaps
%define my_docdir   %{_datadir}/doc/xterm%{my_suffix}

%setup -q -n xterm-%{release}

%build
CPPFLAGS="-DMISC_EXP -DEXP_HTTP_HEADERS" \
%configure \
	--target %{_target_platform} \
	--prefix=%{_prefix} \
	--bindir=%{_bindir} \
	--datadir=%{_datadir} \
	--mandir=%{_mandir} \
%if "%{my_suffix}" != ""
	--program-suffix=%{my_suffix} \
	--without-xterm-symlink \
%endif
	--enable-256-color \
	--enable-88-color \
	--enable-dabbrev \
	--enable-dec-locator \
	--enable-exec-xterm \
	--enable-hp-fkeys \
	--enable-load-vt-fonts \
	--enable-logfile-exec \
	--enable-logging \
	--enable-mini-luit \
	--enable-paste64 \
	--enable-rectangles \
	--enable-sco-fkeys \
	--enable-tcap-fkeys \
	--enable-tcap-query \
	--enable-toolbar \
	--enable-wide-chars \
	--enable-xmc-glitch \
	--with-app-defaults=%{_xresdir} \
	--with-icondir=%{_pixmapsdir} \
	--with-own-terminfo=%{_datadir}/terminfo \
	--with-terminal-type=xterm-new \
	--with-utempter
make

chmod u+w XTerm.ad
cat >>XTerm.ad <<EOF
*backarrowKeyIsErase: true
*ptyInitialErase: true
EOF
ls -l *.ad

%install
rm -rf $RPM_BUILD_ROOT

# Usually do not use install-ti, since that will conflict with ncurses.
make install-bin install-man install-app install-icon \
%if "%{install_ti}" == "yes"
	install-ti \
%endif
	DESTDIR=$RPM_BUILD_ROOT \
	TERMINFO=%{_datadir}/terminfo

	mkdir -p $RPM_BUILD_ROOT%{my_docdir}
	cp \
		ctlseqs.txt \
		README.i18n \
		THANKS \
		xterm.log.html \
	$RPM_BUILD_ROOT%{my_docdir}/

	cp -r vttests \
	$RPM_BUILD_ROOT%{my_docdir}/

	# The scripts are readable, but not executable, to let find-requires
	# know that they do not depend on Perl packages.
	chmod 644 $RPM_BUILD_ROOT%{my_docdir}/vttests/*

%if "%{desktop_utils}" == "yes"
for p in *.desktop
do
	sed -i \
		-e 's/Categories=System;/Categories=Application;Utility;/' \
		-e 's/^\\(Name=.*\\)/\\1%{my_suffix}/' \
		-e 's/^\\(Exec=.*\\)/\\1%{my_suffix}/' \
	$p
done

make install-desktop \
	DESKTOP_FLAGS="--vendor='%{desktop_vendor}' --dir $RPM_BUILD_ROOT%{_datadir}/applications"

test -n "%{my_suffix}" && \
( cd $RPM_BUILD_ROOT%{_datadir}/applications
	for p in *.desktop
	do
		mv $p `basename $p .desktop`%{my_suffix}.desktop
	done
)
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_bindir}/koi8rxterm%{my_suffix}
%{_bindir}/xterm%{my_suffix}
%{_bindir}/uxterm%{my_suffix}
%{_bindir}/resize%{my_suffix}
%{_mandir}/*/*
%{my_docdir}/*
%{_pixmapsdir}/xterm*.xpm
%{_xresdir}/*XTerm*

%if "%{install_ti}" == "yes"
%{_datadir}/terminfo/*
%endif

%if "%{desktop_utils}" == "yes"
%config(missingok) %{_datadir}/applications/%{desktop_vendor}-xterm%{my_suffix}.desktop
%config(missingok) %{_datadir}/applications/%{desktop_vendor}-uxterm%{my_suffix}.desktop
%endif

%changelog

* Fri Oct 22 2010 Thomas E. Dickey
- initial version.
