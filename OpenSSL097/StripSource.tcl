#!/usr/bin/tclsh

proc StripSource {dir} {
	if {[file isdirectory $dir]} {		
		### zap the algorithms (which are not covered by the ifdefs)
		### This pollutes the .orig directory since that copy hasn't been
		### made yet, but I don't really care.
		set MATCHES "_OPEN_SOURCE_\|NO_IDEA"
		set MORE_DEFS "-DNO_IDEA -DOPENSSL_NO_IDEA"
		if {[file exists $dir/openssl]} {
			set base $dir/openssl
		} else {
			set base $dir
		}

		foreach X [glob -nocomplain $dir/openssl/crypto/idea/*.\[ch\]] {
			puts "Truncating $X ..."
			exec rm $X
			exec touch $X
		}
	
		### The standard stuff...
		set olddir [pwd]
		cd $dir
		
		exec cp -R ../[file tail [pwd]] ../[file tail [pwd]].orig
		set ORIG ../[file tail [pwd]].orig/
		
		if {[catch {eval exec grep -r --files-with-match "$MATCHES" [glob *]} files] == 0} {
		foreach X $files {
			puts "Stripping $X ..."
			catch {eval exec unifdef -D_OPEN_SOURCE_ -D__OPEN_SOURCE__ $MORE_DEFS $ORIG$X > $X}
		}
		}
		
		cd $olddir
	} else {
		puts "Error: no such directory: $dir"
	}
}

set mydir [lindex $argv 0]

puts "Using project-only strip source on $mydir:"
StripSource $mydir
