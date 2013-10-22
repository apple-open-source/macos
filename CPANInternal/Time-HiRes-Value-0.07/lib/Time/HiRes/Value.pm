#  You may distribute under the terms of either the GNU General Public License
#  or the Artistic License (the same terms as Perl itself)
#
#  (C) Paul Evans, 2006,2007,2009 -- leonerd@leonerd.org.uk

package Time::HiRes::Value;

use strict;
use warnings;

use Carp;

use Time::HiRes qw( gettimeofday );
use POSIX qw( floor );

our $VERSION = '0.07';

# Since we use this number quite a lot, make a constant out of it to avoid
# typoes
use constant USEC => 1_000_000;

=head1 NAME

C<Time::HiRes::Value> - a class representing a time value or interval in exact
microseconds

=head1 DESCRIPTION

The C<Time::HiRes> module allows perl to access the system's clock to
microsecond accuracy. However, floating point numbers are not suitable for
manipulating such time values, as rounding errors creep in to calculations
performed on floating-point representations of UNIX time. This class provides
a solution to this problem, by storing the seconds and miliseconds in separate
integer values, in an array. In this way, the value can remain exact, and no
rounding errors result.

=cut

# Internal helpers
sub _split_sec_usec($)
{
   my ( $t ) = @_;

   my $negative = 0;
   if( $t =~ s/^-// ) {
      $negative = 1;
   }

   my ( $sec, $usec );

   # Try not to use floating point maths because that loses too much precision
   if( $t =~ m/^(\d+)\.(\d+)$/ ) {
      $sec  = $1;
      $usec = $2;

      # Pad out to 6 digits
      $usec .= "0" x ( 6 - length( $usec ) );
   }
   elsif( $t =~ m/^(\d+)$/ ) {
      # Plain integer
      $sec  = $1;
      $usec = 0;
   }
   else {
      croak "Cannot convert string '$t' into a " . __PACKAGE__;
   }

   if( $negative ) {
      if( $usec != 0 ) {
         $sec  = -$sec - 1;
         $usec = USEC - $usec;
      }
      else {
         $sec = -$sec;
      }
   }

   return [ $sec, $usec ];
}

=head1 FUNCTIONS

=cut

=head2 $time = Time::HiRes::Value->new( $sec, $usec )

This function returns a new instance of a C<Time::HiRes::Value> object. This
object is immutable, and represents the time passed in to the C<I<$sec>> and
C<I<$usec>> parameters.

If the C<I<$usec>> value is provided then the new C<Time::HiRes::Value> object
will store the values passed directly, which must both be integers. Negative
values are represented in "additive" form; that is, a value of C<-1.5> seconds
would be represented by

 Time::HiRes::Value->new( -2, 500000 );

If the C<I<$usec>> value is not provided, then the C<I<$sec>> value will be
parsed as a decimal string, attempting to match out a decimal point to split
seconds and microseconds. This method avoids rounding errors introduced by
floating-point maths. 

=cut

sub new
{
   my $class = shift;

   my ( $sec, $usec );

   if( @_ == 2 ) {
      croak "Cannot accept '$_[0]' for seconds for a "      . __PACKAGE__ unless $_[0] =~ m/^[+-]?\d+(?:\.\d+)?$/;
      croak "Cannot accept '$_[1]' for microseconds for a " . __PACKAGE__ unless $_[1] =~ m/^[+-]?\d+(?:\.\d+)?$/;

      ( $sec, $usec ) = @_;
   }
   elsif( @_ == 1 ) {
      ( $sec, $usec ) = @{ _split_sec_usec( $_[0] ) };
   }
   else {
      carp "Bad number of elements in \@_";
   }

   # Handle case where $sec is non-integer
   $usec += USEC * ( $sec - int( $sec ) );
   $sec = int( $sec );

   # Move overflow from $usec into $sec
   $sec += floor( $usec / USEC );
   $usec %= USEC;

   my $self = [ $sec, $usec ];

   return bless $self, $class;
}

=head2 $time = Time::HiRes::Value->now()

This function returns a new instance of C<Time::HiRes::Value> containing the
current system time, as returned by the system's C<gettimeofday()> call.

=cut

sub now
{
   my $class = shift;
   my @now = gettimeofday();
   return $class->new( @now );
}

use overload '""'  => \&STRING,
             '0+'  => \&NUMBER,
             '+'   => \&add,
             '-'   => \&sub,
             '*'   => \&mult,
             '/'   => \&div,
             '<=>' => \&cmp;

=head1 OPERATORS

Each of the methods here overloads an operator

=cut

=head2 $self->STRING()

=head2 "$self"

This method returns a string representation of the time, in the form of a
decimal string with 6 decimal places. For example

 15.000000
 -3.000000
  4.235996

A leading C<-> sign will be printed if the stored time is negative, and the
C<I<$usec>> part will always contain 6 digits.

=cut

sub STRING
{
   my $self = shift;
   if( $self->[0] < -1 && $self->[1] != 0 ) {
      # Fractional below -1.000000
      return sprintf( '%d.%06d', $self->[0] + 1, USEC - $self->[1] );
   }
   elsif( $self->[0] == -1 && $self->[1] != 0 ) {
      # Special case - between -1 and 0 need to handle the sign carefully
      return sprintf( '-0.%06d', USEC - $self->[1] );
   }
   else {
      return sprintf( '%d.%06d', $self->[0], $self->[1] );
   }
}

sub NUMBER
{
   my $self = shift;
   return $self->[0] + ($self->[1] / USEC);
}

=head2 $self->add( $other )

=head2 $self->sum( $other )

=head2 $self + $other

This method returns a new C<Time::HiRes::Value> value, containing the sum of the
passed values. If a string is passed, it will be parsed according to the same
rules as for the C<new()> constructor.

Note that C<sum> is provided as an alias to C<add>.

=cut

sub add
{
   my $self = shift;
   my ( $other ) = @_;

   if( !ref( $other ) || !$other->isa( __PACKAGE__ ) ) {
      $other = _split_sec_usec( $other );
   }

   return Time::HiRes::Value->new( $self->[0] + $other->[0], $self->[1] + $other->[1] );
}

*sum = \&add;

=head2 $self->sub( $other )

=head2 $self->diff( $other )

=head2 $self - $other

This method returns a new C<Time::HiRes::Value> value, containing the difference
of the passed values. If a string is passed, it will be parsed according to
the same rules as for the C<new()> constructor.

Note that C<diff> is provided as an alias to C<sub>.

=cut

sub sub
{
   my $self = shift;
   my ( $other, $swap ) = @_;

   if( !ref( $other ) || !$other->isa( __PACKAGE__ ) ) {
      $other = _split_sec_usec( $other );
   }

   ( $self, $other ) = ( $other, $self ) if( $swap );

   return Time::HiRes::Value->new( $self->[0] - $other->[0], $self->[1] - $other->[1] );
}

*diff = \&sub;

=head2 $self->mult( $other )

=head2 $self * $other

This method returns a new C<Time::HiRes::Value> value, containing the product
of the passed values. C<$other> must not itself be a C<Time::HiRes::Value>
object; it is an error to attempt to multiply two times together.

=cut

sub mult
{
   my $self = shift;
   my ( $other ) = @_;

   if( ref( $other ) and $other->isa( __PACKAGE__ ) ) {
      croak "Cannot multiply a ".__PACKAGE__." with another";
   }

   return Time::HiRes::Value->new( $self->[0] * $other, $self->[1] * $other );
}

=head2 $self->div( $other )

=head2 $self / $other

This method returns a new C<Time::HiRes::Value> value, containing the quotient
of the passed values. C<$other> must not itself be a C<Time::HiRes::Value>
object; it is an error for a time to be used as a divisor.

=cut

sub div
{
   my $self = shift;
   my ( $other, $swap ) = @_;

   croak "Cannot divide a quantity by a ".__PACKAGE__ if $swap;

   if( ref( $other ) and $other->isa( __PACKAGE__ ) ) {
      croak "Cannot divide a ".__PACKAGE__." by another";
   }

   croak "Illegal division by zero" if $other == 0;

   return Time::HiRes::Value->new( $self->[0] / $other, $self->[1] / $other );
}

=head2 $self->cmp( $other )

=head2 $self <=> $other

This method compares the two passed values, and returns a number that is
positive, negative or zero, as per the usual rules for the C<< <=> >>
operator. If a string is passed, it will be parsed according to the same
rules as for the C<new()> constructor.

=cut

sub cmp
{
   my $self = shift;
   my ( $other ) = @_;

   if( !ref( $other ) || !$other->isa( __PACKAGE__ ) ) {
      $other = _split_sec_usec( $other );
   }

   return $self->[0] <=> $other->[0] ||
          $self->[1] <=> $other->[1];
}

1;

__END__

=head1 SEE ALSO

=over 4

=item *

L<Time::HiRes> - Obtain system timers in resolution greater than 1 second

=back

=head1 AUTHOR

Paul Evans <leonerd@leonerd.org.uk>
