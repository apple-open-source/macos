# -*- tcl -*-
#
# ruler.tcl
#
#	ruler widget and screenruler dialog
#
# Copyright (c) 2005 Jeffrey Hobbs.  All Rights Reserved.
#
# RCS: @(#) $Id: ruler.tcl,v 1.13 2008/02/21 20:11:16 hobbs Exp $
#

###
# Creation and Options - widget::ruler $path ...
#    -foreground	-default black
#    -font		-default {Helvetica 14}
#    -interval		-default [list 5 25 100]
#    -sizes		-default [list 4 8 12]
#    -showvalues	-default 1
#    -outline		-default 1
#    -grid		-default 0
#    -measure		-default pixels ; {pixels points inches mm cm}
#    -zoom		-default 1
#    all other options inherited from canvas
#
# Methods
#  All methods passed to canvas
#
# Bindings
#  <Configure> redraws
#

###
# Creation and Options - widget::screenruler $path ...
#    -alpha	-default 0.8
#    -title	-default ""
#    -topmost	-default 0
#    -reflect	-default 0 ; reflect desktop screen
#    -zoom	-default 1
#
# Methods
#  $path display
#  $path hide
#  All
#
# Bindings
#

if 0 {
    # Samples
    package require widget::screenruler
    set dlg [widget::screenruler .r -grid 1 -title "Screen Ruler"]
    $dlg menu add separator
    $dlg menu add command -label "Exit" -command { exit }
    $dlg display
}

package require widget 3

snit::widgetadaptor widget::ruler {
    delegate option * to hull
    delegate method * to hull

    option -foreground	-default black -configuremethod C-redraw
    option -font	-default {Helvetica 14}
    option -interval	-default [list 5 25 100] -configuremethod C-redraw \
	-type [list snit::listtype -type {snit::double} -minlen 3 -maxlen 3]
    option -sizes	-default [list 4 8 12] -configuremethod C-redraw \
	-type [list snit::listtype -type {snit::double} -minlen 3 -maxlen 3]
    option -showvalues	-default 1 -configuremethod C-redraw \
	-type [list snit::boolean]
    option -outline	-default 1 -configuremethod C-redraw \
	-type [list snit::boolean]
    option -grid	-default 0 -configuremethod C-redraw \
	-type [list snit::boolean]
    option -measure	-default pixels -configuremethod C-measure \
	-type [list snit::enum -values [list pixels points inches mm cm]]
    option -zoom	-default 1 -configuremethod C-redraw \
	-type [list snit::integer -min 1]

    variable shade -array {small gray medium gray large gray}

    constructor {args} {
	installhull using canvas -width 200 -height 50 \
	    -relief flat -bd 0 -background white -highlightthickness 0

	$hull xview moveto 0
	$hull yview moveto 0

	$self _reshade

	bind $win <Configure> [mymethod _resize %W %X %Y]

	#bind $win <Key-minus> [mymethod _adjustinterval -1]
	#bind $win <Key-plus>  [mymethod _adjustinterval 1]
	#bind $win <Key-equal> [mymethod _adjustinterval 1]

	$self configurelist $args

	$self redraw
    }

    destructor {
	catch {after cancel $redrawID}
    }

    ########################################
    ## public methods

    ########################################
    ## configure methods

    variable width    0
    variable height   0
    variable measure  -array {
	what ""
	valid {pixels points inches mm cm}
	cm c mm m inches i points p pixels ""
    }
    variable redrawID {}

    method C-redraw {option value} {
	if {$value ne $options($option)} {
	    set options($option) $value
	    if {$option eq "-foreground"} { $self _reshade }
	    $self redraw
	}
    }

    method C-measure {option value} {
	if {[set idx [lsearch -glob $measure(valid) $value*]] == -1} {
	    return -code error "invalid $option value \"$value\":\
		must be one of [join $measure(valid) {, }]"
	}
	set value [lindex $measure(valid) $idx]
	set measure(what) $measure($value)
	set options($option) $value
	$self redraw
    }

    ########################################
    ## private methods

    method _reshade {} {
	set bg [$hull cget -bg]
	set fg $options(-foreground)
	set shade(small)  [$self shade $bg $fg 0.15]
	set shade(medium) [$self shade $bg $fg 0.4]
	set shade(large)  [$self shade $bg $fg 0.8]
    }

    method redraw {} {
	after cancel $redrawID
	set redrawID [after idle [mymethod _redraw]]
    }

    method _redraw {} {
	$hull delete ruler
	set width  [winfo width $win]
	set height [winfo height $win]
	$self _redraw_x
	$self _redraw_y
	if {$options(-outline) || $options(-grid)} {
	    if {[tk windowingsystem] eq "aqua"} {
		# Aqua has an odd off-by-one drawing
		set coords [list 0 0 $width $height]
	    } else {
		set coords [list 0 0 [expr {$width-1}] [expr {$height-1}]]
	    }
	    $hull create rect $coords -width 1 -outline $options(-foreground) \
		-tags [list ruler outline]
	}
	if {$options(-showvalues) && $height > 20} {
	    if {$measure(what) ne ""} {
		set m   [winfo fpixels $win 1$measure(what)]
		set txt "[format %.2f [expr {$width / $m}]] x\
			[format %.2f [expr {$height / $m}]] $options(-measure)"
	    } else {
		set txt "$width x $height"
	    }
	    if {$options(-zoom) > 1} {
		append txt " (x$options(-zoom))"
	    }
	    $hull create text 15 [expr {$height/2.}] \
		-text $txt \
		-anchor w -tags [list ruler value label] \
		-fill $options(-foreground)
	}
	$hull raise large
	$hull raise value
    }

    method _redraw_x {} {
 	foreach {sms meds lgs} $options(-sizes) { break }
	foreach {smi medi lgi} $options(-interval) { break }
 	for {set x 0} {$x < $width} {set x [expr {$x + $smi}]} {
	    set dx [winfo fpixels $win \
			[expr {$x * $options(-zoom)}]$measure(what)]
	    if {fmod($x, $lgi) == 0.0} {
		# draw large tick
		set h $lgs
		set tags [list ruler tick large]
		if {$x && $options(-showvalues) && $height > $lgs} {
		    $hull create text [expr {$dx+1}] $h -anchor nw \
			-text [format %g $x]$measure(what) \
			-tags [list ruler value]
		}
		set fill $shade(large)
	    } elseif {fmod($x, $medi) == 0.0} {
		set h $meds
		set tags [list ruler tick medium]
		set fill $shade(medium)
	    } else {
		set h $sms
		set tags [list ruler tick small]
		set fill $shade(small)
	    }
	    if {$options(-grid)} {
		$hull create line $dx 0 $dx $height -width 1 -tags $tags \
		    -fill $fill
	    } else {
		$hull create line $dx 0 $dx $h -width 1 -tags $tags \
		    -fill $options(-foreground)
		$hull create line $dx $height $dx [expr {$height - $h}] \
		    -width 1 -tags $tags -fill $options(-foreground)
	    }
	}
    }

    method _redraw_y {} {
 	foreach {sms meds lgs} $options(-sizes) { break }
	foreach {smi medi lgi} $options(-interval) { break }
 	for {set y 0} {$y < $height} {set y [expr {$y + $smi}]} {
	    set dy [winfo fpixels $win \
			[expr {$y * $options(-zoom)}]$measure(what)]
	    if {fmod($y, $lgi) == 0.0} {
		# draw large tick
		set w $lgs
		set tags [list ruler tick large]
		if {$y && $options(-showvalues) && $width > $lgs} {
		    $hull create text $w [expr {$dy+1}] -anchor nw \
			-text [format %g $y]$measure(what) \
			-tags [list ruler value]
		}
		set fill $shade(large)
	    } elseif {fmod($y, $medi) == 0.0} {
		set w $meds
		set tags [list ruler tick medium]
		set fill $shade(medium)
	    } else {
		set w $sms
		set tags [list ruler tick small]
		set fill $shade(small)
	    }
	    if {$options(-grid)} {
		$hull create line 0 $dy $width $dy -width 1 -tags $tags \
		    -fill $fill
	    } else {
		$hull create line 0 $dy $w $dy -width 1 -tags $tags \
		    -fill $options(-foreground)
		$hull create line $width $dy [expr {$width - $w}] $dy \
		    -width 1 -tags $tags -fill $options(-foreground)
	    }
	}
    }

    method _resize {w X Y} {
	if {$w ne $win} { return }
	$self redraw
    }

    method _adjustinterval {dir} {
	set newint {}
	foreach i $options(-interval) {
	    if {$dir < 0} {
		lappend newint [expr {$i/2.0}]
	    } else {
		lappend newint [expr {$i*2.0}]
	    }
	}
	set options(-interval) $newint
	$self redraw
    }

    method shade {orig dest frac} {
	if {$frac >= 1.0} {return $dest} elseif {$frac <= 0.0} {return $orig}
	foreach {oR oG oB} [winfo rgb $win $orig] \
	    {dR dG dB} [winfo rgb $win $dest] {
	    set color [format "\#%02x%02x%02x" \
			   [expr {int($oR+double($dR-$oR)*$frac)}] \
			   [expr {int($oG+double($dG-$oG)*$frac)}] \
			   [expr {int($oB+double($dB-$oB)*$frac)}]]
	    return $color
	}
    }

}

snit::widget widget::screenruler {
    hulltype toplevel

    component ruler -public ruler
    component menu -public menu

    delegate option * to ruler
    delegate method * to ruler

    option -alpha	-default 0.8 -configuremethod C-alpha;
    option -title	-default "" -configuremethod C-title;
    option -topmost	-default 0 -configuremethod C-topmost;
    option -reflect	-default 0 -configuremethod C-reflect;
    # override ruler zoom for reflection control as well
    option -zoom	-default 1 -configuremethod C-zoom;
    option -showgeometry	-default 0 -configuremethod C-showgeometry;

    variable alpha 0.8 ; # internal opacity value
    variable curinterval 5;
    variable curmeasure "";
    variable grid 0;
    variable reflect -array {ok 0 image "" id ""}
    variable curdim -array {x 0 y 0 w 0 h 0}

    constructor {args} {
	wm withdraw $win
	wm overrideredirect $win 1
	$hull configure -bg white

	install ruler using widget::ruler $win.ruler -width 200 -height 50 \
	    -relief flat -bd 0 -background white -highlightthickness 0
	install menu using menu $win.menu -tearoff 0

	# avoid 1.0 because we want to maintain layered class
	if {$::tcl_platform(platform) eq "windows" && $alpha >= 1.0} {
	    set alpha 0.999
	}
	catch {wm attributes $win -alpha $alpha}
	catch {wm attributes $win -topmost $options(-topmost)}

	grid $ruler -sticky news
	grid columnconfigure $win 0 -weight 1
	grid rowconfigure    $win 0 -weight 1

	set reflect(ok) [expr {![catch {package require treectrl}]
			       && [llength [info commands loupe]]}]
	if {$reflect(ok)} {
	    set reflect(do) 0
	    set reflect(x) -1
	    set reflect(y) -1
	    set reflect(w) [winfo width $win]
	    set reflect(h) [winfo height $win]
	    set reflect(image) [image create photo [myvar reflect] \
				    -width  $reflect(w) -height $reflect(h)]
	    $ruler create image 0 0 -anchor nw -image $reflect(image)

	    # Don't use options(-reflect) because it isn't 0/1
	    $menu add checkbutton -label "Reflect Desktop" \
		-accelerator "r" -underline 0 \
		-variable [myvar reflect(do)] \
		-command "[list $win configure -reflect] \$[myvar reflect(do)]"
	    bind $win <Key-r> [list $menu invoke "Reflect Desktop"]
	}
	$menu add checkbutton -label "Show Grid" \
	    -accelerator "d" -underline 8 \
	    -variable [myvar grid] \
	    -command "[list $ruler configure -grid] \$[myvar grid]"
	bind $win <Key-d> [list $menu invoke "Show Grid"]
	$menu add checkbutton -label "Show Geometry" \
	    -accelerator "g" -underline 5 \
	    -variable [myvar options(-showgeometry)] \
	    -command "[list $win configure -showgeometry] \$[myvar options(-showgeometry)]"
	bind $win <Key-g> [list $menu invoke "Show Geometry"]
	if {[tk windowingsystem] ne "x11"} {
	    $menu add checkbutton -label "Keep on Top" \
		-underline 8 -accelerator "t" \
		-variable [myvar options(-topmost)] \
		-command "[list $win configure -topmost] \$[myvar options(-topmost)]"
	    bind $win <Key-t> [list $menu invoke "Keep on Top"]
	}
	set m [menu $menu.interval -tearoff 0]
	$menu add cascade -label "Interval" -menu $m -underline 0
	foreach interval {
	    {2 10 50} {4 20 100} {5 25 100} {10 50 100}
	} {
	    $m add radiobutton -label [lindex $interval 0] \
		-variable [myvar curinterval] -value [lindex $interval 0] \
		-command [list $ruler configure -interval $interval]
	}
	set m [menu $menu.zoom -tearoff 0]
	$menu add cascade -label "Zoom" -menu $m -underline 0
	foreach zoom {1 2 3 4 5 8 10} {
	    set lbl ${zoom}x
	    $m add radiobutton -label $lbl \
		-underline 0 \
		-variable [myvar options(-zoom)] -value $zoom \
		-command "[list $win configure -zoom] \$[myvar options(-zoom)]"
	    bind $win <Key-[string index $zoom end]> \
		[list $m invoke [string map {% %%} $lbl]]
	}
	set m [menu $menu.measure -tearoff 0]
	$menu add cascade -label "Measurement" -menu $m -underline 0
	foreach {val und} {pixels 0 points 1 inches 0 mm 0 cm 0} {
	    $m add radiobutton -label $val \
		-underline $und \
		-variable [myvar curmeasure] -value $val \
		-command [list $ruler configure -measure $val]
	}
	set m [menu $menu.opacity -tearoff 0]
	$menu add cascade -label "Opacity" -menu $m -underline 0
	for {set i 10} {$i <= 100} {incr i 10} {
	    set aval [expr {$i/100.}]
	    $m add radiobutton -label "${i}%" \
		-variable [myvar alpha] -value $aval \
		-command [list $win configure -alpha $aval]
	}

	if {[tk windowingsystem] eq "aqua"} {
	    bind $win <Control-ButtonPress-1> [list tk_popup $menu %X %Y]
	    # Aqua switches 2 and 3 ...
	    bind $win <ButtonPress-2>         [list tk_popup $menu %X %Y]
	} else {
	    bind $win <ButtonPress-3>         [list tk_popup $menu %X %Y]
	}
	bind $win <Configure>     [mymethod _resize %W %x %y %w %h]
	bind $win <ButtonPress-1> [mymethod _dragstart %W %X %Y]
	bind $win <B1-Motion>     [mymethod _drag %W %X %Y]
	bind $win <Motion>        [mymethod _edgecheck %W %x %y]

	#$hull configure -menu $menu

	$self configurelist $args

	set grid [$ruler cget -grid]
	set curinterval [lindex [$ruler cget -interval] 0]
	set curmeasure  [$ruler cget -measure]
    }

    destructor {
	catch {
	    after cancel $reflect(id)
	    image delete $reflect(image)
	}
    }

    ########################################
    ## public methods

    method display {} {
	wm deiconify $win
	raise $win
	focus $win
    }

    method hide {} {
	wm withdraw $win
    }

    ########################################
    ## configure methods

    method C-alpha {option value} {
	if {![string is double -strict $value]
	    || $value < 0.0 || $value > 1.0} {
	    return -code error "invalid $option value \"$value\":\
		must be a double between 0 and 1"
	}
        set options($option) $value
	set alpha $value
	# avoid 1.0 because we want to maintain layered class
	if {$::tcl_platform(platform) eq "windows" && $alpha >= 1.0} {
	    set alpha 0.999
	}
	catch {wm attributes $win -alpha $alpha}
    }
    method C-title {option value} {
	wm title $win $value
	wm iconname $win $value
        set options($option) $value
    }
    method C-topmost {option value} {
        set options($option) $value
	catch {wm attributes $win -topmost $value}
    }

    method C-reflect {option value} {
	if {($value > 0) && !$reflect(ok)} {
	    return -code error "no reflection possible"
	}
	after cancel $reflect(id)
	if {$value > 0} {
	    if {$value < 50} {
		set value 50
	    }
	    set reflect(id) [after idle [mymethod _reflect]]
	} else {
	    catch {$reflect(image) blank}
	}
        set options($option) $value
    }

    method C-zoom {option value} {
	if {![string is integer -strict $value] || $value < 1} {
	    return -code error "invalid $option value \"$value\":\
		must be a valid integer >= 1"
	}
	$ruler configure -zoom $value
	set options($option) $value
    }

    method C-showgeometry {option value} {
	if {![string is boolean -strict $value]} {
	    return -code error "invalid $option value \"$value\":\
		must be a valid boolean"
	}
	set options($option) $value
	$ruler delete geoinfo
	if {$value} {
	    set opts [list -borderwidth 1 -highlightthickness 1 -width 4]
	    set x 20
	    set y 20
	    foreach d {x y w h} {
		set w $win._$d
		destroy $w
		eval [linsert $opts 0 entry $w -textvar [myvar curdim($d)]]
		$ruler create window $x $y -window $w -tags geoinfo
		bind $w <Return> [mymethod _placecmd]
		# Avoid toplevel bindings
		bindtags $w [list $w Entry all]
		incr x [winfo reqwidth $w]
	    }
	}
    }

    ########################################
    ## private methods

    method _placecmd {} {
	wm geometry $win $curdim(w)x$curdim(h)+$curdim(x)+$curdim(y)
    }

    method _resize {W x y w h} {
	if {$W ne $win} { return }
	set curdim(x) $x
	set curdim(y) $y
	set curdim(w) $w
	set curdim(h) $h
    }

    method _reflect {} {
	if {!$reflect(ok)} { return }
	set w [winfo width $win]
	set h [winfo height $win]
	set x [winfo pointerx $win]
	set y [winfo pointery $win]
	if {($reflect(w) != $w) || ($reflect(h) != $h)} {
	    $reflect(image) configure -width $w -height $h
	    set reflect(w) $w
	    set reflect(h) $h
	}
	if {($reflect(x) != $x) || ($reflect(y) != $y)} {
	    loupe $reflect(image) $x $y $w $h $options(-zoom)
	    set reflect(x) $x
	    set reflect(y) $y
	}
	if {$options(-reflect)} {
	    set reflect(id) [after $options(-reflect) [mymethod _reflect]]
	}
    }

    variable edge -array {
	at 0
	left   1
	right  2
	top    3
	bottom 4
    }
    method _edgecheck {w x y} {
	if {$w ne $ruler} { return }
	set edge(at) 0
	set cursor ""
	if {$x < 4 || $x > ([winfo width $win] - 4)} {
	    set cursor sb_h_double_arrow
	    set edge(at) [expr {$x < 4 ? $edge(left) : $edge(right)}]
	} elseif {$y < 4 || $y > ([winfo height $win] - 4)} {
	    set cursor sb_v_double_arrow
	    set edge(at) [expr {$y < 4 ? $edge(top) : $edge(bottom)}]
	}
	$win configure -cursor $cursor
    }

    variable drag -array {}
    method _dragstart {w X Y} {
	set drag(X) [expr {$X - [winfo rootx $win]}]
	set drag(Y) [expr {$Y - [winfo rooty $win]}]
	set drag(w) [winfo width $win]
	set drag(h) [winfo height $win]
	$self _edgecheck $ruler $drag(X) $drag(Y)
	raise $win
	focus $ruler
    }
    method _drag {w X Y} {
	if {$edge(at) == 0} {
	    set dx [expr {$X - $drag(X)}]
	    set dy [expr {$Y - $drag(Y)}]
	    wm geometry $win +$dx+$dy
	} elseif {$edge(at) == $edge(left)} {
	    # need to handle moving root - currently just moves
	    set dx [expr {$X - $drag(X)}]
	    set dy [expr {$Y - $drag(Y)}]
	    wm geometry $win +$dx+$dy
	} elseif {$edge(at) == $edge(right)} {
	    set relx   [expr {$X - [winfo rootx $win]}]
	    set width  [expr {$relx - $drag(X) + $drag(w)}]
	    set height $drag(h)
	    if {$width > 5} {
		wm geometry $win ${width}x${height}
	    }
	} elseif {$edge(at) == $edge(top)} {
	    # need to handle moving root - currently just moves
	    set dx [expr {$X - $drag(X)}]
	    set dy [expr {$Y - $drag(Y)}]
	    wm geometry $win +$dx+$dy
	} elseif {$edge(at) == $edge(bottom)} {
	    set rely   [expr {$Y - [winfo rooty $win]}]
	    set width  $drag(w)
	    set height [expr {$rely - $drag(Y) + $drag(h)}]
	    if {$height > 5} {
		wm geometry $win ${width}x${height}
	    }
	}
    }
}

########################################
## Ready for use

package provide widget::ruler 1.1
package provide widget::screenruler 1.2

if {[info exist ::argv0] && $::argv0 eq [info script]} {
    # We are the main script being run - show ourselves
    wm withdraw .
    set dlg [widget::screenruler .r -grid 1 -title "Screen Ruler"]
    $dlg menu add separator
    $dlg menu add command -label "Exit" -command { exit }
    $dlg display
}
