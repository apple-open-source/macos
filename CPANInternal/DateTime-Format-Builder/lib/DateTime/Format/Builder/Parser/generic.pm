package DateTime::Format::Builder::Parser::generic;
use strict;
use vars qw( $VERSION );
use Carp;
use Params::Validate qw(
    validate SCALAR CODEREF UNDEF
);

$VERSION = '0.77';

=head1 NAME

DateTime::Format::Builder::Parser::generic - Useful routines

=head1 METHODS

=head2 Useful

=head3 new

Standard constructor. Returns a blessed hash; any arguments are placed
in the hash. This is useful for storing information between methods.

=cut

sub new
{
    my $class = shift;
    bless { @_ }, $class;
}

=head3 generic_parser

This is a method provided solely for the benefit of
C<Parser> implementations. It semi-neatly abstracts
a lot of the work involved.

Basically, it takes parameters matching the assorted
callbacks from the parser declarations and makes a coderef
out of it all.

Currently recognized callbacks are:

=over 4

=item *

on_match

=item *

on_fail

=item *

preprocess

=item *

postprocess

=back

=cut

sub generic_parser {
    my $class = shift;
    my %args = validate( @_, {
	    ( map { $_ => { type => CODEREF, optional => 1 } } qw(
	      on_match on_fail preprocess postprocess
	    ) ),
	    label => { type => SCALAR|UNDEF, optional => 1 },
	});
    my $label = $args{label};

    my $callback = (exists $args{on_match} or exists $args{on_fail}) ? 1 : undef;

    return sub
    {
	my ($self, $date, $p, @args) = @_;
	return unless defined $date;
	my %p;
	%p = %$p if $p; # Look! A Copy!

	my %param = (
	    self => $self,
	    ( defined $label ? ( label => $label ) : ()),
	    (@args ? (args => \@args) : ()),
	);

	# Preprocess - can modify $date and fill %p
	if ($args{preprocess})
	{
	    $date = $args{preprocess}->( input => $date, parsed => \%p, %param );
	}

	my $rv = $class->do_match( $date, @args ) if $class->can('do_match');

	# Funky callback thing
	if ($callback)
	{
	    my $type = defined $rv ? "on_match" : "on_fail";
	    $args{$type}->( input => $date, %param ) if $args{$type};
	}
	return unless defined $rv;

	my $dt;
	$dt = $class->post_match( $date, $rv, \%p ) if $class->can('post_match');

	# Allow post processing. Return undef if regarded as failure
	if ($args{postprocess})
	{
	    my $rv = $args{postprocess}->(
		parsed => \%p,
		input => $date,
		post => $dt,
		%param,
	    );
	    return unless $rv;
	}

	# A successful match!
	$dt = $class->make( $date, $dt, \%p ) if $class->can('make');
	return $dt;
    };
}

=head2 Methods for subclassing

These are methods you should define when writing your own subclass.

B<Note>: these methods do not exist in this class. There is no point
trying to call C<< $self->SUPER::do_match( ... ) >>.

=head3 do_match

C<do_match> is the first phase. Arguments are the date and @args.
C<self>, C<label>, C<args>. Return value must be defined if you match
successfully.

=head3 post_match

C<post_match> is called after the appropriate callback out of
C<on_match>/C<on_fail> is done. It's passed the date, the return
value from C<do_match> and the parsing hash.

Its return value is used as the C<post> argument to the C<postprocess>
callback, and as the second argument to C<make>.

=head3 make

C<make> takes the original input, the return value from C<post_match>
and the parsing hash and should return a C<DateTime> object or
undefined.

=head2 Delegations

For use of C<Parser>, this module also delegates C<valid_params> and
C<params>. This is just convenience to save typing the following:

    DateTime::Format::Builder::Parser->valid_params( blah )

Instead we get to type:

    $self->valid_params( blah );
    __PACKAGE__->valid_params( blah );

=cut

{
    no strict 'refs';
    for (qw( valid_params params ))
    {
	*$_ = *{"DateTime::Format::Builder::Parser::$_"};
    }
}

1;

__END__

=head1 WRITING A SUBCLASS

Rather than attempt to explain how it all works, I think it's best if
you take a look at F<Regex.pm> and F<Strptime.pm> as examples and
work from there.

=head1 THANKS

See L<DateTime::Format::Builder>.

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

L<perl>, L<DateTime>, L<DateTime::Format::Builder>,
L<DateTime::Format::Builder::Parser>.

=cut

