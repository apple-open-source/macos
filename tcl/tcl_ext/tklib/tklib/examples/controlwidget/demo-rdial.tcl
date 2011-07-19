# demo-rdial.tcl --

package require controlwidget

array set disp_value {rs -30.0 rh 120.0 rv 10.0}

proc rndcol {} {
    set col "#"
    for {set i 0} {$i<3} {incr i} {
        append col [format "%02x" [expr {int(rand()*230)+10}]]
    }
    return $col
}
proc set_rand_col {} {
    .rs configure -fg [rndcol] -bg [rndcol]
}
proc show_value {which v} {
    set val [.$which cget -value]
    set ::disp_value($which) [format "%.1f" $val]
    switch -- $which {
        "rh" {
            if {abs($val)<30} return
            .rs configure -width [expr {abs($val)}]
        }
        "rv" {
            if {abs($val)<5}  return
            .rs configure -height [expr {abs($val)}]
        }
        "rs" {
            if {!(int($val)%10)} set_rand_col
        }
    }
}
label .lb -text "Use mouse button with Shift &\nControl for dragging the dials"
label .lv -textvariable disp_value(rv)
controlwidget::rdial .rv -callback {show_value rv} -value $disp_value(rv)\
        -width 200 -step 5 -bg blue -fg white \
        -variable score
label .lh -textvariable disp_value(rh)
controlwidget::rdial .rh -callback {show_value rh} -value $disp_value(rh)\
        -width $disp_value(rh) -height 20 -fg blue -bg yellow -orient vertical
label .ls -textvariable disp_value(rs)
controlwidget::rdial .rs -callback {show_value rs} -value $disp_value(rs)\
        -width $disp_value(rh) -height $disp_value(rv)
pack {*}[winfo children .]
wm minsize . 220 300

after 2000 {
    set ::score 0.0
}
after 3000 {
    set ::score 100.0
    .rh set 3
}
