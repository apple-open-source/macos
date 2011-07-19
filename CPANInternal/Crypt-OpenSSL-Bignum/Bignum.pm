package Crypt::OpenSSL::Bignum;

use 5.005;
use strict;
use Carp;

use vars qw( $VERSION @ISA );

require DynaLoader;

@ISA = qw(DynaLoader);

$VERSION = '0.04';

bootstrap Crypt::OpenSSL::Bignum $VERSION;

sub DESTROY
{
    shift->_free_BN();
}

sub bless_pointer
{
    my( $proto, $p_pointer ) = @_;
    return bless( \$p_pointer, $proto );
}

sub equals
{
    my( $self, $a ) = @_;
    return ! $self->cmp( $a );
}


1;
__END__

=head1 NAME

Crypt::OpenSSL::Bignum - OpenSSL's multiprecision integer arithmetic

=head1 SYNOPSIS

  use Crypt::OpenSSL::Bignum;

  my $bn = Crypt::OpenSSL::Bignum->new_from_decimal( "1000" );
  # or
  my $bn = Crypt::OpenSSL::Bignum->new_from_word( 1000 );
  # or
  my $bn = Crypt::OpenSSL::Bignum->new_from_hex("0x3e8");
  # or
  my $bn = Crypt::OpenSSL::Bignum->new_from_bin(pack( "C*", 3, 232 ))

  use Crypt::OpenSSL::Bignum::CTX;

  sub print_factorial
  {
    my( $n ) = @_;
    my $fac = Crypt::OpenSSL::Bignum->one();
    my $ctx = Crypt::OpenSSL::Bignum::CTX->new();
    foreach my $i (1 .. $n)
    {
      $fac->mul( Crypt::OpenSSL::Bignum->new_from_word( $i ), $ctx, $fac );
    }
    print "$n factorial is ", $fac->to_decimal(), "\n";
  }

=head1 DESCRIPTION

Crypt::OpenSSL::Bignum provides access to OpenSSL multiprecision
integer arithmetic libraries.  Presently, many though not all of the
arithmetic operations that OpenSSL provides are exposed to perl.  In
addition, this module can be used to provide access to bignum values
produced by other OpenSSL modules, such as key parameters from
Crypt::OpenSSL::RSA.

I<NOTE>: Many of the methods in this package can croak, so use eval, or
Error.pm's try/catch mechanism to capture errors.

=head1 Class Methods

=over

=item new_from_word

Create a new Crypt::OpenSSL::Bignum object whose value will be the
word given.  Note that numbers represneted by objects created using
this method are necessarily between 0 and 2^32 - 1.

=item new_from_decimal

Create a new Crypt::OpenSSL::Bignum object whose value is specified by
the given decimal representation.

=item new_from_hex

Create a new Crypt::OpenSSL::Bignum object whose value is specified by
the given hexidecimal representation.

=item new_from_bin

Create a new Crypt::OpenSSL::Bignum object whose value is specified by
the given packed binary string.  Note that objects created using this
method are necessarily nonnegative.

=item zero

Returns a new Crypt::OpenSSL::Bignum object representing 0

=item one

Returns a new Crypt::OpenSSL::Bignum object representing 1

=item bless_pointer

Given a pointer to a OpenSSL BIGNUM object in memory, construct and
return Crypt::OpenSSL::Bignum object around this.  Note that the
underlying BIGNUM object will be destroyed (via BN_clear_free(3ssl))
when the returned Crypt::OpenSSL::Bignum object is no longer
referenced, so the pointer passed to this method should only be
referenced via the returned perl object after calling bless_pointer.

This method is intended only for use by XSUB writers writing code that
interfaces with OpenSSL library methods, and who wish to be able to
return a BIGNUM structure to perl as a Crypt::OpenSSL::Bignum object.

=back

=head1 Instance Methods

=over

=item to_decimal

Return a decimal string representation of this object.

=item to_hex

Return a hexidecimal string representation of this object.

=item to_bin

Return a packed binary string representation of this object.  Note
that sign is ignored, so that to bin called on a
Crypt::OpenSSL::Bignum object representing a negative number returns
the same value as it would called on an object representing that
number's absolute value.

=item get_word

Return a scalar integer representation of this object, if it can be
represented as an unsigned long.

=item is_zero

Returns true of this object represents 0.

=item is_one

Returns true of this object represents 1.

=item is_odd

Returns true of this object represents an odd number.

=item copy

Returns a copy of this object.

=item add

This method returns the sum of this object and the first argument.  If
only one argument is passed, a new Crypt::OpenSSL::Bignum object is
created for the return value; otherwise, the value of second argument
is set to the result and returned.

=item sub

This method returns the difference of this object and the first
argument.  If only one argument is passed, a new
Crypt::OpenSSL::Bignum object is created for the return value;
otherwise, the value of second argument is set to the result and
returned.

=item mul

This method returns the product of this object and the first argument,
using the second argument, a Crypt::OpenSSL::Bignum::CTX object, as a
scratchpad.  If only two arguments are passed, a new
Crypt::OpenSSL::Bignum object is created for the return value;
otherwise, the value of third argument is set to the result and
returned.

=item div

This method returns a list consisting of quotient and the remainder
obtained by dividing this object by the first argument, using the
second argument, a Crypt::OpenSSL::Bignum::CTX object, as a
scratchpad.  If only two arguments are passed, new
Crypt::OpenSSL::Bignum objects is created for both return values.  If
a third argument is passed, otherwise, the value of third argument is
set to the quotient.  If a fourth argument is passed, the value of the
fourth argument is set to the remainder.

=item exp

This method returns the product of this object exponeniated by the
first argument, using the second argument, a
Crypt::OpenSSL::Bignum::CTX object, as a scratchpad.

=item mod_exp

This method returns the product of this object exponeniated by the
first argument, modulo the second argument, using the third argument, a
Crypt::OpenSSL::Bignum::CTX object, as a scratchpad.

=item pointer_copy

This method is intended only for use by XSUB writers wanting to have
access to the underlying BIGNUM structure referenced by a
Crypt::OpenSSL::Bignum perl object so that they can pass them to other
routines in the OpenSSL library.  It returns a perl scalar whose IV
can be cast to a BIGNUM* value.  This can then be passed to an XSUB
which can work with the BIGNUM directly.  Note that the BIGNUM object
pointed to will be a copy of the BIGNUM object wrapped by the
instance; it is thus the responsiblity of the client to free space
allocated by this BIGNUM object if and when it is done with it. See
also bless_pointer.

=back

=head1 AUTHOR

Ian Robertson, iroberts@cpan.org

=head1 SEE ALSO

L<perl>, L<bn(3ssl)>

=cut
