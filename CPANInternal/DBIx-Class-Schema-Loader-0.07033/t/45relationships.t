use strict;
use Test::More;
use Test::Exception;
use Try::Tiny;
use lib qw(t/lib);
use make_dbictest_db;

use DBIx::Class::Schema::Loader;

my $schema_counter = 0;

# test skip_relationships
my $regular = schema_with();
is( ref($regular->source('Bar')->relationship_info('fooref')), 'HASH',
    'regularly-made schema has fooref rel',
  );
my $skip_rel = schema_with( skip_relationships => 1 );
is_deeply( $skip_rel->source('Bar')->relationship_info('fooref'), undef,
    'skip_relationships blocks generation of fooref rel',
  );

# test hashref as rel_name_map
my $hash_relationship = schema_with(
    rel_name_map => {
        fooref => "got_fooref",
        bars   => "ignored",
        Foo    => {
            bars => "got_bars",
            fooref => "ignored",
        },
    }
);
is( ref($hash_relationship->source('Foo')->relationship_info('got_bars')),
    'HASH',
    'single level hash in rel_name_map picked up correctly'
  );
is( ref($hash_relationship->source('Bar')->relationship_info('got_fooref')),
    'HASH',
    'double level hash in rel_name_map picked up correctly'
  );

# test coderef as rel_name_map
my $code_relationship = schema_with(
    rel_name_map => sub {
        my ($args) = @_;

        if ($args->{local_moniker} eq 'Foo') {
            is_deeply(
                $args,
                {
		    name           => 'bars',
		    type           => 'has_many',
                    local_class    =>
                        "DBICTest::Schema::${schema_counter}::Result::Foo",
		    local_moniker  => 'Foo',
		    local_columns  => ['fooid'],
                    remote_class   =>
                        "DBICTest::Schema::${schema_counter}::Result::Bar",
		    remote_moniker => 'Bar',
		    remote_columns => ['fooref'],
		},
		'correct args for Foo passed'
              );
	    return 'bars_caught';
        }
	elsif ($args->{local_moniker} eq 'Bar') {
            is_deeply(
                $args,
                {
		    name           => 'fooref',
		    type           => 'belongs_to',
                    local_class    =>
                        "DBICTest::Schema::${schema_counter}::Result::Bar",
		    local_moniker  => 'Bar',
		    local_columns  => ['fooref'],
                    remote_class   =>
                        "DBICTest::Schema::${schema_counter}::Result::Foo",
		    remote_moniker => 'Foo',
		    remote_columns => ['fooid'],
		},
		'correct args for Foo passed'
              );
	
            return 'fooref_caught';
	}
    }
  );
is( ref($code_relationship->source('Foo')->relationship_info('bars_caught')),
    'HASH',
    'rel_name_map overrode local_info correctly'
  );
is( ref($code_relationship->source('Bar')->relationship_info('fooref_caught')),
    'HASH',
    'rel_name_map overrode remote_info correctly'
  );



# test relationship_attrs
throws_ok {
    schema_with( relationship_attrs => 'laughably invalid!!!' );
} qr/relationship_attrs/, 'throws error for invalid (scalar) relationship_attrs';

throws_ok {
    schema_with( relationship_attrs => [qw/laughably invalid/] );
} qr/relationship_attrs/, 'throws error for invalid (arrayref) relationship_attrs';

{
    my $nodelete = schema_with( relationship_attrs =>
				{
				 all        => { cascade_delete => 0 },
				 belongs_to => { cascade_delete => 1 },
				},
			      );

    my $bars_info   = $nodelete->source('Foo')->relationship_info('bars');
    #use Data::Dumper;
    #die Dumper([ $nodelete->source('Foo')->relationships() ]);
    my $fooref_info = $nodelete->source('Bar')->relationship_info('fooref');
    is( ref($fooref_info), 'HASH',
	'fooref rel is present',
      );
    is( $bars_info->{attrs}->{cascade_delete}, 0,
	'relationship_attrs settings seem to be getting through to the generated rels',
      );
    is( $fooref_info->{attrs}->{cascade_delete}, 1,
	'belongs_to in relationship_attrs overrides all def',
      );
}

# test relationship_attrs coderef
{
    my $relationship_attrs_coderef_invoked = 0;
    my $schema;

    lives_ok {
        $schema = schema_with(relationship_attrs => sub {
            my %p = @_;

            $relationship_attrs_coderef_invoked++;

            if ($p{rel_name} eq 'bars') {
                is $p{local_table},  'foo', 'correct local_table';
                is_deeply $p{local_cols}, [ 'fooid' ], 'correct local_cols';
                is $p{remote_table}, 'bar', 'correct remote_table';
                is_deeply $p{remote_cols}, [ 'fooref' ], 'correct remote_cols';
                is_deeply $p{attrs}, {
                    cascade_delete => 0,
                    cascade_copy   => 0,
                }, "got default rel attrs for $p{rel_name} in $p{local_table}";

                like $p{local_source}->result_class,
                    qr/^DBICTest::Schema::\d+::Result::Foo\z/,
                    'correct local source';

                like $p{remote_source}->result_class,
                    qr/^DBICTest::Schema::\d+::Result::Bar\z/,
                    'correct remote source';
 
                $p{attrs}{snoopy} = 1;

                return $p{attrs};
            }
            elsif ($p{rel_name} eq 'fooref') {
                is $p{local_table},  'bar', 'correct local_table';
                is_deeply $p{local_cols}, [ 'fooref' ], 'correct local_cols';
                is $p{remote_table}, 'foo', 'correct remote_table';
                is_deeply $p{remote_cols}, [ 'fooid' ], 'correct remote_cols';
                is_deeply $p{attrs}, {
                    on_delete     => 'NO ACTION',
                    on_update     => 'NO ACTION',
                    is_deferrable => 0,
                }, "got correct rel attrs for $p{rel_name} in $p{local_table}";

                like $p{local_source}->result_class,
                    qr/^DBICTest::Schema::\d+::Result::Bar\z/,
                    'correct local source';

                like $p{remote_source}->result_class,
                    qr/^DBICTest::Schema::\d+::Result::Foo\z/,
                    'correct remote source';
 
                $p{attrs}{scooby} = 1;

                return $p{attrs};
            }
            else {
                fail "unknown rel $p{rel_name} in $p{local_table}";
            }
        });
    } 'dumping schema with coderef relationship_attrs survived';

    is $relationship_attrs_coderef_invoked, 2,
        'relationship_attrs coderef was invoked correct number of times';

    is ((try { $schema->source('Foo')->relationship_info('bars')->{attrs}{snoopy} }) || undef, 1,
        "correct relationship attributes for 'bars' in 'Foo'");

    is ((try { $schema->source('Bar')->relationship_info('fooref')->{attrs}{scooby} }) || undef, 1,
        "correct relationship attributes for 'fooref' in 'Bar'");
}

done_testing;

#### generates a new schema with the given opts every time it's called
sub schema_with {
    $schema_counter++;
    DBIx::Class::Schema::Loader::make_schema_at(
            'DBICTest::Schema::'.$schema_counter,
            { naming => 'current', @_ },
            [ $make_dbictest_db::dsn ],
    );
    "DBICTest::Schema::$schema_counter"->clone;
}
