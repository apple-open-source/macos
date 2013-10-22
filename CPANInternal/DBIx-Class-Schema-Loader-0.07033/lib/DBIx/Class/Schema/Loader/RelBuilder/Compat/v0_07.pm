package DBIx::Class::Schema::Loader::RelBuilder::Compat::v0_07;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::RelBuilder';
use mro 'c3';

=head1 NAME

DBIx::Class::Schema::Loader::RelBuilder::Compat::v0_07 - RelBuilder for
compatibility with DBIx::Class::Schema::Loader version 0.07000

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader::Base/naming> and
L<DBIx::Class::Schema::Loader::RelBuilder>.

=cut

our $VERSION = '0.07033';

sub _strip_id_postfix {
    my ($self, $name) = @_;

    $name =~ s/_id\z//;

    return $name;
}

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
# vim:et sts=4 sw=4 tw=0:
