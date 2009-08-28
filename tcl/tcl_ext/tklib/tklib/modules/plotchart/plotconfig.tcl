# plotconfig.tcl --
#     Facilities for configuring the various procedures of Plotchart
#

namespace eval ::Plotchart {
    variable config

    set config(charttypes) {xyplot xlogyplot piechart polarplot
                            histogram horizbars vertbars ganttchart
                            timechart stripchart isometric 3dplot 3dbars
                            radialchart txplot 3dribbon}

    set config(xyplot,components)      {title margin text legend leftaxis rightaxis bottomaxis background}
    set config(xlogyplot,components)   {title margin text legend leftaxis bottomaxis background}
    set config(piechart,components)    {title margin text legend labels background}
    set config(polarplot,components)   {title margin text legend axis background}
    set config(histogram,components)   {title margin text legend leftaxis rightaxis bottomaxis background}
    set config(horizbars,components)   {title margin text legend leftaxis bottomaxis background}
    set config(vertbars,components)    {title margin text legend leftaxis bottomaxis background}
    set config(ganttchart,components)  {title margin text legend axis background}
    set config(timechart,components)   {title margin text legend leftaxis bottomaxis background}
    set config(stripchart,components)  {title margin text legend leftaxis bottomaxis background}
    set config(isometric,components)   {title margin text legend leftaxis bottomaxis background}
    set config(3dplot,components)      {title margin text legend xaxis yaxis zaxis background}
    set config(3dbars,components)      {title margin text legend leftaxis bottomaxis background}
    set config(radialchart,components) {title margin text legend leftaxis bottomaxis background}
    set config(txplot,components)      {title margin text legend leftaxis rightaxis bottomaxis background}
    set config(3dribbon,components)    {title margin text legend leftaxis bottomaxis background}

    set config(axis,properties)        {color thickness font format ticklength textcolor}
    set config(leftaxis,properties)    $config(axis,properties)
    set config(rightaxis,properties)   $config(axis,properties)
    set config(topaxis,properties)     $config(axis,properties)
    set config(bottomaxis,properties)  $config(axis,properties)
    set config(xaxis,properties)       $config(axis,properties)
    set config(yaxis,properties)       $config(axis,properties)
    set config(zaxis,properties)       $config(axis,properties)
    set config(margin,properties)      {left right top bottom}
    set config(title,properties)       {textcolor font anchor}
    set config(text,properties)        {textcolor font anchor}
    set config(labels,properties)      {textcolor font}
    set config(background,properties)  {outercolor innercolor}
    set config(legend,properties)      {background border position}

    # TODO: default values
    canvas .invisibleCanvas
    set invisibleLabel [.invisibleCanvas create text 0 0 -text ""]

    set _color      "black"
    set _font       [.invisibleCanvas itemcget $invisibleLabel -font]
    set _thickness  1
    set _format     ""
    set _ticklength 3
    set _textcolor  "black"
    set _anchor     n
    set _left       80
    set _right      40
    set _top        28
    set _bottom     30
    set _bgcolor    "white"
    set _outercolor "white"
    set _innercolor "white"  ;# Not implemented yet: "$w lower data" gets in the way
    set _background "white"
    set _border     "black"
    set _position   "top-right"

    destroy .invisibleCanvas

    foreach type $config(charttypes) {
        foreach comp $config($type,components) {
            foreach prop $config($comp,properties) {
                set config($type,$comp,$prop)         [set _$prop]
                set config($type,$comp,$prop,default) [set _$prop]
            }
        }
    }
}

# plotconfig --
#     Set or query general configuration options of Plotchart
#
# Arguments:
#     charttype         Type of plot or chart or empty (optional)
#     component         Component of the type of plot or chart or empty (optional)
#     property          Property of the component or empty (optional)
#     value             New value of the property if given (optional)
#                       (if "default", default is restored)
#
# Result:
#     No arguments: list of supported chart types
#     Only chart type given: list of components for that type
#     Chart type and component given: list of properties for that component
#     Chart type, component and property given: current value
#     If a new value is given, nothing
#
# Note:
#     The command contains a lot of functionality, but its structure is
#     fairly simple. No property has an empty string as a sensible value.
#
proc ::Plotchart::plotconfig {{charttype {}} {component {}} {property {}} {value {}}} {
    variable config

    if { $charttype == {} } {
        return $config(charttypes)
    } else {
        if { [lsearch $config(charttypes) $charttype] < 0 } {
            return -code error "Unknown chart type - $charttype"
        }
    }

    if { $component == {} } {
        return $config($charttype,components)
    } else {
        if { [lsearch $config($charttype,components) $component] < 0 } {
            return -code error "Unknown component '$component' for this chart type - $charttype"
        }
    }

    if { $property == {} } {
        return $config($component,properties)
    } else {
        if { [lsearch $config($component,properties) $property] < 0 } {
            return -code error "Unknown property '$property' for this component '$component' (chart: $charttype)"
        }
    }

    if { $value == {} } {
        return $config($charttype,$component,$property)
    } elseif { $value == "default" } {
        set config($charttype,$component,$property) \
            $config($charttype,$component,$property,default)
        return $config($charttype,$component,$property)
    } else {
        if { $value == "none" } {
            set value ""
        }
        set config($charttype,$component,$property) $value
    }
}

# CopyConfig --
#     Copy the configuration options to a particular plot/chart
#
# Arguments:
#     charttype         Type of plot or chart
#     chart             Widget of the actual chart
#
# Result:
#     None
#
# Side effects:
#     The configuration options are available for the particular plot or
#     chart and can be modified specifically for that plot or chart.
#
proc ::Plotchart::CopyConfig {charttype chart} {
    variable config

    foreach {prop value} [array get config $charttype,*] {
        set chprop [string map [list $charttype, $chart,] $prop]
        set config($chprop) $value
    }
}
