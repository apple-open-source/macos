package Log::Dispatch::Email::MIMELite;
{
  $Log::Dispatch::Email::MIMELite::VERSION = '2.34';
}

use strict;
use warnings;

use Log::Dispatch::Email;

use base qw( Log::Dispatch::Email );

use MIME::Lite;

sub send_email {
    my $self = shift;
    my %p    = @_;

    my %mail = (
        To      => ( join ',', @{ $self->{to} } ),
        Subject => $self->{subject},
        Type    => 'TEXT',
        Data    => $p{message},
    );

    $mail{From} = $self->{from} if defined $self->{from};

    local $?;
    unless ( MIME::Lite->new(%mail)->send ) {
        warn "Error sending mail with MIME::Lite";
    }
}

1;

# ABSTRACT: Subclass of Log::Dispatch::Email that uses the MIME::Lite module

__END__

=pod

=head1 NAME

Log::Dispatch::Email::MIMELite - Subclass of Log::Dispatch::Email that uses the MIME::Lite module

=head1 VERSION

version 2.34

=head1 SYNOPSIS

  use Log::Dispatch;

  my $log = Log::Dispatch->new(
      outputs => [
          [
              'Email::MIMELite',
              min_level => 'emerg',
              to        => [qw( foo@example.com bar@example.org )],
              subject   => 'Big error!'
          ]
      ],
  );

  $log->emerg("Something bad is happening");

=head1 DESCRIPTION

This is a subclass of L<Log::Dispatch::Email> that implements the
send_email method using the L<MIME::Lite> module.

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2011 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

=cut
