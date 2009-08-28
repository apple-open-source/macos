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
	]

set     modules [list]
array set guide {}
foreach {m pkg doc exa} {
    autoscroll  _tcl  _man  _null
    chatwidget  _tcl  _man  _null
    ctext       _ctxt _man  _null
    cursor      _tcl  _man  _null
    datefield   _tcl  _man  _null
    diagrams    _tcl  _man  _null
    getstring   _tcl  _man  _null
    history     _tcl  _man  _null
    ico         _tcl  _man  _null
    ipentry     _tcl  _man  _null
    khim        _tclm _man  _null
    ntext       _tcl  _man  _exa
    plotchart   _tcl  _man  _exa
    style       _tcl  _man  _null
    swaplist    _tcl  _man  _null
    tablelist   _tab  _null _exa
    tkpiechart  _tcl  _man  _null
    tooltip     _tcl  _man  _null
    widget      _tcl  _man  _exa
} {
    lappend modules $m
    set guide($m,pkg) $pkg
    set guide($m,doc) $doc
    set guide($m,exa) $exa
}

# --------------------------------------------------------------
