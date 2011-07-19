BEGIN {
  $^O = 'Unix'; # Test in Unix mode
}

use Test;
use strict;
use Path::Class;
use Cwd;

plan tests => 66;
ok(1);

my $file1 = Path::Class::File->new('foo.txt');
ok $file1, 'foo.txt';
ok $file1->is_absolute, '';
ok $file1->dir, '.';
ok $file1->basename, 'foo.txt';

my $file2 = file('dir', 'bar.txt');
ok $file2, 'dir/bar.txt';
ok $file2->is_absolute, '';
ok $file2->dir, 'dir';
ok $file2->basename, 'bar.txt';

my $dir = dir('tmp');
ok $dir, 'tmp';
ok $dir->is_absolute, '';

my $dir2 = dir('/tmp');
ok $dir2, '/tmp';
ok $dir2->is_absolute, 1;

my $cat = file($dir, 'foo');
ok $cat, 'tmp/foo';
$cat = $dir->file('foo');
ok $cat, 'tmp/foo';
ok $cat->dir, 'tmp';
ok $cat->basename, 'foo';

$cat = file($dir2, 'foo');
ok $cat, '/tmp/foo';
$cat = $dir2->file('foo');
ok $cat, '/tmp/foo';
ok $cat->isa('Path::Class::File');
ok $cat->dir, '/tmp';

$cat = $dir2->subdir('foo');
ok $cat, '/tmp/foo';
ok $cat->isa('Path::Class::Dir');

my $file = file('/foo//baz/./foo')->cleanup;
ok $file, '/foo/baz/foo';
ok $file->dir, '/foo/baz';
ok $file->parent, '/foo/baz';

{
  my $dir = dir('/foo/bar/baz');
  ok $dir->parent, '/foo/bar';
  ok $dir->parent->parent, '/foo';
  ok $dir->parent->parent->parent, '/';
  ok $dir->parent->parent->parent->parent, '/';

  $dir = dir('foo/bar/baz');
  ok $dir->parent, 'foo/bar';
  ok $dir->parent->parent, 'foo';
  ok $dir->parent->parent->parent, '.';
  ok $dir->parent->parent->parent->parent, '..';
  ok $dir->parent->parent->parent->parent->parent, '../..';
}

{
  my $dir = dir("foo/");
  ok $dir, 'foo';
  ok $dir->parent, '.';
}

{
  # Special cases
  ok dir(''), '/';
  ok dir(), '.';
  ok dir('', 'var', 'tmp'), '/var/tmp';
  ok dir()->absolute, dir(Cwd::cwd())->cleanup;
  ok dir(undef), undef;
}

{
  my $file = file('/tmp/foo/bar.txt');
  ok $file->relative('/tmp'), 'foo/bar.txt';
  ok $file->relative('/tmp/foo'), 'bar.txt';
  ok $file->relative('/tmp/'), 'foo/bar.txt';
  ok $file->relative('/tmp/foo/'), 'bar.txt';

  $file = file('one/two/three');
  ok $file->relative('one'), 'two/three';
}

{
  # Try out the dir_list() method
  my $dir = dir('one/two/three/four/five');
  my @d = $dir->dir_list();
  ok "@d", "one two three four five";

  @d = $dir->dir_list(2);
  ok "@d", "three four five";

  @d = $dir->dir_list(-2);
  ok "@d", "four five";

  @d = $dir->dir_list(2, 2);
  ok "@d", "three four", "dir_list(2, 2)";

  @d = $dir->dir_list(-3, 2);
  ok "@d", "three four", "dir_list(-3, 2)";

  @d = $dir->dir_list(-3, -2);
  ok "@d", "three", "dir_list(-3, -2)";

  @d = $dir->dir_list(-3, -1);
  ok "@d", "three four", "dir_list(-3, -1)";

  my $d = $dir->dir_list();
  ok $d, 5, "scalar dir_list()";

  $d = $dir->dir_list(2);
  ok $d, "three", "scalar dir_list(2)";
  
  $d = $dir->dir_list(-2);
  ok $d, "four", "scalar dir_list(-2)";
  
  $d = $dir->dir_list(2, 2);
  ok $d, "four", "scalar dir_list(2, 2)";
}

{
  # Test is_dir()
  ok  dir('foo')->is_dir, 1;
  ok file('foo')->is_dir, 0;
}

{
  # subsumes()
  ok dir('foo/bar')->subsumes('foo/bar/baz'), 1;
  ok dir('/foo/bar')->subsumes('/foo/bar/baz'), 1;
  ok dir('foo/bar')->subsumes('bar/baz'), 0;
  ok dir('/foo/bar')->subsumes('foo/bar'), 0;
  ok dir('/foo/bar')->subsumes('/foo/baz'), 0;
  ok dir('/')->subsumes('/foo/bar'), 1;
}
