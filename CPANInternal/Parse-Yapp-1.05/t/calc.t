$^W=0;

# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..15\n"; }
END {print "not ok 1\n" unless $loaded;}
use Parse::Yapp;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

use Parse::Yapp;

my($testnum)=2;
my($parser,$grammar);
my($yapptxt);

#Test 2
eval  {
	$grammar=join('',<DATA>);
	$parser=new Parse::Yapp(input => $grammar);
};

	$@
and	do {
	print "not ok $testnum\n";
	print "Object not created. Cannot continue test suite: aborting\n";
	exit(1);
};
print "ok $testnum\n";
++$testnum;

#Test 3
eval {
	$yapptxt=$parser->Output(classname => 'Calc');
};
	$@
and	do {
	print "not ok $testnum\n";
	print "Parser not generated. Cannot continue test suite: aborting\n";
	exit(1);
};
print "ok $testnum\n";
++$testnum;

#Test 4
eval $yapptxt;
	$@
and	do {
	print "not ok $testnum\n";
	print "Parser not loaded. Cannot continue test suite: aborting\n";
	exit(1);
};
print "ok $testnum\n";
++$testnum;

#Test 5
my($calc);
eval {
	$calc=new Calc();
};
	$@
and	do {
	print "not ok $testnum\n";
	print "Parser not found. Cannot continue test suite: aborting\n";
	exit(1);
};
print "ok $testnum\n";
++$testnum;

#Test 6
eval {
	$calc->YYData->{INPUT}="13*2\n-(13*2)+3\n5^3+2\n";
	@outcheck=((13*2),(-(13*2)+3),(5**3+2));
	$output=$calc->YYParse(yylex => \&Calc::Lexer);
};
print $@ ? "not ok $testnum\n" : "ok $testnum\n";
++$testnum;

#Test 7
print       join(',',@$output) ne join(',',@outcheck)
        ?   "not ok $testnum\n"
        :   "ok $testnum\n";
++$testnum;

#Test 8
eval {
    delete($calc->YYData->{LINE});
	$calc->YYData->{INPUT}="5+8\n-(13*2)+3--\n3*8\n**7-3(12*55)\n12*(5-2)\n";
	@outcheck=((5+8), undef, (3*8), undef, (12*(5-2)));
    @errcheck=( 2, 4);
    $nberr=2;
	$output=$calc->YYParse(yylex => \&Calc::Lexer, yyerror => \&Calc::Error);
};
print $@ ? "not ok $testnum\n" : "ok $testnum\n";
++$testnum;

#Test 9
print       join(',',@$output) ne join(',',@outcheck)
        ?   "not ok $testnum\n"
        :   "ok $testnum\n";
++$testnum;

#Test 10
print       join(',',@{$calc->YYData->{ERRLINES}}) ne join(',',@errcheck)
        ?   "not ok $testnum\n"
        :   "ok $testnum\n";
++$testnum;

#Test 11
print       $calc->YYNberr != $nberr 
        ?   "not ok $testnum\n"
        :   "ok $testnum\n";
++$testnum;

#Test 12
eval {
	$calc->YYData->{INPUT}="a=-(13*2)+3\nb=12*(5-2)\na*b\n";
	@outcheck=((-(13*2)+3), (12*(5-2)), ((-(13*2)+3)*(12*(5-2))));
			  
	$output=$calc->YYParse(yylex => \&Calc::Lexer, yyerror => \&Calc::Error);
};
print $@ ? "not ok $testnum\n" : "ok $testnum\n";
++$testnum;

#Test 13 
print       join(',',@$output) ne join(',',@outcheck)
        ?   "not ok $testnum\n"
        :   "ok $testnum\n";
++$testnum;

#Test 14
eval {

	local *STDERR;

	close(STDERR);	#Supress debug output

	$calc->YYData->{INPUT}="a=-(13*2)+3\n-*12\nb=12*(5-2)\na*b\n";
	@outcheck=((-(13*2)+3), undef, (12*(5-2)), ((-(13*2)+3)*(12*(5-2))));
			  
	$output=$calc->YYParse(yylex => \&Calc::Lexer,
						 yyerror => \&Calc::Error,
						 yydebug => 0xFF );
};
print $@ ? "not ok $testnum\n" : "ok $testnum\n";
++$testnum;

#Test 15 
print       join(',',@$output) ne join(',',@outcheck)
        ?   "not ok $testnum\n"
        :   "ok $testnum\n";
++$testnum;

__DATA__

%right  '='
%left   '-' '+'
%left   '*' '/'
%left   NEG
%right  '^'

%%
input:  #empty
        |   input line  { push(@{$_[1]},$_[2]); $_[1] }
;

line:       '\n'                { ++$_[0]->YYData->{LINE}; $_[1] }
        |   exp '\n'            { ++$_[0]->YYData->{LINE}; $_[1] }
		|	error '\n'  { ++$_[0]->YYData->{LINE}; $_[0]->YYErrok }
;

exp:        NUM
        |   VAR                 { $_[0]->YYData->{VARS}{$_[1]} }
        |   VAR '=' exp         { $_[0]->YYData->{VARS}{$_[1]}=$_[3] }
        |   exp '+' exp         { $_[1] + $_[3] }
        |   exp '-' exp         { $_[1] - $_[3] }
        |   exp '*' exp         { $_[1] * $_[3] }
        |   exp '/' exp         { $_[1] / $_[3] }
        |   '-' exp %prec NEG   { -$_[2] }
        |   exp '^' exp         { $_[1] ** $_[3] }
        |   '(' exp ')'         { $_[2] }
;

%%

sub Error {
    my($parser)=shift;

	push(@{$parser->YYData->{ERRLINES}}, $parser->YYData->{LINE});
}

sub Lexer {
    my($parser)=shift;

        exists($parser->YYData->{LINE})
    or  $parser->YYData->{LINE}=1;

        $parser->YYData->{INPUT}
    or  return('',undef);

    $parser->YYData->{INPUT}=~s/^[ \t]//;

    for ($parser->YYData->{INPUT}) {
        s/^([0-9]+(?:\.[0-9]+)?)//
                and return('NUM',$1);
        s/^([A-Za-z][A-Za-z0-9_]*)//
                and return('VAR',$1);
        s/^(.)//s
                and return($1,$1);
    }
}

