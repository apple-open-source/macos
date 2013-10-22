use strict;
use Test::More;
use lib qw(t/backcompat/0.04006/lib);
use File::Path;
use make_dbictest_db;
use dbixcsl_test_dir qw/$tdir/;
use Class::Unload ();

require DBIx::Class::Schema::Loader;

plan skip_all => 'set SCHEMA_LOADER_TESTS_BACKCOMPAT to enable these tests'
    unless $ENV{SCHEMA_LOADER_TESTS_BACKCOMPAT};

my $DUMP_PATH = "$tdir/dump";

sub do_dump_test {
    my %tdata = @_;

    my $schema_class = $tdata{classname};

    no strict 'refs';
    @{$schema_class . '::ISA'} = ('DBIx::Class::Schema::Loader');

    $tdata{options}{use_namespaces} ||= 0;

    $schema_class->loader_options(dump_directory => $DUMP_PATH, %{$tdata{options}});

    my @warns;
    eval {
        local $SIG{__WARN__} = sub { push(@warns, @_) };
        $schema_class->connect($make_dbictest_db::dsn);
    };
    my $err = $@;

    Class::Unload->unload($schema_class);

    is($err, $tdata{error});

    my $check_warns = $tdata{warnings};
    is(@warns, @$check_warns);
    for(my $i = 0; $i <= $#$check_warns; $i++) {
        like($warns[$i], $check_warns->[$i]);
    }

    my $file_regexes = $tdata{regexes};
    my $file_neg_regexes = $tdata{neg_regexes} || {};
    my $schema_regexes = delete $file_regexes->{schema};
    
    my $schema_path = $DUMP_PATH . '/' . $schema_class;
    $schema_path =~ s{::}{/}g;
    dump_file_like($schema_path . '.pm', @$schema_regexes);
    foreach my $src (keys %$file_regexes) {
        my $src_file = $schema_path . '/' . $src . '.pm';
        dump_file_like($src_file, @{$file_regexes->{$src}});
    }
    foreach my $src (keys %$file_neg_regexes) {
        my $src_file = $schema_path . '/' . $src . '.pm';
        dump_file_not_like($src_file, @{$file_neg_regexes->{$src}});
    }
}

sub dump_file_like {
    my $path = shift;
    open(my $dumpfh, '<', $path) or die "Failed to open '$path': $!";
    my $contents = do { local $/; <$dumpfh>; };
    close($dumpfh);
    like($contents, $_) for @_;
}

sub dump_file_not_like {
    my $path = shift;
    open(my $dumpfh, '<', $path) or die "Failed to open '$path': $!";
    my $contents = do { local $/; <$dumpfh>; };
    close($dumpfh);
    unlike($contents, $_) for @_;
}

sub append_to_class {
    my ($class, $string) = @_;
    $class =~ s{::}{/}g;
    $class = $DUMP_PATH . '/' . $class . '.pm';
    open(my $appendfh, '>>', $class) or die "Failed to open '$class' for append: $!";
    print $appendfh $string;
    close($appendfh);
}

rmtree($DUMP_PATH, 1, 1);

do_dump_test(
    classname => 'DBICTest::DumpMore::1',
    options => { },
    error => '',
    warnings => [
        qr/Dumping manual schema for DBICTest::DumpMore::1 to directory /,
        qr/Schema dump completed/,
    ],
    regexes => {
        schema => [
            qr/package DBICTest::DumpMore::1;/,
            qr/->load_classes/,
        ],
        Foo => [
            qr/package DBICTest::DumpMore::1::Foo;/,
            qr/->set_primary_key/,
            qr/1;\n$/,
        ],
        Bar => [
            qr/package DBICTest::DumpMore::1::Bar;/,
            qr/->set_primary_key/,
            qr/1;\n$/,
        ],
    },
);

append_to_class('DBICTest::DumpMore::1::Foo',q{# XXX This is my custom content XXX});

do_dump_test(
    classname => 'DBICTest::DumpMore::1',
    options => { },
    error => '',
    warnings => [
        qr/Dumping manual schema for DBICTest::DumpMore::1 to directory /,
        qr/Schema dump completed/,
    ],
    regexes => {
        schema => [
            qr/package DBICTest::DumpMore::1;/,
            qr/->load_classes/,
        ],
        Foo => [
            qr/package DBICTest::DumpMore::1::Foo;/,
            qr/->set_primary_key/,
            qr/1;\n# XXX This is my custom content XXX/,
        ],
        Bar => [
            qr/package DBICTest::DumpMore::1::Bar;/,
            qr/->set_primary_key/,
            qr/1;\n$/,
        ],
    },
);

do_dump_test(
    classname => 'DBICTest::DumpMore::1',
    options => { really_erase_my_files => 1 },
    error => '',
    warnings => [
        qr/Dumping manual schema for DBICTest::DumpMore::1 to directory /,
        qr/Deleting existing file /,
        qr/Deleting existing file /,
        qr/Deleting existing file /,
        qr/Schema dump completed/,
    ],
    regexes => {
        schema => [
            qr/package DBICTest::DumpMore::1;/,
            qr/->load_classes/,
        ],
        Foo => [
            qr/package DBICTest::DumpMore::1::Foo;/,
            qr/->set_primary_key/,
            qr/1;\n$/,
        ],
        Bar => [
            qr/package DBICTest::DumpMore::1::Bar;/,
            qr/->set_primary_key/,
            qr/1;\n$/,
        ],
    },
    neg_regexes => {
        Foo => [
            qr/# XXX This is my custom content XXX/,
        ],
    },
);

do_dump_test(
    classname => 'DBICTest::DumpMore::1',
    options => { use_namespaces => 1 },
    error => '',
    warnings => [
        qr/Dumping manual schema for DBICTest::DumpMore::1 to directory /,
        qr/Schema dump completed/,
    ],
    regexes => {
        schema => [
            qr/package DBICTest::DumpMore::1;/,
            qr/->load_namespaces/,
        ],
        'Result/Foo' => [
            qr/package DBICTest::DumpMore::1::Result::Foo;/,
            qr/->set_primary_key/,
            qr/1;\n$/,
        ],
        'Result/Bar' => [
            qr/package DBICTest::DumpMore::1::Result::Bar;/,
            qr/->set_primary_key/,
            qr/1;\n$/,
        ],
    },
);

do_dump_test(
    classname => 'DBICTest::DumpMore::1',
    options => { use_namespaces => 1,
                 result_namespace => 'Res',
                 resultset_namespace => 'RSet',
                 default_resultset_class => 'RSetBase',
             },
    error => '',
    warnings => [
        qr/Dumping manual schema for DBICTest::DumpMore::1 to directory /,
        qr/Schema dump completed/,
    ],
    regexes => {
        schema => [
            qr/package DBICTest::DumpMore::1;/,
            qr/->load_namespaces/,
            qr/result_namespace => "Res"/,
            qr/resultset_namespace => "RSet"/,
            qr/default_resultset_class => "RSetBase"/,
        ],
        'Res/Foo' => [
            qr/package DBICTest::DumpMore::1::Res::Foo;/,
            qr/->set_primary_key/,
            qr/1;\n$/,
        ],
        'Res/Bar' => [
            qr/package DBICTest::DumpMore::1::Res::Bar;/,
            qr/->set_primary_key/,
            qr/1;\n$/,
        ],
    },
);

do_dump_test(
    classname => 'DBICTest::DumpMore::1',
    options => { use_namespaces => 1,
                 result_namespace => '+DBICTest::DumpMore::1::Res',
                 resultset_namespace => 'RSet',
                 default_resultset_class => 'RSetBase',
                 result_base_class => 'My::ResultBaseClass',
                 schema_base_class => 'My::SchemaBaseClass',
             },
    error => '',
    warnings => [
        qr/Dumping manual schema for DBICTest::DumpMore::1 to directory /,
        qr/Schema dump completed/,
    ],
    regexes => {
        schema => [
            qr/package DBICTest::DumpMore::1;/,
            qr/->load_namespaces/,
            qr/result_namespace => "\+DBICTest::DumpMore::1::Res"/,
            qr/resultset_namespace => "RSet"/,
            qr/default_resultset_class => "RSetBase"/,
            qr/use base 'My::SchemaBaseClass'/,
        ],
        'Res/Foo' => [
            qr/package DBICTest::DumpMore::1::Res::Foo;/,
            qr/use base 'My::ResultBaseClass'/,
            qr/->set_primary_key/,
            qr/1;\n$/,
        ],
        'Res/Bar' => [
            qr/package DBICTest::DumpMore::1::Res::Bar;/,
            qr/use base 'My::ResultBaseClass'/,
            qr/->set_primary_key/,
            qr/1;\n$/,
        ],
    },
);

done_testing;

END { rmtree($DUMP_PATH, 1, 1) if $ENV{SCHEMA_LOADER_TESTS_BACKCOMPAT}; }
