#!/bin/sh
# -*- tcl -*- \
exec tclsh8.5 "$0" ${1+"$@"}
# ### ### ### ######### ######### #########

## DEMO. Uses openstreetmap to show a tile-based world map.

## Call without arguments for a plain web-served map.
## Call with single argument (dir path) to use a tile cache.

## Syntax: osm ?cachedir?

## -- Note: The cache may not exist, it is automatically filled and/or
##    extended from the web-served data. This cache can grow very
##    large very quickly (I have currently seen ranging in size from
##    4K (water) to 124K (dense urban area)).

## Note: The editing of waypoints shows my inexperience with the
##       canvas. Adding points is with <1>, bound to the canvas
##       itself. Removing is with <3>, bound to the item
##       itself. However, often it doesn't work, or rather, only if a
##       add a new point X via <1> over the point of interest, and
##       then remove both X and the point of interest by using <3>
##       twice.
##
##       Oh, and removal via <1> bound the item works not at all,
##       because this triggers the global binding as well, re-adding
##       the point immediately after its removal. Found no way of
##       blocking that.
##
## Note: Currently new point can be added only at the end of the
##       trail. No insertion in the middle possible, although deletion
##       in the middle works. No moving points, yet.
##
## Note: This demo is reaching a size there it should be shifted to
##       tclapps for further development, and cleaned up, with many of
##       the messes encapsulated into snit types or other niceties,
##       separate packages, etc.


## Ideas:
## == DONE ==
## -- Add zoom-control to switch between zoom levels. This has to
##    adjust the scroll-region as well. The control can be something
##    using basic Tk widgets (scale, button), or maybe some constructed
##    from canvas items, to make the map look more like the web-based
##    map displays. For the latter we have to get viewport tracking
##    data out of the canvas::sqmap to move the item-group in sync
##    with scrolling, so that they appear to stay in place.
##
## == DONE ==
## -- Add a filesystem based tile cache to speed up their loading. The
##    pure http access is slow (*) OTOH, this makes the workings of
##    sqmap more observable, as things do not happen as fast as for
##    puzzle and city. (*) The xy store generates some output so you
##    can see that something is happening.
##
## -- Yes, it is possible to use google maps as well. Spying on a
##    browser easily shows the urls needed. But, they are commercial,
##    and some of the servers (sat image data) want some auth cookie.
##    Without they deliver a few proper tiles and then return errors.
##
##    Hence this demo uses the freely available openstreetmap(.org)
##    data instead.
##
## == DONE ==
## -- Select two locations, then compute the geo distance between
##    them. Or, select a series of location, like following a road,
##    and compute the partial and total distances.

## == DONE == (roughly)
## -- Mark, save, load series of points (gps tracks, own tracks).
##    Name point series. Name individual points (location marks).

# ### ### ### ######### ######### #########
## Other requirements for this example.

package require Tcl 8.5
package require Tk
package require widget::scrolledwindow
package require canvas::sqmap
package require canvas::zoom
package require crosshair
package require img::png
package require tooltip

package require map::slippy             ; # Slippy utilities
package require map::slippy::fetcher    ; # Slippy server access
package require map::slippy::cache      ; # Local slippy tile cache
#package require map::slippy::prefetcher ; # Agressive prefetch

package require snit             ; # canvas::sqmap dependency
package require uevent::onidle   ; # ditto
package require cache::async 0.2 ; # ditto

set defaultLocations {
}
set cities {
    "Aachen" {50.7764185111 6.086769104}
    "Anchorage" {61.218333 -149.899167}
    "Banff" {51.1653    -115.5322}
    "Beijing" {39.913889 116.391667}
    "Boston " {42.35 -71.066666}
    "Buenos Aires" {-34.603333 -58.381667}
    "Chicago" {41.8675 -87.6243}
    "Denver" {39.75 -104.98}
    "Honolulu" {21.31 -157.83}
    "Johannesburg" {-26.204444 28.045556}
    "London" {51.508056 -0.124722}
    "Los Angeles" {34.054 -118.245}
    "Mexico City" {19.433333 -99.133333}
    "Moscow" {55.751667 37.617778}
    "New York" {40.7563 -73.9865}
    "Palo Alto" {37.429167 -122.138056}
    "Paris" {48.856667 2.350833}
    "San Francisco" {37.77 -122.43}
    "Sydney" {-33.859972 151.211111}
    "Tokyo" {35.700556 139.715}
    "Vancouver (Lost Lagoon)" {49.30198   -123.13724}
    "Washington DC" {38.9136 -77.0132}
}

# ### ### ### ######### ######### #########

proc Main {} {
    InitModel
    GUI
    LoadInitialMarks

    # Hack to get display to show nicely while the initial maps are
    # loading
    set gridInfo [grid info .sw]
    grid forget .sw
    update
    grid .sw {*}$gridInfo

    SetRegion $::zoom ; # Force initial region as the zoom control
                        # will not call us initially, only on
                        # future changes.
    GetInitialMark
}

# ### ### ### ######### ######### #########

proc InitModel {} {
    global argv cachedir loaddir provider zoom

    set zoom     12
    set cachedir ""
    set loaddir  [pwd]

    # OpenStreetMap. Mapnik rendered tiles.
    # alternative  http://tah.openstreetmap.org/Tiles/tile

    if {"FETCH" in [info commands]} { rename FETCH {}} ;# KPV, allow re-loading
    set provider [map::slippy::fetcher FETCH 19 http://tile.openstreetmap.org]

    # Nothing to do if no cache is specified, and fail for wrong#args

    if {![llength $argv]} return
    if {[llength $argv] > 1} Usage

    # A cache is specified. Create the directory, if necessary, and
    # initialize the necessary objects.

    set cachedir [lindex $argv 0]
    set loaddir  $cachedir
    set provider [map::slippy::cache CACHE $cachedir FETCH]

    # Pre-filling the cache based on map requests. Half-baked. Takes
    # currently to much cycles from the main requests themselves.  set
    #provider [map::slippy::prefetcher PREFE CACHE]
    return
}

proc Usage {} {
    global argv0
    puts stderr "wrong\#args, expected: $argv0 ?cachedir?"
    exit 1
}

# ### ### ### ######### ######### #########

proc GUI {} {
    global provider
    # ---------------------------------------------------------
    # The gui elements, plus connections.

    widget::scrolledwindow .sw
    widget::scrolledwindow .sl

    set th [$provider tileheight]
    set tw [$provider tilewidth]

    canvas::sqmap          .map   -closeenough 3 \
        -viewport-command VPTRACK -grid-cell-command GET \
        -grid-cell-width $tw -grid-cell-height $th -bg yellow

    canvas::zoom           .z    -variable ::zoom -command ZOOM \
        -orient vertical -levels [$provider levels]

    label                  .loc  -textvariable ::location \
        -bd 2 -relief sunken -bg white -width 20 -anchor w
    label                  .dist  -textvariable ::distance \
        -bd 2 -relief sunken -bg white -width 20 -anchor w

    listbox                .lm   -listvariable ::locations \
        -selectmode single -exportselection 0

    button                 .exit -command exit        -text Exit
    button                 .goto -command GotoMark    -text Goto
    button                 .clr  -command ClearPoints -text {Clear Points}
    button                 .ld   -command LoadPoints  -text {Load Points}
    button                 .sv   -command SavePoints  -text {Save Points}

    .sw setwidget .map
    .sl setwidget .lm

    # ---------------------------------------------------------
    # layout of the elements

    grid .sl   -row 1 -column 0 -sticky swen -columnspan 2
    #grid .z    -row 1 -column 2 -sticky wen
    grid .sw   -row 1 -column 3 -sticky swen -columnspan 6

    place .z -in .map -x .2i -y .2i -anchor nw

    grid .exit -row 0 -column 0 -sticky wen
    grid .goto -row 0 -column 1 -sticky wen
    grid .clr  -row 0 -column 3 -sticky wen
    grid .ld   -row 0 -column 4 -sticky wen
    grid .sv   -row 0 -column 5 -sticky wen
    grid .loc  -row 0 -column 6 -sticky wen
    grid .dist -row 0 -column 7 -sticky wen

    grid rowconfigure . 0 -weight 0
    grid rowconfigure . 1 -weight 1

    grid columnconfigure . 0 -weight 0
    grid columnconfigure . 1 -weight 0
    grid columnconfigure . 2 -weight 0
    grid columnconfigure . 3 -weight 0
    grid columnconfigure . 8 -weight 1

    # ---------------------------------------------------------
    # Behaviours

    # Panning via mouse
    bind .map <ButtonPress-2> {%W scan mark   %x %y}
    bind .map <B2-Motion>     {%W scan dragto %x %y}

    # Mark/unmark a point on the canvas
    bind .map <1> {RememberPoint %x %y}

    # Double clicking location selects it
    bind .lm <Double-Button-1> GotoMark

    # Double-clicking right button centers map to mouse location.
    bind .map <Double-Button-3> GotoMouse

    # Cross hairs ...
    .map configure -cursor tcross
    crosshair::crosshair .map -width 0 -fill \#999999 -dash {.}
    crosshair::track on  .map TRACK

    # ---------------------------------------------------------
    return
}

# ### ### ### ######### ######### #########

set location  "location" ; # geo location of the mouse in the canvas (crosshair)
set distance  "distance" ; # distance between marks

proc VPTRACK {xl yt xr yb} {
    # args = viewport, pixels, see also canvas::sqmap, SetPixelView.
    global viewport
    set viewport [list $xl $yt $xr $yb]
    #puts VP-TRACK($viewport)
    return
}

proc TRACK {win x y args} {
    # args = viewport, pixels, see also canvas::sqmap, SetPixelView.
    global location zoom clat clon

    # Convert pixels to geographic location.
    set point [list $zoom $y $x]
    foreach {_ clat clon} [map::slippy point 2geo $point] break

    # Update entry field.
    set location [PrettyLatLon $clat $clon]
    return
}

# ### ### ### ######### ######### #########
# Basic callback structure, log for logging, facade to transform the
# cache/tiles result into what xcanvas is expecting.

proc GET {__ at donecmd} {
    global provider zoom
    set tile [linsert $at 0 $zoom]

    if {![map::slippy tile valid $tile [$provider levels]]} {
        GOT $donecmd unset $tile
        return
    }

    #puts "GET ($tile) ($donecmd)"
    $provider get $tile [list GOT $donecmd]
    return
}

proc GOT {donecmd what tile args} {
    #puts "\tGOT $donecmd $what ($tile) $args"
    set at [lrange $tile 1 end]
    if {[catch {
        uplevel #0 [eval [linsert $args 0 linsert $donecmd end $what $at]]
    }]} { puts $::errorInfo }
    return
}

# ### ### ### ######### ######### #########

proc ZOOM {w level} {
    # The variable 'zoom' is already set to level, as the -variable of
    # our zoom control .z

    #puts ".z = $level"

    SetRegion $level
    ShowPoints
    return
}

proc SetRegion {level} {
    set rlength [map::slippy length $level]
    set region  [list 0 0 $rlength $rlength]

    .map configure -scrollregion $region
    return
}

# ### ### ### ######### ######### #########

proc Goto {geo} {
    global zoom

    #puts Jump($geo)

    # The geo location is converted to pixels, then to a fraction of
    # the scrollregion. This is adjusted so that the fraction
    # specifies the center of the viewed region, and not the upper
    # left corner. for this translation we need the viewport data of
    # VPTRACK.

    foreach {z y x} [map::slippy geo 2point $geo] break
    set zoom $z
    after 200 [list Jigger $z $y $x]
    #.map xview moveto $ofx
    #.map yview moveto $ofy
    return
}

proc Jigger {z y x} {
    global viewport
    set len [map::slippy length $z]
    foreach {l t r b} $viewport break
    set ofy [expr {($y - ($b - $t)/2.0)/$len}]
    set ofx [expr {($x - ($r - $l)/2.0)/$len}]

    .map xview moveto $ofx
    .map yview moveto $ofy
    return
}

# ### ### ### ######### ######### #########

set points    {} ; # way-points loaded list (list (lat lon comment))
set locations {} ; # Location markers (locationmark.gps)
set lmarks    {} ; # Coordinates for items in location

proc SavePoints {} {
    global loaddir

    set chosen [tk_getSaveFile -defaultextension .gps \
                    -filetypes {
                        {GPS {.gps}}
                        {ALL {*}}
                    } \
                    -initialdir $loaddir \
                    -title   {Save waypoints} \
                    -parent .map]

    if {$chosen eq ""} return

    global points
    set lines {}
    foreach p $points {
        foreach {lat lon comment} $p break
        lappend lines [list waypoint $lat $lon $comment]
    }

    fileutil::writeFile $chosen [join $lines \n]\n
    return
}

proc LoadPoints {} {
    global loaddir

    set chosen [tk_getOpenFile -defaultextension .gps \
                    -filetypes {
                        {GPS {.gps}}
                        {ALL {*}}
                    } \
                    -initialdir $loaddir \
                    -title   {Load waypoints} \
                    -parent .map]

    if {$chosen eq ""} return
    if {[catch {
        set waypoints [fileutil::cat $chosen]
    }]} {
        return
    }

    set loaddir [file dirname $chosen]

    ClearPoints
    # Content is TRUSTED. In a proper app this has to be isolated from
    # the main system through a safe interp.
    #eval $waypoints
    ProcessFile $waypoints
    ShowPoints
    return
}
##+##########################################################################
#
# Safer way of processing our GPS file data. Only two commands
# allowed: "poi lat lon comment" and "waypoint lat lon comment"
#
proc ProcessFile {data} {
    foreach line [split $data \n] {
        set line [string trim $line]
        if {$line eq "" || [string match "#*" $line]} continue

        set n [catch {set len [llength $line]}]
        if {$n || $len != 4} {
            puts "bad line: '$line'"
            continue
        }
        lassign $line cmd lat lon comment
        if {$cmd ne "poi" && $cmd ne "waypoint"} {
            puts "bad command: '$line'"
            continue
        }
        $cmd $lat $lon $comment
    }
}

proc waypoint {lat lon comment} {
    global  points
    lappend points [list $lat $lon $comment]
    return
}

proc ShowPoints {} {
    global points zoom distance

    if {![llength $points]} return

    set cmds {}
    set cmd [list .map create line]

    set lat0 {}
    set lon0 {}
    set dist 0

    foreach point $points {
        foreach {lat lon comment} $point break
        foreach {_ y x} [map::slippy geo 2point [list $zoom $lat $lon]] break
        lappend cmd  $x $y
        lappend cmds [list POI $y $x $lat $lon $comment -fill salmon -tags Series]

        if {$lat0 ne {}} {
            set leg [GreatCircleDistance $lat0 $lon0 $lat $lon]
            set dist [expr {$dist + $leg}]
        }
        set lat0 $lat
        set lon0 $lon
    }
    lappend cmd -width 2 -tags Series -capstyle round ;#-smooth 1

    if {[llength $points] > 1} {
        set cmds [linsert $cmds 0 $cmd]
    }

    .map delete Series
    #puts [join $cmds \n]
    eval [join $cmds \n]
    set distance [PrettyDistance $dist]
    return
}
proc PrettyLatLon {lat lon} {
    return [format "%.6f %.6f" $lat $lon]
}

global pcounter
set pcounter 0
proc RememberPoint {x y} {
    #puts REMEMBER///
    global pcounter zoom
    incr   pcounter

    set point [list $zoom [.map canvasy $y] [.map canvasx $x]]
    foreach {_ lat lon} [map::slippy point 2geo $point] break
    lassign [PrettyLatLon $lat $lon] lat lon

    set comment "$pcounter:<$lat,$lon>"
    #puts $x/$y/$lat/$lon/$comment/$pcounter

    global  points
    lappend points [list $lat $lon $comment $pcounter]
    ShowPoints

    # This is handled weird. Placing the mouse on top of a point
    # doesn't trigger, however when I create a new point <1> at the
    # position, and then immediately after use <3> I can remove the
    # new point, and the second click the point underneath triggers as
    # well. Could this be a stacking issue?
    .map bind T/$comment <3> "[list ForgetPoint $pcounter];break"

    # Alternative: Bind <3> and the top level and use 'find
    # overlapping'. In that case however either we, or the sqmap
    # should filter out the background items.

    return
}

proc ForgetPoint {pid} {

    #    puts [.map find overlapping $x $y $x $y]
    #return

    #puts //FORGET//$pid

    global points
    set pos -1
    foreach p $points {
        incr pos
        foreach {lat lon comment id} $p break
        if {$id != $pid} continue
        #puts \tFound/$pos
        set points [lreplace $points $pos $pos]
        if {![llength $points]} {
            ClearPoints
        } else {
            ShowPoints
        }
        return
    }
    #puts Missed
    return
}
# See http://wiki.tcl.tk/8447
proc GreatCircleDistance {lat1 lon1 lat2 lon2} {
    set y1 $lat1
    set x1 $lon1
    set y2 $lat2
    set x2 $lon2

    set pi [expr {acos(-1)}]
    set x1 [expr {$x1 *2*$pi/360.0}]            ;# Convert degrees to radians
    set x2 [expr {$x2 *2*$pi/360.0}]
    set y1 [expr {$y1 *2*$pi/360.0}]
    set y2 [expr {$y2 *2*$pi/360.0}]
    # calculate distance:
    ##set d [expr {acos(sin($y1)*sin($y2)+cos($y1)*cos($y2)*cos($x1-$x2))}]
    set d [expr {sin($y1)*sin($y2)+cos($y1)*cos($y2)*cos($x1-$x2)}]
    if {abs($d) > 1.0} {                        ;# Rounding error
        set d [expr {$d > 0 ? 1.0 : -1.0}]
    }
    set d [expr {acos($d)}]

    set meters [expr {20001600/$pi*$d}]
    return $meters
}
proc PrettyDistance {dist} {
    if {$dist == 0} { return "distance" }
    set meters [expr {round($dist)}]
    if {$meters == 1} { return "1 meter"}
    if {$meters < 1000} { return "$meters meters"}
    return [format "%.1f km" [expr {$dist/1000.0}]]
}
proc POI {y x lat lon comment args} {
    set x1 [expr { $x + 6 }]
    set y1 [expr { $y + 6 }]
    set x  [expr { $x - 6 }]
    set y  [expr { $y - 6 }]

    set id [eval [linsert $args 0 .map create oval $x $y $x1 $y1]]
    if {$comment eq ""} return
    tooltip::tooltip .map -item $id $comment
    .map addtag T/$comment withtag $id
    return
}

proc ClearPoints {} {
    global points
    set points {}
    .map delete Series
    set ::distance "distance"
    return
}

proc LoadInitialMarks {} {
    foreach {name latlon} $::cities {
        lassign $latlon lat lon
        poi $lat $lon $name
    }
}

proc ClearMarks {} {
    global lmarks locations
    set lmarks {}
    set locations {}
    return
}

proc poi {lat lon comment} {
    global lmarks locations
    lappend lmarks [list $lat $lon]
    lappend locations $comment
    return
}

proc ShowMarks {} {
    # locations traced by .lm
    return
}

proc GotoMouse {} {
    global clat clon zoom
    Goto [list $zoom $clat $clon]
    return
}

proc GotoMark {} {
    global lmarks zoom
    set sel [.lm curselection]
    if {![llength $sel]} return
    set sel [lindex $sel 0]
    set sel [lindex $lmarks $sel]
    foreach {lat lon} $sel break
    Goto [list $zoom $lat $lon]
    return
}
proc GetInitialMark {} {
    set n [expr {int(rand()*[llength $::locations])}]
    .lm selection clear 0 end
    .lm selection set $n
    .lm selection anchor $n
    GotoMark
}
# ### ### ### ######### ######### #########

proc ShowGrid {} {
    # Activating the grid leaks items = memory
    .map configure -grid-show-borders 1
    .map flush
    return
}

# ### ### ### ######### ######### #########
# ### ### ### ######### ######### #########
Main

