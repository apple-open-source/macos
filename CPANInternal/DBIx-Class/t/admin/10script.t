# vim: filetype=perl
use strict;
use warnings;

use Test::More;
use Config;
use lib qw(t/lib);
$ENV{PERL5LIB} = join ($Config{path_sep}, @INC);
use DBICTest;


BEGIN {
    require DBIx::Class;
    plan skip_all => 'Test needs ' . DBIx::Class::Optional::Dependencies->req_missing_for('admin_script')
      unless DBIx::Class::Optional::Dependencies->req_ok_for('admin_script');
}

my @json_backends = qw/XS JSON DWIW/;
my $tests_per_run = 5;

plan tests => $tests_per_run * @json_backends;

for my $js (@json_backends) {

    eval {JSON::Any->import ($js) };
    SKIP: {
        skip ("Json backend $js is not available, skip testing", $tests_per_run) if $@;

        $ENV{JSON_ANY_ORDER} = $js;
        eval { test_dbicadmin () };
        diag $@ if $@;
    }
}

sub test_dbicadmin {
    my $schema = DBICTest->init_schema( sqlite_use_file => 1 );  # reinit a fresh db for every run

    my $employees = $schema->resultset('Employee');

    system( _prepare_system_args( qw|--op=insert --set={"name":"Matt"}| ) );
    ok( ($employees->count()==1), "$ENV{JSON_ANY_ORDER}: insert count" );

    my $employee = $employees->find(1);
    ok( ($employee->name() eq 'Matt'), "$ENV{JSON_ANY_ORDER}: insert valid" );

    system( _prepare_system_args( qw|--op=update --set={"name":"Trout"}| ) );
    $employee = $employees->find(1);
    ok( ($employee->name() eq 'Trout'), "$ENV{JSON_ANY_ORDER}: update" );

    system( _prepare_system_args( qw|--op=insert --set={"name":"Aran"}| ) );

    SKIP: {
        skip ("MSWin32 doesn't support -| either", 1) if $^O eq 'MSWin32';

        open(my $fh, "-|",  _prepare_system_args( qw|--op=select --attrs={"order_by":"name"}| ) ) or die $!;
        my $data = do { local $/; <$fh> };
        close($fh);
        if (!ok( ($data=~/Aran.*Trout/s), "$ENV{JSON_ANY_ORDER}: select with attrs" )) {
          diag ("data from select is $data")
        };
    }

    system( _prepare_system_args( qw|--op=delete --where={"name":"Trout"}| ) );
    ok( ($employees->count()==1), "$ENV{JSON_ANY_ORDER}: delete" );
}

# Why do we need this crap? Apparently MSWin32 can not pass through quotes properly
# (sometimes it will and sometimes not, depending on what compiler was used to build
# perl). So we go the extra mile to escape all the quotes. We can't also use ' instead
# of ", because JSON::XS (proudly) does not support "malformed JSON" as the author
# calls it. Bleh.
#
sub _prepare_system_args {
    my $perl = $^X;

    my @args = (
        qw|script/dbicadmin --quiet --schema=DBICTest::Schema --class=Employee|,
        q|--connect=["dbi:SQLite:dbname=t/var/DBIxClass.db","","",{"AutoCommit":1}]|,
        qw|--force|,
        @_,
    );

    if ( $^O eq 'MSWin32' ) {
        $perl = qq|"$perl"|;    # execution will fail if $^X contains paths
        for (@args) {
            $_ =~ s/"/\\"/g;
        }
    }

    return ($perl, @args);
}
