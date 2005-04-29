#!/bin/sh
# the next line restarts using tclsh \
exec tclsh8.4 "$0" "$@"

proc macroman2utf8 { name } {
	set in  [open $name]
	fconfigure $in -encoding binary -translation lf
	set data [gets $in]
	set ismac [string first \x0d $data]
	close $in
	if { $ismac != -1 } {
		set newname "$name-tmp"
		set in  [open $name]
		set out [open $newname {WRONLY CREAT EXCL}]
		fconfigure  $in -encoding macRoman
		fconfigure  $out -encoding utf-8
		fcopy $in $out
		close $out
		close $in
		catch { file mtime $newname [file mtime $name] }
		file rename -force $newname $name
		puts -nonewline "#"
		flush stdout
	}
}
foreach filename $argv { macroman2utf8 $filename }
puts ""