# plotchart.tcl --
#    Facilities to draw simple plots in a dedicated canvas
#
# Note:
#    This source file contains the public functions.
#    The private functions are contained in the files "sourced"
#    at the end.
#
package require Tcl 8.4
package require Tk

# Plotchart --
#    Namespace to hold the procedures and the private data
#
namespace eval ::Plotchart {
   variable settings
   variable legend
   variable scaling
   variable methodProc
   variable data_series

   namespace export worldCoordinates viewPort coordsToPixel \
                    polarCoordinates setZoomPan \
                    world3DCoordinates coordsToPixel \
                    coords3DToPixel polarToPixel \
                    pixelToCoords pixelToIndex \
                    determineScale determineScaleFromList \
                    createXYPlot createPolarplot createPiechart \
                    createBarchart createHorizontalBarchart \
                    createTimechart createStripchart \
                    createIsometricPlot create3DPlot \
                    createGanttChart createHistogram colorMap \
                    create3DBars createRadialchart \
                    createTXPlot createRightAxis \
                    create3DRibbonChart create3DRibbonPlot \
                    createXLogYPlot createLogXYPlot createLogXLogYPlot \
                    createWindrose createTargetDiagram createPerformanceProfile \
                    plotconfig plotpack plotmethod

   #
   # Array linking procedures with methods
   #
   set methodProc(xyplot,title)             DrawTitle
   set methodProc(xyplot,xtext)             DrawXtext
   set methodProc(xyplot,ytext)             DrawYtext
   set methodProc(xyplot,vtext)             DrawVtext
   set methodProc(xyplot,plot)              DrawData
   set methodProc(xyplot,dot)               DrawDot
   set methodProc(xyplot,dotconfig)         DotConfigure
   set methodProc(xyplot,interval)          DrawInterval
   set methodProc(xyplot,trend)             DrawTrendLine
   set methodProc(xyplot,vector)            DrawVector
   set methodProc(xyplot,vectorconfig)      VectorConfigure
   set methodProc(xyplot,rchart)            DrawRchart
   set methodProc(xyplot,grid)              DrawGrid
   set methodProc(xyplot,contourlines)      DrawIsolines
   set methodProc(xyplot,contourfill)       DrawShades
   set methodProc(xyplot,contourbox)        DrawBox
   set methodProc(xyplot,saveplot)          SavePlot
   set methodProc(xyplot,dataconfig)        DataConfig
   set methodProc(xyplot,xconfig)           XConfig
   set methodProc(xyplot,yconfig)           YConfig
   set methodProc(xyplot,xticklines)        DrawXTicklines
   set methodProc(xyplot,yticklines)        DrawYTicklines
   set methodProc(xyplot,background)        BackgroundColour
   set methodProc(xyplot,legendconfig)      LegendConfigure
   set methodProc(xyplot,legend)            DrawLegend
   set methodProc(xyplot,balloon)           DrawBalloon
   set methodProc(xyplot,balloonconfig)     ConfigBalloon
   set methodProc(xyplot,plaintext)         DrawPlainText
   set methodProc(xyplot,plaintextconfig)   ConfigPlainText
   set methodProc(xyplot,bindvar)           BindVar
   set methodProc(xyplot,bindcmd)           BindCmd
   set methodProc(xyplot,rescale)           RescalePlot
   set methodProc(xyplot,box-and-whiskers)  DrawBoxWhiskers
   set methodProc(xyplot,xband)             DrawXband
   set methodProc(xyplot,yband)             DrawYband
   set methodProc(xyplot,labeldot)          DrawLabelDot
   set methodProc(xyplot,bindplot)          BindPlot
   set methodProc(xyplot,bindlast)          BindLast
   set methodProc(xyplot,contourlinesfunctionvalues)      DrawIsolinesFunctionValues
   set methodProc(xyplot,contourlinesfunctionpoints)      DrawIsolinesFunctionPoints
   set methodProc(xyplot,plotfunc)          DrawFunction
   set methodProc(xlogyplot,title)          DrawTitle
   set methodProc(xlogyplot,xtext)          DrawXtext
   set methodProc(xlogyplot,ytext)          DrawYtext
   set methodProc(xlogyplot,vtext)          DrawVtext
   set methodProc(xlogyplot,plot)           DrawLogYData
   set methodProc(xlogyplot,dot)            DrawLogDot
   set methodProc(xlogyplot,dotconfig)      DotConfigure
   set methodProc(xlogyplot,interval)       DrawLogInterval
   set methodProc(xlogyplot,trend)          DrawLogTrendLine
   set methodProc(xlogyplot,saveplot)       SavePlot
   set methodProc(xlogyplot,dataconfig)     DataConfig
   set methodProc(xlogyplot,xconfig)        XConfig
   set methodProc(xlogyplot,yconfig)        YConfig
   set methodProc(xlogyplot,xticklines)     DrawXTicklines
   set methodProc(xlogyplot,yticklines)     DrawYTicklines
   set methodProc(xlogyplot,background)     BackgroundColour
   set methodProc(xlogyplot,legendconfig)   LegendConfigure
   set methodProc(xlogyplot,legend)         DrawLegend
   set methodProc(xlogyplot,balloon)        DrawBalloon
   set methodProc(xlogyplot,balloonconfig)  ConfigBalloon
   set methodProc(xlogyplot,plaintext)      DrawPlainText
   set methodProc(xlogyplot,plaintextconfig) ConfigPlainText
   set methodProc(logxyplot,title)          DrawTitle
   set methodProc(logxyplot,xtext)          DrawXtext
   set methodProc(logxyplot,ytext)          DrawYtext
   set methodProc(logxyplot,vtext)          DrawVtext
   set methodProc(logxyplot,plot)           DrawLogXData
   set methodProc(logxyplot,dot)            DrawLogDot
   set methodProc(logxyplot,dotconfig)      DotConfigure
   set methodProc(logxyplot,interval)       DrawLogInterval
   set methodProc(logxyplot,trend)          DrawLogTrendLine
   set methodProc(logxyplot,saveplot)       SavePlot
   set methodProc(logxyplot,dataconfig)     DataConfig
   set methodProc(logxyplot,xconfig)        XConfig
   set methodProc(logxyplot,yconfig)        YConfig
   set methodProc(logxyplot,xticklines)     DrawXTicklines
   set methodProc(logxyplot,yticklines)     DrawYTicklines
   set methodProc(logxyplot,background)     BackgroundColour
   set methodProc(logxyplot,legendconfig)   LegendConfigure
   set methodProc(logxyplot,legend)         DrawLegend
   set methodProc(logxyplot,balloon)        DrawBalloon
   set methodProc(logxyplot,balloonconfig)  ConfigBalloon
   set methodProc(logxyplot,plaintext)      DrawPlainText
   set methodProc(logxyplot,plaintextconfig)   ConfigPlainText
   set methodProc(logxlogyplot,title)          DrawTitle
   set methodProc(logxlogyplot,xtext)          DrawXtext
   set methodProc(logxlogyplot,ytext)          DrawYtext
   set methodProc(logxlogyplot,vtext)          DrawVtext
   set methodProc(logxlogyplot,plot)           DrawLogXLogYData
   set methodProc(logxlogyplot,dot)            DrawLogDot
   set methodProc(logxlogyplot,dotconfig)      DotConfigure
   set methodProc(logxlogyplot,interval)       DrawLogInterval
   set methodProc(logxlogyplot,trend)          DrawLogTrendLine
   set methodProc(logxlogyplot,saveplot)       SavePlot
   set methodProc(logxlogyplot,dataconfig)     DataConfig
   set methodProc(logxlogyplot,xconfig)        XConfig
   set methodProc(logxlogyplot,yconfig)        YConfig
   set methodProc(logxlogyplot,xticklines)     DrawXTicklines
   set methodProc(logxlogyplot,yticklines)     DrawYTicklines
   set methodProc(logxlogyplot,background)     BackgroundColour
   set methodProc(logxlogyplot,legendconfig)   LegendConfigure
   set methodProc(logxlogyplot,legend)         DrawLegend
   set methodProc(logxlogyplot,balloon)        DrawBalloon
   set methodProc(logxlogyplot,balloonconfig)  ConfigBalloon
   set methodProc(logxlogyplot,plaintext)      DrawPlainText
   set methodProc(logxlogyplot,plaintextconfig) ConfigPlainText
   set methodProc(piechart,title)              DrawTitle
   set methodProc(piechart,plot)               DrawPie
   set methodProc(piechart,saveplot)           SavePlot
   set methodProc(piechart,balloon)            DrawBalloon
   set methodProc(piechart,balloonconfig)      ConfigBalloon
   set methodProc(piechart,explode)            PieExplodeSegment
   set methodProc(piechart,plaintext)          DrawPlainText
   set methodProc(piechart,plaintextconfig)    ConfigPlainText
   set methodProc(polarplot,title)             DrawTitle
   set methodProc(polarplot,plot)              DrawPolarData
   set methodProc(polarplot,saveplot)          SavePlot
   set methodProc(polarplot,dataconfig)        DataConfig
   set methodProc(polarplot,background)        BackgroundColour
   set methodProc(polarplot,legendconfig)      LegendConfigure
   set methodProc(polarplot,legend)            DrawLegend
   set methodProc(polarplot,balloon)           DrawBalloon
   set methodProc(polarplot,balloonconfig)     ConfigBalloon
   set methodProc(polarplot,plaintext)         DrawPlainText
   set methodProc(polarplot,plaintextconfig)   ConfigPlainText
   set methodProc(polarplot,labeldot)          DrawLabelDotPolar
   set methodProc(histogram,title)             DrawTitle
   set methodProc(histogram,xtext)             DrawXtext
   set methodProc(histogram,ytext)             DrawYtext
   set methodProc(histogram,vtext)             DrawVtext
   set methodProc(histogram,plot)              DrawHistogramData
   set methodProc(histogram,saveplot)          SavePlot
   set methodProc(histogram,dataconfig)        DataConfig
   set methodProc(histogram,xconfig)           XConfig
   set methodProc(histogram,yconfig)           YConfig
   set methodProc(histogram,yticklines)        DrawYTicklines
   set methodProc(histogram,background)        BackgroundColour
   set methodProc(histogram,legendconfig)      LegendConfigure
   set methodProc(histogram,legend)            DrawLegend
   set methodProc(histogram,balloon)           DrawBalloon
   set methodProc(histogram,balloonconfig)     ConfigBalloon
   set methodProc(histogram,plaintext)         DrawPlainText
   set methodProc(histogram,plaintextconfig)   ConfigPlainText
   set methodProc(horizbars,title)             DrawTitle
   set methodProc(horizbars,xtext)             DrawXtext
   set methodProc(horizbars,ytext)             DrawYtext
   set methodProc(horizbars,vtext)             DrawVtext
   set methodProc(horizbars,plot)              DrawHorizBarData
   set methodProc(horizbars,xticklines)        DrawXTicklines
   set methodProc(horizbars,background)        BackgroundColour
   set methodProc(horizbars,saveplot)          SavePlot
   set methodProc(horizbars,colours)           SetColours
   set methodProc(horizbars,colors)            SetColours
   set methodProc(horizbars,xconfig)           XConfig
   set methodProc(horizbars,config)            ConfigBar
   set methodProc(horizbars,legendconfig)      LegendConfigure
   set methodProc(horizbars,legend)            DrawLegend
   set methodProc(horizbars,balloon)           DrawBalloon
   set methodProc(horizbars,balloonconfig)     ConfigBalloon
   set methodProc(horizbars,plaintext)         DrawPlainText
   set methodProc(horizbars,plaintextconfig)   ConfigPlainText
   set methodProc(vertbars,title)              DrawTitle
   set methodProc(vertbars,xtext)              DrawXtext
   set methodProc(vertbars,ytext)              DrawYtext
   set methodProc(vertbars,vtext)              DrawVtext
   set methodProc(vertbars,plot)               DrawVertBarData
   set methodProc(vertbars,background)         BackgroundColour
   set methodProc(vertbars,yticklines)         DrawYTicklines
   set methodProc(vertbars,saveplot)           SavePlot
   set methodProc(vertbars,colours)            SetColours
   set methodProc(vertbars,colors)             SetColours
   set methodProc(vertbars,yconfig)            YConfig
   set methodProc(vertbars,config)             ConfigBar
   set methodProc(vertbars,legendconfig)       LegendConfigure
   set methodProc(vertbars,legend)             DrawLegend
   set methodProc(vertbars,balloon)            DrawBalloon
   set methodProc(vertbars,balloonconfig)      ConfigBalloon
   set methodProc(vertbars,plaintext)          DrawPlainText
   set methodProc(vertbars,plaintextconfig)    ConfigPlainText
   set methodProc(timechart,title)             DrawTitle
   set methodProc(timechart,period)            DrawTimePeriod
   set methodProc(timechart,milestone)         DrawTimeMilestone
   set methodProc(timechart,vertline)          DrawTimeVertLine
   set methodProc(timechart,saveplot)          SavePlot
   set methodProc(timechart,background)        BackgroundColour
   set methodProc(timechart,balloon)           DrawBalloon
   set methodProc(timechart,balloonconfig)     ConfigBalloon
   set methodProc(timechart,plaintext)         DrawPlainText
   set methodProc(timechart,plaintextconfig)   ConfigPlainText
   set methodProc(timechart,hscroll)           ConnectHorizScrollbar
   set methodProc(timechart,vscroll)           ConnectVertScrollbar
   set methodProc(ganttchart,title)            DrawTitle
   set methodProc(ganttchart,period)           DrawGanttPeriod
   set methodProc(ganttchart,task)             DrawGanttPeriod
   set methodProc(ganttchart,milestone)        DrawGanttMilestone
   set methodProc(ganttchart,vertline)         DrawGanttVertLine
   set methodProc(ganttchart,saveplot)         SavePlot
   set methodProc(ganttchart,color)            GanttColor
   set methodProc(ganttchart,colour)           GanttColor
   set methodProc(ganttchart,font)             GanttFont
   set methodProc(ganttchart,connect)          DrawGanttConnect
   set methodProc(ganttchart,summary)          DrawGanttSummary
   set methodProc(ganttchart,background)       BackgroundColour
   set methodProc(ganttchart,balloon)          DrawBalloon
   set methodProc(ganttchart,balloonconfig)    ConfigBalloon
   set methodProc(ganttchart,plaintext)        DrawPlainText
   set methodProc(ganttchart,plaintextconfig)  ConfigPlainText
   set methodProc(ganttchart,hscroll)          ConnectHorizScrollbar
   set methodProc(ganttchart,vscroll)          ConnectVertScrollbar
   set methodProc(stripchart,title)            DrawTitle
   set methodProc(stripchart,xtext)            DrawXtext
   set methodProc(stripchart,ytext)            DrawYtext
   set methodProc(stripchart,vtext)            DrawVtext
   set methodProc(stripchart,plot)             DrawStripData
   set methodProc(stripchart,saveplot)         SavePlot
   set methodProc(stripchart,dataconfig)       DataConfig
   set methodProc(stripchart,xconfig)          XConfig
   set methodProc(stripchart,yconfig)          YConfig
   set methodProc(stripchart,yticklines)       DrawYTicklines
   set methodProc(stripchart,background)       BackgroundColour
   set methodProc(stripchart,legendconfig)     LegendConfigure
   set methodProc(stripchart,legend)           DrawLegend
   set methodProc(stripchart,balloon)          DrawBalloon
   set methodProc(stripchart,balloonconfig)    ConfigBalloon
   set methodProc(stripchart,plaintext)        DrawPlainText
   set methodProc(stripchart,plaintextconfig)  ConfigPlainText
   set methodProc(isometric,title)             DrawTitle
   set methodProc(isometric,xtext)             DrawXtext
   set methodProc(isometric,ytext)             DrawYtext
   set methodProc(isometric,vtext)             DrawVtext
   set methodProc(isometric,plot)              DrawIsometricData
   set methodProc(isometric,saveplot)          SavePlot
   set methodProc(isometric,background)        BackgroundColour
   set methodProc(isometric,balloon)           DrawBalloon
   set methodProc(isometric,balloonconfig)     ConfigBalloon
   set methodProc(isometric,plaintext)         DrawPlainText
   set methodProc(isometric,plaintextconfig)   ConfigPlainText
   set methodProc(3dplot,title)                DrawTitle
   set methodProc(3dplot,plotfunc)             Draw3DFunction
   set methodProc(3dplot,plotdata)             Draw3DData
   set methodProc(3dplot,plotline)             Draw3DLineFrom3Dcoordinates
   set methodProc(3dplot,gridsize)             GridSize3D
   set methodProc(3dplot,ribbon)               Draw3DRibbon
   set methodProc(3dplot,saveplot)             SavePlot
   set methodProc(3dplot,colour)               SetColours
   set methodProc(3dplot,color)                SetColours
   set methodProc(3dplot,xconfig)              XConfig
   set methodProc(3dplot,yconfig)              YConfig
   set methodProc(3dplot,zconfig)              ZConfig
   set methodProc(3dplot,plotfuncont)          Draw3DFunctionContour
   set methodProc(3dplot,background)           BackgroundColour
   set methodProc(3dbars,title)                DrawTitle
   set methodProc(3dbars,plot)                 Draw3DBar
   set methodProc(3dbars,yconfig)              YConfig
   set methodProc(3dbars,saveplot)             SavePlot
   set methodProc(3dbars,config)               Config3DBar
   set methodProc(3dbars,balloon)              DrawBalloon
   set methodProc(3dbars,balloonconfig)        ConfigBalloon
   set methodProc(3dbars,plaintext)            DrawPlainText
   set methodProc(3dbars,plaintextconfig)      ConfigPlainText
   set methodProc(radialchart,title)           DrawTitle
   set methodProc(radialchart,plot)            DrawRadial
   set methodProc(radialchart,saveplot)        SavePlot
   set methodProc(radialchart,balloon)         DrawBalloon
   set methodProc(radialchart,plaintext)       DrawPlainText
   set methodProc(radialchart,plaintextconfig) ConfigPlainText
   set methodProc(txplot,title)                DrawTitle
   set methodProc(txplot,xtext)                DrawXtext
   set methodProc(txplot,ytext)                DrawYtext
   set methodProc(txplot,vtext)                DrawVtext
   set methodProc(txplot,plot)                 DrawTimeData
   set methodProc(txplot,interval)             DrawInterval
   set methodProc(txplot,saveplot)             SavePlot
   set methodProc(txplot,dataconfig)           DataConfig
   set methodProc(txplot,xconfig)              XConfig
   set methodProc(txplot,yconfig)              YConfig
   set methodProc(txplot,xticklines)           DrawXTicklines
   set methodProc(txplot,yticklines)           DrawYTicklines
   set methodProc(txplot,background)           BackgroundColour
   set methodProc(txplot,legendconfig)         LegendConfigure
   set methodProc(txplot,legend)               DrawLegend
   set methodProc(txplot,balloon)              DrawBalloon
   set methodProc(txplot,balloonconfig)        ConfigBalloon
   set methodProc(txplot,plaintext)            DrawPlainText
   set methodProc(txplot,plaintextconfig)      ConfigPlainText
   set methodProc(3dribbon,title)              DrawTitle
   set methodProc(3dribbon,saveplot)           SavePlot
   set methodProc(3dribbon,line)               Draw3DLine
   set methodProc(3dribbon,area)               Draw3DArea
   set methodProc(3dribbon,background)         BackgroundColour
   set methodProc(boxplot,title)               DrawTitle
   set methodProc(boxplot,xtext)               DrawXtext
   set methodProc(boxplot,ytext)               DrawYtext
   set methodProc(boxplot,vtext)               DrawVtext
   set methodProc(boxplot,plot)                DrawBoxData
   set methodProc(boxplot,saveplot)            SavePlot
   set methodProc(boxplot,dataconfig)          DataConfig
   set methodProc(boxplot,xconfig)             XConfig
   set methodProc(boxplot,yconfig)             YConfig
   set methodProc(boxplot,xticklines)          DrawXTicklines
   set methodProc(boxplot,yticklines)          DrawYTicklines
   set methodProc(boxplot,background)          BackgroundColour
   set methodProc(boxplot,legendconfig)        LegendConfigure
   set methodProc(boxplot,legend)              DrawLegend
   set methodProc(boxplot,balloon)             DrawBalloon
   set methodProc(boxplot,balloonconfig)       ConfigBalloon
   set methodProc(boxplot,plaintext)           DrawPlainText
   set methodProc(boxplot,plaintextconfig)     ConfigPlainText
   set methodProc(windrose,plot)               DrawWindRoseData
   set methodProc(windrose,saveplot)           SavePlot
   set methodProc(windrose,title)              DrawTitle
   set methodProc(targetdiagram,title)         DrawTitle
   set methodProc(targetdiagram,xtext)         DrawXtext
   set methodProc(targetdiagram,ytext)         DrawYtext
   set methodProc(targetdiagram,vtext)         DrawVtext
   set methodProc(targetdiagram,plot)          DrawTargetData
   set methodProc(targetdiagram,saveplot)      SavePlot
   set methodProc(targetdiagram,background)    BackgroundColour
   set methodProc(targetdiagram,legendconfig)  LegendConfigure
   set methodProc(targetdiagram,legend)        DrawLegend
   set methodProc(targetdiagram,balloon)       DrawBalloon
   set methodProc(targetdiagram,balloonconfig) ConfigBalloon
   set methodProc(targetdiagram,plaintext)     DrawPlainText
   set methodProc(targetdiagram,plaintextconfig) ConfigPlainText
   set methodProc(targetdiagram,dataconfig)    DataConfig
   set methodProc(3dribbonplot,title)          DrawTitle
   set methodProc(3dribbonplot,plot)           Draw3DRibbon
   set methodProc(3dribbonplot,saveplot)       SavePlot
   set methodProc(3dribbonplot,xconfig)        XConfig
   set methodProc(3dribbonplot,yconfig)        YConfig
   set methodProc(3dribbonplot,zconfig)        ZConfig
   set methodProc(3dribbonplot,background)     BackgroundColour
   set methodProc(performance,title)           DrawTitle
   set methodProc(performance,xtext)           DrawXtext
   set methodProc(performance,ytext)           DrawYtext
   set methodProc(performance,vtext)           DrawVtext
   set methodProc(performance,plot)            DrawPerformanceData
   set methodProc(performance,dot)             DrawDot
   set methodProc(performance,saveplot)        SavePlot
   set methodProc(performance,dataconfig)      DataConfig
   set methodProc(performance,xconfig)         XConfig
   set methodProc(performance,yconfig)         YConfig
   set methodProc(performance,xticklines)      DrawXTicklines
   set methodProc(performance,yticklines)      DrawYTicklines
   set methodProc(performance,background)      BackgroundColour
   set methodProc(performance,legendconfig)    LegendConfigure
   set methodProc(performance,legend)          DrawLegend
   set methodProc(performance,balloon)         DrawBalloon
   set methodProc(performance,balloonconfig)   ConfigBalloon
   set methodProc(performance,plaintext)       DrawPlainText
   set methodProc(performance,plaintextconfig) ConfigPlainText

   #
   # Auxiliary parameters
   #
   variable torad
   set torad [expr {3.1415926/180.0}]

   variable options
   variable option_keys
   variable option_values
   set options       {-colour -color  -symbol -type -filled -fillcolour -boxwidth -width}
   set option_keys   {-colour -colour -symbol -type -filled -fillcolour -boxwidth -width}
   set option_values {-colour {...}
                      -symbol {plus cross circle up down dot upfilled downfilled}
                      -type {line symbol both}
                      -filled {no up down}
                      -fillcolour {...}
                      -boxwidth   {...}
                      -width      {...}
                     }

   variable axis_options
   variable axis_option_clear
   variable axis_option_values
   set axis_options       {-format -ticklength -ticklines -scale -minorticks -labeloffset -axisoffset}
   set axis_option_clear  { 0       0           0          1      0           0            0         }
   set axis_option_config { 0       1           0          0      1           1            1         }
   set axis_option_values {-format      {...}
                           -ticklength  {...}
                           -ticklines   {0 1}
                           -scale       {...}
                           -minorticks  {...}
                           -labeloffset {...}
                           -axisoffset  {...}
                          }
   variable contour_options
}

# setZoomPan --
#    Set up the bindings for zooming and panning
# Arguments:
#    w           Name of the canvas window
# Result:
#    None
# Side effect:
#    Bindings set up
#
proc ::Plotchart::setZoomPan { w } {
   set sqrt2  [expr {sqrt(2.0)}]
   set sqrt05 [expr {sqrt(0.5)}]

   bind $w <Control-Button-1> [list ::Plotchart::ScaleItems $w %x %y $sqrt2]
   bind $w <Control-Prior>    [list ::Plotchart::ScaleItems $w %x %y $sqrt2]
   bind $w <Control-Button-2> [list ::Plotchart::ScaleItems $w %x %y $sqrt05]
   bind $w <Control-Button-3> [list ::Plotchart::ScaleItems $w %x %y $sqrt05]
   bind $w <Control-Next>     [list ::Plotchart::ScaleItems $w %x %y $sqrt05]
   bind $w <Control-Up>       [list ::Plotchart::MoveItems  $w   0 -40]
   bind $w <Control-Down>     [list ::Plotchart::MoveItems  $w   0  40]
   bind $w <Control-Left>     [list ::Plotchart::MoveItems  $w -40   0]
   bind $w <Control-Right>    [list ::Plotchart::MoveItems  $w  40   0]
   focus $w
}

# viewPort --
#    Set the pixel extremes for the graph
# Arguments:
#    w           Name of the canvas window
#    pxmin       Minimum X-coordinate
#    pymin       Minimum Y-coordinate
#    pxmax       Maximum X-coordinate
#    pymax       Maximum Y-coordinate
# Result:
#    None
# Side effect:
#    Array scaling filled
#
proc ::Plotchart::viewPort { w pxmin pymin pxmax pymax } {
   variable scaling

   if { $pxmin >= $pxmax || $pymin >= $pymax } {
      return -code error "Inconsistent bounds for viewport - increase canvas size or decrease margins"
   }

   set scaling($w,pxmin)    $pxmin
   set scaling($w,pymin)    $pymin
   set scaling($w,pxmax)    $pxmax
   set scaling($w,pymax)    $pymax
   set scaling($w,new)      1
}

# worldCoordinates --
#    Set the extremes for the world coordinates
# Arguments:
#    w           Name of the canvas window
#    xmin        Minimum X-coordinate
#    ymin        Minimum Y-coordinate
#    xmax        Maximum X-coordinate
#    ymax        Maximum Y-coordinate
# Result:
#    None
# Side effect:
#    Array scaling filled
#
proc ::Plotchart::worldCoordinates { w xmin ymin xmax ymax } {
   variable scaling

   if { $xmin == $xmax || $ymin == $ymax } {
      return -code error "Minimum and maximum must differ for world coordinates"
   }

   set scaling($w,xmin)    [expr {double($xmin)}]
   set scaling($w,ymin)    [expr {double($ymin)}]
   set scaling($w,xmax)    [expr {double($xmax)}]
   set scaling($w,ymax)    [expr {double($ymax)}]

   set scaling($w,new)     1
}

# polarCoordinates --
#    Set the extremes for the polar coordinates
# Arguments:
#    w           Name of the canvas window
#    radmax      Maximum radius
# Result:
#    None
# Side effect:
#    Array scaling filled
#
proc ::Plotchart::polarCoordinates { w radmax } {
   variable scaling

   if { $radmax <= 0.0 } {
      return -code error "Maximum radius must be positive"
   }

   set scaling($w,xmin)    [expr {-double($radmax)}]
   set scaling($w,ymin)    [expr {-double($radmax)}]
   set scaling($w,xmax)    [expr {double($radmax)}]
   set scaling($w,ymax)    [expr {double($radmax)}]

   set scaling($w,new)     1
}

# world3DCoordinates --
#    Set the extremes for the world coordinates in 3D plots
# Arguments:
#    w           Name of the canvas window
#    xmin        Minimum X-coordinate
#    ymin        Minimum Y-coordinate
#    zmin        Minimum Z-coordinate
#    xmax        Maximum X-coordinate
#    ymax        Maximum Y-coordinate
#    zmax        Maximum Z-coordinate
# Result:
#    None
# Side effect:
#    Array scaling filled
#
proc ::Plotchart::world3DCoordinates { w xmin ymin zmin xmax ymax zmax } {
   variable scaling

   if { $xmin == $xmax || $ymin == $ymax || $zmin == $zmax } {
      return -code error "Minimum and maximum must differ for world coordinates"
   }

   set scaling($w,xmin)    [expr {double($xmin)}]
   set scaling($w,ymin)    [expr {double($ymin)}]
   set scaling($w,zmin)    [expr {double($zmin)}]
   set scaling($w,xmax)    [expr {double($xmax)}]
   set scaling($w,ymax)    [expr {double($ymax)}]
   set scaling($w,zmax)    [expr {double($zmax)}]

   set scaling($w,new)     1
}

# coordsToPixel --
#    Convert world coordinates to pixel coordinates
# Arguments:
#    w           Name of the canvas
#    xcrd        X-coordinate
#    ycrd        Y-coordinate
# Result:
#    List of two elements, x- and y-coordinates in pixels
#
proc ::Plotchart::coordsToPixel { w xcrd ycrd } {
   variable scaling

   if { $scaling($w,new) == 1 } {
      set scaling($w,new)     0
      set width               [expr {$scaling($w,pxmax)-$scaling($w,pxmin)}]
      set height              [expr {$scaling($w,pymax)-$scaling($w,pymin)}]

      set dx                  [expr {$scaling($w,xmax)-$scaling($w,xmin)}]
      set dy                  [expr {$scaling($w,ymax)-$scaling($w,ymin)}]
      set scaling($w,xfactor) [expr {$width/$dx}]
      set scaling($w,yfactor) [expr {$height/$dy}]
   }

   set xpix [expr {$scaling($w,pxmin)+($xcrd-$scaling($w,xmin))*$scaling($w,xfactor)}]
   set ypix [expr {$scaling($w,pymin)+($scaling($w,ymax)-$ycrd)*$scaling($w,yfactor)}]
   return [list $xpix $ypix]
}

# coords3DToPixel --
#    Convert world coordinates to pixel coordinates (3D plots)
# Arguments:
#    w           Name of the canvas
#    xcrd        X-coordinate
#    ycrd        Y-coordinate
#    zcrd        Z-coordinate
# Result:
#    List of two elements, x- and y-coordinates in pixels
#
proc ::Plotchart::coords3DToPixel { w xcrd ycrd zcrd } {
   variable scaling

   if { $scaling($w,new) == 1 } {
      set scaling($w,new)      0
      set width                [expr {$scaling($w,pxmax)-$scaling($w,pxmin)}]
      set height               [expr {$scaling($w,pymax)-$scaling($w,pymin)}]

      set dx                   [expr {$scaling($w,xmax)-$scaling($w,xmin)}]
      set dy                   [expr {$scaling($w,ymax)-$scaling($w,ymin)}]
      set dz                   [expr {$scaling($w,zmax)-$scaling($w,zmin)}]
      set scaling($w,xyfactor) [expr {$scaling($w,yfract)*$width/$dx}]
      set scaling($w,xzfactor) [expr {$scaling($w,zfract)*$height/$dx}]
      set scaling($w,yfactor)  [expr {$width/$dy}]
      set scaling($w,zfactor)  [expr {$height/$dz}]
   }

   #
   # The values for xcrd = xmax
   #
   set xpix [expr {$scaling($w,pxmin)+($ycrd-$scaling($w,ymin))*$scaling($w,yfactor)}]
   set ypix [expr {$scaling($w,pymin)+($scaling($w,zmax)-$zcrd)*$scaling($w,zfactor)}]

   #
   # Add the shift due to xcrd-xmax
   #
   set xpix [expr {$xpix + $scaling($w,xyfactor)*($xcrd-$scaling($w,xmax))}]
   set ypix [expr {$ypix - $scaling($w,xzfactor)*($xcrd-$scaling($w,xmax))}]

   return [list $xpix $ypix]
}

# pixelToCoords --
#    Convert pixel coordinates to world coordinates
# Arguments:
#    w           Name of the canvas
#    xpix        X-coordinate (pixel)
#    ypix        Y-coordinate (pixel)
# Result:
#    List of two elements, x- and y-coordinates in world coordinate system
#
proc ::Plotchart::pixelToCoords { w xpix ypix } {
   variable scaling

   if { $scaling($w,new) == 1 } {
      set scaling($w,new)     0
      set width               [expr {$scaling($w,pxmax)-$scaling($w,pxmin)}]
      set height              [expr {$scaling($w,pymax)-$scaling($w,pymin)}]

      set dx                  [expr {$scaling($w,xmax)-$scaling($w,xmin)}]
      set dy                  [expr {$scaling($w,ymax)-$scaling($w,ymin)}]
      set scaling($w,xfactor) [expr {$width/$dx}]
      set scaling($w,yfactor) [expr {$height/$dy}]
   }

   set xcrd [expr {$scaling($w,xmin)+($xpix-$scaling($w,pxmin))/$scaling($w,xfactor)}]
   set ycrd [expr {$scaling($w,ymax)-($ypix-$scaling($w,pymin))/$scaling($w,yfactor)}]
   return [list $xcrd $ycrd]
}

# pixelToIndex --
#    Convert pixel coordinates to elements list index
# Arguments:
#    w           Name of the canvas
#    xpix        X-coordinate (pixel)
#    ypix        Y-coordinate (pixel)
# Result:
#    Elements list index
#
proc ::Plotchart::pixelToIndex { w xpix ypix } {
   variable scaling
   variable torad

   set idx -1
   set radius [expr {($scaling(${w},pxmax) - $scaling(${w},pxmin)) / 2}]
   set xrel [expr {${xpix} - $scaling(${w},pxmin) - ${radius}}]
   set yrel [expr {-${ypix} + $scaling(${w},pymin) + ${radius}}]
   if {[expr {pow(${radius},2) < (pow(${xrel},2) + pow(${yrel},2))}]} {
       # do nothing out of pie chart
   } elseif {[info exists scaling(${w},angles)]} {
       set xy_angle [expr {(360 + round(atan2(${yrel},${xrel})/${torad})) % 360}]
       foreach angle $scaling(${w},angles) {
       if {${xy_angle} <= ${angle}} {
           break
       }
       incr idx
       }
   }
   return ${idx}
}

# polarToPixel --
#    Convert polar coordinates to pixel coordinates
# Arguments:
#    w           Name of the canvas
#    rad         Radius of the point
#    phi         Angle of the point (degrees)
# Result:
#    List of two elements, x- and y-coordinates in pixels
#
proc ::Plotchart::polarToPixel { w rad phi } {
   variable torad

   set xcrd [expr {$rad*cos($phi*$torad)}]
   set ycrd [expr {$rad*sin($phi*$torad)}]

   coordsToPixel $w $xcrd $ycrd
}

# createXYPlot --
#    Create a command for drawing an XY plot
# Arguments:
#    w           Name of the canvas
#    xscale      Minimum, maximum and step for x-axis (initial)
#    yscale      Minimum, maximum and step for y-axis
#    args        Options (currently: "-xlabels list" and "-ylabels list")
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the XY plot.
#    The plot will be drawn with axes
#
proc ::Plotchart::createXYPlot { w xscale yscale args} {
   variable scaling
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "xyplot_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler xyplot $w
   CopyConfig xyplot $w
   set scaling($w,eventobj) ""

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}

   foreach {xmin xmax xdelt} $xscale {break}
   foreach {ymin ymax ydelt} $yscale {break}

   if { $xdelt == 0.0 || $ydelt == 0.0 } {
      return -code error "Step size can not be zero"
   }

   if { $xdelt ne {} && ($xmax-$xmin)*$xdelt < 0.0 } {
      set xdelt [expr {-$xdelt}]
   }
   if { $ydelt ne {} && ($ymax-$ymin)*$ydelt < 0.0 } {
      set ydelt [expr {-$ydelt}]
   }

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   if { $xdelt eq {} } {
       foreach {arg val} $args {
           switch -exact -- $arg {
               -xlabels {
                   DrawXaxis $w $xmin  $xmax  $xdelt $arg $val
               }
               -ylabels {
                   # Ignore
               }
               default {
                   error "Argument $arg not recognized"
               }
           }
       }
   } else {
       DrawXaxis   $w $xmin  $xmax  $xdelt
   }
   if { $ydelt eq {} } {
       foreach {arg val} $args {
           switch -exact -- $arg {
               -ylabels {
                   DrawYaxis $w $ymin  $ymax  $ydelt $arg $val
               }
               -xlabels {
                   # Ignore
               }
               default {
                   error "Argument $arg not recognized"
               }
           }
       }
   } else {
       DrawYaxis        $w $ymin  $ymax  $ydelt
   }
   DrawMask         $w
   DefaultLegend    $w
   DefaultBalloon   $w

   $newchart dataconfig labeldot -colour red -type symbol -symbol dot

   return $newchart
}

# createStripchart --
#    Create a command for drawing a strip chart
# Arguments:
#    w           Name of the canvas
#    xscale      Minimum, maximum and step for x-axis (initial)
#    yscale      Minimum, maximum and step for y-axis
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the stripchart.
#    The stripchart will be drawn with axes
#
proc ::Plotchart::createStripchart { w xscale yscale } {
   variable data_series

   set newchart [createXYPlot $w $xscale $yscale]

   interp alias {} $newchart {}

   set newchart "stripchart_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler stripchart $w
   CopyConfig stripchart $w

   return $newchart
}

# createIsometricPlot --
#    Create a command for drawing an "isometric" plot
# Arguments:
#    w           Name of the canvas
#    xscale      Minimum and maximum for x-axis
#    yscale      Minimum and maximum for y-axis
#    stepsize    Step size for numbers on the axes or "noaxes"
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the plot
#    The plot will be drawn with or without axes
#
proc ::Plotchart::createIsometricPlot { w xscale yscale stepsize } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "isometric_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler isometric $w
   CopyConfig isometric $w

   if { $stepsize != "noaxes" } {
      foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}
   } else {
      set pxmin 0
      set pymin 0
      #set pxmax [$w cget -width]
      #set pymax [$w cget -height]
      set pxmax [WidthCanvas $w]
      set pymax [HeightCanvas $w]
   }

   foreach {xmin xmax xdelt} $xscale {break}
   foreach {ymin ymax ydelt} $yscale {break}

   if { $xmin == $xmax || $ymin == $ymax } {
      return -code error "Extremes for axes must be different"
   }

   viewPort         $w $pxmin $pymin $pxmax $pymax
   ScaleIsometric   $w $xmin  $ymin  $xmax  $ymax

   if { $stepsize != "noaxes" } {
      DrawYaxis        $w $ymin  $ymax  $stepsize
      DrawXaxis        $w $xmin  $xmax  $stepsize
      DrawMask         $w
   }
   DefaultLegend  $w
   DefaultBalloon $w

   return $newchart
}

# createXLogYPlot --
#    Create a command for drawing an XY plot (with a vertical logarithmic axis)
# Arguments:
#    w           Name of the canvas
#    xscale      Minimum, maximum and step for x-axis (initial)
#    yscale      Minimum, maximum and step for y-axis (step is ignored!)
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the XY plot.
#    The plot will be drawn with axes
#
proc ::Plotchart::createXLogYPlot { w xscale yscale } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "xlogyplot_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler xlogyplot $w
   CopyConfig xlogyplot $w

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}

   foreach {xmin xmax xdelt} $xscale {break}
   foreach {ymin ymax ydelt} $yscale {break}

   if { $xdelt == 0.0 || $ydelt == 0.0 } {
      return -code error "Step size can not be zero"
   }

   if { $ymin <= 0.0 || $ymax <= 0.0 } {
      return -code error "Minimum and maximum for y-axis must be positive"
   }

   #
   # TODO: reversed log plot
   #

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  [expr {log10($ymin)}]  $xmax [expr {log10($ymax)}]

   DrawLogYaxis     $w $ymin  $ymax  $ydelt
   DrawXaxis        $w $xmin  $xmax  $xdelt
   DrawMask         $w
   DefaultLegend    $w
   DefaultBalloon   $w

   return $newchart
}

# createLogXYPlot --
#    Create a command for drawing an XY plot (with a horizontal logarithmic axis)
# Arguments:
#    w           Name of the canvas
#    xscale      Minimum, maximum and step for x-axis (step is ignored!)
#    yscale      Minimum, maximum and step for y-axis (initial)
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the XY plot.
#    The plot will be drawn with axes
#
proc ::Plotchart::createLogXYPlot { w xscale yscale } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "logxyplot_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler logxyplot $w
   CopyConfig logxyplot $w

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}

   foreach {xmin xmax xdelt} $xscale {break}
   foreach {ymin ymax ydelt} $yscale {break}

   if { $xmin <= 0.0 || $xmax <= 0.0 } {
      return -code error "Minimum and maximum for x-axis must be positive"
   }

   if { $ydelt == 0.0 } {
      return -code error "Step size can not be zero"
   }

   #
   # TODO: reversed log plot
   #

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w [expr {log10($xmin)}] $ymin [expr {log10($xmax)}] $ymax
   DrawYaxis        $w $ymin  $ymax  $ydelt
   DrawLogXaxis     $w $xmin  $xmax  $xdelt
   DrawMask         $w
   DefaultLegend    $w
   DefaultBalloon   $w

   return $newchart
}

# createLogXLogYPlot --
#    Create a command for drawing an XY plot (with a both logarithmic axis)
# Arguments:
#    w           Name of the canvas
#    xscale      Minimum, maximum and step for x-axis (step is ignored!)
#    yscale      Minimum, maximum and step for y-axis (step is ignored!)
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the XY plot.
#    The plot will be drawn with axes
#
proc ::Plotchart::createLogXLogYPlot { w xscale yscale } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "logxlogyplot_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler logxlogyplot $w
   CopyConfig logxlogyplot $w

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}

   foreach {xmin xmax xdelt} $xscale {break}
   foreach {ymin ymax ydelt} $yscale {break}

   if { $xmin <= 0.0 || $xmax <= 0.0 } {
      return -code error "Minimum and maximum for x-axis must be positive"
   }

   if { $ymin <= 0.0 || $ymax <= 0.0 } {
      return -code error "Minimum and maximum for y-axis must be positive"
   }

   #
   # TODO: reversed log plot
   #

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w [expr {log10($xmin)}] [expr {log10($ymin)}] [expr {log10($xmax)}] [expr {log10($ymax)}]
   DrawLogYaxis     $w $ymin  $ymax  $ydelt
   DrawLogXaxis     $w $xmin  $xmax  $xdelt
   DrawMask         $w
   DefaultLegend    $w
   DefaultBalloon   $w

   return $newchart
}

# createHistogram --
#    Create a command for drawing a histogram
# Arguments:
#    w           Name of the canvas
#    xscale      Minimum, maximum and step for x-axis (initial)
#    yscale      Minimum, maximum and step for y-axis
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the histogram.
#    The plot will be drawn with axes
#    This is almost the same code as for an XY plot
#
proc ::Plotchart::createHistogram { w xscale yscale } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "histogram_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler histogram $w
   CopyConfig histogram $w

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}

   foreach {xmin xmax xdelt} $xscale {break}
   foreach {ymin ymax ydelt} $yscale {break}

   if { $xdelt == 0.0 || $ydelt == 0.0 } {
      return -code error "Step size can not be zero"
   }

   if { ($xmax-$xmin)*$xdelt < 0.0 } {
      set xdelt [expr {-$xdelt}]
   }
   if { ($ymax-$ymin)*$ydelt < 0.0 } {
      set ydelt [expr {-$ydelt}]
   }

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   DrawYaxis        $w $ymin  $ymax  $ydelt
   DrawXaxis        $w $xmin  $xmax  $xdelt
   DrawMask         $w
   DefaultLegend    $w
   DefaultBalloon   $w

   return $newchart
}

# createPiechart --
#    Create a command for drawing a pie chart
# Arguments:
#    w           Name of the canvas
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the pie chart.
#
proc ::Plotchart::createPiechart { w } {
   variable data_series
   variable scaling

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "piechart_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler piechart $w
   CopyConfig piechart $w

   foreach {pxmin pymin pxmax pymax} [MarginsCircle $w] {break}

   viewPort $w $pxmin $pymin $pxmax $pymax
  #$w create oval $pxmin $pymin $pxmax $pymax

   SetColours $w blue lightblue green yellow orange red magenta brown
   DefaultLegend  $w
   DefaultBalloon $w

   set scaling($w,auto)      0
   set scaling($w,exploded) -1

   return $newchart
}

# createPolarplot --
#    Create a command for drawing a polar plot
# Arguments:
#    w             Name of the canvas
#    radius_data   Maximum radius and step
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the polar plot
#    Possible additional arguments (optional): nautical/mathematical
#    step in phi
#
proc ::Plotchart::createPolarplot { w radius_data } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "polarplot_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler polarplot $w
   CopyConfig polarplot $w

   set rad_max   [lindex $radius_data 0]
   set rad_step  [lindex $radius_data 1]

   if { $rad_step <= 0.0 } {
      return -code error "Step size can not be zero or negative"
   }
   if { $rad_max <= 0.0 } {
      return -code error "Maximum radius can not be zero or negative"
   }

   foreach {pxmin pymin pxmax pymax} [MarginsCircle $w] {break}

   viewPort         $w $pxmin     $pymin     $pxmax   $pymax
   polarCoordinates $w $rad_max
   DrawPolarAxes    $w $rad_max   $rad_step
   DefaultLegend    $w
   DefaultBalloon   $w

   $newchart dataconfig labeldot -colour red -type symbol -symbol dot

   return $newchart
}

# createBarchart --
#    Create a command for drawing a barchart with vertical bars
# Arguments:
#    w           Name of the canvas
#    xlabels     List of labels for x-axis
#    yscale      Minimum, maximum and step for y-axis
#    noseries    Number of series or the keyword "stacked"
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the barchart.
#
proc ::Plotchart::createBarchart { w xlabels yscale noseries } {
   variable data_series
   variable settings

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "barchart_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler vertbars $w
   CopyConfig vertbars $w

   set settings($w,showvalues)   0
   set settings($w,valuefont)    ""
   set settings($w,valuecolour)  black
   set settings($w,valueformat)  %s

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}

   set xmin  0.0
   set xmax  [expr {[llength $xlabels] + 0.1}]

   foreach {ymin ymax ydelt} $yscale {break}

   if { $ydelt == 0.0 } {
      return -code error "Step size can not be zero"
   }

   if { ($ymax-$ymin)*$ydelt < 0.0 } {
      set ydelt [expr {-$ydelt}]
   }

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   DrawYaxis        $w $ymin  $ymax  $ydelt
   DrawXlabels      $w $xlabels $noseries
   DrawMask         $w
   DefaultLegend    $w
   set data_series($w,legendtype) "rectangle"
   DefaultBalloon   $w

   SetColours $w blue lightblue green yellow orange red magenta brown

   return $newchart
}

# createHorizontalBarchart --
#    Create a command for drawing a barchart with horizontal bars
# Arguments:
#    w           Name of the canvas
#    xscale      Minimum, maximum and step for x-axis
#    ylabels     List of labels for y-axis
#    noseries    Number of series or the keyword "stacked"
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the barchart.
#
proc ::Plotchart::createHorizontalBarchart { w xscale ylabels noseries } {
   variable data_series
   variable config
   variable settings

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "hbarchart_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler horizbars $w
   CopyConfig horizbars $w

   set settings($w,showvalues)   0
   set settings($w,valuefont)    ""
   set settings($w,valuecolour)  black
   set settings($w,valueformat)  %s

   set font      $config($w,leftaxis,font)
   set xspacemax 0
   foreach ylabel $ylabels {
       set xspace [font measure $font $ylabel]
       if { $xspace > $xspacemax } {
           set xspacemax $xspace
       }
   }
   set config($w,margin,left) [expr {$xspacemax+5}] ;# Slightly more space required!

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}

   set ymin  0.0
   set ymax  [expr {[llength $ylabels] + 0.1}]

   foreach {xmin xmax xdelt} $xscale {break}

   if { $xdelt == 0.0 } {
      return -code error "Step size can not be zero"
   }

   if { ($xmax-$xmin)*$xdelt < 0.0 } {
      set xdelt [expr {-$xdelt}]
   }

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   DrawXaxis        $w $xmin  $xmax  $xdelt
   DrawYlabels      $w $ylabels $noseries
   DrawMask         $w
   DefaultLegend    $w
   set data_series($w,legendtype) "rectangle"
   DefaultBalloon   $w

   SetColours $w blue lightblue green yellow orange red magenta brown

   return $newchart
}

# createBoxplot --
#    Create a command for drawing a plot with box-and-whiskers
# Arguments:
#    w           Name of the canvas
#    xscale      Minimum, maximum and step for x-axis
#    ylabels     List of labels for y-axis
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the boxplot.
#
proc ::Plotchart::createBoxplot { w xscale ylabels } {
   variable data_series
   variable config

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "boxplot_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler boxplot $w
   CopyConfig boxplot $w

   set font      $config($w,leftaxis,font)
   set xspacemax 0
   foreach ylabel $ylabels {
       set xspace [font measure $font $ylabel]
       if { $xspace > $xspacemax } {
           set xspacemax $xspace
       }
   }
   set config($w,margin,left) [expr {$xspacemax+5}] ;# Slightly more space required!
   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}

   set ymin  0.0
   set ymax  [expr {[llength $ylabels] + 0.1}]

   foreach {xmin xmax xdelt} $xscale {break}

   if { $xdelt == 0.0 } {
      return -code error "Step size can not be zero"
   }

   if { ($xmax-$xmin)*$xdelt < 0.0 } {
      set xdelt [expr {-$xdelt}]
   }

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   DrawXaxis        $w $xmin  $xmax  $xdelt
   DrawYlabels      $w $ylabels 1
   DrawMask         $w
   DefaultLegend    $w
   set data_series($w,legendtype) "rectangle"
   DefaultBalloon   $w

   set config($w,axisnames) $ylabels

   return $newchart
}

# createTimechart --
#    Create a command for drawing a simple timechart
# Arguments:
#    w           Name of the canvas
#    time_begin  Start time (in the form of a date/time)
#    time_end    End time (in the form of a date/time)
#    args        Number of items to be shown (determines spacing)
#                or one or more options (-barheight pixels, -ylabelwidth pixels)
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the timechart.
#
proc ::Plotchart::createTimechart { w time_begin time_end args} {
   variable data_series
   variable scaling

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "timechart_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler timechart $w
   CopyConfig timechart $w

   #
   # Handle the arguments
   #
   set barheight    0
   set noitems      [lindex $args 0]
   set ylabelwidth  8

   if { [string is integer -strict $noitems] } {
       set args [lrange $args 1 end]
   }
   foreach {keyword value} $args {
       switch -- $keyword {
           "-barheight" {
                set barheight $value
           }
           "-ylabelwidth" {
                set ylabelwidth [expr {$value/10.0}] ;# Pixels to characters
           }
           default {
                # Ignore
           }
       }
   }

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w 3 $ylabelwidth] {break}

   if { $barheight != 0 } {
       set noitems [expr {($pxmax-$pxmin)/double($barheight)}]
   }
   set scaling($w,barheight) $barheight

   set ymin  0.0
   set ymax  $noitems

   set xmin  [expr {1.0*[clock scan $time_begin]}]
   set xmax  [expr {1.0*[clock scan $time_end]}]

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   set scaling($w,current) $ymax
   set scaling($w,dy)      -0.7

   DrawScrollMask $w
   set scaling($w,curpos)  0
   set scaling($w,curhpos) 0

   return $newchart
}

# createGanttchart --
#    Create a command for drawing a Gantt (planning) chart
# Arguments:
#    w           Name of the canvas
#    time_begin  Start time (in the form of a date/time)
#    time_end    End time (in the form of a date/time)
#    args        (First integer) Number of items to be shown (determines spacing)
#                (Second integer) Estimated maximum length of text (default: 20)
#                Or keyword-value pairs
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the Gantt chart.
#    Most commands taken from time charts.
#
proc ::Plotchart::createGanttchart { w time_begin time_end args} {

   variable data_series
   variable scaling

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "ganttchart_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler ganttchart $w
   CopyConfig ganttchart $w

   #
   # Handle the arguments
   #
   set barheight    0
   set noitems      [lindex $args 0]

   if { [string is integer -strict $noitems] } {
       set args        [lrange $args 1 end]
       set ylabelwidth [lindex $args 0]
       if { [string is integer -strict $ylabelwidth] } {
           set args [lrange $args 1 end]
       } else {
           set ylabelwidth 20
       }
   } else {
       set ylabelwidth 20
   }

   foreach {keyword value} $args {
       switch -- $keyword {
           "-barheight" {
                set barheight $value
           }
           "-ylabelwidth" {
                set ylabelwidth [expr {$value/10.0}] ;# Pixels to characters
           }
           default {
                # Ignore
           }
       }
   }

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w 3 $ylabelwidth] {break}

   if { $barheight != 0 } {
       set noitems [expr {($pxmax-$pxmin)/double($barheight)}]
   }
   set scaling($w,barheight) $barheight

   set ymin  0.0
   set ymax  $noitems

   set xmin  [expr {1.0*[clock scan $time_begin]}]
   set xmax  [expr {1.0*[clock scan $time_end]}]

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   set scaling($w,current) $ymax
   set scaling($w,dy)      -0.7

   #
   # Draw the backgrounds (both in the text part and the
   # graphical part; the text part has the "special" tag
   # "Edit" to enable a GUI to change things)
   #
   set yend 0.0
   for { set i 0 } { $i < $noitems } { incr i } {
       set ybegin $yend
       set yend   [expr {$ybegin+1.0}]
       foreach {x1 y1} [coordsToPixel $w $xmin $ybegin] {break}
       foreach {x2 y2} [coordsToPixel $w $xmax $yend  ] {break}

       if { $i%2 == 0 } {
           set tag odd
       } else {
           set tag even
       }
       $w create rectangle 0   $y1 $x1 $y2 -fill white \
           -tag {Edit vertscroll lowest} -outline white
       $w create rectangle $x1 $y1 $x2 $y2 -fill white \
           -tag [list $tag vertscroll lowest] -outline white
   }

   #
   # Default colours and fonts
   #
   GanttColor $w description black
   GanttColor $w completed   lightblue
   GanttColor $w left        white
   GanttColor $w odd         white
   GanttColor $w even        lightgrey
   GanttColor $w summary     black
   GanttColor $w summarybar  black
   GanttFont  $w description "times 10"
   GanttFont  $w summary     "times 10 bold"
   GanttFont  $w scale       "times 7"
   DefaultBalloon $w

   DrawScrollMask $w
   set scaling($w,curpos)  0
   set scaling($w,curhpos) 0

   return $newchart
}

# create3DPlot --
#    Create a simple 3D plot
# Arguments:
#    w           Name of the canvas
#    xscale      Minimum, maximum and step for x-axis (initial)
#    yscale      Minimum, maximum and step for y-axis
#    zscale      Minimum, maximum and step for z-axis
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the 3D plot
#
proc ::Plotchart::create3DPlot { w xscale yscale zscale } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "3dplot_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler 3dplot $w
   CopyConfig 3dplot $w

   foreach {pxmin pymin pxmax pymax} [Margins3DPlot $w] {break}

   foreach {xmin xmax xstep} $xscale {break}
   foreach {ymin ymax ystep} $yscale {break}
   foreach {zmin zmax zstep} $zscale {break}

   viewPort           $w $pxmin $pymin $pxmax $pymax
   world3DCoordinates $w $xmin  $ymin  $zmin  $xmax  $ymax $zmax

   Draw3DAxes         $w $xmin  $ymin  $zmin  $xmax  $ymax $zmax \
                         $xstep $ystep $zstep
   DefaultLegend      $w
   DefaultBalloon     $w

   SetColours $w grey black

   return $newchart
}

# create3DRibbonPlot --
#    Create a simple 3D plot that allows for ribbons
# Arguments:
#    w           Name of the canvas
#    yscale      Minimum, maximum and step for y-axis
#    zscale      Minimum, maximum and step for z-axis
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the 3D plot
#
proc ::Plotchart::create3DRibbonPlot { w yscale zscale } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "3dribbonplot_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler 3dribbonplot $w
   CopyConfig 3dplot $w

   foreach {pxmin pymin pxmax pymax} [Margins3DPlot $w] {break}

   foreach {xmin xmax xstep} {0.0 1.0 0.0} {break}
   foreach {ymin ymax ystep} $yscale {break}
   foreach {zmin zmax zstep} $zscale {break}

   viewPort           $w $pxmin $pymin $pxmax $pymax
   world3DCoordinates $w $xmin  $ymin  $zmin  $xmax  $ymax $zmax

   Draw3DAxes         $w $xmin  $ymin  $zmin  $xmin  $ymax $zmax \
                         $xstep $ystep $zstep
   DefaultLegend      $w
   DefaultBalloon     $w

   SetColours $w grey black

   return $newchart
}

# create3DBarchart --
#    Create a command for drawing a barchart with vertical 3D bars
# Arguments:
#    w           Name of the canvas
#    yscale      Minimum, maximum and step for y-axis
#    nobars      Number of bars to be drawn
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the barchart.
#
proc ::Plotchart::create3DBarchart { w yscale nobars } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "3dbarchart_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler 3dbars $w
   CopyConfig 3dbars $w

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w 4] {break}

   set xmin  0.0
   set xmax  [expr {$nobars + 0.1}]

   foreach {ymin ymax ydelt} $yscale {break}

   if { $ydelt == 0.0 } {
      return -code error "Step size can not be zero"
   }

   if { ($ymax-$ymin)*$ydelt < 0.0 } {
      set ydelt [expr {-$ydelt}]
   }

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   DrawYaxis        $w $ymin  $ymax  $ydelt
  #DrawMask         $w -- none!
   Draw3DBarchart   $w $yscale $nobars
   DefaultLegend    $w
   DefaultBalloon   $w

   return $newchart
}

# createRadialchart --
#    Create a command for drawing a radial chart
# Arguments:
#    w           Name of the canvas
#    names       Names of the spokes
#    scale       Scale factor for the data
#    style       (Optional) style of the chart (lines, cumulative or filled)
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the radial chart.
#
proc ::Plotchart::createRadialchart { w names scale {style lines} } {
   variable settings
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "radialchart_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler radialchart $w
   CopyConfig radialchart $w

   foreach {pxmin pymin pxmax pymax} [MarginsCircle $w] {break}

   viewPort $w $pxmin $pymin $pxmax $pymax
   $w create oval $pxmin $pymin $pxmax $pymax

   set settings($w,scale)  [expr {double($scale)}]
   set settings($w,style)  $style
   set settings($w,number) [llength $names]

   DrawRadialSpokes $w $names
   DefaultLegend  $w
   DefaultBalloon $w

   return $newchart
}

# createTXPlot --
#    Create a command for drawing a TX plot (x versus date/time)
# Arguments:
#    w           Name of the canvas
#    tscale      Minimum, maximum and step for date/time-axis (initial)
#                (values must be valid dates and the step is in days)
#    xscale      Minimum, maximum and step for vertical axis
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the TX plot.
#    The plot will be drawn with axes
#
proc ::Plotchart::createTXPlot { w tscale xscale } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "txplot_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler txplot $w
   CopyConfig txplot $w

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}

   foreach {tmin tmax tdelt} $tscale {break}

   set xmin  [clock scan $tmin]
   set xmax  [clock scan $tmax]
   set xdelt [expr {86400*$tdelt}]

   foreach {ymin ymax ydelt} $xscale {break}

   if { $xdelt == 0.0 || $ydelt == 0.0 } {
      return -code error "Step size can not be zero"
   }

   if { ($xmax-$xmin)*$xdelt < 0.0 } {
      set xdelt [expr {-$xdelt}]
   }
   if { ($ymax-$ymin)*$ydelt < 0.0 } {
      set ydelt [expr {-$ydelt}]
   }

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   DrawYaxis        $w $ymin  $ymax  $ydelt
   DrawTimeaxis     $w $tmin  $tmax  $tdelt
   DrawMask         $w
   DefaultLegend    $w
   DefaultBalloon   $w

   return $newchart
}

# createRightAxis --
#    Create a command for drawing a plot with a right axis
# Arguments:
#    w           Name of the canvas
#    yscale      Minimum, maximum and step for vertical axis
# Result:
#    Name of a new command
# Note:
#    This command requires that another plot command has been
#    created prior to this one. Some of the properties from that
#    command serve for this one too.
#
proc ::Plotchart::createRightAxis { w yscale } {
   variable data_series
   variable scaling
   variable config

   set newchart "right_$w"

   #
   # Check if there is an appropriate plot already defined - there
   # should be only one!
   #
   if { [llength [info command "*_$w" ]] == 0 } {
       return -code error "There should be a plot with a left axis already defined"
   }
   if { [llength [info command "*_$w" ]] >= 2 } {
       if { [llength [info command "right_$w"]] == 0 } {
           return -code error "There should be only one plot command for this widget ($w)"
       } else {
           catch {
               interp alias {} $newchart {}
           }
       }
   }

   foreach s [array names data_series "r$w,*"] {
      unset data_series($s)
   }

   set type [lindex [interp alias {} [info command "*_$w"]] 1]

   interp alias {} $newchart {} ::Plotchart::PlotHandler $type r$w
   interp alias {} r$w       {} $w
   CopyConfig $type r$w

   set config(r$w,font,char_width)  $config($w,font,char_width)
   set config(r$w,font,char_height) $config($w,font,char_height)

   set xmin $scaling($w,xmin)
   set xmax $scaling($w,xmax)

   set pxmin $scaling($w,pxmin)
   set pxmax $scaling($w,pxmax)
   set pymin $scaling($w,pymin)
   set pymax $scaling($w,pymax)

   foreach {ymin ymax ydelt} $yscale {break}

   if { $ydelt == 0.0 } {
      return -code error "Step size can not be zero"
   }

   if { ($ymax-$ymin)*$ydelt < 0.0 } {
      set ydelt [expr {-$ydelt}]
   }

   viewPort         r$w $pxmin $pymin $pxmax $pymax
   worldCoordinates r$w $xmin  $ymin  $xmax  $ymax

   DrawRightaxis    r$w $ymin  $ymax  $ydelt

   #DefaultLegend    r$w
   #DefaultBalloon   r$w

   return $newchart
}

# create3DRibbonChart --
#    Create a chart that can display 3D lines and areas
# Arguments:
#    w           Name of the canvas
#    names       Labels along the x-axis
#    yscale      Minimum, maximum and step for y-axis
#    zscale      Minimum, maximum and step for z-axis
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the 3D chart
#
proc ::Plotchart::create3DRibbonChart { w names yscale zscale } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "3dribbon_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler 3dribbon $w
   CopyConfig 3dribbon $w

   foreach {pxmin pymin pxmax pymax} [Margins3DPlot $w] {break}

   foreach {xmin xmax xstep} {0.0 1.0 0.0} {break}
   foreach {ymin ymax ystep} $yscale {break}
   foreach {zmin zmax zstep} $zscale {break}

   set xstep [expr {1.0/[llength $names]}]
   set data_series($w,xbase)  [expr {1.0-0.15*$xstep}]
   set data_series($w,xstep)  $xstep
   set data_series($w,xwidth) [expr {0.7*$xstep}]

   viewPort           $w $pxmin $pymin $pxmax $pymax
   world3DCoordinates $w $xmin  $ymin  $zmin  $xmax  $ymax $zmax

   Draw3DAxes         $w $xmin  $ymin  $zmin  $xmax  $ymax $zmax \
                         $xstep $ystep $zstep $names
   DefaultLegend      $w
   DefaultBalloon     $w

   SetColours $w grey black

   return $newchart
}

# createWindRose --
#     Create a new command for plotting a windrose
#
# Arguments:
#    w             Name of the canvas
#    radius_data   Maximum radius and step
#    sectors       Number of sectors (default: 16)
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the windrose
#    Possible additional arguments (optional): nautical/mathematical
#    step in phi
#
proc ::Plotchart::createWindRose { w radius_data {sectors 16}} {
    variable data_series

    foreach s [array names data_series "$w,*"] {
        unset data_series($s)
    }

    set newchart "windrose_$w"
    interp alias {} $newchart {} ::Plotchart::PlotHandler windrose $w
    CopyConfig windrose $w

    set rad_max   [lindex $radius_data 0]
    set rad_step  [lindex $radius_data 1]

    if { $rad_step <= 0.0 } {
        return -code error "Step size can not be zero or negative"
    }
    if { $rad_max <= 0.0 } {
        return -code error "Maximum radius can not be zero or negative"
    }

    foreach {pxmin pymin pxmax pymax} [MarginsCircle $w] {break}

    viewPort         $w $pxmin     $pymin     $pxmax   $pymax
    polarCoordinates $w $rad_max
    DrawRoseAxes     $w $rad_max   $rad_step


    set data_series($w,radius) {}
    for { set i 0 } { $i < $sectors } { incr i } {
        lappend data_series($w,cumulative_radius) 0.0
    }

    set data_series($w,start_angle)     [expr {90.0 - 360.0/(4.0*$sectors)}]
    set data_series($w,d_angle)         [expr {360.0/(2.0*$sectors)}]
    set data_series($w,increment_angle) [expr {360.0/$sectors}]
    set data_series($w,count_data)      0


    return $newchart
}

# createTargetDiagram --
#    Create a command for drawing a target diagram
# Arguments:
#    w           Name of the canvas
#    bounds      List of radii to indicate bounds for the skill
#    scale       Scale of the axes - defaults to 1
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the XY plot.
#    The plot will be drawn with axes
#
proc ::Plotchart::createTargetDiagram { w bounds {scale 1.0}} {
    variable scaling
    variable data_series
    variable config

    foreach s [array names data_series "$w,*"] {
        unset data_series($s)
    }

    set newchart "targetdiagram_$w"
    interp alias {} $newchart {} ::Plotchart::PlotHandler targetdiagram $w
    CopyConfig targetdiagram $w

    foreach {pxmin pymin pxmax pymax} [MarginsSquare $w] {break}

    set extremes [determineScale [expr {-$scale}] $scale]
    foreach {xmin xmax xdelt} $extremes {break}
    foreach {ymin ymax ydelt} $extremes {break}

    if { $xdelt == 0.0 || $ydelt == 0.0 } {
        return -code error "Step size can not be zero"
    }

    if { $xdelt ne {} && ($xmax-$xmin)*$xdelt < 0.0 } {
        set xdelt [expr {-$xdelt}]
    }
    if { ($ymax-$ymin)*$ydelt < 0.0 } {
        set ydelt [expr {-$ydelt}]
    }

    viewPort         $w $pxmin $pymin $pxmax $pymax
    worldCoordinates $w $xmin  $ymin  $xmax  $ymax

    DrawYaxis        $w $ymin  $ymax  $ydelt
    DrawXaxis        $w $xmin  $xmax  $xdelt

    DrawMask         $w
    DefaultLegend    $w
    DefaultBalloon   $w

    foreach {pxcent pycent} [coordsToPixel $w 0.0 0.0] {break}

    $w create line $pxmin  $pycent $pxmax  $pycent -fill $config($w,limits,color) -tag limits
    $w create line $pxcent $pymin  $pxcent $pymax  -fill $config($w,limits,color) -tag limits

    foreach r $bounds {
        foreach {pxmin pymin} [coordsToPixel $w [expr {-$r}] [expr {-$r}]] {break}
        foreach {pxmax pymax} [coordsToPixel $w $r $r] {break}

        $w create oval $pxmin $pymin $pxmax $pymax -outline $config($w,limits,color) -tag limits
    }


    return $newchart
}

# createPerformanceProfile --
#    Create a command for drawing a performance profile
# Arguments:
#    w           Name of the canvas
#    scale       Maximum value for the x-axis
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the XY plot.
#    The plot will be drawn with axes
#
proc ::Plotchart::createPerformanceProfile { w scale } {
   variable scaling
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "performance_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler performance $w
   CopyConfig performance $w
   set scaling($w,eventobj) ""

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}

   foreach {xmin xmax xdelt} [determineScale 1.0 $scale] {break}
   foreach {ymin ymax ydelt} {0.0 1.1 0.25} {break}

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   DrawYaxis        $w $ymin  $ymax  $ydelt
   DrawXaxis        $w $xmin  $xmax  $xdelt

   DrawMask         $w
   DefaultLegend    $w
   LegendConfigure  $w -position bottom-right
   DefaultBalloon   $w


   return $newchart
}

# Load the private procedures
#
source [file join [file dirname [info script]] "plotpriv.tcl"]
source [file join [file dirname [info script]] "plotaxis.tcl"]
source [file join [file dirname [info script]] "plot3d.tcl"]
source [file join [file dirname [info script]] "scaling.tcl"]
source [file join [file dirname [info script]] "plotcontour.tcl"]
source [file join [file dirname [info script]] "plotgantt.tcl"]
source [file join [file dirname [info script]] "plotbusiness.tcl"]
source [file join [file dirname [info script]] "plotannot.tcl"]
source [file join [file dirname [info script]] "plotconfig.tcl"]
source [file join [file dirname [info script]] "plotpack.tcl"]
source [file join [file dirname [info script]] "plotbind.tcl"]
source [file join [file dirname [info script]] "plotspecial.tcl"]

# Announce our presence
#
package provide Plotchart 1.9.2
