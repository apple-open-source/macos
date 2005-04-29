#
# Demo: MailWasher
#
proc DemoMailWasher {} {

	set T .f2.f1.t

	InitPics *checked

	set height [font metrics [$T cget -font] -linespace]
	if {$height < 18} {
		set height 18
	}
	$T configure -showroot no -showrootbutton no -showbuttons no \
		-showlines no -itemheight $height -selectmode browse \
		-xscrollincrement 1

	set pad 4
	$T column configure 0 -text Delete -textpadx $pad -tag delete
	$T column configure 1 -text Bounce -textpadx $pad -tag bounce
	$T column configure 2 -text Status -width 80 -textpadx $pad -tag status
	$T column configure 3 -text Size -width 40 -textpadx $pad -justify right -tag size
	$T column configure 4 -text From -width 140 -textpadx $pad -tag from
	$T column configure 5 -text Subject -width 240 -textpadx $pad -tag subject
	$T column configure 6 -text Received -textpadx $pad -arrow up -arrowpad {4 0} -tag received
	$T column configure 7 -text Attachments -textpadx $pad -tag attachments

	$T element create border rect -open nw -outline gray -outlinewidth 1 \
		-fill [list $::SystemHighlight {selected}]
	$T element create imgOff image -image unchecked
	$T element create imgOn image -image checked
	$T element create txtAny text \
		-fill [list $::SystemHighlightText {selected}] -lines 1
	$T element create txtNone text -text "none" \
		-fill [list $::SystemHighlightText {selected}] -lines 1
	$T element create txtYes text -text "yes" \
		-fill [list $::SystemHighlightText {selected}] -lines 1
	$T element create txtNormal text -text "Normal" \
		-fill [list $::SystemHighlightText {selected} #006800 {}] -lines 1
	$T element create txtPossSpam text -text "Possible Spam"  \
		-fill [list $::SystemHighlightText {selected} #787800 {}] -lines 1
	$T element create txtProbSpam text -text "Probably Spam" \
		-fill [list $::SystemHighlightText {selected} #FF9000 {}] -lines 1
	$T element create txtBlacklist text -text "Blacklisted" \
		-fill [list $::SystemHighlightText {selected} #FF5800 {}] -lines 1

	foreach name {Off On} {
		set S [$T style create sty$name]
		$T style elements $S [list border img$name]
		$T style layout $S border -detach yes -iexpand es
		$T style layout $S img$name -expand wnes
	}

	set pad 4

	foreach name {Any None Yes Normal PossSpam ProbSpam Blacklist} {
		set S [$T style create sty$name]
		$T style elements $S [list border txt$name]
		$T style layout $S border -detach yes -iexpand es
		$T style layout $S txt$name -padx $pad -squeeze x -expand ns
	}
for {set i 0} {$i < 1} {incr i} {
	foreach {from subject} {
		baldy@spammer.com "Your hair is thinning"
		flat@spammer.com "Your breasts are too small"
		tiny@spammer.com "Your penis is too small"
		dumbass@spammer.com "You are not very smart"
		bankrobber@spammer.com "You need more money"
		loser@spammer.com "You need better friends"
		gossip@spammer.com "Find out what your coworkers think about you"
		whoami@spammer.com "Find out what you think about yourself"
		downsized@spammer.com "You need a better job"
		poorhouse@spammer.com "Your mortgage is a joke"
		spam4ever@spammer.com "You need more spam"
	} {
		set item [$T item create]
		set status [lindex [list styNormal styPossSpam styProbSpam styBlacklist] [expr int(rand() * 4)]]
		set delete [lindex [list styOn styOff] [expr int(rand() * 2)]]
		set bounce [lindex [list styOn styOff] [expr int(rand() * 2)]]
		set attachments [lindex [list styNone styYes] [expr int(rand() * 2)]]
		$T item style set $item 0 $delete 1 $bounce 2 $status 3 styAny \
			4 styAny 5 styAny 6 styAny 7 $attachments
		set bytes [expr {512 + int(rand() * 1024 * 12)}]
		set size [expr {$bytes / 1024 + 1}]KB
		set seconds [expr {[clock seconds] - int(rand() * 100000)}]
		set received [clock format $seconds -format "%d/%m/%y %I:%M %p"]
		$T item text $item 3 $size 4 $from 5 $subject 6 $received
		$T item lastchild root $item
	}
}
	if 0 {
		$T notify bind MailWasher <Button1-ElementPress-imgOn> {
			%T item style set %I %C styOff
		}
		$T notify bind MailWasher <Button1-ElementPress-imgOff> {
			%T item style set %I %C styOn
		}
	}

	set ::SortColumn 6
	$T notify bind $T <Header-invoke> {
		if {%C == $SortColumn} {
			if {[%T column cget $SortColumn -arrow] eq "down"} {
				set order -increasing
				set arrow up
			} else {
				set order -decreasing
				set arrow down
			}
		} else {
			if {[%T column cget $SortColumn -arrow] eq "down"} {
				set order -decreasing
				set arrow down
			} else {
				set order -increasing
				set arrow up
			}
			%T column configure $SortColumn -arrow none
			set SortColumn %C
		}
		%T column configure %C -arrow $arrow
		switch [%T column cget %C -tag] {
			bounce -
			delete {
				%T item sort root $order -column %C -command [list CompareOnOff %T %C] -column subject -dictionary
			}
			status {
				%T item sort root $order -column %C -dictionary
			}
			from {
				%T item sort root $order -column %C -dictionary -column subject -dictionary
			}
			subject {
				%T item sort root $order -column %C -dictionary
			}
			size {
				%T item sort root $order -column %C -dictionary -column subject -dictionary
			}
			received {
				%T item sort root $order -column %C -dictionary -column subject -dictionary
			}
			attachments {
				%T item sort root $order -column %C -dictionary -column subject -dictionary
			}
		}
	}

	bind MailWasher <ButtonPress-1> {
		set id [%W identify %x %y]
		if {$id eq ""} {
		} elseif {[lindex $id 0] eq "header"} {
		} else {
			foreach {what item where arg1 arg2 arg3} $id {}
			if {$where eq "column"} {
				set tag [%W column cget $arg1 -tag]
				if {$tag eq "delete" || $tag eq "bounce"} {
					set style [%W item style set $item $arg1]
					if {$style eq "styOn"} {
						set style styOff
					} else {
						set style styOn
					}
					%W item style set $item $arg1 $style
					DisplayStylesInItem $item
#					return -code break
				}
			}
		}
	}

	bindtags $T [list $T MailWasher TreeCtrl [winfo toplevel $T] all]

	return
}

proc CompareOnOff {T C item1 item2} {
	set s1 [$T item style set $item1 $C]
	set s2 [$T item style set $item2 $C]
	if {$s1 eq $s2} { return 0 }
	if {$s1 eq "styOff"} { return -1 }
	return 1
}

