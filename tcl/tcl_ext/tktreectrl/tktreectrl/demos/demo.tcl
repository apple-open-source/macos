#!../Tcl-8.4.1/bin/wish84.exe

set thisPlatform $::tcl_platform(platform)
switch -- $thisPlatform {
	unix {
		if {[package vcompare [info tclversion] 8.3] == 1} {
			if {[string equal [tk windowingsystem] "aqua"]} {
				set thisPlatform "macosx"
			}
		}
	}
}

# Get full pathname to this file
set ScriptDir [file normalize [file dirname [info script]]]
cd $ScriptDir

# Command to create a full pathname in this file's directory
proc Path {args} {
	return [file normalize [eval file join [list $::ScriptDir] $args]]
}

# Create some photo images on demand
proc InitPics {args} {
	foreach pattern $args {
		if {[lsearch [image names] $pattern] == -1} {
			foreach file [glob -directory [Path pics] $pattern.gif] {
				set imageName [file root [file tail $file]]
				# I created an image called "file", which clobbered the
				# original Tcl command "file". Then I got confused.
				if {[llength [info commands $imageName]]} {
					error "don't want to create image called \"$imageName\""
				}
				image create photo $imageName -file $file

				# Hack -- Create a "selected" version too
				image create photo ${imageName}Sel
				${imageName}Sel copy $imageName
				imagetint ${imageName}Sel $::SystemHighlight 128
			}
		}
	}
	return
}

if {[catch {
	package require dbwin 1.0
}]} {
	proc dbwin s {puts -nonewline $s}
}

# Return TRUE if we are running from the development directory
proc IsDevelopment {} {
	return [file exists [Path .. generic]]
}

if {[IsDevelopment]} {

	switch -- $::thisPlatform {
		macintosh {
			load [Path .. treectrl.shlb]
		}
		macosx {
			load [Path .. build treectrl.dylib]
		}
		unix {

			# Try to load libtreectrl*.so on Unix
			load [glob -directory [Path ..] libtreectrl*[info sharedlibextension]]
		}
		default {

			# Windows build
			load [Path .. Build treectrl[info sharedlibextension]]
		}
	}

	# Default TreeCtrl bindings
	source [Path .. library treectrl.tcl]

	# Other useful bindings
	source [Path .. library filelist-bindings.tcl]

} else {

	lappend ::auto_path [Path ..]
	package require treectrl
}

# Demo sources
foreach file {
	bitmaps
	explorer
	help
	imovie
	layout
	mailwasher
	outlook-folders
	outlook-newgroup
	random
	www-options
	} {
	source [Path $file.tcl]
}

# Get default colors
set w [listbox .listbox]
set SystemButtonFace [$w cget -highlightbackground]
set SystemHighlight [$w cget -selectbackground]
set SystemHighlightText [$w cget -selectforeground]
destroy $w

proc MakeMenuBar {} {
	set m [menu .menubar]
	. configure -menu $m
	set m2 [menu $m.mFile -tearoff no]
	if {$::tcl_platform(platform) ne "unix"} {
		console eval {.console conf -height 8}
		$m2 add command -label "Console" -command {
			if {[console eval {winfo ismapped .}]} {
				console hide
			} else {
				console show
			}
		}
	}
	$m2 add command -label "View Source" -command ToggleSourceWindow
	$m2 add command -label Quit -command exit
	$m add cascade -label File -menu $m2
	return
}

proc MakeSourceWindow {} {
	set w [toplevel .source]
	set f [frame $w.f -borderwidth 0]
	switch -- $::thisPlatform {
		macintosh -
		macosx {
			set font {Geneva 9}
		}
		unix {
			set font {Courier 16}
		}
		default {
			set font {Courier 9}
		}
	}
	text $f.t -font $font -tabs [font measure $font 1234] -wrap none \
		-yscrollcommand "$f.sv set" -xscrollcommand "$f.sh set"
	scrollbar $f.sv -orient vertical -command "$f.t yview"
	scrollbar $f.sh -orient horizontal -command "$f.t xview"
	pack $f -expand yes -fill both
	grid columnconfigure $f 0 -weight 1
	grid rowconfigure $f 0 -weight 1
	grid configure $f.t -row 0 -column 0 -sticky news
	grid configure $f.sh -row 1 -column 0 -sticky we
	grid configure $f.sv -row 0 -column 1 -sticky ns

	wm protocol $w WM_DELETE_WINDOW "wm withdraw $w"
	wm geom $w -0+0
	wm withdraw $w

	return
}
proc ShowSource {file} {
	wm title .source "Demo Source: $file"
	set path [Path $file]
	set t .source.f.t
	set chan [open $path]
	$t delete 1.0 end
	$t insert end [read $chan]
	$t mark set insert 1.0
	close $chan
	return
}
proc ToggleSourceWindow {} {
	set w .source
	if {[winfo ismapped $w]} {
		wm withdraw $w
	} else {
		wm deiconify $w
	}
	return
}

MakeSourceWindow
MakeMenuBar

proc TreePlusScrollbarsInAFrame {f h v} {
	frame $f -borderwidth 1 -relief sunken
	switch -- $::thisPlatform {
		macintosh {
			set font {Geneva 9}
		}
		macosx {
			set font {{Lucida Grande} 11}
		}
		unix {
			set font {Helvetica 16}
		}
		default {
			# There is a bug on my Win98 box with Tk_MeasureChars() and
			# MS Sans Serif 8.
			set font {{MS Sans} 8}
		}
	}
	treectrl $f.t -highlightthickness 0 -borderwidth 0 -font $font
	$f.t configure -xscrollincrement 20
	$f.t debug configure -enable no -display no
	if {$h} {
		scrollbar $f.sh -orient horizontal -command "$f.t xview"
#		$f.t configure -xscrollcommand "$f.sh set"
		$f.t notify bind $f.sh <Scroll-x> { %W set %l %u }
		bind $f.sh <ButtonPress-1> "focus $f.t"
	}
	if {$v} {
		scrollbar $f.sv -orient vertical -command "$f.t yview"
#		$f.t configure -yscrollcommand "$f.sv set"
		$f.t notify bind $f.sv <Scroll-y> { %W set %l %u }
		bind $f.sv <ButtonPress-1> "focus $f.t"
	}
	grid columnconfigure $f 0 -weight 1
	grid rowconfigure $f 0 -weight 1
	grid configure $f.t -row 0 -column 0 -sticky news
	if {$h} {
		grid configure $f.sh -row 1 -column 0 -sticky we
	}
	if {$v} {
		grid configure $f.sv -row 0 -column 1 -sticky ns
	}
	return
}

proc MakeMainWindow {} {

	wm title . "TkTreeCtrl Demo"

	switch -- $::thisPlatform {
		macintosh -
		macosx {
			wm geometry . +40+40
		}
		default {
			wm geometry . +0+0
		}
	}

	panedwindow .pw2 -orient horizontal -borderwidth 0
	panedwindow .pw1 -orient vertical -borderwidth 0

	# Tree + scrollbar: demos
	TreePlusScrollbarsInAFrame .f1 0 1
	.f1.t configure -showbuttons no -showlines no -showroot no -height 100
	.f1.t column configure 0 -text "List of Demos" -expand yes -button no

	# Tree + scrollbar: styles + elements in list
	TreePlusScrollbarsInAFrame .f4 0 1
	.f4.t configure -showroot no -height 140
	.f4.t column configure 0 -text "Elements and Styles" -expand yes -button no

	# Tree + scrollbar: styles + elements in selected item
	TreePlusScrollbarsInAFrame .f3 0 1
	.f3.t configure -showroot no
	.f3.t column configure 0 -text "Styles in Item" -expand yes -button no

	.pw1 add .f1 .f4 .f3 -height 150

	# Frame on right
	frame .f2

	# Tree + scrollbars
	TreePlusScrollbarsInAFrame .f2.f1 1 1
	.f2.f1.t configure -indent 19
	.f2.f1.t debug configure -enable no -display yes -erasecolor pink -displaydelay 30

	# Give it a big border to debug drawing
	.f2.f1.t configure -borderwidth 6 -relief ridge -highlightthickness 3

	grid columnconfigure .f2 0 -weight 1
	grid rowconfigure .f2 0 -weight 1
	grid configure .f2.f1 -row 0 -column 0 -sticky news -pady 0

	.pw2 add .pw1 -width 200
	.pw2 add .f2 -width 450

	pack .pw2 -expand yes -fill both

	###
	# A treectrl widget can generate the following built-in events:
	# <ActiveItem> called when the active item changes
	# <Collapse-before> called before an item is closed
	# <Collapse-after> called after an item is closed
	# <Expand-before> called before an item is opened
	# <Expand-after> called after an item is opened
	# <Selection> called when items are added to or removed from the selection
	# <Scroll-x> called when horizontal scroll position changes
	# <Scroll-y> called when vertical scroll position changes
	#
	# The application programmer can define custom events to be
	# generated by the "T notify generate" command. The following events
	# are generated by the example bindings.

	.f2.f1.t notify install event Header
	.f2.f1.t notify install detail Header invoke

	.f2.f1.t notify install event Drag
	.f2.f1.t notify install detail Drag begin
	.f2.f1.t notify install detail Drag end
	.f2.f1.t notify install detail Drag receive

	.f2.f1.t notify install event Edit
	.f2.f1.t notify install detail Edit accept
	###

	return
}

proc MakeListPopup {} {
	set m [menu .f2.f1.t.mTree -tearoff no]

	set m2 [menu $m.mCollapse -tearoff no]
	$m add cascade -label Collapse -menu $m2

	set m2 [menu $m.mExpand -tearoff no]
	$m add cascade -label Expand -menu $m2

	set m2 [menu $m.mDebug -tearoff no]
	$m2 add checkbutton -label Data -variable Popup(debug,data) \
		-command {.f2.f1.t debug configure -data $Popup(debug,data)}
	$m2 add checkbutton -label Display -variable Popup(debug,display) \
		-command {.f2.f1.t debug configure -display $Popup(debug,display)}
	$m2 add checkbutton -label Enable -variable Popup(debug,enable) \
		-command {.f2.f1.t debug configure -enable $Popup(debug,enable)}
	$m add cascade -label Debug -menu $m2

	set m2 [menu $m.mBuffer -tearoff no]
	$m2 add radiobutton -label "none" -variable Popup(doublebuffer) -value none \
		-command {.f2.f1.t configure -doublebuffer $Popup(doublebuffer)}
	$m2 add radiobutton -label "item" -variable Popup(doublebuffer) -value item \
		-command {.f2.f1.t configure -doublebuffer $Popup(doublebuffer)}
	$m2 add radiobutton -label "window" -variable Popup(doublebuffer) -value window \
		-command {.f2.f1.t configure -doublebuffer $Popup(doublebuffer)}
	$m add cascade -label Buffering -menu $m2

	set m2 [menu $m.mLineStyle -tearoff no]
	$m2 add radiobutton -label "dot" -variable Popup(linestyle) -value dot \
		-command {.f2.f1.t configure -linestyle $Popup(linestyle)}
	$m2 add radiobutton -label "solid" -variable Popup(linestyle) -value solid \
		-command {.f2.f1.t configure -linestyle $Popup(linestyle)}
	$m add cascade -label "Line style" -menu $m2

	set m2 [menu $m.mOrient -tearoff no]
	$m2 add radiobutton -label "Horizontal" -variable Popup(orient) -value horizontal \
		-command {.f2.f1.t configure -orient $Popup(orient)}
	$m2 add radiobutton -label "Vertical" -variable Popup(orient) -value vertical \
		-command {.f2.f1.t configure -orient $Popup(orient)}
	$m add cascade -label Orient -menu $m2

	set m2 [menu $m.mSelectMode -tearoff no]
	foreach mode [list browse extended multiple single] {
		$m2 add radiobutton -label $mode -variable Popup(selectmode) -value $mode \
			-command {.f2.f1.t configure -selectmode $Popup(selectmode)}
	}
	$m add cascade -label Selectmode -menu $m2

	set m2 [menu $m.mShow -tearoff no]
	$m2 add checkbutton -label "Buttons" -variable Popup(showbuttons) \
		-command {.f2.f1.t configure -showbuttons $Popup(showbuttons)}
	$m2 add checkbutton -label "Header" -variable Popup(showheader) \
		-command {.f2.f1.t configure -showheader $Popup(showheader)}
	$m2 add checkbutton -label "Lines" -variable Popup(showlines) \
		-command {.f2.f1.t configure -showlines $Popup(showlines)}
	$m2 add checkbutton -label "Root" -variable Popup(showroot) \
		-command {.f2.f1.t configure -showroot $Popup(showroot)}
	$m2 add checkbutton -label "Root Button" -variable Popup(showrootbutton) \
		-command {.f2.f1.t configure -showrootbutton $Popup(showrootbutton)}
	$m add cascade -label Show -menu $m2

	set m2 [menu $m.mVisible -tearoff no]
	$m add cascade -label Visible -menu $m2
	return
}

proc MakeHeaderPopup {} {
	set m [menu .f2.f1.t.mHeader -tearoff no]

	set m2 [menu $m.mArrow -tearoff no]
	$m add cascade -label Arrow -menu $m2
	$m2 add radiobutton -label "None" -variable Popup(arrow) -value none -command {.f2.f1.t column configure $Popup(column) -arrow none}
	$m2 add radiobutton -label "Up" -variable Popup(arrow) -value up -command {.f2.f1.t column configure $Popup(column) -arrow up}
	$m2 add radiobutton -label "Down" -variable Popup(arrow) -value down -command {.f2.f1.t column configure $Popup(column) -arrow down}
	$m2 add separator
	$m2 add radiobutton -label "Side Left" -variable Popup(arrow,side) -value left -command {.f2.f1.t column configure $Popup(column) -arrowside left}
	$m2 add radiobutton -label "Side Right" -variable Popup(arrow,side) -value right -command {.f2.f1.t column configure $Popup(column) -arrowside right}
	$m2 add separator
	$m2 add radiobutton -label "Gravity Left" -variable Popup(arrow,gravity) -value left -command {.f2.f1.t column configure $Popup(column) -arrowgravity left}
	$m2 add radiobutton -label "Gravity Right" -variable Popup(arrow,gravity) -value right -command {.f2.f1.t column configure $Popup(column) -arrowgravity right}

	$m add checkbutton -label "Expand" -variable Popup(expand) -command {.f2.f1.t column configure $Popup(column) -expand $Popup(expand)}

	set m2 [menu $m.mJustify -tearoff no]
	$m add cascade -label Justify -menu $m2
	$m2 add radiobutton -label "Left" -variable Popup(justify) -value left -command {.f2.f1.t column configure $Popup(column) -justify left}
	$m2 add radiobutton -label "Center" -variable Popup(justify) -value center -command {.f2.f1.t column configure $Popup(column) -justify center}
	$m2 add radiobutton -label "Right" -variable Popup(justify) -value right -command {.f2.f1.t column configure $Popup(column) -justify right}
	return
}

MakeMainWindow
MakeListPopup
MakeHeaderPopup

bind .f2.f1.t <ButtonPress-3> {
	set id [%W identify %x %y]
	if {$id ne ""} {
		if {[lindex $id 0] eq "header"} {
			set Popup(column) [lindex $id 1]
			set Popup(arrow) [%W column cget $Popup(column) -arrow]
			set Popup(arrow,side) [%W column cget $Popup(column) -arrowside]
			set Popup(arrow,gravity) [%W column cget $Popup(column) -arrowgravity]
			set Popup(expand) [%W column cget $Popup(column) -expand]
			set Popup(justify) [%W column cget $Popup(column) -justify]
			tk_popup %W.mHeader %X %Y
			return
		}
	}
	set m %W.mTree.mCollapse
	$m delete 0 end
	$m add command -label "All" -command {%W collapse all}
	if {$id ne ""} {
		if {[lindex $id 0] eq "item"} {
			set item [lindex $id 1]
			$m add command -label "Item $item" -command "%W collapse $item"
			$m add command -label "Item $item (recurse)" -command "%W collapse -recurse $item"
		}
	}
	set m %W.mTree.mExpand
	$m delete 0 end
	$m add command -label "All" -command {%W expand all}
	if {$id ne ""} {
		if {[lindex $id 0] eq "item"} {
			set item [lindex $id 1]
			$m add command -label "Item $item" -command "%W expand $item"
			$m add command -label "Item $item (recurse)" -command "%W expand -recurse $item"
		}
	}
	foreach option {data display enable} {
		set Popup(debug,$option) [%W debug cget -$option]
	}
	set Popup(doublebuffer) [%W cget -doublebuffer]
	set Popup(linestyle) [%W cget -linestyle]
	set Popup(orient) [%W cget -orient]
	set Popup(selectmode) [%W cget -selectmode]
	set Popup(showbuttons) [%W cget -showbuttons]
	set Popup(showheader) [%W cget -showheader]
	set Popup(showlines) [%W cget -showlines]
	set Popup(showroot) [%W cget -showroot]
	set Popup(showrootbutton) [%W cget -showrootbutton]
	set m %W.mTree.mVisible
	$m delete 0 end
	for {set i 0} {$i < [%W numcolumns]} {incr i} {
		set Popup(visible,$i) [%W column cget $i -visible]
		$m add checkbutton -label "Column $i \"[%W column cget $i -text]\" \[[%W column cget $i -image]\]" -variable Popup(visible,$i) \
			-command "%W column configure $i -visible \$Popup(visible,$i)"
	}
	tk_popup %W.mTree %X %Y
}

# Allow "scan" bindings
if {$::thisPlatform eq "windows"} {
	bind .f2.f1.t <Control-ButtonPress-3> { }
}

#
# List of demos
#
proc InitDemoList {} {
	global DemoCmd
	global DemoFile

	set t .f1.t
	$t element create e1 text -fill [list $::SystemHighlightText {selected focus}]
	$t element create e2 rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
		-showfocus yes
	$t style create s1
	$t style elements s1 {e2 e1}
	# Tk listbox has linespace + 1 height
	$t style layout s1 e2 -union [list e1] -ipadx 2 -ipady {0 1} -iexpand e

	#	"Picture Catalog" DemoPictureCatalog
	#	"Picture Catalog 2" DemoPictureCatalog2
	#	"Folder Contents (Vertical)" DemoExplorerFilesV
	foreach {label command file} [list \
		"Random $::RandomN Items" DemoRandom random.tcl \
		"Random $::RandomN Items, Button Images" DemoRandom2 random.tcl \
		"Outlook Express (Folders)" DemoOutlookFolders outlook-folders.tcl \
		"Outlook Express (Newsgroup)" DemoOutlookNewsgroup outlook-newgroup.tcl \
		"Explorer (Details)" DemoExplorerDetails explorer.tcl \
		"Explorer (List)" DemoExplorerList explorer.tcl \
		"Explorer (Large icons)" DemoExplorerLargeIcons explorer.tcl \
		"Explorer (Small icons)" DemoExplorerSmallIcons explorer.tcl \
		"Internet Options" DemoInternetOptions www-options.tcl \
		"Help Contents" DemoHelpContents help.tcl \
		"Layout" DemoLayout layout.tcl \
		"MailWasher" DemoMailWasher mailwasher.tcl \
		"Bitmaps" DemoBitmaps bitmaps.tcl \
		"iMovie" DemoIMovie imovie.tcl \
	] {
		set item [$t item create]
		$t item lastchild root $item
		$t item style set $item 0 s1
		$t item text $item 0 $label
		set DemoCmd($item) $command
		set DemoFile($item) $file
	}
	$t yview moveto 0.0
	return
}

InitDemoList

proc ClicksToSeconds {clicks} {
	return [format "%.2g" [expr {$clicks / 1000000.0}]]
}

proc DemoSet {cmd file} {
	DemoClear
	set clicks [clock clicks]
	uplevel #0 $cmd
	set clicks [expr {[clock clicks] - $clicks}]
	dbwin "set list in [ClicksToSeconds $clicks] seconds ($clicks clicks)\n"
	.f2.f1.t xview moveto 0
	.f2.f1.t yview moveto 0
	update
	DisplayStylesInList
	ShowSource $file
}

.f1.t notify bind .f1.t <Selection> {
	if {%c == 1} {
		set selection [%T selection get]
		set item [lindex $selection 0]
		DemoSet $DemoCmd($item) $DemoFile($item)
	}
}

proc DisplayStylesInList {} {

	set T .f2.f1.t
	set t .f4.t

	# Create elements and styles the first time this is called
	if {[llength [$t style names]] == 0} {
		$t element create e1 text -fill [list $::SystemHighlightText {selected focus}]
		$t element create e2 text -fill [list $::SystemHighlightText {selected focus} "" {selected !focus} blue {}]
		$t element create e3 rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
			-showfocus yes

		$t style create s1
		$t style elements s1 {e3 e1}
		$t style layout s1 e3 -union [list e1] -ipadx 1 -ipady {0 1}

		$t style create s2
		$t style elements s2 {e3 e1 e2}
		$t style layout s2 e1 -padx {0 4}
		$t style layout s2 e3 -union [list e1 e2] -ipadx 1 -ipady {0 1}
	}

	# Clear the list
	$t item delete all

	# One item for each element in the demo list
	foreach elem [lsort [$T element names]] {
		set item [$t item create]
		$t collapse $item
		$t item hasbutton $item yes
		$t item style set $item 0 s1
		$t item text $item 0 "Element $elem ([$T element type $elem])"

		# One item for each configuration option for this element
		foreach list [$T element configure $elem] {
			foreach {name x y default current} $list {}
			set item2 [$t item create]
			if {[string equal $default $current]} {
				$t item style set $item2 0 s1
				$t item complex $item2 [list [list e1 -text [list $name $current]]]
			} else {
				$t item style set $item2 0 s2
				$t item complex $item2 [list [list e1 -text $name] [list e2 -text [list $current]]]
			}
			$t item lastchild $item $item2
		}
		$t item lastchild root $item
	}

	# One item for each style in the demo list
	foreach style [lsort [$T style names]] {
		set item [$t item create]
		$t collapse $item
		$t item hasbutton $item yes
		$t item style set $item 0 s1
		$t item text $item 0 "Style $style"

		# One item for each element in the style
		foreach elem [$T style elements $style] {
			set item2 [$t item create]
			$t collapse $item2
			$t item hasbutton $item2 yes
			$t item style set $item2 0 s1
			$t item text $item2 0 "Element $elem ([$T element type $elem])"

			# One item for each layout option for this element in this style
			foreach {option value} [$T style layout $style $elem] {
				set item3 [$t item create]
				$t item hasbutton $item3 no
				$t item style set $item3 0 s1
				$t item text $item3 0 [list $option $value]
				$t item lastchild $item2 $item3
			}
			$t item lastchild $item $item2
		}
		$t item lastchild root $item
	}

	$t xview moveto 0
	$t yview moveto 0
	return
}

proc DisplayStylesInItem {item} {

	set T .f2.f1.t
	set t .f3.t
	$t column configure 0 -text "Styles in item [$T index $item]"

	# Create elements and styles the first time this is called
	if {[llength [$t style names]] == 0} {
		$t element create e1 text -fill [list $::SystemHighlightText {selected focus}]
		$t element create e2 text -fill [list $::SystemHighlightText {selected focus} "" {selected !focus} blue {}]
		$t element create e3 rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
			-showfocus yes

		$t style create s1
		$t style elements s1 {e3 e1}
		$t style layout s1 e3 -union [list e1] -ipadx {1 2} -ipady {0 1}

		$t style create s2
		$t style elements s2 {e3 e1 e2}
		$t style layout s2 e1 -padx {0 4}
		$t style layout s2 e3 -union [list e1 e2] -ipadx 1 -ipady {0 1}
	}

	# Clear the list
	$t item delete all

	# One item for each item-column
	set column 0
	foreach style [$T item style set $item] {
		set item2 [$t item create]
		$t collapse $item2
		$t item style set $item2 0 s1
		$t item element configure $item2 0 e1 -text "Column $column: Style $style"
		set button 0

		# One item for each element in this style
		if {[string length $style]} {
			foreach elem [$T item style elements $item $column] {
				set button 1
				set item3 [$t item create]
				$t collapse $item3
				$t item hasbutton $item3 yes
				$t item style set $item3 0 s1
				$t item element configure $item3 0 e1 -text "Element $elem ([$T element type $elem])"

				# One item for each configuration option in this element
				foreach list [$T item element configure $item $column $elem] {
					foreach {name x y default current} $list {}
					set item4 [$t item create]
					set masterDefault [$T element cget $elem $name]
					set sameAsMaster [string equal $masterDefault $current]
					if {!$sameAsMaster && ![string length $current]} {
						set sameAsMaster 1
						set current $masterDefault
					}

					if {$sameAsMaster} {
						$t item style set $item4 0 s1
						$t item complex $item4 [list [list e1 -text "$name [list $current]"]]
					} else {
						$t item style set $item4 0 s2
						$t item complex $item4 [list [list e1 -text $name] [list e2 -text [list $current]]]
					}
					$t item lastchild $item3 $item4
				}
				$t item lastchild $item2 $item3
			}
			if {$button} {
				$t item hasbutton $item2 yes
			}
		}
		$t item lastchild root $item2
		incr column
	}
	$t xview moveto 0
	$t yview moveto 0

	return
}

# When one item is selected in the demo list, display the styles in that item.
# See DemoClear for why the tag "DontDelete" is used
.f2.f1.t notify bind DontDelete <Selection> {
	if {%c == 1} {
		set selection [%T selection get]
		DisplayStylesInItem [lindex $selection 0]
	}
}

proc DemoClear {} {

	set T .f2.f1.t

	# Clear the demo list
	$T item delete all

	# Clear all bindings on the demo list added by the previous demo.
	# This is why DontDelete it used for the <Selection> binding.
	foreach pattern [$T notify bind $T] {
		$T notify bind $T $pattern {}
	}

	# Clear all run-time states
	foreach state [$T state names] {
		$T state undefine $state
	}

	# Clear the styles-in-item list
	.f3.t item delete all

	# Delete columns in demo list
	while {[$T numcolumns]} {
		$T column delete 0
	}

	# Delete all styles in demo list
	eval $T style delete [$T style names]

	# Delete all elements in demo list
	eval $T element delete [$T element names]

	$T item hasbutton root no
	$T expand root

	# Restore some happy defaults to the demo list
	$T configure -orient vertical -wrap "" -xscrollincrement 0 \
		-yscrollincrement 0 -itemheight 0 -showheader yes \
		-background white -scrollmargin 0 -xscrolldelay 50 -yscrolldelay 50 \
		-openbuttonimage "" -closedbuttonimage "" -backgroundmode row \
		-treecolumn 0 -indent 19

	# Restore default bindings to the demo list
	bindtags $T [list $T TreeCtrl [winfo toplevel $T] all]

	catch {destroy $T.entry}
	catch {destroy $T.text}

	return
}

#
# Demo: Picture catalog
#
proc DemoPictureCatalog {} {

	set T .f2.f1.t

	$T configure -showroot no -showbuttons no -showlines no \
		-selectmode multiple -orient horizontal -wrap window \
		-yscrollincrement 50 -showheader no

	$T element create elemTxt text -fill {SystemHighlightText {selected focus}}
	$T element create elemSelTxt rect -fill {SystemHighlight {selected focus}}
	$T element create elemSelImg rect -outline {SystemHighlight {selected focus}} \
		-outlinewidth 4
	$T element create elemImg rect -fill gray -width 80 -height 120

	set S [$T style create STYLE -orient vertical]
	$T style elements $S {elemSelImg elemImg elemSelTxt elemTxt}
	$T style layout $S elemSelImg -union elemImg -ipadx 6 -ipady 6
	$T style layout $S elemSelTxt -union elemTxt
	$T style layout $S elemImg -pady {0 6}

	for {set i 1} {$i <= 10} {incr i} {
		set I [$T item create]
		$T item style set $I 0 $S
		$T item text $I 0 "Picture #$i"
		$T item lastchild root $I
	}

	return
}

#
# Demo: Picture catalog
#
proc DemoPictureCatalog2 {} {

	set T .f2.f1.t

	$T configure -showroot no -showbuttons no -showlines no \
		-selectmode multiple -orient horizontal -wrap window \
		-yscrollincrement 50 -showheader no

	$T element create elemTxt text -fill {SystemHighlightText {selected focus}} \
		-justify left -wrap word -lines 2
	$T element create elemSelTxt rect -fill {SystemHighlight {selected focus}}
	$T element create elemSelImg rect -outline {SystemHighlight {selected focus}} \
		-outlinewidth 4
	$T element create elemImg rect -fill gray

	set S [$T style create STYLE -orient vertical]
	$T style elements $S {elemSelImg elemImg elemSelTxt elemTxt}
	$T style layout $S elemSelImg -union elemImg \
		-ipadx 6 -ipady 6
	$T style layout $S elemSelTxt -union elemTxt
	$T style layout $S elemImg -pady {0 6}
	$T style layout $S elemImg -expand n
	$T style layout $S elemTxt -expand s

	for {set i 1} {$i <= 10} {incr i} {
		set I [$T item create]
		$T item style set $I 0 $S
		$T item text $I 0 "This is\nPicture\n#$i"
		$T item element configure $I 0 elemImg -width [expr int(20 + rand() * 80)] \
			-height [expr int(20 + rand() * 120)]
		$T item lastchild root $I
	}

	return
}




proc CursorWindow {} {
	set w .cursors
	if {[winfo exists $w]} {
		destroy $w
	}
	toplevel $w
	set c [canvas $w.canvas -background white -width [expr {50 * 10}] \
		-highlightthickness 0 -borderwidth 0]
	pack $c -expand yes -fill both
	set cursors {
		X_cursor
		arrow
		based_arrow_down
		based_arrow_up
		boat
		bogosity
		bottom_left_corner
		bottom_right_corner
		bottom_side
		bottom_tee
		box_spiral
		center_ptr
		circle
		clock
		coffee_mug
		cross
		cross_reverse
		crosshair
		diamond_cross
		dot
		dotbox
		double_arrow
		draft_large
		draft_small
		draped_box
		exchange
		fleur
		gobbler
		gumby
		hand1
		hand2
		heart
		icon
		iron_cross
		left_ptr
		left_side
		left_tee
		leftbutton
		ll_angle
		lr_angle
		man
		middlebutton
		mouse
		pencil
		pirate
		plus
		question_arrow
		right_ptr
		right_side
		right_tee
		rightbutton
		rtl_logo
		sailboat
		sb_down_arrow
		sb_h_double_arrow
		sb_left_arrow
		sb_right_arrow
		sb_up_arrow
		sb_v_double_arrow
		shuttle
		sizing
		spider
		spraycan
		star
		target
		tcross
		top_left_arrow
		top_left_corner
		top_right_corner
		top_side
		top_tee
		trek
		ul_angle
		umbrella
		ur_angle
		watch
		xterm
	}
	set col 0
	set row 0
	foreach cursor $cursors {
		set x [expr {$col * 50}]
		set y [expr {$row * 40}]
		$c create rectangle $x $y [expr {$x + 50}] [expr {$y + 40}] \
			-fill gray90 -outline black -width 2 -tags $cursor.rect
		$c create text [expr {$x + 50 / 2}] [expr {$y + 4}] -text $cursor \
			-anchor n -width 42 -tags $cursor.text
		if {[incr col] == 10} {
			set col 0
			incr row
		}
		$c bind $cursor.rect <Enter> "
			$c configure -cursor $cursor
			$c itemconfigure $cursor.rect -fill linen
		"
		$c bind $cursor.rect <Leave> "
			$c configure -cursor {}
			$c itemconfigure $cursor.rect -fill gray90
		"
		$c bind $cursor.text <Enter> "
			$c configure -cursor $cursor
		"
		$c bind $cursor.text <Leave> "
			$c configure -cursor {}
		"
	}
	$c configure -height [expr {($row + 1) * 40}]
	return
}

proc compare {i1 i2} {
	if {$i1 < $i2} { return -1 }
	if {$i1 == $i2} { return 0 }
	return 1
}

# A little screen magnifier for X11
if {$::thisPlatform eq "unix"} {

set Loupe(zoom) 3
set Loupe(x) 0
set Loupe(y) 0
set Loupe(auto) 1

proc LoupeAfter {} {

	global Loupe

	set x [winfo pointerx .]
	set y [winfo pointery .]
	if {$Loupe(auto) || ($Loupe(x) != $x) || ($Loupe(y) != $y)} {
		set w [image width $Loupe(image)]
		set h [image height $Loupe(image)]
		loupe $Loupe(image) $x $y $w $h $::Loupe(zoom)
		set Loupe(x) $x
		set Loupe(y) $y
	}
	after $Loupe(delay) LoupeAfter
	return
}

proc MakeLoupeWindow {} {

	global Loupe

	set w [toplevel .loupe]
	wm geometry $w -0+0
	image create photo ImageLoupe -width 150 -height 150
	pack [label $w.label -image ImageLoupe]
	set Loupe(image) ImageLoupe
	set Loupe(delay) 500
	after $Loupe(delay) LoupeAfter
	return
}
MakeLoupeWindow

# unix
}

