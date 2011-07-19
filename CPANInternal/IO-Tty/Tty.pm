# Documentation at the __END__
# -*-cperl-*-

package IO::Tty;

use IO::Handle;
use IO::File;
use IO::Tty::Constant;
use Carp;

require POSIX;
require DynaLoader;

use vars qw(@ISA $VERSION $XS_VERSION $CONFIG $DEBUG);

$VERSION = 1.08;
$XS_VERSION = "1.08";
@ISA = qw(IO::Handle);

eval { local $^W = 0; undef local $SIG{__DIE__}; require IO::Stty };
push @ISA, "IO::Stty" if (not $@);  # if IO::Stty is installed

BOOT_XS: {
    # If I inherit DynaLoader then I inherit AutoLoader and I DON'T WANT TO
    require DynaLoader;

    # DynaLoader calls dl_load_flags as a static method.
    *dl_load_flags = DynaLoader->can('dl_load_flags');

    do {
	defined(&bootstrap)
		? \&bootstrap
		: \&DynaLoader::bootstrap
    }->(__PACKAGE__);
}

sub import {
    IO::Tty::Constant->export_to_level(1, @_);
}

sub open {
    my($tty,$dev,$mode) = @_;

    IO::File::open($tty,$dev,$mode) or
	return undef;

    $tty->autoflush;

    1;
}

sub clone_winsize_from {
  my ($self, $fh) = @_;
  croak "Given filehandle is not a tty in clone_winsize_from, called"
    if not POSIX::isatty($fh);  
  return 1 if not POSIX::isatty($self);  # ignored for master ptys
  my $winsize = " "x1024; # preallocate memory
  ioctl($fh, &IO::Tty::Constant::TIOCGWINSZ, $winsize)
    and ioctl($self, &IO::Tty::Constant::TIOCSWINSZ, $winsize)
      and return 1;
  warn "clone_winsize_from: error: $!" if $^W;
  return undef;
}

sub set_raw($) {
  require POSIX;
  my $self = shift;
  return 1 if not POSIX::isatty($self);
  my $ttyno = fileno($self);
  my $termios = new POSIX::Termios;
  unless ($termios) {
    warn "set_raw: new POSIX::Termios failed: $!";
    return undef;
  }
  unless ($termios->getattr($ttyno)) {
    warn "set_raw: getattr($ttyno) failed: $!";
    return undef;
  }
  $termios->setiflag(0);
  $termios->setoflag(0);
  $termios->setlflag(0);
  $termios->setcc(&POSIX::VMIN, 1);
  $termios->setcc(&POSIX::VTIME, 0);
  unless ($termios->setattr($ttyno, &POSIX::TCSANOW)) {
    warn "set_raw: setattr($ttyno) failed: $!";
    return undef;
  }
  return 1;
}


1;

__END__

=head1 NAME

IO::Tty - Low-level allocate a pseudo-Tty, import constants.

=head1 VERSION

1.08

=head1 SYNOPSIS

    use IO::Tty qw(TIOCNOTTY);
    ...
    # use only to import constants, see IO::Pty to create ptys.

=head1 DESCRIPTION

C<IO::Tty> is used internally by C<IO::Pty> to create a pseudo-tty.
You wouldn't want to use it directly except to import constants, use
C<IO::Pty>.  For a list of importable constants, see
L<IO::Tty::Constant>.

Windows is now supported, but ONLY under the Cygwin
environment, see L<http://sources.redhat.com/cygwin/>.

Please note that pty creation is very system-dependend.  From my
experience, any modern POSIX system should be fine.  Find below a list
of systems that C<IO::Tty> should work on.  A more detailed table
(which is slowly getting out-of-date) is available from the project
pages document manager at SourceForge
L<http://sourceforge.net/projects/expectperl/>.

If you have problems on your system and your system is listed in the
"verified" list, you probably have some non-standard setup, e.g. you
compiled your Linux-kernel yourself and disabled ptys (bummer!).
Please ask your friendly sysadmin for help.

If your system is not listed, unpack the latest version of C<IO::Tty>,
do a C<'perl Makefile.PL; make; make test; uname -a'> and send me
(F<RGiersig@cpan.org>) the results and I'll see what I can deduce from
that.  There are chances that it will work right out-of-the-box...

If it's working on your system, please send me a short note with
details (version number, distribution, etc. 'uname -a' and 'perl -V'
is a good start; also, the output from "perl Makefile.PL" contains a
lot of interesting info, so please include that as well) so I can get
an overview.  Thanks!


=head1 VERIFIED SYSTEMS, KNOWN ISSUES

This is a list of systems that C<IO::Tty> seems to work on ('make
test' passes) with comments about "features":

=over 4

=item * AIX 4.3

Returns EIO instead of EOF when the slave is closed.  Benign.

=item * AIX 5.x

=item * FreeBSD 4.4

EOF on the slave tty is not reported back to the master.

=item * OpenBSD 2.8

The ioctl TIOCSCTTY sometimes fails.  This is also known in
Tcl/Expect, see http://expect.nist.gov/FAQ.html

EOF on the slave tty is not reported back to the master.

=item * Darwin 7.9.0

=item * HPUX 10.20 & 11.00

EOF on the slave tty is not reported back to the master.

=item * IRIX 6.5

=item * Linux 2.2.x & 2.4.x

Returns EIO instead of EOF when the slave is closed.  Benign.

=item * OSF 4.0

EOF on the slave tty is not reported back to the master.

=item * Solaris 8, 2.7, 2.6

Has the "feature" of returning EOF just once?!

EOF on the slave tty is not reported back to the master.

=item * Windows NT/2k/XP (under Cygwin)

When you send (print) a too long line (>160 chars) to a non-raw pty,
the call just hangs forever and even alarm() cannot get you out.
Don't complain to me...

EOF on the slave tty is not reported back to the master.

=item * z/OS

=back

The following systems have not been verified yet for this version, but
a previous version worked on them:

=over 4

=item * SCO Unix

=item * NetBSD

probably the same as the other *BSDs...

=back

If you have additions to these lists, please mail them to
E<lt>F<RGiersig@cpan.org>E<gt>.


=head1 SEE ALSO

L<IO::Pty>, L<IO::Tty::Constant>


=head1 MAILING LISTS

As this module is mainly used by Expect, support for it is available
via the two Expect mailing lists, expectperl-announce and
expectperl-discuss, at

  http://lists.sourceforge.net/lists/listinfo/expectperl-announce

and

  http://lists.sourceforge.net/lists/listinfo/expectperl-discuss


=head1 AUTHORS

Originally by Graham Barr E<lt>F<gbarr@pobox.com>E<gt>, based on the
Ptty module by Nick Ing-Simmons E<lt>F<nik@tiuk.ti.com>E<gt>.

Now maintained and heavily rewritten by Roland Giersig
E<lt>F<RGiersig@cpan.org>E<gt>.

Contains copyrighted stuff from openssh v3.0p1, authored by Tatu
Ylonen <ylo@cs.hut.fi>, Markus Friedl and Todd C. Miller
<Todd.Miller@courtesan.com>.  I also got a lot of inspiry from the pty
code in Xemacs.


=head1 COPYRIGHT

Now all code is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

Nevertheless the above AUTHORS retain their copyrights to the various
parts and want to receive credit if their source code is used.
See the source for details.


=head1 DISCLAIMER

THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

In other words: Use at your own risk.  Provided as is.  Your mileage
may vary.  Read the source, Luke!

And finally, just to be sure:

Any Use of This Product, in Any Manner Whatsoever, Will Increase the
Amount of Disorder in the Universe. Although No Liability Is Implied
Herein, the Consumer Is Warned That This Process Will Ultimately Lead
to the Heat Death of the Universe.

=cut
