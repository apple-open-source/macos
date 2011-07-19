# -*- tcl -*-
# --------------------------------------------------------------
# List of modules to install and definitions guiding the process of
# doing so.
#
# This file is shared between 'installer.tcl' and 'sak.tcl', like
# 'package_version.tcl'. The swiss army knife requires access to the
# data in this file to be able to check if there are modules in the
# directory hierarchy, but missing in the list of installed modules.
# --------------------------------------------------------------

proc Exclude     {m} {global excluded ; lappend excluded $m ; return }
proc Application {a} {global apps     ; lappend apps     $a ; return }

proc Module  {m pkg doc exa} {
    global modules guide

    lappend   modules $m
    set guide($m,pkg) $pkg
    set guide($m,doc) $doc
    set guide($m,exa) $exa
    return
}

set excluded [list]
set modules  [list]
set apps     [list]
array set guide {}

# --------------------------------------------------------------
# @@ Registration START

#      name           pkg   doc   example
Module autoscroll    _tcl  _man  _null
Module canvas        _tcl  _man  _null
Module chatwidget    _tcl  _man  _null
Module controlwidget _tcl  _man  _exa
Module ctext         _ctxt _man  _null
Module cursor        _tcl  _man  _null
Module crosshair     _tcl  _man  _null
Module datefield     _tcl  _man  _null
Module diagrams      _tcl  _man  _null
Module getstring     _tcl  _man  _null
Module history       _tcl  _man  _null
Module ico           _tcl  _man  _null
Module ipentry       _tcl  _man  _null
Module khim          _tclm _man  _null
Module menubar       _tcl  _man  _exa
Module ntext         _tcl  _man  _exa
Module plotchart     _tcl  _man  _exa
Module style         _tcl  _man  _null
Module swaplist      _tcl  _man  _null
Module tablelist     _tab  _null _exa
Module tkpiechart    _tcl  _man  _exa
Module tooltip       _tcl  _man  _null
Module widget        _tcl  _man  _exa

Application  dia
Application  bitmap-editor

# @@ Registration END
# --------------------------------------------------------------
