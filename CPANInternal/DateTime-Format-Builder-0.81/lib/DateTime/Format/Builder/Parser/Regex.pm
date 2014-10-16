package DateTime::Format::Builder::Parser::Regex;
{
  $DateTime::Format::Builder::Parser::Regex::VERSION = '0.81';
}


use strict;
use warnings;
use vars qw( @ISA );
use Params::Validate qw( validate ARRAYREF SCALARREF HASHREF CODEREF );

use DateTime::Format::Builder::Parser::generic;
@ISA = qw( DateTime::Format::Builder::Parser::generic );

__PACKAGE__->valid_params(

    # How to match
    params => {
        type => ARRAYREF,    # mapping $1,$2,... to new() args
    },
    regex => {
        type      => SCALARREF,
        callbacks => {
            'is a regex' => sub { ref(shift) eq 'Regexp' }
        }
    },

    # How to create
    extra => {
        type     => HASHREF,
        optional => 1,
    },
    constructor => {
        type      => CODEREF | ARRAYREF,
        optional  => 1,
        callbacks => {
            'array has 2 elements' => sub {
                ref( $_[0] ) eq 'ARRAY' ? ( @{ $_[0] } == 2 ) : 1;
                }
        }
    },
);

sub do_match {
    my $self    = shift;
    my $date    = shift;
    my @matches = $date =~ $self->{regex};
    return @matches ? \@matches : undef;
}

sub post_match {
    my $self = shift;
    my ( $date, $matches, $p ) = @_;

    # Fill %p from match
    @{$p}{ @{ $self->{params} } } = @$matches;
    return;
}

sub make {
    my $self = shift;
    my ( $date, $dt, $p ) = @_;
    my @args = ( %$p, %{ $self->{extra} } );
    if ( my $cons = $self->{constructor} ) {
        if ( ref $cons eq 'ARRAY' ) {
            my ( $class, $method ) = @$cons;
            return $class->$method(@args);
        }
        elsif ( ref $cons eq 'CODE' ) {
            return $self->$cons(@args);
        }
    }
    else {
        return DateTime->new(@args);
    }
}

sub create_parser {
    my ( $self, %args ) = @_;
    $args{extra} ||= {};
    unless ( ref $self ) {
        $self = $self->new(%args);
    }

    # Create our parser
    return $self->generic_parser(
        (
            map { exists $args{$_} ? ( $_ => $args{$_} ) : () }
                qw(
                on_match on_fail preprocess postprocess
                )
        ),
        label => $args{label},
    );
}

1;

# ABSTRACT: Regex based date parsing

__END__

=pod

=head1 NAME

DateTime::Format::Builder::Parser::Regex - Regex based date parsing

=head1 VERSION

version 0.81

=head1 SYNOPSIS

   my $parser = DateTime::Format::Builder->create_parser(
	regex  => qr/^(\d\d\d\d)(\d\d)(\d\d)T(\d\d)(\d\d)(\d\d)$/,
	params => [ qw( year month day hour minute second ) ],
   );

=head1 SPECIFICATION

In addition to the
L<common keys|DateTime::Format::Builder/"SINGLE SPECIFICATIONS">,
C<Regex> supports:

=over 4

=item *

B<regex> is a regular expression that should capture
elements of the datetime string.
This is a required element. This is the key whose presence
indicates it's a specification that belongs to this class.

=item *

B<params> is an arrayref of key names. The captures from the
regex are mapped to these (C<$1> to the first element, C<$2>
to the second, and so on) and handed to
C<< DateTime->new() >>.
This is a required element.

=item *

B<extra> is a hashref of extra arguments you wish to give to
C<< DateTime->new() >>. For example, you could set the
C<year> or C<time_zone> to defaults:

    extra => { year => 2004, time_zone => "Australia/Sydney" },

=item *

B<constructor> is either an arrayref or a coderef. If an arrayref
then the first element is a class name or object, and the second
element is a method name (or coderef since Perl allows that sort of
thing).  The arguments to the call are anything in C<$p> and
anything given in the C<extra> option above.

If only a coderef is supplied, then it is called with arguments of
C<$self>, C<$p> and C<extra>.

In short:

            $self->$coderef( %$p, %{ $self->{extra} } );

The method is expected to return a valid L<DateTime> object,
or undef in event of failure, but can conceivably return anything
it likes. So long as it's 'true'.

=back

=head1 SUPPORT

See L<DateTime::Format::Builder> for details.

=head1 SEE ALSO

C<datetime@perl.org> mailing list.

http://datetime.perl.org/

L<perl>, L<DateTime>,
L<DateTime::Format::Builder>

=head1 AUTHORS

=over 4

=item *

Dave Rolsky <autarch@urth.org>

=item *

Iain Truskett

=back

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2013 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

=cut
