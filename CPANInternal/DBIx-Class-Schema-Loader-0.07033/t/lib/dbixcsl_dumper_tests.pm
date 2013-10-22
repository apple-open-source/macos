package dbixcsl_dumper_tests;

use strict;
use Test::More;
use File::Path;
use IPC::Open3;
use IO::Handle;
use List::MoreUtils 'any';
use DBIx::Class::Schema::Loader::Utils 'dumper_squashed';
use DBIx::Class::Schema::Loader ();
use Class::Unload ();
use namespace::clean;

use dbixcsl_test_dir '$tdir';

my $DUMP_PATH = "$tdir/dump";

sub cleanup {
    rmtree($DUMP_PATH, 1, 1);
}

sub class_file {
    my ($self, $class) = @_;

    $class =~ s{::}{/}g;
    $class = $DUMP_PATH . '/' . $class . '.pm';

    return $class;
}

sub append_to_class {
    my ($self, $class, $string) = @_;

    $class = $self->class_file($class);

    open(my $appendfh, '>>', $class) or die "Failed to open '$class' for append: $!";

    print $appendfh $string;

    close($appendfh);
}

sub dump_test {
    my ($self, %tdata) = @_;


    $tdata{options}{dump_directory} = $DUMP_PATH;
    $tdata{options}{use_namespaces} ||= 0;

    SKIP: for my $dumper (\&_dump_directly, \&_dump_dbicdump) {
        skip 'skipping dbicdump tests on Win32', 1,
            if $dumper == \&_dump_dbicdump && $^O eq 'MSWin32';

        _test_dumps(\%tdata, $dumper->(%tdata));
    }
}


sub _dump_directly {
    my %tdata = @_;

    my $schema_class = $tdata{classname};

    no strict 'refs';
    @{$schema_class . '::ISA'} = ('DBIx::Class::Schema::Loader');
    $schema_class->loader_options(
      quiet => 1,
      %{$tdata{options}},
    );

    my @warns;
    eval {
        local $SIG{__WARN__} = sub { push(@warns, @_) };
        $schema_class->connect(_get_connect_info(\%tdata));
    };
    my $err = $@;

    Class::Unload->unload($schema_class);

    _check_error($err, $tdata{error});

    return @warns;
}

sub _dump_dbicdump {
    my %tdata = @_;

    # use $^X so we execute ./script/dbicdump with the same perl binary that the tests were executed with
    my @cmd = ($^X, qw(script/dbicdump));

    $tdata{options}{quiet} = 1 unless exists $tdata{options}{quiet};

    while (my ($opt, $val) = each(%{ $tdata{options} })) {
        $val = dumper_squashed $val if ref $val;

        my $param = "$opt=$val";

        if ($^O eq 'MSWin32') {
            $param = q{"} . $param . q{"}; # that's not nearly enough...
        }

        push @cmd, '-o', $param;
    }

    my @connect_info = _get_connect_info(\%tdata);

    for my $info (@connect_info) {
        $info = dumper_squashed $info if ref $info;
    }

    push @cmd, $tdata{classname}, @connect_info;

    # make sure our current @INC gets used by dbicdump
    use Config;
    local $ENV{PERL5LIB} = join $Config{path_sep}, @INC, ($ENV{PERL5LIB} || '');

    my $std = { map { $_ => IO::Handle->new } (qw/in out err/) };
    my $pid = open3(@{$std}{qw/in out err/}, @cmd);

    waitpid($pid, 0);

    my @stdout = $std->{out}->getlines;
    ok (!scalar @stdout, 'Silence on STDOUT');

    my @warnings = $std->{err}->getlines;
    if ($? >> 8 != 0) {
        my $exception = pop @warnings;
        _check_error($exception, $tdata{error});
    }

    return @warnings;
}

sub _get_connect_info {
    my $opts = shift;

    my $test_db_class = $opts->{test_db_class} || 'make_dbictest_db';

    eval "require $test_db_class;";
    die $@ if $@;

    my $dsn = do {
        no strict 'refs';
        ${$test_db_class . '::dsn'};
    };

    return ($dsn, @{ $opts->{extra_connect_info} || [] });
}

sub _check_error {
    my ($got, $expected) = @_;

    return unless $got;

    if (not $expected) {
        fail "Unexpected error in " . ((caller(1))[3]) . ": $got";
        return;
    }

    if (ref $expected eq 'Regexp') {
        like $got, $expected, 'error matches expected pattern';
        return;
    }

    is $got, $expected, 'error matches';
}

sub _test_dumps {
    my ($tdata, @warns) = @_;

    my %tdata = %{$tdata};

    my $schema_class = $tdata{classname};
    my $check_warns = $tdata{warnings};

    is(@warns, @$check_warns, "$schema_class warning count")
      or diag @warns;

    for(my $i = 0; $i <= $#$check_warns; $i++) {
        like(($warns[$i] || ''), $check_warns->[$i], "$schema_class warning $i");
    }

    my $file_regexes = $tdata{regexes};
    my $file_neg_regexes = $tdata{neg_regexes} || {};
    my $schema_regexes = delete $file_regexes->{schema};

    my $schema_path = $DUMP_PATH . '/' . $schema_class;
    $schema_path =~ s{::}{/}g;

    _dump_file_like($schema_path . '.pm', @$schema_regexes) if $schema_regexes;

    foreach my $src (keys %$file_regexes) {
        my $src_file = $schema_path . '/' . $src . '.pm';
        _dump_file_like($src_file, @{$file_regexes->{$src}});
    }
    foreach my $src (keys %$file_neg_regexes) {
        my $src_file = $schema_path . '/' . $src . '.pm';
        _dump_file_not_like($src_file, @{$file_neg_regexes->{$src}});
    }
}

sub _dump_file_like {
    my $path = shift;
    open(my $dumpfh, '<', $path) or die "Failed to open '$path': $!";
    my $contents = do { local $/; <$dumpfh>; };
    close($dumpfh);
    like($contents, $_, "$path matches $_") for @_;
}

sub _dump_file_not_like {
    my $path = shift;
    open(my $dumpfh, '<', $path) or die "Failed to open '$path': $!";
    my $contents = do { local $/; <$dumpfh>; };
    close($dumpfh);
    unlike($contents, $_, "$path does not match $_") for @_;
}

END {
    __PACKAGE__->cleanup unless $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP}
}
