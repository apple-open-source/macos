$DEBUG=0;
$|=1;

@tests=(
  [ #1  Basic test
<<'EOT'
%{ my $out; %}
%%
S:  A { return($out) } ;
A:  'a' 'b' 'c' D { $out=$_[1].$_[2].$_[3].$_[4]; undef } ;
D:  'd' ;
%%
EOT
, [ 'a','b','c','d' ], "abcd"
],[ #2  In rule actions
<<'EOT'
%{ my $out; %}
%%
S:  A { return($out) } ;
A:  'a' { $out=$_[1] } 'b' { $out.=$_[3]} 'c' { $out.=$_[5]}
    D { $out.=$_[7].$_[5].$_[3].$_[1] } ;
D:  'd' ;
%%
EOT
, [ 'a', 'b', 'c', 'd' ], "abcdcba"
],[ #3  YYSemval > 0
<<'EOT'
%{ my $out; %}
%%
S:  A { return($out) } ;
A:  'a' 'b' 'c' D { $out.=$_[0]->YYSemval(1).
                          $_[0]->YYSemval(2).
                          $_[0]->YYSemval(3).
                          $_[0]->YYSemval(4);
                    undef
                  }
;
D:  'd' ;
%%
EOT
, [ 'a', 'b', 'c', 'd' ], "abcd"
],[ #4  YYSemval < 0
<<'EOT'
%{ my $out; %}
%%
S:  A { return($out) } ;
A:  'a' 'b' X ;
X:  'c' 'd' { $out=$_[0]->YYSemval(-1).$_[0]->YYSemval(0).$_[1].$_[2] };
%%
EOT
, [ 'a', 'b', 'c', 'd' ], "abcd"
],[ #5  Left assoc
<<'EOT'
%{ my $out; %}
%left '*'
%%
S:  A { return($out) } ;
A:  A '*' A { $out="($_[1]$_[2]$_[3])" }
  | B
;
B:  'a' | 'b' | 'c' | 'd' ;
%%
EOT
, [ 'a', '*', 'b', '*', 'c', '*', 'd' ], "(((a*b)*c)*d)"
],[ #6  Right assoc
<<'EOT'
%{ my $out; %}
%right '*'
%%
S:  A { return($out) } ;
A:  A '*' A { $out="($_[1]$_[2]$_[3])" }
  | B
;
B:  'a' | 'b' | 'c' | 'd' ;
%%
EOT
, [ 'a', '*', 'b', '*', 'c', '*', 'd' ], "(a*(b*(c*d)))"
],
[ #7 nonassoc
<<'EOT'
%{ my $out; %}
%nonassoc '+'
#%left '+'
%%
S:      S '+' S { $out }
    |   'a'
    |   error { $out="nonassoc" }
    ;
%%
EOT
, [ 'a' , '+', 'a', '+', 'a' ], "nonassoc"
],
[ #8  Left assoc with '\\'
<<'EOT'
%{ my $out; %}
%left '\\'
%%
S:  A { return($out) } ;
A:  A '\\' A { $out="($_[1]$_[2]$_[3])" }
  | B
;
B:  'a' | 'b' | 'c' | 'd' ;
%%
EOT
, [ 'a', '\\', 'b', '\\', 'c', '\\', 'd' ], '(((a\b)\c)\d)'
],
);

use Parse::Yapp;

my($count)=0;

sub TestIt {
    my($g,$in,$chk)=@_;

    my($lex) = sub {
        my($t)=shift(@$in);

            defined($t)
        or  $t='';
        return($t,$t);
    };

    ++$count;

    my($p)=new Parse::Yapp(input => $g);
    $p=$p->Output(classname => 'Test');

        $DEBUG
    and print $p;

    eval $p;
        $@
    and do {
        print "$@\n";
        print "not ok $count\n";
        return;
    };

    $p=new Test(yylex => $lex, yyerror => sub {});

    $out=$p->YYParse;
    undef $p;

        $out eq $chk
    or  do {
        print "Got '$out' instead of '$chk'\n";
        print 'not ';
    };
    print 'ok'," $count\n";
    undef(&Test::new);
}

print '1..'.@tests."\n";

for (@tests) {
    TestIt(@$_);
}
