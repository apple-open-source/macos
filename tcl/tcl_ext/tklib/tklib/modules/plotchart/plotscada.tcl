# plotscada.tcl --
#     Facilities for plotting "SCADA" displays:
#     - Schematic drawings of a factory for instance
#       and measured values in the various parts
#     - Telemetry systems
#

source plotchart.tcl

namespace eval ::Plotchart {

   set methodProc(scada,scaling)           ScadaScaling
   set methodProc(scada,axis)              ScadaAxis
   set methodProc(scada,object)            ScadaObject
   set methodProc(scada,plot)              ScadaPlot
   set methodProc(scada,angular-scaling)   ScadaAngularScaling
}

# createScada --
#     Create a new command for plotting SCADA displays
#
# Arguments:
#    w             Name of the canvas
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the display
#
proc ::Plotchart::createScada { w } {
    variable scada_scaling

    set newchart "scada_$w"
    interp alias {} $newchart {} ::Plotchart::PlotHandler scada $w
    #CopyConfig scada $w

    set pxmin 0
    set pxmax [expr {[WidthCanvas $w]  - 1}]
    set pymin [expr {[HeightCanvas $w] - 1}]
    set pymax 0

    # Default scaling: use pixels - not an error
    ScadaScaling $w default [list $pxmin $pymax $pxmax $pymin] \
                            [list $pxmin $pymin $pxmax $pymax]

    return $newchart
}


# ScadaScaling --
#     Create a new scaling for the SCADA display
#
# Arguments:
#    w             Name of the canvas
#    name          Name of the scaling
#    rectangle     Rectangle in the window (pixels)
#    worldcoords   Associated world coordinates
# Result:
#    None
# Note:
#    Order of the pixel coordinates: xmin, ymin, xmax, ymax
#    Ditto for the world coordinates, but the pixel y-coordinate
#    is actually inverted
#
proc ::Plotchart::ScadaScaling { w name rectangle worldcoords } {

    viewPort         $w,$name {*}$rectangle
    worldCoordinates $w,$name {*}$worldcoords

}


# ScadaObject --
#     Create a new object in the SCADA display
#
# Arguments:
#    w             Name of the canvas
#    type          Type of the object
#    objname       Name of the object
#    coords        List of coordinates
#    args          List of all remaining arguments
# Result:
#    None
# Side effects:
#    A new object is created on the canvas and its properties are stored
#
proc ::Plotchart::ScadaObject { w type objname coords args} {
    variable scada_scaling
    variable scada_object

    set scada_object($w,$objname,scaling) default

    set options {}
    foreach {key value} $args {
        switch -- $key {
            "-scaling" {
                set scada_object($w,$objname,scaling) $value
            }
            "-text" {
                set scada_object($w,$objname,text) $value
            }
            default {
                lappend options $key $value
            }
        }
    }

    switch -- $type {
        "rectangle" {
            set scada_object($w,$objname,canvasid) \
                [$w create rectangle {-10 -10 -10 -10} {*}$options]
        }
        "line" {
            set scada_object($w,$objname,canvasid) \
                [$w create line {-10 -10 -10 -10} {*}$options]
        }
        "polygon" {
            set scada_object($w,$objname,canvasid) \
                [$w create polygon {-10 -10 -10 -10 -10 -10} {*}$options]
        }
        "text" {
            set scada_object($w,$objname,canvasid) \
                [$w create text {-10 -10} -text $scada_object($w,$objname,text) {*}$options]
        }
        default {
            return -code error "Unknown object type: $type"
        }
    }

    set scada_object($w,$objname,coords) $coords
    set scada_object($w,$objname,type)   $type
}


# ScadaPlot --
#     Change the properties (coordinates or text) of a SCADA object
#
# Arguments:
#    w             Name of the canvas
#    objname       Name of the object
#    args          List of the parameters that need to be changed
# Result:
#    None
# Side effects:
#    The coordinates or the text are changed
#
proc ::Plotchart::ScadaPlot { w objname args} {
    variable scada_scaling
    variable scada_object

    set wcoords $scada_object($w,$objname,coords)

    set replace 0
    set index   0
    foreach coord $wcoords {
        if { $coord eq "*" } {
            lset wcoords $index [lindex $args $replace]
            incr replace
        }
        incr index
    }

    set scaling $w,$scada_object($w,$objname,scaling)

    set coords {}
    foreach {x y} $wcoords {
        if {[catch {
        foreach {px py} [coordsToPixel $scaling $x $y] {break}
        lappend coords $px $py
        } msg] } {
            puts "problem: $scaling -- $x -- $y"
        }
    }

    $w coords $scada_object($w,$objname,canvasid) $coords

    if { $scada_object($w,$objname,type) eq "text" } {
        if { $scada_object($w,$objname,text) eq "*" } {
            set text [lindex $args end]
            $w itemconfig $scada_object($w,$objname,canvasid) -text $text
        }
    }
}

# main --
#     Simple test
#

if {0} {
pack [canvas .c -height 400]

set p [::Plotchart::createScada .c]


$p scaling height {0 0 40 300} {0 0 40 1.5}

$p object rectangle box {10 0.0 30 *} -fill green -outline black -width 3 -scaling height
$p object text      label {35 *} -text * -anchor w                       -scaling height

$p plot box 1.2
$p plot label 1.2 "1.2"

after 2000 {
    $p plot box 0.6
    $p plot label 0.6 "0.6"
}
}

#
# Display a simple vessel
#
pack [canvas .c -height 300 -width 600]

set p [::Plotchart::createScada .c]

scale .c.scalea -orient vertical -from 0.10 -to 0.0 -variable ratea -tickinterval 0.02 -digits 3 -resolution 0.001
scale .c.scaleb -orient vertical -from 0.10 -to 0.0 -variable rateb -tickinterval 0.02 -digits 3 -resolution 0.001
button .c.start -text Start -command {startComputation} -width 10
button .c.stop  -text Stop  -command {stopComputation}  -width 10

.c create window  10  30 -window .c.scalea -anchor nw
.c create window 110  70 -window .c.scaleb -anchor nw
.c create window  10 200 -window .c.start  -anchor nw
.c create window 110 200 -window .c.stop   -anchor nw

.c create line { 20  40  230   40} -width 2 -arrow last -fill red
.c create text  230  30  -text "Component A" -anchor e  -fill red
.c create line {200  70  230   70} -width 2 -arrow last -fill blue
.c create text  230  60  -text "Component B" -anchor e  -fill blue
.c create line {325 210  360  210} -width 2 -arrow last
.c create text  340 200  -text "Product C" -anchor w
.c create polygon {240  10 240  10
                   240 240 240 240
                   280 260
                   320 240 320 240
                   320  10 320  10} \
     -fill orange -smooth 1 -outline black -width 2

.c create line      {280   0 280 200} -width 2
.c create line      {270 200 290 200} -width 2
.c create rectangle {250 190 270 210} -width 2 -fill black
.c create rectangle {290 190 310 210} -width 2 -fill black


$p scaling temperature {400  22 428  148} {0.0 10.0 1.0 100.0}
.c create rectangle {400 20 430 150} -width 2 -fill white -outline black

$p object rectangle thermometer {0.0 10.0 1.0 *} -scaling temperature -fill red
$p object text temperature {1.3 *} -text * -scaling temperature -anchor w

$p scaling concentration {480 102 510 248} {0.0 0.0 1.0 1.0}
.c create rectangle {480 100 510 250} -width 2 -fill yellow -outline black

$p object rectangle conca   { 0.0 0.0  1.0 *} -fill red    -scaling concentration
$p object rectangle concb   { 0.0   *  1.0 *} -fill blue   -scaling concentration

$p object text      labela  {1.3 *} -text A      -scaling concentration
$p object text      labelb  {1.3 *} -text B      -scaling concentration
$p object text      labelc  {1.3 *} -text C      -scaling concentration

$p object line      maxline {-0.1   *  1.4 *}              -scaling concentration
$p object text      maximum { 1.4   *}       -text Maximum -scaling concentration -anchor w

$p plot maxline 0.25 0.25
$p plot maximum 0.25
$p plot labelc  1.0

$p plot thermometer 25.0
$p plot temperature 25.0 "25"

$p plot conca  0.1
$p plot concb  0.1 0.3
$p plot labela 0.1
$p plot labelb 0.3

