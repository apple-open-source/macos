package Log::Dispatch::Email::MailSend;

use strict;
use warnings;

use Log::Dispatch::Email;

use base qw( Log::Dispatch::Email );

use Mail::Send;

our $VERSION = '1.19';

sub send_email
{
    my $self = shift;
    my %p = @_;

    my $msg = Mail::Send->new;

    $msg->to( join ',', @{ $self->{to} } );
    $msg->subject( $self->{subject} );

    # Does this ever work for this module?
    $msg->set('From', $self->{from}) if $self->{from};

    local $?;
    eval
    {
        my $fh = $msg->open
            or die "Cannot open handle to mail program";

        $fh->print( $p{message} )
            or die "Cannot print message to mail program handle";

        $fh->close
            or die "Cannot close handle to mail program";
    };

    warn $@ if $@ && warnings::enabled();
}


1;

__END__

=head1 NAME

Log::Dispatch::Email::MailSend - Subclass of Log::Dispatch::Email that uses the Mail::Send module

=head1 SYNOPSIS

  use Log::Dispatch::Email::MailSend;

  my $email =
      Log::Dispatch::Email::MailSend->new
          ( name => 'email',
            min_level => 'emerg',
            to => [ qw( foo@bar.com bar@baz.org ) ],
            subject => 'Oh no!!!!!!!!!!!', );

  $email->log( message => 'Something bad is happening', level => 'emerg' );

=head1 DESCRIPTION

This is a subclass of Log::Dispatch::Email that implements the
send_email method using the Mail::Send module.

=head1 METHODS

=over 4

=item * new

This method takes a hash of parameters.  The following options are
valid:

=over 8

=item * name ($)

The name of the object (not the filename!).  Required.

=item * min_level ($)

The minimum logging level this object will accept.  See the
Log::Dispatch documentation on L<Log Levels|Log::Dispatch/"Log Levels"> for more information.  Required.

=item * max_level ($)

The maximum logging level this obejct will accept.  See the
Log::Dispatch documentation on L<Log Levels|Log::Dispatch/"Log Levels"> for more information.  This is not
required.  By default the maximum is the highest possible level (which
means functionally that the object has no maximum).

=item * subject ($)

The subject of the email messages which are sent.  Defaults to "$0:
log email"

=item * to ($ or \@)

Either a string or a list reference of strings containing email
addresses.  Required.

=item * from ($)

A string containing an email address.  This is optional and may not
work with all mail sending methods.

=item * buffered (0 or 1)

This determines whether the object sends one email per message it is
given or whether it stores them up and sends them all at once.  The
default is to buffer messages.

=item * callbacks( \& or [ \&, \&, ... ] )

This parameter may be a single subroutine reference or an array
reference of subroutine references.  These callbacks will be called in
the order they are given and passed a hash containing the following keys:

 ( message => $log_message, level => $log_level )

The callbacks are expected to modify the message and then return a
single scalar containing that modified message.  These callbacks will
be called when either the C<log> or C<log_to> methods are called and
will only be applied to a given message once.

=back

=item * log_message( level => $, message => $ )

Sends a message if the level is greater than or equal to the object's
minimum level.

=back

=head1 CHANGING HOW MAIL IS SENT

Since C<Mail::Send> is a subclass of C<Mail::Mailer>, you can change
how mail is sent from this module by simply C<use>ing C<Mail::Mailer>
in your code before mail is sent.  For example, to send mail via smtp,
you could do:

  use Mail::Mailer 'smtp', Server => 'foo.example.com';

For more details, see the C<Mail::Mailer> docs.

=head1 AUTHOR

Dave Rolsky, <autarch@urth.org>

=head1 SEE ALSO


Log::Dispatch, Log::Dispatch::ApacheLog, Log::Dispatch::Email,
Log::Dispatch::Email::MailSendmail, Log::Dispatch::Email::MIMELite,
Log::Dispatch::File, Log::Dispatch::Handle, Log::Dispatch::Output,
Log::Dispatch::Screen, Log::Dispatch::Syslog

=cut
