package DBIx::Class::Schema::Loader::DBI::Sybase::Common;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBI';
use Carp::Clan qw/^DBIx::Class/;
use Class::C3;

our $VERSION = '0.05003';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::Sybase::Common - Common functions for Sybase
and MSSQL

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader::Base>.

=cut

# DBD::Sybase doesn't implement get_info properly
sub _build_quoter  { '"' }
sub _build_namesep { '.' }

sub _set_quote_char_and_name_sep {
    my $self = shift;

    $self->schema->storage->sql_maker->quote_char([qw/[ ]/])
        unless $self->schema->storage->sql_maker->quote_char;

    $self->schema->storage->sql_maker->name_sep('.')
        unless $self->schema->storage->sql_maker->name_sep;
}

sub _build_db_schema {
    my $self = shift;
    my $dbh  = $self->schema->storage->dbh;

    local $dbh->{FetchHashKeyName} = 'NAME_lc';
    
    my $test_table = "_loader_test_$$";

    my $db_schema = 'dbo'; # default

    eval {
        $dbh->do("create table $test_table (id integer)");
        my $sth = $dbh->prepare('sp_tables');
        $sth->execute;
        while (my $row = $sth->fetchrow_hashref) {
            next unless $row->{table_name} eq $test_table;

            $db_schema = $row->{table_owner};
            last;
        }
        $sth->finish;
        $dbh->do("drop table $test_table");
    };
    my $exception = $@;
    eval { $dbh->do("drop table $test_table") };
    carp "Could not determine db_schema, defaulting to $db_schema : $exception"
        if $exception;

    return $db_schema;
}

# remove 'IDENTITY' from column data_type
sub _columns_info_for {
    my $self   = shift;
    my $result = $self->next::method(@_);

    foreach my $col (keys %$result) {
        $result->{$col}->{data_type} =~ s/\s* identity \s*//ix;
    }

    return $result;
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader::DBI::Sybase>,
L<DBIx::Class::Schema::Loader::DBI::MSSQL>,
L<DBIx::Class::Schema::Loader::DBI>
L<DBIx::Class::Schema::Loader>, L<DBIx::Class::Schema::Loader::Base>,

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
