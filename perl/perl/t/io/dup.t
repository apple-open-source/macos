#!./perl

# $RCSfile: dup.t,v $$Revision: 1.2 $$Date: 2002/03/14 09:02:10 $

print "1..6\n";

print "ok 1\n";

open(dupout,">&STDOUT");
open(duperr,">&STDERR");

open(STDOUT,">Io.dup") || die "Can't open stdout";
open(STDERR,">&STDOUT") || die "Can't open stderr";

select(STDERR); $| = 1;
select(STDOUT); $| = 1;

print STDOUT "ok 2\n";
print STDERR "ok 3\n";
if ($^O eq 'MSWin32') {
    print `echo ok 4`;
    print `echo ok 5 1>&2`; # does this work?
}
else {
    system 'echo ok 4';
    system 'echo ok 5 1>&2';
}

close(STDOUT);
close(STDERR);

open(STDOUT,">&dupout");
open(STDERR,">&duperr");

if ($^O eq 'MSWin32') { print `type Io.dup` }
else                  { system 'cat Io.dup' }
unlink 'Io.dup';

print STDOUT "ok 6\n";

