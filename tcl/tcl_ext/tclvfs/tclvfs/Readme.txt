Hello!  The code here has evolved from ideas and excellent work by Matt
Newman, Jean-Claude Wippler, TclKit etc.  To make this really successful,
we need a group of volunteers to enhance what we have and build a new way
of writing and distributing Tcl code.

Introduction
------------

This is an implementation of a 'vfs' extension (and a 'vfs' package,
including a small library of Tcl code).  The goal of this extension
is to expose Tcl 8.4's new filesystem C API to the Tcl level.

Using this extension, the editor Alphatk can actually auto-mount,
view and edit (but not save, since they're read-only) the contents of
.zip files directly (see <http://www.purl.org/net/alphatk/>), and you
can do things like:

    file copy ftp://ftp.foo.com/pub/readme.txt .

With 'Tkhtml' and this extension, writing a web-browser in Tcl should be 
pretty trivial.

None of the vfs's included are 100% complete or optimal yet, so if only for
that reason, code contributions are very welcome.  Many of them still
contain various debugging code, etc.  This will be gradually removed and
the code completely cleaned up and documented as the package evolves.

-- Vince Darley, April 2002-February 2003

Compile/build
-------------

The standard 'configure ; make ; make install' should work, but if it
doesn't, I'm afraid I can't help --- I am not an expert on these issues
and find it amazing that to compile a single C file (generic/vfs.c) a
dozen or so TEA 'helper' files are required.  I believe 'gmake' may
be required on some platforms.

For windows, there is a VC++ makefile in the win directory ('nmake -f
makefile.vc') should do the trick.

Tests and installation
----------------------

The tests/vfs*.test files should all pass (provided you have an active
internet connection).

To install, you probably want to rename the directory 'library' to 'vfs1.0'
and place it in your Tcl hierarchy, with the necessary shared library inside
(improvements to makefiles to streamline this much appreciated).  On Windows
'nmake -f makefile.vc install' should do everything for you.

Current implementation
----------------------

Some of the provided vfs's require the Memchan extension for any operation 
which involves opening files.  The zip vfs also require 'Trf' (for its
'zip' command).

The vfs's currently available are:

package vfs::ftp 1.0 
package vfs::http 0.5
package vfs::mk4 1.6 
package vfs::ns 0.5 
package vfs::tar 0.9
package vfs::test 1.0
package vfs::urltype 1.0
package vfs::webdav 0.1
package vfs::zip 1.0 

--------+-----------------------------------------------------------------
vfs     |  example mount command                       
--------+-----------------------------------------------------------------
zip     |  vfs::zip::Mount my.zip local
ftp     |  vfs::ftp::Mount ftp://user:pass@ftp.foo.com/dir/name/ local
mk4     |  vfs::mk4::Mount myMk4database local
urltype |  vfs::urltype::Mount ftp
test    |  vfs::test::Mount ...
--------+-----------------------------------------------------------------

These are also available, but not so heavily debugged:

--------+-----------------------------------------------------------------
ns      |  vfs::ns::Mount ::tcl local
webdav  |  vfs::webdav::Mount http://user:pass@foo.com/blah local
http    |  vfs::http::Mount http://foo.com/blah local
--------+-----------------------------------------------------------------

For file-systems which make use of a local file (e.g. mounting zip or mk4
archives), it is often most simple to have 'local' be the same name as 
the archive itself.  The result of this is that Tcl will then see the
archive as a directory, rather than a file.  Otherwise you might wish
to create a dummy file/directory called 'local' before mounting.

C versus Tcl
------------

It may be worth writing a vfs for commonly used formats like 'zip' in C. 
This would make it easier to create single-file executables because with
this extension we have a bootstrap problem: to mount the executable
(assuming it has a .zip archive appended to it) we need to have
'vfs::zip::Mount' and related procedures loaded, but this means that those 
procedures would have to be stored in the executable outside the zip
archive, wasting space.

Note: Richard Hipp has written 'zvfs' which uses the older, less-complete
vfs support in Tcl 8.3.  It is GNU-licensed, which makes distributing binary
versions a little more complex.  Also Prowrap contains a similar zip-vfs
implementation using the same old APIs (it is BSD-licensed).  Either of 
these can probably be modified to work with the new APIs quite easily.

Helping!
--------

Any help is much appreciated!  The current code has very much _evolved_
which means it isn't necessarily even particular well thought out, so if
you wish to contribute a single line of code or a complete re-write, I'd be
very happy!

Future thoughts
---------------

See:

http://developer.gnome.org/doc/API/gnome-vfs/
http://www.appwatch.com/lists/gnome-announce/2001-May/000267.html
http://www.lh.com/~oleg/ftp/HTTP-VFS.html
http://www.atnf.csiro.au/~rgooch/linux/vfs.txt

for some ideas.  It would be good to accumulate ideas on the limitations of
the current VFS support so we can plan out what vfs 2.0 will look like (and
what changes will be needed in Tcl's core to support it).  

"Asynchronicity" -- Obvious things which come to mind are asynchronicity:
'file copy' from a mounted remote site (ftp or http) is going to be very
slow and simply block the application.  Commands like that should have new
asynchronous versions which can be used when desired (for example, 'file
copy from to -callback foo' would be one approach to handling this).

"exec" -- this Tcl command effectively boils down to forking off a variety
of processes and hooking their input/output/errors up appropriately.  Most
of this code is quite generic, and ends up in 'TclpCreateProcess' for the
actual forking and execution of another process (whose name is given by
'argv[0]' in TclpCreateProcess).  Would it be possible to make a
Tcl_FSCreateProcess which can pass the command on either to the native
filesystem or to virtual filesystems?  The simpler answer is "yes", given
that we can simply examine 'argv[0]' and see if it is it is a path in a
virtual filesystem and then hand it off appropriately, but could a vfs
actually implement anything sensible?  The kind of thing I'm thinking of is
this: we mount an ftp site and would then like to execute various ftp
commands directly.  Now, we could use 'ftp::Quote' (from the ftp package) to
send commands directly, but why not 'exec' them?  If my ftp site is mounted
at /tcl/ftppub, why couldn't "exec /tcl/ftppub FOO arg1 arg2" attempt a
verbatim "FOO arg1 arg2" command on the ftp connection?  (Or would perhaps
"exec /tcl/ftppub/FOO arg1 arg2" be the command?).  Similarly a Tcl
'namespace' filesystem could use 'exec' to evaluate code in the relevant
namespace (of course you could just use 'namespace eval' directly, but then
you couldn't hook the code up to input/output pipes).

Debugging virtual filesystems
-----------------------------

Bugs in Tcl vfs's are hard to track down, since error _messages_ can't
necessarily propagate to the toplevel (errors of course do propagate and
result in a filesystem action failing, but informative error messages cannot
usually be provided, since Tcl is only expecting one of the standard POSIX
error codes).  We could add a debugging command to this extension so
unexpected errors are logged somewhere.  Alternatively the 'reporting'
filesystem in Tcl's test suite can be used to aid debugging.

