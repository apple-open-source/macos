use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

use Data::Dumper;

my @serializers = (
    {	module => 'YAML.pm',
	inflater => sub { YAML::Load (shift) },
	deflater => sub { die "Expecting a reference" unless (ref $_[0]); YAML::Dump (shift) },
    },
    {	module => 'Storable.pm',
	inflater => sub { Storable::thaw (shift) },
	deflater => sub { die "Expecting a reference" unless (ref $_[0]); Storable::nfreeze (shift) },
    },
);


my $selected;
foreach my $serializer (@serializers) {
    eval { require $serializer->{module} };
    unless ($@) {
	$selected = $serializer;
	last;
    }
}

plan (skip_all => "No suitable serializer found") unless $selected;

plan (tests => 8);
DBICTest::Schema::Serialized->inflate_column( 'serialized',
    { inflate => $selected->{inflater},
      deflate => $selected->{deflater},
    },
);
Class::C3->reinitialize;

my $struct_hash = {
    a => 1,
    b => [ 
        { c => 2 },
    ],
    d => 3,
};

my $struct_array = [
    'a', 
    { 
	b => 1,
	c => 2
    },
    'd',
];

my $rs = $schema->resultset('Serialized');
my $inflated;

#======= testing hashref serialization

my $object = $rs->create( { 
    id => 1,
    serialized => '',
} );
ok($object->update( { serialized => $struct_hash } ), 'hashref deflation');
ok($inflated = $object->serialized, 'hashref inflation');
is_deeply($inflated, $struct_hash, 'inflated hash matches original');

$object = $rs->create( { 
    id => 2,
    serialized => '',
} );
eval { $object->set_inflated_column('serialized', $struct_hash) };
ok(!$@, 'set_inflated_column to a hashref');
is_deeply($object->serialized, $struct_hash, 'inflated hash matches original');


#====== testing arrayref serialization

ok($object->update( { serialized => $struct_array } ), 'arrayref deflation');
ok($inflated = $object->serialized, 'arrayref inflation');
is_deeply($inflated, $struct_array, 'inflated array matches original');
