# file: help.tcl
# toplevel widget to display a help text (modal)
#
# $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/tcl-lib/help.tcl,v 1.1.1.1 2001/05/18 23:14:10 mb Exp $
# $Log: help.tcl,v $
# Revision 1.1.1.1  2001/05/18 23:14:10  mb
# Move from private repository to open source repository
#
# Revision 1.1.1.1  1999/03/16 18:06:55  aram
# Originals from SMIME Free Library.
#
# Revision 1.1  1997/01/01 23:11:54  rj
# first check-in
#

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc help {w helptext} \
{
  set help .help
  set text $help.text
  set sb $help.sb
  set dismiss $help.dismiss

  getpos $w x y
  incr x -100
  toplevel $help -class Dialog
  wm title $help {Help}
  wm transient $help .
  wm geometry $help +$x+$y
  wm minsize $help 0 0

  text $text -borderwidth 2 -relief sunken -yscrollcommand [list $sb set] -width 32 -height 8
  scrollbar $sb -relief sunken -command [list $text yview] -width 10 -cursor arrow
  button $dismiss -text Dismiss -command [list destroy $help]

  pack $dismiss -side bottom -pady 2
  pack $sb -side right -fill y
  pack $text -expand true -fill both

  bind $text <Any-Key> [list destroy $help]

  $text insert end $helptext

  set oldfocus [focus]
  focus $text
  tkwait window $help
  focus $oldfocus
}
