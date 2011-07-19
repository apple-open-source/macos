use Test;
use strict;
BEGIN { plan tests => 28 }; #30, todo => [29,30] };
use Path::Class qw(file dir foreign_file foreign_dir);
ok(1);


my $file = Path::Class::File->new_foreign('Unix', 'dir', 'foo.txt');
ok $file, 'dir/foo.txt';

ok $file->as_foreign('Win32'), 'dir\foo.txt';
ok $file->as_foreign('Mac'), ':dir:foo.txt';
ok $file->as_foreign('OS2'), 'dir/foo.txt';

if ($^O eq 'VMS') {
  ok $file->as_foreign('VMS'), '[.dir]foo.txt';
} else {
  skip "skip Can't test VMS code on other platforms", 1;
}

$file = foreign_file('Mac', ':dir:foo.txt');
ok $file, ':dir:foo.txt';
ok $file->as_foreign('Unix'), 'dir/foo.txt';
ok $file->dir, ':dir:';


my $dir = Path::Class::Dir->new_foreign('Unix', 'dir/subdir');
ok $dir, 'dir/subdir';
ok $dir->as_foreign('Win32'), 'dir\subdir';
ok $dir->as_foreign('Mac'),  ':dir:subdir:';
ok $dir->as_foreign('OS2'),   'dir/subdir';

if ($^O eq 'VMS') {
  ok $dir->as_foreign('VMS'), '[.dir.subdir]';
} else {
  skip "skip Can't test VMS code on other platforms", 1;
}

{
  # subsumes() should respect foreignness
  my ($me, $other) = map { Path::Class::Dir->new_foreign('Unix', $_) } qw(/ /Foo);
  ok($me->subsumes($other));

  ($me, $other) =  map { Path::Class::Dir->new_foreign('Win32', $_) } qw(C:\ C:\Foo);
  ok($me->subsumes($other));
}

# Note that "\\" and '\\' are each a single backslash
$dir = foreign_dir('Win32', 'C:\\');
ok $dir, 'C:\\';
$dir = foreign_dir('Win32', 'C:/');
ok $dir, 'C:\\';
ok $dir->subdir('Program Files'), 'C:\\Program Files';

$dir = foreign_dir('Mac', ':dir:subdir:');
ok $dir, ':dir:subdir:';
ok $dir->subdir('foo'),   ':dir:subdir:foo:';
ok $dir->file('foo.txt'), ':dir:subdir:foo.txt';
ok $dir->parent,          ':dir:';
ok $dir->is_relative, 1;

$dir = foreign_dir('Mac', ':dir::dir2:subdir');
ok $dir, ':dir::dir2:subdir:';
ok $dir->as_foreign('Unix'), 'dir/../dir2/subdir';

$dir = foreign_dir('Mac', 'Volume:dir:subdir:');
ok $dir, 'Volume:dir:subdir:';
ok $dir->is_absolute;
# TODO ok $dir->as_foreign('Unix'), '/dir/subdir';
# TODO ok $dir->as_foreign('Unix')->is_absolute, 1;
