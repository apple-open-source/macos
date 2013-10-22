package Log::Dispatch::Email::MailSender;
{
  $Log::Dispatch::Email::MailSender::VERSION = '2.34';
}

# By: Joseph Annino
# (c) 2002
# Licensed under the same terms as Perl
#

use strict;
use warnings;

use Log::Dispatch::Email;

use base qw( Log::Dispatch::Email );

use Mail::Sender ();

sub new {
    my $proto = shift;
    my $class = ref $proto || $proto;

    my %p = @_;

    my $smtp = delete $p{smtp} || 'localhost';

    my $self = $class->SUPER::new(%p);

    $self->{smtp} = $smtp;

    return $self;
}

sub send_email {
    my $self = shift;
    my %p    = @_;

    local $?;
    eval {
        my $sender = Mail::Sender->new(
            {
                from    => $self->{from} || 'LogDispatch@foo.bar',
                replyto => $self->{from} || 'LogDispatch@foo.bar',
                to      => ( join ',', @{ $self->{to} } ),
                subject => $self->{subject},
                smtp    => $self->{smtp},
            }
        );

        die "Error sending mail ($sender): $Mail::Sender::Error"
            unless ref $sender;

        ref $sender->MailMsg( { msg => $p{message} } )
            or die "Error sending mail: $Mail::Sender::Error";
    };

    warn $@ if $@;
}

1;

# ABSTRACT: Subclass of Log::Dispatch::Email that uses the Mail::Sender module

__END__

=pod

=head1 NAME

Log::Dispatch::Email::MailSender - Subclass of Log::Dispatch::Email that uses the Mail::Sender module

=head1 VERSION

version 2.34

=head1 SYNOPSIS

  use Log::Dispatch;

  my $log = Log::Dispatch->new(
      outputs => [
          [
              'Email::MailSender',
              min_level => 'emerg',
              to        => [qw( foo@example.com bar@example.org )],
              subject   => 'Big error!'
          ]
      ],
  );

  $log->emerg("Something bad is happening");

=head1 DESCRIPTION

This is a subclass of L<Log::Dispatch::Email> that implements the send_email
method using the L<Mail::Sender> module.

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2011 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

=cut
