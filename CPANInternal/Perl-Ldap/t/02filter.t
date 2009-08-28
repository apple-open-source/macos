#!perl
# Testcase contributed by  Julian Onions <Julian.Onions@nexor.co.uk>

use Net::LDAP::Filter;
use Net::LDAP::ASN qw(Filter);
use Convert::ASN1 qw(asn_dump);

my $asn = $Filter;

my @tests = do { local $/=""; <DATA> };

print "1..", 4*scalar(@tests), "\n";
my $testno = 0;
my $test;
foreach $test (@tests) {
    my ($filter, $ber, $filter_out) = $test =~ /^
      (?:\#.*\n)*
      (.*)\n
      ((?:[\da-fA-F]+:.*\n)+)
      (.*)
    /x or die "Cannot parse test\n$test\n";

    $filter_out ||= $filter;

    my $binary = pack("H*", join("", map { /\w+/g } $ber =~ /:((?: [\dA-Fa-f]{2}){1,16})/g));

    $testno ++;
    print "# ",$filter,"\n";
    $filt = new Net::LDAP::Filter $filter or print "not ";
    print "ok $testno\n";
    $testno ++;
    my $data = $asn->encode($filt) or print "# ",$asn->error,"\nnot ";
    print "ok $testno\n";
    $testno ++;
    unless($data eq $binary) {
	require Data::Dumper;
	print Data::Dumper::Dumper($filt);
	print "got    ", unpack("H*", $data), "\n";
	asn_dump(\*STDOUT, $data);
	print "wanted ", unpack("H*", $binary), "\n";
	asn_dump(\*STDOUT, $binary);

	print "not "
    }
    print "ok $testno\n";
    $testno ++;

    my $str = $filt->as_string;
    print "# ", $str,"\nnot " unless $str eq $filter_out;
    print "ok $testno\n";
}

__DATA__
(objectclass=foo)
0000: A3 12 04 0B 6F 62 6A 65 63 74 63 6C 61 73 73 04 ....objectclass.
0010: 03 66 6F 6F __ __ __ __ __ __ __ __ __ __ __ __ .foo

(objectclass=)
0000: A3 0F 04 0B 6F 62 6A 65 63 74 63 6C 61 73 73 04 ....objectclass.
0010: 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ .

createTimestamp>=199701011200Z
0000: A5 20 04 0F 63 72 65 61 74 65 54 69 6D 65 73 74 . ..createTimest
0010: 61 6D 70 04 0D 31 39 39 37 30 31 30 31 31 32 30 amp..19970101120
0020: 30 5A __ __ __ __ __ __ __ __ __ __ __ __ __ __ 0Z
(createTimestamp>=199701011200Z)

createTimestamp<=199801011210Z
0000: A6 20 04 0F 63 72 65 61 74 65 54 69 6D 65 73 74 . ..createTimest
0010: 61 6D 70 04 0D 31 39 39 38 30 31 30 31 31 32 31 amp..19980101121
0020: 30 5A __ __ __ __ __ __ __ __ __ __ __ __ __ __ 0Z
(createTimestamp<=199801011210Z)

(cn=a*)
0000: A4 09 04 02 63 6E 30 03 80 01 61 __ __ __ __ __ ....cn0...a

(cn=*a)
0000: A4 09 04 02 63 6E 30 03 82 01 61 __ __ __ __ __ ....cn0...a

cn=*a*
0000: A4 09 04 02 63 6E 30 03 81 01 61 __ __ __ __ __ ....cn0...a
(cn=*a*)

(cn=*)
0000: 87 02 63 6E __ __ __ __ __ __ __ __ __ __ __ __ ..cn

(cn~=foo)
0000: A8 09 04 02 63 6E 04 03 66 6F 6F __ __ __ __ __ ....cn..foo

(cn=Babs Jensen)
0000: A3 11 04 02 63 6E 04 0B 42 61 62 73 20 4A 65 6E ....cn..Babs Jen
0010: 73 65 6E __ __ __ __ __ __ __ __ __ __ __ __ __ sen

(!(cn=Tim Howes))
0000: A2 11 A3 0F 04 02 63 6E 04 09 54 69 6D 20 48 6F ......cn..Tim Ho
0010: 77 65 73 __ __ __ __ __ __ __ __ __ __ __ __ __ wes

!(cn=Tim Howes)
0000: A2 11 A3 0F 04 02 63 6E 04 09 54 69 6D 20 48 6F ......cn..Tim Ho
0010: 77 65 73 __ __ __ __ __ __ __ __ __ __ __ __ __ wes
(!(cn=Tim Howes))

(&(objectClass=Person)(|(sn=Jensen)(cn=Babs J*)))
0000: A0 37 A3 15 04 0B 6F 62 6A 65 63 74 43 6C 61 73 .7....objectClas
0010: 73 04 06 50 65 72 73 6F 6E A1 1E A3 0C 04 02 73 s..Person......s
0020: 6E 04 06 4A 65 6E 73 65 6E A4 0E 04 02 63 6E 30 n..Jensen....cn0
0030: 08 80 06 42 61 62 73 20 4A __ __ __ __ __ __ __ ...Babs J

(o=univ*of*mich*)
0000: A4 15 04 01 6F 30 10 80 04 75 6E 69 76 81 02 6F ....o0...univ..o
0010: 66 81 04 6D 69 63 68 __ __ __ __ __ __ __ __ __ f..mich

(cn:1.2.3.4.5:=Fred Flintstone)
0000: A9 20 81 09 31 2E 32 2E 33 2E 34 2E 35 82 02 63 .#..1.2.3.4.5..c
0010: 6E 83 0F 46 72 65 64 20 46 6C 69 6E 74 73 74 6F n..Fred Flintsto
0020: 6E 65 __ __ __ __ __ __ __ __ __ __ __ __ __ __ ne

(sn:dn:2.4.6.8.10:=Barney Rubble)
0000: A9 22 81 0A 32 2E 34 2E 36 2E 38 2E 31 30 82 02 ."..2.4.6.8.10..
0010: 73 6E 83 0D 42 61 72 6E 65 79 20 52 75 62 62 6C sn..Barney Rubbl
0020: 65 84 01 FF __ __ __ __ __ __ __ __ __ __ __ __ e...

(o:dn:=Ace Industry)
0000: A9 14 82 01 6F 83 0C 41 63 65 20 49 6E 64 75 73 ....o..Ace Indus
0010: 74 72 79 84 01 FF __ __ __ __ __ __ __ __ __ __ try...

(:dn:2.4.6.8.10:=Dino)
0000: A9 15 81 0A 32 2E 34 2E 36 2E 38 2E 31 30 83 04 ....2.4.6.8.10..
0010: 44 69 6E 6F 84 01 FF __ __ __ __ __ __ __ __ __ Dino...

(o=univ*of*mich*an)
0000: A4 19 04 01 6F 30 14 80 04 75 6E 69 76 81 02 6F ....o0...univ..o
0010: 66 81 04 6D 69 63 68 82 02 61 6E __ __ __ __ __ f..mich..an

(&(cn=fred)(!(objectclass=organization)))
0000: A0 2B A3 0A 04 02 63 6E 04 04 66 72 65 64 A2 1D .+....cn..fred..
0010: A3 1B 04 0B 6F 62 6A 65 63 74 63 6C 61 73 73 04 ....objectclass.
0020: 0C 6F 72 67 61 6E 69 7A 61 74 69 6F 6E __ __ __ .organization

(| (& (cn=test)) (| (cn=foo)))
0000: A1 1B A0 0C A3 0A 04 02 63 6E 04 04 74 65 73 74 ........cn..test
0010: A1 0B A3 09 04 02 63 6E 04 03 66 6F 6F __ __ __ ......cn..foo
(|(&(cn=test))(|(cn=foo)))

(| (cn=foo) (cn=test))
0000: A1 17 A3 09 04 02 63 6E 04 03 66 6F 6F A3 0A 04 ......cn..foo...
0010: 02 63 6E 04 04 74 65 73 74 __ __ __ __ __ __ __ .cn..test
(|(cn=foo)(cn=test))

(& (| (cn=test) (cn=foo) (sn=bar)) (| (c=GB) (c=AU)))
0000: A0 38 A1 22 A3 0A 04 02 63 6E 04 04 74 65 73 74 .8."....cn..test
0010: A3 09 04 02 63 6E 04 03 66 6F 6F A3 09 04 02 73 ....cn..foo....s
0020: 6E 04 03 62 61 72 A1 12 A3 07 04 01 63 04 02 47 n..bar......c..G
0030: 42 A3 07 04 01 63 04 02 41 55 __ __ __ __ __ __ B....c..AU
(&(|(cn=test)(cn=foo)(sn=bar))(|(c=GB)(c=AU)))

(| (& (c=GB) (cn=test)) (& (c=AU) (cn=test)) (& (c=GB) (cn=foo)) (& (c=AU) (cn=foo)) (& (c=GB) (sn=bar)) (& (c=AU) (sn=bar)))
0000: A1 81 86 A0 15 A3 07 04 01 63 04 02 47 42 A3 0A .........c..GB..
0010: 04 02 63 6E 04 04 74 65 73 74 A0 15 A3 07 04 01 ..cn..test......
0020: 63 04 02 41 55 A3 0A 04 02 63 6E 04 04 74 65 73 c..AU....cn..tes
0030: 74 A0 14 A3 07 04 01 63 04 02 47 42 A3 09 04 02 t......c..GB....
0040: 63 6E 04 03 66 6F 6F A0 14 A3 07 04 01 63 04 02 cn..foo......c..
0050: 41 55 A3 09 04 02 63 6E 04 03 66 6F 6F A0 14 A3 AU....cn..foo...
0060: 07 04 01 63 04 02 47 42 A3 09 04 02 73 6E 04 03 ...c..GB....sn..
0070: 62 61 72 A0 14 A3 07 04 01 63 04 02 41 55 A3 09 bar......c..AU..
0080: 04 02 73 6E 04 03 62 61 72 __ __ __ __ __ __ __ ..sn..bar
(|(&(c=GB)(cn=test))(&(c=AU)(cn=test))(&(c=GB)(cn=foo))(&(c=AU)(cn=foo))(&(c=GB)(sn=bar))(&(c=AU)(sn=bar)))

(& (| (cn=test) (cn=foo) (sn=bar)) (c=GB))
0000: A0 2D A1 22 A3 0A 04 02 63 6E 04 04 74 65 73 74 .-."....cn..test
0010: A3 09 04 02 63 6E 04 03 66 6F 6F A3 09 04 02 73 ....cn..foo....s
0020: 6E 04 03 62 61 72 A3 07 04 01 63 04 02 47 42 __ n..bar....c..GB
(&(|(cn=test)(cn=foo)(sn=bar))(c=GB))

(| (& (sn=bar) (c=GB)) (& (cn=foo) (c=GB)) (& (cn=test) (c=GB)))
0000: A1 43 A0 14 A3 09 04 02 73 6E 04 03 62 61 72 A3 .C......sn..bar.
0010: 07 04 01 63 04 02 47 42 A0 14 A3 09 04 02 63 6E ...c..GB......cn
0020: 04 03 66 6F 6F A3 07 04 01 63 04 02 47 42 A0 15 ..foo....c..GB..
0030: A3 0A 04 02 63 6E 04 04 74 65 73 74 A3 07 04 01 ....cn..test....
0040: 63 04 02 47 42 __ __ __ __ __ __ __ __ __ __ __ c..GB
(|(&(sn=bar)(c=GB))(&(cn=foo)(c=GB))(&(cn=test)(c=GB)))

(& (& (cn=foo) (| (cn=bar) (cn=xyz))) (& (cn=foo2) (| (cn=1) (cn=2))))
0000: A0 47 A0 23 A3 09 04 02 63 6E 04 03 66 6F 6F A1 .G.#....cn..foo.
0010: 16 A3 09 04 02 63 6E 04 03 62 61 72 A3 09 04 02 .....cn..bar....
0020: 63 6E 04 03 78 79 7A A0 20 A3 0A 04 02 63 6E 04 cn..xyz. ....cn.
0030: 04 66 6F 6F 32 A1 12 A3 07 04 02 63 6E 04 01 31 .foo2......cn..1
0040: A3 07 04 02 63 6E 04 01 32 __ __ __ __ __ __ __ ....cn..2
(&(&(cn=foo)(|(cn=bar)(cn=xyz)))(&(cn=foo2)(|(cn=1)(cn=2))))

(& (& (cn=foo) (! (cn=bar))) (| (cn=oof) (cn=foobie)))
0000: A0 35 A0 18 A3 09 04 02 63 6E 04 03 66 6F 6F A2 .5......cn..foo.
0010: 0B A3 09 04 02 63 6E 04 03 62 61 72 A1 19 A3 09 .....cn..bar....
0020: 04 02 63 6E 04 03 6F 6F 66 A3 0C 04 02 63 6E 04 ..cn..oof....cn.
0030: 06 66 6F 6F 62 69 65 __ __ __ __ __ __ __ __ __ .foobie
(&(&(cn=foo)(!(cn=bar)))(|(cn=oof)(cn=foobie)))

(| (& (cn=foobie) (cn=foo) (! (cn=bar))) (& (cn=oof) (cn=foo) (! (cn=bar))))
0000: A1 4D A0 26 A3 0C 04 02 63 6E 04 06 66 6F 6F 62 .M.&....cn..foob
0010: 69 65 A3 09 04 02 63 6E 04 03 66 6F 6F A2 0B A3 ie....cn..foo...
0020: 09 04 02 63 6E 04 03 62 61 72 A0 23 A3 09 04 02 ...cn..bar.#....
0030: 63 6E 04 03 6F 6F 66 A3 09 04 02 63 6E 04 03 66 cn..oof....cn..f
0040: 6F 6F A2 0B A3 09 04 02 63 6E 04 03 62 61 72 __ oo......cn..bar
(|(&(cn=foobie)(cn=foo)(!(cn=bar)))(&(cn=oof)(cn=foo)(!(cn=bar))))

(| (cn=foo) (cn=bar) (! (& (cn=a) (cn=b) (cn=c))))
0000: A1 35 A3 09 04 02 63 6E 04 03 66 6F 6F A3 09 04 .5....cn..foo...
0010: 02 63 6E 04 03 62 61 72 A2 1D A0 1B A3 07 04 02 .cn..bar........
0020: 63 6E 04 01 61 A3 07 04 02 63 6E 04 01 62 A3 07 cn..a....cn..b..
0030: 04 02 63 6E 04 01 63 __ __ __ __ __ __ __ __ __ ..cn..c
(|(cn=foo)(cn=bar)(!(&(cn=a)(cn=b)(cn=c))))

(| (! (cn=a)) (! (cn=b)) (! (cn=c)) (cn=foo) (cn=bar))
0000: A1 37 A2 09 A3 07 04 02 63 6E 04 01 61 A2 09 A3 .7......cn..a...
0010: 07 04 02 63 6E 04 01 62 A2 09 A3 07 04 02 63 6E ...cn..b......cn
0020: 04 01 63 A3 09 04 02 63 6E 04 03 66 6F 6F A3 09 ..c....cn..foo..
0030: 04 02 63 6E 04 03 62 61 72 __ __ __ __ __ __ __ ..cn..bar
(|(!(cn=a))(!(cn=b))(!(cn=c))(cn=foo)(cn=bar))

(& (cn=foo) (cn=bar) (! (& (cn=a) (cn=b) (cn=c))))
0000: A0 35 A3 09 04 02 63 6E 04 03 66 6F 6F A3 09 04 .5....cn..foo...
0010: 02 63 6E 04 03 62 61 72 A2 1D A0 1B A3 07 04 02 .cn..bar........
0020: 63 6E 04 01 61 A3 07 04 02 63 6E 04 01 62 A3 07 cn..a....cn..b..
0030: 04 02 63 6E 04 01 63 __ __ __ __ __ __ __ __ __ ..cn..c
(&(cn=foo)(cn=bar)(!(&(cn=a)(cn=b)(cn=c))))

(| (& (! (cn=a)) (cn=bar) (cn=foo)) (& (! (cn=b)) (cn=bar) (cn=foo)) (& (! (cn=c)) (cn=bar) (cn=foo)))
0000: A1 69 A0 21 A2 09 A3 07 04 02 63 6E 04 01 61 A3 .i.!......cn..a.
0010: 09 04 02 63 6E 04 03 62 61 72 A3 09 04 02 63 6E ...cn..bar....cn
0020: 04 03 66 6F 6F A0 21 A2 09 A3 07 04 02 63 6E 04 ..foo.!......cn.
0030: 01 62 A3 09 04 02 63 6E 04 03 62 61 72 A3 09 04 .b....cn..bar...
0040: 02 63 6E 04 03 66 6F 6F A0 21 A2 09 A3 07 04 02 .cn..foo.!......
0050: 63 6E 04 01 63 A3 09 04 02 63 6E 04 03 62 61 72 cn..c....cn..bar
0060: A3 09 04 02 63 6E 04 03 66 6F 6F __ __ __ __ __ ....cn..foo
(|(&(!(cn=a))(cn=bar)(cn=foo))(&(!(cn=b))(cn=bar)(cn=foo))(&(!(cn=c))(cn=bar)(cn=foo)))

(| (cn=foo\(bar\)) (cn=test))
0000: A1 1C A3 0E 04 02 63 6E 04 08 66 6F 6F 28 62 61 ......cn..foo(ba
0010: 72 29 A3 0A 04 02 63 6E 04 04 74 65 73 74 __ __ r)....cn..test
(|(cn=foo\28bar\29)(cn=test))

(cn=foo\*)
0000: A3 0A 04 02 63 6E 04 04 66 6F 6F 2A __ __ __ __ ....cn..foo*
(cn=foo\2a)

(cn=foo\\*)
0000: A4 0C 04 02 63 6E 30 06 80 04 66 6F 6F 5C __ __ ....cn0...foo\
(cn=foo\5c*)

(cn=\\*foo)
0000: A4 0E 04 02 63 6E 30 08 80 01 5C 82 03 66 6F 6F ....cn0...\..foo
(cn=\5c*foo)

(cn=\\*foo\\*)
0000: A4 0F 04 02 63 6E 30 09 80 01 5C 81 04 66 6F 6F ....cn0...\..foo
0010: 5C __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ \
(cn=\5c*foo\5c*)

(ou:dn:caseIgnoreMatch:=people)
0000: A9 20 81 0F 63 61 73 65 49 67 6E 6F 72 65 4D 61 . ..caseIgnoreMa
0010: 74 63 68 82 02 6F 75 83 06 70 65 6F 70 6C 65 84 tch..ou..people.
0020: 01 FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ ..

(sn:caseIgnoreMatch:=barr)
0000: A9 1B 81 0F 63 61 73 65 49 67 6E 6F 72 65 4D 61 ....caseIgnoreMa
0010: 74 63 68 82 02 73 6E 83 04 62 61 72 72 __ __ __ tch..sn..barr

(attr=*)
0000: 87 04 61 74 74 72 __ __ __ __ __ __ __ __ __ __ ..attr

(attr;x-tag=*)
0000: 87 0A 61 74 74 72 3B 78 2D 74 61 67 __ __ __ __ ..attr;x-tag

(attr=)
0000: A3 08 04 04 61 74 74 72 04 00 __ __ __ __ __ __ ....attr..

(1.2.3.4.5=)
0000: A3 0D 04 09 31 2E 32 2E 33 2E 34 2E 35 04 00 __ ....1.2.3.4.5..

(1.2.3.4.5;x-tag=)
0000: A3 13 04 0F 31 2E 32 2E 33 2E 34 2E 35 3B 78 2D ....1.2.3.4.5;x-
0010: 74 61 67 04 00 __ __ __ __ __ __ __ __ __ __ __ tag..

(attr=value)
0000: A3 0D 04 04 61 74 74 72 04 05 76 61 6C 75 65 __ ....attr..value

(space= )
0000: A3 0A 04 05 73 70 61 63 65 04 01 20 __ __ __ __ ....space.. 

(null=\00)
0000: A3 09 04 04 6E 75 6C 6C 04 01 00 __ __ __ __ __ ....null...

(bell=\07)
0000: A3 09 04 04 62 65 6C 6C 04 01 07 __ __ __ __ __ ....bell...

(bell=)
0000: A3 09 04 04 62 65 6C 6C 04 01 07 __ __ __ __ __ ....bell...
(bell=\07)

(attr;x-star=\2a)
0000: A3 10 04 0B 61 74 74 72 3B 78 2D 73 74 61 72 04 ....attr;x-star.
0010: 01 2A __ __ __ __ __ __ __ __ __ __ __ __ __ __ .*
(attr;x-star=\2a)

(attr;x-escape=\5C)
0000: A3 12 04 0D 61 74 74 72 3B 78 2D 65 73 63 61 70 ....attr;x-escap
0010: 65 04 01 5C __ __ __ __ __ __ __ __ __ __ __ __ e..\
(attr;x-escape=\5c)

(attr=initial*)
0000: A4 11 04 04 61 74 74 72 30 09 80 07 69 6E 69 74 ....attr0...init
0010: 69 61 6C __ __ __ __ __ __ __ __ __ __ __ __ __ ial

(attr=*any*)
0000: A4 0D 04 04 61 74 74 72 30 05 81 03 61 6E 79 __ ....attr0...any

(attr=*final)
0000: A4 0F 04 04 61 74 74 72 30 07 82 05 66 69 6E 61 ....attr0...fina
0010: 6C __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ l

(attr=initial*final)
0000: A4 18 04 04 61 74 74 72 30 10 80 07 69 6E 69 74 ....attr0...init
0010: 69 61 6C 82 05 66 69 6E 61 6C __ __ __ __ __ __ ial..final

(attr=initial*any*any*final)
0000: A4 22 04 04 61 74 74 72 30 1A 80 07 69 6E 69 74 ."..attr0...init
0010: 69 61 6C 81 03 61 6E 79 81 03 61 6E 79 82 05 66 ial..any..any..f
0020: 69 6E 61 6C __ __ __ __ __ __ __ __ __ __ __ __ inal

(attr=initial*any*)
0000: A4 16 04 04 61 74 74 72 30 0E 80 07 69 6E 69 74 ....attr0...init
0010: 69 61 6C 81 03 61 6E 79 __ __ __ __ __ __ __ __ ial..any

(attr=*any*final)
0000: A4 14 04 04 61 74 74 72 30 0C 81 03 61 6E 79 82 ....attr0...any.
0010: 05 66 69 6E 61 6C __ __ __ __ __ __ __ __ __ __ .final

(attr=*any*any*)
0000: A4 12 04 04 61 74 74 72 30 0A 81 03 61 6E 79 81 ....attr0...any.
0010: 03 61 6E 79 __ __ __ __ __ __ __ __ __ __ __ __ .any

(attr=**)
0000: A4 0A 04 04 61 74 74 72 30 02 81 00 __ __ __ __ ....attr0...

(attr=initial**)
0000: A4 13 04 04 61 74 74 72 30 0B 80 07 69 6E 69 74 ....attr0...init
0010: 69 61 6C 81 00 __ __ __ __ __ __ __ __ __ __ __ ial..

(attr=**final)
0000: A4 11 04 04 61 74 74 72 30 09 81 00 82 05 66 69 ....attr0.....fi
0010: 6E 61 6C __ __ __ __ __ __ __ __ __ __ __ __ __ nal

(attr=initial**final)
0000: A4 1A 04 04 61 74 74 72 30 12 80 07 69 6E 69 74 ....attr0...init
0010: 69 61 6C 81 00 82 05 66 69 6E 61 6C __ __ __ __ ial....final

(attr=initial***final)
0000: A4 1C 04 04 61 74 74 72 30 14 80 07 69 6E 69 74 ....attr0...init
0010: 69 61 6C 81 00 81 00 82 05 66 69 6E 61 6C __ __ ial......final

(attr~=)
0000: A8 08 04 04 61 74 74 72 04 00 __ __ __ __ __ __ ....attr..

(attr~=fubar)
0000: A8 0D 04 04 61 74 74 72 04 05 66 75 62 61 72 __ ....attr..fubar

(attr>=fubar)
0000: A5 0D 04 04 61 74 74 72 04 05 66 75 62 61 72 __ ....attr..fubar

(attr<=fubar)
0000: A6 0D 04 04 61 74 74 72 04 05 66 75 62 61 72 __ ....attr..fubar

(attr:1.2.3:=fubar)
0000: A9 14 81 05 31 2E 32 2E 33 82 04 61 74 74 72 83 ....1.2.3..attr.
0010: 05 66 75 62 61 72 __ __ __ __ __ __ __ __ __ __ .fubar

(attr:dn:=fubar)
0000: A9 10 82 04 61 74 74 72 83 05 66 75 62 61 72 84 ....attr..fubar.
0010: 01 FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ ..

(attr:DN:=fubar)
0000: A9 11 81 02 44 4E 82 04 61 74 74 72 83 05 66 75 ....DN..attr..fu
0010: 62 61 72 __ __ __ __ __ __ __ __ __ __ __ __ __ bar

(attr:dn:1.2.3:=fubar)
0000: A9 17 81 05 31 2E 32 2E 33 82 04 61 74 74 72 83 ....1.2.3..attr.
0010: 05 66 75 62 61 72 84 01 FF __ __ __ __ __ __ __ .fubar...

(:1.2.3:=fubar)
0000: A9 0E 81 05 31 2E 32 2E 33 83 05 66 75 62 61 72 ....1.2.3..fubar

(:caseExactMatch:=fubar)
0000: A9 17 81 0E 63 61 73 65 45 78 61 63 74 4D 61 74 ....caseExactMat
0010: 63 68 83 05 66 75 62 61 72 __ __ __ __ __ __ __ ch..fubar

(:dn:1.2.3:=fubar)
0000: A9 11 81 05 31 2E 32 2E 33 83 05 66 75 62 61 72 ....1.2.3..fubar
0010: 84 01 FF __ __ __ __ __ __ __ __ __ __ __ __ __ ...

(:dn:caseIgnoreMatch:=fubar)
0000: A9 1B 81 0F 63 61 73 65 49 67 6E 6F 72 65 4D 61 ....caseIgnoreMa
0010: 74 63 68 83 05 66 75 62 61 72 84 01 FF __ __ __ tch..fubar...

(!(objectClass=*))
0000: A2 0D 87 0B 6F 62 6A 65 63 74 43 6C 61 73 73 __ ....objectClass

(!(|(&(!(objectClass=*)))))
0000: A2 13 A1 11 A0 0F A2 0D 87 0B 6F 62 6A 65 63 74 ..........object
0010: 43 6C 61 73 73 __ __ __ __ __ __ __ __ __ __ __ Class

(&(objectClass=*))
0000: A0 0D 87 0B 6F 62 6A 65 63 74 43 6C 61 73 73 __ ....objectClass

(&(objectClass=*)(name~=))
0000: A0 17 87 0B 6F 62 6A 65 63 74 43 6C 61 73 73 A8 ....objectClass.
0010: 08 04 04 6E 61 6D 65 04 00 __ __ __ __ __ __ __ ...name..

(|(objectClass=*))
0000: A1 0D 87 0B 6F 62 6A 65 63 74 43 6C 61 73 73 __ ....objectClass

(|(objectClass=*)(name~=))
0000: A1 17 87 0B 6F 62 6A 65 63 74 43 6C 61 73 73 A8 ....objectClass.
0010: 08 04 04 6E 61 6D 65 04 00 __ __ __ __ __ __ __ ...name..
