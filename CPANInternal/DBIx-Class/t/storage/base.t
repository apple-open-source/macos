use strict;
use warnings;

use Test::More;
use Test::Warn;
use lib qw(t/lib);
use DBICTest;
use Data::Dumper;

{
    package DBICTest::ExplodingStorage::Sth;
    use strict;
    use warnings;

    sub execute { die "Kablammo!" }

    sub bind_param {}

    package DBICTest::ExplodingStorage;
    use strict;
    use warnings;
    use base 'DBIx::Class::Storage::DBI::SQLite';

    my $count = 0;
    sub sth {
      my ($self, $sql) = @_;
      return bless {},  "DBICTest::ExplodingStorage::Sth" unless $count++;
      return $self->next::method($sql);
    }

    sub connected {
      return 0 if $count == 1;
      return shift->next::method(@_);
    }
}

my $schema = DBICTest->init_schema( sqlite_use_file => 1 );

is( ref($schema->storage), 'DBIx::Class::Storage::DBI::SQLite',
    'Storage reblessed correctly into DBIx::Class::Storage::DBI::SQLite' );

my $storage = $schema->storage;
$storage->ensure_connected;

eval {
    $schema->storage->throw_exception('test_exception_42');
};
like($@, qr/\btest_exception_42\b/, 'basic exception');

eval {
    $schema->resultset('CD')->search_literal('broken +%$#$1')->all;
};
like($@, qr/prepare_cached failed/, 'exception via DBI->HandleError, etc');

bless $storage, "DBICTest::ExplodingStorage";
$schema->storage($storage);

eval { 
    $schema->resultset('Artist')->create({ name => "Exploding Sheep" });
};

is($@, "", "Exploding \$sth->execute was caught");

is(1, $schema->resultset('Artist')->search({name => "Exploding Sheep" })->count,
  "And the STH was retired");


# testing various invocations of connect_info ([ ... ])

my $coderef = sub { 42 };
my $invocations = {
  'connect_info ([ $d, $u, $p, \%attr, \%extra_attr])' => {
      args => [
          'foo',
          'bar',
          undef,
          {
            on_connect_do => [qw/a b c/],
            PrintError => 0,
          },
          {
            AutoCommit => 1,
            on_disconnect_do => [qw/d e f/],
          },
          {
            unsafe => 1,
            auto_savepoint => 1,
          },
        ],
      dbi_connect_info => [
          'foo',
          'bar',
          undef,
          {
            %{$storage->_default_dbi_connect_attributes || {} },
            PrintError => 0,
            AutoCommit => 1,
          },
      ],
  },

  'connect_info ([ \%code, \%extra_attr ])' => {
      args => [
          $coderef,
          {
            on_connect_do => [qw/a b c/],
            PrintError => 0,
            AutoCommit => 1,
            on_disconnect_do => [qw/d e f/],
          },
          {
            unsafe => 1,
            auto_savepoint => 1,
          },
        ],
      dbi_connect_info => [
          $coderef,
      ],
  },

  'connect_info ([ \%attr ])' => {
      args => [
          {
            on_connect_do => [qw/a b c/],
            PrintError => 1,
            AutoCommit => 0,
            on_disconnect_do => [qw/d e f/],
            user => 'bar',
            dsn => 'foo',
          },
          {
            unsafe => 1,
            auto_savepoint => 1,
          },
      ],
      dbi_connect_info => [
          'foo',
          'bar',
          undef,
          {
            %{$storage->_default_dbi_connect_attributes || {} },
            PrintError => 1,
            AutoCommit => 0,
          },
      ],
  },
  'connect_info ([ \%attr_with_coderef ])' => {
      args => [ {
        dbh_maker => $coderef,
        dsn => 'blah',
        user => 'bleh',
        on_connect_do => [qw/a b c/],
        on_disconnect_do => [qw/d e f/],
      } ],
      dbi_connect_info => [
        $coderef
      ],
      warn => qr/Attribute\(s\) 'dsn', 'user' in connect_info were ignored/,
  },
};

for my $type (keys %$invocations) {

  # we can not use a cloner portably because of the coderef
  # so compare dumps instead
  local $Data::Dumper::Sortkeys = 1;
  my $arg_dump = Dumper ($invocations->{$type}{args});

  warnings_exist (
    sub { $storage->connect_info ($invocations->{$type}{args}) },
     $invocations->{$type}{warn} || (),
    'Warned about ignored attributes',
  );

  is ($arg_dump, Dumper ($invocations->{$type}{args}), "$type didn't modify passed arguments");

  is_deeply ($storage->_dbi_connect_info, $invocations->{$type}{dbi_connect_info}, "$type produced correct _dbi_connect_info");
  ok ( (not $storage->auto_savepoint and not $storage->unsafe), "$type correctly ignored extra hashref");

  is_deeply (
    [$storage->on_connect_do, $storage->on_disconnect_do ],
    [ [qw/a b c/], [qw/d e f/] ],
    "$type correctly parsed DBIC specific on_[dis]connect_do",
  );
}

done_testing;

1;
