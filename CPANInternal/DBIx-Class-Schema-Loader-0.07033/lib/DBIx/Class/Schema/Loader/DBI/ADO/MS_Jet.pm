package DBIx::Class::Schema::Loader::DBI::ADO::MS_Jet;

use strict;
use warnings;
use base qw/
    DBIx::Class::Schema::Loader::DBI::ADO
    DBIx::Class::Schema::Loader::DBI::ODBC::ACCESS
/;
use mro 'c3';
use Try::Tiny;
use namespace::clean;

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::ADO::MS_Jet - ADO wrapper for
L<DBIx::Class::Schema::Loader::DBI::ODBC::ACCESS>

=head1 DESCRIPTION

Proxy for L<DBIx::Class::Schema::Loader::DBI::ODBC::ACCESS> when using
L<DBD::ADO>.

See L<DBIx::Class::Schema::Loader::Base> for usage information.

=cut

sub _db_path {
    my $self = shift;

    $self->schema->storage->dbh->get_info(2);
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

    $dsn =~ s/^dbi:[^:]+://i;

    local $Win32::OLE::Warn = 0;

    my @dsn;
    for my $s (split /;/, $dsn) {
        my ($k, $v) = split /=/, $s, 2;
        if (defined $conn->{$k}) {
            $conn->{$k} = $v;
            next;
        }
        push @dsn, $s;
    }

    $dsn = join ';', @dsn;

    $user = '' unless defined $user;

    if ((not $have_pass) && exists $self->_passwords->{$dsn}{$user}) {
        $pass = $self->_passwords->{$dsn}{$user};
        $have_pass = 1;
    }
    $pass = '' unless defined $pass;

    try {
        $conn->Open($dsn, $user, $pass);
    }
    catch {
        if (not $have_pass) {
            if (exists $ENV{DBI_PASS}) {
                $pass = $ENV{DBI_PASS};
                try {
                    $conn->Open($dsn, $user, $pass);
                    $self->_passwords->{$dsn}{$user} = $pass;
                }
                catch {
                    print "Enter database password for $user ($dsn): ";
                    chomp($pass = <STDIN>);
                    $conn->Open($dsn, $user, $pass);
                    $self->_passwords->{$dsn}{$user} = $pass;
                };
            }
            else {
                print "Enter database password for $user ($dsn): ";
                chomp($pass = <STDIN>);
                $conn->Open($dsn, $user, $pass);
                $self->_passwords->{$dsn}{$user} = $pass;
            }
        }
        else {
            die $_;
        }
    };

    $self->__ado_connection($conn);

    return $conn;
}

sub _columns_info_for {
    my $self    = shift;
    my ($table) = @_;

    my $result = $self->next::method(@_);

    while (my ($col, $info) = each %$result) {
        my $data_type = $info->{data_type};

        my $col_obj = $self->_adox_column($table, $col);

        if ($data_type eq 'long') {
            $info->{data_type} = 'integer';
            delete $info->{size};

            my $props = $col_obj->Properties;
            for my $prop_idx (0..$props->Count-1) {
                my $prop = $props->Item($prop_idx);
                if ($prop->Name eq 'Autoincrement' && $prop->Value == 1) {
                    $info->{is_auto_increment} = 1;
                    last;
                }
            }
        }
        elsif ($data_type eq 'short') {
            $info->{data_type} = 'smallint';
            delete $info->{size};
        }
        elsif ($data_type eq 'single') {
            $info->{data_type} = 'real';
            delete $info->{size};
        }
        elsif ($data_type eq 'money') {
            if (ref $info->{size} eq 'ARRAY') {
                if ($info->{size}[0] == 19 && $info->{size}[1] == 255) {
                    delete $info->{size};
                }
                else {
                    # it's really a decimal
                    $info->{data_type} = 'decimal';

                    if ($info->{size}[0] == 18 && $info->{size}[1] == 0) {
                        # default size
                        delete $info->{size};
                    }
                    delete $info->{original};
                }
            }
        }
        elsif ($data_type eq 'varchar') {
            $info->{data_type} = 'char' if $col_obj->Type == 130;
            $info->{size} = $col_obj->DefinedSize;
        }
        elsif ($data_type eq 'bigbinary') {
            $info->{data_type} = 'varbinary';

            my $props = $col_obj->Properties;
            for my $prop_idx (0..$props->Count-1) {
                my $prop = $props->Item($prop_idx);
                if ($prop->Name eq 'Fixed Length' && $prop->Value == 1) {
                    $info->{data_type} = 'binary';
                    last;
                }
            }

            $info->{size} = $col_obj->DefinedSize;
        }
        elsif ($data_type eq 'longtext') {
            $info->{data_type} = 'text';
            $info->{original}{data_type} = 'longchar';
            delete $info->{size};
        }
    }

    return $result;
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader::DBI::ODBC::ACCESS>,
L<DBIx::Class::Schema::Loader::DBI::ADO>,
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
