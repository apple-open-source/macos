package DateTime::Format::Builder::Parser::Dispatch;
use strict;
use vars qw( $VERSION %dispatch_data );
use Params::Validate qw( CODEREF validate );
use DateTime::Format::Builder::Parser;

=head1 NAME

DateTime::Format::Builder::Parser::Dispatch - Dispatch parsers by group

=head1 SYNOPSIS

    package SampleDispatch;
    use DateTime::Format::Builder
    (
	parsers => {
	    parse_datetime => [
		{
		    Dispatch => sub {
			return 'fnerk';
		    }
		}
	    ]
	},
	groups => {
	    fnerk => [
		{
		    regex => qr/^(\d{4})(\d\d)(\d\d)$/,
		    params => [qw( year month day )],
		},
	    ]
	}
    );

=head1 DESCRIPTION

C<Dispatch> adds another parser type to C<Builder> permitting
dispatch of parsing according to group names.

=head1 SPECIFICATION

C<Dispatch> has just one key: C<Dispatch>. The value should be a
reference to a subroutine that returns one of:

=over 4

=item *

C<undef>, meaning no groups could be found.

=item *

An empty list, meaning no groups could be found.

=item *

A single string, meaning: use this group

=item *

A list of strings, meaning: use these groups in this order.

=back

Groups are specified much like the example in the L<SYNOPSIS>.
They follow the same format as when you specify them for methods.

=head1 SIDEEFFECTS

Your group parser can also be a Dispatch parser. Thus you could
potentially end up with an infinitely recursive parser.

=cut

$VERSION = '0.78';

{
    no strict 'refs';
    *dispatch_data = *DateTime::Format::Builder::dispatch_data;
    *params = *DateTime::Format::Builder::Parser::params;
}

DateTime::Format::Builder::Parser->valid_params(
    Dispatch => {
	type => CODEREF,
    }
);

sub create_parser
{
    my ($self, %args) = @_;
    my $coderef = $args{Dispatch};

    return sub {
	my ($self, $date, $p, @args) = @_;
	return unless defined $date;
	my $class = ref($self)||$self;

	my @results = $coderef->( $date );
	return unless @results;
	return unless defined $results[0];

	for my $group (@results)
	{
	    my $parser = $dispatch_data{$class}{$group};
	    die "Unknown parsing group: $class\n" unless defined $parser;
	    my $rv = eval { $parser->parse( $self, $date, $p, @args ) };
	    return $rv unless $@ or not defined $rv;
	}
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


