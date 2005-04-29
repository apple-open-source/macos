%define name metakit
%define ver 2.0
%define extension tar.gz

Summary: The Metakit Library 2.0
Name: %{name}
Version: %{ver}
Release: 1
Copyright: GPL
Group: Applications/Databases
Source: %{name}-%{ver}.%{extension}
Patch: metakit-install.patch
URL: http://www.equi4.com/metakit/
Buildroot: /tmp/%{name}-%{ver}-root
Packager: Sean Summers <rpm-metakit@GeneralProtectionfault.com>

%description
Metakit is an embeddable database which runs on Unix, Windows,
Macintosh, and other platforms.  It lets you build applications which
store their data efficiently, in a portable way, and which will not need a
complex runtime installation.  In terms of the data model, Metakit takes
the middle ground between RDBMS, OODBMS, and flat-file databases - yet it
is quite different from each of them.

WHAT IT ISN'T - Metakit is not: 1) an SQL database, 2) multi-user/-threading,
3) scalable to gigabytes, 4) proprietary software, 5) a toy.

%package devel
Summary: Development Libraries for The Metakit Library 2.0
Group: Development/Libraries
%description devel
The %{name}-devel package contains the libraries and header files necessary
for writing programs that make use of the Metakit library.

%package python
Group: Development/Libraries
Summary: Python module for The Metakit Library 2.0
%description python
The %{name}-python package contains the libraries necessary
for using the Metakit as a python module.

#%package tcl
#Group: Development/Libraries
#Summary: TCL module for The Metakit Library 2.0
#%description tcl
#The %{name}-tcl package contains the libraries necessary
#for using the Metakit as a tcl module.

%prep
%setup
%patch -p1

%build
cd builds
../unix/configure --prefix=/usr --with-python
#--with-tcl ## maybe TCL_DECLARE_MUTEX is too new for RH6.1.92?
make ${RPM_BUILD_OPTS}

# Testing takes a while
rm tests/\!keepme.txt
make ${RPM_BUILD_OPTS} test

%install
cd builds
make install DESTDIR=${RPM_BUILD_ROOT}
libtool --finish ${RPM_BUILD_ROOT}/usr/lib/

#python setup
make Mk4py.so
install -Ds Mk4py.so ${RPM_BUILD_ROOT}/usr/lib/python1.5/site-packages/Mk4pymodule.so

#%install tcl
#make Mk4tcl.so

%clean
rm -rf $RPM_BUILD_ROOT

%files
%doc CHANGES README Metakit.html
%doc doc/e4s.gif doc/format.html
/usr/lib/libmk4.so
/usr/lib/libmk4.so.0
/usr/lib/libmk4.so.0.0.0
/usr/lib/libmk4.la

%files devel
%doc doc/api/ demos/
/usr/lib/libmk4.a
/usr/include/mk4.h
/usr/include/mk4.inl
/usr/include/mk4str.h
/usr/include/mk4str.inl

%files python
%doc python/*.py
%doc doc/python.*
/usr/lib/python1.5/site-packages/Mk4pymodule.so

#%files tcl
#%doc tcl/*.tcl
#%doc doc/tcl.*
#/usr/lib/tcl

