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

Exclude calendar

#       name         pkg   doc   example
Module  aes         _tcl  _man  _null
Module  amazon-s3   _tcl  _man  _null
Module  asn         _tcl  _man  _null
Module  base64      _tcl  _man  _null
Module  base32      _tcl  _man  _null
Module  bee         _tcl  _man  _null
Module  bench       _tcl _null  _null
Module  bibtex      _tcl  _man  _null
Module  blowfish    _tcl  _man  _null
Module  cache       _tcl  _man  _null
Module  calendar     _tci _man  _null
Module  cmdline     _tcl  _man  _null
Module  comm        _tcl  _man  _null
Module  control      _tci _man  _null
Module  counter     _tcl  _man  _null
Module  crc         _tcl  _man  _null
Module  csv         _tcl  _man _exa
Module  des         _tcl  _man  _null
Module  dns          _msg _man _exa
Module  docstrip    _tcl  _man  _null
Module  doctools     _doc _man _exa
Module  exif        _tcl  _man  _null
Module  fileutil    _tcl  _man  _null
Module  ftp         _tcl  _man _exa
Module  ftpd        _tcl  _man _exa
Module  fumagic     _tcl  _man  _null
Module  grammar_fa  _tcl  _man  _null
Module  grammar_me  _tcl  _man  _null
Module  grammar_peg _tcl  _man  _null
Module  html        _tcl  _man  _null
Module  htmlparse   _tcl  _man  _null
Module  http        _tcl  _man  _null
Module  ident       _tcl  _man  _null
Module  inifile     _tcl  _man  _null
Module  interp      _tcl  _man  _null
Module  irc         _tcl  _man _exa
Module  javascript  _tcl  _man  _null
Module  jpeg        _tcl  _man  _null
Module  json        _tcl  _man  _null
Module  ldap        _tcl  _man _exa
Module  log          _msg _man  _null
Module  map         _tcl  _man  _null
Module  mapproj     _tcl  _man _exa
Module  math         _tci _man _exa
Module  md4         _tcl  _man  _null
Module  md5         _tcl  _man  _null
Module  md5crypt    _tcl  _man _null
Module  mime        _tcl  _man _exa
Module  multiplexer _tcl  _man  _null
Module  ncgi        _tcl  _man  _null
Module  nmea        _tcl  _man  _null
Module  nntp        _tcl  _man _exa
Module  nns         _tcl  _man  _null
Module  ntp         _tcl  _man _exa
Module  otp         _tcl  _man  _null
Module  page         _tcr _man  _null
Module  pluginmgr   _tcl  _man  _null
Module  png         _tcl  _man  _null
Module  pop3        _tcl  _man  _null
Module  pop3d       _tcl  _man  _null
Module  profiler    _tcl  _man  _null
Module  rc4         _tcl  _man  _null
Module  rcs         _tcl  _man  _null
Module  report      _tcl  _man  _null
Module  ripemd      _tcl  _man  _null
Module  sasl        _tcl  _man  _null
Module  sha1        _tcl  _man  _null
Module  simulation  _tcl  _man  _null
Module  smtpd       _tcl  _man _exa
Module  snit        _tcl  _man  _null
Module  soundex     _tcl  _man  _null
Module  stooop      _tcl  _man  _null
Module  stringprep  _tcl  _man  _null
Module  struct      _tcl  _man _exa
Module  tar         _tcl  _man  _null
Module  term         _tcr _man _exa
Module  textutil     _tex _man  _null
Module  tie         _tcl  _man  _null
Module  tiff        _tcl  _man  _null
Module  transfer    _tcl  _man  _null
Module  treeql      _tcl  _man  _null
Module  uev         _tcl  _man  _null
Module  units       _tcl  _man  _null
Module  uri         _tcl  _man  _null
Module  uuid        _tcl  _man  _null
Module  wip         _tcl  _man  _null
Module  yaml        _tcl  _man  _null

Application  dtplite
Application  tcldocstrip
Application  page

# @@ Registration END
# --------------------------------------------------------------
