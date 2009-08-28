#!/bin/tclsh8.3

proc main {argc argv} {

	array set functions ""

	foreach f $argv {
		puts stderr "PROCESSING FILE $f"

		catch {exec cc -DNeedFunctionPrototypes -E $f} data
		#set functionList [regexp -all -inline {[a-zA-Z0-9_-]+[ \t\n\r]+([a-zA-Z0-9_-]+)[ \t\n\r]+\([ \t\n\r]*([^\)]+)[ \t\n\r]*\)[ \t\n\r]*;} $data]
		set functionList [regexp -all -inline {[a-zA-Z0-9_\-\*]+[ \t\n\r\*]+([a-zA-Z0-9_\-\*]+)[ \t\n\r]*\(([^\)]*)\)[ \t\n\r]*;} $data]
		set functionList [concat $functionList \
			[regexp -all -inline {[a-zA-Z0-9_\-\*]+[ \t\n\r\*]+([a-zA-Z0-9_\-\*]+)[ \t\n\r]*_ANSI_ARGS_\(\(([^\)]*)\)\)[ \t\n\r]*;} $data]]
		#puts "FL $functionList"
		foreach {junk function args} $functionList {
			#puts "FUNC $function ARGS $args"
			set args [string map {"\n" "" "\r" "" "\t" " " "," ", "} $args]
			regsub -all {\s{2,}} $args " " args
			set functions($function) $args
		}
	}

	puts "array set ::functions \{"
	foreach function [lsort -dictionary [array names functions]] {
		if {"_" == [string index $function 0] || "_" == [string index $function end]} {
			continue
		}
		puts "\t$function [list [set functions($function)]]"
	}
	puts "\}"
}

proc sglob {pattern} {
	return [glob -nocomplain $pattern]
}

#main $argc /usr/local/include/tclDecls.h
#return
        
main $argc [concat [sglob /usr/include/*.h] [sglob /usr/include/*/*.h] \
[sglob /usr/local/include/*.h] [sglob /usr/local/include/*/*.h] \
[sglob /usr/X11R6/include/*.h] [sglob /usr/X11R6/include/*/*.h] \
[sglob /usr/X11R6/include/*/*/*.h] [sglob /usr/local/include/X11/*.h] \
[sglob /usr/local/include/X11/*/*.h]]
