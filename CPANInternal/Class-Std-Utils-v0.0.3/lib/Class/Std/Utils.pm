package Class::Std::Utils;

use version; $VERSION = qv('0.0.3');

use warnings;
use strict;
use Carp;
use Scalar::Util qw( refaddr );

sub import {
    my $caller = caller;

    no strict qw( refs );
    *{ $caller . '::anon_scalar' }               = \&anon_scalar;
    *{ $caller . '::ident' }                     = \&refaddr;
    *{ $caller . '::extract_initializers_from' } = \&extract_initializers_from;
}

sub anon_scalar { return \my $scalar; }

use List::Util qw( first );

sub extract_initializers_from {
    my ($arg_ref) = @_;

    my $class_name = caller;
 
    # Find the class-specific sub-hash (if any)...
    my $specific_inits_ref 
        = first {defined $_} $arg_ref->{$class_name}, {};
    croak "$class_name initializer must be a nested hash"        if ref $specific_inits_ref ne 'HASH'; 
    # Return initializers, overriding general initializers from the top level
    # with any second-level initializers that are specific to the class....
    return ( %{$arg_ref}, %{$specific_inits_ref} );
}


1; # Magic true value required at end of module
__END__

=head1 NAME

Class::Std::Utils - Utility subroutines for building "inside-out" objects


=head1 VERSION

This document describes Class::Std::Utils version 0.0.3


=head1 SYNOPSIS

    use Class::Std::Utils;

    # Constructor for anonymous scalars...
    my $new_object = bless anon_scalar(), $class;

    # Convert an object reference into a unique ID number...
    my $ID_num = ident $new_object;

    # Extract class-specific arguments from a hash reference...
    my %args = extract_initializers_from($arg_ref);


=head1 DESCRIPTION

This module provides three utility subroutines that simplify the creation of
"inside-out" classes. See Chapters 15 and 16 of "Perl Best Practices"
(O'Reilly, 2005) for details.

=head1 INTERFACE 

=over 

=item C<anon_scalar()>

This subroutine is always exported. It takes no arguments and returns a
reference to an anonymous scalar, suitable for blessing as an object.

=item C<ident()>

This subroutine is always exported. It takes one argument--a reference--
and acts exactly like the C<Scalar::Util::refaddr()>, returning a unique
integer value suitable for identifying the referent.

=item C<extract_initializers_from()>

This subroutine is always exported. It takes one argument--a hash reference--
and returns a "flattened" set of key/value pairs extracted from that hash.

The typical usage is:

    my %class_specific_args = extract_initializers_from($args_ref);

The argument hash is flattened as described in Chapter 16 of "Perl Best
Practices":

=over

I<The subroutine is always called with the original multi-level argument
hash from the constructor. It then looks up the class's own name (i.e.
its C<caller> package) in the argument hash, to see if an initializer
with that key has been defined. Finally, C<extract_initializers_for()>
returns the flattened set of key/value pairs for the class's initializer
set, by appending the class-specific initializer subhash to the end of
the original generic initializer hash. Appending the specific
initializers after the generic ones means that any key in the class-
specific set will override any key in the generic set, thereby ensuring
that the most relevant initializers are always selected, but that
generic initializers are still available where no class-specific value
has been passed in.>

=back

In other words, given:

    my $arg_ref = {
        key_1 => 'generic value 1',
        key_2 => 'generic value 2',

        'Base::Class' => {
            key_1 => 'base value 1'
        },

        'Der::Class' => {
            key_1 => 'der value 1'
            key_2 => 'der value 2'
        },
    };

    package Base::Class;
    use Class::Std::Utils;

    my %base_args = extract_initializers_from($arg_ref);

    package Der::Class;
    use Class::Std::Utils;

    my %der_args = extract_initializers_from($arg_ref);
            
then C<%base_args> would be initialized to:

    (
        key_1 => 'base value 1',
        key_2 => 'generic value 2',

        'Base::Class' => {
            key_1 => 'base value 1',
        },

        'Der::Class' => {
            key_1 => 'der value 1',
            key_2 => 'der value 2',
        },
    )

whilst C<%der_args> would be initialized to:

    (
        key_1 => 'der value 1',
        key_2 => 'der value 2',

        'Base::Class' => {
            key_1 => 'base value 1',
        },

        'Der::Class' => {
            key_1 => 'der value 1',
            key_2 => 'der value 2',
        },
    )

That is, the top-level entries would be replaced by any second-level
entries with the same key that appear in a top-level entry of the same name as
the calling package.

This means that each class can just refer to C<$args{key_1}> and
C<$args{key_2}> and be confident that the resulting values will be the
most specific available for that class.

=back

=head1 DIAGNOSTICS

=over 

=item C<< %s initializer must be a nested hash >>

Thrown by C<extract_initializers_from()>. You specified a top-level key
that has the same name of the current class, but the value of that key
wasn't a hash reference.

=back


=head1 CONFIGURATION AND ENVIRONMENT

Class::Std::Utils requires no configuration files or environment variables.


=head1 DEPENDENCIES

Thsi module requires both the C<Scalar::Util> and C<List::Util> modules,
which are standard in Perl 5.8 and available from the CPAN for earlier
versions of Perl.


=head1 INCOMPATIBILITIES

None reported.


=head1 SEE ALSO

The C<Class::Std> module

"Perl Best Practices", O'Reilly, 2005.


=head1 BUGS AND LIMITATIONS

No bugs have been reported.

Please report any bugs or feature requests to
C<bug-class-std-utils@rt.cpan.org>, or through the web interface at
L<http://rt.cpan.org>.


=head1 AUTHOR

Damian Conway  C<< <DCONWAY@cpan.org> >>


=head1 LICENCE AND COPYRIGHT

Copyright (c) 2005, Damian Conway C<< <DCONWAY@cpan.org> >>. All rights reserved.

This module is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.


=head1 DISCLAIMER OF WARRANTY

BECAUSE THIS SOFTWARE IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
FOR THE SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN
OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES
PROVIDE THE SOFTWARE "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER
EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE SOFTWARE IS WITH
YOU. SHOULD THE SOFTWARE PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL
NECESSARY SERVICING, REPAIR, OR CORRECTION.

IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
REDISTRIBUTE THE SOFTWARE AS PERMITTED BY THE ABOVE LICENCE, BE
LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL,
OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE
THE SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR DATA BEING
RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR A
FAILURE OF THE SOFTWARE TO OPERATE WITH ANY OTHER SOFTWARE), EVEN IF
SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF
SUCH DAMAGES.
