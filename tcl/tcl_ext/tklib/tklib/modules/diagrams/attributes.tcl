## -*- tcl -*-
## (C) 2010 Andreas Kupries <andreas_kupries@users.sourceforge.net>
## BSD Licensed
# # ## ### ##### ######## ############# ######################

#
# diagram attribute database, basic data plus extensibility features.

##
# # ## ### ##### ######## ############# ######################
## Requisites

package require Tcl 8.5             ; # Want the nice things it brings (dicts, {*}, etc.)
package require snit                ; # Object framework.
package require struct::queue       ; # Word storage when processing attribute arguments.

# # ## ### ##### ######## ############# ######################
## Implementation

snit::type ::diagram::attribute {

    # # ## ### ##### ######## ############# ######################
    ## Public API :: Attribute extensibility

    method new {name args} {
	array set spec $args

	if {![info exists spec(key)]} { set spec(key) $name }
	set key $spec(key)

	set getvalue   [GetFunction       spec]
	set ovalidate  [ValidateFunction  spec] ; # snit validation type, or API compatible.
	set otransform [TransformFunction spec] ; # o* <=> optional function.
	set merger     [MergeFunction     spec $key]
	set odefault   [DefaultFunction   spec $key]

	set myattrp($name) [ProcessingFunction $getvalue $ovalidate $otransform $merger]

	if {![llength $odefault]} return

	set myattrd($key) $odefault
	{*}$odefault init
	return
    }

    method {unknown =} {unknowncmd} {
	set myunknown [list $unknowncmd]
	return
    }

    method {unknown +} {unknowncmd} {
	lappend myunknown $unknowncmd
	return
    }

    # # ## ### ##### ######## ############# ######################
    ## Public API :: attribute processing, integrated loading of requested defaults.

    method attributes {shape words required} {
	return [$self defaults [$self process $shape $words] $required]
    }

    method process {shape words} {
	if {![llength $words]} {
	    return {}
	}

	set attributes [ReadySame $shape]

	{*}$wq clear
	{*}$wq put {*}$words

	while {[{*}$wq size]} {
	    set aname [{*}$wq get]
	    set shape [dict get $attributes /shape]

	    if {[{*}$wq size]} {
		#puts A|do|$aname|/$shape|\t\t(([{*}$wq peek [{*}$wq size]]))
	    } else {
		#puts A|do|$aname|/$shape|\t\t(())
	    }

	    # Check for a shape-specific attribute first, then try the
	    # name as is.

	    if {[info exists myattrp(${shape}::$aname)]} {
		{*}$myattrp(${shape}::$aname) $wq attributes
		continue
	    } elseif {[info exists myattrp($aname)]} {
		{*}$myattrp($aname) $wq attributes
		continue
	    }

	    #puts A|unknown|$aname|

	    # Hooks for unknown names, for dynamic extension.
	    {*}$wq unget $aname
	    set ok 0
	    foreach hook $myunknown {
		#puts A|unknown/$shape|\t\t(([{*}$wq peek [{*}$wq size]]))
		if {[{*}$hook $shape $wq]} {
		    #puts A|unknown|taken|$hook
		    set ok 1
		    break
		}
	    }
	    if {$ok} continue
	    BadAttribute $shape $wq
	}

	#puts A|done|$attributes|

	SaveSame $attributes
	return $attributes
    }

    method defaults {attributes required} {
	# Note: All default hooks are run, even if the key is already
	# specified. This gives the hook the opportunity to not only
	# fill in defaults, but to compute and store derived
	# information (from multiple other attributes) as well. An
	# example using this ability are the Waypoint and ArcLocation
	# handlers which essentially precompute large parts of their
	# elements' geometry.

	foreach key $required {
	    #if {[dict exists $attributes $key]} continue
	    if {![info exists myattrd($key)]} {
		#return -code error "Unable to determine a default for \"$key\""
		continue
	    }
	    {*}$myattrd($key) fill attributes
	}
	return $attributes
    }

    method set {attributes} {
	dict for {key value} $attributes {
	    if {![info exists myattrd($key)]} continue
	    {*}$myattrd($key) set $key $value
	}
	return
    }

    # # ## ### ##### ######## ############# ######################
    ## Public API :: Instance construction

    constructor {core} {
	# Core attributes (shape redefinition, history access (same))
	set mycore    $core
	#set myunknown [myproc BadAttribute]

	$self new /shape                        merge [mymethod Merge/shape]
	$self new same   get [mymethod GetSame] merge [mymethod MergeSame]

	install wq using struct::queue ${selfns}::WQ

	# Queue Tracer
	if {0} {set wq [list ::apply [list {args} {
	    puts $args
	    uplevel 1 $args
	}] $wq]}

	return
    }

    # # ## ### ##### ######## ############# ######################
    ##

    proc ReadySame {shape} {
	upvar 1 mycurrentsame mycurrentsame mysame mysame
	set mycurrentsame {}
	catch {
	    set mycurrentsame $mysame($shape)
	}
	return [list /shape $shape]
    }

    proc SaveSame {attributes} {
	upvar 1 mysame mysame
	set shape [dict get $attributes /shape]
	set mysame($shape) $attributes
	return
    }

    # # ## ### ##### ######## ############# ######################

    proc BadAttribute {shape words} {
	return -code error "Expected attribute, got \"[{*}$words peek]\""
    }

    # # ## ### ##### ######## ############# ######################

    method GetSame {words_dummy} {
	return $mycurrentsame
    }

    method MergeSame {key samedict attributes} {
	# key == "same"
	return [dict merge $attributes $samedict]
    }

    method Merge/shape {key newshape attributes} {
	# key == "/shape"
	ReadySame $newshape
	dict set attributes /shape $newshape
	return $attributes
    }

    # # ## ### ##### ######## ############# ######################

    method Get {words} {
	return [{*}$words get]
    }

    # # ## ### ##### ######## ############# ######################

    method Set {key value attributes} {
	#puts AM.=|$key||$value|\t|$attributes|

	dict set attributes $key $value

	#puts AM:=|$attributes|
	return $attributes
    }

    method Lappend {key value attributes} {
	#puts AM++|$key||$value|\t|$attributes|

	dict lappend attributes $key $value

	#puts AM:=|$attributes|
	return $attributes
    }

    # # ## ### ##### ######## ############# ######################

    method Linked {key varname defaultvalue cmd args} {
	#puts "Linked ($key $varname $defaultvalue) $cmd $args"

	$self Linked_ $cmd $key $varname $defaultvalue {*}$args
    }

    method {Linked_ init} {key varname defaultvalue} {
	$mycore state set $varname $defaultvalue
	return
    }

    method {Linked_ set} {key varname defaultvalue _key newvalue} {
	$mycore state set $varname $newvalue
	return
    }

    method {Linked_ fill} {key varname defaultvalue av} {
	upvar 2 $av attributes ; # Bypass the 'Linked' dispatcher.
	#puts LINK|$key|$varname|-|$attributes|-|[$mycore state get $varname]|
	if {[dict exists $attributes $key]} return
	dict set attributes $key [$mycore state get $varname]
	return
    }

    # # ## ### ##### ######## ############# ######################
    ## Helper commands processing an attribute specification into a set of anonymous functions

    proc GetFunction {sv} {
	upvar 1 $sv spec selfns selfns
	if {[info exists spec(get)]} { return $spec(get) }
	return [mymethod Get]
    }

    proc ValidateFunction {sv} {
	upvar 1 $sv spec
	if {[info exists spec(type)]} {
	    set f $spec(type)
	    if {[llength $f] > 1} {
		# The specification is type + arguments. Create a
		# proper object by inserting a name into the command and then running it.
		set f [eval [linsert $f 1 AttrType%AUTO%]]
	    }
	    return [list {*}$f validate]
	}
	return {}
    }

    proc TransformFunction {sv} {
	upvar 1 $sv spec
	if {[info exists spec(transform)]} { return $spec(transform) }
	return {}
    }

    proc MergeFunction {sv key} {
	upvar 1 $sv spec selfns selfns
	if {[info exists spec(merge)]} { return [list {*}$spec(merge) $key] }
	if {![info exists spec(aggregate)]} {
	    set spec(aggregate) 0
	}
	if {$spec(aggregate)} {
	    return [mymethod Lappend $key]
	} else {
	    return [mymethod Set $key]
	}
    }

    proc DefaultFunction {sv key} {
	upvar 1 $sv spec selfns selfns
	if {[info exists spec(default)]} { return $spec(default) }
	if {[info exists spec(linked)]} {
	    #lassign $spec(linked) varname defaultvalue
	    return [mymethod Linked $key {*}$spec(linked)]
	}
	return {}
    }

    proc ProcessingFunction {get validate transform merge} {
	# partial functions.
	# validate, transform - optional
	# get, merge          - required

	# Types
	# get       : wordvar -> value
	# transform : value   -> value
	# validate  : value   -> value
	# merge     : value -> dict -> dict

	if {[llength $validate] && [llength $transform]} {
	    return [list ::apply [list {get validate transform merge words av} {
		upvar 1 $av attributes
		set value      [{*}$get       $words]
		set value      [{*}$transform $value]
		set value      [{*}$validate  $value]
		set attributes [{*}$merge     $value $attributes]
	    }] $get $validate $transform $merge]

	} elseif {[llength $validate]} {
	    return [list ::apply [list {get validate merge words av} {
		upvar 1 $av attributes
		set value      [{*}$get      $words]
		set value      [{*}$validate $value]
		set attributes [{*}$merge    $value $attributes]
	    }] $get $validate $merge]

	} elseif {[llength $transform]} {
	    return [list ::apply [list {get transform merge words av} {
		upvar 1 $av attributes
		set value      [{*}$get       $words]
		set value      [{*}$transform $value]
		set attributes [{*}$merge     $value $attributes]
	    }] $get $transform $merge]

	} else {
	    return [list ::apply [list {get merge words av} {
		upvar 1 $av attributes
		set value      [{*}$get   $words]
		set attributes [{*}$merge $value $attributes]
	    }] $get $merge]
	}
    }

    # # ## ### ##### ######## ############# ######################
    ## Instance data. Maps from attribute names and dictionary keys to
    ## relevant functions for processing input and defaults.

    variable mycore    {}
    variable myunknown {}

    variable myattrp -array {} ; # attribute command -> processing function
    variable myattrd -array {} ; # attribute key -> default management function

    # History stack, one level deep, keyed by shape name.

    variable mysame -array {}
    variable mycurrentsame {}

    component wq ; # Storage for the words we are processing as attributes.

    ##
    # # ## ### ##### ######## ############# ######################
}

# # ## ### ##### ######## ############# ######################
## Ready

package provide diagram::attribute 1
