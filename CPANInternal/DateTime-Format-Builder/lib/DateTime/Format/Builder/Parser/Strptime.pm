package DateTime::Format::Builder::Parser::Strptime;

=head1 NAME

DateTime::Format::Builder::Parser::Strptime - strptime based date parsing

=head1 SYNOPSIS

   my $parser = DateTime::Format::Builder->create_parser(
	strptime => '%e/%b/%Y:%H:%M:%S %z',
   );

=head1 SPECIFICATION

=over 4

=item *

B<strptime> takes as its argument a strptime string.
See L<DateTime::Format::Strptime> for more information
on valid patterns.

=back

=cut

use strict;
use vars qw( $VERSION @ISA );
use Params::Validate qw( validate SCALAR HASHREF );

$VERSION = '0.77';
use DateTime::Format::Builder::Parser::generic;
@ISA = qw( DateTime::Format::Builder::Parser::generic );

__PACKAGE__->valid_params(
    strptime	=> {
	type	=> SCALAR|HASHREF, # straight pattern or options to DTF::Strptime
    },
);

sub create_parser
{
    my ($self, %args) = @_;

    # Arguments to DTF::Strptime
    my $pattern = $args{strptime};

    # Create our strptime parser
    require DateTime::Format::Strptime;
    my $strptime = DateTime::Format::Strptime->new(
	( ref $pattern ? %$pattern : ( pattern => $pattern ) ),
    );
    unless (ref $self)
    {
	$self = $self->new( %args );
    }
    $self->{strptime} = $strptime;

    # Create our parser
    return $self->generic_parser(
	( map { exists $args{$_} ? ( $_ => $args{$_} ) : () } qw(
	    on_match on_fail preprocess postprocess
	    ) ),
	label => $args{label},
    );
}

sub do_match
{
    my $self = shift;
    my $date = shift;
    local $^W; # bizarre bug
    # Do the match!
    my $dt = eval { $self->{strptime}->parse_datetime( $date ) };
    return $@ ? undef : $dt;
}

sub post_match
{
    return $_[2];
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


