use strict;
use warnings;

sub run_it
{
	my ($code) = @_;
	my $pkg = caller(1);
	eval "package $pkg;$code";
	my $ok = $@ ? 0 : 1;

	return ($ok, $@);
}

sub runs_ok
{
	local($Test::Builder::Level) = $Test::Builder::Level + 1;
	my ($code, $name) = @_;
	my ($ok, $err) = run_it($code);

	ok($ok, $name) || diag("eval died with $err");

	return $ok;
}

sub dies_ok
{
	local($Test::Builder::Level) = $Test::Builder::Level + 1;
	my ($code, $name) = @_;
	my ($ok, $err) = run_it($code);

	ok(!$ok, $name) || diag("$code executed successfully");

	return !$ok;
}

1;
