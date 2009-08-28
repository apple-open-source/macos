use Test::More;

eval "use Test::Pod::Coverage 1.04";
plan skip_all => 'Test::Pod::Coverage 1.04 required' if $@;
plan skip_all => 'set TEST_POD to enable this test'
  unless ($ENV{TEST_POD} || -e 'MANIFEST.SKIP');

my @modules = sort { $a cmp $b } (Test::Pod::Coverage::all_modules());
plan tests => scalar(@modules);

# Since this is about checking documentation, a little documentation
# of what this is doing might be in order...
# The exceptions structure below is a hash keyed by the module
# name.  The value for each is a hash, which contains one or more
# (although currently more than one makes no sense) of the following
# things:-
#   skip   => a true value means this module is not checked
#   ignore => array ref containing list of methods which
#             do not need to be documented.
my $exceptions = {
    'DBIx::Class' => {
        ignore => [
            qw/MODIFY_CODE_ATTRIBUTES
              component_base_class
              mk_classdata
              mk_classaccessor/
        ]
    },
    'DBIx::Class::Storage' => {
        ignore => [
            qw(cursor)
        ]
    },
    'DBIx::Class::CDBICompat::AccessorMapping'          => { skip => 1 },
    'DBIx::Class::CDBICompat::AttributeAPI'             => { skip => 1 },
    'DBIx::Class::CDBICompat::AutoUpdate'               => { skip => 1 },
    'DBIx::Class::CDBICompat::ColumnCase'               => { skip => 1 },
    'DBIx::Class::CDBICompat::ColumnGroups'             => { skip => 1 },
    'DBIx::Class::CDBICompat::Constraints'              => { skip => 1 },
    'DBIx::Class::CDBICompat::Constructor'              => { skip => 1 },
    'DBIx::Class::CDBICompat::DestroyWarning'           => { skip => 1 },
    'DBIx::Class::CDBICompat::GetSet'                   => { skip => 1 },
    'DBIx::Class::CDBICompat::HasA'                     => { skip => 1 },
    'DBIx::Class::CDBICompat::HasMany'                  => { skip => 1 },
    'DBIx::Class::CDBICompat::ImaDBI'                   => { skip => 1 },
    'DBIx::Class::CDBICompat::LazyLoading'              => { skip => 1 },
    'DBIx::Class::CDBICompat::LiveObjectIndex'          => { skip => 1 },
    'DBIx::Class::CDBICompat::MightHave'                => { skip => 1 },
    'DBIx::Class::CDBICompat::ObjIndexStubs'            => { skip => 1 },
    'DBIx::Class::CDBICompat::Pager'                    => { skip => 1 },
    'DBIx::Class::CDBICompat::ReadOnly'                 => { skip => 1 },
    'DBIx::Class::CDBICompat::Retrieve'                 => { skip => 1 },
    'DBIx::Class::CDBICompat::Stringify'                => { skip => 1 },
    'DBIx::Class::CDBICompat::TempColumns'              => { skip => 1 },
    'DBIx::Class::CDBICompat::Triggers'                 => { skip => 1 },
    'DBIx::Class::ClassResolver::PassThrough'           => { skip => 1 },
    'DBIx::Class::Componentised'                        => { skip => 1 },
    'DBIx::Class::Relationship::Accessor'               => { skip => 1 },
    'DBIx::Class::Relationship::BelongsTo'              => { skip => 1 },
    'DBIx::Class::Relationship::CascadeActions'         => { skip => 1 },
    'DBIx::Class::Relationship::HasMany'                => { skip => 1 },
    'DBIx::Class::Relationship::HasOne'                 => { skip => 1 },
    'DBIx::Class::Relationship::Helpers'                => { skip => 1 },
    'DBIx::Class::Relationship::ManyToMany'             => { skip => 1 },
    'DBIx::Class::Relationship::ProxyMethods'           => { skip => 1 },
    'DBIx::Class::ResultSetProxy'                       => { skip => 1 },
    'DBIx::Class::ResultSourceProxy'                    => { skip => 1 },
    'DBIx::Class::Storage::DBI'                         => { skip => 1 },
    'DBIx::Class::Storage::DBI::DB2'                    => { skip => 1 },
    'DBIx::Class::Storage::DBI::MSSQL'                  => { skip => 1 },
    'DBIx::Class::Storage::DBI::MultiDistinctEmulation' => { skip => 1 },
    'DBIx::Class::Storage::DBI::ODBC400'                => { skip => 1 },
    'DBIx::Class::Storage::DBI::ODBC::DB2_400_SQL'      => { skip => 1 },
    'DBIx::Class::Storage::DBI::Oracle'                 => { skip => 1 },
    'DBIx::Class::Storage::DBI::Pg'                     => { skip => 1 },
    'DBIx::Class::Storage::DBI::SQLite'                 => { skip => 1 },
    'DBIx::Class::Storage::DBI::mysql'                  => { skip => 1 },
    'SQL::Translator::Parser::DBIx::Class'              => { skip => 1 },
    'SQL::Translator::Producer::DBIx::Class::File'      => { skip => 1 },

# skipped because the synopsis covers it clearly

    'DBIx::Class::InflateColumn::File'                  => { skip => 1 },

# skip connection since it's just an override

    'DBIx::Class::Schema::Versioned' => { ignore => [ qw(connection) ] },

# must kill authors.

    'DBIx::Class::Storage::DBI::Replication' => { skip => 1 },
};

foreach my $module (@modules) {
  SKIP:
    {
        skip "$module - No real methods", 1 if ($exceptions->{$module}{skip});

        # build parms up from ignore list
        my $parms = {};
        $parms->{trustme} =
          [ map { qr/^$_$/ } @{ $exceptions->{$module}{ignore} } ]
          if exists($exceptions->{$module}{ignore});

        # run the test with the potentially modified parm set
        pod_coverage_ok($module, $parms, "$module POD coverage");
    }
}
