# install.tcl --
#
# Generic installer script
# (Adapted from BLT installer script)
#
# Copyright (c) 2001 Zveno Pty Ltd
# http://www.zveno.com/
#
# Zveno makes this software available free of charge for any purpose.
# Copies may be made of this software but all of this notice must be included
# on any copy.
#
# The software was developed for research purposes only and Zveno does not
# warrant that it is error free or fit for any purpose.  Zveno disclaims any
# liability for all claims, expenses, losses, damages and costs any user may
# incur as a result of using, copying or modifying this software.
#
# $Id: install.tcl,v 1.1 2001/08/22 17:45:05 doss Exp $

namespace eval Installer {
    variable datafilename install.data
    variable buildfilename build.data

    variable Package Undefined
    variable Version {Not specified}
    variable Welcome {No welcome message has been provided}
    variable PackageNotes {}

    variable commandList {}
    variable cmdLog {}

    # list of components in package (in declaration order which is also display order)
    variable components {}
    # array of boolean to hold selection checkbox setting
    variable component 

    # array of lists
    variable componentAttrs 
    array set componentAttrs {
	programs	{-recommended}
	scripts         {-recommended}
	binaries	{-recommended}
	documentation-man	{-optional}
	documentation-generic	{-optional}
	demos		{-optional}
	headers		{-optional}
    }

    # array of strings - defaults 
    variable componentDescription
    array set componentDescription {
	programs	{Executable programs}
	scripts		{Script libraries}
	binaries	{Shared and static libraries}
	documentation-generic	{Documentation (html and/or plain text)}
	documentation-man	{Unix-style Manual Pages (nroff format)}
	demos		{Examples and demonstrations}
	headers		{Header files for program development}
    }
    variable currentComponent
    set currentComponent(base) ""
 
    variable files
    array set files {}
    variable totalBytes 0
    variable totalFiles 0

    variable requirements {}

    variable panel 0
    variable panelList {
	Welcome Directory Components Ready Finish
    }

    variable warningMessages {}

    variable macroExt {.in}
    variable starttag "@"
    variable endtag   "@"

    variable shlib {so}
    variable lib {lib}
    variable exe {}

    variable path_reLists
    set path_reLists(VC) [list {(^|/)CVS(/|$)} {(^|/)RCS(/|$)} {,v$}]
    set path_reLists(NONE) {}
    variable ignoreList $::Installer::path_reLists(VC) 
    variable user_ignoreList {}

    variable SrcPath ""
    variable BuildPath ""
}

# 

proc Installer::GetBuildData { paths } {
    variable buildfilename

    foreach dir $paths {
	set filename [file join $dir $buildfilename]
	set teafilename [file join $dir config.status]
	if {! [file exists $filename] } {
	    if { [file exists $teafilename] } {
		puts "Build data file is missing."
		puts "Attempting to build from $teafilename ."
		
		set cmd {
		    set srcId  [open $teafilename "r"]
		    set macros ""
		    while { [ gets $srcId line ] >= 0 && ! [eof $srcId] } {
			if { [regsub {^[ t]*s%@([A-Za-z0-9_]+)@%([^%]+)%.*$} $line \
				{    macro \1 {\2}} line] } {
			    append macros "$line\n"
			}
		    }
		    if [ string length $macros ] {
			set destId [open $filename "w"]
			puts $destId "# build.data --\n#\n"
			
			puts $destId $macros
			puts $destId "\n"
			close $destId
			close $srcId
		    } else {
			close $srcId
			return "No macros found"
		    }
		}
		if { [catch $cmd result] != 0 } {
		    tk_messageBox -parent . -icon error -type ok -title "Build status file Error" \
			    -message "Warning: An error occurred while parsing the build config status data.
		    $result"
		    return 0
		}
		break
	    }
	} else {
	    break
	}
    }
    if {! [file exists $filename] } { return 0 }

#    set slave [interp create buildData]
    set ch [open $filename]
#    if {[catch {interp eval $slave [list eval [read $ch]]} msg]} 
    if {[catch { namespace eval ::TEA [read $ch] } msg]} {
	tk_messageBox -parent . -icon error -type ok -title "Install Error" \
		-message "An error occurred while reading the build configuration data.
	
	Contact vendor for a new installer, reporting this error mesage \"$msg\""
	
	close $ch
	exit 2
    }
    close $ch
    return 1
}


# Installer::GetInstallData --
#
#	Read in configuration data
#
# Arguments:
#	srcdir	source directory
#
# Results:
#	Configuration array populated.
#	Error exits if no data found, or errors in data

proc Installer::GetInstallData srcdir {
    variable datafilename

    set filename [file join $srcdir $datafilename]
    if {![file exists $filename]} {
	set message "Installation configuration data is missing.
Unable to continue installation.

Contact vendor for complete installer"

	if {[catch {tk_messageBox -message $message -icon error -parent . -title "Missing Data" -type ok}]} {
	    puts stderr \n$message\n
	}
	exit 1
    }

    # The installer uses Tk (at the moment).
    # We'll provide a Tcl-only installer later

    if {![info exists ::tk_library]} {
	puts stderr "
This installer requires Tk.

Please restart the installer using wish,
or contact the package vendor.
"
	exit 3
    }

    # Evaluate the install data script in a slave interpreter
    # to protect the main interpreter from corruption

    set slave [interp create installData]
    foreach cmd {
	Package MajorVersion MinorVersion Patchlevel Version
	Require
	Welcome
	Component
	Executable
	File
	Files
	Index
	Description
	Ignore
	IgnorePattern
	BasePath
    } {
	$slave alias $cmd Slave_$cmd $slave
    }

    set ch [open $filename]
    if {[catch {interp eval $slave [list eval [read $ch]]} msg]} {
	tk_messageBox -parent . -icon error -type ok -title "Install Error" \
		-message "An error occurred while preparing the installation configuration data.

Contact vendor for a new installer, and tell them to fix error \"$msg\""

	close $ch
	exit 2
    }
    close $ch

    return {}
}

proc Slave_Package {slave name} {
    $slave eval [list set Package $name]
    set ::Installer::Package $name
}
proc Slave_MajorVersion {slave number} {
    $slave eval [list set MajorVersion $number]
    set ::Installer::MajorVersion $number
}
proc Slave_MinorVersion {slave number} {
    $slave eval [list set MinorVersion $number]
    set ::Installer::MinorVersion $number
}
proc Slave_Patchlevel {slave name} {
    $slave eval [list set Patchlevel $name]
    set ::Installer::Patchlevel $name
}
proc Slave_Version {slave name} {
    $slave eval [list set Version $name]
    set ::Installer::Version $name
}
proc Slave_Require {slave name {number 1.0} {alternatives {}}} {
    if {[catch {package require $name $number}]} { 
        if { [llength $alternatives] } {
	    $slave eval $alternatives
        } else {
	    tk_messageBox -parent . -title {Prerequisite Package Not Installed} -type ok \
	                  -message "Package $name version $number is required by $::Installer::Package , but is not installed .

Install the required package, and then restart this installer."
          exit 4
        }
    } else {
        lappend ::Installer::requirements $name $number
    }
}
proc Slave_Welcome {slave msg {notes ""}} {
    set ::Installer::Welcome $msg 
    set ::Installer::PackageNotes $notes
}
proc Slave_Ignore {slave listName} {
    #set listName [string tolower $listName]
    if {[info exists ::Installer::path_reLists($listName)]} {
	set ::Installer::ignoreList $::Installer::path_reLists($listName)
    }
}
proc Slave_IgnorePattern{slave args} {
    set ::Installer::ignoreList $args
}

proc Slave_BasePath {slave args} {
    foreach attr $args {
	switch -- $attr {
	    -buildpath {  
		set ::Installer::currentComponent(base) $::Installer::BuildPath
		if {! [file exists $::Installer::BuildPath] || ! [ file isdirectory $::Installer::BuildPath] } {
		    return -code error "build directory \"$::Installer::BuildPath\" does not exist"
		}
	    }
	    -reset { 
		set ::Installer::currentComponent(base) "$::Installer::SrcPath"
	    }
	    -info  {
	    }
	    default { 
		if {! [file exists $arg] || ! [ file isdirectory $arg ] } {
		    return -code error "directory $arg does not exist"
		}
		set ::Installer::currentComponent(base) $arg	
	    }
	}
    }
    return $::Installer::currentComponent(base)
}

proc Slave_Component {slave type {args {}}} {
    lappend ::Installer::components $type
    foreach attr $args {
	switch -- $attr {
	    -mandatory {  
		set ::Installer::component($type) 1
	    }
	    -recommended { 
		set ::Installer::component($type) 1
	    }
	    -optional { 
		set ::Installer::component($type) 0
	    }
	}
    } 
    set ::Installer::currentComponent(type) $type 
    set ::Installer::currentComponent(base) "$::Installer::SrcPath"
    if [ llength args ] { set ::Installer::componentAttrs($type) $args }
    return {}
}

proc Slave_Executable {slave filename} {
    if {![info exists ::Installer::currentComponent(type)]} {
	return -code error "no component has been specified"
    }
    if { ![string compare [file pathtype $filename] relative] } {
	set filename [ file join $::Installer::currentComponent(base) $filename ]
    }
    set ignore 0
    foreach re $::Installer::ignoreList {
	if { [set ignore [regexp $re $filename]] } { break } 
    }
    if { ! $ignore } {
	if {[file exists $filename]} {
	    lappend ::Installer::executables($::Installer::currentComponent(type)) $filename
	} else {
	    Warning "Executable file \"$filename\" not found"
	}
    } 
}
proc Slave_File {slave filename} {
    if {![info exists ::Installer::currentComponent(type)]} {
	return -code error "no component has been specified"
    }
    if { ![string compare [file pathtype $filename] relative] } {
	set filename [ file join $::Installer::currentComponent(base) $filename]
    }
    set ignore 0
    foreach re $::Installer::ignoreList {
	if { [set ignore [regexp $re $filename]] } { break } 
    }
    if { ! $ignore } {
	if {[file exists $filename]} {
	    lappend ::Installer::files($::Installer::currentComponent(type)) $filename
	} else {
	    Warning "File \"$filename\" not found"
	}
    }
}

proc Slave_Files {slave pattern} {

    if {![info exists ::Installer::currentComponent(type)]} {
	return -code error "no component has been specified"
    }
    if { ![string compare [file pathtype $pattern] relative] } {
	set pattern [ file join $::Installer::currentComponent(base) $pattern]
    }
    set files [glob -nocomplain $pattern]
    if {[llength $files]} {
	foreach filename $files {
	    set ignore 0
	    foreach re $::Installer::ignoreList {
		if { [set ignore [regexp $re $filename]] } { break } 
	    }
	    if { ! $ignore } {
		    eval lappend [list ::Installer::files($::Installer::currentComponent(type))] $filename
	    }
	}
    } else {
	Warning "No files matching pattern \"$pattern\" were found"
    }
}
proc Slave_Index {slave req} {
    if {![info exists ::Installer::currentComponent(type)]} {
	return -code error "no component has been specified"
    }
    switch -- $req {
	supplied {
	    set ::Installer::pkgindex($::Installer::currentComponent(type)) 0
	}
	needed {
	    set ::Installer::pkgindex($::Installer::currentComponent(type)) 1
	}
	default {
	    return -code error "invalid value \"$req\", must be \"supplied\" or \"needed\""
	}
    }
}
proc Slave_Description {slave desc} {
    if {![info exists ::Installer::currentComponent(type)]} {
	return -code error "no component has been specified"
    }

    if [ string length $desc ] { 
	set ::Installer::componentDescription $desc 
    } else {
	Warning "An empty component description was found"
    }
}

proc Warning msg {
    append ::Installer::warningMessages $msg \n

    ::Installer::Warnings $msg
}

proc Installer::DoInstall { package version } {
    variable currentComponent 

    global tcl_platform
    variable packagedir 
    variable component
    variable components

    variable executables
    variable files

    set packagedir [file join $::TEA::libdir [string tolower $package]-$::Installer::MajorVersion.$::Installer::MinorVersion]

    
    foreach c $components {
	set b $component($c)
	if {$b} {
	    switch $c {
		programs {
		    if { [info exists executables($c)] } {			
			foreach file $executables($c) {
			    Add "" -perm 0755 \
				    -file $file \
				    $::TEA::bindir       ;# [file join $prefix bin]
			}
		    }
		}
		binaries {
		    if { [info exists files($c)] } {
			foreach file $files($c) {
			    Add "" -perm 0644 \
				    -file $file \
				    $::TEA::libdir       ;# [file join $prefix lib]
			}
		    }
		}
		headers {
		    if { [info exists files($c)] } {
			foreach file $files($c) {
			    Add "" -perm 0644 \
				    -file $file \
				    $::TEA::includedir   ;# [file join $prefix $include]
			}
		    }
		}
		documentation-generic {		   
		    if { [info exists files($c)] } {
			foreach file $files($c) {
			    switch [file extension $file] {
				default {
				    Add "" -perm 0644 \
					    -file $file \
					    [file join $::TEA::prefix doc]
				}
			    }
			}
		    }
		}
		documentation-man {
		    if { [info exists files($c)] } {
			foreach file $files($c) {
			    set section [ string trimleft [file extension $file] "." ] 
			    Add ""  -perm 0644 \
				    -file $file \
				    [file join $::TEA::mandir man$section]
			}
		    }
		}
		scripts {
		    if { [info exists files($c)] } {
			foreach file $files($c) {
			    Add "" -perm 0755 \
				    -file $file \
				    $packagedir
			}
		    }
		}
		demos { 
		    if { [info exists files($c)] } {
			foreach file $files($c) {
			    Add "" -perm 0644 \
				    -file $file \
				    [file join $packagedir demos]
			}
		    }
		    if { [info exists executables($c)] } {
			foreach file $executables($c) {
			    Add ""  -perm 0755 \
				    -file $file \
				    [file join $packagedir demos]
			}
		    }
		}
	    }
	}
    }
    
    Install $package $version
}


proc Installer::InstallDirectory { dest } {
    variable commandList
    lappend commandList [list CheckPath $dest]
}

proc Installer::UpdateCopy { src dest size perm } {
    variable currentBytes
    variable totalBytes

    .install.text insert end "copy $src --> $dest\n"
    if { [catch {file copy -force $src $dest} results] != 0 } {
	.install.text insert end "Error: $results\n" fail
    } else {
	incr currentBytes $size
    }
    Update $src $dest $size $perm
}

proc Installer::Update { src dest size perm } {
    variable currentBytes
    variable totalBytes

    global tcl_platform
    if { $tcl_platform(platform) == "unix" } {
	.install.text insert end "set $dest permissions to $perm\n"
	if { [catch {file attributes $dest -permissions $perm} results] != 0 } {
	    .install.text insert end "Error: $results\n" fail
	}
    }
    set percent [expr round(double($currentBytes)/$totalBytes * 100.0)]
    .install.current configure -text "$percent% complete"
    update
}

proc Installer::InstallFile { src dest perm } {
    variable commandList
    variable totalBytes
    variable totalFiles

    if { [catch { file size $src } size ] != 0 } {
	set size 0
    }
    lappend commandList [list UpdateCopy $src $dest $size $perm]
    incr totalBytes $size
    incr totalFiles
}

proc Installer::Add { dir args } {
    variable commandList 
    variable totalBytes

    if { [string length $dir] && ![file exists $dir] }  {
	error "can't find directory \"$dir\"" 
    }
    set argc [llength $args]
    set destDir [lindex $args end]
    incr argc -2
    set perm 0644
    set macroExt $::Installer::macroExt

    InstallDirectory $destDir
 
    # preconfigure actions options
    foreach { option value } [lrange $args 0 $argc] {
	switch -- $option {
    	    "-perm" {  
		set perm $value
	    }
    	    "-macroExt" {  
		set macroExt $value 
	    }
    	    "-preprocess" {  
		set preprocessCmd "$value" 
	    }
    	    "-postprocess" {  
		set postprocessCmd "$value" 
	    }	  
	    default {
		#error "Unknown option \"$option\""
	    }
	}
    }


    # launch approriate action
    foreach { option value } [lrange $args 0 $argc] {
	switch -- $option {
	    "-pattern" {  
		foreach f [lsort [glob [file join $dir $value]]] {
		    if { [string length $macroExt] \
			    && [regexp "$macroExt\$" $f] } {
			set destFile [file tail [file rootname $f]]
			set installCmd InstallTemplatedFile 
		    } else {
			set destFile [file tail $f]
			set installCmd InstallFile 
		    }
		    $installCmd $f [file join $destDir $destFile] $perm
		}
	    }
	    "-rename" {
		set src [lindex $value 0]
		set dest [lindex $value 1]
		if { [string length $macroExt] \
			&& [regexp "$macroExt\$" $src] } {
		    set installCmd InstallTemplatedFile 
		} else {
		    set installCmd InstallFile 
		}  
		$installCmd [file join $dir $src] [file join $destDir $dest] $perm
	    }
    	    "-file" {
		if { [string length $macroExt] \
			&& [regexp "$macroExt\$" $value] } {
		    set destFile [file tail [file rootname $value]]
		    set installCmd InstallTemplatedFile 
		} else {
		    set destFile [file tail $value]
		    set installCmd InstallFile 
		}  
		$installCmd [file join $dir $value] [file join $destDir $destFile] $perm
	    }
	}
    }
}

proc Installer::CheckPath { dest } {
    set save [pwd]
    if { [file pathtype $dest] == "absolute" } {
	if { [string match  {[a-zA-Z]:*} $dest] } {
	    cd [string range $dest 0 2]
	    set dest [string range $dest 3 end]
	} else {
	    cd /
	    set dest [string range $dest 1 end]
	}
    }	
    set dirs [file split $dest]
    foreach d $dirs {
	if { ![file exists $d] } {
	    .install.text insert end "create directory $d\n"
	    if { [catch { file mkdir $d } result] != 0 } {
		.install.text insert end "Error: $result\n" fail
		break
	    }
	}
	if { ![file isdirectory $d] } {
	    .install.text insert end "Error: Not a directory: \"$d\"" fail
	    break
	}
	update
	cd $d 
    }
    cd $save
}

# substitute for macros in template file while installing
# initial args must mirror InstallFile args
 
proc Installer::InstallTemplatedFile { src dest perm {subslist {$::TEA::macronames}} } {
    variable commandList
    variable totalBytes
    variable totalFiles

    if { [catch { file size $src } size ] != 0 } {
	set size 0
    }
    lappend commandList [list UpdateTemplatedCopy $src $dest $size $perm subslist]
    incr totalBytes $size
    incr totalFiles
}

proc Installer::UpdateTemplatedCopy { src dest size perm macroArray} {
    upvar $macroArray macros
    variable currentBytes
    variable totalBytes
    variable starttag
    variable endtag

    set cmd {
	set srcId [open $src "r"]
	set destId [open $dest "w"]
 
	while { [ gets $srcId line ] >= 0 && ! [eof $srcId] } {
	    regsub -all {\$} $line {\\$} line
	    set newline $line
	    if { [regsub -all ${starttag}(\[A-Za-z0-9_\-\]+)${endtag} \
		    $line {$::TEA::\1} line ] } {
		catch { 
		    set newline [ subst  -nocommands  $line ] 
		} result
	    }	    
	    regsub -all {\\\$} $newline {$} line
	    puts $destId $line
	}
        close $srcId
	close $destId
    }

    if { [catch $cmd result] != 0 } {
	.install.text insert end "Error: $result\n" fail
    } else {
	.install.text insert end "created file $dest from template $src  \n"
	incr currentBytes [file size $dest]
    }

    Update $src $dest $size $perm
}

proc Installer::MakePackageIndex { package version file } {

    set prefix $::TEA::prefix
    set suffix [info sharedlibextension]

    regsub {\.} $version {} version_no_dots
    set libName "${package}${version_no_dots}${suffix}"
    set libPath [file join ${prefix} bin $libName]
    set cmd [list load $libPath $package]

    if { [file exists $file] } {
	file delete $file
    }
    set cmd {
	set fid [open $file "w"] 
	puts $fid "# Package Index for $package"
	puts $fid "# generated on [clock format [clock seconds]]"
	puts $fid ""
	puts $fid [list package ifneeded $package $version $cmd]
	close $fid
    }
    if { [catch $cmd result] != 0 } {
	.install.text insert end "Error: $result\n" fail
    }
}

proc Installer::SetRegistryKey { package version valueName } {
    variable packagedir 
    global tcl_version
    
    package require registry
    set key HKEY_LOCAL_MACHINE\\Software\\$package\\$version\\$tcl_version 
    registry set $key $valueName $packagedir
}

proc Installer::Install { package version } {
    variable commandList 
    variable totalBytes 
    variable currentBytes 0
    variable totalFiles
    variable packagedir 

    .install.totals configure -text "Files: $totalFiles Size: $totalBytes"
    foreach cmd $commandList {
	if { ![winfo exists .install] } {
	    return 
	}
	if { [catch $cmd result] != 0 } {
	    .install.text insert end "Error: $result\n" fail
	} 
	update
    }
    global tcl_version tcl_platform
    set prefix $::TEA::prefix
    set name [string tolower $package]
    if {[info exists ::Installer::pkgindex(scripts)] } {
	if { $::Installer::pkgindex(scripts)} {
	    MakePackageIndex $package $version [file join $packagedir pkgIndex.tcl]
#	    MakePackageIndex $package $version \
#		[file join $prefix lib tcl${tcl_version} ${name}${version} pkgIndex.tcl]
        } else {
	    
	}
    }
    if { $tcl_platform(platform) == "windows" } {
	SetRegistryKey $package $version ${package}_LIBRARY
    }
    .install.cancel configure -text "Done"
}

proc Installer::Next {} {
    variable panel
    variable continue
    variable panelList

    incr panel
    set max [llength $panelList]
    if { $panel >= $max } {
	exit 0
    }
    if { ($panel + 1) == $max } {
	.next configure -text "Finish"
	.cancel configure -state disabled
    } else {
	.next configure -text "Next"
    }
    if { $panel > 0 } {
	.back configure -state normal
    }
    set continue 1
}

proc Installer::Back {} {
    variable panel
    variable continue
    
    incr panel -1 
    if { $panel <= 0 } {
	.back configure -state disabled
	set panel 0
    } else {
	.back configure -state normal
    }
    .next configure -text "Next"
    .cancel configure -state normal
    set continue 0
}

proc Installer::Help {} {
 
   if { [winfo exists .helppopup] } {
	raise .helppopup
	wm deiconify .helppopup
	return
    }

    toplevel .helppopup
    text .helppopup.text -wrap word -width 40 -height 20 \
	-relief flat -padx 4 -pady 4 -cursor arrow \
	-background [. cget -bg]
    button .helppopup.ok -text "Done" -command {
	#grab release .helppopup
	destroy .helppopup
    }
    scrollbar .helppopup.sbar -command { .helppopup.text yview }
    frame .helppopup.sep -height 2 -borderwidth 1 -relief sunken

    wm protocol .helppopup WM_DELETE_WINDOW { .helppopup.ok invoke }
    #wm transient .helppopup .
    wm title .helppopup "Package Installer Help"

    grid .helppopup.text -row 0 -column 0 -sticky ewns -padx 4 -pady 4 
    grid .helppopup.sbar -row 0 -column 1 -sticky nsw 
    grid .helppopup.ok   -row 1 -column 0 -columnspan 2  -padx 4 -pady 4 

    grid rowconfigure    .helppopup 0 -weight 1
    grid rowconfigure    .helppopup 1 -weight 0
    grid columnconfigure .helppopup 0 -weight 1
    grid columnconfigure .helppopup 1 -weight 0


    wm withdraw .helppopup
    after idle { 
	Installer::CenterPanel .helppopup 
	#grab set .helppopup
    } 

#    MakeLink .helppopup next "Installer::Next"
#    MakeLink .helppopup cancel "Installer::Cancel"

    .helppopup.text insert end \
        "\nPress the \"Next\" button to continue.\n" \
        "Press the \"Back\" button to retrace your steps.\n" \
        "Press the \"Cancel\" button if you do not wish to install $::Installer::Package at this time."
}

proc Installer::Cancel {} {
    exit 0
}

proc Installer::MakeLink { widget tag command } {
    $widget tag configure $tag -foreground blue -underline yes 
    $widget tag bind $tag <ButtonRelease-1> \
	"$command; $widget tag configure $tag -foreground blue"
    $widget tag bind $tag <ButtonPress-1> \
	[list $widget tag configure $tag -foreground red]
}

proc Installer::Welcome { package version }  {
    global tcl_version 
    variable Welcome
    variable PackageNotes
    variable requirements

    if { [winfo exists .panel] } {
	destroy .panel
    }
    text .panel -wrap word -width 10 -height 18 \
	-relief flat -padx 4 -pady 4 -cursor arrow \
	-background [. cget -bg]

    .panel tag configure text    -justify center
    .panel tag configure welcome -font titleFont -justify center \
	                         -foreground navyblue
    .panel tag configure package -font hugeFont  -justify center \
                                 -foreground red
    .panel tag configure reqlist -font fixedFont -justify center \
	                         -foreground navyblue 

    .panel insert end "Welcome!\n" welcome
    .panel insert end \n text
    .panel insert end "$package Package" package
    .panel insert end \n\n text
    .panel insert end "Version $version" package
    .panel insert end \n\n\n text
    .panel insert end "$Welcome\n" text

    if {[llength $requirements]} {
	.panel insert end "\nTo use this package the following packages must also be installed:\n\n" text
    }
    foreach {name number} $requirements {
	.panel insert end "$name v$number " reqlist
	.panel insert end "(or later)\n" text
    }

    if {[llength PackageNotes]} {
        .panel insert end "\n$PackageNotes" text
    }

    .panel configure -state disabled
    grid .panel -in . -column 1 -row 0 -columnspan 4 -pady 5 -padx 5 -sticky news
    tkwait variable Installer::continue
}

proc Installer::OpenDir { widget path atnode } {
    set save [pwd]
    global tcl_platform
	
    if { $tcl_platform(platform) == "windows" } {
	if { $path == "/" } {
	    foreach v [file volumes] {
		if { ![string match {[A-B]:/} $v] } {
		    .browser.h insert end $v -button yes
		}
	    }
	    return
	}
	set path [string trimleft $path /]
    } 
    cd $path/
    foreach dir [lsort [glob -nocomplain */ ]] { 
	set node [$widget insert -at $atnode end $dir]
	# Does the directory have subdirectories?
	set subdirs [glob -nocomplain $dir/*/ ]
	if { $subdirs != "" } {
	    $widget entry configure $node -button yes
	} else {
	    $widget entry configure $node -button no
	}	
    }
    cd $save
}

# Installer::CenterPanel --
#
#	Redundant because toplevel is fixed in size

proc Installer::CenterPanel { panel }  {
    update idletasks

    set x [expr ([winfo width .] - [winfo reqwidth $panel]) / 2]
    set y [expr ([winfo height .] - [winfo reqheight $panel]) / 2]
    incr x [winfo rootx .]
    incr y [winfo rooty .]
    wm geometry $panel +$x+$y
    wm deiconify $panel
}

# Installer::CenterAndSizeToplevel --
#
#	Centers the toplevel window on the screen and
#	fixes its size
#
# Arguments:
#	t	toplevel window
#
# Results:
#	Window geometry set

proc Installer::CenterAndSizeToplevel { t } {
    set width [expr [winfo screenwidth $t] / 2]
    set height [expr [winfo screenheight $t]/ 2]
    set x [expr [winfo screenwidth $t] / 4]
    set y [expr [winfo screenheight $t] / 4]
    wm geometry $t ${width}x${height}+$x+$y
    wm deiconify $t

    return {}
}

proc Installer::Browse { dir }  {   
    global tcl_platform 

    if { [file exists $dir] } {
	set root $dir
    } else {
	set root "/"
    }

    if { $::tcl_version < 8.3 } {
	set chooser tk_getDirectory
    } else {
	set chooser tk_chooseDirectory
    }
    set dir [
      $chooser \
        -title "Installer: Select Install Directory" \
        -initialdir $root ]

    if [ string is graph -strict $dir ] { 
	return $dir
    }
    return $root

}


#########################################################
# Directory Selector - version 1.5
#
# Daniel Roche, <daniel.roche@bigfoot.com>
#
# thanks to :
#  Cyrille Artho <cartho@netlink.ch> for the 'saving pwd fix'
#  Terry Griffin <terryg@axian.com> for <Return> key bindings on buttons.
#  Kenneth Kehl  <Kenneth.Kehl@marconiastronics.com> for blocking at end of dir tree
#  Michael Barth <m.barth@de.bosch.com> for finding the "-fill on image" problem 
#  Mike Avery <avery@loran.com> for finding the "myfont already exist" problem
#  Branko Rogic <b.rogic@lectra.com> for gif icons, parent and background options
#  Reinhard Holler <rones@augusta.de> for colors ,font and autovalid options
#
#########################################################


#########################################################
# 
# tk_getDirectory [option value ...]
#
#  options are :
#   [-initialdir dir]     display in dir
#   [-title string]       make string title of dialog window
#   [-ok string]          make string the label of OK button
#   [-open string]        make string the label of OPEN button
#   [-cancel string]      make string the label of CANCEL button
#   [-msg1 string]        make string the label of the first directory message
#   [-msg2 string]        make string the label of the second directory message
#   [-parent string]      make string the parent of tkgetdir for transient window
#   [-geometry string]    make string the geometry (WxH+X+Y)
#   [-listbg string]      make string the list and entry background color
#   [-listfg string]      make string the list and entry foreground color
#   [-selectbg string]    make string the background color for selected directory
#   [-selectfg string]    make string the foreground color for selected directory
#   [-hilightbg string]   make string the background color for highlighted dir
#   [-hilightfg string]   make string the foreground color for highlighted dir
#   [-endcolor string]    make string the color for the last directory flag
#   [-autovalid]          if it is set : selecting no directory in the window
#                          will return the directory the window shows currently
#   [-font string]        make string the font name or font description
#                          for directory window
#
#########################################################

namespace eval tkgetdir {
    variable drives
    variable fini
    variable svwd
    variable msg
    variable geometry
    variable parent
    variable colors
    variable font
    variable myfont
    variable autovalid
    
    namespace export tk_getDirectory
}

proc tkgetdir::tk_getDirectory {args} {
    global tcl_platform

    #
    # default
    #
    set tkgetdir::msg(title) "Directory Selector"
    set tkgetdir::msg(ldir) "Directory:"
    set tkgetdir::msg(ldnam) "Directory Name:"
    set tkgetdir::msg(open) " Ok "
    set tkgetdir::msg(expand) "Open"
    set tkgetdir::msg(cancel) "Cancel"
    set tkgetdir::geometry "500x250"
    set tkgetdir::colors(lstfg) "#000000"
    if [string compare $tcl_platform(platform) windows] {
	set tkgetdir::colors(lstbg) ""
	set tkgetdir::font {-family lucida -size 14 -weight bold}
    } else {
	set tkgetdir::colors(lstbg) "#FFFFFF"
	set tkgetdir::font {-family courier -size 10}
    }
    set tkgetdir::colors(hilfg) "#FFFF00"
    set tkgetdir::colors(hilbg) "#808080"
    set tkgetdir::colors(selfg) "#FFFFFF"
    set tkgetdir::colors(selbg) "#000080"
    set tkgetdir::colors(endcol) "#FF8000"
    set tkgetdir::parent ""
    set tkgetdir::autovalid 0
    set tkgetdir::svwd [pwd]
    
    #
    # arguments
    #
    set ind 0
    set max [llength $args]
    while { $ind < $max } {
	switch -exact -- [lindex $args $ind] {
	    "-initialdir" {
		incr ind
		cd [lindex $args $ind]
		incr ind
	    }
	    "-title" {
		incr ind
		set tkgetdir::msg(title) [lindex $args $ind]
		incr ind
	    }
	    "-ok" {
		incr ind
		set tkgetdir::msg(open) [lindex $args $ind]
		incr ind
	    }
	    "-open" {
		incr ind
		set tkgetdir::msg(expand) [lindex $args $ind]
		incr ind
	    }
	    "-cancel" {
		incr ind
		set tkgetdir::msg(cancel) [lindex $args $ind]
		incr ind
	    }
	    "-msg1" {
		incr ind
		set tkgetdir::msg(ldir) [lindex $args $ind]
		incr ind
	    }
	    "-msg2" {
		incr ind
		set tkgetdir::msg(ldnam) [lindex $args $ind]
		incr ind
	    }
	    "-geometry" {
		incr ind
		set tkgetdir::geometry [lindex $args $ind]
		incr ind
	    }
	    "-parent" {
		incr ind
		set tkgetdir::parent [lindex $args $ind]
		incr ind
	    }
	    "-listfg" {
		incr ind
		set tkgetdir::colors(lstfg) [lindex $args $ind]
		incr ind
	    }
	    "-listbg" {
		incr ind
		set tkgetdir::colors(lstbg) [lindex $args $ind]
		incr ind
	    }
	    "-selectfg" {
		incr ind
		set tkgetdir::colors(selfg) [lindex $args $ind]
		incr ind
	    }
	    "-selectbg" {
		incr ind
		set tkgetdir::colors(selbg) [lindex $args $ind]
		incr ind
	    }
	    "-hilightfg" {
		incr ind
		set tkgetdir::colors(hilfg) [lindex $args $ind]
		incr ind
	    }
	    "-hilightbg" {
		incr ind
		set tkgetdir::colors(hilbg) [lindex $args $ind]
		incr ind
	    }
	    "-endcolor" {
		incr ind
		set tkgetdir::colors(endcol) [lindex $args $ind]
		incr ind
	    }
	    "-autovalid" {
		incr ind
		set tkgetdir::autovalid 1
	    }
	    "-font" {
		incr ind
		set tkgetdir::font [lindex $args $ind]
		incr ind
	    }
	    default {
		puts "unknown option [lindex $args $ind]"
		return ""
	    }
	}
    }
    
    #
    # variables et data
    #
    set tkgetdir::fini 0
    
    image create photo b_up -data {
	R0lGODlhFgATAMIAAHt7e9/fX////gAAAK6uSv///////////yH+Dk1hZGUgd2l0aCBHSU1QACH5
	BAEAAAcALAAAAAAWABMAAANVeArcoDBKEKoNT2p6b9ZLJzrkAQhoqq4qMJxi3LnwRcjeK9jDjWM6
	C2FA9Mlou8CQWMQhO4Nf5XmJSqkW6w9bYXqZFq40HBzPymYyac1uDA7fuJyZAAA7
    }

    image create photo b_dir -data {
	R0lGODlhEAAQAMIAAHB/cN/fX////gAAAP///////////////yH+Dk1hZGUgd2l0aCBHSU1QACH5
	BAEAAAQALAAAAAAQABAAAAM2SLrc/jA2QKkEIWcAsdZVpQBCaZ4lMBDk525r+34qK8x0fOOwzfcy
	Xi2IG4aOoRVhwGw6nYQEADs=
    }

    if {[lsearch -exact $tkgetdir::font -family] >= 0} {
	eval font create dlistfont $tkgetdir::font
	set tkgetdir::myfont dlistfont
    } else {
	set tkgetdir::myfont $tkgetdir::font
    }

    #
    # widgets
    #
    toplevel .dirsel
    grab set .dirsel
    wm geometry .dirsel $tkgetdir::geometry
    if {$tkgetdir::parent != ""} {
        set par $tkgetdir::parent
	set xOrgWin [expr [winfo rootx $par] + [winfo width $par] / 2 ]
	set yOrgWin [expr [winfo rooty $par] + [winfo height $par] / 2 ]
	wm geometry .dirsel +$xOrgWin+$yOrgWin
	wm transient .dirsel $tkgetdir::parent
    }
    wm title .dirsel $tkgetdir::msg(title)

    event add <<RetEnt>> <Return> <KP_Enter>
    
    frame .dirsel.f1 -relief flat -borderwidth 0
    frame .dirsel.f2 -relief sunken -borderwidth 2 
    frame .dirsel.f3 -relief flat -borderwidth 0
    frame .dirsel.f4 -relief flat -borderwidth 0
    
    pack .dirsel.f1 -fill x
    pack .dirsel.f2 -fill both -expand 1 -padx 6 -pady 6
    pack .dirsel.f3 -fill x
    pack .dirsel.f4 -fill x
    
    label .dirsel.f1.lab -text $tkgetdir::msg(ldir)
    menubutton .dirsel.f1.dir -relief raised -indicatoron 1 -anchor w \
	    -menu .dirsel.f1.dir.m
    menu .dirsel.f1.dir.m -tearoff 0
    button .dirsel.f1.up -image b_up -command tkgetdir::UpDir
    bind .dirsel.f1.up <<RetEnt>> {.dirsel.f1.up invoke}

    pack .dirsel.f1.up -side right -padx 4 -pady 4
    pack .dirsel.f1.lab -side left -padx 4 -pady 4
    pack .dirsel.f1.dir -side right -padx 4 -pady 4 -fill x -expand 1
    
    canvas .dirsel.f2.cv -borderwidth 0 -yscrollcommand ".dirsel.f2.sb set"
    if { $tkgetdir::colors(lstbg) != "" } {
	.dirsel.f2.cv configure -background $tkgetdir::colors(lstbg)
    }
    scrollbar .dirsel.f2.sb -command ".dirsel.f2.cv yview"
    set scw 16
    place .dirsel.f2.cv -x 0 -relwidth 1.0 -width [expr -$scw ] -y 0 \
	    -relheight 1.0
    place .dirsel.f2.sb -relx 1.0 -x [expr -$scw ] -width $scw -y 0 \
	    -relheight 1.0
    unset scw
    
    .dirsel.f2.cv bind TXT <Any-Enter> tkgetdir::EnterItem
    .dirsel.f2.cv bind TXT <Any-Leave> tkgetdir::LeaveItem
    .dirsel.f2.cv bind TXT <Any-Button> tkgetdir::ClickItem
    .dirsel.f2.cv bind TXT <Double-Button> tkgetdir::DoubleClickItem
    .dirsel.f2.cv bind IMG <Any-Enter> tkgetdir::EnterItem
    .dirsel.f2.cv bind IMG <Any-Leave> tkgetdir::LeaveItem
    .dirsel.f2.cv bind IMG <Any-Button> tkgetdir::ClickItem
    .dirsel.f2.cv bind IMG <Double-Button> tkgetdir::DoubleClickItem
    
    label .dirsel.f3.lnam -text $tkgetdir::msg(ldnam)
    entry .dirsel.f3.chosen -takefocus 0
    if { $tkgetdir::colors(lstbg) != "" } {
	.dirsel.f3.chosen configure -background $tkgetdir::colors(lstbg)
    }
    pack .dirsel.f3.lnam -side left -padx 4 -pady 4
    pack .dirsel.f3.chosen -side right -fill x -expand 1 -padx 4 -pady 4
    
    button .dirsel.f4.open -text $tkgetdir::msg(open) -command { 
	set tmp [.dirsel.f3.chosen get]
	if { ( [ string length $tmp ] ) || ( $tkgetdir::autovalid == 1 ) } {
	    set tkgetdir::fini 1 
	}
    }
    bind .dirsel.f4.open <<RetEnt>> {.dirsel.f4.open invoke}

    button .dirsel.f4.expand -text $tkgetdir::msg(expand) -command tkgetdir::DownDir
    bind .dirsel.f4.expand <<RetEnt>> {.dirsel.f4.expand invoke}

    button .dirsel.f4.cancel -text $tkgetdir::msg(cancel) -command { 
	set tkgetdir::fini -1 
    }
    bind .dirsel.f4.cancel <<RetEnt>> {.dirsel.f4.cancel invoke}
    
    pack .dirsel.f4.open .dirsel.f4.expand -side left -padx 10 -pady 4
    pack .dirsel.f4.cancel -side right -padx 10 -pady 4
    
    bind .dirsel.f1 <Destroy> tkgetdir::CloseDirSel

    #
    # realwork
    #
    tkgetdir::ShowDir [pwd]
    
    #
    # wait user
    #
    tkwait variable tkgetdir::fini

    if { $tkgetdir::fini == 1 } {
	set curdir [.dirsel.f1.dir cget -text]
	set nnam [.dirsel.f3.chosen get]
	set retval [ file join $curdir $nnam ]
    } else {
	set retval ""
    }
    
    destroy .dirsel
    
    return $retval
}

proc tkgetdir::CloseDirSel {} {
    set wt [font names]
    if {[lsearch -exact $wt dlistfont] >= 0} {
	font delete dlistfont 
    }
    event delete <<RetEnt>>
    cd $tkgetdir::svwd
    set tkgetdir::fini 0
}

proc tkgetdir::ShowDir {curdir} {

    global tcl_platform
    variable drives 
    
    cd $curdir
    .dirsel.f1.dir configure -text $curdir
    
    set hi1 [font metrics $tkgetdir::myfont -linespace]
    set hi2 [image height b_dir]
    if { $hi1 > $hi2 } {
	set hi $hi1
    } else {
	set hi $hi2
    }
    set wi1 [image width b_dir]
    incr wi1 4
    set wi2 [winfo width .dirsel.f2.cv]
    
    set lidir [list]
    foreach file [ glob -nocomplain * ] {
	if [ file isdirectory [string trim $file "~"] ] { 
	    lappend lidir $file
	}
    }
    set sldir [lsort $lidir]
    
    .dirsel.f2.cv delete all
    set ind 0
    # Adjust the position of the text wi1 with an offset.
    if { $hi1 < $hi2 } {
	set offset [expr $hi2 - $hi1]
    } else {
	set offset 0
    }
    foreach file $sldir {
	if [ file isdirectory $file ] { 
	    .dirsel.f2.cv create image 2 [expr $ind * $hi] \
		    -anchor nw -image b_dir -tags IMG
	    .dirsel.f2.cv create text $wi1 [expr ($ind * $hi) + $offset] \
		    -anchor nw -text $file -fill $tkgetdir::colors(lstfg) \
		    -font $tkgetdir::myfont -tags TXT
	    set ind [ expr $ind + 1 ]
	}
    }

    set ha [expr $ind * $hi]
    .dirsel.f2.cv configure -scrollregion [list 0 0 $wi2 $ha]
    
    set curlst [file split $curdir]
    set nbr [llength $curlst]
    
    .dirsel.f1.dir.m delete 0 last
    incr nbr -2
    for {set ind $nbr} {$ind >= 0} {incr ind -1} {
	set tmplst [ lrange $curlst 0 $ind] 
	set tmpdir [ eval file join $tmplst] 
	.dirsel.f1.dir.m add command -label $tmpdir \
		-command "tkgetdir::ShowDir {$tmpdir}"
    }
    if {[info exist drives] == 0} {
	set drives [file volume]
    }
    if ![string compare $tcl_platform(platform) windows] {
	foreach drive $drives {
	    .dirsel.f1.dir.m add command -label $drive \
		    -command "tkgetdir::ShowDir {$drive}"
	}
    }
    
}

proc tkgetdir::UpDir {} {
    set curdir [.dirsel.f1.dir cget -text]
    set curlst [file split $curdir]
    
    set nbr [llength $curlst]
    if { $nbr < 2 } {
	return
    }
    set tmp [expr $nbr - 2]
    
    set newlst [ lrange $curlst 0 $tmp ]
    set newdir [ eval file join $newlst ]
    
    .dirsel.f3.chosen delete 0 end
    tkgetdir::ShowDir $newdir
}

proc tkgetdir::DownDir {} {
    set curdir [.dirsel.f1.dir cget -text]
    set nnam [.dirsel.f3.chosen get]

    set newdir [ file join $curdir $nnam ]

    # change 07/19/99
    # If there are more dirs, permit display of one level down.
    # Otherwise, block display and hilight selection in red.
    set areDirs 0
    foreach f [glob -nocomplain [file join $newdir *]] {
	if {[file isdirectory $f]} {
	    set areDirs 1
	    break
	}
    }
 
    if {$areDirs} {
	.dirsel.f3.chosen delete 0 end
	tkgetdir::ShowDir $newdir
    } else {
	set id [.dirsel.f2.cv find withtag HASBOX ]
	.dirsel.f2.cv itemconfigure $id -fill $tkgetdir::colors(endcol)
    }
}

proc tkgetdir::EnterItem {} {
    global tcl_platform

    set id [.dirsel.f2.cv find withtag current]
    set wt [.dirsel.f2.cv itemcget $id -tags]
    if {[lsearch -exact $wt IMG] >= 0} {
	set id [.dirsel.f2.cv find above $id]
    }

    .dirsel.f2.cv itemconfigure $id -fill $tkgetdir::colors(hilfg)
    set bxr [.dirsel.f2.cv bbox $id]
    eval .dirsel.f2.cv create rectangle $bxr \
	    -fill $tkgetdir::colors(hilbg) -outline $tkgetdir::colors(hilbg) \
	    -tags HILIT
    .dirsel.f2.cv lower HILIT
}

proc tkgetdir::LeaveItem {} {
    .dirsel.f2.cv delete HILIT
    set id [.dirsel.f2.cv find withtag current]
    set wt [.dirsel.f2.cv itemcget $id -tags]
    if {[lsearch -exact $wt IMG] >= 0} {
	set id [.dirsel.f2.cv find above $id]
    }
    set wt [.dirsel.f2.cv itemcget $id -tags]
    if {[lsearch -exact $wt HASBOX] >= 0} {
	.dirsel.f2.cv itemconfigure $id -fill $tkgetdir::colors(selfg)
    } else {
	.dirsel.f2.cv itemconfigure $id -fill $tkgetdir::colors(lstfg)
    }
}

proc tkgetdir::ClickItem {} {
    .dirsel.f2.cv delete HILIT
    # put old selected item in normal state
    .dirsel.f2.cv delete BOX
    set id [.dirsel.f2.cv find withtag HASBOX]
    .dirsel.f2.cv itemconfigure $id -fill $tkgetdir::colors(lstfg)
    .dirsel.f2.cv dtag HASBOX HASBOX
    # put new selected item in selected state
    set id [.dirsel.f2.cv find withtag current]
    set wt [.dirsel.f2.cv itemcget $id -tags]
    if {[lsearch -exact $wt IMG] >= 0} {
	set id [.dirsel.f2.cv find above $id]
    }
     set bxr [.dirsel.f2.cv bbox $id]
    .dirsel.f2.cv addtag HASBOX withtag $id
    .dirsel.f2.cv itemconfigure $id -fill $tkgetdir::colors(selfg)
    eval .dirsel.f2.cv create rectangle $bxr \
	    -fill $tkgetdir::colors(selbg) -outline $tkgetdir::colors(selbg) \
	    -tags BOX
    .dirsel.f2.cv lower BOX
    set nam [.dirsel.f2.cv itemcget $id -text]
    .dirsel.f3.chosen delete 0 end
    .dirsel.f3.chosen insert 0 $nam
}

proc tkgetdir::DoubleClickItem {} {
    set id [.dirsel.f2.cv find withtag current]
    tkgetdir::DownDir
}

namespace import tkgetdir::*

########################################################################


proc Installer::Directory { package version }  {

    global tcl_version 
    global tmpdir
    set tmpdir $::TEA::prefix

    if { [winfo exists .panel] } {
	destroy .panel
    }
    frame .panel
    text .panel.text -wrap word -width 20 -height 10 \
        -borderwidth 0 -padx 4 -pady 4 -cursor arrow \
	-background [.panel cget -background]
    .panel.text tag configure title -font titleFont -justify center \
	-foreground navyblue
    .panel.text insert end "Select Destination Directory\n" title
    .panel.text insert end "\n\n"
    .panel.text insert end "Please select the directory where Tcl/Tk \
$tcl_version is installed.  The package $package $version will be \
installed in the library subdirectory.\n" 
    .panel.text configure -state disabled

    frame .panel.dirspec -relief groove -borderwidth 2

    entry .panel.dirspec.text -width 30 -textvariable tmpdir
    button .panel.dirspec.browsebtn -text "Browse..." -command {set tmpdir [Installer::Browse $tmpdir]}
    if { $::tcl_version < 8.3 } {
	#.panel.dirspec.browsebtn configure -state disabled 
    }
    grid .panel.dirspec.text      -row 0 -column 0 -padx 4 -pady 4 -sticky w 
    grid .panel.dirspec.browsebtn -row 0 -column 1 -padx 4 -pady 4 -sticky e 
    grid .panel.text              -row 0 -column 0 -padx 4 -pady 4 -sticky ewns 
    grid .panel.dirspec           -row 1 -column 0 -padx 4         -sticky ew
    grid .panel -in . -row 0 -column 1 -columnspan 4 -pady 10 -padx 5 -sticky ewns

    tkwait variable Installer::continue
    ::TEA::macro prefix $tmpdir 
    ::TEA::expand
    unset tmpdir
}

proc Installer::Warnings {msg}  {

    if { [winfo exists .warnings] } {
#	destroy .warnings
    } else {
        toplevel .warnings
        text .warnings.text -height 10 -width 50 -wrap none -bg white \
           -yscrollcommand { .warnings.ybar set } \
           -xscrollcommand { .warnings.xbar set } 
        .warnings.text tag configure fail -foreground red
        label .warnings.label -text "Installation Warnings" -width 50
        scrollbar .warnings.ybar -command { .warnings.text yview }
        scrollbar .warnings.xbar -command { .warnings.text xview } -orient horizontal
        wm protocol .warnings WM_DELETE_WINDOW { .warnings.cancel invoke }
        wm transient .warnings .
        button .warnings.cancel -text "Cancel" -command {
            grab release .warnings
            destroy .warnings
        }
        grid .warnings.label  -row 0 -column 0     -columnspan 2  
        grid .warnings.text    -row 1 -column 0 -sticky ewns
        grid .warnings.ybar    -row 1 -column 1 -sticky ns
        grid .warnings.xbar    -row 2 -column 0 -sticky ew 
        grid .warnings.cancel  -row 3 -column 0 -padx 4 -pady 4 -columnspan 2
        grid rowconfigure .warnings 0 -weight 0 
        grid rowconfigure .warnings 1 -weight 1 
        grid rowconfigure .warnings 2 -weight 0
        grid rowconfigure .warnings 3 -weight 0
        grid columnconfigure .warnings 0 -weight 1
        grid columnconfigure .warnings 1 -weight 0

        wm withdraw .warnings
    }
    after idle { 
        ::Installer::CenterAndSizeToplevel .warnings 
	grab set .warnings
    } 
    .warnings.text insert end "$msg\n"
}

proc Installer::Components { package version }  {

    global tcl_version 
    if { [winfo exists .panel] } {
	destroy .panel
    }
    frame .panel
    text .panel.text -wrap word -width 20 \
	-height 8 -borderwidth 0 -padx 4 -pady 4 -cursor arrow \
	-background [.panel cget -background]

    .panel.text tag configure text  -justify center
    .panel.text tag configure title -font titleFont -justify center \
	-foreground navyblue

    .panel.text insert end "Select Components\n" title
    .panel.text insert end "\n\n" text
    .panel.text insert end "Please select the components you wish to install.\n" text 
    # TODO: Minimal, Typical, Full and Customised installations
    .panel.text configure -state disabled
    frame .panel.frame -relief groove -borderwidth 2

    variable component
    variable components
    variable componentAttrs
    variable componentDescription

    foreach {c} $components  {
        set b $component($c)
	checkbutton .panel.frame.$c \
		-text $componentDescription($c) \
		-variable Installer::component($c)
	foreach attr $componentAttrs($c) {
	    switch -- $attr {
		-mandatory {  
		    .panel.frame.$c config -state disabled 
		}
		-recommended { 
		}
		-optional { 
		}
	    } 
	}
	grid .panel.frame.$c -column 0 -padx 4 -pady 4 -sticky w 
    }

    grid .panel.text  -row 0 -column 0 -padx 4 -pady 4 -sticky ewns 
    grid .panel.frame -row 1 -column 0 -padx 4 -sticky ewns

    grid .panel -row 0 -column 1 -columnspan 4 -pady 10 -padx 5 -sticky ewns
    tkwait variable Installer::continue
}

proc Installer::Ready { package version }  {
    global tcl_version 
    if { [winfo exists .panel] } {
	destroy .panel
    }
    text .panel -wrap word -width 10 -height 18 \
	-relief flat -padx 4 -pady 4 -cursor arrow \
	-background [. cget -bg]
    .panel tag configure welcome -font titleFont -justify center \
	-foreground navyblue
    .panel tag configure package -font hugeFont -foreground red \
	-justify center

     MakeLink .panel next "Installer::Next"
     MakeLink .panel cancel "Installer::Cancel"
     MakeLink .panel back "Installer::Back"
    .panel insert end "Ready To Install!\n" welcome
    .panel insert end "\n"
    .panel insert end "We're now ready to install ${package} ${version} \
and its components.\n\n"
    .panel insert end \
	"Press the " "" \
	"Next" next \
	" button to install all selected components.\n\n" "" 
    .panel insert end \
	"To reselect components, click on the " "" \
	"Back" back \
	" button.\n\n"
    .panel insert end \
	"Press the " "" \
	"Cancel" cancel \
	" button if you do not wish to install $package at this time."
    .panel configure -state disabled
    grid .panel -row 0 -column 1 -columnspan 4 -pady 10 -padx 5 -sticky ewn
    tkwait variable Installer::continue
    if { $Installer::continue } {
	Results
	update
	DoInstall $package $version
    }
}

proc Installer::Results { }  {
    if { [winfo exists .install] } {
	destroy .install
    }
    toplevel .install
    text .install.text -height 10 -width 50 -wrap none -bg white \
	-yscrollcommand { .install.ybar set } \
	-xscrollcommand { .install.xbar set } 
    .install.text tag configure fail -foreground red
    label .install.totals -text "Files: 0  Bytes: 0" -width 50
    label .install.current -text "Installing:\n" -height 2 -width 50
    scrollbar .install.ybar -command { .install.text yview }
    scrollbar .install.xbar -command { .install.text xview } -orient horizontal
    wm protocol .install WM_DELETE_WINDOW { .install.cancel invoke }
    wm transient .install .
    button .install.cancel -text "Cancel" -command {
	grab release .install
	destroy .install
    }
    grid .install.totals  -row 0 -column 0     -columnspan 2  
    grid .install.current -row 1 -column 0     -columnspan 2 
    grid .install.text    -row 2 -column 0 -sticky ewns
    grid .install.ybar    -row 2 -column 1 -sticky ns
    grid .install.xbar    -row 3 -column 0 -sticky ew 
    grid .install.cancel  -row 4 -column 0 -padx 4 -pady 4 -columnspan 2
    grid rowconfigure .install 0 -weight 0 
    grid rowconfigure .install 1 -weight 0 
    grid rowconfigure .install 2 -weight 1 
    grid rowconfigure .install 3 -weight 0
    grid rowconfigure .install 4 -weight 0
    grid columnconfigure .install 0 -weight 1
    grid columnconfigure .install 1 -weight 0

 #   blt::table configure .install c1 r0 r1 r3  -resize none

    wm withdraw .install
    after idle { 
	Installer::CenterPanel .install 
	grab set .install
    } 
}

proc Installer::Finish { package version }  {
    global tcl_version 
    if { [winfo exists .panel] } {
	destroy .panel
    }
    text .panel -wrap word -width 10 -height 18 \
	-relief flat -padx 4 -pady 4 -cursor arrow \
	-background [. cget -bg]
    .panel tag configure welcome -font titleFont -justify center \
	-foreground navyblue
    .panel tag configure package -font hugeFont -foreground red \
	-justify center
    .panel insert end "Installation Completed\n" welcome
    .panel insert end "\n"
    .panel insert end "${package} ${version} is now installed.\n\n"
     MakeLink .panel finish "Installer::Next"
    .panel insert end \
	"Press the " "" \
	"Finish" finish \
	" button to exit this installation"
    .panel configure -state disabled
    grid .panel	-row 0 -column 1 -columnspan 3 -pady 10 -padx 5 -sticky ewn
    tkwait variable Installer::continue
}

set ::Installer::SrcPath [file dirname [info script]]
set ::Installer::BuildPath [pwd]

set c 0
while { $c < $argc }  {
    set arg [lindex $argv $c]
    switch -- $arg {
	-srcdir {
	    set argv [ lreplace $argv $c $c {} ]
	    incr c
	    set ::Installer::SrcPath [lindex $argv $c] 
 	}
	-builddir {
	    set argv [ lreplace $argv $c $c {} ]
	    incr c
	    set ::Installer::BuildPath [lindex $argv $c] 
 	}
    }
    set argv [ lreplace $argv $c $c {} ]
    incr c
}

if { ! [file exists $::Installer::SrcPath] } {
    error "Can't find source directory \"$::Installer::SrcPath\" "
}

switch $::tcl_platform(platform) {
    "mac*"     {  
	set platformSrcPath "mac"
    }
    "windows"  { 
	set defaultBuild "Release"
	set platformSrcPath "win"
    }
    default {
	# unix 

	# multiple targets with mutiple build configs : srcdir/build/platform(/config)
	set defaultBuild "release"
	#[ file join build $::tcl_platform(os) $defaultBuild]
	set platformSrcPath "" 
    }
}
set platformSrcPath [file join $::Installer::SrcPath $platformSrcPath] 

namespace eval ::TEA {
    variable macros
    variable macronames

}

proc ::TEA::macro {name value} {
    
    variable macros
    variable macronames

    if { ! [info exists macros($name)] } {
	lappend macronames ::TEA::$name
    } 
    set macros($name) $value
}

proc ::TEA::expand {} {

    variable macros
    array set tmp [ array get macros]
    
    set nvars [array size macros]
    set expvars 0
    set loops 0
    while { ($expvars < $nvars) && ($loops < $nvars - 1) } {
	foreach {macro value} [array get tmp] {
	    if { ! [regexp {\$} $value] } {
		variable $macro
		set $macro [subst $value]
		incr expvars
		unset tmp($macro)
	    } else {
		catch { set tmp($macro) [subst $value] }
	    }
	}
	incr loops
    }
    unset tmp
}

namespace eval ::TEA {

    set prefix [file dirname [file dirname [info library]]]
  
    if { ! [string length $prefix] } { 
	switch $::tcl_platform(platform) {
	    unix {
		macro prefix "/usr/local/tcl"
	    }
	    macintosh {
		macro prefix ${::PREFS_FOLDER}:Tcl
	    }
	    default {
		macro prefix "C:/Program Files/Tcl"
	    }
	}
    } else {
	macro prefix $prefix
    }

    # could also provide list of likely prefix dirs from env vars like TCL_PACKAGE_PATH,
    # TCLXML,TCL registry entries etc

    macro       exec_prefix             {$prefix}
    macro       PACKAGE                 {$::Installer::Package}
    macro       VERSION                 {$::Installer::Version}
    macro	RELPATH                 ..
    macro	program_transform_name  "s,x,x,"
    macro	bindir                  {[file join $exec_prefix bin]}
    macro	sbindir                 {[file join $exec_prefix sbin]}
    macro	libexecdir              {[file join $exec_prefix libexec]}
    macro	datadir                 {[file join $prefix share]}
    macro	sysconfdir              {[file join $prefix etc]}
    macro	sharedstatedir          {[file join $prefix com]}
    macro	localstatedir           {[file join $prefix var]}
    macro	libdir                  {[file join $exec_prefix lib]}
    macro	includedir              {[file join $prefix include]}
    macro	infodir                 {[file join $prefix info]}
    macro	htmldir                 {[file join $prefix html]}
    macro	mandir                  {[file join $prefix man]}
    macro	EXEEXT                  {}

    # These hard-coded values are not used
#    macro	TCL_BIN_DIR             /usr/local/tcl/lib
#    macro	TCL_SRC_DIR             /usr/local/src/tcl/tcl8.3.2
#    macro	TCL_LIB_FILE            libtcl8.3.so
#    macro	TCL_TOP_DIR_NATIVE      /usr/local/src/tcl/tcl8.3.2
#    macro	TCL_GENERIC_DIR_NATIVE  {$TCL_TOP_DIR_NATIVE/generic}
#    macro	TCL_UNIX_DIR_NATIVE     {$TCL_TOP_DIR_NATIVE/unix}
#    macro	TCL_WIN_DIR_NATIVE      {$TCL_TOP_DIR_NATIVE/win}
#    macro	TCL_BMAP_DIR_NATIVE     {$TCL_TOP_DIR_NATIVE/bitmaps}
#    macro	TCL_TOOL_DIR_NATIVE     {$TCL_TOP_DIR_NATIVE/tools}
#    macro	TCL_PLATFORM_DIR_NATIVE {$TCL_TOP_DIR_NATIVE/unix}
    macro	TCL_BIN_DIR_NATIVE      {}
    macro	INCLUDE_DIR_NATIVE      {[file join $prefix include]}
    macro	CLEANFILES              {}
    macro	SHELL                   /bin/sh
}    
    
# Read in any build generated configuration data.
# This will cache a build.data file it doesnt already exist

Installer::GetBuildData [list $::Installer::BuildPath $platformSrcPath $::Installer::SrcPath  ]
  
# Read in the installation configuration data.
# This will exit if no data can be found

Installer::GetInstallData $::Installer::SrcPath

TEA::expand

if { $tcl_platform(platform) == "unix" } {
    font create textFont -family Helvetica -weight normal -size 11
    font create titleFont -family Helvetica -weight normal -size 14
    font create fixedFont -family fixed -weight normal -size 12
} else {
    font create titleFont -family Arial -weight bold -size 12
    font create textFont -family Arial -weight normal -size 9
    font create fixedFont -family courier -weight normal -size 12
}
font create hugeFont -family {Times New Roman} -size 18 -slant italic \
    -weight bold

#option add *Hierbox.openCommand	{Installer::OpenDir %W "%P" %n}

set c 0
while { $c < $argc }  {
    set arg [lindex $argv $c]
    switch -- $arg {
	default {
	    incr c
	    ::TEA::macro $arg [lindex $argv $c] 
 	}
    }
    incr c
}

set ext [info sharedlibextension]
set package $::TEA::PACKAGE
regsub {\.} $::TEA::VERSION {} v2

namespace eval ::Installer {

    if { $tcl_platform(platform) == "unix" } {
	set shlib lib${package}${v2}${ext}
	set lib lib${package}${v2}.a
	set exe ""
    } elseif { $tcl_platform(platform) == "macintosh" } {
	set shlib ${package}${v2}${ext}
	set lib lib${package}${v2}.shlib
	set exe ""
    } else { #windows
	set shlib ${package}${v2}${ext}
	set lib ${package}${v2}.lib
	set exe ".exe"
    }
}

image create photo openFolder -format gif -data {
R0lGODlhEAANAPIAAAAAAH9/f7+/v///////AAAAAAAAAAAAACH+JEZpbGUgd3JpdHRlbiBi
eSBBZG9iZSBQaG90b3Nob3CoIDUuMAAsAAAAABAADQAAAzk4Gsz6cIQ44xqCZCGbk4MmclAA
gNs4ml7rEaxVAkKc3gTAnBO+sbyQT6M7gVQpk9HlAhgHzqhUmgAAOw==
}
image create photo closeFolder -format gif -data {
R0lGODlhEAANAPIAAAAAAH9/f7+/v///AP///wAAAAAAAAAAACH+JEZpbGUgd3JpdHRlbiBi
eSBBZG9iZSBQaG90b3Nob3CoIDUuMAAsAAAAABAADQAAAzNIGsz6kAQxqAjxzcpvc1KWBUDY
nRQZWmilYi37EmztlrAt43R8mzrO60P8lAiApHK5TAAAOw==
}
#image create photo blt -file ${SrcPath}/demos/images/blt98.gif

option add *Text.font textFont
option add *HighlightThickness 0
#option add *Hierbox.icons "closeFolder openFolder"
#option add *Hierbox.button yes

set color \#accaff
option add *Frame.background $color
option add *Toplevel.background $color
#option add *Button.background $color
option add *Checkbutton.background $color
option add *Label.background $color
option add *Text.background $color
. configure -bg $color
wm title . "$package $::TEA::VERSION Installer"
label .image -text [string toupper $::Installer::Package] -borderwidth 2 -relief groove
button .back -text "Back" -state disabled -command Installer::Back -underline 0
button .next -text "Next" -command Installer::Next -underline 0
button .cancel -text "Cancel" -command Installer::Cancel -underline 0
button .help -text "Help" -command Installer::Help -underline 0
frame .sep -height 2 -borderwidth 1 -relief sunken

grid .image  -row 0 -column 0 -sticky ewns -padx  4 -pady  4 
grid .sep    -row 1 -column 0 -sticky ew   -padx  0 -pady  0 -columnspan 5 
grid .back   -row 2 -column 1 -sticky e    -padx  4 -pady  4 
grid .next   -row 2 -column 2 -sticky w    -padx  4 -pady  4 
grid .help   -row 2 -column 3 -sticky w    -padx 10 -pady  4 
grid .cancel -row 2 -column 4 -sticky w    -padx 10 -pady  4

grid rowconfigure . 0 -weight 1
grid rowconfigure . 1 -minsize .125i -weight 0
grid rowconfigure . 2 -minsize 0.125i

grid columnconfigure . 1 -weight 1
grid columnconfigure . 2 -weight 1
grid columnconfigure . 3 -weight 1
grid columnconfigure . 4 -weight 1

Installer::CenterAndSizeToplevel .

while { 1 } {
    namespace eval ::Installer {
	variable panel
	set cmd [lindex $panelList $panel]
	eval [list $cmd $::TEA::PACKAGE $::TEA::VERSION]
    }	
    update
} 
