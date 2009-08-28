# $Id: wafecompat.tcl,v 1.4 2006/09/27 08:12:40 neumann Exp $
package provide xotcl::wafecompat 0.9

set WAFELIB        /usr/lib/X11/wafe/
set MODULE_PATH    "$WAFELIB $auto_path" 
set COMPONENT_PATH $WAFELIB/otcl-classes
proc MOTIFPREFIX {} {return {}}
proc requireModules modules {
  global MODULE_PATH 
  foreach {cmd module} $modules {
    if {{} ne [info command $cmd] } continue
    if {[regexp {([A-Za-z1-9]+)Gen} $module _ n] ||
	[regexp {lib([a-z]+)} $module _ n] ||
	[regexp {^(.+)[.]so} $module _ n]
      } {
      set name [string toupper $n]
    }
    foreach path $MODULE_PATH {
      set f $path/tcllib/bin/$module
      if {[set found [file exists $f]]} {
	puts stderr "Loading module $name from $f"
	load $f $name
	break
      }
    }
    if {!$found} { error "Could not find module $module in {$MODULE_PATH}"}
}}
proc requireTclComponents {files} {
  global COMPONENT_PATH _componentLoaded
  foreach component $files {
    if {[info exists _componentLoaded($component)]} continue
    foreach path $COMPONENT_PATH {
      set f $path/$component
      if {[file exists $f]} {
	puts stderr "Loading source file $f"
	uplevel \#0 source $f
	set _componentLoaded($component) $f
	break
      }
    }
    if {![info exists _componentLoaded($component)]} {
      error "Could not find component $component in {$COMPONENT_PATH}"
    }
}}
proc addTimeOut {n cmd} {
  after $n $cmd
}
proc removeTimeOut {n} {
  after cancel $n
}
proc quit {} { exit }
