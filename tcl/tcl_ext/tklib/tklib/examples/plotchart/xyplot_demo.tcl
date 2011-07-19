# xyplot_demo.tcl --
#     Demonstration of the xyplot package
#

package require xyplot

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

set xyp [xyplot .xyp -xformat "%5.0f" -yformat "%5.0f" -title "XY plot testing" -background gray90]
pack $xyp -fill both -expand true

set s1 [$xyp add_data sf1 $xydata1 -legend "Serie 1 data" -color red]
set s2 [$xyp add_data sf2 $xydata2 -legend "Serie 2 data" -color green]
set s3 [$xyp add_data sf3 $xydata3 -legend "Serie 3 data" -color blue]
set s4 [$xyp add_data sf4 $xydata4 -legend "Serie 4 data" -color orange]

set xyp2 [xyplot .xyp2 -xticks 8 -yticks 4 -yformat %.2f -xformat %.0f]
pack $xyp2 -fill both -expand true

set s1 [$xyp2 add_data sf1 $xydata1]
set s2 [$xyp2 add_data sf2 $xydata2]
set s3 [$xyp2 add_data sf3 $xydata3]
set s4 [$xyp2 add_data sf4 $xydata4]
