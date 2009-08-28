package DBIx::Class::StartupCheck;

BEGIN {

    { package TestRHBug; use overload bool => sub { 0 } }

    sub _has_bug_34925 {
	my %thing;
	my $r1 = \%thing;
	my $r2 = \%thing;
	bless $r1 => 'TestRHBug';
	return !!$r2;
    }

    sub _possibly_has_bad_overload_performance {
	return $] < 5.008009 && ! _has_bug_34925();
    }

    unless ($ENV{DBIC_NO_WARN_BAD_PERL}) {
	if (_possibly_has_bad_overload_performance()) {
	    print STDERR "\n\nWARNING: " . __PACKAGE__ . ": This version of Perl is likely to exhibit\n" .
		"extremely slow performance for certain critical operations.\n" .
		"Please consider recompiling Perl.  For more information, see\n" .
		"https://bugzilla.redhat.com/show_bug.cgi?id=196836 and/or\n" .
		"http://lists.scsys.co.uk/pipermail/dbix-class/2007-October/005119.html.\n" .
		"You can suppress this message by setting DBIC_NO_WARN_BAD_PERL=1 in your\n" .
		"environment.\n\n";
	}
    }
}

=head1 NAME

DBIx::Class::StartupCheck - Run environment checks on startup

=head1 SYNOPSIS

  use DBIx::Class::StartupCheck;
  
=head1 DESCRIPTION

Currently this module checks for, and if necessary issues a warning for, a
particular bug found on RedHat systems from perl-5.8.8-10 and up.  Other checks
may be added from time to time.

Any checks herein can be disabled by setting an appropriate environment
variable.  If your system suffers from a particular bug, you will get a warning
message on startup sent to STDERR, explaining what to do about it and how to
suppress the message.  If you don't see any messages, you have nothing to worry
about.

=head1 CONTRIBUTORS

Nigel Metheringham

Brandon Black

Matt S. Trout

=head1 AUTHOR

Jon Schutz

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut

1;
