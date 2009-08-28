package Log::Dispatch::File;

use strict;
use warnings;

use Log::Dispatch::Output;

use base qw( Log::Dispatch::Output );

use Params::Validate qw(validate SCALAR BOOLEAN);
Params::Validate::validation_options( allow_extra => 1 );

our $VERSION = '1.22';

# Prevents death later on if IO::File can't export this constant.
*O_APPEND = \&APPEND unless defined &O_APPEND;

sub APPEND { 0 }


sub new
{
    my $proto = shift;
    my $class = ref $proto || $proto;

    my %p = @_;

    my $self = bless {}, $class;

    $self->_basic_init(%p);
    $self->_make_handle(%p);

    return $self;
}

sub _make_handle
{
    my $self = shift;

    my %p = validate( @_, { filename  => { type => SCALAR },
                            mode      => { type => SCALAR,
                                           default => '>' },
                            binmode   => { type => SCALAR,
                                           default => undef },
                            autoflush => { type => BOOLEAN,
                                           default => 1 },
                            close_after_write => { type => BOOLEAN,
                                                   default => 0 },
                            permissions => { type => SCALAR,
                                             optional => 1 },
                          } );

    $self->{filename}    = $p{filename};
    $self->{close}       = $p{close_after_write};
    $self->{permissions} = $p{permissions};
    $self->{binmode}     = $p{binmode};

    if ( $self->{close} )
    {
        $self->{mode} = '>>';
    }
    elsif ( exists $p{mode} &&
         defined $p{mode} &&
         ( $p{mode} =~ /^(?:>>|append)$/ ||
           ( $p{mode} =~ /^\d+$/ &&
             $p{mode} == O_APPEND() ) ) )
    {
        $self->{mode} = '>>';
    }
    else
    {
        $self->{mode} = '>';
    }

    $self->{autoflush} = $p{autoflush};

    $self->_open_file() unless $p{close_after_write};

}

sub _open_file
{
    my $self = shift;

    open my $fh, $self->{mode}, $self->{filename}
        or die "Cannot write to '$self->{filename}': $!";

    if ( $self->{autoflush} )
    {
        my $oldfh = select $fh; $| = 1; select $oldfh;
    }

    if ( $self->{permissions}
         && ! $self->{chmodded} )
    {
        my $current_mode = ( stat $self->{filename} )[2] & 07777;
        if ( $current_mode ne $self->{permissions} )
        {
            chmod $self->{permissions}, $self->{filename}
                or die "Cannot chmod $self->{filename} to $self->{permissions}: $!";
        }

        $self->{chmodded} = 1;
    }

    if ( $self->{binmode} )
    {
        binmode $fh, $self->{binmode};
    }

    $self->{fh} = $fh;
}

sub log_message
{
    my $self = shift;
    my %p = @_;

    my $fh;

    if ( $self->{close} )
    {
        $self->_open_file;
        $fh = $self->{fh};
        print $fh $p{message}
            or die "Cannot write to '$self->{filename}': $!";

        close $fh
            or die "Cannot close '$self->{filename}': $!";
    }
    else
    {
        $fh = $self->{fh};
        print $fh $p{message}
            or die "Cannot write to '$self->{filename}': $!";
    }
}

sub DESTROY
{
    my $self = shift;

    if ( $self->{fh} )
    {
        my $fh = $self->{fh};
        close $fh;
    }
}


1;

__END__

=head1 NAME

Log::Dispatch::File - Object for logging to files

=head1 SYNOPSIS

  use Log::Dispatch::File;

  my $file = Log::Dispatch::File->new( name      => 'file1',
                                       min_level => 'info',
                                       filename  => 'Somefile.log',
                                       mode      => 'append' );

  $file->log( level => 'emerg', message => "I've fallen and I can't get up\n" );

=head1 DESCRIPTION

This module provides a simple object for logging to files under the
Log::Dispatch::* system.

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

=item * filename ($)

The filename to be opened for writing.

=item * mode ($)

The mode the file should be opened with.  Valid options are 'write',
'>', 'append', '>>', or the relevant constants from Fcntl.  The
default is 'write'.

=item * binmode ($)

A layer name to be passed to binmode, like ":utf8" or ":raw".

=item * close_after_write ($)

Whether or not the file should be closed after each write.  This
defaults to false.

If this is true, then the mode will aways be append, so that the file
is not re-written for each new message.

=item * autoflush ($)

Whether or not the file should be autoflushed.  This defaults to true.

=item * permissions ($)

If the file does not already exist, the permissions that it should
be created with.  Optional.  The argument passed must be a valid
octal value, such as 0600 or the constants available from Fcntl, like
S_IRUSR|S_IWUSR.

See L<perlfunc/chmod> for more on potential traps when passing octal
values around.  Most importantly, remember that if you pass a string
that looks like an octal value, like this:

 my $mode = '0644';

Then the resulting file will end up with permissions like this:

 --w----r-T

which is probably not what you want.

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
