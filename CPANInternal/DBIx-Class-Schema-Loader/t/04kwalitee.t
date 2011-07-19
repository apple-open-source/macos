use Test::More;

plan skip_all => "Not maintainer" unless -f 'MANIFEST.SKIP';

eval { require Test::Kwalitee; };
plan skip_all => 'Test::Kwalitee not installed' if $@;

Test::Kwalitee->import(); 
