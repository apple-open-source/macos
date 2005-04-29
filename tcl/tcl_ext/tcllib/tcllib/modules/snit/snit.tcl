#-----------------------------------------------------------------------
# TITLE:
#	snit.tcl
#
# AUTHOR:
#	Will Duquette
#
# DESCRIPTION:
#       Snit's Not Incr Tcl: yet another simple object system in Pure Tcl, 
#	just because I wanted to.
#
#-----------------------------------------------------------------------

package provide snit 0.93

#-----------------------------------------------------------------------
# Namespace

namespace eval ::snit:: {
    namespace export type widget widgetadaptor typemethod method
}

#-----------------------------------------------------------------------
# Standard method and typemethod definition templates

namespace eval ::snit:: {
    variable reservedArgs {type selfns win self}

    # If true, get a pretty, fixed-up stack trace.  Otherwise, get raw
    # stack trace.
    variable prettyStackTrace 1

    # The elements of defs are standard methods and typemethods; it's
    # most convenient to define these as though they'd been typed in by
    # the class author.  The following tokens will be substituted:
    #
    # %TYPE%    The type name
    #
    variable defs

    # Methods common to both types and widgettypes.
    set defs(common) {
        # Options array
        variable options

        # Instance Introspection: info <command> <args>
        method info {command args} {
            global errorInfo

            switch -exact $command {
                type    -
                vars    -
                options -
                typevars {
                    set errflag [catch {
                        uplevel ::snit::InstanceInfo_$command \
                            $type $selfns $win $self $args
                    } result]

                    if {$errflag} {
                        return -code error -errorinfo $errorInfo $result
                    } else {
                        return $result
                    }
                }
                default {
                    error "'$self info $command' is not defined."
                }
            }
        }

        # Type Introspection: info <command> <args>
        typemethod info {command args} {
            global errorInfo
            global errorCode

            switch -exact $command {
                typevars - 
                instances {
                    # TBD: it should be possible to delete this error
                    # handling.
                    set errflag [catch {
                        uplevel ::snit::TypeInfo_$command \
                            $type $args
                    } result]

                    if {$errflag} {
                        return -code error -errorinfo $errorInfo \
                            -errorcode $errorCode $result
                    } else {
                        return $result
                    }
                }
                default {
                    error "'$type info $command' is not defined."
                }
            }
        }

        method cget {option} {
            typevariable Snit_optiondefaults
            typevariable Snit_delegatedoptions
            typevariable Snit_info
                
            if {[info exists Snit_optiondefaults($option)]} {
                # Normal option; return it.
                return [Snit_cget$option $type $selfns $win $self]
            } elseif {[info exists Snit_delegatedoptions($option)]} {
                # Delegated option: get target.
                set comp [lindex $Snit_delegatedoptions($option) 0]
                set target [lindex $Snit_delegatedoptions($option) 1]
            } elseif {[info exists Snit_delegatedoptions(*)] &&
                      [lsearch -exact $Snit_info(exceptopts) $option] == -1} {
                # Unknown option, but unknowns are delegated; get target.
                set comp [lindex $Snit_delegatedoptions(*) 0]
                set target $option
            } else {
                # Use quotes because Tk does.
                error "unknown option \"$option\""
            }
            
            # Get the component's object.
            set obj [Snit_component $selfns $comp]

            # TBD: I'll probably want to fix up certain error
            # messages, but I'm not sure how yet.
            return [$obj cget $target]
        }

        method configurelist {optionlist} {
            typevariable Snit_optiondefaults
            typevariable Snit_delegatedoptions
            typevariable Snit_info

            foreach {option value} $optionlist {
                if {[info exist Snit_optiondefaults($option)]} {
                    Snit_configure$option $type $selfns $win $self $value
                    continue
                } elseif {[info exists Snit_delegatedoptions($option)]} {
                    # Delegated option: get target.
                    set comp [lindex $Snit_delegatedoptions($option) 0]
                    set target [lindex $Snit_delegatedoptions($option) 1]
                } elseif {[info exists Snit_delegatedoptions(*)] &&
                      [lsearch -exact $Snit_info(exceptopts) $option] == -1} {
                    # Unknown option, but unknowns are delegated.
                    set comp [lindex $Snit_delegatedoptions(*) 0]
                    set target $option
                } else {
                    # Use quotes because Tk does.
                    error "unknown option \"$option\""
                }

                # Get the component's object
                set obj [Snit_component $selfns $comp]
                    
                $obj configure $target $value
            }
            
            return
        }

        method configure {args} {
            typevariable Snit_delegatedoptions
            typevariable Snit_optiondefaults
            typevariable Snit_optiondbspec
            typevariable Snit_info

            # If two or more arguments, set values as usual.
            if {[llength $args] >= 2} {
                $self configurelist $args
                return
            }

            # If zero arguments, acquire data for each known option
            # and return the list
            if {[llength $args] == 0} {
                set result {}
                foreach opt [$self info options] {
                    lappend result [$self configure $opt]
                }

                return $result
            }

            # They want it for just one.
            upvar ${selfns}::Snit_components Snit_components
            set opt [lindex $args 0]

            if {[info exists options($opt)]} {
                # This is a locally-defined option.  Just build the
                # list and return it.
                set res [lindex $Snit_optiondbspec($opt) 0]
                set cls [lindex $Snit_optiondbspec($opt) 1]

                return [list $opt $res $cls $Snit_optiondefaults($opt) \
                            [$self cget $opt]]
            } elseif {[info exists Snit_delegatedoptions($opt)]} {
                # This is an explicitly delegated option.  The only
                # thing we don't have is the default.
                set res [lindex $Snit_optiondbspec($opt) 0]
                set cls [lindex $Snit_optiondbspec($opt) 1]

                # Get the default
                set logicalName [lindex $Snit_delegatedoptions($opt) 0]
                set comp $Snit_components($logicalName)
                set target [lindex $Snit_delegatedoptions($opt) 1]

                if {[catch {$comp configure $target} result]} {
                    set defValue {}
                } else {
                    set defValue [lindex $result 3]
                }

                return [list $opt $res $cls $defValue [$self cget $opt]]
            } elseif {[info exists Snit_delegatedoptions(*)] &&
                      [lsearch -exact $Snit_info(exceptopts) $opt] == -1} {
                set logicalName [lindex $Snit_delegatedoptions(*) 0]
                set target $opt
                set comp $Snit_components($logicalName)

                if {[catch {set value [$comp cget $target]} result]} {
                    error "unknown option \"$opt\""
                }

                if {![catch {$comp configure $target} result]} {
                    # Replace the delegated option name with the local name.
                    return [snit::Expand $result $target $opt]
                }

                # configure didn't work; return simple form.
                return [list $opt "" "" "" $value]
            } else {
                error "unknown option \"$opt\""
            }
        }

        # $type destroy
        #
        # Destroys a type completely.
        typemethod destroy {} {
            typevariable Snit_isWidget

            # FIRST, destroy all instances
            foreach selfns [namespace children $type] {
                if {![namespace exists $selfns]} {
                    continue
                }
                upvar ${selfns}::Snit_instance obj
                
                if {$Snit_isWidget} {
                    destroy $obj
                } else {
                    if {"" != [info commands $obj]} {
                        $obj destroy
                    }
                }
            }

            # NEXT, destroy the type's data.
            namespace delete $type

            # NEXT, get rid of the type command.
            rename $type ""
        }
    }

    # Methods specific to plain types.
    set defs(type) {
        # Calls Snit_cleanup, which (among other things) calls the
        # user's destructor.
        method destroy {} {
            Snit_cleanup $selfns $win
        }

        # Creates a new instance of the type given its name and the args.
        typemethod create {name args} {
            typevariable Snit_info
            typevariable Snit_optiondefaults

            # FIRST, qualify the name.
            if {![string match "::*" $name]} {
                # Get caller's namespace; 
                # append :: if not global namespace.
                set ns [uplevel 1 namespace current]
                if {"::" != $ns} {
                    append ns "::"
                }
        
                set name "$ns$name"
            }

            # NEXT, if %AUTO% appears in the name, generate a unique 
            # command name.
            if {[string match "*%AUTO%*" $name]} {
                set name [snit::UniqueName Snit_info(counter) $type $name]
            }

            # NEXT, create the instance's namespace.
            set selfns \
                [snit::UniqueInstanceNamespace Snit_info(counter) %TYPE%]
            namespace eval $selfns {}

            # NEXT, install the dispatcher
            Snit_install $selfns $name

            # Initialize the options to their defaults. 

            upvar ${selfns}::options options
            foreach opt $Snit_info(options) {
                set options($opt) $Snit_optiondefaults($opt)
            }

            # Initialize the instance vars to their defaults.
            # selfns must be defined, as it is used implicitly.
            Snit_instanceVars $selfns

            # Execute the type's constructor.
            set errcode [catch {
                eval Snit_constructor %TYPE% $selfns \
                    [list $name] [list $name] $args
            } result]

            if {$errcode} {
                global errorInfo
                global errorCode

                set theInfo $errorInfo
                set theCode $errorCode
                Snit_cleanup $selfns $name
                error "Error in constructor: $result" $theInfo $theCode
            }

            # NEXT, return the object's name.
            return $name
        }
    }

    # Methods specific to widgets.
    set defs(widget) {
        # Creates a new instance of the widget, given the name and args.
        typemethod create {name args} {
            typevariable Snit_info
            typevariable Snit_optiondefaults
            typevariable Snit_optiondbspec
            typevariable Snit_isWidgetAdaptor

            # FIRST, if %AUTO% appears in the name, generate a unique 
            # command name.
            if {[string match "*%AUTO%*" $name]} {
                set name [snit::UniqueName Snit_info(counter) $type $name]
            }
            
            # NEXT, create the instance's namespace.
            set selfns \
                [snit::UniqueInstanceNamespace Snit_info(counter) %TYPE%]
            namespace eval $selfns { }
            
            # NEXT, Initialize the widget's own options to their defaults.
            upvar ${selfns}::options options
            foreach opt $Snit_info(options) {
                set options($opt) $Snit_optiondefaults($opt)
            }

            # Initialize the instance vars to their defaults.
            Snit_instanceVars $selfns

            # NEXT, if this is a normal widget (not a widget adaptor) then 
            # create a frame as its hull.  We set the frame's -class to
            # the user's widgetclass, or, if none, to the basename of
            # the %TYPE% with an initial upper case letter.
            if {!$Snit_isWidgetAdaptor} {
                # FIRST, determine the class name
                if {"" == $Snit_info(widgetclass)} {
                    set Snit_info(widgetclass) \
                        [::snit::Capitalize [namespace tail %TYPE%]]
                }

                # NEXT, create the widget
                set self $name
		package require Tk
                installhull using \
                    $Snit_info(hulltype) -class $Snit_info(widgetclass)

                # NEXT, let's query the option database for our
                # widget, now that we know that it exists.
                foreach opt $Snit_info(options) {
                    set dbval [Snit_optionget $name $opt]

                    if {"" != $dbval} {
                        set options($opt) $dbval
                    }
                }
            }

            # Execute the type's constructor, and verify that it
            # has a hull.
            set errcode [catch {
                eval Snit_constructor %TYPE% $selfns [list $name] \
                    [list $name] $args

                Snit_component $selfns hull

                # Prepare to call the object's destructor when the
                # <Destroy> event is received.  Use a Snit-specific bindtag
                # so that the widget name's tag is unencumbered.

                bind Snit%TYPE%$name <Destroy> [::snit::Expand {
                    %TYPE%::Snit_cleanup %NS% %W
                } %NS% $selfns]
                
                # Insert the bindtag into the list of bindtags right
                # after the widget name.
                set taglist [bindtags $name]
                set ndx [lsearch $taglist $name]
                incr ndx
                bindtags $name [linsert $taglist $ndx Snit%TYPE%$name]
            } result]

            if {$errcode} {
                global errorInfo
                global errorCode

                set theInfo $errorInfo
                set theCode $errorCode
                Snit_cleanup $selfns $name
                error "Error in constructor: $result" $theInfo $theCode
            }
            
            # NEXT, return the object's name.
            return $name
        }
    }
}

#-----------------------------------------------------------------------
# Snit Type Implementation template

namespace eval ::snit:: {
    # Template type definition: All internal and user-visible Snit
    # implementation code.
    #
    # The following placeholders will automatically be replaced with
    # the client's code, in two passes:
    #
    # First pass:
    # %COMPILEDDEFS%  The compiled type definition.
    #
    # Second pass:
    # %TYPE%          The fully qualified type name.
    # %IVARDECS%      Instance variable declarations
    # %TVARDECS%      Type variable declarations
    # %TCONSTBODY%    Type constructor body
    # %INSTANCEVARS%  The compiled instance variable initialization code.
    # %TYPEVARS%      The compiled type variable initialization code.

    variable typeTemplate {

        #----------------------------------------------------------------
        # Snit Internals
        #
        # These commands are used internally by Snit, and are not to be
        # used directly by any client code.  Nevertheless they are
        # defined here so that they live in the correct namespace.

	namespace eval %TYPE% {%TYPEVARS%
        }

        # Snit_cleanup selfns win
        #
        # This is the function that really cleans up; it's automatically 
        # called when any instance is destroyed, e.g., by "$object destroy"
        # for types, and by the <Destroy> event for widgets.

        proc %TYPE%::Snit_cleanup {selfns win} {
            typevariable Snit_isWidget

            # If the variable Snit_instance doesn't exist then there's no
            # instance command for this object -- it's most likely a 
            # widgetadaptor. Consequently, there are some things that
            # we don't need to do.
            if {[info exists ${selfns}::Snit_instance]} {
                upvar ${selfns}::Snit_instance instance

                # First, remove the trace on the instance name, so that we
                # don't call Snit_cleanup recursively.
                Snit_removetrace $selfns $win $instance

                # Next, call the user's destructor
                Snit_destructor %TYPE% $selfns $win $instance

                # Next, if this isn't a widget, delete the instance command.
                # If it is a widget, get the hull component's name, and rename
                # it back to the widget name
                
                # Next, delete the hull component's instance command,
                # if there is one.
                if {$Snit_isWidget} {
                    set hullcmd [Snit_component $selfns hull]
                
                    catch {rename $instance ""}

                    # Clear the bind event
                    bind Snit%TYPE%$win <Destroy> ""

                    if {[info command $hullcmd] != ""} {
                        rename $hullcmd ::$instance
                    }
                } else {
                    catch {rename $instance ""}
                }
            }

            # Next, delete the instance's namespace.  This kills any
            # instance variables.
            namespace delete $selfns
        }

        # Retrieves the object name given the component name.
        proc  %TYPE%::Snit_component {selfns name} {
            upvar ${selfns}::Snit_components Snit_components
            upvar ${selfns}::Snit_instance self

            if {![info exists Snit_components($name)]} {
                error "component '$name' is undefined in %TYPE% $self."
            }

            return $Snit_components($name)
        }

        # Retrieves an option's value from the option database
        proc %TYPE%::Snit_optionget {self opt} {
            typevariable Snit_optiondbspec

            set res [lindex $Snit_optiondbspec($opt) 0]
            set cls [lindex $Snit_optiondbspec($opt) 1]

            return [option get $self $res $cls]
        }

        #----------------------------------------------------------------
        # Compiled Procs
        #
        # These commands are created or replaced during compilation:

        # Snit_constructor type selfns win self args
        #
        # By default, just configures any passed options.  
        # Redefined by the "constructor" definition, hence always redefined
        # for widgets.

        proc %TYPE%::Snit_constructor {type selfns win self args} { 
            $self configurelist $args
        }

        # Snit_instanceVars selfns
        #
        # Initializes the instance variables, if any.  Called during
        # instance creation.

        proc %TYPE%::Snit_instanceVars {selfns} {%IVARDECS%
            %INSTANCEVARS%
        }

        # Snit_comptrace 
        #
        # Component trace; used for write trace on component instance 
        # variables.  Saves the new component object name, provided 
        # that certain conditions are met.

        proc %TYPE%::Snit_comptrace {selfns component n1 n2 op} {
            typevariable Snit_isWidget
            upvar ${selfns}::${component} cvar
            upvar ${selfns}::Snit_components Snit_components

            # If they try to redefine the hull component after
            # it's been defined, that's an error--but only if
            # this is a widget or widget adaptor.
            if {"hull" == $component && 
                $Snit_isWidget &&
                [info exists ${selfns}::Snit_components($component)]} {
                set cvar $Snit_components($component)
                error "The hull component cannot be redefined."
            }

            # Save the new component value.
            set Snit_components($component) $cvar
        }

        # Snit_destructor type selfns win self
        #
        # Default destructor for the type.  By default, it does
        # nothing.  It's replaced by any user destructor.
        # For types, it's called by method destroy; for widgettypes,
        # it's called by a destroy event handler.

        proc %TYPE%::Snit_destructor {type selfns win self} { }

        # Snit_configure<option> type selfns win self value
        #
        # Defined for each local option.  By default, just updates the
        # options array.  Redefined by an onconfigure definition.

        # Snit_cget<option> type selfns win self value
        #
        # Defined for each local option.  By default, just retrieves the
        # element from the options array.  Redefined by an oncget definition.

        # Snit_method<name> type selfns win self args...
        #
        # Defined for each local instance method.

        # Snit_typemethod<name> type args...
        #
        # Defined for each typemethod.

        #----------------------------------------------------------------
        # Snit variable management 

        # typevariable Declares that a variable is a static type variable.
        # It's equivalent to "::variable", operating in the %TYPE%
        # namespace.
        interp alias {} %TYPE%::typevariable {} ::variable

        # Declares an instance variable in a method or proc, -OR- allows
        # the method or proc to reference a variable in some other 
        # namespace by its bare name.  It's only valid in instance code; 
        # it requires that selfns be defined.
        proc %TYPE%::variable {varname} {
            upvar selfns selfns

            if {![string match "::*" $varname]} {
                uplevel upvar ${selfns}::$varname $varname
            } else {
                # varname is fully qualified; let the standard
                # "variable" command handle it.
                uplevel ::variable $varname
            }
        }

        # Returns the fully qualified name of a typevariable.
        proc %TYPE%::typevarname {name} {
            return %TYPE%::$name
        }

        # Returns the fully qualified name of an instance variable.  
        # As with "variable", must be called in the context of a method.
        proc %TYPE%::varname {name} {
            upvar selfns selfns
            return ${selfns}::$name
        }

        # Returns the fully qualified name of a proc (or typemethod).  
        # Unlike "variable", need not be called in the context of an
        # instance method.
        proc %TYPE%::codename {name} {
            return %TYPE%::$name
        }

        # Use this like "list" to create a command string to pass to
        # another object (e.g., as a -command); it automatically inserts
        # the code at the beginning to call the right object, even if
        # the object's name has changed.  Requires that selfns be defined
        # in the calling context.
        proc %TYPE%::mymethod {args} {
            upvar selfns selfns
            return [linsert $args 0 ::snit::CallInstance ${selfns}]
        }


        # Installs the named widget as the hull of a 
        # widgetadaptor.  Once the widget is hijacked, it's new name
        # is assigned to the hull component.
        proc %TYPE%::installhull {{using "using"} {widgetType ""} args} {
            typevariable Snit_isWidget
            typevariable Snit_info
            typevariable Snit_compoptions
            typevariable Snit_delegatedoptions
            upvar self self
            upvar selfns selfns
            upvar ${selfns}::hull hull
            upvar ${selfns}::options options

            # FIRST, make sure we can do it.
            if {!$Snit_isWidget} { 
                error "installhull is valid only for snit::widgetadaptors"
            }
            
            if {[info exists ${selfns}::Snit_instance]} {
                error "hull already installed for %TYPE% $self"
            }

            # NEXT, has it been created yet?  If not, create it using
            # the specified arguments.
            if {"using" == $using} {
                # FIRST, create the widget
                set cmd [concat [list $widgetType $self] $args]
                set obj [uplevel 1 $cmd]

                # NEXT, for each option explicitly delegated to the hull
                # that doesn't appear in the usedOpts list, get the
                # option database value and apply it--provided that the
                # real option name and the target option name are different.
                # (If they are the same, then the option database was
                # already queried as part of the normal widget creation.)
                #
                # Also, we don't need to worry about implicitly delegated
                # options, as the option and target option names must be
                # the same.
                if {[info exists Snit_compoptions(hull)]} {

                    # FIRST, extract all option names from args
                    set usedOpts {}
                    set ndx [lsearch -glob $args "-*"]
                    foreach {opt val} [lrange $args $ndx end] {
                        lappend usedOpts $opt
                    }
                
                    foreach opt $Snit_compoptions(hull) {
                        if {"*" == $opt} {
                            continue
                        }

                        set target [lindex $Snit_delegatedoptions($opt) 1]

                        if {"$target" == $opt} {
                            continue
                        }

                        set result [lsearch -exact $usedOpts $target]

                        if {$result != -1} {
                            continue
                        }

                        set dbval [Snit_optionget $self $opt]
                        $obj configure $target $dbval
                    }
                }
            } else {
                set obj $using

                if {![string equal $obj $self]} {
                    error \
                        "hull name mismatch: '$obj' != '$self'"
                }
            }

            # NEXT, get the local option defaults.
            foreach opt $Snit_info(options) {
                set dbval [Snit_optionget $self $opt]
                
                if {"" != $dbval} {
                    set options($opt) $dbval
                }
            }


            # NEXT, do the magic
            set i 0
            while 1 {
                incr i
                set newName "::hull${i}$self"
                if {"" == [info commands $newName]} {
                    break
                }
            }

            rename ::$self $newName
            Snit_install $selfns $self

            # Note: this relies on Snit_comptrace to do the dirty work.
            set hull $newName

            return
        }

        # Creates a widget and installs it as the named component.
        proc %TYPE%::install {compName "using" widgetType winPath args} {
            typevariable Snit_isWidget
            typevariable Snit_compoptions
            typevariable Snit_delegatedoptions
            typevariable Snit_optiondbspec
            typevariable Snit_info
            upvar self self
            upvar selfns selfns
            upvar ${selfns}::$compName comp
            upvar ${selfns}::hull hull


            # We do the magic option database stuff only if $self is
            # a widget.
            if {$Snit_isWidget} {
                if {"" == $hull} {
                    error "tried to install '$compName' before the hull exists"
                }

                # FIRST, query the option database and save the results 
                # into args.  Insert them before the first option in the
                # list, in case there are any non-standard parameters.
                #
                # Note: there might not be any delegated options; if so,
                # don't bother.

                set gotStar 0

                if {[info exists Snit_compoptions($compName)]} {
                    set ndx [lsearch -glob $args "-*"]

                    foreach opt $Snit_compoptions($compName) {
                        # Handle * later
                        if {"*" == $opt} {
                            set gotStar 1
                            continue
                        }

                        set dbval [Snit_optionget $self $opt]
                    
                        if {"" != $dbval} {
                            set target [lindex $Snit_delegatedoptions($opt) 1]
                            set args [linsert $args $ndx $target $dbval]
                        }
                    }
                }
            }
             
            # NEXT, create the component and save it.
            set cmd [concat [list $widgetType $winPath] $args]
            set comp [uplevel 1 $cmd]

            # NEXT, handle the option database for "delegate option *",
            # in widgets only.
            if {$Snit_isWidget && $gotStar} {
                # FIRST, get the list of option specs from the widget.
                # If configure doesn't work, skip it.
                if {[catch {$comp configure} specs]} {
                    return
                }

                # NEXT, get the set of explicitly used options from args
                set usedOpts {}
                set ndx [lsearch -glob $args "-*"]
                foreach {opt val} [lrange $args $ndx end] {
                    lappend usedOpts $opt
                }

                # NEXT, "delegate option *" matches all options defined
                # by this widget that aren't defined by the widget as a whole,
                # and that aren't excepted.  Plus, we skip usedOpts.  So build 
                # a list of the invalid option names.
                set skiplist [concat $usedOpts $Snit_info(exceptopts) \
                                  [array names Snit_optiondbspec]]

                # NEXT, loop over all of the component's options, and set
                # any not in the skip list for which there is an option 
                # database value.
                foreach spec $specs {
                    # Skip aliases
                    if {[llength $spec] != 5} {
                        continue
                    }

                    set opt [lindex $spec 0]

                    if {[lsearch -exact $skiplist $opt] != -1} {
                        continue
                    }

                    set res [lindex $spec 1]
                    set cls [lindex $spec 2]

                    set dbvalue [option get $self $res $cls]

                    if {"" != $dbvalue} {
                        $comp configure $opt $dbvalue
                    }
                }
            }


            return
        }

        # Looks for the named option in the named variable.  If found,
        # it and its value are removed from the list, and the value
        # is returned.  Otherwise, the default value is returned.
        # If the option is undelegated, it's own default value will be
        # used if none is specified.
        proc %TYPE%::from {argvName option {defvalue ""}} {
            typevariable Snit_optiondefaults
            upvar $argvName argv

            set ioption [lsearch -exact $argv $option]

            if {$ioption == -1} {
                if {"" == $defvalue &&
                    [info exists Snit_optiondefaults($option)]} {
                    return $Snit_optiondefaults($option)
                } else {
                    return $defvalue
                }
            }

            set ivalue [expr {$ioption + 1}]
            set value [lindex $argv $ivalue]

            set argv [lreplace $argv $ioption $ivalue] 

            return $value
        }

        #----------------------------------------------------------------
        # Snit variables 
        #
        # TBD: At some point I really need to review all of these variables
        # and see if there's more a straightforward, clearer way to store
        # all of the same information.

	namespace eval %TYPE% {
	    # Array: General Snit Info
	    #
	    # ns:            The type's namespace
	    # options:       List of the names of the type's local options.
	    # counter:       Count of instances created so far.
            # widgetclass:   Set by widgetclass statement.
            # hulltype:      Hull type (frame or toplevel) for widgets only.
            # exceptmethods: Methods explicitly not delegated to *
            # exceptopts:    Options explicitly not delegated to *
            # tvardecs:      Type variable declarations--for dynamic methods
            # ivardecs:      Instance variable declarations--for dyn. methods
	    typevariable Snit_info
	    set Snit_info(ns)      %TYPE%::
	    set Snit_info(options) {}
	    set Snit_info(counter) 0
            set Snit_info(widgetclass) {}
            set Snit_info(hulltype) frame
            set Snit_info(exceptmethods) {}
            set Snit_info(exceptopts) {}
            set Snit_info(tvardecs) {%TVARDECS%}
            set Snit_info(ivardecs) {%IVARDECS%}

	    # Array: Public methods of this type.
	    # Index is typemethod name; value is proc name.
	    typevariable Snit_typemethods
	    array unset Snit_typemethods

	    # Array: Public methods of instances of this type.
	    # The index is the method name.  For normal methods, 
	    # the value is "".  For delegated methods, the value is
	    # [list $component $command].
	    typevariable Snit_methods
	    array unset Snit_methods

	    # Array: default option values
	    #
	    # $option          Default value for the option
	    typevariable Snit_optiondefaults

	    # Array: database spec values for explicitly defined
            # options
	    #
	    # $option          [list resourceName className]
	    typevariable Snit_optiondbspec

	    # Array: delegated option components
	    #
	    # $option          Component to which the option is delegated.
	    typevariable Snit_delegatedoptions

            # Array: delegated options by component.
            typevariable Snit_compoptions
	}

        #----------------------------------------------------------
        # Type Command

        # Type dispatcher function.  Note: This function lives
        # in the parent of the %TYPE% namespace!  All accesses to 
        # %TYPE% variables and methods must be qualified!
        proc %TYPE% {method args} {
            global errorInfo
            global errorCode

            # First, if the typemethod is unknown, we'll assume that it's
            # an instance name if we can.
            if {![info exists %TYPE%::Snit_typemethods($method)]} {
                if {[set %TYPE%::Snit_isWidget] && 
                    ![string match ".*" $method]} {
                    return -code error  "\"%TYPE% $method\" is not defined"
                }
                set args [concat $method $args]
                set method create
            }

            set procname [set %TYPE%::Snit_typemethods($method)]

            set errflag [catch {
                uplevel [concat %TYPE%::$procname %TYPE% $args]
            } result]

            if {$errflag} {
                return -code error \
                    -errorinfo $errorInfo \
                    -errorcode $errorCode \
                    $result
            } else {
                return $result
            }
        }

        #----------------------------------------------------------------
        # Dispatcher Command

        # Snit_install selfns instance
        #
        # Creates the instance proc, which calls the Snit_dispatcher.
        # "self" is the initial name of the instance, and "selfns" is
        # the instance namespace.
        proc %TYPE%::Snit_install {selfns instance} {
            typevariable Snit_isWidget

            # FIRST, remember the instance name.  The Snit_instance variable
            # allows the instance to figure out its current name given the
            # instance namespace.
            upvar ${selfns}::Snit_instance Snit_instance
            set Snit_instance $instance

            # NEXT, qualify the proc name if it's a widget.
            if {$Snit_isWidget} {
                set procname ::$instance
            } else {
                set procname $instance
            }

            # NEXT, install the new proc
            proc $procname {method args} \
                "%TYPE%::Snit_dispatcher %TYPE% $selfns $instance \[set ${selfns}::Snit_instance] \$method \$args"

            # NEXT, add the trace.
            trace add command $procname {rename delete} \
                [list %TYPE%::Snit_tracer $selfns $instance]
        }

        # Snit_removetrace selfns instance
        proc %TYPE%::Snit_removetrace {selfns win instance} {
            typevariable Snit_isWidget

            if {$Snit_isWidget} {
                set procname ::$instance
            } else {
                set procname $instance
            }

            # NEXT, remove any trace on this name
	    catch {
		trace remove command $procname {rename delete} \
			[list %TYPE%::Snit_tracer $selfns $win]
	    }
        }

        # Snit_tracer old new op
        #
        # This proc is called when the instance command is renamed.  old
        # is the old name, new is the new name, and op is rename or delete.
        # If op is delete, then new will always be "", so op is redundant.
        #
        # If the op is delete, we need to clean up the object; otherwise,
        # we need to track the change.
        #
        # NOTE: In Tcl 8.4.2 there's a bug: errors in rename and delete
        # traces aren't propagated correctly.  Instead, they silently
        # vanish.  Add a catch to output any error message.

        proc %TYPE%::Snit_tracer {selfns win old new op} {
            typevariable Snit_isWidget

	    # Note to developers ...
	    # For Tcl 8.4.0, errors thrown in trace handlers vanish silently.
	    # Therefore we catch them here and create some output to help in
	    # debugging such problems.

            if {[catch {
                # FIRST, clean up if necessary
                if {"" == $new} {
                    if {$Snit_isWidget} {
                        destroy $win
                    } else {
                        Snit_cleanup $selfns $win
                    }
                } else {
                    # Otherwise, track the change.
                    upvar ${selfns}::Snit_instance Snit_instance
                    set Snit_instance [uplevel namespace which -command $new]
                }
            } result]} {
                global errorInfo
                # Pop up the console on Windows wish, to enable stdout.
		# This clobbers errorInfo unix, so save it.
		set ei $errorInfo
                catch {console show}
                puts "Error in Snit_tracer $selfns $win $old $new $op:"
                puts $ei
            }
        }

        # Calls a local method or a delegated method.
        #
        # type:		The instance's type
        # selfns:	The instance's private namespace
        # win:          The instance's original name (a Tk widget name, for
        #               snit::widgets.
        # self:         The instance's current name.
        # method:	The name of the method to call.
        # argList:      Arguments for the method.
        proc %TYPE%::Snit_dispatcher {type selfns win self method argList} {
            global errorInfo
            global errorCode

            typevariable Snit_info
            typevariable Snit_methods
            upvar ${selfns}::Snit_components Snit_components
            
            if {![info exists Snit_methods($method)]} {
                if {![info exists Snit_methods(*)] ||
                    [lsearch -exact $Snit_info(exceptmethods) $method] != -1} {
                    return -code error \
                        "'$self $method' is not defined."
                }
                set delegate [concat $Snit_methods(*) $method]
            } else {
                set delegate $Snit_methods($method)
            }

            if {[string length $delegate] == 0} {
                set command \
                    [list ${type}::Snit_method$method $type $selfns $win $self]
            } else {
                # Handle delegate
                set component [lindex $delegate 0]

                if {![info exists Snit_components($component)]} {
                    error "$type $self delegates '$method' to undefined component '$component'."
                }

                set comp $Snit_components($component)
                
                set command [lreplace $delegate 0 0 $comp]
            }

            set errflag [catch {
                uplevel 2 $command $argList
            } result]

            if {$errflag} {
		# Used to try to fix up "bad option", but did it badly.
                
                return -code error -errorinfo $errorInfo \
                    -errorcode $errorCode $result
            } else {
                return $result
            }
        }


        #----------------------------------------------------------
        # Compiled Definitions
            
        %COMPILEDDEFS%

        #----------------------------------------------------------
        # Type Constructor

        proc %TYPE%::Snit_typeconstructor {type} {
            %TVARDECS%
            %TCONSTBODY%
        }

        %TYPE%::Snit_typeconstructor %TYPE%
    }
}

#-----------------------------------------------------------------------
# Snit Compilation Variables
#
# The following variables are used while Snit is compiling a type,
# and are disposed afterwards.

namespace eval ::snit:: {
    # The compile array accumulates information about the type or
    # widgettype being compiled.  It is cleared before and after each
    # compilation.  It has these indices:
    #
    # defs:              Compiled definitions, both standard and client.
    # which:             type, widget, widgetadaptor
    # instancevars:      Instance variable definitions and initializations.
    # ivprocdec:         Instance variable proc declarations.
    # tvprocdec:         Type variable proc declarations.
    # typeconstructor:   Type constructor body.
    # widgetclass:       The widgetclass, for snit::widgets, only
    # localoptions:      Names of local options.
    # delegatedoptions:  Names of delegated options.
    # localmethods:      Names of locally defined methods.
    # delegatedmethods:  Names of delegated methods.
    # components:        Names of defined components.
    # typevars:          See 'instancevars' above, except this is for typevariables.

    variable compile

}

#-----------------------------------------------------------------------
# type compilation commands
#
# The type and widgettype commands use a slave interpreter to compile
# the type definition.  These are the procs
# that are aliased into it.

# Defines a widget's option class name.  
# This statement is only available for snit::widgets,
# not for snit::types or snit::widgetadaptors.
proc ::snit::Type.Widgetclass {type name} {
    variable compile

    # First, widgetclass can only be set for true widgets
    if {"widget" != $compile(which)} {
        error "widgetclass cannot be set for snit::$compile(which)s"
    }

    # Next, validate the option name.  We'll require that it begin
    # with an uppercase letter.
    set initial [string index $name 0]
    if {![string is upper $initial]} {
        error "widgetclass '$name' does not begin with an uppercase letter"
    }

    if {"" != $compile(widgetclass)} {
        error "too many widgetclass statements"
    }

    # Next, save it.
    Mappend compile(defs) {
        set  %TYPE%::Snit_info(widgetclass) %WIDGETCLASS%
    } %WIDGETCLASS% [list $name]

    set compile(widgetclass) $name
}

# Defines a widget's hull type.
# This statement is only available for snit::widgets,
# not for snit::types or snit::widgetadaptors.
proc ::snit::Type.Hulltype {type name} {
    variable compile

    # First, hulltype can only be set for true widgets
    if {"widget" != $compile(which)} {
        error "hulltype cannot be set for snit::$compile(which)s"
    }

    # Next, it must be either "frame" or "toplevel"
    if {"frame" != $name && "toplevel" != $name} {
        error "invalid hulltype '$name', should be 'frame' or 'toplevel'"
    }

    if {"" != $compile(hulltype)} {
        error "too many hulltype statements"
    }

    # Next, save it.
    Mappend compile(defs) {
        set  %TYPE%::Snit_info(hulltype) %HULLTYPE%
    } %HULLTYPE% $name

    set compile(hulltype) $name
}

# Defines a constructor.
proc ::snit::Type.Constructor {type arglist body} {
    variable compile

    CheckArgs "constructor" $arglist

    # Next, add a magic reference to self.
    set arglist [concat type selfns win self $arglist]

    # Next, add variable declarations to body:
    set body "%TVARDECS%%IVARDECS%\n$body"

    append compile(defs) "proc %TYPE%::Snit_constructor [list $arglist] [list $body]\n"
} 

# Defines a destructor.
proc ::snit::Type.Destructor {type body} {
    variable compile

    # Next, add variable declarations to body:
    set body "%TVARDECS%%IVARDECS%\n$body"

    append compile(defs) "proc %TYPE%::Snit_destructor {type selfns win self} [list $body]"
} 

# Defines a type option.  The option value can be a triple, specifying
# the option's -name, resource name, and class name. 
proc ::snit::Type.Option {type optionDef {defvalue ""}} {
    variable compile

    # First, get the three option names.
    set option [lindex $optionDef 0]
    set resourceName [lindex $optionDef 1]
    set className [lindex $optionDef 2]

    # Next, validate the option name.
    if {![string match {-*} $option] || [string match {*[A-Z ]*} $option]} {
        error "badly named option '$option'"
    }

    if {[Contains $option $compile(delegatedoptions)]} {
        error "cannot delegate '$option'; it has been defined locally."
    }

    if {[Contains $option $compile(localoptions)]} {
        error "option '$option' is multiply defined."
    }

    # Next, compute the resource and class names, if they aren't
    # already defined.

    if {"" == $resourceName} {
        set resourceName [string range $option 1 end]
    }

    if {"" == $className} {
        set className [Capitalize $resourceName]
    }

    lappend compile(localoptions) $option
    Mappend compile(defs) {

        # Option $option
        lappend %TYPE%::Snit_info(options) %OPTION%

        set %TYPE%::Snit_optiondefaults(%OPTION%) %DEFVALUE%
        set %TYPE%::Snit_optiondbspec(%OPTION%) [list %RES% %CLASS%]

        proc %TYPE%::Snit_configure%OPTION% {type selfns win self value} {
            %TVARDECS%
            %IVARDECS%
            set options(%OPTION%) $value
        }

        proc %TYPE%::Snit_cget%OPTION% {type selfns win self} {
            %TVARDECS%
            %IVARDECS%
            return $options(%OPTION%)
        }
    } %OPTION% $option %DEFVALUE% [list $defvalue] \
        %RES% $resourceName %CLASS% $className
}

# Defines an option's cget handler
proc ::snit::Type.Oncget {type option body} {
    variable compile

    if {[lsearch $compile(delegatedoptions) $option] != -1} {
        error "oncget $option: option '$option' is delegated."
    }

    if {[lsearch $compile(localoptions) $option] == -1} {
        error "oncget $option: option '$option' unknown."
    }

    # Next, add variable declarations to body:
    set body "%TVARDECS%%IVARDECS%\n$body"

    append compile(defs) "

        proc [list %TYPE%::Snit_cget$option] {type selfns win self} [list $body]
    "
} 

# Defines an option's configure handler.
proc ::snit::Type.Onconfigure {type option arglist body} {
    variable compile

    if {[lsearch $compile(delegatedoptions) $option] != -1} {
        error "onconfigure $option: option '$option' is delegated."
    }

    if {[lsearch $compile(localoptions) $option] == -1} {
        error "onconfigure $option: option '$option' unknown."
    }

    if {[llength $arglist] != 1} {
        error \
       "onconfigure $option handler should have one argument, got '$arglist'."
    }

    CheckArgs "onconfigure $option" $arglist

    # Next, add a magic reference to self and to options
    set arglist [concat type selfns win self $arglist]

    # Next, add variable declarations to body:
    set body "%TVARDECS%%IVARDECS%\n$body"

    append compile(defs) "

        proc [list %TYPE%::Snit_configure$option $arglist $body]
    "
} 

# Defines an instance method.
proc ::snit::Type.Method {type method arglist body} {
    variable compile

    if {[Contains $method $compile(delegatedmethods)]} {
        error "cannot delegate '$method'; it has been defined locally."
    }

    lappend compile(localmethods) $method

    CheckArgs "method $method" $arglist

    # Next, add magic references to type and self.
    set arglist [concat type selfns win self $arglist]

    # Next, add variable declarations to body:
    set body "%TVARDECS%%IVARDECS%\n$body"

    # Next, save the definition script.
    Mappend compile(defs) {

        # Method %METHOD%
        set  %TYPE%::Snit_methods(%METHOD%) ""
        proc %TYPE%::Snit_method%METHOD% %ARGLIST% %BODY% 
    } %METHOD% $method %ARGLIST% [list $arglist] %BODY% [list $body] 
} 

# Defines a typemethod method.
proc ::snit::Type.Typemethod {type method arglist body} {
    variable compile

    CheckArgs "typemethod $method" $arglist

    # First, add magic reference to type.
    set arglist [concat type $arglist]

    # Next, add typevariable declarations to body:
    set body "%TVARDECS%\n$body"

    Mappend compile(defs) {

        # Typemethod %METHOD%
        set  %TYPE%::Snit_typemethods(%METHOD%) Snit_typemethod%METHOD%
        proc %TYPE%::Snit_typemethod%METHOD% %ARGLIST% %BODY%
    } %METHOD% $method %ARGLIST% [list $arglist] %BODY% [list $body]
} 

# Defines a typemethod method.
proc ::snit::Type.Typeconstructor {type body} {
    variable compile

    if {"" != $compile(typeconstructor)} {
        error "too many typeconstructors"
    }

    set compile(typeconstructor) $body
} 

# Defines a static proc in the type's namespace.
proc ::snit::Type.Proc {type proc arglist body} {
    variable compile

    # If "ns" is defined, the proc can see instance variables.
    if {[lsearch -exact $arglist selfns] != -1} {
        # Next, add instance variable declarations to body:
        set body "%IVARDECS%\n$body"
    }

    # The proc can always see typevariables.
    set body "%TVARDECS%\n$body"

    append compile(defs) "

        # Proc $proc
        proc [list %TYPE%::$proc $arglist $body]
    "
} 

# Defines a static variable in the type's namespace.
proc ::snit::Type.Typevariable {type name args} {
    variable compile

    if {[llength $args] > 1} {
        error "typevariable '$name' has too many initializers"
    }

    if {[llength $args] == 1} {
        append compile(typevars) \
		"\n\t    [list ::variable $name [lindex $args 0]]"
    } else {
        append compile(typevars) \
		"\n\t    [list ::variable $name]"
    }

    append compile(tvprocdec) "\n\t    typevariable ${name}"
} 

# Defines an instance variable; the definition will go in the
# type's create typemethod.
proc ::snit::Type.Variable {type name args} {
    variable compile
    
    if {[llength $args] > 1} {
        error "variable '$name' has too many initializers"
    }

    if {[llength $args] == 1} {
        append compile(instancevars) \
		"\n\t    [list set $name [lindex $args 0]]"
    }

    append  compile(ivprocdec) "\n\t    "
    Mappend compile(ivprocdec) {::variable ${selfns}::%N} %N $name 
} 

# Creates a delegated method or option, delegating it to a particular
# component and, optionally, to a particular option or method of that
# component.
#
# type          The type name
# which         method | option
# name          The name of the method or option, or * for all unknown
#               methods or options
# "to"          sugar; must be "to"
# component     The logical name of the delegate
# "as"          sugar; if not "", must be "as" or "except"
# thing         The name of the delegate's option, or the delegate's method,
#               possibly with arguments, or a list of excepted methods.
# args          Must be {}; it's here to let Type.Delegate do better error
#               handling.

proc ::snit::Type.Delegate {
    type which name "to" component {"as" ""} {thing ""} args
} {
    variable compile

    set target ""
    set exceptions ""

    # FIRST, check syntax.
    set errFlag 0

    if {![string equal $to "to"] || 
        [llength $args] != 0} {
        # Basic syntax
        set errFlag 1
    }

    if {![string equal $thing ""]} {
        # If there's a "thing", then the "as" argument indicates what kind
        # of thing it is.

        if {[string equal $as "as"] && ![string equal $name "*"]} {
            set target $thing
        } elseif {[string equal $as "except"] && [string equal $name "*"]} {
            set exceptions $thing
        } else {
            set errFlag 1
        }
    } elseif {![string equal $as ""]} {
        # If there's no "thing" then $as had better be empty.

        set errFlag 1
    }

    if {$errFlag} {
        error "syntax error in definition: delegate $which $name..."
    }

    # NEXT, dispatch to method or option handler.
    switch $which {
        method {
            DelegatedMethod $type $name $component $target $exceptions
        }
        option {
            DelegatedOption $type $name $component $target $exceptions
        }
        default {
            error "syntax error in definition: delegate $which $name..."
        }
    }

    # NEXT, define the component
    DefineComponent $type $component
}

# Defines a name to be a component
# 
# The name becomes an instance variable; in addition, it gets a 
# write trace so that when it is set, all of the component mechanisms
# get updated.
#
# NOTE: This currently only gets done for components to which something
# is delegated.  Vanilla components which are just saved to an instance
# variable get none of this special handling.  But since this handling
# is to support the delegate statements, that's OK.
#
# type          The type name
# component     The component name

proc ::snit::DefineComponent {type component} {
    variable compile

    if {[lsearch $compile(components) $component] == -1} {
        # Remember we've done this.
        lappend compile(components) $component

        # Make it an instance variable with no initial value
        Type.Variable $type $component ""

        # Add a write trace to do the component thing.
        Mappend compile(instancevars) {
            trace add variable %COMP% write \
                [list %TYPE%::Snit_comptrace $selfns %COMP%]
        } %TYPE% $type %COMP% $component
    }
} 

# Creates a delegated method delegating it to a particular
# component and, optionally, to a particular method of that
# component.
#
# type          The type name
# method        The name of the method
# component     The logical name of the delegate
# target        The name of the delegate's method, possibly with arguments,
#               or "".
# exceptions    When method is "*", this can be a list of methods not to 
#               delegate, or {}; otherwise, it's guaranteed to be {}.

proc ::snit::DelegatedMethod {type method component target exceptions} {
    variable compile

    if {![string equal $method "*"] &&
        [string equal $target ""]} {
        set target $method
    }

    if {[Contains $method $compile(localmethods)]} {
        error "cannot delegate '$method'; it has been defined locally."
    }

    Mappend compile(defs) {
        # Delegated method %METH% to %COMP%
        set %TYPE%::Snit_methods(%METH%) [concat %COMP% %TARGET%]
    } %METH% $method %COMP% $component %TARGET% $target

    if {![string equal $method "*"]} {
        lappend compile(delegatedmethods) $method
    } else {
        Mappend compile(defs) {
            set %TYPE%::Snit_info(exceptmethods) %EXCEPT%
        } %EXCEPT% [list $exceptions]
    }
} 

# Creates a delegated option, delegating it to a particular
# component and, optionally, to a particular option of that
# component.
#
# type          The type name
# option        The name of the option
# component     The logical name of the delegate
# target        The name of the delegate's option, or "".
# exceptions    When option is "*", this can be a list of options not to 
#               delegate, or {}

proc ::snit::DelegatedOption {type optionDef component target exceptions} {
    variable compile

    # First, get the three option names.
    set option [lindex $optionDef 0]
    set resourceName [lindex $optionDef 1]
    set className [lindex $optionDef 2]

    # Next, validate the option name

    if {"*" != $option} {
        if {![string match {-*} $option] || 
            [string match {*[A-Z ]*} $option]} {
            error "badly named option '$option'"
        }
    }

    if {[Contains $option $compile(localoptions)]} {
        error "cannot delegate '$option'; it has been defined locally."
    }

    if {[Contains $option $compile(delegatedoptions)]} {
        error "option '$option' is multiply defined."
    }

    # Next, define the target option, if not specified.
    if {![string equal $option "*"] &&
        [string equal $target ""]} {
        set target $option
    }

    Mappend compile(defs) {
        # Delegated option %OPT% to %COMP% as %TARGET%
        set %TYPE%::Snit_delegatedoptions(%OPT%) [list %COMP% %TARGET%]
        lappend %TYPE%::Snit_compoptions(%COMP%) %OPT%
    } %OPT% $option %COMP% $component %TARGET% $target


    if {![string equal $option "*"]} {
        lappend compile(delegatedoptions) $option

        # Next, compute the resource and class names, if they aren't
        # already defined.

        if {"" == $resourceName} {
            set resourceName [string range $option 1 end]
        }

        if {"" == $className} {
            set className [Capitalize $resourceName]
        }

        Mappend  compile(defs) {
            set %TYPE%::Snit_optiondbspec(%OPTION%) [list %RES% %CLASS%]
        } %OPTION% $option %RES% $resourceName %CLASS% $className
    } else {
        Mappend  compile(defs) {
            set %TYPE%::Snit_info(exceptopts) %EXCEPT%
        } %EXCEPT% [list $exceptions]
    }
} 

# Exposes a component, effectively making the component's command an
# instance method.
#
# type          The type name
# component     The logical name of the delegate
# "as"          sugar; if not "", must be "as"
# methodname    The desired method name for the component's command, or ""

proc ::snit::Type.Expose {type component {"as" ""} {methodname ""}} {
    variable compile


    # FIRST, define the component
    DefineComponent $type $component

    # NEXT, define the method just as though it were in the type
    # definition.
    if {[string equal $methodname ""]} {
        set methodname $component
    }

    Type.Method $type $methodname args [Expand {
        if {[llength $args] == 0} {
            return $%COMPONENT%
        }

        if {[string equal $%COMPONENT% ""]} {
            error "undefined component '%COMPONENT%'"
        }


        set cmd [linsert $args 0 $%COMPONENT%]
        return [uplevel 1 $cmd]
    } %COMPONENT% $component]
}



#-----------------------------------------------------------------------
# Public commands

proc ::snit::type {type body} {
    return [Define type $type $body]
}

proc ::snit::widgetadaptor {type body} {
    return [Define widgetadaptor $type $body]
}

proc ::snit::widget {type body} {
    return [Define widget $type $body]
}

proc ::snit::typemethod {type method arglist body} {
    # Make sure the type exists.
    if {![info exists ${type}::Snit_info]} {
        error "no such type: '$type'"
    }

    upvar ${type}::Snit_info Snit_info
    upvar ${type}::Snit_typemethods Snit_typemethods

    CheckArgs "snit::typemethod $type $method" $arglist

    # First, add magic reference to type.
    set arglist [concat type $arglist]

    # Next, add typevariable declarations to body:
    set body "$Snit_info(tvardecs)\n$body"

    # Next, define it.
    set Snit_typemethods($method) Snit_typemethod$method
    uplevel [list proc ${type}::Snit_typemethod$method $arglist $body]
}

proc ::snit::method {type method arglist body} {
    # Make sure the type exists.
    if {![info exists ${type}::Snit_info]} {
        error "no such type: '$type'"
    }

    upvar ${type}::Snit_methods Snit_methods
    upvar ${type}::Snit_info Snit_info

    if {![info exists Snit_info]} {
        error "no such type: '$type'"
    }

    # FIRST, can't redefine delegated methods.
    if {[info exists Snit_methods($method)] &&
        $Snit_methods($method) ne ""} {
        error "Cannot define '$method', it has been delegated."
    }

    # NEXT, check the arguments
    CheckArgs "snit::method $type $method" $arglist

    # Next, add magic references to type and self.
    set arglist [concat type selfns win self $arglist]

    # Next, add variable declarations to body:
    set body "$Snit_info(tvardecs)$Snit_info(ivardecs)\n$body"

    # Next, define it.
    set Snit_methods($method) ""
    uplevel [list proc ${type}::Snit_method$method $arglist $body]
}


#-----------------------------------------------------------------------
# Definition commands

proc ::snit::Define {which type body} {
    variable typeTemplate
    variable defs
    variable compile

    # FIRST, qualify the name.
    if {![string match "::*" $type]} {
        # Get caller's namespace; 
        # append :: if not global namespace.
        set ns [uplevel 2 namespace current]
        if {"::" != $ns} {
            append ns "::"
        }
        
        set type "$ns$type"
    }

    # NEXT, create the class interpreter
    if {![string length [info command class.interp]]} {
        interp create class.interp
	class.interp eval {catch {package require snit::__does_not_exist__}}
    }

    class.interp alias widgetclass     ::snit::Type.Widgetclass     $type
    class.interp alias hulltype        ::snit::Type.Hulltype        $type
    class.interp alias constructor     ::snit::Type.Constructor     $type
    class.interp alias destructor      ::snit::Type.Destructor      $type
    class.interp alias option          ::snit::Type.Option          $type
    class.interp alias onconfigure     ::snit::Type.Onconfigure     $type
    class.interp alias oncget          ::snit::Type.Oncget          $type
    class.interp alias typemethod      ::snit::Type.Typemethod      $type
    class.interp alias typeconstructor ::snit::Type.Typeconstructor $type
    class.interp alias method          ::snit::Type.Method          $type
    class.interp alias proc            ::snit::Type.Proc            $type
    class.interp alias typevariable    ::snit::Type.Typevariable    $type
    class.interp alias variable        ::snit::Type.Variable        $type
    class.interp alias delegate        ::snit::Type.Delegate        $type
    class.interp alias expose          ::snit::Type.Expose          $type

    # NEXT, initialize the class data
    set compile(defs) {}
    set compile(which) $which
    set compile(localoptions) {}
    set compile(instancevars) {}
    set compile(typevars) {}
    set compile(delegatedoptions) {}
    set compile(ivprocdec) {}
    set compile(tvprocdec) {}
    set compile(typeconstructor) {}
    set compile(widgetclass) {}
    set compile(hulltype) {}
    set compile(localmethods) {}
    set compile(delegatedmethods) {}
    set compile(components) {}

    append compile(defs) \
	    "set %TYPE%::Snit_isWidget        [string match widget* $which]\n"
    append compile(defs) \
	    "\tset %TYPE%::Snit_isWidgetAdaptor [string match widgetadaptor $which]"

    if {"widgetadaptor" == $which} {
        # A widgetadaptor is also a widget.
        set which widget
    }

    # NEXT, Add the standard definitions; then 
    # evaluate the type's definition in the class interpreter.
    class.interp eval [Expand $defs(common) %TYPE% $type]
    class.interp eval [Expand $defs($which) %TYPE% $type]
    class.interp eval $body

    # NEXT, if this is a widget define the hull component if it isn't
    # already defined.
    if {"widget" == $which} {
        DefineComponent $type hull
    }

    # NEXT, substitute the compiled definition into the type template
    # to get the type definition script.
    set defscript [Expand $typeTemplate \
                       %COMPILEDDEFS% $compile(defs)]

    # NEXT, substitute the defined macros into the type definition script.
    # This is done as a separate step so that the compile(defs) can 
    # contain the macros defined below.

    set defscript [Expand $defscript \
                       %TYPE%         $type \
                       %IVARDECS%     $compile(ivprocdec) \
                       %TVARDECS%     $compile(tvprocdec) \
                       %TCONSTBODY%   $compile(typeconstructor) \
                       %INSTANCEVARS% $compile(instancevars) \
                       %TYPEVARS%     $compile(typevars) \
		       ]

    array unset compile

    return [DefineDo $which $type $defscript]
}

proc ::snit::DefineDo {which type body} {
    # Do something with the collected and generated snit
    # definition. The regular action is to execute the type definition
    # script, IOW to instantiate the type in the interpreter, and to
    # return the name of the new type (== namespace it is in).

    # NEXT, execute the type definition script.

    #puts "eval $body\n"
    if {[catch {eval $body} result]} {
        namespace delete $type
        catch {rename $type ""}
        error $result
    }

    return $type
}



#-----------------------------------------------------------------------
# Instance introspection commands

# Returns the instance's type.
proc ::snit::InstanceInfo_type {type selfns win self} {
    return $type
}

# Returns the instance's type's typevariables
proc ::snit::InstanceInfo_typevars {type selfns win self {pattern *}} {
    return [TypeInfo_typevars $type $pattern]
}

# Returns the instance's instance variables
proc ::snit::InstanceInfo_vars {type selfns win self {pattern *}} {
    set result {}
    foreach name [info vars "${selfns}::$pattern"] {
        set tail [namespace tail $name]
        if {![string match "Snit_*" $tail]} {
            lappend result $name
        }
    }

    return $result
}

# Returns a list of the names of the instance's options
proc ::snit::InstanceInfo_options {type selfns win self {pattern *}} {
    upvar ${type}::Snit_optiondefaults   Snit_optiondefaults
    upvar ${type}::Snit_delegatedoptions Snit_delegatedoptions
    upvar ${type}::Snit_info             Snit_info

    set result {}

    # First, get the local options
    foreach name [array names Snit_optiondefaults] {
        lappend result $name
    }

    # Next, get the delegated options.  Check and see if unknown 
    # options are delegated.
    set gotStar 0
    foreach name [array names Snit_delegatedoptions] {
        if {"*" == $name} {
            set gotStar 1
        } else {
            lappend result $name
        }
    }

    # If "configure" works as for Tk widgets, add the resulting
    # options to the list.  Skip excepted options
    if {$gotStar} {
        upvar ${selfns}::Snit_components Snit_components
        set logicalName [lindex $Snit_delegatedoptions(*) 0]
        set comp $Snit_components($logicalName)

        if {![catch {$comp configure} records]} {
            foreach record $records {
                set opt [lindex $record 0]
                if {[lsearch -exact $result $opt] == -1 &&
                    [lsearch -exact $Snit_info(exceptopts) $opt] == -1} {
                    lappend result $opt
                }
            }
        }
    }

    # Next, apply the pattern
    set names {}

    foreach name $result {
        if {[string match $pattern $name]} {
            lappend names $name
        }
    }

    return $names
}

#-----------------------------------------------------------------------
# Type introspection commands

# Returns the instance's type's typevariables
proc ::snit::TypeInfo_typevars {type {pattern *}} {
    set result {}
    foreach name [info vars "${type}::$pattern"] {
        set tail [namespace tail $name]
        if {![string match "Snit_*" $tail]} {
            lappend result $name
        }
    }
    
    return $result
}

# Returns the instance's instance variables
proc ::snit::TypeInfo_instances {type {pattern *}} {
    upvar ${type}::Snit_isWidget Snit_isWidget
    set result {}

    foreach selfns [namespace children $type] {
        upvar ${selfns}::Snit_instance instance

        if {[string match $pattern $instance]} {
            lappend result $instance
        }
    }

    return $result
}


#-----------------------------------------------------------------------
# Utility Functions

# Builds a template from a tagged list of text blocks, then substitutes
# all symbols in the mapTable, returning the expanded template.
proc ::snit::Expand {template args} {
    return [string map $args $template]
}

# Expands a template and appends it to a variable.
proc ::snit::Mappend {varname template args} {
    upvar $varname myvar

    append myvar [string map $args $template]
}

# Return a unique command name.  
#
# Require: type is a fully qualified name.
# Require: name contains "%AUTO%"
proc ::snit::UniqueName {countervar type name} {
    upvar $countervar counter 
    while 1 {
        # FIRST, bump the counter and define the %AUTO% instance name;
        # then substitute it into the specified name.  Wrap around at
        # 2^31 - 1 to prevent overflow problems.
        incr counter
        if {$counter > 2147483647} {
            set counter 0
        }
        set auto "[namespace tail $type]$counter"
        set candidate [snit::Expand $name %AUTO% $auto]
        if {[info commands $candidate] == ""} {
            return $candidate
        }
    }
}

# Return a unique instance namespace
proc ::snit::UniqueInstanceNamespace {countervar type} {
    upvar $countervar counter 
    while 1 {
        # FIRST, bump the counter and define the namespace name.
        # Then see if it already exists.
        incr counter
        set ins "${type}::Snit_inst${counter}"
        if {![namespace exists $ins]} {
            return $ins
        }
    }
}

# Checks argument list against reserved args 
proc ::snit::CheckArgs {which arglist} {
    variable reservedArgs
    
    foreach name $reservedArgs {
        if {[Contains $name $arglist]} {
            error "$which's arglist may not contain '$name' explicitly."
        }
    }
}

# Returns 1 if a value is in a list, and 0 otherwise.
proc ::snit::Contains {value list} {
    if {[lsearch -exact $list $value] != -1} {
        return 1
    } else {
        return 0
    }
}

# Capitalizes the first letter of a string.
proc ::snit::Capitalize {text} {
    set first [string index $text 0]
    set rest [string range $text 1 end]
    return "[string toupper $first]$rest"
}

proc ::snit::CallInstance {selfns args} {
    upvar ${selfns}::Snit_instance self
    return [uplevel 1 [linsert $args 0 $self]]
}


