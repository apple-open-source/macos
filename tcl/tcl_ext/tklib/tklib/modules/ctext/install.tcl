#Run this with the wish (Tk shell) that you want to install for.
#For example: $ wish8.4 install.tcl
	
proc event.select.install.path win {
	set i [$win curselection]
	set ::installPath [$win get $i]
}

proc install {} {
	set idir [file join $::installPath ctext]
	file mkdir $idir
	file copy -force pkgIndex.tcl $idir
	file copy -force ctext.tcl $idir
	tk_messageBox -icon info -message "Successfully installed into $idir" \
-title {Install Successful} -type ok

	exit
}

proc main {} {
	option add *foreground black
	option add *background gray65
	. config -bg gray65
	
	wm title . {Ctext Installer}
	label .title -text {Welcome to the Ctext installer} -font {Helvetica 14}
	
	message .msgauto -aspect 300 -text {The auto_path directories are automatically searched by Tcl/Tk for packages.  You may select a directory to install Ctext into, or type in a new directory.  Your auto_path directories are:}
	
	set autoLen [llength $::auto_path]
	listbox .listauto -height $autoLen
	
	for {set i 0} {$i < $autoLen} {incr i} {
		.listauto insert end [lindex $::auto_path $i]
	}
	
	bind .listauto <<ListboxSelect>> [list event.select.install.path %W]
	
	label .lipath -text {Install Path:}
	set ::installPath [lindex $::auto_path end]
	entry .installPath -textvariable ::installPath
	
	frame .fcontrol
	frame .fcontrol.finst -relief sunken -bd 1
	pack [button .fcontrol.finst.install -text Install -command install] -padx 4 -pady 4
	button .fcontrol.cancel -text Cancel -command exit
	pack .fcontrol.finst -side left -padx 5
	pack .fcontrol.cancel -side right -padx 5
	
	pack .title -fill x
	pack .msgauto -anchor w
	pack .listauto -fill both -expand 1
	pack .lipath -anchor w
	pack .installPath -fill x
	pack .fcontrol -pady 10
}
main
