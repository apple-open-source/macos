
# Testing QuickTimeStat Components
package require QuickTimeTcl
set w ._jfrj8943jfs
toplevel $w
wm title $w {QuickTime Components}
switch $tcl_platform(platform) {
    windows {
	set listfont(s) {Courier 8 normal}
	set listfont(b) {Courier 8 bold}
    }
    default {
	set listfont(s) {Courier 10 normal}
	set listfont(b) {Courier 10 bold}
    }
}

label $w.lab -font $listfont(b) -text {Type    Subtype   Manufacture Name}  \
  -anchor w -bg gray87
listbox $w.lb -height 20 -width 80 -yscrollcommand "$w.sby set"   \
  -borderwidth 1 -relief sunken -bg gray87 -font $listfont(s)
scrollbar $w.sby -orient vertical -command "$w.lb yview"
grid $w.lab -columnspan 2 -column 0 -row 0 -sticky ew
grid $w.lb -column 0 -row 1 -sticky news
grid $w.sby -column 1 -row 1 -sticky ns
grid columnconfigure $w 0 -weight 1
grid rowconfigure $w 1 -weight 1

set comps [::quicktimetcl::info components]
foreach comp $comps {
    set txt {}
    foreach {key value} $comp {
	append txt " $value      "
    }
    $w.lb insert end $txt
}
