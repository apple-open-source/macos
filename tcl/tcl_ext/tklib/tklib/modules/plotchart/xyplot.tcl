# xyplot.tcl --
#     Interactive layer for plotting XY graphs via Plotchart
#     Package provided by Jos Decoster
#
package require Tcl 8.5
package require Tk 8.5
package require Plotchart 1.5
package require cmdline
package provide xyplot 1.0.1

namespace eval xyplot {
    proc ::xyplot { path args } {
	return [::xyplot::create $path {*}$args]
    }
}

proc xyplot::create { path args } {
    variable data

    set options {
	{xinteger.arg         0          "Integer X-axis"}
	{xtext.arg            ""         "Set x-axis text"}
	{xticks.arg           "10"       "Set number of x-axis ticks"}
	{xformat.arg          ""         "Set x-scale format"}
	{yinteger.arg         0          "Integer Y-axis"}
	{ytext.arg            ""         "Set y-axis text"}
	{yticks.arg           "10"       "Set number of y-axis ticks"}
	{yformat.arg          ""         "Set y-scale format"}
	{canvasbackground.arg ""         "Set plot canvas background"}
	{background.arg       ""         "Set plot background"}
	{title.arg            ""         "Set barchart title"}
	{type.arg             "line"     "Set plot type (line/dot)"}
	{width.arg            "600"      "Set plot width"}
	{height.arg           "400"      "Set plot height"}
	{ttk.arg              0          "Use TTK widgets"}
    }
    set usage "::xyplot <path> \[options\]"
    array set params [::cmdline::getoptions args $options $usage]

    set data($path,title)    $params(title)
    set data($path,xinteger) $params(xinteger)
    set data($path,xformat)  $params(xformat)
    set data($path,yformat)  $params(yformat)
    set data($path,xtext)    $params(xtext)
    set data($path,yinteger) $params(yinteger)
    set data($path,ytext)    $params(ytext)
    set data($path,xticks)   $params(xticks)
    set data($path,yticks)   $params(yticks)
    set data($path,cbg)      $params(canvasbackground)
    set data($path,bg)       $params(background)
    set data($path,type)     $params(type)
    set data($path,ttk)      $params(ttk)

    if {$data($path,ttk)} {
	set f [ttk::frame $path]
	set c [canvas $f.c -xscrollcommand [list xyplot::sbx_set $f] -yscrollcommand [list xyplot::sby_set $f]]
	set sbx [ttk::scrollbar $f.sbx -orient horizontal -command [list xyplot::c_xview $f]]
	set sby [ttk::scrollbar $f.sby -orient vertical   -command [list xyplot::c_yview $f]]
    } else {
	set f [frame $path]
	set c [canvas $f.c -xscrollcommand [list xyplot::sbx_set $f] -yscrollcommand [list xyplot::sby_set $f]]
	set sbx [scrollbar $f.sbx -orient horizontal -command [list xyplot::c_xview $f]]
	set sby [scrollbar $f.sby -orient vertical   -command [list xyplot::c_yview $f]]
    }
    grid $c $sby -sticky ewns
    grid $sbx -sticky ew
    grid columnconfigure $f 0 -weight 1
    grid rowconfigure $f 0 -weight 1
    bind $c <1> [list xyplot::zoom_start $path %x %y]
    bind $c <B1-Motion> [list xyplot::zoom_move $path %x %y]
    bind $c <ButtonRelease-1> [list xyplot::zoom_end $path %x %y]
    bind $c <3> [list xyplot::unzoom $path]
    bind $c <Configure> [list xyplot::do_resize $path]
    set data($f,c) $c
    set data($f,sbx) $sbx
    set data($f,sby) $sby
    set data($f,minmax) {}
    set data($f,series) {}
    set data($f,zoomstack) {}
    draw $f

    rename $f ::$f:cmd
    proc ::$f { cmd args } "return \[eval ::xyplot::\$cmd $f \$args\]"
    bind $f <Destroy> [list ::xyplot::destroy $f]

    return $f
}

proc xyplot::destroy { path } {
    variable data
    foreach k [array names data "$path,*"] {
	unset data($k)
    }
    return
}

proc xyplot::add_data { path sid xydata args } {
    variable data
    if { [info exists data($path,after_id)] } {
        catch {after cancel $data($path,after_id)}
	unset data($path,after_id)
    }

    set options {
	{color.arg  ""         "Set data color"}
	{legend.arg ""         "Set data title"}
    }
    set usage "add_data <path> \[options\]"
    array set params [::cmdline::getoptions args $options $usage]

    lappend data($path,series) $sid
    set data($path,$sid,data) $xydata
    set data($path,$sid,legend) $params(legend)
    set data($path,$sid,color) $params(color)
    set data($path,after_id) [after idle [list xyplot::draw $path]]
    return $sid
}

proc xyplot::set_data { path sid xydata } {
    variable data
    if { [info exists data($path,after_id)] } {
        catch {after cancel $data($path,after_id)}
	unset data($path,after_id)
    }
    set data($path,$sid,data) $xydata
    set data($path,after_id) [after idle [list xyplot::draw $path]]
    return
}

proc xyplot::remove_data { path sid } {
    variable data
    if { [info exists data($path,after_id)] } {
        catch {after cancel $data($path,after_id)}
	unset data($path,after_id)
    }
    foreach k [array names data "$path,$sid,*"] {
	unset data($k)
    }
    set idx [lsearch $data($path,series) $sid]
    if {$idx>=0} {
	set data($path,series) [lreplace $data($path,series) $idx $idx]
    }
    set data($path,after_id) [after idle [list xyplot::draw $path]]
    return
}

proc xyplot::rescale { path mx Mx my My sx Sx sy Sy } {
    variable data
    if {$mx>=$Mx} {
	set Mx [expr {$mx+1.0}]
    }
    if {$sx>=$Sx} {
	set Sx [expr {$sx+1.0}]
    }
    if {$my>=$My} {
	set My [expr {$my+1.0}]
    }
    if {$sy>=$Sy} {
	set Sy [expr {$sy+1.0}]
    }
    set data($path,minmax) [list $mx $Mx $my $My $sx $Sx $sy $Sy]
    set dx [expr {($Sx-$sx)/double($data($path,xticks))}]
    set dy [expr {($Sy-$sy)/double($data($path,yticks))}]
    $data($path,xyplot) rescale [list $sx $Sx $dx] [list $sy $Sy $dy]
    sbx_set $path
    sby_set $path
    set gx $sx
    set tglx {}
    for { set x 0 } { $x <= $data($path,xticks) } { incr x } {
	lappend tglx $gx
	set gx [expr {$gx+$dx}]
    }
    set glx {}
    for { set x 0 } { $x <= $data($path,yticks) } { incr x } {
	lappend glx $tglx
    }
    set gy $sy
    set gly {}
    for { set y 0 } { $y <= $data($path,yticks) } { incr y } {
	set sgly {}
	for { set i 0 } { $i <= $data($path,xticks) } { incr i } {
	    lappend sgly $gy
	}
	lappend gly $sgly
	set gy [expr {$gy+$dy}]
    }
    $data($path,xyplot) grid $glx $gly
    $data($path,c) raise legendbg
    $data($path,c) raise legend
    $data($path,c) lower grid
    if {$data($path,type) eq "dot"} {
	$data($path,c) delete dot
	foreach sid $data($path,series) {
	    $data($path,xyplot) dotconfig $sid -scalebyvalue off -colour $data($path,$sid,color)
	    foreach {x y} $data($path,$sid,data) {
		$data($path,xyplot) dot $sid $x $y 1
	    }
	}
    }
    return
}

proc xyplot::draw { path {scale {}} } {
    variable data
    if {[info exists data($path,after_id)]} {
        catch {after cancel $data($path,after_id)}
	unset data($path,after_id)
    }
    $data($path,c) delete all
    set s [::Plotchart::createXYPlot $data($path,c) {0.0 1.0 1.0} {0.0 1.0 1.0}]

    if {[string length $data($path,title)]} {
	$s title $data($path,title)
    }
    if {[string length $data($path,bg)]} {
	$s background plot $data($path,bg)
    }
    if {[string length $data($path,cbg)]} {
	$s background axes $data($path,cbg)
    }
    if {[string length $data($path,xformat)]} {
 	$s xconfig -format $data($path,xformat)
    }
    if {[string length $data($path,yformat)]} {
 	$s yconfig -format $data($path,yformat)
    }
    $s xtext $data($path,xtext)
    $s ytext $data($path,ytext)
    set mx 0x7fffffff
    set Mx -0x7fffffff
    set my 0x7fffffff
    set My -0x7fffffff
    set first 1
    foreach sid $data($path,series) {
	if {[string length $data($path,$sid,color)]} {
	    $s dataconfig $sid -color $data($path,$sid,color)
	}
	if {[string length $data($path,$sid,legend)]} {
	    $s legend $sid $data($path,$sid,legend)
	}
	foreach {x y} $data($path,$sid,data) {
	    if {$data($path,type) ne "dot"} {
		$s plot $sid $x $y
	    }
	    if {$first || $x < $mx} {
		set mx $x
	    }
	    if {$first || $x > $Mx} {
		set Mx $x
	    }
	    if {$first || $y < $my} {
		set my $y
	    }
	    if {$first || $y > $My} {
		set My $y
	    }
	    set first 0
	}
    }
    if {$first} {
	set mx 0
	set Mx 10
	set my 0
	set My 10
    }
    set data($path,xyplot) $s
    if {[llength $scale]} {
	rescale $path $mx $Mx $my $My {*}$scale
    } else {
	rescale $path $mx $Mx $my $My $mx $Mx $my $My
    }
    return
}

proc xyplot::do_resize { path } {
    variable data
    if { [info exists data($path,after_id)] } {
        catch {after cancel $data($path,after_id)}
	unset data($path,after_id)
    }
    lassign $data($path,minmax) mx Mx my My sx Sx sy Sy
    set data($path,after_id) [after idle [list xyplot::draw $path [list $sx $Sx $sy $Sy]]]
    return
}

proc xyplot::zoom_start { path x y } {
    variable data
    set data($path,zoom_x0) [$data($path,c) canvasx $x]
    set data($path,zoom_y0) [$data($path,c) canvasy $y]
    $data($path,c) create rectangle $data($path,zoom_x0) $data($path,zoom_y0) $data($path,zoom_x0) $data($path,zoom_y0) -outline black -tag zoom_window -dash {4 4}
    return
}

proc xyplot::zoom_move { path x y } {
    variable data
    set zoom_x1 [$data($path,c) canvasx $x]
    set zoom_y1 [$data($path,c) canvasy $y]
    $data($path,c) coords zoom_window $data($path,zoom_x0) $data($path,zoom_y0) $zoom_x1 $zoom_y1
    return
}

proc xyplot::zoom_end { path x y } {
    variable data
    $data($path,c) delete zoom_window
    set zoom_x1 [$data($path,c) canvasx $x]
    set zoom_y1 [$data($path,c) canvasy $y]
    lassign [::Plotchart::pixelToCoords $data($path,c) $data($path,zoom_x0) $data($path,zoom_y0)] x0 y0
    lassign [::Plotchart::pixelToCoords $data($path,c) $zoom_x1             $zoom_y1            ] x1 y1
    if {$x0==$x1 || $y0==$y1} {
	return
    }
    set window {}
    if {$x0 <= $x1} {
	lappend window $x0 $x1
    } else {
	lappend window $x1 $x0
    }
    if {$y0 <= $y1} {
	lappend window $y0 $y1
    } else {
	lappend window $y1 $y0
    }
    lassign $data($path,minmax) mx Mx my My sx Sx sy Sy
    lassign $window nsx nSx nsy nSy
    set w  [expr {$Sx-$sx}]
    set nw [expr {$nSx-$nsx}]
    set h  [expr {$Sy-$sy}]
    set nh [expr {$nSy-$nsy}]
    if {$nw>$w} {
	set nsx $sx
	set nSx $Sx
	set nw $w
    }
    if {$nh>$h} {
	set nsy $sy
	set nSy $Sy
	set nh $h
    }
    if {$nsx < $sx || $nsx > $Sx} {
	set nsx $sx
	set nSx [expr {$sx+$nw}]
    }
    if {$nSx < $sx || $nSx > $Sx} {
	set nSx $Sx
	set nsx [expr {$Sx-$nw}]
    }
    if {$nsy < $sy || $nsy > $Sy} {
	set nsy $sy
	set nSy [expr {$sy+$nh}]
    }
    if {$nSy < $sy || $nSy > $Sy} {
	set nSy $Sy
	set nsy [expr {$Sy-$nh}]
    }

    if {$data($path,xinteger)} {
	set nsx [expr {int($nsx)}]
	set nSx [expr {int($nSx)}]
	if {[expr {$nSx-$nsx}] < $data($path,xticks)} {
	    set nSx [expr {$nsx+$data($path,xticks)}]
	}
	if {[expr {($nSx-$nsx)%$data($path,xticks)}]} {
	    set n [expr {round(($nSx-$nsx)/$data($path,xticks))}]
	    set nSx [expr {$nsx+$data($path,xticks)*$n}]
	}
    }

    if {$data($path,yinteger)} {
	set nsy [expr {int($nsy)}]
	set nSy [expr {int($nSy)}]
	if {[expr {$nSy-$nsy}] < $data($path,yticks)} {
	    set nSy [expr {$nsy+$data($path,yticks)}]
	}
	if {[expr {($nSy-$nsy)%$data($path,yticks)}]} {
	    set n [expr {round(($nSy-$nsy)/$data($path,yticks))}]
	    set nSy [expr {$nsy+$data($path,yticks)*$n}]
	}
    }

    lappend data($path,zoomstack) [list $nsx $nSx $nsy $nSy]
    rescale $path $mx $Mx $my $My $nsx $nSx $nsy $nSy
    return
}

proc xyplot::unzoom { path } {
    variable data
    if {[llength $data($path,zoomstack)] == 0} {
	return
    }
    set data($path,zoomstack) [lrange $data($path,zoomstack) 0 end-1]
    lassign $data($path,minmax) mx Mx my My sx Sx sy Sy
    set window [lindex $data($path,zoomstack) end]
    if {[llength $window]} {
	lassign $window sx Sx sy Sy
    } else {
	lassign $data($path,minmax) sx Sx sy Sy
    }
    rescale $path $mx $Mx $my $My $sx $Sx $sy $Sy
    return
}

proc xyplot::set_zoomstack { path stack } {
    variable data
    set data($path,zoomstack) $stack
    return
}

proc xyplot::sbx_set { path args } {
    variable data
    if {![info exists data($path,minmax)] || [llength $data($path,minmax)] != 8} {
	return
    }
    lassign $data($path,minmax) mx Mx my My sx Sx sy Sy
    set w [expr {double($Mx-$mx)}]
    $data($path,sbx) set [expr {($sx-$mx)/$w}] [expr {($Sx-$mx)/$w}]
    return
}

proc xyplot::sby_set { path args } {
    variable data
    if {![info exists data($path,minmax)] || [llength $data($path,minmax)] != 8} {
	return
    }
    lassign $data($path,minmax) mx Mx my My sx Sx sy Sy
    set h [expr {double($My-$my)}]
    $data($path,sby) set [expr {1-($Sy-$my)/$h}] [expr {1-($sy-$my)/$h}]
}

proc xyplot::c_xview { path mode number {unit ""} } {
    variable data
    lassign $data($path,minmax) mx Mx my My sx Sx sy Sy
    switch -exact -- $mode {
	moveto {
	    if {$number < 0} {
		set number 0.0
	    }
	    set w  [expr {double($Mx-$mx)}]
	    set sw [expr {double($Sx-$sx)}]
	    set fsw [expr {$sw/$w}]
	    if {($number+$fsw) > 1.0} {
		set number [expr {1.0 - $fsw}]
	    }
	    set sx [expr {$mx+$w*$number}]
	    set Sx [expr {$sx+$sw}]
	}
	scroll {
	    switch -exact -- $unit {
		units {
		    if {$data($path,xticks)} {
			set pfactor [expr {1.0/$data($path,xticks)}]
		    } else {
			set pfactor 0.05
		    }
		}
		pages {
		    set pfactor 1
		}
	    }
	    set sw [expr {double($Sx-$sx)}]
	    set sx [expr {$sx + $number*$sw*$pfactor}]
	    if {$sx < $mx } {
		set sx $mx
	    }
	    set Sx [expr {$sx + $sw}]
	    if {$Sx > $Mx} {
		set Sx $Mx
		set sx [expr {$Sx - $sw}]
	    }
	}
    }
    rescale $path $mx $Mx $my $My $sx $Sx $sy $Sy
    return
}

proc xyplot::c_yview { path mode number {unit ""} } {
    variable data
    lassign $data($path,minmax) mx Mx my My sx Sx sy Sy
    switch -exact -- $mode {
	moveto {
	    if {$number < 0} {
		set number 0.0
	    }
	    set w  [expr {double($My-$my)}]
	    set sw [expr {double($Sy-$sy)}]
	    set fsw [expr {$sw/$w}]
	    if {($number+$fsw) > 1.0} {
		set number [expr {1.0 - $fsw}]
	    }
	    set Sy [expr {$My-$w*$number}]
	    set sy [expr {$Sy-$sw}]
	}
	scroll {
	    switch -exact -- $unit {
		units {
		    if {$data($path,yticks)} {
			set pfactor [expr {1.0/$data($path,yticks)}]
		    } else {
			set pfactor 0.05
		    }
		}
		pages {
		    set pfactor 1
		}
	    }
	    set sw [expr {double($Sy-$sy)}]
	    set sy [expr {$sy - $number*$sw*$pfactor}]
	    if {$sy < $my } {
		set sy $my
	    }
	    set Sy [expr {$sy + $sw}]
	    if {$Sy > $My} {
		set Sy $My
		set sy [expr {$Sy - $sw}]
	    }
	}
    }
    rescale $path $mx $Mx $my $My $sx $Sx $sy $Sy
    return
}

proc xyplot::cget { path option args } {
    variable data
    switch -exact -- $option {
	canvas {
	    return $data($path,c)
	}
	data {
	    return $data($path,[lindex $args 0],data)
	}
	scale {
	    return $data($path,minmax)
	}
	series {
	    return $data($path,series)
	}
	xyplot {
	    return $data($path,xyplot)
	}
	zoomstack {
	    return $data($path,zoomstack)
	}
	default {
	    return -code error "Unknown option '$option'"
	}
    }
}

# Test
if {0} {
set xydata1 {}
set xydata2 {}
set xydata3 {}
set xydata4 {}
for { set i 0 } { $i < 1024 } { incr i } {
    lappend xydata1 [expr {$i-1000}] [expr {$i * sin($i/4096.0*3.1415*2) * (sin($i/256.0*3.1415*2))}]
    lappend xydata2 [expr {$i-1000}] [expr {$i * sin($i/4096.0*3.1415*2) * (sin($i/256.0*3.1415*2) + 0.25 * sin($i/256.0*3.1415*6))}]
    lappend xydata3 [expr {$i-1000}] [expr {$i * sin($i/4096.0*3.1415*2) * (sin($i/256.0*3.1415*2) + 0.25 * sin($i/256.0*3.1415*6) + 0.0625 * sin($i/256.0*3.1415*10))}]
    lappend xydata4 [expr {$i-1000}] [expr {$i * sin($i/4096.0*3.1415*2) * (sin($i/256.0*3.1415*2) + 0.25 * sin($i/256.0*3.1415*6) + 0.0625 * sin($i/256.0*3.1415*10) + 0.015625 * sin($i/256.0*3.1415*14))}]
}

set xyp [xyplot .xyp -xformat "%5.2f" -yformat "%5.0f" -title "XY plot testing" -background gray90 -xinteger 1 -yinteger 1]
pack $xyp -fill both -expand true

set s1 [$xyp add_data sf1 $xydata1 -legend "Serie 1 data" -color red]
set s2 [$xyp add_data sf2 $xydata2 -legend "Serie 2 data" -color green]
set s3 [$xyp add_data sf3 $xydata3 -legend "Serie 3 data" -color blue]
set s4 [$xyp add_data sf4 $xydata4 -legend "Serie 4 data" -color orange]

set xyp2 [xyplot .xyp2 -xticks 8 -yticks 4]
pack $xyp2 -fill both -expand true

set s1 [$xyp2 add_data sf1 $xydata1]
set s2 [$xyp2 add_data sf2 $xydata2]
set s3 [$xyp2 add_data sf3 $xydata3]
set s4 [$xyp2 add_data sf4 $xydata4]
}
