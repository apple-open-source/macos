use strict;
use Test::More;
use lib qw(t/lib);
use File::Path;
use IPC::Open3;
use make_dbictest_db;
require DBIx::Class::Schema::Loader;

my $DUMP_PATH = './t/_dump';

sub dump_directly {
    my %tdata = @_;

    my $schema_class = $tdata{classname};

    no strict 'refs';
    @{$schema_class . '::ISA'} = ('DBIx::Class::Schema::Loader');
    $schema_class->loader_options(%{$tdata{options}});

    my @warns;
    eval {
        local $SIG{__WARN__} = sub { push(@warns, @_) };
        $schema_class->connect($make_dbictest_db::dsn);
    };
    my $err = $@;
    $schema_class->storage->disconnect if !$err && $schema_class->storage;
    undef *{$schema_class};

    check_error($err, $tdata{error});

    return @warns;
}

sub dump_dbicdump {
    my %tdata = @_;

    # use $^X so we execute ./script/dbicdump with the same perl binary that the tests were executed with
    my @cmd = ($^X, qw(./script/dbicdump));

    while (my ($opt, $val) = each(%{ $tdata{options} })) {
        push @cmd, '-o', "$opt=$val";
    }

    push @cmd, $tdata{classname}, $make_dbictest_db::dsn;

    # make sure our current @INC gets used by dbicdump
    use Config;
    local $ENV{PERL5LIB} = join $Config{path_sep}, @INC, ($ENV{PERL5LIB} || '');

    my ($in, $out, $err);
    my $pid = open3($in, $out, $err, @cmd);

    my @out = <$out>;
    waitpid($pid, 0);

    my ($error, @warns);

    if ($? >> 8 != 0) {
        $error = $out[0];
        check_error($error, $tdata{error});
    }
    else {
        @warns = @out;
    }

    return @warns;
}

sub check_error {
    my ($got, $expected) = @_;

    return unless $got && $expected;

    if (ref $expected eq 'Regexp') {
        like $got, $expected, 'error matches expected pattern';
        return;
    }

    is $got, $expected, 'error matches';
}

sub do_dump_test {
    my %tdata = @_;
    
    $tdata{options}{dump_directory} = $DUMP_PATH;
    $tdata{options}{use_namespaces} ||= 0;

    for my $dumper (\&dump_directly, \&dump_dbicdump) {
        test_dumps(\%tdata, $dumper->(%tdata));
    }
}

sub test_dumps {
    my ($tdata, @warns) = @_;

    my %tdata = %{$tdata};

    my $schema_class = $tdata{classname};
    my $check_warns = $tdata{warnings};
    is(@warns, @$check_warns, "$schema_class warning count");

    for(my $i = 0; $i <= $#$check_warns; $i++) {
        like($warns[$i], $check_warns->[$i], "$schema_class warning $i");
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
    my $num = 1;
    like($contents, $_, "like $path " . $num++) for @_;
}

sub dump_file_not_like {
    my $path = shift;
    open(my $dumpfh, '<', $path) or die "Failed to open '$path': $!";
    my $contents = do { local $/; <$dumpfh>; };
    close($dumpfh);
    my $num = 1;
    unlike($contents, $_, "unlike $path ". $num++) for @_;
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

# test loading external content
do_dump_test(
    classname => 'DBICTest::Schema::13',
    options => { },
    error => '',
    warnings => [
        qr/Dumping manual schema for DBICTest::Schema::13 to directory /,
        qr/Schema dump completed/,
    ],
    regexes => {
        Foo => [
qr/package DBICTest::Schema::13::Foo;\nour \$skip_me = "bad mojo";\n1;/
        ],
    },
);

# test skipping external content
do_dump_test(
    classname => 'DBICTest::Schema::14',
    options => { skip_load_external => 1 },
    error => '',
    warnings => [
        qr/Dumping manual schema for DBICTest::Schema::14 to directory /,
        qr/Schema dump completed/,
    ],
    neg_regexes => {
        Foo => [
qr/package DBICTest::Schema::14::Foo;\nour \$skip_me = "bad mojo";\n1;/
        ],
    },
);

rmtree($DUMP_PATH, 1, 1);

# test out the POD

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
qr/=head1 NAME\n\nDBICTest::DumpMore::1::Foo\n\n=cut\n\n/,
qr/=head1 ACCESSORS\n\n/,
qr/=head2 fooid\n\n  data_type: INTEGER\n  default_value: undef\n  is_nullable: 1\n  size: undef\n\n/,
qr/=head2 footext\n\n  data_type: TEXT\n  default_value: undef\n  is_nullable: 1\n  size: undef\n\n/,
qr/->set_primary_key/,
qr/=head1 RELATIONS\n\n/,
qr/=head2 bars\n\nType: has_many\n\nRelated object: L<DBICTest::DumpMore::1::Bar>\n\n=cut\n\n/,
qr/1;\n$/,
        ],
        Bar => [
qr/package DBICTest::DumpMore::1::Bar;/,
qr/=head1 NAME\n\nDBICTest::DumpMore::1::Bar\n\n=cut\n\n/,
qr/=head1 ACCESSORS\n\n/,
qr/=head2 barid\n\n  data_type: INTEGER\n  default_value: undef\n  is_nullable: 1\n  size: undef\n\n/,
qr/=head2 fooref\n\n  data_type: INTEGER\n  default_value: undef\n  is_foreign_key: 1\n  is_nullable: 1\n  size: undef\n\n/,
qr/->set_primary_key/,
qr/=head1 RELATIONS\n\n/,
qr/=head2 fooref\n\nType: belongs_to\n\nRelated object: L<DBICTest::DumpMore::1::Foo>\n\n=cut\n\n/,
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
    options => { use_namespaces => 1, generate_pod => 0 },
    error => '',
    warnings => [
        qr/Dumping manual schema for DBICTest::DumpMore::1 to directory /,
        qr/Schema dump completed/,
    ],
    neg_regexes => {
        'Result/Foo' => [
            qr/^=/m,
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
            qr/result_namespace => 'Res'/,
            qr/resultset_namespace => 'RSet'/,
            qr/default_resultset_class => 'RSetBase'/,
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
            qr/result_namespace => '\+DBICTest::DumpMore::1::Res'/,
            qr/resultset_namespace => 'RSet'/,
            qr/default_resultset_class => 'RSetBase'/,
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

do_dump_test(
    classname => 'DBICTest::DumpMore::1',
    options   => {
        use_namespaces    => 1,
        result_base_class => 'My::MissingResultBaseClass',
    },
    error => qr/My::MissingResultBaseClass.*is not installed/,
);

done_testing;

END { rmtree($DUMP_PATH, 1, 1) unless $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP} }
