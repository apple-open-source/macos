CatFish 1.9                                                      December 1999
==============================================================================


This is the source code release of CatFish, a fast and compact disk cataloger
based on the Metakit embedded database engine.

There are two directories in this archive: catfish and msvc152.

To start with that last one: CatFish 1.9 is a 16-bit Windows application,
with some serious trickery to make it use long filenames on Windows 96/98/NT.

To that end, CatFish has been created with MSVC 1.52 - this archive includes
the three project files required to rebuild it:

	msvc152\mk4wvms.mak	static MFC-based 16-bit version of Metakit
	msvc152\mk4wvmsd.mak	similar, but this is a debug build
	catfish\catfish.mak	the CatFish project, debug and release modes

The projects ought to build as is, if placed *inside* the "win" directory of
the Metakit 2.0 source distribution, see http://www.equi4.com/metakit/ for
full details on where to download this open source portable database library.

There are many interesting ways to take this popular utility (there have been
many tens of thousands of downloads of the original CatFish 1.8 release):

  1)  Convert the project to run natively on Windows 95/98/NT/2K.
      This is likely to result in a 3- to 5-fold performance improvement
      (which ought to be stunning, CatFish 1.9 is already very good).
      
  2)  Write a new version, perhaps in the Tcl/Tk scripting language and GUI.
      This could take full advantage of the multi-platform capability of
      Metakit, and make all CatFish catalog files useable on any platform.
      
  3)  You tell me...
  
If you are a software developer and are considering any of these steps, then
please get in touch - either by email to me, or by posting a note on one of
the Metakit discussion groups or on the Metakit Wiki (see the website).

Though I'm involved in other projects, I will definitely support anyone who
wishes to contribute - from all the feedback I have received, it is clear
that CatFish has become a standard utility for many people (me too, btw).
Extending the functionality further would be great news for many, I'm sure.

If you make changes to CatFish, you must remove the Equi4 Software logo, and
you must leave an acknowledgement with my name in the About box - please...
Details about the amazing qualities of Metakit are ok, but not required :)


LAST-MINUTE UPDATE, 20-12-1999
==============================

After the release of CatFish, it turns out that there was a bug causing it
to fail on Windows NT.  I've just updated the binary release, with a minor
code change.  If you intend to re-build CatFish yourself, please make the
following change - on line 420 of scandisk.cpp, change this:
    fp = WOWGetProcAddress32(_ghLib, "GetDiskFreeSpaceExA");
to: fp = WOWGetProcAddress32(_ghLib, "GetDriveTypeA");



LICENSE AND COPYRIGHT STATEMENT
===============================

Copyright (c) 1996-2000 Jean-Claude Wippler

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.


==============================================================================
-- Jean-Claude Wippler <jcw@equi4.com>
