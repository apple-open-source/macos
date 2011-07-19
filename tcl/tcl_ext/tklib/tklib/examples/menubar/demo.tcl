#!/bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}


package require Tcl 8.6
package require Tk

package require TclOO

## --
## Extend auto_path so package require will find the menubar package
## in the tklib directory hierarchy.
set selfdir  [file dirname [file normalize [info script]]]
set modules [file join [file dirname [file dirname $selfdir]] modules]
lappend auto_path [file join ${modules} menubar]

package require menubar

# uncomment the following line to enable the debugging menu
# package require menubar::debug

package provide AppMain 0.5

# --
# 
namespace eval Main {

	variable wid
	variable mbar
	variable wid

	proc main { } {
		variable mbar
		variable wid
		set wid 0
		
		wm withdraw .

		##
		## Create a menu bar definition
		##

		# create an instance of the menubar class
		set mbar [menubar new \
			-borderwidth 4 \
			-relief groove  \
			-foreground black \
			-background tan \
			-cursor dot \
			-activebackground red \
			-activeforeground white \
			]

		# define the menu tree for the instance
		${mbar} define {
			File M:file {
			#   Label				 Type	Tag Name(s)	
			#   ----------------- 	 ----	---------
				"New Window"	 	 C 		new
				--					 S 							s0
				"Show Macros Menu"	 C 		mshow
				"Hide Macros Menu"   C 		mhide
				"Toggle Paste State" C 		paste_state
				--					 S 							s1
				Close                C      close
				--					 S 							s2
				Exit			  	 C		exit
			}
			Edit M:items+ {
			#   Label				Type	Tag Name(s)	
			#   ----------------- 	----	---------
				"Cut"				C 		cut
				"Copy"				C 		copy
				"Paste"				C 		paste
				"Scope (buttons)"		S 							s3
				"Global" M:opts+ {
					"CheckButtons"	S							s4
						Apple		X 		apple+
						Bread		X 		bread
						Coffee		X 		coffee
						Donut		X 		donut+
					"RadioButtons"	S							s5
						"Red"		R 		color
						"Green"		R 		color+
						"Blue"		R 		color
						"~!@#%^&*()_+{}: <>?`-=;',./" R color
				}
				"Local" M:opts2 {
					"Default" M:local1+ {
						"CheckButtons"	S						s6
							Square  	X@		square
							Triangle	X@		triangle+
							rectangle	X@		rectangle
						"RadioButtons"	S						s7
							"Magenta"	R@ 		ryb+
							"Yellow"	R@ 		ryb
							"Cyan"		R@ 		ryb
					}
					"Notebook Tab" M:local2+ {
						"CheckButtons"	S						s8
							Right  		X=		right
							Left		X=		left+
							Top			X=		top
						"RadioButtons"	S						s9
							"North"		R= 		compass+
							"South"		R= 		compass
							"East"		R= 		compass
					}
				}
			}
 			Macros M:macros+ {
 			#	Label				Type	Tag Name(s) 
 			#	-----------------	----	---------
				"Add Item" 			C		item_add
				"Delete Item" 		C		item_delete
				"Add MARK Item" 	C		mark_add
				"Move MARK Up"  	C		mark_up
				"Move MARK Down"	C		mark_down
				"Delete MARK"		C		mark_del
				"Macros"		  	C 		macro_entries
				"Save Macros"  		C		serialize
				"Restore Macros" 	C		deserialize
				--COMMANDGROUP--	G		macro
 			}
			Debug M:debug {
			#   Label				Type	Tag Name(s)	
			#   ----------------- 	----	---------
				"Test tag.cget"		C 		testcget
				"Debug Tree"		C 		debug_tree
				"Debug Nodes"		C 		debug_nodes
				"Debug Installs"	C 		debug_installs
				"Debug notebook"	C 		debug_notebook
				"ptree"				C 		ptree
				"pnodes"			C 		pnodes
				"pkeys"				C 		pkeys
			}
			Help M:help {
			#   Label				Type	Tag Name(s)	
			#   ----------------- 	----	---------
				About			  	C 		about
				--					S						s10
				Clear			  	C 		clear
			}
		}
		
		NewWindow

	}
	
	proc NewWindow { args } {
		variable mbar
		variable wid

		# create pathname for new toplevel window
		set w ".top${wid}"
		incr wid
		
		Gui new ${wid} ${w} ${mbar}
	}
}

# --
# 
oo::class create Gui {

	# ----------------------------------------
	# Create a toplevel with a menu bar
	constructor { wid w menubar } {
		my variable mbar
		my variable wtop
		my variable nb
		my variable tout
		my variable tabvars
		
		## 
		## Create toplevel window
		##

		set wtop ${w}
		toplevel ${wtop}
		wm withdraw ${wtop}
		
		##
		## Define the GUI
		##

		# -- note
		# This demo doesn't use the notebook frames.
		# A real application would include gui elements in the
		# notebook frames.

		set nb [ttk::notebook ${wtop}.nb]
		set tout [text ${wtop}.t -height 12]
		grid ${nb} -sticky news
		grid ${tout} -sticky news
		grid rowconfigure ${wtop} 1 -weight 1
		grid rowconfigure ${wtop} 2 -weight 0

		# add binding for notebook tabs
		bind ${nb} "<<NotebookTabChanged>>" [list [self object] nbTabSelect ${wtop}]

		## 
		## Install & Configure the menu bar
		##
		
		set mbar ${menubar}
		
		${mbar} install ${wtop} {

			# Create tags for this windows text widget. They will be 
			# used by the menubar callbacks to direct output to the
			# text widget.
			${mbar} tag.add tout ${tout}
			${mbar} tag.add gui [self object]

			${mbar} menu.configure -command {
				# file menu
				new				{::Main::NewWindow}
				mshow			{my mShow}
				mhide			{my mHide}
				paste_state		{my TogglePasteState}
				close			{my Close}
				exit			{my Exit}
				# Item menu
				cut				{my Edit cut}
				copy			{my Edit copy}
				paste			{my Edit paste}
				# boolean menu
				apple	     	{my BoolToggle}
				bread	     	{my BoolToggle}
				coffee	     	{my BoolToggle}
				donut	     	{my BoolToggle}
				square	     	{my BoolToggle}
				triangle     	{my BoolToggle}
				rectangle     	{my BoolToggle}
				left     		{my NotebookBoolToggle}
				right     		{my NotebookBoolToggle}
				top    			{my NotebookBoolToggle}
				# radio menu
				color	     	{my RadioToggle}
				ryb		     	{my RadioToggle}
				compass	     	{my NotebookRadioToggle}
				# Help menu
				about			{my About}
				clear			{my Clear}
			} -state {
				mhide	    	disabled
				paste	    	disabled
			} -bind {
				exit		{1 Cntl+Q  Control-Key-q}
				cut			{2 Cntl+X  Control-Key-x}
				copy		{0 Cntl+C  Control-Key-c}
				paste		{0 Cntl+V  Control-Key-v}
				apple		{0 Cntl+A  Control-Key-a}
				bread		{0 Cntl+B  Control-Key-b}
				about		0
				clear		{0 {}	  Control-Key-d}
			} -background {
				exit red
			} -foreground {
				exit white
			}


			# change the namespace for commands associated the 
			# 'macros' commands and 'macro' command group
			${mbar} menu.namespace macros ::Macros
			${mbar} menu.namespace macro  ::Macros
			
			# configure the macros menu
			${mbar} menu.configure -command {
				item_add		{NewItem}
				item_delete		{DeleteItem}
				mark_add		{Mark add}
				mark_up			{Mark up}
				mark_down		{Mark down}
				mark_del		{Mark delete}
				macro_entries	{Macros}
				serialize		{Serialize}
				deserialize		{Deserialize}
			} -bind {
				item_add	{0 Cntl+I  Control-Key-i}
				mark_add	{0 Cntl+m  Control-Key-m}
				mark_up		{0 Cntl+U  Control-Key-u}
				mark_down	{0 Cntl+J  Control-Key-j}
				mark_del	{0 Cntl+K  Control-Key-k}
			}
			
			# initally hide the macros menu
			${mbar} menu.hide macros

			# hide the debugging menu unless the package is loaded
			if { [catch {package present menubar::debug}] } {
				${mbar} menu.hide debug
			} else {
				${mbar} menu.configure -command {
					testcget		{my TestCget}
					debug_tree		{my Debug tree}
					debug_nodes		{my Debug nodes}
					debug_installs	{my Debug installs}
					debug_notebook	{my Debug notebook}
					ptree			{my print tree}
					pnodes			{my print nodes}
					pkeys			{my print keys}
				}
			}
		}

		# After the menubar is installed we add 3 tabs
		# to its widget scope.
		my nbNewTab "One"
		my nbNewTab "Two"
		my nbNewTab "Three"

		wm minsize ${wtop} 300 300
		wm geometry ${wtop} 300x300+[expr ${wid}*20]+[expr ${wid}*20]
		wm protocol ${wtop} WM_DELETE_WINDOW [list [self object] closeWindow ${wtop}]
		wm title ${wtop} "Menubar Demo"
		wm focusmodel ${wtop} active
		wm deiconify ${wtop}

		return
	}
	
	method pout { txt } {
		my variable wtop
		my variable mbar
		set tout [${mbar} tag.cget ${wtop} tout]
		${tout} insert end "${txt}\n"
	}
	
	method nbNewTab { text } {
		my variable mbar
		my variable wtop
		my variable nb
		set tabid [${nb} index end]
		incr tabid
		set tabwin ${wtop}.tab${tabid}
		${nb} add [frame ${tabwin}] -text ${text}
		${mbar} notebook.addTabStore ${tabwin}
	}
	
	method nbTabSelect { wtop args } {
		my variable mbar
		my variable nb
		my Clear
		# restore tab values
		set tabwin [${nb} select]
		${mbar} notebook.restoreTabValues ${tabwin}
		my pout "Tab Selected: ${tabwin}"
	}

	method mShow { args } {
		my variable mbar
		${mbar} menu.show macros
		${mbar} menu.configure -state {
			mshow		disabled
			mhide		normal
		}
	}

	method mHide { args } {
		my variable mbar
		${mbar} menu.hide macros
		${mbar} menu.configure -state {
			mshow		normal
			mhide		disabled
		}
	}

	method closeWindow { wtop } {
		my variable mbar
		destroy ${wtop}
		# check to see if we closed the last window
		if { [winfo children .] eq ""  } {
			my Exit
		}
	}

	method Close { args } {
		my closeWindow {*}${args}
	}

	method Exit { args } {
		puts "Goodbye"
		exit
	}

	method Debug { args } {
		my variable wtop
		my variable mbar
		lassign ${args} type
		my Clear
		foreach line [${mbar} debug ${type}] {
			my pout ${line}
		}
	}
	method Clear { args } {
		my variable wtop
		my variable mbar
		set tout [${mbar} tag.cget ${wtop} tout]
		${tout} delete 0.0 end
	}
	
	method TestCget { args } {
		my variable wtop
		my variable mbar
		my Clear
		my pout "user define tag: tout = [${mbar} tag.cget ${wtop} tout]"
		my pout "Command tag: exit -background = [${mbar} tag.cget ${wtop} exit -background]"
		my pout "Checkbutton tag: apple -state = [${mbar} tag.cget ${wtop} apple -state]"
		my pout "Radiobutton tag: color -state = [${mbar} tag.cget ${wtop} color -state]"
		my pout "Cascade tag: chx -background = [${mbar} tag.cget ${wtop} chx -background]"
	}

	method Edit { args } {
		my pout "Edit: [join ${args} {, }]"
	}

	method TogglePasteState { args } {
		my variable mbar
		my pout "TogglePasteState: [join ${args} {, }]"
		lassign ${args} wtop
		set value [${mbar} tag.cget ${wtop} paste -state]
		if { [${mbar} tag.cget ${wtop} paste -state] eq "normal" } {
			${mbar} tag.configure ${wtop} paste -state "disabled" -background {}
		} else {
			${mbar} tag.configure ${wtop} paste -state "normal" -background green
		}
	}

	method BoolToggle { args } {
		my variable wtop
		my variable mbar
		my variable nb
		my pout "BoolToggle: [join ${args} {, }]"
	}

	method RadioToggle { args } {
		my variable wtop
		my variable mbar
		my variable nb
		my pout "RadioToggle: [join ${args} {, }]"
	}

	method NotebookBoolToggle { args } {
		my variable wtop
		my variable mbar
		my variable nb
		my pout "NotebookBoolToggle: [join ${args} {, }]"
		lassign ${args} wtop tag val
		set tabwin [${nb} select]
		${mbar} notebook.setTabValue ${tabwin} ${tag}
	}

	method NotebookRadioToggle { args } {
		my variable wtop
		my variable mbar
		my variable nb
		my pout "NotebookRadioToggle: [join ${args} {, }]"
		lassign ${args} wtop tag val
		set tabwin [${nb} select]
		${mbar} notebook.setTabValue ${tabwin} ${tag}
	}

	method About { args } {
		my pout "MenuBar Demo 0.5"
	}

	method print { args } {
		my variable mbar
		lassign ${args} type wtop
		${mbar} print ${type}
	}
}

# --
# 
namespace eval Macros {

	variable next 0
	variable stream
	variable stream_next

	proc Mark { args } {
		set mbar $::Main::mbar
		
		lassign ${args} action wtop
		set gui [${mbar} tag.cget ${wtop} gui]

		set errno 0
		switch -exact -- ${action} {
		"add"	 {
			set errno [${mbar} group.add macro MARK {Mout "MARK"} Cntl+0 Control-Key-0]
 			if { ${errno} != 0 } {
				${gui} pout "warning: MARK already exists"
 			} else {
 				${mbar} group.configure macro MARK \
 					-background tan \
					-foreground white
 			}
		}
		"delete" {
			set errno [${mbar} group.delete macro MARK]
			if { ${errno} != 0 } {
				${gui} pout  "warning: MARK not found"
			}
		}
		"up"	 {
			set errno [${mbar} group.move up macro MARK]
 			if { ${errno} != 0 } {
				${gui} pout "warning: MARK move up failed"
 			}
		}
		"down"	 {
			set errno [${mbar} group.move down macro MARK]
 			if { ${errno} != 0 } {
				${gui} pout "warning: MARK move down failed"
 			}
		}}
	}
	
	proc NewItem { args } {
		variable next
		if { ${next} == 9 } { return }
		incr next
		set mbar $::Main::mbar
		set errno [${mbar} group.add macro Item${next} "Mout item${next}" Cntl+${next} Control-Key-${next}]
 		if { ${errno} != 0 } {
			lassign ${args} wtop
			set gui [${mbar} tag.cget ${wtop} gui]
			${gui} pout "warning: Item${next} already exists"
 		}	
	}
	
	proc DeleteItem { args } {
		variable next
		set mbar $::Main::mbar
		set item "Item${next}"
		${mbar} group.delete macro ${item}
		if { ${next} > 0 } {
			incr next -1
		}
	}

	proc Macros { args } {
		set mbar $::Main::mbar
		puts "---"
		puts [${mbar} group.entries macro]
	}

	proc Serialize { args } {
		variable next
		variable stream
		variable stream_next
		set mbar $::Main::mbar
		set stream [${mbar} group.serialize macro]
		set stream_next ${next}
		puts "---"
		puts ${stream}
	}

	proc Deserialize { args } {
		variable next
		variable stream
		variable stream_next
		set next ${stream_next}
		set mbar $::Main::mbar
		${mbar} group.deserialize macro ${stream}
	}

	proc Mout { args } {
		set mbar $::Main::mbar
		lassign ${args} action wtop
		set gui [${mbar} tag.cget ${wtop} gui]
		${gui} pout  "Mout: [join ${args} {, }]"
	}
}


Main::main
