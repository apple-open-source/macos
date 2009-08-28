package Log::Dispatch::Screen;

use strict;
use warnings;

use Log::Dispatch::Output;

use base qw( Log::Dispatch::Output );

use Params::Validate qw(validate BOOLEAN);
Params::Validate::validation_options( allow_extra => 1 );

our $VERSION = '1.17';


sub new
{
    my $proto = shift;
    my $class = ref $proto || $proto;

    my %p = validate( @_, { stderr => { type => BOOLEAN,
                                        default => 1 },
                          } );

    my $self = bless {}, $class;

    $self->_basic_init(%p);
    $self->{stderr} = exists $p{stderr} ? $p{stderr} : 1;

    return $self;
}

sub log_message
{
    my $self = shift;
    my %p = @_;

    if ($self->{stderr})
    {
        print STDERR $p{message};
    }
    else
    {
        print STDOUT $p{message};
    }
}


1;

__END__

=head1 NAME

Log::Dispatch::Screen - Object for logging to the screen

=head1 SYNOPSIS

  use Log::Dispatch::Screen;

  my $screen = Log::Dispatch::Screen->new( name      => 'screen',
                                           min_level => 'debug',
                                           stderr    => 1 );

  $screen->log( level => 'alert', message => "I'm searching the city for sci-fi wasabi\n" );

=head1 DESCRIPTION

This module provides an object for logging to the screen (really
STDOUT or STDERR).

=head1 METHODS

=over 4

=item * new(%p)

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

=item * stderr (0 or 1)

Indicates whether or not logging information should go to STDERR.  If
false, logging information is printed to STDOUT instead.  This
defaults to true.

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

=item * log_message( message => $ )

Sends a message to the appropriate output.  Generally this shouldn't
be called directly but should be called through the C<log()> method
(in Log::Dispatch::Output).

=back

=head1 AUTHOR

Dave Rolsky, <autarch@urth.org>

=cut
