package Log::Dispatch::ApacheLog;

use strict;
use warnings;

use Log::Dispatch::Output;

use base qw( Log::Dispatch::Output );

use Params::Validate qw(validate);
Params::Validate::validation_options( allow_extra => 1 );

our $VERSION = '1.09';


BEGIN
{
    if ( $ENV{MOD_PERL} && $ENV{MOD_PERL} =~ /2\./ )
    {
        require Apache2::Log;
    }
    else
    {
        require Apache::Log;
    }
}


sub new
{
    my $proto = shift;
    my $class = ref $proto || $proto;

    my %p = validate( @_, { apache => { can => 'log' } } );

    my $self = bless {}, $class;

    $self->_basic_init(%p);
    $self->{apache_log} = $p{apache}->log;

    return $self;
}

{
    my %methods =
        ( emergency => 'emerg',
          critical  => 'crit',
          warning   => 'warn',
        );
    sub log_message
    {
        my $self = shift;
        my %p = @_;

        my $level = $self->_level_as_name($p{level});

        my $method = $methods{$level} || $level;

        $self->{apache_log}->$method( $p{message} );
    }
}


1;

__END__

=head1 NAME

Log::Dispatch::ApacheLog - Object for logging to Apache::Log objects

=head1 SYNOPSIS

  use Log::Dispatch::ApacheLog;

  my $handle = Log::Dispatch::ApacheLog->new( name      => 'apache log',
                                              min_level => 'emerg',
                                              apache    => $r );

  $handle->log( level => 'emerg', message => 'Kaboom' );

=head1 DESCRIPTION

This module allows you to pass messages Apache's log object,
represented by the Apache::Log class.

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

=item * apache ($)

An object of either the Apache or Apache::Server classes.

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
