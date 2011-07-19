package DBIx::Class::Schema::Loader::DBI::Component::QuotedDefault;

use strict;
use warnings;
use Class::C3;

our $VERSION = '0.05003';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::Component::QuotedDefault -- Loader::DBI
Component to parse quoted default constants and functions

=head1 DESCRIPTION

If C<COLUMN_DEF> from L<DBI/column_info> returns character constants quoted,
then we need to remove the quotes. This also allows distinguishing between
default functions without information schema introspection.

=cut

sub _columns_info_for {
    my $self    = shift;
    my ($table) = @_;

    my $result = $self->next::method(@_);

    while (my ($col, $info) = each %$result) {
        if (my $def = $info->{default_value}) {
            $def =~ s/^\s+//;
            $def =~ s/\s+\z//;

# remove Pg typecasts (e.g. 'foo'::character varying) too
            if ($def =~ /^["'](.*?)['"](?:::[\w\s]+)?\z/) {
                $info->{default_value} = $1;
            }
            else {
                $info->{default_value} = $def =~ /^\d/ ? $def : \$def;
            }
        }
    }

    return $result;
}

1;

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader>, L<DBIx::Class::Schema::Loader::Base>,
L<DBIx::Class::Schema::Loader::DBI>

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut
