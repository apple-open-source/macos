package DBIx::Class::Schema::Loader::DBI::DB2;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBI';
use Carp::Clan qw/^DBIx::Class/;
use Class::C3;

our $VERSION = '0.04005';

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

sub _tables_list {
    my $self = shift;
    return map lc, $self->next::method;
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

=cut

1;
