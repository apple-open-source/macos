use strict;
use Test::More tests => 10;

BEGIN {
    use_ok 'DBIx::Class::Schema::Loader';
    use_ok 'DBIx::Class::Schema::Loader::Base';
    use_ok 'DBIx::Class::Schema::Loader::DBI';
    use_ok 'DBIx::Class::Schema::Loader::RelBuilder';
    use_ok 'DBIx::Class::Schema::Loader::DBI::SQLite';
    use_ok 'DBIx::Class::Schema::Loader::DBI::mysql';
    use_ok 'DBIx::Class::Schema::Loader::DBI::Pg';
    use_ok 'DBIx::Class::Schema::Loader::DBI::DB2';
    use_ok 'DBIx::Class::Schema::Loader::DBI::Oracle';
    use_ok 'DBIx::Class::Schema::Loader::DBI::Writing';
}
