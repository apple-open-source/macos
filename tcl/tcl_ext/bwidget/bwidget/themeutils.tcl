# themeutils.tcl ---
# ----------------------------------------------------------------------------
# Purpose:
#      This file is a contribution to the Unifix BWidget Toolkit.
#      An approach to re-vitalize the package and to take advantage of tile!
#      Author: Johann dot Oberdorfer at Googlemail dot com
#
#  $Id: themeutils.tcl,v 1.04 2009/11/01 20:20:50 oberdorfer Exp $
# ----------------------------------------------------------------------------
#  Index of commands:
#     - BWidget::use
#     - BWidget::using
#     - BWidget::wrap
#     - BWidget::set_themedefaults
#     - BWidget::getAvailableThemes
#     - BWidget::default_Color
#     - BWidget::[themename]_Color
#     - BWidget::_get_colordcls
#     - BWidget::_getSystemDefaultStyle
#     - BWidget::_read_ttk_current_theme
#     - BWidget::_getDefaultClassOpt
#     - BWidget::_read_ttkstylecolors
#     - BWidget::_themechanged
# ----------------------------------------------------------------------------
# color mapping:
#     SystemWindow        -background
#     SystemWindowFrame   -background
#     SystemWindowText    -foreground
#     SystemButtonText    -activeforeground
#     SystemButtonFace    -activebackground
#     SystemDisabledText  -disabledforeground
#     SystemHighlight     -selectbackground
#     SystemHighlightText -selectforeground
#     SystemMenu          -background
#     SystemMenuText      -foreground
#     SystemScrollbar     -troughcolor
# ----------------------------------------------------------------------------

# Notes:
#
#   - Themed color declarations and names do not follow a strict rule,
#     so -most likely- there might be differences from theme to theme.
#     As a consequece of this fact, we have to support a minimum set
#     of color declarations which needs to be declared for most common
#     themes. Unsupported themes 'll fall back to the "default" color scheme.
#
# Further explanations:
#   - In respect of color settings, BW basically runs in 2 different modes:
#
#       - without tile:
#            the standard initialization sequence (as usual) is:
#               "package require BWidget"
#            in this case, color codes are set according to the OS
#            currently running on and acc. to a predefined color scheme,
#            which is a "best guess" of what might look good for most of the
#            users ...
#            
#       - With tile - "themed":
#           See notes above.
#           In addition to the standard initialization sequence,
#           the following line must be present in order to activate theming:
#              "BWidget::usepackage ttk"
#           and within the code:
#              "BWidget::using ttk"
#           might be used to distinguish between the 2 different modes.
#
#       - As themes are not support within the BW distribution (except some
#         themes found in the demo), a programmer needs to support prefered
#         theme packages separately.
#         A typical initialization code block might look like:
#              ...
#              lappend auto_path [where to find specific theme package]
#              set ctheme winxpblue
#              package require ttk::theme::${ctheme}
#              ttk::setTheme $ctheme
#              ...
#         
#       - In addition, a separate procedure must be provided, to manage
#         and control standard tk "widgets", which needs to be re-colorized
#         as well, when a <<ThemeChanged>> virtual event arises.
#         See code below, how a "BWidget::<mythemeName>_Color" procedure
#         looks like!
#------------------------------------------------------------------------------
#------------------------------------------------------------------------------

namespace eval ::BWidget:: {
    variable colors
    variable _properties

    set colors(style) default

    array set _properties \
      [list \
          package   "" \
          style     "default" \
          setoptdb  "no" \
          themedirs {} \
      ]
}


# BWidget::use
#   Argument usage:
#      -package ttk
#               |
#               specify a package name to be initialized, currently
#		support for the following packages is implemented:
#		   ttk ... try to use tile'd widget set (if available)
#		
#      -style default / native / myFavoriteStyleName
#             |         |        |
#	      |		|	 specify a valid style name,
#	      |		|	 use "BWidget::_get_colordcls" which gives
#	      | 	|	 you a list of what's avaliable for tk
#	      |		|
#	      |		if specified, BW tries to emulate OS color scheme,
#             |         a specific color schema associated to each individual
#             |         operationg system is going to be used
#	      |
#             same behaviour as before, stay compatible
#             with previous releases
#	 
#      -setoptdb [no=default|0|yes|1]
#                              |
#                              maintain the option database
#                              if you need a dynamic behavior when changing
#                              the underlying style, activate this option!
#
#      -themedirs {} = default / a list of valid directory names,
#                                to specifing additional ttk theme packages

proc ::BWidget::use { args } {
    variable _properties

    # argument processing:
    array set p { -package "" -style "" -setoptdb 0 -themedirs "" }

    foreach {key value} $args {
      if { ![info exists p($key)] } {
          return -code error "[namespace current] - bad option: '$key'"
      }
      set p($key) $value
    }

    # -- package:
    set package $p(-package)
    if { [string length $package] > 0 &&
         ![info exists _properties($package)] } { 

        # each package supported to enhance BWidgets is setup here.
        switch -- $package {
            "ttk" {
	    
	        # attempt to load tile package (with the required version)
	        #   if { [catch {package present tile 0.8}] != 0 } {
                #       return -code error "Tile 0.8 is not available!"
		#   }

                if { [catch {uplevel "#0" package require tile 0.8}] != 0 } {
                         set _properties($package) 0
		} else { set _properties($package) 1 }
            }
	    default {
	        return -code error \
		  "[namespace current] bad option: '$package'"
	    }
        }
    }

    # -- style:
    if { [string length $p(-style)] > 0 } {

        set _properties(style) $p(-style)

        if { [string compare $p(-style) "native"] == 0 } {
             set _properties(style) [_getSystemDefaultStyle]
        }
    }

    # -- setoptdb:
    if { [string length $p(-setoptdb)] > 0 } {
	set _properties(setoptdb) \
	      [regexp -nocase {^(1|yes|true|on)$} $p(-setoptdb)]
    }

    # -- themedirs:
    if { [llength $p(-themedirs)] > 0 } {
        foreach themedir $p(-themedirs) {
            if { [file isdirectory $themedir] &&
	         [lsearch $_properties(themedirs) $themedir] == -1 } {
                
		# maintain auto_path for further usage of theme'd packages
		if { [lsearch -exact ::auto_path $themedir] == -1 } {
                    lappend ::auto_path $themedir
                }

		lappend _properties(themedirs) $themedir
            }
	}
    }

    # perform required initializations
    set_themedefaults $_properties(style)

    # theme related bindings:    
    if { $_properties(setoptdb) != 0 } {
        if {[lsearch [bindtags .] BWThemeChanged] < 0} {
            bindtags . [linsert [bindtags .] 1 BWThemeChanged]
	    bind BWThemeChanged <<ThemeChanged>> \
                    "+ [namespace current]::_themechanged"
        }
    }
}


proc ::BWidget::using { optName } {
    variable _properties

    if {[info exists _properties($optName)]} {
        return $_properties($optName)
    }
    return 0
}


# a simple wrapper to distinguish between tk and ttk
proc ::BWidget::wrap {wtype wpath args} {

    set _ttkunsupported_opt \
           { -font -fg -foreground -background
	     -highlightthickness -bd -borderwidth
	     -padx -pady -anchor
             -relief -selectforeground -selectbackground }

    if { [using ttk] } {
        # filter out (ttk-)unsupported (tk-)options:
	foreach opt $$_ttkunsupported_opt {
            set args [Widget::getArgument $args $opt tmp]
	}

        return [eval ttk::${wtype} $wpath $args]
    } else {
        return [eval $wtype $wpath $args]
    }
}


# returns a list of themes, which are are declared as well for both: tk and ttk,
# all other ttk themes, that might be available down below the given theme dir's
# are skipped!

proc ::BWidget::getAvailableThemes {} {
  variable _properties

  set themes [list default]
  set tk_colordcls [_get_colordcls]

  if { [BWidget::using ttk] } {
  
      foreach themedir $_properties(themedirs) {
          if { [file isdirectory $themedir] } {
              foreach dir [glob -nocomplain [file join $themedir "*"]] {
	        set themeName [file tail $dir]
	        # check, if there is a corresponding color declaration available
                if { [file isdirectory $dir] &&
		     [lsearch $tk_colordcls $themeName] != -1 } {
                   lappend themes $themeName
              }}
          }
      }

  } else {
      foreach dcls $tk_colordcls {
          if { [lsearch $themes $dcls] == -1 } {
              lappend themes $dcls
      }}
  }

  return $themes
}


# returns the system default theme

proc ::BWidget::_getSystemDefaultStyle {} {

    if {$::tcl_version >= 8.4} {
        set plat [tk windowingsystem]
	if { $plat == "x11" } {
	    switch -- $::tcl_platform(os) {
		"AIX"    { set plat "AIX" }
	        "Darwin" { set plat "Darwin" }
	    }
	}   
    } else {
        set plat $::tcl_platform(platform)
    }

    switch -glob -- $plat {
	"classic" - "aqua" - "Darwin" \
	          { set style "aquativo" }
	"win*"    { set style "winxpblue" }
	"AIX"     { set style "grey" }
	"x11" -
	default   { set style "default" }
    }

    return $style
}


proc ::BWidget::_read_ttk_current_theme {} {
 
    if { [using ttk] } {
        # future version: "return [ttk::style theme use]"
        return $ttk::currentTheme
    } 
    return ""
}


# arguments:
#   cname = a valid class name
#           e.g.: "foreground", "background", "font"...
#   return value: the desired option or an empty string
# example call:
#   set fg [_getDefaultClassOpt "foreground" "nocache"]

proc ::BWidget::_getDefaultClassOpt {cname {mode ""}} {
  variable _class_options

  # should we use the cached list?
  if { $mode != "" ||
       ![info exists _class_options] ||
        [llength $_class_options] == 0 } {

     # retrieve options from the listbox

     set tmpWidget ".__tmp__"
     set count 0
     while {[winfo exists $tmpWidget] == 1} {
       set tmpWidget ".__tmp__$count"
       incr count
     }
  
     label $tmpWidget 
     set _class_options [$tmpWidget configure]
     destroy $tmpWidget
  }
  
  # and now, we retrieve the option ...
  # foreach c [lsort -dictionary $_class_options] { puts $c }

  foreach opt $_class_options {

    if {[llength $opt] == 5} {
      set option [lindex $opt 1]
      set value  [lindex $opt 4]

      if {$cname == $option} {
        return $value
      }
    }
  }
  
  return ""
}


# this function is a replacement for [ttk::style configure .]
# and takes as well the style decl's from the "default" style into account

proc ::BWidget::_read_ttkstylecolors {} {

    # default style comes 1st:
    # temporarily sets the current theme to themeName,
    # evaluate script, then restore the previous theme.

    ttk::style theme settings "default" {
        set cargs [ttk::style configure .]
    }
    
    # superseed color defaults with values from currently active theme:
    foreach {opt val} [ttk::style configure .] {
      if { [set idx [lsearch $cargs $opt]] == -1 } {
          lappend cargs $opt $val
      } else {
          incr idx
	  if { ![string equal [lindex $cargs $idx] $val] } {
              set cargs [lreplace $cargs $idx $idx $val]
	  }
      }
    }

    return $cargs
}


proc ::BWidget::_createOrUpdateButtonStyles {} {

    if { ![using ttk] } { return }

    # create a new element for each available theme...
    foreach themeName [ttk::style theme names] {

       # temporarily sets the current theme to themeName,
       # evaluate script, then restore the previous theme.

        ttk::style theme settings $themeName {
  
            # emulate tk behavior, referenced later on such like:
            #    -style "${relief}BW.Toolbutton"

            ::ttk::style configure BWraised.Toolbutton -relief raised
            ::ttk::style configure BWsunken.Toolbutton -relief sunken
            ::ttk::style configure BWflat.Toolbutton   -relief flat
            ::ttk::style configure BWsolid.Toolbutton  -relief solid
            ::ttk::style configure BWgroove.Toolbutton -relief groove
            ::ttk::style configure BWlink.Toolbutton   -relief flat -bd 2
	    
	    ::ttk::style map BWlink.Toolbutton \
	        -relief [list {selected !disabled} sunken]

	    ::ttk::style configure BWSlim.Toolbutton -relief flat -bd 2
            ::ttk::style map BWSlim.Toolbutton \
	        -relief [list {selected !disabled} sunken]
        }
    }
}


# Purpose:
#   Sets the current style + default ttk (tyled) theme,
#   ensure, color related array: "BWidget::colors" is updated as well
#
#   This procedure is called under the following conditions:
#      - When initializing the package, to make sure, that the default
#        style is set (which basically establishes a color array with
#        predefined colors).
#      - In any case a style has changed, if a ttk theme driven style is
#        changed, the virtual event <<ThemeChanged>> is fired, which in turn
#        causes this function to be called.

proc ::BWidget::set_themedefaults { {styleName ""} } {
    variable colors
    variable _properties
    variable _previous_style

    if { $styleName != "" } {
        set cstyle $styleName
    } else {
        set cstyle $colors(style)
    }

    # determine, it style has changed in the meantime...
    if { [info exists _previous_style] &&
         [string compare $_previous_style $cstyle] == 0 } {
        return
    }
    set _previous_style $cstyle

    # style name available ?
    if { [lsearch [_get_colordcls] $cstyle] == -1 } {
        return -code error \
	   "specified style '$cstyle' is not available!"
    }

    # evaluate procedure where the naming matches the currently active theme:
    if { [catch { "${cstyle}_Color" }] != 0 } {
        default_Color
    }

    if { ![using ttk] } {
        if { $_properties(setoptdb) != 0 } {
            event generate . <<ThemeChanged>>
	}

        return
    }

    # if not already set, try to set specified ttk style as well
    if { [string compare [_read_ttk_current_theme] $cstyle] != 0 } {

        uplevel "#0" package require ttk::theme::${cstyle}
        ttk::setTheme $cstyle
    }

    # superseed color options from the ones provided by ttk theme (if available),
    # to fit as close as possible with the curent style, if one of the refered
    # option is not provided (which is most likely the case), we take the ones
    # which are declared by our own!

    # "-selectbackground" { set colors(SystemHighlight)     $val }
    # "-selectforeground" { set colors(SystemHighlightText) $val }

    foreach {opt val} [BWidget::_read_ttkstylecolors] {
      switch -- $opt {
          "-foreground"  { set colors(SystemWindowText)  $val }
          "-background"  { set colors(SystemWindowFrame) $val }
          "-troughcolor" { set colors(SystemScrollbar)   $val }
      }
    }
    
    _createOrUpdateButtonStyles
}


proc ::BWidget::_themechanged {} {
  variable _properties

    set_themedefaults

    # proceed in case the user really want's this behaviour - might cause some
    # side effects - as the option settings are affecting almost everything...
    if { $_properties(setoptdb) == 0 } {
        return
    }

    # -- propagate new color settings:
    # note:
    #   modifying the option database doesn't affect existing widgets,
    #   only new widgets 'll be taken into account
    #
    #     Priorities:
    #         widgetDefault: 20   /   userDefault:   60
    #         startupFile:   40   /   interactive:   80 (D)
    set prio "userDefault"
    
    option add *background       $BWidget::colors(SystemWindowFrame)   $prio
    option add *foreground       $BWidget::colors(SystemWindowText)    $prio
    option add *selectbackground $BWidget::colors(SystemHighlight)     $prio
    option add *selectforeground $BWidget::colors(SystemHighlightText) $prio

    option add *Entry.highlightColor $BWidget::colors(SystemHighlight) $prio
    option add *Entry.highlightThickness 2 $prio

    option add *Text.background  $BWidget::colors(SystemWindow)        $prio
    option add *Text.foreground  $BWidget::colors(SystemWindowText)    $prio
    option add *Text.highlightBackground \
                                 $BWidget::colors(SystemHighlight)     $prio

    # -- modify existing tk widgts...
    set standard_dcls [list \
            [list -background          $BWidget::colors(SystemWindowFrame)] \
            [list -foreground          $BWidget::colors(SystemWindowText)] \
            [list -highlightColor      $BWidget::colors(SystemHighlight)] \
            [list -highlightBackground $BWidget::colors(SystemHighlight)] \
            [list -selectbackground    $BWidget::colors(SystemHighlight)] \
            [list -selectforeground    $BWidget::colors(SystemHighlightText)] \
    ]

    set menu_dcls [list \
            [list -background          $BWidget::colors(SystemMenu)] \
            [list -foreground          $BWidget::colors(SystemMenuText)] \
            [list -activebackground    $BWidget::colors(SystemHighlight)] \
            [list -activeforeground    $BWidget::colors(SystemHighlightText)] \
    ]

    # filter out:
    #  - ttk witdgets, which do not support a "-style" argument,
    #  - as well as custom widgets
    # widgets which fail to have an argument list at all are skipped

    set custom_classes {
            ComboBox ListBox MainFrame ScrollableFrame Tree
	    ScrolledWindow PanedWindow LabelFrame TitleFrame NoteBook
	    DragSite DropSite Listbox SelectFont ButtonBox DynamicHelp
	    ArrowButton LabelEntry SpinBox Separator
	    TLabel TFrame ProgressBar ComboboxPopdown
	    Canvas Entry Text
    }

    set widget_list {}
    foreach w [Widget::getallwidgets .] {
        set nostyle 0
        if { [lsearch $custom_classes [winfo class $w]] == -1 &&
	     [catch {$w configure} cargs] == 0 } {
	    foreach item $cargs {
	        if { [string compare [lindex $item 0] "-style"] != 0 } {
	            set nostyle 1
		    break
	        }
	    }
        }
	if { $nostyle == 1 } { lappend widget_list $w }
    }

    # o.k now for processing the color adjustment...

    foreach child $widget_list {
        set wclass [winfo class $child]

        switch -- $wclass {
	  "Menu"  { set col_dcls [lrange $menu_dcls 0 end] }
	  default { set col_dcls [lrange $standard_dcls 0 end] }
	}
        foreach citem $col_dcls {
            set copt [lindex $citem 0]
            set cval [lindex $citem 1]

            foreach optitem [$child configure] {
                if { [lsearch $optitem $copt] != -1 } {
	            catch { $child configure $copt $cval }
                }
            }
	}
    }
}


#------------------------------------------------------------------------------
# color declarations and related functions
#------------------------------------------------------------------------------

proc ::BWidget::_get_colordcls { } {
    set stylelist {}
    set keyword "_Color"
    foreach name [info proc] {
         if { [regexp -all -- $keyword $name] } {
	     set nlen [string length $name]
	     set klen [string length $keyword]
	     set stylename [string range $name 0 [expr {$nlen - $klen - 1}]]
	     # ![regexp -nocase -- "default" $stylename]
             lappend stylelist $stylename
	 }
    }
    return [lsort -dictionary $stylelist]
}


proc ::BWidget::default_Color { } {
    variable colors
    set colors(style) "default"

    # !!! doesn't work on winxp64
    #     + starpacked executable from equi4
    # if {[string equal $::tcl_platform(platform) "---windows---"]} {
    #    array set colors {
    #      SystemWindow        SystemWindow
    #      SystemWindowFrame   SystemWindowFrame
    #      SystemWindowText    SystemWindowText
    #      SystemButtonFace    SystemButtonFace
    #      SystemButtonText    SystemButtonText
    #      SystemDisabledText  SystemDisabledText
    #      SystemHighlight     SystemHighlight
    #      SystemHighlightText SystemHighlightText
    #      SystemMenu          SystemMenu
    #      SystemMenuText      SystemMenuText
    #      SystemScrollbar     SystemScrollbar
    #    }
    # }

    # try to stay compatible as much as possible,
    # therefor we try to map tk option database with the color array:

    array set colors {
            SystemWindow        "Black"
            SystemWindowFrame   "#d9d9d9"
            SystemWindowText    "Black"
            SystemButtonFace    "#d9d9d9"
            SystemButtonText    "Black"
            SystemDisabledText  "#a3a3a3"
            SystemHighlight     "#c3c3c3"
            SystemHighlightText "White"
            SystemMenu          "#d9d9d9"
            SystemMenuText      "Black"
            SystemScrollbar     "#d9d9d9"
    }
   
    # override our own defaults with
    # colors from the option database (if available):

    foreach {colSynonym colOptName} \
                  { SystemWindow        background
                    SystemWindowFrame   background
                    SystemWindowText    foreground
                    SystemButtonText    activeForeground
                    SystemButtonFace    activeBackground
                    SystemDisabledText  disabledForeground
                    SystemHighlight     selectBackground
                    SystemHighlightText selectForeground
                    SystemMenu          background
                    SystemMenuText      foreground
                    SystemScrollbar     troughcolor } {

      set opt [_getDefaultClassOpt $colOptName]

      if { [string length $opt] > 0 } {
          # puts "$colSynonym : $colOptName : $opt"
          set colors($colSynonym) $opt
      }
    }
}


proc ::BWidget::grey_Color { } {
    variable colors
    set colors(style) "grey"

    array set colors {
           SystemWindow        "#6e7c94"
           SystemWindowFrame   "#788c9c"
           SystemWindowText    "White"
           SystemButtonFace    "#6e7c94"
           SystemButtonText    "Black"
           SystemDisabledText  "#aaaaaa"
           SystemHighlight     "DarkGrey"
           SystemHighlightText "White"
           SystemMenu          "#6e7c94"
           SystemMenuText      "White"
           SystemScrollbar     "#788c9c"
    }
    
    option add *highlightThickness  1
    option add *HighlightColor      $colors(SystemHighlight)
    option add *highlightBackground $colors(SystemWindowFrame)
}


proc ::BWidget::winxpblue_Color { } {
    variable colors
    set colors(style) "winxpblue"

    array set colors {
      Style               "winxpblue"
      SystemWindow        "White"
      SystemWindowFrame   "#ece9d8"
      SystemWindowText    "Black"
      SystemButtonFace    "#d9d9d9"
      SystemButtonText    "Black"
      SystemDisabledText  "#a3a3a3"
      SystemHighlight     "#c1d2ee"
      SystemHighlightText "Black"
      SystemMenu          "LightGrey"
      SystemMenuText      "Black"
      SystemScrollbar     "#d9d9d9"
    }
}

proc ::BWidget::plastik_Color { } {
    variable colors
    set colors(style) "plastik"

    array set colors {
      SystemWindow        "LightGrey"
      SystemWindowFrame   "#efefef"
      SystemWindowText    "Black"
      SystemButtonFace    "#efefef"
      SystemButtonText    "Black"
      SystemDisabledText  "#aaaaaa"
      SystemHighlight     "#c3c3c3"
      SystemHighlightText "Black"
      SystemMenu          "LightGrey"
      SystemMenuText      "Black"
      SystemScrollbar     "#efefef"
    }
}

proc ::BWidget::keramik_Color { } {
    variable colors
    set colors(style) "keramik"

    array set colors {
      SystemWindow        "#ffffff"
      SystemWindowFrame   "#dddddd"
      SystemWindowText    "Black"
      SystemButtonFace    "#efefef"
      SystemButtonText    "Black"
      SystemDisabledText  "#aaaaaa"
      SystemHighlight     "#eeeeee"
      SystemHighlightText "Black"
      SystemMenu          "LightGrey"
      SystemMenuText      "Black"
      SystemScrollbar     "#efefef"
    }
}

proc ::BWidget::keramik_alt_Color { } {
    keramik_Color
    set colors(style) "keramik_alt"

}

proc ::BWidget::black_Color { } {
    variable colors
    set colors(style) "black"

    array set colors {
      SystemWindow        "#424242"
      SystemWindowFrame   "#424242"
      SystemWindowText    "Black"
      SystemButtonFace    "#efefef"
      SystemButtonText    "Black"
      SystemDisabledText  "DarkGrey"
      SystemHighlight     "Black"
      SystemHighlightText "LightGrey"
      SystemMenu          "#222222"
      SystemMenuText      "#ffffff"
      SystemScrollbar     "#626262"
    }
}


proc ::BWidget::aquativo_Color { } {
    variable colors
    set colors(style) "aquativo"

    array set colors {
      SystemWindow        "#EDF3FE"
      SystemWindowFrame   "White"
      SystemWindowText    "Black"
      SystemButtonFace    "#fafafa"
      SystemButtonText    "Black"
      SystemDisabledText  "#fafafa"
      SystemHighlight     "RoyalBlue"
      SystemHighlightText "White"
      SystemMenu          "LightGrey"
      SystemMenuText      "Black"
      SystemScrollbar     "White"
    }
}

