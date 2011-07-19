use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;
use File::Compare;
use Path::Class qw/file/;

my $schema = DBICTest->init_schema();

plan tests => 10;

my $rs = $schema->resultset('FileColumn');
my $source_file = file(__FILE__);
my $fname = $source_file->basename;
my $fh = $source_file->open('r') or die "failed to open $source_file: $!\n";
my $fc = eval {
    $rs->create({ file => { handle => $fh, filename => $fname } })
};
is ( $@, '', 'created' );

$fh->close;

my $storage = file(
    $fc->column_info('file')->{file_column_path},
    $fc->id,
    $fc->file->{filename},
);
ok ( -e $storage, 'storage exists' );

# read it back
$fc = $rs->find({ id => $fc->id });

is ( $fc->file->{filename}, $fname, 'filename matches' );
ok ( compare($storage, $source_file) == 0, 'file contents matches' );

# update
my $new_fname = 'File.pm';
my $new_source_file = file(qw/lib DBIx Class InflateColumn File.pm/);
my $new_storage = file(
    $fc->column_info('file')->{file_column_path},
    $fc->id,
    $new_fname,
);
$fh = $new_source_file->open('r') or die "failed to open $new_source_file: $!\n";

$fc->file({ handle => $fh, filename => $new_fname });
$fc->update;

TODO: {
    local $TODO = 'design change required';
    ok ( ! -e $storage, 'old storage does not exist' );
};

ok ( -e $new_storage, 'new storage exists' );

# read it back
$fc = $rs->find({ id => $fc->id });

is ( $fc->file->{filename}, $new_fname, 'new filname matches' );
ok ( compare($new_storage, $new_source_file) == 0, 'new content matches' );

$fc->delete;

ok ( ! -e $storage, 'storage deleted' );

$fh = $source_file->openr or die "failed to open $source_file: $!\n";
$fc = $rs->create({ file => { handle => $fh, filename => $fname } });

# read it back
$fc->discard_changes;

$storage = file(
    $fc->column_info('file')->{file_column_path},
    $fc->id,
    $fc->file->{filename},
);

TODO: {
    local $TODO = 'need resultset delete override to delete_all';
    $rs->delete;
    ok ( ! -e $storage, 'storage does not exist after $rs->delete' );
};
