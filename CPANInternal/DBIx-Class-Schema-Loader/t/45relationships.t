use strict;
use Test::More tests => 6;
use Test::Exception;
use lib qw(t/lib);
use make_dbictest_db;

use DBIx::Class::Schema::Loader;

# test skip_relationships
my $regular = schema_with();
is( ref($regular->source('Bar')->relationship_info('fooref')), 'HASH',
    'regularly-made schema has fooref rel',
  );
my $skip_rel = schema_with( skip_relationships => 1 );
is_deeply( $skip_rel->source('Bar')->relationship_info('fooref'), undef,
    'skip_relationships blocks generation of fooref rel',
  );


# test relationship_attrs
throws_ok {
    schema_with( relationship_attrs => 'laughably invalid!!!' );
} qr/relationship_attrs/, 'throws error for invalid relationship_attrs';


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


#### generates a new schema with the given opts every time it's called
my $schema_counter = 0;
sub schema_with {
    $schema_counter++;
    DBIx::Class::Schema::Loader::make_schema_at(
            'DBICTest::Schema::'.$schema_counter,
            { naming => 'current', @_ },
            [ $make_dbictest_db::dsn ],
    );
    "DBICTest::Schema::$schema_counter"->clone;
}
