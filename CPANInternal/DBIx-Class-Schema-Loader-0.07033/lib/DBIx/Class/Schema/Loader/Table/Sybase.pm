package DBIx::Class::Schema::Loader::Table::Sybase;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBObject::Sybase';
use mro 'c3';

=head1 NAME

DBIx::Class::Schema::Loader::Table::Sybase - Class for Sybase ASE and MSSQL
Tables in L<DBIx::Class::Schema::Loader>

=head1 DESCRIPTION

Inherits from L<DBIx::Class::Schema::Loader::DBObject::Sybase>, see that module for details.

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader::DBObject::Sybase>,
L<DBIx::Class::Schema::Loader::DBObject>,
L<DBIx::Class::Schema::Loader::Table>, L<DBIx::Class::Schema::Loader>,
L<DBIx::Class::Schema::Loader::Base>

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
# vim:et sts=4 sw=4 tw=0:
