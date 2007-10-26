use Apache::testold;

print fetch "http://$net::httpserver$net::perldir/api.pl?arg1=one&arg2=two";
