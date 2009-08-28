#!/bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}

# --------------------------------------------------------------
# Perform various checks and operations on the distribution.
# SAK = Swiss Army Knife.

set distribution   [file dirname [info script]]
lappend auto_path  [file join $distribution modules]

source [file join $distribution tklib_version.tcl] ; # Get version information.

catch {eval file delete -force [glob [file rootname [info script]].tmp.*]}

# Transduction old/new names (release uses new information).
set package_version $tklib_version
set package_name    $tklib_name

# --------------------------------------------------------------
# SAK internal debugging support.

# Configuration, change as needed
set  debug 0

if {$debug} {
    proc sakdebug {script} {uplevel 1 $script ; return}
} else {
    proc sakdebug {args} {}
}

# --------------------------------------------------------------
# Internal helper to load packages straight out of the local directory
# tree. Not something from an installation, possibly incompatible.

proc getpackage {package} {
    package require $package
}

# --------------------------------------------------------------

proc tclfiles {} {
    global distribution
    package require fileutil
    set fl [fileutil::findByPattern $distribution -glob *.tcl]
    proc tclfiles {} [list return $fl]
    return $fl
}

proc modtclfiles {modules} {
    global mfiles guide
    load_modinfo
    set mfiles [list]
    foreach m $modules {
	eval $guide($m,pkg) $m __dummy__
    }
    return $mfiles
}


proc modules {} {
    global distribution
    set fl [list]
    foreach f [glob -nocomplain [file join $distribution modules *]] {
	if {![file isdirectory $f]} {continue}
	if {[string match CVS [file tail $f]]} {continue}

	if {![file exists [file join $f pkgIndex.tcl]]} {continue}

	lappend fl [file tail $f]
    }
    set fl [lsort $fl]
    proc modules {} [list return $fl]
    return $fl
}

proc modules_mod {m} {
    return [expr {[lsearch -exact [modules] $m] >= 0}]
}

proc load_modinfo {} {
    global distribution modules guide
    source [file join $distribution installed_modules.tcl] ; # Get list of installed modules.
    source [file join $distribution install_action.tcl] ; # Get list of installed modules.
    proc load_modinfo {} {}
    return
}

proc imodules {} {global modules ; load_modinfo ; return $modules}

proc imodules_mod {m} {
    global modules
    load_modinfo
    return [expr {[lsearch -exact $modules $m] > 0}]
}


proc loadpkglist {fname} {
    set f [open $fname r]
    foreach line [split [read $f] \n] {
	foreach {n v} $line break
	set p($n) $v
    }
    close $f
    return [array get p]
}

proc ipackages {args} {
    # Determine indexed packages (ifneeded, pkgIndex.tcl)

    global distribution

    if {[llength $args] == 0} {set args [modules]}

    array set p {}
    foreach m $args {
	set f [open [file join $distribution modules $m pkgIndex.tcl] r]
	foreach line [split [read $f] \n] {
	    if { [regexp {#}        $line]} {continue}
	    if {![regexp {ifneeded} $line]} {continue}
	    regsub {^.*ifneeded } $line {} line
	    regsub {([0-9]) \[.*$}  $line {\1} line
	    regsub {([0-9]) \{.*$}  $line {\1} line

	    foreach {n v} $line break
	    set p($n) $v
	}
	close $f
    }
    return [array get p]
}

# Result: dict (package name --> list of package versions).

proc ppackages {args} {
    # Determine provided packages (provide, *.tcl - pkgIndex.tcl)
    # We cache results for a bit of speed, some stuff uses this
    # multiple times for the same arguments.

    global ppcache
    if {[info exists ppcache($args)]} {
	return $ppcache($args)
    }

    global    p pf currentfile
    array set p {}

    if {[llength $args] == 0} {
	set files [tclfiles]
    } else {
	set files [modtclfiles $args]
    }

    getpackage fileutil
    set capout [fileutil::tempfile] ; set capcout [open $capout w]
    set caperr [fileutil::tempfile] ; set capcerr [open $caperr w]

    foreach f $files {
	# We ignore package indices and all files not in a module.

	if {[string equal pkgIndex.tcl [file tail $f]]} {continue}
	if {![regexp modules $f]}                       {continue}

	# We use two methods to extract the version information from a
	# module and its packages. First we do a static scan for
	# appropriate statements. If that did not work out we try to
	# execute the script in a modified interpreter which lets us
	# pick up dynamically generated version data (like stored in
	# variables). If the second method fails as well we give up.

	# Method I. Static scan.

	# We do heuristic scanning of the code to locate suitable
	# package provide statements.

	set fh [open $f r]

	set currentfile [eval file join [lrange [file split $f] end-1 end]]

	set ok -1
	foreach line [split [read $fh] \n] {
	    regsub "\#.*$" $line {} line
	    if {![regexp {provide} $line]} {continue}
	    if {![regexp {package} $line]} {continue}

	    set xline $line
	    regsub {^.*provide } $line {} line
	    regsub {\].*$}       $line {\1} line

	    sakdebug {puts stderr __$f\ _________$line}

	    foreach {n v} $line break

	    # HACK ...
	    # Module 'page', package 'page::gen::peg::cpkg'.
	    # Has a provide statement inside a template codeblock.
	    # Name is placeholder @@. Ignore this specific name.
	    # Better would be to use general static Tcl parsing
	    # to find that the string is a variable value.

	    if {[string equal $n @@]} continue

	    if {[regexp {^[0-9]+(\.[0-9]+)*$} $v]} {
		lappend p($n) $v
		set p($n) [lsort -uniq -dict $p($n)]
		set pf($n,$v) $currentfile
		set ok 1

		# We continue the scan. The file may provide several
		# versions of the same package, or multiple packages.
		continue
	    }

	    # 'package provide foo' are tests. Ignore.
	    if {$v == ""} continue

	    # We do not set the state to bad if we found ok provide
	    # statements before, only if nothing was found before.
	    if {$ok < 0} {
		set ok 0

		# No good version found on the current line. We scan
		# further through the file and hope for more luck.

		sakdebug {puts stderr @_$f\ _________$xline\t<$n>\t($v)}
	    }
	}
	close $fh

	# Method II. Restricted Execution.
	# We now try to run the code through a safe interpreter
	# and hope for better luck regarding package information.

	if {$ok == -1} {sakdebug {puts stderr $f\ IGNORE}}
	if {$ok == 0} {
	    sakdebug {puts -nonewline stderr $f\ EVAL}

	    # Source the code into a sub-interpreter. The sub
	    # interpreter overloads 'package provide' so that the
	    # information about new packages goes directly to us. We
	    # also make sure that the sub interpreter doesn't kill us,
	    # and will not get stuck early by trying to load other
	    # files, or when creating procedures in namespaces which
	    # do not exist due to us disabling most of the package
	    # management.

	    set fh [open $f r]

	    set ip [interp create]

	    # Kill control structures. Namespace is required, but we
	    # skip everything related to loading of packages,
	    # i.e. 'command import'.

	    $ip eval {
		rename ::if        ::_if_
		rename ::namespace ::_namespace_

		proc ::if {args} {}
		proc ::namespace {cmd args} {
		    #puts stderr "_nscmd_ $cmd"
		    ::_if_ {[string equal $cmd import]} return
		    #puts stderr "_nsdo_ $cmd $args"
		    return [uplevel 1 [linsert $args 0 ::_namespace_ $cmd]]
		}
	    }

	    # Kill more package stuff, and ensure that unknown
	    # commands are neither loaded nor abort execution. We also
	    # stop anything trying to kill the application at large.

	    interp alias $ip package {} xPackage
	    interp alias $ip source  {} xNULL
	    interp alias $ip unknown {} xNULL
	    interp alias $ip proc    {} xNULL
	    interp alias $ip exit    {} xNULL

	    # From here on no redefinitions anymore, proc == xNULL !!

	    $ip eval {close stdout} ; interp share {} $capcout $ip
	    $ip eval {close stderr} ; interp share {} $capcerr $ip

	    if {[catch {$ip eval [read $fh]} msg]} {
		sakdebug {puts stderr "ERROR in $currentfile:\n$::errorInfo\n"}
	    }

	    sakdebug {puts stderr ""}

	    close $fh
	    interp delete $ip
	}
    }

    close $capcout ; file delete $capout
    close $capcerr ; file delete $caperr

    set   pp [array get p]
    unset p

    set ppcache($args) $pp
    return $pp 
}

proc xNULL    {args} {}
proc xPackage {cmd args} {

    if {[string equal $cmd provide]} {
	global p pf currentfile
	foreach {n v} $args break

	# No version specified, this is an inquiry, we ignore these.
	if {$v == {}} {return}

	set p($n) $v
	set pf($n) $currentfile
    }
    return
}



proc sep {} {puts ~~~~~~~~~~~~~~~~~~~~~~~~}

proc gendoc {fmt ext args} {
    global distribution
    global tcl_platform

    set null 0
    if {![string compare $fmt null]} {set null 1}
    if {[llength $args] == 0} {set args [modules]}

    # Direct usage of the doctools instead of the mini application 'mpexpand' ...

    package require doctools
    ::doctools::new mpe -format $fmt

    if {!$null} {
	file mkdir [file join doc $fmt]
    } else {
	mpe configure -deprecated 1
    }

    foreach m $args {
	set fl [glob -nocomplain [file join $distribution modules $m *.man]]
	if {[llength $fl] == 0} {continue}

	mpe configure -module $m

	foreach f $fl {
	    if {!$null} {
                set target [file join doc $fmt \
                                [file rootname [file tail $f]].$ext]
                if {[file exists $target] 
                    && [file mtime $target] > [file mtime $f]} {
                    continue
                }
	    }
	    puts "Gen ($fmt): $f"

	    if {[catch {
		set result [mpe format [read [set if [open $f r]]][close $if]]
	    } msg]} {
		puts stdout $msg
	    }

	    set warnings [mpe warnings]
	    if {[llength $warnings] > 0} {
		puts stdout [join $warnings \n]
	    }

	    if {!$null} {
		set of [open $target w]
		puts -nonewline $of $result
		close $of
	    }
	}
    }

    mpe destroy
    return
}

proc gd-cleanup {} {
    global tklib_version

    puts {Cleaning up...}

    set        fl [glob -nocomplain tklib-${tklib_version}*]
    foreach f $fl {
	puts "    Deleting $f ..."
	catch {file delete -force $f}
    }
    return
}

proc gd-gen-archives {} {
    global tklib_version

    puts {Generating archives...}

    set tar [auto_execok tar]
    if {$tar != {}} {
        puts "    Gzipped tarball (tklib-${tklib_version}.tar.gz)..."
        catch {
            exec $tar cf - tklib-${tklib_version} | gzip --best > tklib-${tklib_version}.tar.gz
        }

        set bzip [auto_execok bzip2]
        if {$bzip != {}} {
            puts "    Bzipped tarball (tklib-${tklib_version}.tar.bz2)..."
            exec tar cf - tklib-${tklib_version} | bzip2 > tklib-${tklib_version}.tar.bz2
        }
    }

    set zip [auto_execok zip]
    if {$zip != {}} {
        puts "    Zip archive     (tklib-${tklib_version}.zip)..."
        catch {
            exec $zip -r   tklib-${tklib_version}.zip             tklib-${tklib_version}
        }
    }

    set sdx [auto_execok sdx]
    if {$sdx != {}} {
	file rename tklib-${tklib_version} tklib.vfs

	puts "    Starkit         (tklib-${tklib_version}.kit)..."
	exec sdx wrap tklib
	file rename   tklib tklib-${tklib_version}.kit

	if {![file exists tclkit]} {
	    puts "    No tclkit present in current working directory, no starpack."
	} else {
	    puts "    Starpack        (tklib-${tklib_version}.exe)..."
	    exec sdx wrap tklib -runtime tclkit
	    file rename   tklib tklib-${tklib_version}.exe
	}

	file rename tklib.vfs tklib-${tklib_version}
    }

    puts {    Keeping directory for other archive types}

    ## Keep the directory for 'sdx' - kit/pack
    return
}

proc xcopyfile {src dest} {
    # dest can be dir or file
    global  mfiles
    lappend mfiles $src
    return
}

proc xcopy {src dest recurse {pattern *}} {
    foreach file [glob [file join $src $pattern]] {
        set base [file tail $file]
	set sub  [file join $dest $base]
	if {0 == [string compare CVS $base]} {continue}
        if {[file isdirectory $file]} then {
	    if {$recurse} {
		xcopy $file $sub $recurse $pattern
	    }
        } else {
            xcopyfile $file $sub
        }
    }
}


proc xxcopy {src dest recurse {pattern *}} {
    file mkdir $dest
    foreach file [glob -nocomplain [file join $src $pattern]] {
        set base [file tail $file]
	set sub  [file join $dest $base]

	# Exclude CVS automatically, and possibly the temp hierarchy
	# itself too.

	if {0 == [string compare CVS $base]} {continue}
	if {[string match tklib-*   $base]} {continue}
	if {[string match *~         $base]} {continue}

        if {[file isdirectory $file]} then {
	    if {$recurse} {
		file mkdir  $sub
		xxcopy $file $sub $recurse $pattern
	    }
        } else {
	    puts -nonewline stdout . ; flush stdout
            file copy -force $file $sub
        }
    }
}

proc gd-assemble {} {
    global tklib_version distribution

    puts "Assembling distribution in directory 'tklib-${tklib_version}'"

    xxcopy $distribution tklib-${tklib_version} 1
    file delete -force \
	    tklib-${tklib_version}/config \
	    tklib-${tklib_version}/modules/ftp/example \
	    tklib-${tklib_version}/modules/ftpd/examples \
	    tklib-${tklib_version}/modules/stats \
	    tklib-${tklib_version}/modules/fileinput
    puts ""
    return
}

proc normalize-version {v} {
    # Strip everything after the first non-version character, and any
    # trailing dots left behind by that, to avoid the insertion of bad
    # version numbers into the generated .tap file.

    regsub {[^0-9.].*$} $v {} v
    return [string trimright $v .]
}

proc gd-gen-tap {} {
    getpackage textutil
    getpackage fileutil

    global package_name package_version distribution tcl_platform

    set pname [textutil::cap $package_name]

    set modules   [imodules]
    array set pd  [getpdesc]
    set     lines [list]
    # Header
    lappend lines {format  {TclDevKit Project File}}
    lappend lines {fmtver  2.0}
    lappend lines {fmttool {TclDevKit TclApp PackageDefinition} 2.5}
    lappend lines {}
    lappend lines "##  Saved at : [clock format [clock seconds]]"
    lappend lines "##  By       : $tcl_platform(user)"
    lappend lines {##}
    lappend lines "##  Generated by \"[file tail [info script]] tap\""
    lappend lines "##  of $package_name $package_version"
    lappend lines {}
    lappend lines {########}
    lappend lines {#####}
    lappend lines {###}
    lappend lines {##}
    lappend lines {#}

    # Bundle definition
    lappend lines {}
    lappend lines {# ###############}
    lappend lines {# Complete bundle}
    lappend lines {}
    lappend lines [list Package [list $package_name [normalize-version $package_version]]]
    lappend lines "Base     @TAP_DIR@"
    lappend lines "Platform *"
    lappend lines "Desc     \{$pname: Bundle of all packages\}"
    lappend lines "Path     pkgIndex.tcl"
    lappend lines "Path     [join $modules "\nPath     "]"

    set  strip [llength [file split $distribution]]
    incr strip 2

    foreach m $modules {
	# File set of module ...

	puts "M $m"
	catch { unset ps }
	array set ps {}

	lappend lines {}
	lappend lines "# #########[::textutil::strRepeat {#} [string length $m]]" ; # {}
	lappend lines "# Module \"$m\""
	set n 0
	foreach {p vlist} [ppackages $m] {
	    puts "  | $p"
	    foreach v $vlist {
		lappend lines "# \[[format %1d [incr n]]\]    | \"$p\" ($v)"
	    }
	    set ps($p) .
	}
	if {$m == "tablelist"} {
	    # Tablelist contains dynamics in the provide statements
	    # the analysis in ppackages is unable to deal with,
	    # causing it to locate only tablelist::common. We now fix
	    # this here by adding the indexed package names as well
	    # and assuming that they are provided.
	    foreach {p vlist} [ipackages $m] {
		if {[info exists ps($p)]} continue
		puts "  / $p"
		lappend lines "# \[[format %1d [incr n]]\]    | \"$p\" ($v)"
	    }
	}

	if {$n > 1} {
	    # Multiple packages (*). We create one hidden package to
	    # contain all the files and then have all the true
	    # packages in the module refer to it.
	    #
	    # (*) This can also be one package for which we have
	    # several versions. Or a combination thereof.

	    catch { unset ps }
	    array set ps {}
	    array set _ {}
	    foreach {p vlist} [ppackages $m] {
		catch {set _([lindex $pd($p) 0]) .}
		set ps($p) .
	    }
	    if {$m == "tablelist"} {
		# S.a. for explantions.
		foreach {p vlist} [ipackages $m] {
		    if {[info exists ps($p)]} continue
		    catch {set _([lindex $pd($p) 0]) .}
		}
	    }

	    set desc [string trim [join [array names _] ", "] " \n\t\r,"]
	    if {$desc == ""} {set desc "$pname module"}
	    unset _

	    lappend lines "# -------+"
	    lappend lines {}
	    lappend lines [list Package [list __$m 0.0]]
	    lappend lines "Platform *"
	    lappend lines "Desc     \{$desc\}"
	    lappend lines Hidden
	    lappend lines "Base     @TAP_DIR@/$m"

	    foreach f [lsort -dict [modtclfiles $m]] {
		lappend lines "Path     [fileutil::stripN $f $strip]"
	    }

	    catch { unset ps }
	    array set ps {}

	    # Packages in the module ...
	    foreach {p vlist} [ppackages $m] {
		set ps($p) .
		# NO DANGER. As we are listing only the packages P for
		# the module any other version of P in a different
		# module is _not_ listed here.

		set desc ""
		catch {set desc [string trim [lindex $pd($p) 1]]}
		if {$desc == ""} {set desc "$pname package"}

		foreach v $vlist {
		    lappend lines {}
		    lappend lines [list Package [list $p [normalize-version $v]]]
		    lappend lines "See   [list __$m]"
		    lappend lines "Platform *"
		    lappend lines "Desc     \{$desc\}"
		}
	    }

	    if {$m == "tablelist"} {
		# S.a. for explantions.
		foreach {p vlist} [ipackages $m] {
		    if {[info exists ps($p)]} continue

		    # factor into procedure ...
		    set desc ""
		    catch {set desc [string trim [lindex $pd($p) 1]]}
		    if {$desc == ""} {set desc "$pname package"}

		    foreach v $vlist {
			lappend lines {}
			lappend lines [list Package [list $p [normalize-version $v]]]
			lappend lines "See   [list __$m]"
			lappend lines "Platform *"
			lappend lines "Desc     \{$desc\}"
		    }
		}
	    }
	} else {
	    # A single package in the module. And only one version of
	    # it as well. Otherwise we are in the multi-pkg branch.

	    foreach {p vlist} {{} {}} break
	    foreach {p vlist} [ppackages $m] break
	    set desc ""
	    catch {set desc [string trim [lindex $pd($p) 1]]}
	    if {$desc == ""} {set desc "$pname package"}

	    set v [lindex $vlist 0]

	    lappend lines "# -------+"
	    lappend lines {}
	    lappend lines [list Package [list $p [normalize-version $v]]]
	    lappend lines "Platform *"
	    lappend lines "Desc     \{$desc\}"
	    lappend lines "Base     @TAP_DIR@/$m"

	    foreach f [lsort -dict [modtclfiles $m]] {
		lappend lines "Path     [fileutil::stripN $f $strip]"
	    }
	}
	lappend lines {}
	lappend lines {#}
	lappend lines "# #########[::textutil::strRepeat {#} [string length $m]]"
    }

    lappend lines {}
    lappend lines {#}
    lappend lines {##}
    lappend lines {###}
    lappend lines {#####}
    lappend lines {########}

    # Write definition
    set    f [open [file join $distribution ${package_name}.tap] w]
    puts  $f [join $lines \n]
    close $f
    return
}

proc getpdesc  {} {
    global argv ; if {![checkmod]} return

    eval gendoc desc l $argv
    
    array set _ {}
    foreach file [glob -nocomplain doc/desc/*.l] {
        set f [open $file r]
	foreach l [split [read $f] \n] {
	    foreach {p sd d} $l break
	    set _($p) [list $sd $d]
	}
        close $f
    }
    file delete -force doc/desc

    return [array get _]
}

proc gd-gen-rpmspec {} {
    global tklib_version tklib_name distribution

    set header [string map [list @@@@ $tklib_version @__@ $tklib_name] {# $Id: sak.tcl,v 1.7 2008/05/01 17:16:01 andreas_kupries Exp $

%define version @@@@
%define directory /usr

Summary: The standard Tk library
Name: @__@
Version: %{version}
Release: 2
Copyright: BSD
Group: Development/Languages
Source: %{name}-%{version}.tar.bz2
URL: http://tcllib.sourceforge.net/
Packager: Jean-Luc Fontaine <jfontain@free.fr>
BuildArchitectures: noarch
Prefix: /usr
Requires: tcl >= 8.3.1
BuildRequires: tcl >= 8.3.1
Buildroot: /var/tmp/%{name}-%{version}

%description
Tklib, the Tk Standard Library is a collection of Tcl packages that
provide Tk utility functions and widgets useful to a large collection
of Tcl/Tk programmers.
The home web site for this code is http://tcllib.sourceforge.net/.
At this web site, you will find mailing lists, web forums, databases
for bug reports and feature requests, the CVS repository (browsable
on the web, or read-only accessible via CVS), and more.
Note: also grab source tarball for more documentation, examples, ...

%prep

%setup -q

%install
# compensate for missing manual files:
# - nothing yet
/usr/bin/tclsh installer.tcl -no-gui -no-wait -no-html -no-examples\
    -pkg-path $RPM_BUILD_ROOT/usr/lib/%{name}-%{version}\
    -nroff-path $RPM_BUILD_ROOT/usr/share/man/mann/
# install HTML documentation to specific modules sub-directories:
# generate list of files in the package (man pages are compressed):
find $RPM_BUILD_ROOT ! -type d |\
    sed -e "s,^$RPM_BUILD_ROOT,,;" -e 's,\.n$,\.n\.gz,;' >\
    %{_builddir}/%{name}-%{version}/files

%clean
rm -rf $RPM_BUILD_ROOT

%files -f %{_builddir}/%{name}-%{version}/files
%defattr(-,root,root)
%doc README ChangeLog license.terms
}]

    set    f [open [file join $distribution tklib.spec] w]
    puts  $f $header
    close $f
    return
}

proc gd-gen-yml {} {
    # YAML is the format used for the FreePAN archive network.
    # http://freepan.org/
    global tklib_version tklib_name distribution
    set yml [string map \
                 [list %V $tklib_version %N $tklib_name] \
                 {dist_id: tklib
version: %V
language: tcl
description: |
   This package is intended to be a collection of Tcl packages that provide
   Tk utility functions and widgets useful to a large collection of Tcl/Tk
   programmers.

   The home web site for this code is http://tcllib.sourceforge.net/.
   At this web site, you will find mailing lists, web forums, databases
   for bug reports and feature requests, the CVS repository (browsable
   on the web, or read-only accessible via CVS), and more.

categories: 
  - Library/Utility
  - Library/UI
  - Library/Graphics
license: BSD
owner_id: AndreasKupries
wrapped_content: %N-%V/
}]
    set f [open [file join $distribution tklib.yml] w]
    puts $f $yml
    close $f
}

proc docfiles {} {
    global distribution
    package require fileutil
    set res [list]
    foreach f [fileutil::findByPattern $distribution -glob *.man] {
	lappend res [file rootname [file tail $f]].n
    }
    proc tclfiles {} [list return $res]
    return $res
}

proc gd-tip55 {} {
    global tklib_version tklib_name distribution contributors
    contributors

    set md {Identifier: %N
Title:  Tk Standard Library
Description: This package is intended to be a collection of
    Tcl packages that provide Tk utility functions and widgets
    useful to a large collection of Tcl/Tk programmers.
Rights: BSD
Version: %V
URL: http://tcllib.sourceforge.net/
Architecture: tcl
}

    regsub {Version: %V} $md "Version: $tklib_version" md
    regsub {Identifier: %N} $md "Identifier: $tklib_name" md
    foreach person [lsort [array names contributors]] {
        set mail $contributors($person)
        regsub {@}  $mail " at " mail
        regsub -all {\.} $mail " dot " mail
        append md "Contributor: $person <$mail>\n"
    }

    set f [open [file join $distribution DESCRIPTION.txt] w]
    puts $f $md
    close $f
}

# Fill the global array of contributors to tklib by processing the
# ChangeLog entries.
#
proc contributors {} {
    global distribution contributors
    if {![info exists contributors] || [array size contributors] == 0} {
        get_contributors [file join $distribution ChangeLog]

        foreach f [glob -nocomplain [file join $distribution modules *]] {
            if {![file isdirectory $f]} {continue}
            if {[string match CVS [file tail $f]]} {continue}
            if {![file exists [file join $f ChangeLog]]} {continue}
            get_contributors [file join $f ChangeLog]
        }
    }
}

proc get_contributors {changelog} {
    global contributors
    set f [open $changelog r]
    while {![eof $f]} {
        gets $f line
        if {[regexp {^[\d-]+\s+(.*?)<(.*?)>} $line r name mail]} {
            set name [string trim $name]
            if {![info exists names($name)]} {
                set contributors($name) $mail
            }
        }
    }
    close $f
}

proc validate_imodules_cmp {imvar dmvar} {
    upvar $imvar im $dmvar dm

    foreach m [lsort [array names im]] {
	if {![info exists dm($m)]} {
	    puts "  Installed, does not exist: $m"
	}
    }
    foreach m [lsort [array names dm]] {
	if {![info exists im($m)]} {
	    puts "  Missing in installer:      $m"
	}
    }
    return
}

proc validate_imodules {} {
    foreach m [imodules] {set im($m) .}
    foreach m [modules]  {set dm($m) .}

    validate_imodules_cmp im dm
    return
}

proc validate_imodules_mod {m} {
    array set im {}
    array set dm {}
    if {[imodules_mod $m]} {set im($m) .}
    if {[modules_mod  $m]} {set dm($m) .}

    validate_imodules_cmp im dm
    return
}
proc validate_versions_cmp {ipvar ppvar} {
    upvar $ipvar ip $ppvar pp
    set maxl 0
    foreach name [array names ip] {if {[string length $name] > $maxl} {set maxl [string length $name]}}
    foreach name [array names pp] {if {[string length $name] > $maxl} {set maxl [string length $name]}}

    foreach p [lsort [array names ip]] {
	if {![info exists pp($p)]} {
	    puts "  Indexed, no provider:           $p"
	}
    }
    foreach p [lsort [array names pp]] {
	if {![info exists ip($p)]} {
	    puts "  Provided, not indexed:          [format "%-*s | %s" $maxl $p $pp($p)]"
	}
    }
    foreach p [lsort [array names ip]] {
	if {
	    [info exists pp($p)] && ![string equal $pp($p) $ip($p)]
	} {
	    puts "  Index/provided versions differ: [format "%-*s | %8s | %8s" $maxl $p $ip($p) $pp($p)]"
	}
    }
}

proc validate_versions {} {
    foreach {p v} [ipackages] {set ip($p) $v}
    foreach {p v} [ppackages] {set pp($p) $v}

    validate_versions_cmp ip pp
    return
}

proc validate_versions_mod {m} {
    foreach {p v} [ipackages $m] {set ip($p) $v}
    foreach {p v} [ppackages $m] {set pp($p) $v}

    validate_versions_cmp ip pp
    return
}

proc validate_testsuite_mod {m} {
    global distribution
    if {[llength [glob -nocomplain [file join $distribution modules $m *.test]]] == 0} {
	puts "  Without testsuite : $m"
    }
    return
}

proc validate_testsuites {} {
    foreach m [modules] {
	validate_testsuite_mod $m
    }
    return
}

proc validate_pkgIndex_mod {m} {
    global distribution
    if {[llength [glob -nocomplain [file join $distribution modules $m pkgIndex.tcl]]] == 0} {
	puts "  Without package index : $m"
    }
    return
}

proc validate_pkgIndex {} {
    global distribution
    foreach m [modules] {
	validate_pkgIndex_mod $m
    }
    return
}

proc validate_doc_existence_mod {m} {
    global distribution
    if {[llength [glob -nocomplain [file join $distribution modules $m {*.[13n]}]]] == 0} {
	if {[llength [glob -nocomplain [file join $distribution modules $m {*.man}]]] == 0} {
	    puts "  Without * any ** manpages : $m"
	}
    } elseif {[llength [glob -nocomplain [file join $distribution modules $m {*.man}]]] == 0} {
	puts "  Without doctools manpages : $m"
    } else {
	foreach f [glob -nocomplain [file join $distribution modules $m {*.[13n]}]] {
	    if {![file exists [file rootname $f].man]} {
		puts "     no .man equivalent : $f"
	    }
	}
    }
    return
}

proc validate_doc_existence {} {
    global distribution
    foreach m [modules] {
	validate_doc_existence_mod $m
    }
    return
}


proc validate_doc_markup_mod {m} {
    gendoc null null $m
    return
}

proc validate_doc_markup {} {
    gendoc null null
    return
}


proc run-frink {args} {
    global distribution

    set tmp [file rootname [info script]].tmp.[pid]

    if {[llength $args] == 0} {
	set files [tclfiles]
    } else {
	set files [modtclfiles $args]
    }

    foreach f $files {
	puts "FRINK ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
	puts "$f..."
	puts "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"

	catch {exec frink 2> $tmp -H $f}
	set data [get_input $tmp]
	if {[string length $data] > 0} {
	    puts $data
	}
    }
    catch {file delete -force $tmp}
    return
}

proc run-procheck {args} {
    global distribution

    if {[llength $args] == 0} {
	set files [tclfiles]
    } else {
	set files [modtclfiles $args]
    }

    foreach f $files {
	puts "PROCHECK ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
	puts "$f ..."
	puts "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"

	catch {exec procheck >@ stdout $f}
    }
    return
}

proc get_input {f} {return [read [set if [open $f r]]][close $if]}

proc gd-gen-packages {} {
    global package_version distribution

    set P [file join $distribution PACKAGES]
    file copy -force $P $P.LAST
    set f [open $P w]
    puts $f "@@ RELEASE $package_version"
    puts $f ""

    array set packages {}
    foreach {p vm} [ipackages] {
	set packages($p) [lindex $vm 0]
    }

    nparray packages $f
    close $f
}

proc modified-modules {} {
    global distribution

    set mlist [modules]
    set modified [list]

    foreach m $mlist {
	set cl [file join $distribution modules $m ChangeLog]
	if {![file exists $cl]} {
	    lappend modified [list $m no-changelog]
	    continue
	}
	# Look for 'Released and tagged' within
	# the first four lines of the file. If
	# not present assume that the line is
	# deeper down, indicatating that the module
	# has been modified since the last release.

	set f [open $cl r]
	set n 0
	set mod 1
	while {$n < 5} {
	    gets $f line
	    incr n
	    if {[string match -nocase "*Released and tagged*" $line]} {
		if {$n <= 4} {set mod 0 ; break}
	    }
	}
	if {$mod} {
	    lappend modified $m
	}
	close $f
    }

    return $modified
}

# --------------------------------------------------------------
# Help

proc __help {} {
    ##    critcl-modules   - Return a list of modules with critcl enhancements.
    ##    critcl ?module?  - Build a critcl module [default is tklibc].

    puts stdout {
	Commands avalable through the swiss army knife aka SAK:

	help     - This help

	/Configuration
	version  - Return tklib version number
	major    - Return tklib major version number
	minor    - Return tklib minor version number
	name     - Return tklib package name

	/Development
	modules          - Return list of modules.
        contributors     - Print a list of contributors to tklib.
	lmodules         - See above, however one module per line
	imodules         - Return list of modules known to the installer.

	packages         - Return indexed packages in tklib, plus versions,
	                   one package per line. Extracted from the
	                   package indices found in the modules.
	provided         - Return list and versions of provided packages
	                   (in contrast to indexed).
	vcompare pkglist - Compare package list of previous 'packages'
	                   call with current packages. Marks all new
	                   and unchanged packages for higher attention.

        validate ?module..?     - Check listed modules for problems.
                                  For all modules if none specified.

	test ?module...?        - Run testsuite for listed modules.
	                          For all modules if none specified.

	/Release engineering
	gendist  - Generate distribution from CVS snapshot
        gentip55 - Generate a TIP55-style DESCRIPTION.txt file.
	rpmspec  - Generate a spec file for the creation of RPM's.
	tap      - Generate a TclApp Package Definition for use in the Tcl Dev Kit.
        yml      - Generate a YAML description file.

        release name sf-user-id
                 - Marks the current state of all files as a new
                   release. This updates all ChangeLog's, and
                   regenerates the contents of PACKAGES

        rstatus  - Determines the status of the code base with regard
                   to the last release.

	/Documentation
	nroff ?module...?    - Generate manpages
	html  ?module...?    - Generate HTML pages
	tmml  ?module...?    - Generate TMML
	text  ?module...?    - Generate plain text
	list  ?module...?    - Generate a list of manpages
	wiki  ?module...?    - Generate wiki markup
	latex ?module...?    - Generate LaTeX pages
	dvi   ?module...?    - See latex, + conversion to dvi
	ps    ?module...?    - See dvi,   + conversion to PostScript

        desc  ?module...?    - Module/Package descriptions
        desc/2 ?module...?   - Module/Package descriptions, alternate format.
    }
}

# --------------------------------------------------------------
# Configuration

proc __name    {} {global tklib_name    ; puts -nonewline $tklib_name}
proc __version {} {global tklib_version ; puts -nonewline $tklib_version}
proc __minor   {} {global tklib_version ; puts -nonewline [lindex [split $tklib_version .] 1]}
proc __major   {} {global tklib_version ; puts -nonewline [lindex [split $tklib_version .] 0]}

# --------------------------------------------------------------
# Development

proc __imodules {}  {puts [imodules]}
proc __modules {}  {puts [modules]}
proc __lmodules {} {puts [join [modules] \n]}


proc nparray {a {chan stdout}} {
    upvar $a packages

    set maxl 0
    foreach name [lsort [array names packages]] {
        if {[string length $name] > $maxl} {
            set maxl [string length $name]
        }
    }
    foreach name [lsort [array names packages]] {
	foreach v $packages($name) {
	    puts $chan [format "%-*s %s" $maxl $name $v]
	}
    }
    return
}

proc __packages {} {
    array set packages [ipackages]
    nparray packages
    return
}

proc __provided {} {
    array set packages [ppackages]
    nparray packages
    return
}


proc __vcompare {} {
    global argv
    set oldplist [lindex $argv 0]
    pkg-compare $oldplist
    return
}

proc __rstatus {} {
    global distribution approved

    catch {
	set f [file join $distribution .APPROVE]
	set f [open $f r]
	while {![eof $f]} {
	    if {[gets $f line] < 0} continue
	    set line [string trim $line]
	    if {$line == {}} continue
	    set approved($line) .
	}
	close $f
    }
    pkg-compare [file join $distribution PACKAGES]
    return
}

proc pkg-compare {oldplist} {
    global approved ; array set approved {}

    getpackage struct::set

    array set curpkg [ipackages]
    array set oldpkg [loadpkglist $oldplist]
    array set mod {}
    array set changed {}
    foreach m [modified-modules] {
	set mod($m) .
    }

    foreach p [array names curpkg] {
	set __($p) .
	foreach {vlist module} $curpkg($p) break
	set curpkg($p) $vlist
	set changed($p) [info exists mod($module)]
    }
    foreach p [array names oldpkg] {set __($p) .}
    set unified [lsort [array names __]]
    unset __

    set maxl 0
    foreach name $unified {
        if {[string length $name] > $maxl} {
            set maxl [string length $name]
        }
    }

    set maxm 0
    foreach m [modules] {
        if {[string length $m] > $maxm} {
            set maxm [string length $m]
        }
    }

    set lastm ""
    foreach m [lsort -dict [modules]] {
	set packages {}
	foreach {p ___} [ppackages $m] {
	    lappend packages $p
	}
	foreach name [lsort -dict $packages] {
	    set skip 0
	    set suffix ""
	    set prefix "   "
	    if {![info exists curpkg($name)]} {set curpkg($name) {}}
	    if {![info exists oldpkg($name)]} {
		set oldpkg($name) {}
		set suffix " NEW"
		set prefix "Nn "
		set skip 1
	    }
	    if {!$skip} {
		# Draw attention to changed packages where version is
		# unchanged.

		set vequal [struct::set equal $oldpkg($name) $curpkg($name)]

		if {$changed($name)} {
		    if {$vequal} {
			# Changed according to ChangeLog, Version is not. ALERT.
			set prefix "!! "
			set suffix "\t<<< MISMATCH. Version ==, ChangeLog ++"
		    } else {
			# Both changelog and version number indicate a change.
			# Small alert, have to classify the order of changes.
			set prefix "cv "
			set suffix "\t=== Classify changes."
		    }
		} else {
		    if {$vequal} {
			# Versions are unchanged, changelog also indicates no change.
			# No particular attention here.
		    } else {
			# Versions changed, but according to changelog nothing in code. ALERT.
			set prefix "!! "
			set suffix "\t<<< MISMATCH. ChangeLog ==, Version ++"
		    }
		}
		if {[info exists approved($name)]} {
		    set prefix "   "
		    set suffix ""
		}
	    }

	    # To handle multiple versions we match the found versions up
	    # by major version. We assume that we have only one version
	    # per major version. This allows us to detect changes within
	    # each major version, new major versions, etc.

	    array set om {} ; foreach v $oldpkg($name) {set om([lindex [split $v .] 0]) $v}
	    array set cm {} ; foreach v $curpkg($name) {set cm([lindex [split $v .] 0]) $v}

	    set all [lsort -dict [struct::set union [array names om] [array names cm]]]

	    sakdebug {
		puts @@@@@@@@@@@@@@@@
		parray om
		parray cm
		puts all\ $all
		puts @@@@@@@@@@@@@@@@
	    }

	    foreach v $all {
		if {![string equal $m $lastm]} {
		    set mdis $m
		} else {
		    set mdis ""
		}
		set lastm $m

		if {[info exists om($v)]} {set ov $om($v)} else {set ov "--"}
		if {[info exists cm($v)]} {set cv $cm($v)} else {set cv "--"}

		puts stdout ${prefix}[format "%-*s %-*s %-*s %-*s" \
					  $maxm $mdis $maxl $name 8 $ov 8 $cv]$suffix
	    }

	    unset om cm
	}
    }
    return
}

proc __test {} {
    global argv distribution
    # Run testsuite

    set modules $argv
    if {[llength $modules] == 0} {
	set modules [modules]
    }

    exec [info nameofexecutable] \
	    [file join $distribution all.tcl] \
	    -modules $modules \
	    >@ stdout 2>@ stderr
    return
}

proc checkmod {} {
    global argv
    set fail 0
    foreach m $argv {
	if {![modules_mod $m]} {
	    puts "  Bogus module: $m"
	    set fail 1
	}
    }
    if {$fail} {
	puts "  Stop."
	return 0
    }
    return 1
}

# -------------------------------------------------------------------------
# Critcl stuff
# -------------------------------------------------------------------------

array set critclmodules {
    tklibc   {}
}

# Build critcl modules. If no args then build the tklibc module.
proc **__critcl {} {
    global argv critcl critclmodules tcl_platform
    if {$tcl_platform(platform) == "windows"} {
        set critcl [auto_execok tclkitsh]
        if {$critcl != {}} {
            set critcl [concat $critcl [auto_execok critcl.kit]]
        }
    } else {
        set critcl [auto_execok critcl]
    }

    if {$critcl != {}} {
        if {[llength $argv] == 0} {
            #foreach p [array names critclmodules] {
            #    critcl_module $p
            #}
            critcl_module tklibc
        } else {
            foreach m $argv {
                if {[info exists critclmodules($m)]} {
                    critcl_module $m
                } else {
                    puts "warning: $m is not a critcl module"
                }
            }
        }
    } else {
        puts "error: cannot find a critcl to run."
        return 1
    }
    return
}

# Prints a list of all the modules supporting critcl enhancement.
proc **__critcl-modules {} {
    global critclmodules
    puts tklibc
    foreach m [array names critclmodules] {
        puts $m
    }
    return
}

proc critcl_module {pkg} {
    global critcl distribution critclmodules
    if {$pkg == "tklibc"} {
        set files [file join $distribution modules tklibc.tcl]
        foreach m [array names critclmodules] {
            foreach f $critclmodules($m) {
                lappend files [file join $distribution modules $f]
            }
        }
    } else {
        foreach f $critclmodules($pkg) {
            lappend files [file join $distribution modules $f]
        }
    }
    set target [file join $distribution modules]
    catch {
        puts "$critcl -force -libdir [list $target] -pkg [list $pkg] $files"
        eval exec $critcl -force -libdir [list $target] -pkg [list $pkg] $files 
    } r
    puts $r
    return
}

# -------------------------------------------------------------------------

proc __validate {} {
    global argv
    if {[llength $argv] == 0} {
	_validate_all
    } else {
	if {![checkmod]} {return}
	foreach m $argv {
	    _validate_module $m
	}
    }
    return
}

proc _validate_all {} {
    global tklib_name tklib_version
    set i 0

    puts "Validating $tklib_name $tklib_version development"
    puts "==================================================="
    puts "[incr i]: Existence of testsuites ..."
    puts "------------------------------------------------------"
    validate_testsuites
    puts "------------------------------------------------------"
    puts ""

    puts "[incr i]: Existence of package indices ..."
    puts "------------------------------------------------------"
    validate_pkgIndex
    puts "------------------------------------------------------"
    puts ""

    puts "[incr i]: Consistency of package versions ..."
    puts "------------------------------------------------------"
    validate_versions
    puts "------------------------------------------------------"
    puts ""

    puts "[incr i]: Installed vs. developed modules ..."
    puts "------------------------------------------------------"
    validate_imodules
    puts "------------------------------------------------------"
    puts ""

    puts "[incr i]: Existence of documentation ..."
    puts "------------------------------------------------------"
    validate_doc_existence
    puts "------------------------------------------------------"
    puts ""

    puts "[incr i]: Validate documentation markup (doctools) ..."
    puts "------------------------------------------------------"
    validate_doc_markup
    puts "------------------------------------------------------"
    puts ""

    puts "[incr i]: Static syntax check ..."
    puts "------------------------------------------------------"

    set frink    [auto_execok frink]
    set procheck [auto_execok procheck]

    if {$frink    == {}} {puts "  Tool 'frink'    not found, no check"}
    if {$procheck == {}} {puts "  Tool 'procheck' not found, no check"}
    if {($frink == {}) || ($procheck == {})} {
	puts "------------------------------------------------------"
    }
    if {($frink == {}) && ($procheck == {})} {
	return
    }
    if {$frink    != {}} {
	run-frink
	puts "------------------------------------------------------"
    }
    if {$procheck    != {}} {
	run-procheck
	puts "------------------------------------------------------"
    }
    puts ""

    return
}

proc _validate_module {m} {
    global tklib_name tklib_version
    set i 0

    puts "Validating $tklib_name $tklib_version development -- $m"
    puts "==================================================="
    puts "[incr i]: Existence of testsuites ..."
    puts "------------------------------------------------------"
    validate_testsuite_mod $m
    puts "------------------------------------------------------"
    puts ""

    puts "[incr i]: Existence of package indices ..."
    puts "------------------------------------------------------"
    validate_pkgIndex_mod $m
    puts "------------------------------------------------------"
    puts ""

    puts "[incr i]: Consistency of package versions ..."
    puts "------------------------------------------------------"
    validate_versions_mod $m
    puts "------------------------------------------------------"
    puts ""

    #puts "[incr i]: Installed vs. developed modules ..."
    puts "------------------------------------------------------"
    validate_imodules_mod $m
    puts "------------------------------------------------------"
    puts ""

    puts "[incr i]: Existence of documentation ..."
    puts "------------------------------------------------------"
    validate_doc_existence_mod $m
    puts "------------------------------------------------------"
    puts ""

    puts "[incr i]: Validate documentation markup (doctools) ..."
    puts "------------------------------------------------------"
    validate_doc_markup_mod $m
    puts "------------------------------------------------------"
    puts ""

    puts "[incr i]: Static syntax check ..."
    puts "------------------------------------------------------"

    set frink    [auto_execok frink]
    set procheck [auto_execok procheck]

    if {$frink    == {}} {puts "  Tool 'frink'    not found, no check"}
    if {$procheck == {}} {puts "  Tool 'procheck' not found, no check"}
    if {($frink == {}) || ($procheck == {})} {
	puts "------------------------------------------------------"
    }
    if {($frink == {}) && ($procheck == {})} {
	return
    }
    if {$frink    != {}} {
	run-frink $m
	puts "------------------------------------------------------"
    }
    if {$procheck    != {}} {
	run-procheck $m
	puts "------------------------------------------------------"
    }
    puts ""

    return
}

# --------------------------------------------------------------
# Release engineering

proc __gendist {} {
    gd-cleanup
    gd-tip55
    gd-gen-rpmspec
    gd-gen-tap
    gd-assemble
    gd-gen-archives

    puts ...Done
    return
}

proc __gentip55 {} {
    gd-tip55
    puts "Created DESCRIPTION.txt"
    return
}

proc __yml {} {
    gd-gen-yml
    puts "Created YAML spec file \"tklib.yml\""
    return
}

proc __contributors {} {
    global contributors
    contributors
    foreach person [lsort [array names contributors]] {
        puts "$person <$contributors($person)>"
    }
    return
}

proc __tap {} {
    gd-gen-tap
    puts "Created Tcl Dev Kit \"tklib.tap\""
}

proc __rpmspec {} {
    gd-gen-rpmspec
    puts "Created RPM spec file \"tklib.spec\""
}

proc __release {} {
    # Regenerate PACKAGES, and extend

    global argv argv0 distribution package_name package_version

    getpackage textutil

    if {[llength $argv] != 2} {
	puts stderr "$argv0: wrong#args: release name sf-user-id"
	exit 1
    }

    foreach {name sfuser} $argv break
    set email "<${sfuser}@users.sourceforge.net>"
    set pname [textutil::cap $package_name]

    set notice "[clock format [clock seconds] -format "%Y-%m-%d"]  $name  $email

	*
	* Released and tagged $pname $package_version ========================
	* 

"

    set logs [list [file join $distribution ChangeLog]]
    foreach m [modules] {
	set m [file join $distribution modules $m ChangeLog]
	if {![file exists $m]} continue
	lappend logs $m
    }

    foreach f $logs {
	puts "\tAdding release notice to $f"
	set fh [open $f r] ; set data [read $fh] ; close $fh
	set fh [open $f w] ; puts -nonewline $fh $notice$data ; close $fh
    }

    gd-gen-packages
    return
}

proc __approve {} {
    global argv distribution

    # Record the package as approved. This will suppress any alerts
    # for that package by rstatus. Required for packages which have
    # been classified, and for packages where a MISMATCH is bogus (due
    # to several packages sharing a ChangeLog)

    set f [open [file join $distribution .APPROVE] a]
    foreach package $argv {
	puts $f $package
    }
    close $f
    return
}


# --------------------------------------------------------------
# Documentation

proc __html  {} {global argv ; if {![checkmod]} return ; eval gendoc html  html $argv}
proc __nroff {} {global argv ; if {![checkmod]} return ; eval gendoc nroff n    $argv}
proc __tmml  {} {global argv ; if {![checkmod]} return ; eval gendoc tmml  tmml $argv}
proc __text  {} {global argv ; if {![checkmod]} return ; eval gendoc text  txt  $argv}
proc __wiki  {} {global argv ; if {![checkmod]} return ; eval gendoc wiki  wiki $argv}
proc __latex {} {global argv ; if {![checkmod]} return ; eval gendoc latex tex  $argv}
proc __dvi   {} {
    global argv ; if {![checkmod]} return
    __latex
    file mkdir [file join doc dvi]
    cd         [file join doc dvi]
    foreach f [glob -nocomplain ../latex/*.tex] {
	puts "Gen (dvi): $f"
	exec latex $f 1>@ stdout 2>@ stderr
    }
    cd ../..
}
proc __ps   {} {
    global argv ; if {![checkmod]} return
    __dvi
    file mkdir [file join doc ps]
    cd         [file join doc ps]
    foreach f [glob -nocomplain ../dvi/*.dvi] {
	puts "Gen (dvi): $f"
	exec dvips -o [file rootname [file tail $f]].ps $f 1>@ stdout 2>@ stderr
    }
    cd ../..
}

proc __list  {} {
    global argv ; if {![checkmod]} return
    eval gendoc list l $argv
    
    set FILES [glob -nocomplain doc/list/*.l]
    set LIST [open [file join doc list manpages.tcl] w]

    foreach file $FILES {
        set f [open $file r]
        puts $LIST [read $f]
        close $f
    }
    close $LIST

    eval file delete -force $FILES

    return
}

proc __desc  {} {
    global argv ; if {![checkmod]} return
    array set pd [getpdesc]

    getpackage struct::matrix
    getpackage textutil

    struct::matrix m
    m add columns 3

    puts {Descriptions...}
    if {[llength $argv] == 0} {set argv [modules]}

    foreach m [lsort $argv] {
	array set _ {}
	set pkg {}
	foreach {p vlist} [ppackages $m] {
	    catch {set _([lindex $pd($p) 0]) .}
	    lappend pkg $p
	}
	set desc [string trim [join [array names _] ", "] " \n\t\r,"]
	set desc [textutil::adjust $desc -length 20]
	unset _

	m add row [list $m $desc]
	m add row {}

	foreach p [lsort -dictionary $pkg] {
	    set desc ""
	    catch {set desc [lindex $pd($p) 1]}
	    if {$desc != ""} {
		set desc [string trim $desc]
		set desc [textutil::adjust $desc -length 50]
		m add row [list {} $p $desc]
	    } else {
		m add row [list {**} $p ]
	    }
	}
	m add row {}
    }

    m format 2chan
    puts ""
    return
}

proc __desc/2  {} {
    global argv ; if {![checkmod]} return
    array set pd [getpdesc]

    getpackage struct::matrix
    getpackage textutil

    puts {Descriptions...}
    if {[llength $argv] == 0} {set argv [modules]}

    foreach m [lsort $argv] {
	struct::matrix m
	m add columns 3

	m add row {}

	set pkg {}
	foreach {p vlist} [ppackages $m] {lappend pkg $p}

	foreach p [lsort -dictionary $pkg] {
	    set desc ""
	    set sdes ""
	    catch {set desc [lindex $pd($p) 1]}
	    catch {set sdes [lindex $pd($p) 0]}

	    if {$desc != ""} {
		set desc [string trim $desc]
		#set desc [textutil::adjust $desc -length 50]
	    }

	    if {$desc != ""} {
		set desc [string trim $desc]
		#set desc [textutil::adjust $desc -length 50]
	    }

	    m add row [list $p "  $sdes" "  $desc"]
	}
	m format 2chan
	puts ""
	m destroy
    }

    return
}

# --------------------------------------------------------------

set cmd [lindex $argv 0]
if {[llength [info procs __$cmd]] == 0} {
    puts stderr "unknown command $cmd"
    set fl {}
    foreach p [lsort [info procs __*]] {
	lappend fl [string range $p 2 end]
    }
    puts stderr "use: [join $fl ", "]"
    exit 1
}

set  argv [lrange $argv 1 end]
incr argc -1

__$cmd
exit 0
