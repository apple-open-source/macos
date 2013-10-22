package Log::Dispatch::Screen;
{
  $Log::Dispatch::Screen::VERSION = '2.34';
}

use strict;
use warnings;

use Log::Dispatch::Output;

use base qw( Log::Dispatch::Output );

use Params::Validate qw(validate BOOLEAN);
Params::Validate::validation_options( allow_extra => 1 );

sub new {
    my $proto = shift;
    my $class = ref $proto || $proto;

    my %p = validate(
        @_, {
            stderr => {
                type    => BOOLEAN,
                default => 1
            },
        }
    );

    my $self = bless {}, $class;

    $self->_basic_init(%p);
    $self->{stderr} = exists $p{stderr} ? $p{stderr} : 1;

    return $self;
}

sub log_message {
    my $self = shift;
    my %p    = @_;

    if ( $self->{stderr} ) {
        print STDERR $p{message};
    }
    else {
        print STDOUT $p{message};
    }
}

1;

# ABSTRACT: Object for logging to the screen

__END__

=pod

=head1 NAME

Log::Dispatch::Screen - Object for logging to the screen

=head1 VERSION

version 2.34

=head1 SYNOPSIS

  use Log::Dispatch;

  my $log = Log::Dispatch->new(
      outputs => [
          [
              'Screen',
              min_level => 'debug',
              stderr    => 1,
              newline   => 1
          ]
      ],
  );

  $log->alert("I'm searching the city for sci-fi wasabi");

=head1 DESCRIPTION

This module provides an object for logging to the screen (really
STDOUT or STDERR).

Note that a newline will I<not> be added automatically at the end of a
message by default.  To do that, pass C<newline =E<gt> 1>.

=head1 CONSTRUCTOR

The constructor takes the following parameters in addition to the standard
parameters documented in L<Log::Dispatch::Output>:

=over 4

=item * stderr (0 or 1)

Indicates whether or not logging information should go to STDERR.  If
false, logging information is printed to STDOUT instead.  This
defaults to true.

=back

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2011 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

=cut
