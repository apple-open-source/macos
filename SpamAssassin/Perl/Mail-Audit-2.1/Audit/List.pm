package Mail::Audit::List;
use Mail::Audit;
use vars q(@VERSION);
$VERSION = '1.8';
1;

package Mail::Audit;

use strict;
use Mail::ListDetector;
my $DEFAULT_DIR = $ENV{HOME} . "/mail";

sub list_accept {
  my $self = shift;
  my $dir  = shift;
  $dir ||= $DEFAULT_DIR;
  my $list = new Mail::ListDetector($self->{obj});

  if (!(defined $list)) {
    return 0;
  } else {
    my $name = $list->listname;
    $name =~ tr/A-Za-z0-9_-//dc;
    my $deliver_filename = join '/', $dir, $name;
    $self->accept($deliver_filename);
    return $deliver_filename;
  }
}

1;
__END__

=pod

=head1 NAME

Mail::Audit::List - Mail::Audit plugin for automatic list delivery

=head1 SYNOPSIS

    use Mail::Audit qw(List);
	my $mail = Mail::Audit->new;
    ...
        $mail->list_accept || $main->accept;

=head1 DESCRIPTION

This is a Mail::Audit plugin which provides a method for automatically
delivering mailing lists to a suitable mainbox. It requires the CPAN
C<Mail::ListDetector> module.

=head2 METHODS

=over 4

=item C<list_accept([$delivery_dir])>

Attempts to deliver the message as a mailing list. It will place each 
message in C<$deliver_dir/$list_name>. The default value of C<$deliver_dir>
is C<$ENV{HOME} . "/mail">.

For instance, mail to C<perl5-porters@perl.org> will end up by default in
F</home/you/mail/perl5-porters>. 

Calls C<accept> and returns the filename delivered to if
C<Mail::ListDetector> can identify this mail as coming from a mailing
list, or 0 otherwise. 

The recipe given above should be able to replace a great number of
special-casing recipes.

=head1 AUTHOR

Michael Stevens <michael@etla.org>

=head1 SEE ALSO

L<Mail::Audit>
