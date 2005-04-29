Summary:	Tcl Image Formats (Img)
Name:		img
Version:	1.2.4
Release:	1
Copyright:	BSD
Group:		Libraries
URL:		http://purl.oclc.org/net/nijtmans/
Vendor:		Jan Nijtmans <j.nijtmans@chello.nl>
Source:		%{name}%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}%{version}-root
Prefix:		%{_prefix}
Packager:	Chris Ausbrooks <weed@bucket.pp.ualr.edu>
Requires:	tcl >= 8.0, tk >= 8.0

%description
This file contains a collection of format handlers for Tk.
It can be used in combination with Tcl/Tk 8.0 or later
but 8.3 is highly recommended.
 
%prep
%setup -q -n %{name}%{version}

%build
if [ -f %{prefix}/lib/tclConfig.sh ] ; then
	. %{prefix}/lib/tclConfig.sh
fi
if [ ! "$TCL_VERSION" = "8.0" ] ; then
	configure --prefix=%{prefix}
else
	configure --prefix=%{prefix} --disable-stubs
fi
make

%install
rm -rf %{buildroot}
make INSTALL_ROOT=%{buildroot} install

%clean
rm -rf %{buildroot}

%files
%defattr(-, root, root)
%doc ANNOUNCE README bmp.txt changes demo.tcl doc license.terms tkv.tcl
%{prefix}/lib/*

%changelog
* Tue Feb 22 2000 Chris Ausbrooks <weed@bucket.pp.ualr.edu>
- original rpm
- if you use tcl/tk 8.1 or above, please compile img from srpm
  you will gain functionality
