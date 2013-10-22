package DBIx::Class::Schema::Loader::DBI::Writing;
use strict;

our $VERSION = '0.07033';

# Empty. POD only.

=head1 NAME                                                                     
                                                                                
DBIx::Class::Schema::Loader::DBI::Writing - Loader subclass writing guide for DBI

=head1 SYNOPSIS

  package DBIx::Class::Schema::Loader::DBI::Foo;

  # THIS IS JUST A TEMPLATE TO GET YOU STARTED.

  use strict;
  use warnings;
  use base 'DBIx::Class::Schema::Loader::DBI';
  use mro 'c3';

  sub _table_uniq_info {
      my ($self, $table) = @_;

      # ... get UNIQUE info for $table somehow
      # and return a data structure that looks like this:

      return [
         [ 'keyname' => [ 'colname' ] ],
         [ 'keyname2' => [ 'col1name', 'col2name' ] ],
         [ 'keyname3' => [ 'colname' ] ],
      ];

      # Where the "keyname"'s are just unique identifiers, such as the
      # name of the unique constraint, or the names of the columns involved
      # concatenated if you wish.
  }

  sub _table_comment {
      my ( $self, $table ) = @_;
      return 'Comment';
  }

  sub _column_comment {
      my ( $self, $table, $column_number ) = @_;
      return 'Col. comment';
  }

  1;

=head1 DETAILS

The only required method for new subclasses is C<_table_uniq_info>,
as there is not (yet) any standardized, DBD-agnostic way for obtaining
this information from DBI.

The base DBI Loader contains generic methods that *should* work for
everything else in theory, although in practice some DBDs need to
override one or more of the other methods.  The other methods one might
likely want to override are: C<_table_pk_info>, C<_table_fk_info>,
C<_tables_list> and C<_extra_column_info>.  See the included DBD drivers
for examples of these.

To import comments from the database you need to implement C<_table_comment>,
C<_column_comment>

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
