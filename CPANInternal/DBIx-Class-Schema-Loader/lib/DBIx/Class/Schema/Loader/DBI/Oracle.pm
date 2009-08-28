package # hide from pause/cpan for now, as there's a permissions
        # issue and it's screwing the rest of the package
  DBIx::Class::Schema::Loader::DBI::Oracle;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBI';
use Carp::Clan qw/^DBIx::Class/;
use Class::C3;

our $VERSION = '0.04005';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::Oracle - DBIx::Class::Schema::Loader::DBI 
Oracle Implementation.

=head1 SYNOPSIS

  package My::Schema;
  use base qw/DBIx::Class::Schema::Loader/;

  __PACKAGE__->loader_options( debug => 1 );

  1;

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader::Base>.

This module is considered experimental and not well tested yet.

=cut

sub _setup {
    my $self = shift;

    $self->next::method(@_);

    my $dbh = $self->schema->storage->dbh;
    $self->{db_schema} ||= $dbh->selectrow_array('SELECT USER FROM DUAL', {});
}


sub _table_columns {
    my ($self, $table) = @_;

    my $dbh = $self->schema->storage->dbh;

    my $sth = $dbh->prepare($self->schema->storage->sql_maker->select($table, undef, \'1 = 0'));
    $sth->execute;
    return \@{$sth->{NAME_lc}};
}

sub _tables_list { 
    my $self = shift;

    my $dbh = $self->schema->storage->dbh;

    my @tables;
    for my $table ( $dbh->tables(undef, $self->db_schema, '%', 'TABLE,VIEW') ) { #catalog, schema, table, type
        my $quoter = $dbh->get_info(29);
        $table =~ s/$quoter//g;

        # remove "user." (schema) prefixes
        $table =~ s/\w+\.//;

        next if $table eq 'PLAN_TABLE';
        $table = lc $table;
        push @tables, $1
          if $table =~ /\A(\w+)\z/;
    }
    return @tables;
}

sub _table_uniq_info {
    my ($self, $table) = @_;

    my $dbh = $self->schema->storage->dbh;

    my $sth = $dbh->prepare_cached(
        q{
            SELECT constraint_name, ucc.column_name
            FROM user_constraints JOIN user_cons_columns ucc USING (constraint_name)
            WHERE ucc.table_name=? AND constraint_type='U'
            ORDER BY ucc.position
        },
        {}, 1);

    $sth->execute(uc $table);
    my %constr_names;
    while(my $constr = $sth->fetchrow_arrayref) {
        my $constr_name = lc $constr->[0];
        my $constr_def  = lc $constr->[1];
        $constr_name =~ s/\Q$self->{_quoter}\E//;
        $constr_def =~ s/\Q$self->{_quoter}\E//;
        push @{$constr_names{$constr_name}}, $constr_def;
    }
    
    my @uniqs = map { [ $_ => $constr_names{$_} ] } keys %constr_names;
    return \@uniqs;
}

sub _table_pk_info {
    my ($self, $table) = @_;
    return $self->next::method(uc $table);
}

sub _table_fk_info {
    my ($self, $table) = @_;

    my $rels = $self->next::method(uc $table);

    foreach my $rel (@$rels) {
        $rel->{remote_table} = lc $rel->{remote_table};
    }

    return $rels;
}

sub _columns_info_for {
    my ($self, $table) = @_;
    return $self->next::method(uc $table);
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader>, L<DBIx::Class::Schema::Loader::Base>,
L<DBIx::Class::Schema::Loader::DBI>

=head1 AUTHOR

TSUNODA Kazuya C<drk@drk7.jp>

=cut

1;
