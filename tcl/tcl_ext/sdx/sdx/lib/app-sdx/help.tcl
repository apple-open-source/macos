array set help {
    addtoc { Adds a "TOC" to a dir-tree, containing a full MD5 file index

        Usage: addtoc dir
    }
    eval { Evaluate one Tcl command from the command line

        Usage: eval cmd ?arg ...?
    }
    fetch { Fetch file from a HTTP or FTP server

        Usage: fetch url ?destdir/file? ?chunksize?

        If a destination dir is specified, the file will be stored
        there under the same name as the original.
    }
    ftpd { World's smallest FTP server?

        Usage: ftpd ?options?

        -port      n      Port number to listen on (default: 8021)
        -root      dir    Root of pages served (default: /ftproot)
        -debug     n      Enable debug tracing level (default: 1)
        -timeout   n      Server idle timeout seconds (default: 600)
        -email     addr   This address is mentioned to send support emails to

        Log output is also saved in the file "ftpd.log".
    }
    httpd { Simple HTTPD server by Steve Uhler and Brent Welch

        Usage: httpd ?options?

        -port      n      Port nunber to listen on (default: 8080)
        -root      dir    Root of pages served (default: /wwwroot)
        -default   file   File to use for dir (default: index.html)
        -bufsize   n      Buffer size used for transfers (default: 32768)
        -sockblock 0|1    Use blocking sockets (default: no)
        -config    file   Read configurations from this file (default: none)
        -launch    0|1    Launch a webbrowser on homepage (default: no)
        -ipaddr    addr   Override listening IP address (default: std)
    }
    httpdist { Fetch/upload updates using the HTTPSYNC protocol

        Usage:  httpdist @?url?                 Update in current directory
                httpdist pack ?...?             Create packing list file
                httpdist send ftpurl ?arch?     Upload through FTP

        For more information, type "httpdist" without further parameters.
        The HTTPSYNC protocol is described on this site:
                http://www.mibsoftware.com/httpsync/
    }
    ls { A very simple Unix-like "ls" command in pure Tcl

        Usage: ls ?-l? ?files/dirs?
    }
    lsk { List contents of a starkit or starpack

        Usage: lsk starkit
    }
    md5sum { Calculate and print the MD5 message digest of files

        Usage: md5sum files ?...?
    }
    mkinfo { Metakit file structure display

        Usage: mkinfo file ?...?

        Reports starting offset of Metakit data and structure of all views.
    }
    mkpack { Remove free space from Metakit file or starkit or starpack

        Usage: mkpack infile outfile

        The input file is written to the output file, removing any free
        space which may have been created by subsequent commit changes.
        The header of the datafile is copied intact to the output file,
        the result is in every way equivalent but with an optimal size.
    }
    mkshow { MetaKit raw datafile dump/view utility

        Usage: mkshow file view ?prop ...?

        For more information, type "mkshow" without further parameters.
    }
    mksplit { Split starkit/starpack into head and tail files

        Usage: mksplit file ?...?

        File "foo/bar.kit" is copied to "foo.head" and "foo.tail", with
        the head containing the starkit header (or raw executable), and
        all Metakit data stored in the tail file.  See also mkinfo.
    }
    mkzipkit { Convert a zip archive into a Tcl Module or zipkit file

        Usage: mkzipkit infile outfile ?stubfile?

        Convert a zip archive into a Tcl Module or zipkit file by adding
        a SFX header that can enable TclKit to mount the archive. This
        provides an alternative to Metakit-based starkits. If no stubfile
        is specified, a standard one using vfs::zip::mount will be used.
    }
    qwrap { Quick-wrap the specified source file into a starkit

        Usage: qwrap file ?name? ?options?

        -runtime   file   Take starkit runtime prefix from file

        Generates a temporary .vfs structure and calls wrap to create
        a starkit for you.  The resulting starkit is placed into file.kit
        (or name.kit if name is specified). If the -runtime option is
        specified a starpack will be created using the specified runtime
        file instead of a starkit.

        Note that file may be a local file, or URL (http or ftp).
    }
    ratarx { Reverse actions of a "tar x" command

        Usage: ratarx ?-n? targzfile

        This command deletes those files from a directory tree which have
        the same size and modification date as in a specified targz file.
        Empty directories are recursively deleted as final cleanup step.

        The logic is such that deletion is "safe": only files/dirs which
        are redundant (i.e. also present in the tar file) will be deleted.

        The following two commands together will usually be a no-op:
                tar xfz mydata.tar.gz
                ratar mydata.tar.fz

        Use the "-n" option to see what would be deleted without doing so.
    }
    rexecd { An rexec-compatible remote Tcl command server

        Usage: rexecd ?options?

        -port      n      Port nunber to listen on (default: 512)
        -ipaddr    ip     Listen address (default: 0.0.0.0, all interfaces)
    }
    starsync { Starsync CGI server

        Usage: starsync ?logfile?

        Reads starsync input, returns HTTP response with starsync reply.
        This is sufficient to create a starsync server which serves all
        starkits found in the current directory (named *.kit).  Such a
        server never alters anything, other than optionally appending one
        line for each serviced request to the specified writable logfile.
    }
    sync { Synchronize two directory trees (either can use any type of VFS)

        Usage: sync ?options? src dest

        For more information, type "sync" without further parameters.
    }
    tgz2kit { Convert a tar/gz file to a starkit

        Usage: tgz2kit ?-notop? inputfile

        Output is a file in local dir with same name, but .kit extension.
        The optional -notop flag strips top-level dirname from all paths.
    }
    treetime { Adjust modtimes in dir trees to match most recent file inside

        Usage: treetime dirs...

        Scans dir tree and resets mod dates on directories
        to match the date of the newest files inside them.
    }
    unwrap { Unpack a starkit into a new directory

        Usage: unwrap name
        
        The name specified is the name of the starkit file.
        The results are placed in a directory "name.vfs", which must
        not yet exist.  The inverse of "unwrap" is called "wrap".
    }
    update { Fetch or update a starkit from a Starsync server (via http)

        Usage: update ?-from url? ?-n? starkit

        -from   url     Use specified Starsync server instead of default.
        -n              Show differences, but do not make any changes.

        Fetch changes so local starkit matches the one on the server.
        The Starsync mechanism only transfers files which have changed.

        Warning: this adds, modifies, *and* deletes files in the starkit.
    }
    version { Establish "version" of a starkit, based on what it contains

        Usage: $argv0 ?-fixtime? file ...

        Scans a starkit and produces a signature based on names of all
        dirs and files in it, their sizes, and their modification dates.
        Also reports modification time of newest file found (as GMT).

        The "-fixtime" flag will adjust the starkit's modification time
        to reflect that same time, this is merely a convenience feature.
    }
    wrap { Pack a file system directory area to a starkit

        Usage: wrap name ?options?

        -interp    name   Start something other than "tclsh" up
        -nocomp           Do not compress files added to starkit
        -runtime   file   Take starkit runtime prefix from file
        -verbose          Report actions taken
	-vfs       dir    Use this directory as the vfs tree
        -writable         Allow modifications (must be single writer)

        Expects a directory called "name.vfs", and creates a fresh
        starkit from it, called "name".  The -vfs option lets you
	use something other than "name.vfs".  If a starkit is specified
	as runtime prefix, then files will be merged with it.
    }
}

set cmd [lindex $argv 0]
if {[info exists help($cmd)]} {
    puts "\n$help($cmd)"
    exit
}

cd [file dirname [info script]]
set l [lsort [glob *.tcl]]
puts "Specify one of the following commands:"

foreach f $l {
    set cmd [file rootname $f]
    if {![info exists help($cmd)]} continue
    switch -- $cmd {
        help continue
        main continue
    }
    if {$argv0 == "help"} {
        puts [format {  %-10s %s} $cmd [lindex [split $help($cmd) \n] 0]]
    } else { # called by default, so just enumerate all the known commands
        puts -nonewline [format { %-9s} $cmd]
    }
}
puts "\nFor more information, type:  $argv0 help ?command?"
