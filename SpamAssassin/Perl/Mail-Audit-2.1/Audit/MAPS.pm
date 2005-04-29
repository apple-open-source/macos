package Mail::Audit::MAPS;
use Mail::Audit;
use vars q(@VERSION);
$VERSION = '1.8';
$host          = '.blackholes.mail-abuse.org';
1;

package Mail::Audit;

use strict;
use Net::SMTP;
use Mail::Internet;
use Sys::Hostname;

sub myALRM { die "alarm\n" }
sub rblcheck {
my ($self, $timeout) = (shift, shift);
_log(1,"Performing RBL check");
my @recieved      = $self->received;
my $rcvcount      = 0;
$timeout = 10 unless defined $timeout;

# Catch ALRM signals so we can timeout DNS lookups
$SIG{ALRM} = 'myALRM';
&myALRM() if 0;              # make -w shut up
for (@recieved) {
    my $x = _checkit($rcvcount,$_,$timeout);
    if ($x) {
        _log(2, "Check returned $x after ".(1+$rcvcount)." recieved headers");
        return $x
    }
    $rcvcount++;  # Any further Received lines won't be the first.
}
_log(2, "Check was fine");
return '';
}

sub _checkit {
    my $OK            = '';
    my $InvalidIP     = '1 Invalid IP address ';
    my $RcvBlackHole  = '2 Received from RBL-registered spam site ';
    my $RlyBlackHole  = '3 Relayed through RBL-registered spam site ';

   my($relay,$rcvd,$timeout) = @_;
   my($IP,@IP) = $rcvd =~ /\[((\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3}))\]/;
   my($name,$x);
   # We can't complain if there's no IP address in this Received header.
   return ($OK) unless defined $IP;
   # Outer limits lose
   return ($InvalidIP.$IP) if $IP eq '0.0.0.0';
   return ($InvalidIP.$IP) if $IP eq '255.255.255.255';
   # All @IP components must be >= 0 and <= 255
   foreach $x ( @IP ) {
      return ($InvalidIP.$IP) if $x > 255;
      return ($InvalidIP.$IP) if $x =~ /^0\d/;    # no leading zeroes allowed
   }
   #
   # Wrap the gethostbyname call with eval in case it times out.
   #
   eval {
      alarm($timeout);
      ($name) = gethostbyname(join('.',reverse @IP) . $Mail::Audit::MAPS::host);
      alarm(0);
   };
   return($OK) if $@ =~ /^alarm/;  # Timed out.  Let it through.
   return($OK) unless $name;       # If it's ok with MAPS, it's OK with us.
   return($relay ? $RlyBlackHole.$IP : $RcvBlackHole.$IP);
}


1;
__END__

=pod

=head1 NAME

Mail::Audit::MAPS - Mail::Audit plugin for RBL checking

=head1 SYNOPSIS

    use Mail::Audit qw(MAPS);
	my $mail = Mail::Audit->new;
    ...
	if ($mail->rblcheck) {
        ...
    }

=head1 DESCRIPTION

This is a Mail::Audit plugin which provides a method for checking
messages against the Relay Black List.

=head2 METHODS

=over 4

=item C<rblcheck([$timeout])>

Attempts to check the mail headers with the Relay Blackhole List. 
Returns false if the headers check out fine or the query times out,
returns a reason if the mail is considered spam.

=head1 AUTHOR

Simon Cozens <simon@cpan.org>

=head1 SEE ALSO

L<Mail::Audit>
