package DBIx::Class::Schema::Loader::DBI::DB2;

use strict;
use warnings;
use base qw/
    DBIx::Class::Schema::Loader::DBI::Component::QuotedDefault
    DBIx::Class::Schema::Loader::DBI
/;
use Carp::Clan qw/^DBIx::Class/;
use Class::C3;

our $VERSION = '0.05003';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::DB2 - DBIx::Class::Schema::Loader::DBI DB2 Implementation.

=head1 SYNOPSIS

  package My::Schema;
  use base qw/DBIx::Class::Schema::Loader/;

  __PACKAGE__->loader_options( db_schema => "MYSCHEMA" );

  1;

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader::Base>.

=cut

sub _setup {
    my $self = shift;

    $self->next::method(@_);

    my $dbh = $self->schema->storage->dbh;
    $self->{db_schema} ||= $dbh->selectrow_array('VALUES(CURRENT_USER)', {});
}

sub _table_uniq_info {
    my ($self, $table) = @_;

    my @uniqs;

    my $dbh = $self->schema->storage->dbh;

    my $sth = $self->{_cache}->{db2_uniq} ||= $dbh->prepare(
        q{SELECT kcu.COLNAME, kcu.CONSTNAME, kcu.COLSEQ
        FROM SYSCAT.TABCONST as tc
        JOIN SYSCAT.KEYCOLUSE as kcu ON tc.CONSTNAME = kcu.CONSTNAME
        WHERE tc.TABSCHEMA = ? and tc.TABNAME = ? and tc.TYPE = 'U'}
    ) or die $DBI::errstr;

    $sth->execute($self->db_schema, uc $table) or die $DBI::errstr;

    my %keydata;
    while(my $row = $sth->fetchrow_arrayref) {
        my ($col, $constname, $seq) = @$row;
        push(@{$keydata{$constname}}, [ $seq, lc $col ]);
    }
    foreach my $keyname (keys %keydata) {
        my @ordered_cols = map { $_->[1] } sort { $a->[0] <=> $b->[0] }
            @{$keydata{$keyname}};
        push(@uniqs, [ $keyname => \@ordered_cols ]);
    }

    $sth->finish;
    
    return \@uniqs;
}

# DBD::DB2 doesn't follow the DBI API for ->tables
sub _tables_list { 
    my $self = shift;
    
    my $dbh = $self->schema->storage->dbh;
    my @tables = map { lc } $dbh->tables(
        $self->db_schema ? { TABLE_SCHEM => $self->db_schema } : undef
    );
    s/\Q$self->{_quoter}\E//g for @tables;
    s/^.*\Q$self->{_namesep}\E// for @tables;

    return @tables;
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

sub _extra_column_info {
    my ($self, $info) = @_;
    my %extra_info;

    my ($table, $column) = @$info{qw/TABLE_NAME COLUMN_NAME/};

    my $dbh = $self->schema->storage->dbh;
    my $sth = $dbh->prepare_cached(
        q{
            SELECT COUNT(*)
            FROM syscat.columns
            WHERE tabschema = ? AND tabname = ? AND colname = ?
            AND identity = 'Y' AND generated != ''
        },
        {}, 1);
    $sth->execute($self->db_schema, $table, $column);
    if ($sth->fetchrow_array) {
        $extra_info{is_auto_increment} = 1;
    }

    return \%extra_info;
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader>, L<DBIx::Class::Schema::Loader::Base>,
L<DBIx::Class::Schema::Loader::DBI>

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
