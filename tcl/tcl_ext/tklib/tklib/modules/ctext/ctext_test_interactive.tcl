#!/bin/sh 
# the next line restarts using wish \
exec wish "$0" ${1+"$@"} 

#set tcl_traceExec 1
proc linemap_mark_cmd {win type line} {
	puts "line $line was $type in $win"
}

proc main {} {
	source ./ctext.tcl

	pack [frame .f] -fill both -expand 1
	#Of course this could be cscrollbar instead, but it's not as common.
	pack [scrollbar .f.s -command {.f.t yview}] -side right -fill y

	#Dark colors
	pack [ctext .f.t -bg black -fg white -insertbackground yellow \
		-yscrollcommand {.f.s set} -linemap_mark_command linemap_mark_cmd] -fill both -expand 1

	ctext::addHighlightClass .f.t widgets purple [list obutton button label text frame toplevel \
		cscrollbar scrollbar checkbutton canvas listbox menu menubar menubutton \
		radiobutton scale entry message tk_chooseDir tk_getSaveFile \
		tk_getOpenFile tk_chooseColor tk_optionMenu]

	ctext::addHighlightClass .f.t flags orange [list -text -command -yscrollcommand \
		-xscrollcommand -background -foreground -fg -bg \
		-highlightbackground -y -x -highlightcolor -relief -width \
		-height -wrap -font -fill -side -outline -style -insertwidth \
		-textvariable -activebackground -activeforeground -insertbackground \
		-anchor -orient -troughcolor -nonewline -expand -type -message \
		-title -offset -in -after -yscroll -xscroll -forward -regexp -count \
		-exact -padx -ipadx -filetypes -all -from -to -label -value -variable \
		-regexp -backwards -forwards -bd -pady -ipady -state -row -column \
		-cursor -highlightcolors -linemap -menu -tearoff -displayof -cursor \
		-underline -tags -tag]

	ctext::addHighlightClass .f.t stackControl red {proc uplevel namespace while for foreach if else}
	ctext::addHighlightClassWithOnlyCharStart .f.t vars mediumspringgreen "\$"
	ctext::addHighlightClass .f.t htmlText yellow "<b> </b> <i> </i>"
	ctext::addHighlightClass .f.t variable_funcs gold {set global variable unset}
	ctext::addHighlightClassForSpecialChars .f.t brackets green {[]{}}
	ctext::addHighlightClassForRegexp .f.t paths lightblue {\.[a-zA-Z0-9\_\-]+}
	ctext::addHighlightClassForRegexp .f.t comments khaki {#[^\n\r]*}
	#After overloading, insertion is a little slower with the 
	#regular insert, so use fastinsert.
	#set fi [open Ctext_Bug_Crasher.tcl r]
	set fi [open long_test_script r]
	.f.t fastinsert end [read $fi]
	close $fi
	
	pack [frame .f1] -fill x

	pack [button .f1.append -text Append -command {.f.t append}] -side left
	pack [button .f1.cut -text Cut -command {.f.t cut}] -side left
	pack [button .f1.copy -text Copy -command {.f.t copy}] -side left
	pack [button .f1.paste -text Paste -command {.f.t paste}] -side left
	.f.t highlight 1.0 end
	pack [button .f1.test -text {Remove all Tags and Highlight} \
		-command {puts [time {
			foreach tag [.f.t tag names] {
				.f.t tag remove $tag 1.0 end
			}
			update idletasks
			.f.t highlight 1.0 end
			}]
		}
	] -side left
	pack [button .f1.fastdel -text {Fast Delete} -command {.f.t fastdelete 1.0 end}] -side left

	pack [frame .f2] -fill x
	pack [button .f2.test2 -text {Scrollbar Command {}} -command {.f.t config -yscrollcommand {}}] -side left
	pack [button .f2.cl -text {Clear Classes} -command {ctext::clearHighlightClasses .f.t}] -side left
	pack [button .f2.des -text Destroy -command {destroy .f.t}] -side left
	pack [button .f2.editModSet0 -text "Set Modified 0" -command {puts [.f.t edit modified 0]}] -side left
	pack [button .f2.editModGet -text "Print Modified" -command {puts [.f.t edit modified]}] -side left
	
	pack [button .f2.exit -text Exit -command exit] -side left

	pack [entry .e] -side bottom -fill x
	.e insert end "ctext::deleteHighlightClass .f.t "
	bind .e <Return> {puts [eval [.e get]]}
	
	puts [.f.t cget -linemap]
	puts [.f.t cget -linemapfg]
	puts [.f.t cget -linemapbg]
	puts [.f.t cget -bg]
}
main
