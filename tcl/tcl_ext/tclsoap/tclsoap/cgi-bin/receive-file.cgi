#!/opt/TclPro1.4/win32-ix86/bin/tclsh83
#

set ::auto_path [linsert $::auto_path 0 {/users/pat/lib/tcl}]
package require cgi

set title {File Recipient}
set soapdir "soap"

proc validate {filename} {
    if {[catch {
	set f [open $filename "r"]
	set data [read $f]
	close $f
    }]} {
	error "Your file could not be read. Uploading failed."
    }

    return $data
}

proc copy {filename methodName} {
    global soapdir

    set methodName [file tail $methodName]

    if {[file exists soapdir/$methodName]} {
	error "invalid method: \"$methodName\" already exists"
    }
    if {[catch {
	file copy $filename [file join $soapdir $methodName]
    } msg]} {
	error "installation failed: \"$msg\""
    }
}

if {[catch {
    
    cgi_input [lindex $argv 0]
    cgi_import methodName

    cgi_title $title

    cgi_body {
	cgi_h1 align = center $title

	set local [cgi_import_filename -local file1]
	set remote [cgi_import_filename -remote file1]
	set type [cgi_import_filename -type file1]

	# validate the file
	set msg {}
	set failed [catch { 
	    validate $local
	    copy $local $methodName
	} msg]
	    
	file delete $local
	
	if {$failed} { error "$msg"}

	cgi_p {
	    Your file has been sucessfully uploaded and should now be
	    available for use.
	}

    }
} msg]} {
    puts "$msg"
}

#
# Local variables:
# mode: tcl
# End:
