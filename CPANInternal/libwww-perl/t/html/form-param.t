#!perl -w

use strict;
use Test qw(plan ok);

plan tests => 24;

use HTML::Form;

my $form = HTML::Form->parse(<<"EOT", "http://example.com");
<form>
<input type="hidden" name="hidden_1">

<input type="checkbox" name="checkbox_1" value="c1_v1" CHECKED>
<input type="checkbox" name="checkbox_1" value="c1_v2" CHECKED>
<input type="checkbox" name="checkbox_2" value="c2_v1" CHECKED>

<select name="multi_select_field" multiple="1">
 <option> 1
 <option> 2
 <option> 3
</select>
</form>
EOT

my @warn;
$SIG{__WARN__} = sub { push(@warn, @_) };

# list names
ok($form->param, 4);
ok(j($form->param), "hidden_1:checkbox_1:checkbox_2:multi_select_field");

# get
ok($form->param('hidden_1'), '');
ok($form->param('checkbox_1'), 'c1_v1');
ok(j($form->param('checkbox_1')), 'c1_v1:c1_v2');
ok($form->param('checkbox_2'), 'c2_v1');
ok(j($form->param('checkbox_2')), 'c2_v1');
ok(!defined($form->param('multi_select_field')));
ok(j($form->param('multi_select_field')), '');
ok(!defined($form->param('unknown')));
ok(j($form->param('unknown')), '');
ok(!@warn);

# set
$form->param('hidden_1', 'x');
ok(@warn && $warn[0] =~ /^Input 'hidden_1' is readonly/);
@warn = ();
ok(j($form->param('hidden_1')), 'x');

eval {
    $form->param('checkbox_1', 'foo');
};
ok($@);
ok(j($form->param('checkbox_1')), 'c1_v1:c1_v2');

$form->param('checkbox_1', 'c1_v2');
ok(j($form->param('checkbox_1')), 'c1_v2');
$form->param('checkbox_1', 'c1_v2');
ok(j($form->param('checkbox_1')), 'c1_v2');
$form->param('checkbox_1', []);
ok(j($form->param('checkbox_1')), '');
$form->param('checkbox_1', ['c1_v2', 'c1_v1']);
ok(j($form->param('checkbox_1')), 'c1_v1:c1_v2');
$form->param('checkbox_1', []);
ok(j($form->param('checkbox_1')), '');
$form->param('checkbox_1', 'c1_v2', 'c1_v1');
ok(j($form->param('checkbox_1')), 'c1_v1:c1_v2');

$form->param('multi_select_field', 3, 2);
ok(j($form->param('multi_select_field')), "2:3");

print "# Done\n";
ok(!@warn);

sub j {
    join(":", @_);
}
