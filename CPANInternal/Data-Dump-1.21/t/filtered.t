#!perl -w

use strict;
use Test qw(plan ok);
plan tests => 39;

use Data::Dump qw(dumpf);

ok(dumpf("foo", sub { return { dump => "x" }}), 'x');
ok(dumpf("foo", sub { return { object => "x" }}), '"x"');
ok(dumpf("foo", sub { return { comment => "x" }}), "# x\n\"foo\"");
ok(dumpf({},    sub { return { bless => "x"}}), "bless({}, \"x\")");
ok(dumpf({a => 1, b => 2}, sub { return { hide_keys => ["b"] }}), "{ a => 1 }");
ok(dumpf("foo", sub { return }), '"foo"');

my $cb_count = 0;
ok(dumpf("foo", sub {
    my($ctx, $obj) = @_;
    $cb_count++;
    ok($$obj, "foo");
    ok($ctx->object_ref, $obj);
    ok($ctx->class, "");
    ok(!$ctx->object_isa("SCALAR"));
    ok($ctx->container_class, "");
    ok(!$ctx->container_isa("SCALAR"));
    ok($ctx->container_self, "");
    ok(!$ctx->is_ref);
    ok(!$ctx->is_blessed);
    ok(!$ctx->is_array);
    ok(!$ctx->is_hash);
    ok( $ctx->is_scalar);
    ok(!$ctx->is_code);
    ok($ctx->depth, 0);
    return;
}), '"foo"');
ok($cb_count, 1);

$cb_count = 0;
ok(dumpf(bless({ a => 1, b => bless {}, "Bar"}, "Foo"), sub {
    my($ctx, $obj) = @_;
    $cb_count++;
    return unless $ctx->object_isa("Bar");
    ok(ref($obj), "Bar");
    ok($ctx->object_ref, $obj);
    ok($ctx->class, "Bar");
    ok($ctx->object_isa("Bar"));
    ok(!$ctx->object_isa("Foo"));
    ok($ctx->container_class, "Foo");
    ok($ctx->container_isa("Foo"));
    ok($ctx->container_self, '$self->{b}');
    ok($ctx->is_ref);
    ok($ctx->is_blessed);
    ok(!$ctx->is_array);
    ok($ctx->is_hash);
    ok(!$ctx->is_scalar);
    ok(!$ctx->is_code);
    ok($ctx->depth, 1);
    return;
}) =~ /^bless\(.*, "Foo"\)\z/);
ok($cb_count, 3);
