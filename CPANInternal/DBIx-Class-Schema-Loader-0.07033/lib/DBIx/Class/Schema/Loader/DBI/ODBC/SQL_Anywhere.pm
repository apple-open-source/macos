package DBIx::Class::Schema::Loader::DBI::ODBC::SQL_Anywhere;

use strict;
use warnings;
use base qw/
    DBIx::Class::Schema::Loader::DBI::ODBC
    DBIx::Class::Schema::Loader::DBI::SQLAnywhere
/;
use mro 'c3';

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::ODBC::SQL_Anywhere - ODBC wrapper for
L<DBIx::Class::Schema::Loader::DBI::SQLAnywhere>

=head1 DESCRIPTION

Proxy for L<DBIx::Class::Schema::Loader::DBI::SQLAnywhere> when using L<DBD::ODBC>.

See L<DBIx::Class::Schema::Loader::Base> for usage information.

=cut

sub _columns_info_for {
    my $self = shift;

    my $result = $self->next::method(@_);

    while (my ($col, $info) = each %$result) {
        # The ODBC driver sets the default value to NULL even when it was not specified.
        if (ref $info->{default_value} && ${ $info->{default_value} } eq 'null') {
            delete $info->{default_value};
        }
    }

    return $result;
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader::DBI::SQLAnywhere>,
L<DBIx::Class::Schema::Loader>, L<DBIx::Class::Schema::Loader::Base>,
L<DBIx::Class::Schema::Loader::DBI>

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
# vim:et sw=4 sts=4 tw=0:
