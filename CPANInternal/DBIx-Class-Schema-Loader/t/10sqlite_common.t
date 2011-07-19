use strict;
use Test::More;
use lib qw(t/lib);
use dbixcsl_common_tests;

eval { require DBD::SQLite };
my $class = $@ ? 'SQLite2' : 'SQLite';

my $tester = dbixcsl_common_tests->new(
    vendor          => 'SQLite',
    auto_inc_pk     => 'INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT',
    dsn             => "dbi:$class:dbname=./t/sqlite_test",
    user            => '',
    password        => '',
    extra           => {
        create => [
            # 'sqlite_' is reserved, so we use 'extra_'
            q{
                CREATE TABLE "extra_loader_test1" (
                    "id" NOT NULL PRIMARY KEY,
                    "value" VARCHAR(100)
                )
            },
            q{
                CREATE TABLE extra_loader_test2 (
                    event_id INTEGER PRIMARY KEY
                )
            },
            q{
                CREATE TABLE extra_loader_test3 (
                    person_id INTEGER PRIMARY KEY
                )
            },
            # Wordy, newline-heavy SQL to stress the regexes
            q{
                CREATE TABLE extra_loader_test4 (
                    event_id INTEGER NOT NULL
                        CONSTRAINT fk_event_id
                        REFERENCES extra_loader_test2(event_id),
                    person_id INTEGER NOT NULL
                        CONSTRAINT fk_person_id
                        REFERENCES extra_loader_test3 (person_id),
                    PRIMARY KEY (event_id, person_id)
                )
            },
        ],
        drop  => [ qw/extra_loader_test1 extra_loader_test2 extra_loader_test3 extra_loader_test4 / ],
        count => 5,
        run   => sub {
            my ($schema, $monikers, $classes) = @_;

            ok ((my $rs = $schema->resultset($monikers->{extra_loader_test1})),
                'resultset for quoted table');

            is_deeply [ $rs->result_source->columns ], [ qw/id value/ ],
                'retrieved quoted column names from quoted table';

            ok ((my $source = $schema->source($monikers->{extra_loader_test4})),
                'verbose table');

            is_deeply [ $source->primary_columns ], [ qw/event_id person_id/ ],
                'composite primary key';

            is ($source->relationships, 2,
                '2 foreign key constraints found');

        },
    },
);

$tester->run_tests();

END {
    unlink './t/sqlite_test';
}
