                     ====================================
                       Package "Carp::Clan" Version 5.3
                     ====================================


This package is available for download either from my web site at

                  http://www.engelschall.com/u/sb/download/

or from any CPAN (= "Comprehensive Perl Archive Network") mirror server:

               http://www.perl.com/CPAN/authors/id/S/ST/STBEY/


Abstract:
---------

This module reports errors from the perspective of the caller of a
"clan" of modules, similar to "Carp.pm" itself. But instead of giving
it a number of levels to skip on the calling stack, you give it a
pattern to characterize the package names of the "clan" of modules
which shall never be blamed for any error. :-)

So these modules stick together like a "clan" and any error which
occurs will be blamed on the "outsider" script or modules not belonging
to this "clan".


Legal issues:
-------------

This package with all its parts is

Copyright (c) 2001 - 2004 by Steffen Beyer.
All rights reserved.

This package is free software; you can use, modify and redistribute
it under the same terms as Perl itself, i.e., under the terms of
the "Artistic License" or the "GNU General Public License".

Please refer to the files "Artistic.txt" and "GNU_GPL.txt"
in this distribution, respectively, for details!


Prerequisites:
--------------

Perl version 5.005_03 or higher.

(Should probably also work with older versions of Perl, however.)


Installation:
-------------

As usual:

    perl Makefile.PL
    make
    make test
    make install


Changes over previous versions:
-------------------------------

Please refer to the file "CHANGES.txt" in this distribution for a more
detailed version history log.


Documentation:
--------------

The documentation of this package is included in POD format (= "Plain
Old Documentation") in the file with the extension ".pod" in this
distribution, the human-readable markup-language standard for Perl
documentation.

By building this package, this documentation will automatically be
converted into a man page, which will automatically be installed in
your Perl tree for further reference through the installation process,
where it can be accessed by the commands "man Carp::Clan" (Unix)
and "perldoc Carp::Clan" (Unix and Win32 alike), for example.


What does it do:
----------------

This module is based on "Carp.pm" from Perl 5.005_03. It
has been modified to skip all package names matching the
pattern given in the "use" statement inside the "qw()"
term (or argument list).

Suppose you have a family of modules or classes named
"Pack::A", "Pack::B" and so on, and each of them uses
"Carp::Clan qw(^Pack::);" (or at least the one in which
the error or warning gets raised).

Thus when for example your script "tool.pl" calls module
"Pack::A", and module "Pack::A" calls module "Pack::B", an
exception raised in module "Pack::B" will appear to have
originated in "tool.pl" where "Pack::A" was called, and
not in "Pack::A" where "Pack::B" was called, as the
unmodified "Carp.pm" would try to make you believe :-).

This works similarly if "Pack::B" calls "Pack::C" where
the exception is raised, etcetera.

In other words, this blames all errors in the "Pack::*"
modules on the user of these modules, i.e., on you. ;-)

The skipping of a clan (or family) of packages according
to a pattern describing its members is necessary in cases
where these modules are not classes derived from each
other (and thus when examining "@ISA" - as in the original
"Carp.pm" module - doesn't help).

The purpose and advantage of this is that a "clan" of
modules can work together (and call each other) and throw
exceptions at various depths down the calling hierarchy
and still appear as a monolithic block (as though they
were a single module) from the perspective of the caller.

In case you just want to ward off all error messages from
the module in which you "use Carp::Clan", i.e., if you
want to make all error messages or warnings to appear to
originate from where your module was called (this is what
you usually used to "use Carp;" for ;-) ), instead of
in your module itself (which is what you can do with a
"die" or "warn" anyway), you do not need to provide a
pattern, the module will automatically provide the correct
one for you.

I.e., just "use Carp::Clan;" without any arguments and
call "carp" or "croak" as appropriate, and they will
automatically defend your module against all blames!

In other words, a pattern is only necessary if you want to
make several modules (more than one) work together and
appear as though they were only one.


Author's note:
--------------

If you have any questions, suggestions or need any assistance, please
let me know!

Please do send feedback, this is essential for improving this module
according to your needs!

I hope you will find this module useful. Enjoy!

Yours,
--
  Steffen Beyer <sb@engelschall.com> http://www.engelschall.com/u/sb/
  "There is enough for the need of everyone in this world, but not
   for the greed of everyone." - Mohandas Karamchand "Mahatma" Gandhi
