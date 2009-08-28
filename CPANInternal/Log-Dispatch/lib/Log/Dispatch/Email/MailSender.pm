package Log::Dispatch::Email::MailSender;

# By: Joseph Annino
# (c) 2002
# Licensed under the same terms as Perl
#

use strict;
use warnings;

use Log::Dispatch::Email;

use base qw( Log::Dispatch::Email );

use Mail::Sender ();

our $VERSION = '1.04';

sub new
{
    my $proto = shift;
    my $class = ref $proto || $proto;

    my %p = @_;

    my $smtp = delete $p{smtp} || 'localhost';

    my $self = $class->SUPER::new(%p);

    $self->{smtp} = $smtp;

    return $self;
}

sub send_email
{
    my $self = shift;
    my %p = @_;

    local $?;
    eval
    {
        my $sender =
            Mail::Sender->new( { from => $self->{from} || 'LogDispatch@foo.bar',
                                 replyto => $self->{from} || 'LogDispatch@foo.bar',
                                 to => ( join ',', @{ $self->{to} } ),
                                 subject => $self->{subject},
                                 smtp => $self->{smtp},
                               } );

        die "Error sending mail ($sender): $Mail::Sender::Error"
            unless ref $sender;

        ref $sender->MailMsg( { msg => $p{message} } )
            or die "Error sending mail: $Mail::Sender::Error";
    };

    warn $@ if $@ && warnings::enabled();
}


1;

__END__

=head1 NAME

Log::Dispatch::Email::MailSender - Subclass of Log::Dispatch::Email that uses the Mail::Sender module

=head1 SYNOPSIS

  use Log::Dispatch::Email::MailSender;

  my $email =
      Log::Dispatch::Email::MailSender->new
          ( name => 'email',
            min_level => 'emerg',
            to => [ qw( foo@bar.com bar@baz.org ) ],
            subject => 'Oh no!!!!!!!!!!!',
            smtp => 'mail.foo.bar' );

  $email->log( message => 'Something bad is happening', level => 'emerg' );

=head1 DESCRIPTION

This is a subclass of Log::Dispatch::Email that implements the
send_email method using the Mail::Sender module.

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

NOTE: The Mail::Sender module requires an address be passed to it to
set this in the mail it sends.  We pass in 'LogDispatch@foo.bar' as
the default.

=item * buffered (0 or 1)

This determines whether the object sends one email per message it is
given or whether it stores them up and sends them all at once.  The
default is to buffer messages.

=item * smtp ($)

A string containing the network address of the SMTP server to use for
sending the email.  This defaults to localhost.

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

=head1 AUTHORS

Joseph Annino. <jannino@jannino.com>

Dave Rolsky, <autarch@urth.org>

=cut
