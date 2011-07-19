## -*- tcl -*-
## (C) 2010 Andreas Kupries <andreas_kupries@users.sourceforge.net>
## BSD Licensed
# # ## ### ##### ######## ############# ######################

#
# application on top of the diagram drawing package.
#

## Use Cases
## (1) Reading a single diagram file and showing it on a canvas.

## (1a) Like (1), for multiple input files. This requires an additional
##     selection step before the diagram is shown.

## (2) Convert one or more diagram files into raster images in various
##     formats.

# # ## ### ##### ######## ############# #####################
## Command syntax

## (Ad 1)  show picfile
## (Ad 1a) show picfile picfile...

## (Ad 2)  convert -o output-file-or-dir format picfile
##         convert -o output-dir         format picfile picfile...

# # ## ### ##### ######## ############# #####################
## Requirements

package require Tcl 8.5
package require Tk  8.5
package require fileutil

wm withdraw . ; # Hide the main toplevel until we actually need it, if
		# ever.
namespace eval ::diagram::application {}

# # ## ### ##### ######## ############# #####################
## Implementation

proc ::diagram::application {arguments} {
    variable application::mode
    application::ProcessCmdline $arguments
    application::Run::$mode
    return
}

proc ::diagram::application::showerror {text} {
    global argv0
    puts stderr "$argv0: $text"
    exit 1
}

# # ## ### ##### ######## ############# #####################
## Internal data and status

namespace eval ::diagram::application {
    # Path to where the output goes to. Depending on the chosen mode
    # this information may be irrelevant, a file, or a directory.
    # Specified through the option '-o' where suitable.

    variable  output ""

    # Paths of the documents to convert. Always a list, even in the
    # case of a single input file. Specified through the trailing
    # arguments on the command line. The relative path of a file under
    # 'input' also becomes its relative path under 'output'.

    variable  input  ""

    # The name of the format to convert the diagram documents
    # into. Used as extension for the generated files as well when
    # converting multiple files. Internally this is the name of the
    # canvas::* or img::* package for the image format. The two cases
    # are distinguished by the value of the boolean flag "snap". True
    # indicates a raster format via img::*, false a canvas::* dump
    # package ... FUTURE :: Should have a 'canvas::write::*' or
    # somesuch family of packages which hide this type of difference
    # from us.

    variable  format ""
    variable  snap   0

    # Name of the found processing mode. Derived during processing all
    # arguments on the command line. This value is used during the
    # dispatch to the command implementing the mode, after processing
    # the command line.
    #
    # Possible/Legal values:	Meaning
    # ---------------------	-------
    # ---------------------	-------

    variable  mode   ""
}

# # ## ### ##### ######## ############# #####################
##

proc ::diagram::application::ProcessCmdline {arguments} {
    variable input  {} ; # Set defaults.
    variable output "" ; #
    variable format "" ; #
    variable mode   "" ; #

    # syntax: show file...
    #         convert -o output format file...

    if {[llength $arguments] < 2} Usage
    set arguments [lassign $arguments command]

    switch -exact -- $command {
	show    {ProcessShow    $arguments}
	convert {ProcessConvert $arguments}
	default Usage
    }

    set mode $command
    return
}

proc ::diagram::application::ProcessShow {arguments} {
    if {[llength $arguments] < 1} Usage
    variable input   {}
    variable trusted 0

    # Basic option processing and validation.
    while {[llength $arguments]} {
        set opt [lindex $arguments 0]
        if {![string match "-*" $opt]} break

        switch -exact -- $opt {
            -t {
                if {[llength $arguments] < 1} Usage
                set arguments [lassign $arguments _opt_]
                set trusted 1
            }
            default Usage
        }
    }

    set input $arguments
    CheckInput
    return
}

proc ::diagram::application::ProcessConvert {arguments} {
    variable output ""
    variable input  {}
    variable format ""
    variable trusted 0

    if {[llength $arguments] < 4} Usage

    # Basic option processing and validation.
    while {[llength $arguments]} {
	set opt [lindex $arguments 0]
	if {![string match "-*" $opt]} break

	switch -exact -- $opt {
	    -o {
		if {[llength $arguments] < 2} Usage
		set arguments [lassign $arguments _opt_ output]
	    }
            -t {
                if {[llength $arguments] < 1} Usage
                set arguments [lassign $arguments _opt_]
                set trusted 1
            }
	    default Usage
	}
    }
    # Format and at least one file are expected.
    if {[llength $arguments] < 2} Usage
    set input [lassign $arguments format]

    ValidateFormat
    CheckInput
    CheckOutput
    return
}

# # ## ### ##### ######## ############# #####################

proc ::diagram::application::Usage {} {
    showerror "wrong#args, expected: show file...|convert -o outputpath format file..."
    # not reached ...
}

# # ## ### ##### ######## ############# #####################
## Various complex checks on the arguments

proc ::diagram::application::ValidateFormat {} {
    variable format
    variable snap
    if {![catch {
	package require canvas::snap
	package require img::$format
	set snap 1
    } msgA]} return

    if {![catch {
	package require canvas::$format
    } msgB]} return

    showerror "Unable to handle format \"$format\", because of: $msgA and $msgB"
    return
}

proc ::diagram::application::CheckInput {} {
    variable input
    foreach f $input {
	if {![file exists $f]} {
	    showerror "Unable to find picture \"$f\""
	} elseif {![file readable $f]} {
	    showerror "picture \"$f\" not readable (permission denied)"
	}
    }
    if {[llength $input] < 1} {
	showerror "No picture(s) specified"
    }
    return
}

proc ::diagram::application::CheckOutput {} {
    variable input
    variable output

    if {$output eq ""} {
	showerror "No output path specified"
    }

    set base [file dirname $output]
    if {$base eq ""} {set base [pwd]}

    # Multiple inputs: Output must either exist as directory, or
    # output base writable so that we can create the directory.
    # Single input: As above except existence as file.

    if {![file exists $output]} {
	if {![file exists $base]} {
	    showerror "Output base path \"$base\" not found"
	}
	if {![file writable $base]} {
	    showerror "Output base path \"$base\" not writable (permission denied)"
	}
    } else {
	if {![file writable $output]} {
	    showerror "Output path \"$output\" not writable (permission denied)"
	}

	if {[llength $input] > 1} {
	    if {![file isdirectory $output]} {
		showerror "Output path \"$output\" not a directory"
	    }
	} else {
	    if {![file isfile $output]} {
		showerror "Output path \"$output\" not a file"
	    }
	}
    }
    return
}

# # ## ### ##### ######## ############# #####################
##

namespace eval ::diagram::application::Run::GUI {}

proc ::diagram::application::Run::show {} {
    variable ::diagram::application::input

    GUI::Show

    if {[llength $input] == 1} {
	after 100 {
	    .l selection clear 0 end
	    .l selection set   0
	    event generate .l <<ListboxSelect>>
	}
    }

    vwait __forever__
    return
}

proc ::diagram::application::Run::convert {} {
    variable ::diagram::application::input
    variable ::diagram::application::output

    set dip [MakeInterpreter]
    GUI::Convert
    PrepareOutput

    if {[llength $input] > 1} {
	foreach f $input {
	    Convert $dip $f [GetDestination $f]
	}
    } else {
	set f [lindex $input 0]
	if {[file exists $output] && [file isdirectory $output]} {
	    Convert $dip $f [GetExtension $output/[file tail $input]]
	} else {
	    Convert $dip $f $output
	}
    }

    interp delete $dip
    GUI::Close
    return
}

proc ::diagram::application::Run::Convert {dip src dst} {
    variable ::diagram::application::format
    variable ::diagram::application::snap

    puts ${src}...
    set pic [fileutil::cat $src]

    if {[catch {
	$dip eval [list D draw $pic]
    } msg]} {
	puts "FAIL $msg : $src"
    } elseif {$snap} {
	set DIA [canvas::snap .c]
	$DIA write $dst -format $format
	image delete $DIA
    } else {
	# Direct canvas dump ...
	fileutil::writeFile $dst [canvas::$format .c]
    }

    # Wipe controller state, no information transfer between pictures.
    $dip eval {D reset}
    return
}

proc ::diagram::application::Run::GUI::Show {} {
    package require widget::scrolledwindow
    #package require crosshair

    set dip [::diagram::application::Run::MakeInterpreter]

    button                 .e -text Exit -command ::exit
    widget::scrolledwindow .sl -borderwidth 1 -relief sunken
    widget::scrolledwindow .sc -borderwidth 1 -relief sunken
    listbox                .l -width 40 -selectmode single -listvariable ::diagram::application::input
    canvas                 .c -width 800 -height 600 -scrollregion {-4000 -4000 4000 4000}

    .sl setwidget .l
    .sc setwidget .c

    pack .e  -fill none -expand 0 -side bottom -anchor e

    #panedwindow .p -orient vertical
    #.p add .sl .sc
    #.p paneconfigure .sl -width 100

    pack .sl -fill both -expand 1 -padx 4 -pady 4 -side left
    pack .sc -fill both -expand 1 -padx 4 -pady 4 -side right

    bind .l <<ListboxSelect>> [list ::diagram::application::Run::GUI::ShowPicture $dip]


    # Panning via mouse
    bind .c <ButtonPress-2> {%W scan mark   %x %y}
    bind .c <B2-Motion>     {%W scan dragto %x %y}

    # Cross hairs ...
    #.c configure -cursor tcross
    #crosshair::crosshair .c -width 0 -fill \#999999 -dash {.}
    #crosshair::track on  .c TRACK

    wm deiconify .
    return
}

proc ::diagram::application::Run::GUI::ShowPicture {dip} {

    set selection [.l curselection]
    if {![llength $selection]} return

    $dip eval {catch {D destroy}}
    $dip eval {diagram D .c}

    set pic [fileutil::cat [.l get $selection]]

    after 0 [list $dip eval [list D draw $pic]]
    return
}

proc ::diagram::application::Run::GUI::Convert {} {
    canvas .c -width 800 -height 600 -scrollregion {0 0 1200 1000}
    grid   .c -row 0 -column 0 -sticky swen

    grid rowconfigure    . 0 -weight 1
    grid columnconfigure . 0 -weight 1

    wm attributes     . -fullscreen 1
    wm deiconify      .
    tkwait visibility .
    return
}

proc ::diagram::application::Run::GUI::Close {} {
    wm withdraw .
    destroy     .
    return
}

proc ::diagram::application::Run::PrepareOutput {} {
    variable ::diagram::application::input
    variable ::diagram::application::output

    if {[llength $input] > 1} {
	file mkdir [file dirname $output]
    }
    return
}

proc ::diagram::application::Run::GetDestination {f} {
    variable ::diagram::application::output

    if {[file pathtype $f] ne "relative"} {
	return set f [file join $output {*}[lrange [file split $f] 1 end]]
    } else {
       set f $output/$f
    }
    file mkdir [file dirname $f]
    return [GetExtension $f]
}

proc ::diagram::application::Run::GetExtension {f} {
    variable ::diagram::application::format
    return [file rootname $f].$format
}

proc ::diagram::application::Run::MakeInterpreter {} {
    variable ::diagram::application::trusted
    set sec [expr {[lindex [time {
        if {$trusted} {
            puts {Creating trusted environment, please wait...}
            set dip [interp create]
            $dip eval [list set auto_path $::auto_path]
        } else {
            puts {Creating safe environment, please wait...}
	    set dip [::safe::interpCreate]
        }
	interp alias $dip .c {} .c ; # Import of canvas
	interp alias $dip tk {} tk ; # enable tk scaling
	$dip eval {package require diagram}
	$dip eval {diagram D .c}
    }] 0]/double(1e6)}]
    puts "... completed in $sec seconds."
    after 100
    return $dip
}

# # ## ### ##### ######## ############# #####################
package provide diagram::application 1.1
return
