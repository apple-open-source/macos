=head1 NAME

Perl4::CoreLibs - libraries historically supplied with Perl 4

=head1 DESCRIPTION

This is a collection of C<.pl> files that have historically been
bundled with the Perl core but are planned not to be so distributed
with core version 5.15 or later.  Relying on their presence in the core
distribution is deprecated; they should be acquired from this CPAN
distribution instead.  From core version 5.13, until their removal,
it is planned that the core versions of these libraries will emit a
warning when loaded.  The CPAN version will not emit such a warning.

The entire Perl 4 approach to libraries was largely superseded in Perl
5.000 by the system of module namespaces and C<.pm> files.  Most of
the libraries in this collection predate Perl 5.000, but a handful were
first introduced in that version.  Functionally, most have been directly
superseded by modules in the Perl 5 style.  These libraries should not
be used by new code.  This collection exists to support old Perl programs
that predates satisfactory replacements.

Most of these libraries have not been substantially maintained in the
course of Perl 5 development.  They are now very antiquated in style,
making no use of the language facilities introduced since Perl 4.
They should therefore not be used as programming examples.

=head1 LIBRARIES

The libraries in this collection are:

=over

=item abbrev.pl

Build a dictionary of unambiguous abbreviations for a group of words.
Prefer L<Text::Abbrev>.

=item assert.pl

Assertion checking with stack trace upon assertion failure.

=item bigfloat.pl

Arbitrary precision decimal floating point arithmetic.
Prefer L<Math::BigFloat>.

=item bigint.pl

Arbitrary precision integer arithmetic.
Prefer L<Math::BigInt>.

=item bigrat.pl

Arbitrary precision rational arithmetic.
Prefer L<Math::BigRat>.

=item cacheout.pl

Manage output to a large number of files to avoid running out of file
descriptors.

=item chat2.pl

Framework for partial automation of communication with a remote process
over IP.
Prefer L<IO::Socket::INET>.

=item complete.pl

Interactive line input with word completion.
Prefer L<Term::Complete>.

=item ctime.pl

One form of textual representation of time.
Prefer C<scalar(localtime())> or L<POSIX/ctime>.

=item dotsh.pl

Inhale shell variables set by a shell script.

=item exceptions.pl

String-based exception handling built on C<eval> and C<die>.
Prefer L<Try::Tiny> or L<TryCatch>.

=item fastcwd.pl

Determine current directory.
Prefer L<Cwd>.

=item find.pl

Historical interface for a way of searching for files.
Prefer L<File::Find>.

=item finddepth.pl

Historical interface for a way of searching for files.
Prefer L<File::Find>.

=item flush.pl

Flush an I/O handle's output buffer.
Prefer L<IO::Handle/flush>.

=item ftp.pl

File Transfer Protocol (FTP) over IP.
Prefer L<Net::FTP>.

=item getcwd.pl

Determine current directory.
Prefer L<Cwd>.

=item getopt.pl

Unix-like option processing with all option taking arguments.
Prefer L<Getopt::Std>.

=item getopts.pl

Full Unix-like option processing.
Prefer L<Getopt::Std>.

=item hostname.pl

Determine host's hostname.
Prefer L<Sys::Hostname>.

=item importenv.pl

Import environment variables as Perl package variables.

=item look.pl

Data-based seek within regular file.

=item newgetopt.pl

GNU-like option processing.
Prefer L<Getopt::Long>.

=item open2.pl

Open a subprocess for both reading and writing.
Prefer L<IPC::Open2>.

=item open3.pl

Open a subprocess for reading, writing, and error handling.
Prefer L<IPC::Open3>.

=item pwd.pl

Track changes of current directory in C<$ENV{PWD}>.

=item shellwords.pl

Interpret shell quoting.
Prefer L<Text::ParseWords>.

=item stat.pl

Access fields of a L<stat|perldoc/stat> structure by name.
Prefer L<File::stat>.

=item syslog.pl

Write to Unix system log.
Prefer L<Sys::Syslog>.

=item tainted.pl

Determine whether data is tainted.
Prefer L<Taint::Util>.

=item termcap.pl

Generate escape sequences to control arbitrary terminal.
Prefer L<Term::Cap>.

=item timelocal.pl

Generate time number from broken-down time.
Prefer L<Time::Local>.

=item validate.pl

Check permissions on a group of files.

=back

=cut

package Perl4::CoreLibs;

{ use 5.006; }
use warnings;
use strict;

our $VERSION = "0.003";

=head1 AUTHOR

Known contributing authors for the libraries in this package are
Brandon S. Allbery, John Bazik, Tom Christiansen <tchrist@convex.com>,
Charles Collins, Joe Doupnik <JRD@CC.USU.EDU>, Marion Hakanson
<hakanson@cse.ogi.edu>, Waldemar Kebsch <kebsch.pad@nixpbe.UUCP>,
Lee McLoughlin <lmjm@doc.ic.ac.uk>, <A.Macpherson@bnr.co.uk>, Randal
L. Schwartz <merlyn@stonehenge.com>, Aaron Sherman <asherman@fmrco.com>,
Wayne Thompson, Larry Wall <lwall@jpl-devvax.jpl.nasa.gov>, and Ilya
Zakharevich.  (Most of these email addresses are probably out of date.)

Known contributing authors for the tests in this package are Tom
Christiansen <tchrist@convex.com>, Alexandr Ciornii (alexchorny at
gmail.com), Marc Horowitz <marc@mit.edu>, Dave Rolsky <autarch@urth.org>,
and David Sundstrom <sunds@asictest.sc.ti.com>.

Andrew Main (Zefram) <zefram@fysh.org> built the Perl4::CoreLibs package.

=head1 COPYRIGHT

Copyright (C) 1987-2009 Larry Wall et al

Copyright (C) 2010, 2011 Andrew Main (Zefram) <zefram@fysh.org>

=head1 LICENSE

This module is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=cut

1;
