package DBIx::Class::Schema::Loader::RelBuilder::Compat::v0_06;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::RelBuilder::Compat::v0_07';
use mro 'c3';

our $VERSION = '0.07033';

sub _normalize_name {
    my ($self, $name) = @_;

    $name = $self->_sanitize_name($name);

    return lc $name;
}

=head1 NAME

DBIx::Class::Schema::Loader::RelBuilder::Compat::v0_06 - RelBuilder for
compatibility with DBIx::Class::Schema::Loader version 0.06000

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader::Base/naming> and
L<DBIx::Class::Schema::Loader::RelBuilder>.

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
# vim:et sts=4 sw=4 tw=0:
