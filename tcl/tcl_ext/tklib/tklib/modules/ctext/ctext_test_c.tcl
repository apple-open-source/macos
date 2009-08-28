#!/bin/sh 
# the next line restarts using wish \
exec wish "$0" ${1+"$@"} 

#use :: so I don't forget it's global
#set ::tcl_traceExec 1

proc highlight:addClasses {win} {
	ctext::addHighlightClassForSpecialChars $win brackets green {[]}
	ctext::addHighlightClassForSpecialChars $win braces lawngreen {{}}
	ctext::addHighlightClassForSpecialChars $win parentheses palegreen {()}
	ctext::addHighlightClassForSpecialChars $win quotes "#c65e3c" {"'}

	ctext::addHighlightClass $win control red [list namespace while for if else do switch case]
		
	ctext::addHighlightClass $win types purple [list \
	int char u_char u_int long double float typedef unsigned signed]
	
	ctext::addHighlightClass $win macros mediumslateblue [list \
	#define #undef #if #ifdef #ifndef #endif #elseif #include #import #exclude]
	
	ctext::addHighlightClassForSpecialChars $win math cyan {+=*-/&^%!|<>}
}

proc main {} {
	source ./ctext.tcl

	pack [frame .f] -fill both -expand 1
	#Of course this could be cscrollbar instead, but it's not as common.
	pack [scrollbar .f.s -command ".f.t yview"] -side right -fill y

	#Dark colors			
	pack [ctext .f.t -linemap 1 \
		-bg black -fg white -insertbackground yellow \
		-yscrollcommand ".f.s set"] -fill both -expand 1

	highlight:addClasses .f.t
	ctext::enableComments .f.t

	set fi [open test.c r]
	.f.t fastinsert end [read $fi]
	close $fi
	
	pack [button .append -text Append -command {.f.t append}] -side left
	pack [button .cut -text Cut -command {.f.t cut}] -side left
	pack [button .copy -text Copy -command {.f.t copy}] -side left
	pack [button .paste -text Paste -command {.f.t paste}] -side left
	.f.t highlight 1.0 end
	pack [button .test -text {Remove all Tags and Highlight} \
		-command {puts [time {
			foreach tag [.f.t tag names] {
				.f.t tag remove $tag 1.0 end
			}
			update idletasks
			.f.t highlight 1.0 end
			}]
		}
	] -side left
	pack [button .test2 -text {Scrollbar Command {}} -command {.f.t config -yscrollcommand {}}] -side left
	pack [button .cl -text {Clear Classes} \
		-command {ctext::clearHighlightClasses .f.t}] -side left
	pack [button .exit -text Exit -command exit] -side left
	#pack [ctext .ct2 -linemap 1] -side bottom	

	#update
	#console show
	#puts [.f.t cget -linemap]
	#puts [.f.t cget -bg]
}
main
