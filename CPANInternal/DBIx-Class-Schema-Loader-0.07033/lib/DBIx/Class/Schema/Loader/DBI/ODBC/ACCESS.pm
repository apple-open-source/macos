package DBIx::Class::Schema::Loader::DBI::ODBC::ACCESS;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBI::ODBC';
use mro 'c3';
use Try::Tiny;
use namespace::clean;
use DBIx::Class::Schema::Loader::Table ();

our $VERSION = '0.07033';

__PACKAGE__->mk_group_accessors('simple', qw/
    __ado_connection
    __adox_catalog
/);

=head1 NAME

DBIx::Class::Schema::Loader::DBI::ODBC::ACCESS - Microsoft Access driver for
DBIx::Class::Schema::Loader

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader::Base> for usage information.

=cut

sub _supports_db_schema { 0 }

sub _db_path {
    my $self = shift;

    $self->schema->storage->dbh->get_info(16);
}

sub _open_ado_connection {
    my ($self, $conn, $user, $pass) = @_;

    my @info = ({
        provider => 'Microsoft.ACE.OLEDB.12.0',
        dsn_extra => 'Persist Security Info=False',
    }, {
        provider => 'Microsoft.Jet.OLEDB.4.0',
    });

    my $opened = 0;
    my $exception;

    for my $info (@info) {
        $conn->{Provider} = $info->{provider};

        my $dsn = 'Data Source='.($self->_db_path);
        $dsn .= ";$info->{dsn_extra}" if exists $info->{dsn_extra};

        try {
            $conn->Open($dsn, $user, $pass);
            undef $exception;
        }
        catch {
            $exception = $_;
        };

        next if $exception;

        $opened = 1;
        last;
    }

    return ($opened, $exception);
}


sub _ado_connection {
    my $self = shift;

    return $self->__ado_connection if $self->__ado_connection;

    my ($dsn, $user, $pass) = @{ $self->schema->storage->_dbi_connect_info };

    my $have_pass = 1;

    if (ref $dsn eq 'CODE') {
        ($dsn, $user, $pass) = $self->_try_infer_connect_info_from_coderef($dsn);

        if (not $dsn) {
            my $dbh = $self->schema->storage->dbh;
            $dsn  = $dbh->{Name};
            $user = $dbh->{Username};
            $have_pass = 0;
        }
    }

    require Win32::OLE;
    my $conn = Win32::OLE->new('ADODB.Connection');

    $user = '' unless defined $user;
    if ((not $have_pass) && exists $self->_passwords->{$dsn}{$user}) {
        $pass = $self->_passwords->{$dsn}{$user};
        $have_pass = 1;
    }
    $pass = '' unless defined $pass;

    my ($opened, $exception) = $self->_open_ado_connection($conn, $user, $pass);

    if ((not $opened) && (not $have_pass)) {
        if (exists $ENV{DBI_PASS}) {
            $pass = $ENV{DBI_PASS};

            ($opened, $exception) = $self->_open_ado_connection($conn, $user, $pass);

            if ($opened) {
                $self->_passwords->{$dsn}{$user} = $pass;
            }
            else {
                print "Enter database password for $user ($dsn): ";
                chomp($pass = <STDIN>);

                ($opened, $exception) = $self->_open_ado_connection($conn, $user, $pass);

                if ($opened) {
                    $self->_passwords->{$dsn}{$user} = $pass;
                }
            }
        }
        else {
            print "Enter database password for $user ($dsn): ";
            chomp($pass = <STDIN>);

            ($opened, $exception) = $self->_open_ado_connection($conn, $user, $pass);

            if ($opened) {
                $self->_passwords->{$dsn}{$user} = $pass;
            }
        }
    }

    if (not $opened) {
        die "Failed to open ADO connection: $exception";
    }

    $self->__ado_connection($conn);

    return $conn;
}

sub _adox_catalog {
    my $self = shift;

    return $self->__adox_catalog if $self->__adox_catalog;

    require Win32::OLE;
    my $cat = Win32::OLE->new('ADOX.Catalog');
    $cat->{ActiveConnection} = $self->_ado_connection;

    $self->__adox_catalog($cat);

    return $cat;
}

sub _adox_column {
    my ($self, $table, $col) = @_;

    my $col_obj;

    my $cols = $self->_adox_catalog->Tables->Item($table->name)->Columns;

    for my $col_idx (0..$cols->Count-1) {
        $col_obj = $cols->Item($col_idx);
        if ($self->preserve_case) {
            last if $col_obj->Name eq $col;
        }
        else {
            last if lc($col_obj->Name) eq lc($col);
        }
    }

    return $col_obj;
}

sub rescan {
    my $self = shift;

    if ($self->__adox_catalog) {
        $self->__ado_connection(undef);
        $self->__adox_catalog(undef);
    }

    return $self->next::method(@_);
}

sub _table_pk_info {
    my ($self, $table) = @_;

    return [] if $self->_disable_pk_detection;

    my @keydata;

    my $indexes = try {
        $self->_adox_catalog->Tables->Item($table->name)->Indexes
    }
    catch {
        warn "Could not retrieve indexes in table '$table', disabling primary key detection: $_\n";
        return undef;
    };

    if (not $indexes) {
        $self->_disable_pk_detection(1);
        return [];
    }

    for my $idx_num (0..($indexes->Count-1)) {
        my $idx = $indexes->Item($idx_num);
        if ($idx->PrimaryKey) {
            my $cols = $idx->Columns;
            for my $col_idx (0..$cols->Count-1) {
                push @keydata, $self->_lc($cols->Item($col_idx)->Name);
            }
        }
    }

    return \@keydata;
}

sub _table_fk_info {
    my ($self, $table) = @_;

    return [] if $self->_disable_fk_detection;

    my $keys = try {
        $self->_adox_catalog->Tables->Item($table->name)->Keys;
    }
    catch {
        warn "Could not retrieve keys in table '$table', disabling relationship detection: $_\n";
        return undef;
    };

    if (not $keys) {
        $self->_disable_fk_detection(1);
        return [];
    }

    my @rels;

    for my $key_idx (0..($keys->Count-1)) {
        my $key = $keys->Item($key_idx);

        next unless $key->Type == 2;

        my $local_cols   = $key->Columns;
        my $remote_table = $key->RelatedTable;
        my (@local_cols, @remote_cols);

        for my $col_idx (0..$local_cols->Count-1) {
            my $col = $local_cols->Item($col_idx);
            push @local_cols,  $self->_lc($col->Name);
            push @remote_cols, $self->_lc($col->RelatedColumn);
        }

        push @rels, {
            local_columns => \@local_cols,
            remote_columns => \@remote_cols,
            remote_table => DBIx::Class::Schema::Loader::Table->new(
                loader => $self,
                name   => $remote_table,
                ($self->db_schema ? (
                    schema        => $self->db_schema->[0],
                    ignore_schema => 1,
                ) : ()),
            ),
        };
    }

    return \@rels;
}

sub _columns_info_for {
    my $self    = shift;
    my ($table) = @_;

    my $result = $self->next::method(@_);

    while (my ($col, $info) = each %$result) {
        my $data_type = $info->{data_type};

        my $col_obj = $self->_adox_column($table, $col);

        $info->{is_nullable} = ($col_obj->Attributes & 2) == 2 ? 1 : 0;

        if ($data_type eq 'counter') {
            $info->{data_type} = 'integer';
            $info->{is_auto_increment} = 1;
            delete $info->{size};
        }
        elsif ($data_type eq 'longbinary') {
            $info->{data_type} = 'image';
            $info->{original}{data_type} = 'longbinary';
        }
        elsif ($data_type eq 'longchar') {
            $info->{data_type} = 'text';
            $info->{original}{data_type} = 'longchar';
        }
        elsif ($data_type eq 'double') {
            $info->{data_type} = 'double precision';
            $info->{original}{data_type} = 'double';
        }
        elsif ($data_type eq 'guid') {
            $info->{data_type} = 'uniqueidentifier';
            $info->{original}{data_type} = 'guid';
        }
        elsif ($data_type eq 'byte') {
            $info->{data_type} = 'tinyint';
            $info->{original}{data_type} = 'byte';
        }
        elsif ($data_type eq 'currency') {
            $info->{data_type} = 'money';
            $info->{original}{data_type} = 'currency';

            if (ref $info->{size} eq 'ARRAY' && $info->{size}[0] == 19 && $info->{size}[1] == 4) {
                # Actual money column via ODBC, otherwise we pass the sizes on to the ADO driver for
                # decimal columns (which masquerade as money columns...)
                delete $info->{size};
            }
        }
        elsif ($data_type eq 'decimal') {
            if (ref $info->{size} eq 'ARRAY' && $info->{size}[0] == 18 && $info->{size}[1] == 0) {
                delete $info->{size};
            }
        }

# Pass through currency (which can be decimal for ADO.)
        if ($data_type !~ /^(?:(?:var)?(?:char|binary)|decimal)\z/ && $data_type ne 'currency') {
            delete $info->{size};
        }
    }

    return $result;
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
# vim:et sts=4 sw=4 tw=0:
