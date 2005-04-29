# -*- tcl -*-
# --------------------------------------------------------------
# List of modules to install and definitions guiding the process of
# doing so.
#
# This file is shared between 'installer.tcl' and 'sak.tcl', like
# 'tcllib_version.tcl'. The swiss army knife requires access to the
# data in this file to be able to check if there are modules in the
# directory hierarchy, but missing in the list of installed modules.
# --------------------------------------------------------------

# Excluded:
set excluded [list \
	calendar \
	]

# Notes: struct1 is a backward compatibility module for people using
# 'struct 1.x'. The package is itself installed, but its documentation
# is not. Nor do the struct examples belong to it.

set     modules [list]
array set guide {}
foreach {m pkg doc exa} {
    base64	_tcl  _man  _null
    calendar	 _tci _man  _null
    cmdline	_tcl  _man  _null
    comm	_tcl  _man  _null
    control	 _tci _man  _null
    counter	_tcl  _man  _null
    crc		_tcl  _man  _null
    csv		_tcl  _man _exa
    des		_tcl  _man  _null
    dns		_tcl  _man _exa
    doctools	 _doc _man _exa
    exif	_tcl  _man  _null
    fileutil	_tcl  _man  _null
    ftp		_tcl  _man _exa
    ftpd	_tcl  _man _exa
    html	_tcl  _man  _null
    htmlparse	_tcl  _man  _null
    inifile     _tcl  _man  _null
    irc		_tcl  _man _exa
    javascript	_tcl  _man  _null
    log		_tcl  _man  _null
    math	 _tci _man  _null
    md4		_tcl  _man  _null
    md5		_tcl  _man  _null
    md5crypt	_tcl  _null _null
    mime	_tcl  _man _exa
    multiplexer _tcl  _man  _null
    ncgi	_tcl  _man  _null
    nntp	_tcl  _man _exa
    ntp		_tcl  _man _exa
    pop3	_tcl  _man  _null
    pop3d	_tcl  _man  _null
    profiler	_tcl  _man  _null
    report	_tcl  _man  _null
    sha1	_tcl  _man  _null
    smtpd	_tcl  _man _exa
    snit        _tcl  _man  _null
    soundex	_tcl  _man  _null
    stooop	_tcl  _man  _null
    struct	_tcl  _man _exa
    struct1	_tcl  _null _null
    textutil	 _tex _man  _null
    uri		_tcl  _man  _null
} {
    lappend modules $m
    set guide($m,pkg) $pkg
    set guide($m,doc) $doc
    set guide($m,exa) $exa
}

# --------------------------------------------------------------
