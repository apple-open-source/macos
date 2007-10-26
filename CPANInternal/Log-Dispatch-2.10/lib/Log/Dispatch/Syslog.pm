package Log::Dispatch::Syslog;

use strict;

use Log::Dispatch::Output;

use base qw( Log::Dispatch::Output );

use Params::Validate qw(validate SCALAR);
Params::Validate::validation_options( allow_extra => 1 );

use Sys::Syslog ();

# This is old school!
require 'syslog.ph' if $] < 5.006;

use vars qw[ $VERSION ];

$VERSION = sprintf "%d.%02d", q$Revision: 1.18 $ =~ /: (\d+)\.(\d+)/;

1;

sub new
{
    my $proto = shift;
    my $class = ref $proto || $proto;

    my %p = @_;

    my $self = bless {}, $class;

    $self->_basic_init(%p);
    $self->_init(%p);

    return $self;
}

sub _init
{
    my $self = shift;

    my %p = validate( @_, { ident    => { type => SCALAR,
					  default => $0 },
			    logopt   => { type => SCALAR,
					  default => '' },
			    facility => { type => SCALAR,
					  default => 'user' },
			    socket   => { type => SCALAR,
					  default => 'unix' },
			  } );

    $self->{ident}    = $p{ident};
    $self->{logopt}   = $p{logopt};
    $self->{facility} = $p{facility};
    $self->{socket}   = $p{socket};

    $self->{priorities} = [ 'DEBUG',
			    'INFO',
			    'NOTICE',
			    'WARNING',
			    'ERR',
			    'CRIT',
			    'ALERT',
			    'EMERG' ];

    Sys::Syslog::setlogsock $self->{socket};
}

sub log_message
{
    my $self = shift;
    my %p = @_;

    my $pri = $self->_level_as_number($p{level});

    Sys::Syslog::openlog($self->{ident}, $self->{logopt}, $self->{facility});
    Sys::Syslog::syslog($self->{priorities}[$pri], '%s', $p{message});
    Sys::Syslog::closelog;
}

__END__

=head1 NAME

Log::Dispatch::Syslog - Object for logging to system log.

=head1 SYNOPSIS

  use Log::Dispatch::Syslog;

  my $file = Log::Dispatch::Syslog->new( name      => 'file1',
                                         min_level => 'info',
                                         ident     => 'Yadda yadda' );

  $file->log( level => 'emerg', message => "Time to die." );

=head1 DESCRIPTION

This module provides a simple object for sending messages to the
system log (via UNIX syslog calls).

=head1 METHODS

=over 4

=item * new(%p)

This method takes a hash of parameters.  The following options are
valid:

=over 8

=item * name ($)

The name of the object.  Required.

=item * min_level ($)

The minimum logging level this object will accept.  See the
Log::Dispatch documentation for more information.  Required.

=item * max_level ($)

The maximum logging level this obejct will accept.  See the
Log::Dispatch documentation for more information.  This is not
required.  By default the maximum is the highest possible level (which
means functionally that the object has no maximum).

=item * ident ($)

This string will be prepended to all messages in the system log.
Defaults to $0.

=item * logopt ($)

A string containing the log options (separated by any separator you
like).  Valid options are 'cons', 'pid', 'ndelay', and 'nowait'.  See
the openlog(3) and Sys::Syslog docs for more details.  I would suggest
not using 'cons' but instead using Log::Dispatch::Screen.  Defaults to
''.

=item * facility ($)

Specifies what type of program is doing the logging to the system log.
Valid options are 'auth', 'authpriv', 'cron', 'daemon', 'kern',
'local0' through 'local7', 'mail, 'news', 'syslog', 'user',
'uucp'.  Defaults to 'user'

=item * socket ($)

Tells what type of socket to use for sending syslog messages.  Valid
options are 'unix' or 'inet'.  Defaults to 'inet'.

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
