package DateTime::Format::Builder::Parser::Quick;
use strict;
use vars qw( $VERSION %dispatch_data );
use Params::Validate qw( SCALAR OBJECT CODEREF validate );
use base qw( DateTime::Format::Builder::Parser );

=head1 NAME

DateTime::Format::Builder::Parser::Quick - Use another formatter, simply

=head1 SYNOPSIS

    use DateTime::Format::Builder (
    parsers => { parse_datetime => [
        { Quick => 'DateTime::Format::HTTP' },
        { Quick => 'DateTime::Format::Mail' },
        { Quick => 'DateTime::Format::IBeat' },
    ]});

is the same as:

    use DateTime::Format::HTTP;
    use DateTime::Format::Mail;
    use DateTime::Format::IBeat;

    use DateTime::Format::Builder (
    parsers => { parse_datetime => [
        sub { eval { DateTime::Format::HTTP->parse_datetime( $_[1] ) } },
        sub { eval { DateTime::Format::Mail->parse_datetime( $_[1] ) } },
        sub { eval { DateTime::Format::IBeat->parse_datetime( $_[1] ) } },
    ]});

(These two pieces of code can both be found in the test
suite; one as F<quick.t>, the other as F<fall.t>.)

=head1 DESCRIPTION

C<Quick> adds a parser that allows some shortcuts when
writing fairly standard and mundane calls to other
formatting modules.

=head1 SPECIFICATION

C<Quick> has two keys, one optional.

The C<Quick> keyword should have an argument of either an
object or a class name. If it's a class name then the class
is C<use>d.

The C<method> keyword is optional with a default of
C<parse_datetime>. It's either name of the method to invoke
on the object, or a reference to a piece of code.

In any case, the resultant code ends up looking like:

     my $rv = $Quick->$method( $date );

=cut

$VERSION = '0.77';

__PACKAGE__->valid_params(
    Quick => {
	type => SCALAR|OBJECT,
        callbacks => {
            good_classname => sub {
                (ref $_[0]) or ( $_[0] =~ /^\w+[:'\w+]*\w+/ )
            },
        }
    },
    method => {
        optional => 1,
        type => SCALAR|CODEREF,
    },
);

sub create_parser
{
    my ($self, %args) = @_;
    my $class = $args{Quick};
    my $method = $args{method};
    $method = 'parse_datetime' unless defined $method;
    eval "use $class";
    die $@ if $@;

    return sub {
        my ($self, $date) = @_;
	return unless defined $date;
        my $rv = eval { $class->$method( $date ) };
        return $rv if defined $rv;
	return;
    };
}

1;

__END__

=head1 THANKS

See L<the main module's section|DateTime::Format::Builder/"THANKS">.

=head1 SUPPORT

Support for this module is provided via the datetime@perl.org email
list. See http://lists.perl.org/ for more details.

Alternatively, log them via the CPAN RT system via the web or email:

    http://perl.dellah.org/rt/dtbuilder
    bug-datetime-format-builder@rt.cpan.org

This makes it much easier for me to track things and thus means
your problem is less likely to be neglected.

=head1 LICENCE AND COPYRIGHT

Copyright E<copy> Iain Truskett, 2003. All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.000 or,
at your option, any later version of Perl 5 you may have available.

The full text of the licences can be found in the F<Artistic> and
F<COPYING> files included with this module, or in L<perlartistic> and
L<perlgpl> as supplied with Perl 5.8.1 and later.

=head1 AUTHOR

Iain Truskett <spoon@cpan.org>

=head1 SEE ALSO

C<datetime@perl.org> mailing list.

http://datetime.perl.org/

L<perl>, L<DateTime>,
L<DateTime::Format::Builder>

=cut


