# file: tkuti.tcl
# miscellaneous Tk utilities.
#
# $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/tcl-lib/tkuti.tcl,v 1.1.1.1 2001/05/18 23:14:11 mb Exp $
# $Log: tkuti.tcl,v $
# Revision 1.1.1.1  2001/05/18 23:14:11  mb
# Move from private repository to open source repository
#
# Revision 1.1.1.1  1999/03/16 18:06:56  aram
# Originals from SMIME Free Library.
#
# Revision 1.1  1997/01/01 23:12:03  rj
# first check-in
#

proc getpos {w xn yn} \
{
  upvar $xn x $yn y
  set geom [wm geometry $w]
  scan $geom {%dx%d+%d+%d} w h x y
}
