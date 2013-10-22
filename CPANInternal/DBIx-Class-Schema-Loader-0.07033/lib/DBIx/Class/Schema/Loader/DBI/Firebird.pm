package DBIx::Class::Schema::Loader::DBI::Firebird;

use strict;
use warnings;
use base qw/DBIx::Class::Schema::Loader::DBI::InterBase/;
use mro 'c3';

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::Firebird - DBIx::Class::Schema::Loader::DBI
L<DBD::Firebird> subclass

=head1 DESCRIPTION

This is an empty subclass of L<DBIx::Class::Schema::Loader::DBI::InterBase> for
use with L<DBD::Firebird>, see that driver for details.

See L<DBIx::Class::Schema::Loader> and L<DBIx::Class::Schema::Loader::Base> for
general Schema::Loader information.

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader>, L<DBIx::Class::Schema::Loader::Base>,
L<DBIx::Class::Schema::Loader::DBI>, L<DBIx::Class::Schema::Loader::DBI::InterBase>

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
# vim:et sw=4 sts=4 tw=0:
