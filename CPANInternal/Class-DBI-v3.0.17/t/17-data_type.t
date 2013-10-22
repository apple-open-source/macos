use strict;
use Test::More;

BEGIN {
	eval { require DBD::Pg; };
	plan skip_all => 'needs DBD::Pg for testing' if $@;
}

use lib 't/testlib';
use Binary;

eval { Binary->CONSTRUCT; };
if ($@) {
	diag <<SKIP;
Pg connection failed ($@). Set env variables DBD_PG_DBNAME,  DBD_PG_USER,
DBD_PG_PASSWD to enable testing.
SKIP
	plan skip_all => 'Pg connection failed.';
}

plan tests => 40;

Binary->data_type(bin => DBI::SQL_BINARY);
Binary->db_Main->{ AutoCommit } = 0;

SKIP: {
	for my $id (1 .. 10) {
		my $bin = "foo\0$id";
		my $obj = eval { Binary->insert(
			{
				# id  => $id,
				bin => $bin,
			}
		) };
		skip $@, 40 if $@;
		isa_ok $obj, 'Binary';
		is $obj->id,  $id,  "id is $id";
		is $obj->bin, $bin, "insert: bin ok";

		$obj->bin("bar\0$id");
		$obj->update;

		if ($obj->id % 2) {
			$obj->dbi_commit;
			my $new_obj = $obj->retrieve($obj->id);
			is $obj->bin, "bar\0$id", "update: bin ok";
		} else {
			$obj->dbi_rollback;
			my $new_obj = $obj->retrieve($obj->id);
			is $new_obj, undef, "Rolled back";
		}
	}
}

