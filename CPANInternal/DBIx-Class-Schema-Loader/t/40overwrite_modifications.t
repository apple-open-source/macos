use strict;
use Test::More tests => 3;
use Test::Exception;
use lib qw(t/lib);
use make_dbictest_db;

use File::Copy;
use File::Spec;
use File::Temp qw/ tempdir tempfile /;

use DBIx::Class::Schema::Loader;

my $tempdir = tempdir( CLEANUP => 1 );
my $foopm = File::Spec->catfile( $tempdir,
    qw| DBICTest Schema Overwrite_modifications Result Foo.pm |);
dump_schema();

# check that we dumped
ok( -f $foopm, 'looks like it dumped' );

# now modify one of the files
{
    open my $in, '<', $foopm or die "$! reading $foopm";
    my ($tfh,$temp) = tempfile( UNLINK => 1);
    while(<$in>) {
	s/"bars"/"somethingelse"/;
	print $tfh $_;
    }
    close $tfh;
    copy( $temp, $foopm );
}

# and dump again without overwrites
throws_ok {
    dump_schema();
} qr/mismatch/, 'throws error dumping without overwrite_modifications';

# and then dump with overwrite
lives_ok {
    dump_schema( overwrite_modifications => 1 );
} 'does not throw when dumping with overwrite_modifications';

sub dump_schema {

    # need to poke _loader_invoked in order to be able to rerun the
    # loader multiple times.
    DBICTest::Schema::Overwrite_modifications->_loader_invoked(0)
	  if @DBICTest::Schema::Overwrite_modifications::ISA;

    local $SIG{__WARN__} = sub {
        warn @_
            unless $_[0] =~ /^Dumping manual schema|^Schema dump completed/;
    };
    DBIx::Class::Schema::Loader::make_schema_at( 'DBICTest::Schema::Overwrite_modifications',
						 { dump_directory => $tempdir,
						   @_,
						 },
						 [ $make_dbictest_db::dsn ],
					       );
}
